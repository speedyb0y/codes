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

#define MAGIC 0x84E946564115E598ULL

// TODO: FIXME: tamanho de certas coisas, posicao delas etc
#define CHECK ( \
              sizeof(Tail) \
    + ((u64)offsetof(Tail, check)               <<  1) \
    + ((u64)offsetof(Tail, checksum)            <<  2) \
    + ((u64)offsetof(Tail, time)                <<  3) \
    + ((u64)offsetof(Tail, random)              <<  4) \
    + ((u64)offsetof(Tail, size)                <<  5) \
    + ((u64)offsetof(Tail, threadsN)            <<  5) \
    + ((u64)offsetof(Tail, compression)         <<  6) \
    + ((u64)offsetof(Tail, dictSize)            <<  7) \
    + ((u64)offsetof(Tail, blockSize)           <<  8) \
    + ((u64)offsetof(Tail, blocks)              <<  9) \
    )

    //+ ((u64)sizeof  (HeaderThread)                << 13)
    //+ ((u64)offsetof(HeaderThread, dictSize)      << 14)
    //+ ((u64)offsetof(HeaderThread, remainingSize) << 15)
    //+ ((u64)offsetof(HeaderThread, checksum)      << 16)

// TODO: FIXME: enforce um tamanho para isso
typedef struct Tail {
    u64 magic;
    u64 check;
    u64 checksum; // DO END INTEIRO, ALINHADO
    u64 time; // em milésimos de segundo
    u64 random;
    u64 size; // ORIGINAL SIZE
    u32 compression; // COMPRESSION FORMAT
    u32 dictSize; // DICTIONARY SIZE (DEFAULT, CADA THREAD PODE TER O SEU) - SE TIVER, FICA APÓS O REMAINING
    u32 blockSize;
    u32 blocks; // do arquivo comprimido final - juntando blockSize*blocks = 64 bits
    u32 threadInBuffSize;
    u32 threadOutBuffSize;
    u8  threadsMax;
    u8  threadsN; // NUMBER OF THREADS
    u8  chunkODiffSize;
    u8  chunkSizeSize;
    u8  reserved;
    u8  cChunkSize;
    u8  cChunkBitsSize;
    u8  cChunkBitsThread;
    u64 endSize;
} Tail; // seguido pelo dict

typedef struct ThreadResult {
    u64 checksum;
    uint dictSize;
    uint remainingSize;
    void* remaining;
} ThreadResult;

static volatile sig_atomic_t status = EXIT_SUCCESS;

#define EXIT_SUCCESS            0
#define EXIT_SUCCESS_NOTHING    1 // OK BUT NOTHING WAS READ
#define EXIT_TERMINATED         3 // BUT FINISHED
#define EXIT_TERMINATED_NOTHING 4 // WAS STILL INITIALIZING
#define EXIT_IN_ERR             5 // BUT FINISHED
#define EXIT_IN_ERR_NOTHING     6 // ERROR ON INPUT - NOTHING READ
#define EXIT_OUT_ERR            7 // NOT FINISHED
#define EXIT_FATAL              8

static u64 outOffset = 0;

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

static uint threadsMax    = 32;

static uint blockSize     = 65536;

static u64  chunkODiffMask   = 0xFFFFFFFFULL;
static uint chunkODiffSize   = 4;
static u64  chunkSizeMask    = 0xFFFFFFFFULL;
static uint chunkSizeSize    = 4;

static uint cChunkSize       = 2;
static uint cChunkBitsThread = 5;
static uint cChunkBitsSize   = 11;
static uint cChunkMask       = 0b1111111111111111U; // ((1ULL << (cChunkSize    *8)) - 1)
static uint cChunkMaskThread = 0b0000000000011111U;
static uint cChunkMaskSize   = 0b1111111111100000U; // TODO: FIXME: não uar isso, usar os n bits direto
// primeiro LE A PALAVRA
// DEPOIS EXTRAI ESSES BITS

static void* threadsBuff;

static u64 inOffset;
//static u64 outOffset;

static volatile sig_atomic_t inFD;
static volatile sig_atomic_t outFD;

static uint threadBuffSize;
static uint threadInBuffSize; // per thread
static uint threadOutBuffSize;

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

static inline int compressor_(const int threadID) {

    void* const in  = threadsBuff + threadID*threadBuffSize;
    void* const out = threadsBuff + threadID*threadBuffSize + threadInBuffSize;

    uint inSize = 0; // O QUANTO JÁ TEM NO BUFFER DE ENTRADA

    // O QUANTO JÁ TEM NO BUFFER DE SAÍDA
    uint outSize = cChunkSize;

    ZSTD_CCtx* const cctx = ZSTD_createCCtx();

    if (cctx == NULL)
        return THREAD_ERROR;

    // TODO: FIXME:
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 18);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

    u64 offsetLast = 0;

    while (inFD >= 0) {

        // SE ESTÁ QUERENDO LER, TEM QUE TER ESPAÇO
        if ((inSize + 16) >= threadInBuffSize)
            _RET_ERR;

        // LÊ DA STREAM
        if(pthread_mutex_lock(&inLock))
            _RET_ERR;

        if (inFD != FD_INVALID) { int size; //  TODO: FIXME: ENCAPSULAR READ E WRITE PAA A LIDAR COM SIGNALS
            if ((size = read(inFD, in + inSize + chunkODiffSize + chunkSizeSize, threadInBuffSize - inSize - chunkODiffSize - chunkSizeSize)) > 0) {
                const u64 oDiff = inOffset - offsetLast;
                if (oDiff >= chunkODiffMask) // TODO: FIME: manter uma variavel threadAtrasada, threadAtrasadaOffset, e um lock; lockar, ver se é a pior, setar ; quando uma ver que a  outra esta muito atrasada, manter o lock; {a que está atrasada, lendo o atrasadaFD SEM LOCK, ver que é == ela mesma, dar o release se o (threadAdiantada - self->offset) - 0xFFFFFFFFULL } |||
                    _RET_ERR;
                WRITE_UINT(in + inSize, chunkODiffSize, oDiff); inSize += chunkODiffSize; // ONDE ESTÁ ESTE CHUNK
                WRITE_UINT(in + inSize, chunkSizeSize, size);                   inSize += chunkSizeSize;  // TAMANHO DESTE CHUNK
                inSize += size; // AGORA SIM CONSIDERA ISSO TUDO
                inOffset += size; // A PRÓXIMA LIDA ESTARÁ NESTE NOVO OFFSET
                offsetLast = inOffset;
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

            if (inSize > threadInBuffSize)
                _RET_ERR;
            if (outSize > threadOutBuffSize)
                _RET_ERR;

            ZSTD_inBuffer input = { in, inSize, 0 }; // BUFF, BUFF_SIZE, OFFSET
            ZSTD_outBuffer output = { out, threadOutBuffSize, outSize };

            const uint remaining = ZSTD_compressStream2(cctx, &output , &input, ZSTD_e_continue);

            if (ZSTD_isError(remaining))
                _RET_ERR;

            //CHECK_ZSTD(remaining);
            const uint inRemaining = inSize - input.pos;

            // move de volta o restante
            memmove(in, in + input.pos, inRemaining);

            inSize = inRemaining; // TODO: FIXME: é o input.pos?
            outSize = output.pos;

            if (inSize > threadInBuffSize)
                _RET_ERR;
            if (outSize > threadOutBuffSize)
                _RET_ERR;

            if ((threadOutBuffSize - outSize) < 32) {

                const uint outSizeAligned = (outSize / blockSize) * blockSize;

                WRITE_UINT(out, cChunkSize, ((outSizeAligned / blockSize) << cChunkBitsThread) | threadID);

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
        ZSTD_outBuffer output = { out, threadOutBuffSize, outSize };

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
    result->dictSize      = 0;

    dbg("THREAD %d SUCCESS - RESULT @ %p REMAINING SIZE %d CHECKSUM %016llX OFFSET LAST %lld", threadID, (void*)result, result->remainingSize, (uintll)result->checksum, (uintll)offsetLast);

    return THREAD_SUCCESS;
}

static void* compressor (void* threadID_) {
    if (compressor_((int)(intptr_t)threadID_)) {
        dbg("THREAD %d FAILED", (int)(intptr_t)threadID_);
        status = EXIT_FATAL;
        inFD  = FD_INVALID;
        outFD = FD_INVALID;
        // abort(); ou _exit(????);
    }
    return NULL;
}

// TODO: FIXME:
//      szstd THREADS_N IN_BUFF_SIZE outBuffSize OUTPUT_FILENAME READ_FD
//      szstd THREADS_N IN_BUFF_SIZE outBuffSize OUTPUT_FILENAME WRITE_FD COMMAND
//          READ_FD - fd que temos de usar para ler - se não for especificado, é o standard input
//          WRITE_FD - fd que o comando usará para escrever para nós
// if (fd <= STDERR_FILENO) close(fd), abrir o arquivo if(fd != filefd) dup2(,)  executar o comando
int main (void) {

    // INSTALA O SIGNAL HANDLER
    inFD = FD_INVALID;
    outFD = FD_INVALID;

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

    uint threadsN = 7;

    uint inBuffSize  = 100*1024*1024;
    uint outBuffSize = 100*1024*1024;

    // TODO: FIXME: pegaro threadsN e arredondar para potência de 2
    // threadsMax

    // PER THREAD SIZES
    threadInBuffSize  = ALIGNED(inBuffSize  / threadsN, blockSize);
    threadOutBuffSize = ALIGNED(outBuffSize / threadsN, blockSize);

    threadBuffSize = threadInBuffSize + threadOutBuffSize;

    //
    const uint endBuffSize = ALIGNED(sizeof(Tail), blockSize);

    // BUFFER SIZE
    const uint buffSize = endBuffSize + threadsN * threadBuffSize;

    dbg("blockSize %d", blockSize);
    dbg("threadsN %d", threadsN);
    dbg("threadsMax %d", threadsMax);

    dbg("inBuffSize        %5u",  inBuffSize);
    dbg("outBuffSize       %5u",  outBuffSize);
    dbg("threadBuffSize    %5u",  threadBuffSize);
    dbg("threadInBuffSize  %5u" , threadInBuffSize);
    dbg("threadOutBuffSize %5u",  threadOutBuffSize);
    dbg("cChunkSize        %u",   cChunkSize);
    dbg("cChunkBitsSize    %u",   cChunkBitsSize);
    dbg("cChunkBitsThread  %u",   cChunkBitsThread);

    // CHECKS
    if (inBuffSize  % blockSize) return EXIT_FATAL;
    if (outBuffSize % blockSize) return EXIT_FATAL;

    if (threadBuffSize % blockSize)
        return EXIT_FATAL;
    if (threadInBuffSize  % blockSize)
        return EXIT_FATAL;
    if (threadOutBuffSize % blockSize)
        return EXIT_FATAL;

    if (threadInBuffSize > chunkSizeMask)
        return EXIT_FATAL;

    // DARIA PARA ESCREVER O BUFFER FULL?
    if (threadOutBuffSize > ((cChunkMaskSize >> cChunkBitsThread) * blockSize))
        return EXIT_FATAL;
    // A  ULTIMA THREAD VAI CABER?
    if ((threadsMax - 1) != cChunkMaskThread)
        return EXIT_FATAL;

    if ((512*1024 + 64) >= threadInBuffSize)
        return EXIT_FATAL;

    //
    if (!(
        (threadsN >= 1) &&
        (threadsN <= threadsMax) &&
        (cChunkBitsSize >= 1) &&
        (cChunkBitsThread >= 1) &&
        ((cChunkBitsSize + cChunkBitsThread) == (cChunkSize*8)) &&
        (chunkODiffMask   == ((1ULL << (chunkODiffSize*8)) - 1)) &&
        (cChunkMaskThread == ((1ULL << (cChunkBitsThread)) - 1)) &&
        (cChunkMaskSize   == (((1ULL << (cChunkBitsSize  )) - 1) << cChunkBitsThread)) &&
        (cChunkMask       == (cChunkMaskThread ^ cChunkMaskSize))
        ))
        return EXIT_FATAL;

    // offset chunksize

    char fname[64];

    // GENERATE A FILENAME WITH TIMESTAMP AND RANDOM
    // TODO: FIXME: em milésimos de segundo
    const u64 time_ = (u64)time(NULL) * 1000;
    const u64 random = rdtsc();

    sprintf(fname, "%016llX%016llX", (uintll)time_, (uintll)random);

    dbg("OUTPUT %s\n", fname);

    // CRIA E ABRE O ARQUIVO COMPRESSED
    if ((outFD = open(fname, O_WRONLY | O_CREAT | O_EXCL | O_DIRECT | O_SYNC | O_NOCTTY | O_CLOEXEC, 0444)) == -1)
        return EXIT_FATAL;

    // BUFFER
    void* const buff = malloc_aligned(buffSize, blockSize);

    if (buff == NULL)
        return EXIT_FATAL;

    // COMEÇO DO BUFFER DAS THREADS
    threadsBuff = buff + endBuffSize;

    // DEIXA O ESPAÇO RESERVADO PARA O HEADER, E JÁ CONFIRMA QUE O ALINHAMENTO, ESCRITA ETC ESTÁ FUNCIONANDO
    //if (write(outFD, buff, HEADER_SIZE) != HEADER_SIZE) // TODO: FIXME: escrever um header mínimo
        //return EXIT_FATAL;

    // INPUT FD
    inFD = STDIN_FILENO;

    // INPUT OFFSET
    inOffset = 0;

    //
    //threadCount = threadsN;
    // dai dar um lock nisso e deiminuir toda vez que umaestiver a encerrar
    // a ultima vai escrever o block size
    //

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

        // POE O DICT...

        threadID = 0;

        do {
            const ThreadResult* const result = threadsBuff + threadID*threadBuffSize;
            // NÃO TEM RISCO DE SOBRESCREVER, POR QUE A CADA THREAD LIBERA AO MENOS 1 DO OUT BUFF + IN BUFF, E A PRIMEIRA TEM O HEADER SIZE ALIGNED
            dbg("THREAD %d RESULT @ %p REMAINING SIZE %u CHECKSUM 0x%016llX", threadID, (void*)result, result->remainingSize, (uintll)result->checksum);
            *(u64*)end_ = result->checksum;      end_ += sizeof(u64);
            *(u16*)end_ = result->remainingSize; end_ += sizeof(u16);
            *(u32*)end_ = result->dictSize;      end_ += sizeof(u32); // se o dicionário fosse computado na hora, teria de ficar num buffer após tudo
            memmove(end_, result->remaining, result->remainingSize); end_ += result->remainingSize; // REMAINING
            //memmove(end_, dict, 0); end_ += 0; // DICIONÁRIO
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

        tail->magic            = MAGIC;
        tail->check            = CHECK;
        tail->checksum         = 0;
        tail->time             = time_;
        tail->random           = random;
        tail->size             = inOffset;
        tail->threadsN         = threadsN;
        tail->threadsMax       = threadsMax;
        tail->cChunkSize       = cChunkSize;
        tail->reserved         = 0;
        tail->cChunkBitsSize   = cChunkBitsSize;
        tail->cChunkBitsThread = cChunkBitsThread;
        tail->chunkODiffSize   = chunkODiffSize;
        tail->chunkSizeSize    = chunkSizeSize;
        tail->compression      = 0;
        tail->dictSize         = 0;
        tail->blockSize        = blockSize;
        tail->blocks           = outOffset / blockSize;
        tail->threadInBuffSize   = threadInBuffSize;
        tail->threadOutBuffSize  = threadOutBuffSize;
        tail->endSize            = endSize; // NOTA: transformar no endSizeAligned, ler ele, e ai usar o endSize

        const u64 endChecksum = 0;

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
    dbg("INPUT  FD %d OFFSET %lld", inFD,  (uintll)inOffset);
    dbg("OUTPUT FD %d OFFSET %lld", outFD, (uintll)outOffset);

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
