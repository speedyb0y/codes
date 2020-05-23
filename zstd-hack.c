/*
    zstd-hack

    gcc -Wall -Wextra -fwhole-program -O2 -march=native -pthread -lzstd zstd-hack.c -o zstd-hack

    TODO: FIXME: colocar um checksum para cada thread, no header
*/

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 64
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <zstd.h>

#define FD_INVALID -1

#if 1
#define dbg(fmt, ...) ({ fprintf(stderr, fmt "\n", ##__VA_ARGS__); })
#else
#define dbg(fmt, ...) ({})
#endif

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

#ifndef offsetof
#define offsetof __builtin_offsetof
#endif

#define loop while (1)

#define elif else if

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int64_t i64;

typedef unsigned int uint;
typedef unsigned long long int uintll;

typedef long long int intll;

#define ALIGNED(value, alignment) ((((value) + (alignment) - 1) / (alignment)) * (alignment))

#define VALBITS(value) ((sizeof(u64)*8) - __builtin_clzll((u64)(value)))
#define VALMASK(value) ((1ULL << VALBITS(value)) - 1)
#define VALSIZE(value) ((VALBITS(value) + 7) / 8)

#define EXIT_SUCCESS            0
#define EXIT_SUCCESS_NOTHING    1 // OK BUT NOTHING WAS READ
#define EXIT_TERMINATED         2 // BUT FINISHED
#define EXIT_TERMINATED_NOTHING 3 // WAS STILL INITIALIZING
#define EXIT_IN_ERR             4 // BUT FINISHED
#define EXIT_IN_ERR_NOTHING     5 // ERROR ON INPUT - NOTHING READ
#define EXIT_OUT_ERR            6 // NOT FINISHED
#define EXIT_FATAL              7

// TODO: FIXME: tamanho de certas coisas, posicao delas etc
#define CHECK ( \
              sizeof(Tail) \
    + ((u64)offsetof(Tail, check)               <<  1) \
    + ((u64)offsetof(Tail, checksum)            <<  2) \
    + ((u64)offsetof(Tail, time)                <<  3) \
    + ((u64)offsetof(Tail, random)              <<  4) \
    + ((u64)offsetof(Tail, size)                <<  5) \
    + ((u64)offsetof(Tail, blockSize)           <<  6) \
    + ((u64)offsetof(Tail, blocks)              <<  7) \
    + ((u64)offsetof(Tail, threadBuffInBlocks)  <<  8) \
    + ((u64)offsetof(Tail, threadBuffOutBlocks) <<  9) \
    + ((u64)offsetof(Tail, threadsN)            << 10) \
    + ((u64)offsetof(Tail, compression)         << 11) \
    + ((u64)offsetof(Tail, chunkODiffSize)      << 12) \
    + ((u64)offsetof(Tail, chunkSizeSize)       << 13) \
    + ((u64)offsetof(Tail, cChunkSize)          << 14) \
    + ((u64)offsetof(Tail, cChunkBitsBlocks)    << 15) \
    + ((u64)offsetof(Tail, cChunkBitsThread)    << 16) \
    + ((u64)offsetof(Tail, endSize)             << 17) \
    )

// TODO: FIXME: enforce um tamanho para isso
typedef struct Tail {
    u64 check;
    u64 checksum; // DO END INTEIRO, ALINHADO
    u64 time; // em milésimos de segundo
    u64 random;
    u64 size; // ORIGINAL SIZE
    u32 blockSize;
    u32 blocks; // do arquivo comprimido final - juntando blockSize*blocks = 64 bits
    u16 threadBuffInBlocks;
    u16 threadBuffOutBlocks;
    u16 threadsN; // NUMBER OF THREADS
    u8  compression; // COMPRESSION FORMAT
    u8  chunkODiffSize;
    u8  chunkSizeSize;
    u8  cChunkSize;
    u8  cChunkBitsBlocks;
    u8  cChunkBitsThread;
    u32 endSize;
} Tail;

typedef struct ThreadResult {
    u64 checksum;
    uint remainingSize;
    void* remaining;
} ThreadResult;

static volatile sig_atomic_t status = EXIT_SUCCESS;

#define READWORD(bits) ( switch )

// TODO: FIXME: BYTE ORDER NEUTRAL
// TODO: FIXME: SUPORTAR 24, 48 , 40, ETC
static inline uint READWORDUINT(const void* restrict const buff, uint size) {
    switch (size) {
        case 8:
            return *(u64*)buff;
        case 4:
            return *(u32*)buff;
        case 2:
            return *(u16*)buff;
        default: // 1
            return *(u8*)buff;
    }
}

static inline void WRITE_UINT(void* restrict const buff, uintll size, uint value) {
    switch (size) {
        case 8:
            *(u64*)buff = value;
            break;
        case 4:
            *(u32*)buff = value;
            break;
        case 2:
            *(u16*)buff = value;
            break;
        default: // 1
            *(u8*)buff = value;
    }
}

typedef u64 v32u64 __attribute__ ((vector_size (32)));

static inline u64 soma (const v32u64* restrict buff, uint size) {

    v32u64 checksum = { 0, 0, 0, 0 };

    size /= sizeof(v32u64);

    while (size--)
       checksum += *buff++;

    char x[32];

    *(v32u64*)x = checksum;

    return
        ((u64*)x)[0] +
        ((u64*)x)[1] +
        ((u64*)x)[2] +
        ((u64*)x)[3]
        ;
}

static uint  threadsN;
static void* threadsBuff;
static uint  threadBuffSize;
static uint  threadBuffInSize;
static uint  threadBuffOutSize;

static uint buffSizeIn;
static uint buffSizeOut;

static uint blockSize;

static uintll chunkODiffMask;
static uint   chunkODiffSize;
static uint   chunkSizeMask;
static uint   chunkSizeSize;

#define CCHUNK_THREAD(cchunk) ((cchunk) &   cChunkMaskThread)
#define CCHUNK_BLOCKS(cchunk) ((cchunk) >> (cChunkBitsThread))

static uint cChunkSize; // WORD SIZE IN BYTES
static uint cChunkBitsThread;
static uint cChunkBitsBlocks;
static u64  cChunkMaskThread;
static u64  cChunkMaskBlocks;
// primeiro LE A PALAVRA
// DEPOIS EXTRAI ESSES BITS

static uintll inOffset;
static uintll outOffset;

static volatile sig_atomic_t inFD;
static volatile sig_atomic_t outFD;

static pthread_mutex_t inLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t outLock = PTHREAD_MUTEX_INITIALIZER;

#define _RET_ERR ({ printf("!!! THREAD ERR %d\n", __LINE__); return THREAD_ERROR; })
//#define _RET_ERR return THREAD_ERROR

#define THREAD_SUCCESS 0
#define THREAD_ERROR 1 // SÃO TODOS ERROS FATAIS

// TODO: FIXME: CPU affinity

static inline u64 rdtsc(void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void* malloc_aligned (const uintll size, const uintll alignment) {

    void* ptr = NULL;

    if (posix_memalign(&ptr, alignment, size) || ptr == NULL)
        return NULL;

    return  ptr;
}

static void signal_handler(int signal, siginfo_t* signalInfo, void* signalData) {

    (void)signalInfo;
    (void)signalData;

    switch (signal) {
        case SIGTERM:
        case SIGINT:
            if (status == EXIT_SUCCESS)
                status = EXIT_TERMINATED;
            inFD = FD_INVALID;
            break;
        case SIGUSR1:
            break;
        case SIGUSR2:
            break;
        case SIGALRM:
            break;
        case SIGCHLD: // TODO: FIXME: foi do nosso in/out fd?
            break;
        case SIGPIPE:
            //inFD = FD_CLOSED; // TODO: FIXME: some other value para diferenciar de receber 0? :/
            // TODO: FIXME: identificar qual foi o signal handler
            break;
    }

}

static inline int compressor_(const uint threadID) {

    void* const in  = threadsBuff + threadID*threadBuffSize;
    void* const out = threadsBuff + threadID*threadBuffSize + threadBuffInSize;

    uint inSize = 0; // O QUANTO JÁ TEM NO BUFFER DE ENTRADA
    uint outSize = cChunkSize; // O QUANTO JÁ TEM NO BUFFER DE SAÍDA

    u64 offset = 0;
    u64 checksum = threadID;

    ZSTD_CCtx* const cctx = ZSTD_createCCtx();

    if (cctx == NULL)
        return THREAD_ERROR;

    // TODO: FIXME:
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 18);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

    while (inFD != FD_INVALID) {

        // SE ESTÁ QUERENDO LER, TEM QUE TER ESPAÇO
        if ((inSize + 16) >= threadBuffInSize)
            _RET_ERR;

        // LÊ DA STREAM
        if(pthread_mutex_lock(&inLock))
            _RET_ERR;

        if (inFD != FD_INVALID) { int size; //  TODO: FIXME: ENCAPSULAR READ E WRITE PAA A LIDAR COM SIGNALS
            if ((size = read(inFD, in + inSize + chunkODiffSize + chunkSizeSize, threadBuffInSize - inSize - chunkODiffSize - chunkSizeSize)) > 0) {
                const u64 oDiff = inOffset - offset;
                if (oDiff > chunkODiffMask) // TODO: FIME: manter uma variavel threadAtrasada, threadAtrasadaOffset, e um lock; lockar, ver se é a pior, setar ; quando uma ver que a  outra esta muito atrasada, manter o lock; {a que está atrasada, lendo o atrasadaFD SEM LOCK, ver que é == ela mesma, dar o release se o (threadAdiantada - self->offset) - 0xFFFFFFFFULL } |||
                    _RET_ERR;
                WRITE_UINT(in + inSize, chunkODiffSize, oDiff); inSize += chunkODiffSize; // ONDE ESTÁ ESTE CHUNK
                WRITE_UINT(in + inSize, chunkSizeSize,  size);  inSize += chunkSizeSize;  // TAMANHO DESTE CHUNK
                inSize += size; // AGORA SIM CONSIDERA ISSO TUDO
                inOffset += size; // A PRÓXIMA LIDA ESTARÁ NESTE NOVO OFFSET
                if (offset >= inOffset)
                    _RET_ERR;
                offset = inOffset;
            } elif (size) {
                dbg("READ INPUT FAILED/INTERRUPTED");
                if (errno != EINTR) {
                    dbg("READ INPUT FAILED");
                    if (status == EXIT_SUCCESS)
                        status = EXIT_IN_ERR; // TODO: FIXME: s eusar co  mo flags, devemos dar um lock -> user statusInErr = 1
                    inFD = FD_INVALID;
                }
            } else {
                dbg("READ CLOSED");
                inFD = FD_INVALID;
                //SE TERMINAR POR CAUSA DO SIGTERM-> TERMINATED // by signal, etc
            }
        }

        if (pthread_mutex_unlock(&inLock))
            _RET_ERR;

        if (inSize >= (32*1024)) {
            // COMPRESS

            if (inSize > threadBuffInSize)
                _RET_ERR;
            if (outSize > threadBuffOutSize)
                _RET_ERR;

            ZSTD_inBuffer input = { in, inSize, 0 }; // BUFF, BUFF_SIZE, OFFSET
            ZSTD_outBuffer output = { out, threadBuffOutSize, outSize };

            const uint remaining = ZSTD_compressStream2(cctx, &output , &input, ZSTD_e_continue);

            if (ZSTD_isError(remaining))
                _RET_ERR;

            //CHECK_ZSTD(remaining);
            const uint inRemaining = inSize - input.pos;

            // move de volta o restante
            memmove(in, in + input.pos, inRemaining);

            inSize = inRemaining; // TODO: FIXME: é o input.pos?
            outSize = output.pos;

            if (inSize > threadBuffInSize)
                _RET_ERR;
            if (outSize > threadBuffOutSize)
                _RET_ERR;

            if ((threadBuffOutSize - outSize) < 32) {

                const uint outSizeAligned = (outSize / blockSize) * blockSize;

                WRITE_UINT(out, cChunkSize, ((outSizeAligned / blockSize) << cChunkBitsThread) | threadID);

                checksum += soma(out, outSizeAligned);

                if (pthread_mutex_lock(&outLock))
                    _RET_ERR; // SE O FD ESTIVER MARCADO COMO INVALIDO, O WRITE VAI DAR ERRO
                if (write(outFD, out, outSizeAligned) != outSizeAligned) { // TODO: FIXME: HANDLE SIGNALS
                    if (status == EXIT_SUCCESS)
                        status = EXIT_OUT_ERR;
                    outFD = FD_INVALID;
                    inFD  = FD_INVALID; // SE O OUT ESTÁ RUIM, PARA DE CONSUMIR O IN TAMBÉM
                } else {
                    outOffset += outSizeAligned;
                }
                if (pthread_mutex_unlock(&outLock))
                    _RET_ERR;
                // SE DEI ERRO NA ESCRITA, FINGE QUE ESCREVEU MESMO E SEGUE EM FRENTE POR ENQUANTO
                // TIRA O QUE FOI, FICANDO O REMAINING
                outSize -= outSizeAligned;
                // MOVE O REMAINING PARA DEPOIS DO COMPRESSED SIZE
                memmove(out + cChunkSize, out + outSizeAligned, outSize);
                // CONSIDERA O RESTANTE
                outSize += cChunkSize;
            }
        }
    }

    // NÃO TEM MAIS NADA PARA LER

    // FLUSH O QUE TEM QUE COMPRIMIR
    if (inSize) {

        ZSTD_inBuffer input = { in, inSize, 0 }; // BUFF, BUFF_SIZE, OFFSET
        ZSTD_outBuffer output = { out, threadBuffOutSize, outSize };

        const int remaining = ZSTD_compressStream2(cctx, &output , &input, ZSTD_e_end);

        if (ZSTD_isError(remaining))
            _RET_ERR;

        // CHECK_ZSTD(remaining);

        // TEM QUE TER CONSUMIDO TUDO
        if (inSize != (uint)input.pos)
            _RET_ERR;

        //
        outSize = output.pos;
    }

    uint outSizeAligned = outSize;

    // FLUSH O QUE TEM QUE ESCREVER
    if (outSizeAligned != cChunkSize) {
        if ((outSizeAligned = (outSizeAligned / blockSize) * blockSize)) {

            WRITE_UINT(out, cChunkSize, ((outSizeAligned / blockSize) << cChunkBitsThread) | threadID);

            checksum += soma(out, outSizeAligned);

            if (pthread_mutex_lock(&outLock))
                _RET_ERR;
            if (write(outFD, out, outSizeAligned) == outSizeAligned) {
                outOffset += outSizeAligned;
            } else {
                if (status == EXIT_SUCCESS)
                    status = EXIT_OUT_ERR;
                outFD = FD_INVALID;
                inFD =  FD_INVALID; // SE O OUT ESTÁ RUIM, PARA DE CONSUMIR O IN
            }
            if (pthread_mutex_unlock(&outLock))
                _RET_ERR;
        }
    } else  {
        outSize = 0;
        outSizeAligned = 0;
    }

    // DEIXA O PONTEIRO DO REMAINING E O TAMANHO DELE
    ThreadResult* const result = in;

    result->remaining     = out     + outSizeAligned;
    result->remainingSize = outSize - outSizeAligned;
    result->checksum      = 0;

    dbg("THREAD %u SUCCESS - RESULT @ %p REMAINING SIZE %u CHECKSUM %016llX OFFSET %llu", threadID, (void*)result, result->remainingSize, (uintll)result->checksum, (uintll)offset);

    return THREAD_SUCCESS;
}

static void* compressor (void* threadID_) {
    if (compressor_((uint)(intptr_t)threadID_)) {
        dbg("THREAD %d FAILED", (uint)(intptr_t)threadID_);
        status = EXIT_FATAL;
        inFD  = FD_INVALID;
        outFD = FD_INVALID;
        // abort(); ou _exit(????);
    }
    return NULL;
}

// TODO: FIXME:
//      szstd THREADS_N IN_BUFF_SIZE buffSizeOut OUTPUT_FILENAME READ_FD
//      szstd THREADS_N IN_BUFF_SIZE buffSizeOut OUTPUT_FILENAME WRITE_FD COMMAND
//          READ_FD - fd que temos de usar para ler - se não for especificado, é o standard input
//          WRITE_FD - fd que o comando usará para escrever para nós
// if (fd <= STDERR_FILENO) close(fd), abrir o arquivo if(fd != filefd) dup2(,)  executar o comando
int main (void) {

    // DEFAULTS
    threadsN = 0;
    blockSize = 0;
    inFD = FD_INVALID;
    inOffset = 0;
    outFD = FD_INVALID;
    outOffset = 0;

    //
    threadsN = 7;
    blockSize = 65536;

    buffSizeIn  = 100*1024*1024;
    buffSizeOut = 100*1024*1024;

    chunkODiffMask = 0xFFFFFFFF;

    // PER THREAD SIZES
    threadBuffInSize  = ALIGNED(buffSizeIn  / threadsN, blockSize);
    threadBuffOutSize = ALIGNED(buffSizeOut / threadsN, blockSize);

    threadBuffSize = threadBuffInSize + threadBuffOutSize;

    cChunkBitsThread = VALBITS(threadsN - 1); // IT WILL STORE ONLY THE LAST ONE, NOT THE QUANTITY OF ALL
    cChunkBitsBlocks = VALBITS(threadBuffOutSize / blockSize); // threadBuffOutSize é TAMANHO MÁXIMO DE UM COMPRESSED BLOCK - COMPRESSED_SIZE EM |[THREAD_ID|COMPRESSED_SIZE|COMPRESSED] REMAINING|
    cChunkMaskThread = VALMASK(threadsN - 1);
    cChunkMaskBlocks = VALMASK(threadBuffOutSize / blockSize) << cChunkBitsThread;
    cChunkSize       = VALSIZE(cChunkMaskBlocks | cChunkMaskThread);

    // APROVEITA TODA A PALAVRA
    chunkSizeSize = VALSIZE(threadBuffInSize);
    chunkSizeMask = (1ULL << (chunkSizeSize*8)) - 1;

    chunkODiffSize = VALSIZE(chunkODiffMask);
    chunkODiffMask = (1ULL << (chunkODiffSize*8)) - 1;

    //
    const uint buffSizeEnd = ALIGNED(sizeof(Tail), blockSize);

    // BUFFER SIZE
    const uint buffSize = buffSizeEnd + threadsN * threadBuffSize;

    char fname[64];

    // GENERATE A FILENAME WITH TIMESTAMP AND RANDOM
    // TODO: FIXME: em milésimos de segundo
    const u64 time_ = (u64)time(NULL) * 1000;
    const u64 random = rdtsc();

    sprintf(fname, "%016llX%016llX", (uintll)time_, (uintll)random);

    //
    dbg("blockSize         %9u blocks %9u",  blockSize, blockSize/blockSize);
    dbg("buffSize          %9u blocks %9u",  buffSize, buffSize/blockSize);
    dbg("buffSizeEnd       %9u blocks %9u",  buffSizeEnd, buffSizeEnd/blockSize);
    dbg("buffSizeIn        %9u blocks %9u",  buffSizeIn, buffSizeIn/blockSize);
    dbg("buffSizeOut       %9u blocks %9u",  buffSizeOut, buffSizeOut/blockSize);
    dbg("threadsN          %9u",             threadsN);
    dbg("threadBuffSize    %9u blocks %9u",  threadBuffSize, threadBuffSize/blockSize);
    dbg("threadBuffInSize  %9u blocks %9u" , threadBuffInSize, threadBuffInSize/blockSize);
    dbg("threadBuffOutSize %9u blocks %9u",  threadBuffOutSize, threadBuffOutSize/blockSize);
    dbg("cChunkSize        %9u",             cChunkSize);
    dbg("cChunkBitsBlocks  %9u",             cChunkBitsBlocks);
    dbg("cChunkBitsThread  %9u",             cChunkBitsThread);
    dbg("cChunkMaskBlocks  %9llu",   (uintll)cChunkMaskBlocks);
    dbg("cChunkMaskThread  %9llu",   (uintll)cChunkMaskThread);
    dbg("sizeof(Tail)      %9u",  (uint)sizeof(Tail));
    dbg("OUTPUT            %s",   fname);
    dbg("TIME              %llu",      (uintll)time_);
    dbg("RANDOM            0x%016llX", (uintll)random);

    // TEM QUE SER ALINHADO AO BLOCK SIZE
    if (buffSizeIn        % blockSize)
        return EXIT_FATAL;
    if (buffSizeOut       % blockSize)
        return EXIT_FATAL;
    if (threadBuffSize    % blockSize)
        return EXIT_FATAL;
    if (threadBuffInSize  % blockSize)
        return EXIT_FATAL;
    if (threadBuffOutSize % blockSize)
        return EXIT_FATAL;

    // POR CAUSA DO CHECKSUM
    if (blockSize         % sizeof(v32u64))
        return EXIT_FATAL;
    if (threadBuffOutSize % sizeof(v32u64))
        return EXIT_FATAL;

    // TEM QUE CABER
    if (threadBuffInSize > chunkSizeMask)
        return EXIT_FATAL;

    // DÁ PARA ESCREVER O BUFFER FULL?
    if ((threadBuffOutSize / blockSize) > CCHUNK_BLOCKS(cChunkMaskBlocks))
        return EXIT_FATAL;
    // DÁ PARA ESCREVER A ÚLTIMA THREAD?
    if ((threadsN - 1)                  > CCHUNK_THREAD(cChunkMaskThread))
        return EXIT_FATAL;

    // UM MÍNIMO
    if (threadsN < 1)
        return EXIT_FATAL;
    if (threadBuffInSize < (512*1024))
        return EXIT_FATAL;
    if (cChunkBitsThread < 1)
        return EXIT_FATAL;
    if (cChunkBitsBlocks < 1)
        return EXIT_FATAL;
    if (chunkODiffMask < 0xFF)
        return EXIT_FATAL;
    if (chunkSizeMask < 0xFF)
        return EXIT_FATAL;

    // NÃO COLIDEM
    if ((cChunkMaskBlocks | cChunkMaskThread) != (cChunkMaskBlocks + cChunkMaskThread))
        return EXIT_FATAL;

    //
    if (sizeof(Tail) > blockSize)
        return EXIT_FATAL;

    //
    if (!(
        (chunkODiffMask   == ((1ULL << (chunkODiffSize*8)) - 1)) &&
        (cChunkMaskThread == ((1ULL << (cChunkBitsThread)) - 1)) &&
        (cChunkMaskBlocks   == (((1ULL << (cChunkBitsBlocks  )) - 1) << cChunkBitsThread))
        ))
        return EXIT_FATAL;

    //
    if (VALSIZE(chunkSizeMask) != chunkSizeSize)
        return EXIT_FATAL;
    if (VALSIZE(chunkODiffMask) != chunkODiffSize)
        return EXIT_FATAL;

    // BUFFER
    void* const buff = malloc_aligned(buffSize, blockSize);

    if (buff == NULL)
        return EXIT_FATAL;

    // COMEÇO DO BUFFER DAS THREADS
    threadsBuff = buff + buffSizeEnd;

    // DEIXA O ESPAÇO RESERVADO PARA O HEADER, E JÁ CONFIRMA QUE O ALINHAMENTO, ESCRITA ETC ESTÁ FUNCIONANDO
    //if (write(outFD, buff, HEADER_SIZE) != HEADER_SIZE) // TODO: FIXME: escrever um header mínimo
        //return EXIT_FATAL;

    // INPUT FD
    inFD = STDIN_FILENO;

    //
    //threadCount = threadsN;
    // dai dar um lock nisso e deiminuir toda vez que umaestiver a encerrar
    // a ultima vai escrever o block size
    //

    // CRIA E ABRE O ARQUIVO COMPRESSED
    if ((outFD = open(fname, O_WRONLY | O_CREAT | O_EXCL | O_DIRECT | O_SYNC | O_NOCTTY | O_CLOEXEC, 0444)) == -1)
        return EXIT_FATAL;

    // INSTALA O SIGNAL HANDLER
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_sigaction = signal_handler;
    action.sa_flags = SA_SIGINFO;

    if (sigaction(SIGTERM,   &action, NULL) ||
        sigaction(SIGUSR1,   &action, NULL) ||
        sigaction(SIGUSR2,   &action, NULL) ||
        sigaction(SIGIO,     &action, NULL) || // SIGPOLL
        sigaction(SIGURG,    &action, NULL) ||
        sigaction(SIGPIPE,   &action, NULL) ||
        sigaction(SIGPROF,   &action, NULL) ||
        sigaction(SIGHUP,    &action, NULL) ||
        sigaction(SIGQUIT,   &action, NULL) ||
        sigaction(SIGINT,    &action, NULL) ||
        sigaction(SIGCHLD,   &action, NULL) ||
        sigaction(SIGALRM,   &action, NULL) ||
        sigaction(SIGCONT,   &action, NULL) ||
        sigaction(SIGVTALRM, &action, NULL)
        ) return EXIT_FATAL; // fatal("FAILED TO INSTALL SIGNAL HANDLER")

    // LAUNCH ALL
    pthread_t threads[threadsN];

    uint threadID;

    if (pthread_mutex_lock(&inLock))
        return EXIT_FATAL;

    threadID = threadsN;

    while (threadID--)
        if (pthread_create(&threads[threadID], NULL, compressor, (void*)(intptr_t)threadID))
            return EXIT_FATAL;

    if (pthread_mutex_unlock(&inLock))
        return EXIT_FATAL;

    // WAIT THEM TO FINISH
    threadID = threadsN;

    while (threadID--)
        if (pthread_join(threads[threadID], NULL))
            return EXIT_FATAL;

    // ESCREVE O HEADER
    if (outFD != FD_INVALID) {

        dbg("WRITING END");

        void* const end = buff;
        void* end_      = buff;

        threadID = 0;

        do {
            const ThreadResult* const result = threadsBuff + threadID*threadBuffSize;
            // NÃO TEM RISCO DE SOBRESCREVER, POR QUE A CADA THREAD LIBERA AO MENOS 1 DO OUT BUFF + IN BUFF, E A PRIMEIRA TEM O HEADER SIZE ALIGNED
            dbg("THREAD %d RESULT @ %p REMAINING SIZE %u CHECKSUM 0x%016llX", threadID, (void*)result, result->remainingSize, (uintll)result->checksum);
            *(u64*)end_ = result->checksum;      end_ += sizeof(u64);
            *(u16*)end_ = result->remainingSize; end_ += sizeof(u16);
            memmove(end_, result->remaining, result->remainingSize); end_ += result->remainingSize; // REMAINING
        } while (++threadID != threadsN);

        const uint endSize = end_ - buff; // TAMANDO EFETIVO
        const uint endSizeAligned = ALIGNED(endSize + sizeof(Tail), blockSize); // TAMANHO STORED

        dbg("END SIZE %u",                  endSize);
        dbg("END SIZE ALIGNED %u",          endSizeAligned);

        //
        outOffset += endSizeAligned;

        // Clear the padding and the tail
        memset(end_, 0, endSizeAligned - endSize);

        Tail* const tail = end + endSizeAligned - sizeof(Tail);

        tail->check               = CHECK;
        tail->checksum            = 0;
        tail->time                = time_;
        tail->random              = random;
        tail->size                = inOffset;
        tail->blockSize           = blockSize;
        tail->blocks              = outOffset / blockSize;
        tail->threadBuffInBlocks  = threadBuffInSize / blockSize;
        tail->threadBuffOutBlocks = threadBuffOutSize / blockSize;
        tail->threadsN            = threadsN;
        tail->chunkODiffSize      = chunkODiffSize;
        tail->chunkSizeSize       = chunkSizeSize;
        tail->cChunkSize          = cChunkSize;
        tail->cChunkBitsBlocks    = cChunkBitsBlocks;
        tail->cChunkBitsThread    = cChunkBitsThread;
        tail->compression         = 0;
        tail->endSize             = endSize; // NOTA: transformar no endSizeAligned, ler ele, e ai usar o endSize

        const u64 endChecksum = soma(end, endSizeAligned);

        dbg("END CHECKSUM %016llX", (uintll)endChecksum);

        tail->checksum = endChecksum; // TODO: FIXME: CHECKSUM DESTE HEADER

        if (write(outFD, end, endSizeAligned) != endSizeAligned) { // TODO: FIXME: HANDLE SIGNALS - SÓ ENCAPSULAR NUMA FUNÇÃO
            dbg("FAILED TO WRITE END");
            outFD = FD_INVALID;
        }
    }

    //
    free(buff);

    // CLOSE BOTH AT THE SAME TIME
    // se output foi criado por nós, então fecharmos ele primeiro
    //close(outFD);
    //close(inFD);
    dbg("INPUT  FD %d OFFSET %llu", inFD,  (uintll)inOffset);
    dbg("OUTPUT FD %d OFFSET %llu", outFD, (uintll)outOffset);

    if (inOffset == 0) {
        if   (status == EXIT_SUCCESS)    status = EXIT_SUCCESS_NOTHING;
        elif (status == EXIT_TERMINATED) status = EXIT_TERMINATED_NOTHING;
        elif (status == EXIT_IN_ERR)     status = EXIT_IN_ERR_NOTHING;
    }

    // huuumm... mas se já fechou o input, então terminou com sucesso -> usa r tudo +1 inclusiv eosuccess, e depois - 1 ao sair
    dbg("EXIT STATUS: %d - %s", status,
        (status == EXIT_SUCCESS)            ? "SUCCESS" :
        (status == EXIT_SUCCESS_NOTHING)    ? "SUCCESS_NOTHING" :
        (status == EXIT_TERMINATED)         ? "TERMINATED" :
        (status == EXIT_TERMINATED_NOTHING) ? "TERMINATED_NOTHING" :
        (status == EXIT_IN_ERR)             ? "IN_ERR" :
        (status == EXIT_IN_ERR_NOTHING)     ? "IN_ERR_NOTHING" :
        (status == EXIT_OUT_ERR)            ? "OUT_ERR" :
        (status == EXIT_FATAL)              ? "FATAL" :
        "???");

    return status;
}

// o  hash do arquivo será TIME-RANDOM-ORIGINAL_SIZE-COMPRESSED_SIZE-CHECKSUM
// como o threadsN e os demais checksums estão nesse end, inclusive todo oremaining, assim como todos o parâmetros que influenciaram nesse checksumming, então eles refletem no checksum dele e ele identifica bem o original
// POREM este checksum depende a ordem e tamanho dos reads().

// cria descomprimido
// fseek(outFD, header->size)
// mmap()
// decompres in thread buffers
// lista os iovecs disponíveis
// pwritev
//      se o ultimo ainda nao terminou, arrasta para trás
//


// TODO :FIXME: terminar as demais threads que stão dando um read() :/

// mmap
// cada thread vai lendo
// se ainda nao chegou no offset deste, espera chegar
//  a thread com a posição atual, mantem o lock e desloca quando depender de outra!!!
//      xD


//na hroa d edar o flush final, tem q ue ter de espaço no OUT = TAMANHO TOTAL DO IN + RESERVA
// sem problema, basta comprimir o máximo, escrever, esvaziar o buffer out, e comprimir orestante
//           entao OUT está vazio, e IN é no máximo IN_SIZE

    //if ((counter % threadsN) != threadID) {
        //loop {
            //lock(inLock);
            //if ((counter % threadsN) == threadID)
                //break;
            //release(inLock);
        //}
    //}

    //// ...

    //release(inLock); // tem que liberar primeiro; eles nao vao avancar mesmo enquanto nao mudar
    //inCounter++;

//  inicia no main
//inCounter = 0;
//inLock
