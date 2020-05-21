/*
    zstd-hack

    Exit status
        0 - SUCCESS
        1 - FAILED TO OPEN/CREATE/READ/WRITE STDIN/STDOUT
        2 - SOME RUNTIME/FATAL ERROR

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

#define FD_CLOSED -1
#define FD_ERR -2

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

#define HEADER_SIZE 4096
#define BLOCK_SIZE 4096

#define MAGIC 0xFFFFFFFFFFFFFFFFULL
#define CHECK (sizeof(Header) + offsetof(Header, checksum))

typedef struct HeaderThread {
    u64 remainingSize;
    u64 checksum;
} HeaderThread;

typedef struct Header {
    u64 magic;
    u64 check;
    u64 time; // em milésimos de segundo
    u64 random;
    u64 blockSize;
    u64 blocks;
    u64 compression; // COMPRESSION FORMAT
    u64 dictOffset; // APÓS TODA A COMPRESSÃO
    u64 dictSize; // DICTIONARY SIZE
    u64 size; // ORIGINAL SIZE
    u64 checksum; // DESTE HEADER
    u64 threadsN; // NUMBER OF THREADS
    HeaderThread threads[]; // REMAINING SIZES + CHECKSUMS    - CALCULAR NA HORA O TAMANHO TOAL DOREMAININGS E ALINHAR
} Header; // seguido pelo dict

typedef struct ThreadResult {
    void* remaining;
    int remainingSize;
    u64 checksum;
} ThreadResult;

static void* buff_;

static u64 inOffset;

static volatile sig_atomic_t inFD;
static volatile sig_atomic_t outFD;

static int buffSize_;
static int inBuffSize_; // per thread
static int outBuffSize_;

static int outSizeFlush_;

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

    switch (signal) {
        case SIGTERM:
        case SIGINT:
            inFD = FD_ERR; // TODO: FIXME: some other value :/
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
            inFD = FD_CLOSED; // TODO: FIXME: some other value para diferenciar de receber 0? :/
            // TODO: FIXME: identificar qual foi o signal handler
            break;
    }

    (void)signalInfo;
    (void)signalData;
}

static inline int compressor_(const int threadID) {

    void* const in  = buff_ + threadID*buffSize_;
    void* const out = buff_ + threadID*buffSize_ + inBuffSize_;

    int inSize = 0; // O QUANTO JÁ TEM NO BUFFER DE ENTRADA

    // O QUANTO JÁ TEM NO BUFFER DE SAÍDA
    int outSize = sizeof(u16);

    ZSTD_CCtx* const cctx = ZSTD_createCCtx();

    if (cctx == NULL)
        return THREAD_ERROR;

    // TODO: FIXME:
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 18);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

    u64 offsetLast = 0;

    while (inFD >= 0) {

        // SE ESTÁ QUERENDO LER, TEM QUE TER ESPAÇO
        if ((int)(inSize + 16) >= inBuffSize_)
            _RET_ERR;
        if ((outSize + 1024) >= outBuffSize_)
            _RET_ERR;

        // LÊ DA STREAM
        if(pthread_mutex_lock(&inLock))
            _RET_ERR;

        if (inFD >= 0) { int size;
            if ((size = read(inFD, in + inSize + sizeof(u32) + sizeof(u32), inBuffSize_ - inSize - sizeof(u32) - sizeof(u32))) > 0) {
                if ((inOffset - offsetLast) >= 0xFFFFFFFFULL) // TODO: FIME: manter uma variavel threadAtrasada, threadAtrasadaOffset, e um lock; lockar, ver se é a pior, setar ; quando uma ver que a  outra esta muito atrasada, manter o lock; {a que está atrasada, lendo o atrasadaFD SEM LOCK, ver que é == ela mesma, dar o release se o (threadAdiantada - self->offset) - 0xFFFFFFFFULL } |||
                    _RET_ERR;
                *(u32*)(in + inSize) = inOffset - offsetLast; inSize += sizeof(u32); // ONDE ESTÁ ESTE CHUNK
                *(u32*)(in + inSize) = size;                  inSize += sizeof(u32); // TAMANHO DESTE CHUNK
                inSize += size; // AGORA SIM CONSIDERA ISSO TUDO
                inOffset += size; // A PRÓXIMA LIDA ESTARÁ NESTE NOVO OFFSET
                offsetLast = inOffset;
            } elif (size) {
                dbg("READ INPUT FAILED/INTERRUPTED");
                if (errno != EINTR) {
                    dbg("READ INPUT FAILED");
                    inFD = FD_ERR;
                }
            } else {
                dbg("READ CLOSED");
                inFD = FD_CLOSED;
            }
        }

        if (pthread_mutex_unlock(&inLock))
            _RET_ERR;

        if (inFD < 0)
            break;

        if (inSize >= (128*1024)) {
            // COMPRESS

            if (inSize > inBuffSize_)
                _RET_ERR;
            if (outSize > outBuffSize_)
                _RET_ERR;

            ZSTD_inBuffer input = { in, inSize, 0 }; // BUFF, BUFF_SIZE, OFFSET
            ZSTD_outBuffer output = { out, outBuffSize_, outSize };

            const int remaining = ZSTD_compressStream2(cctx, &output , &input, ZSTD_e_continue);

            if (ZSTD_isError(remaining))
                _RET_ERR;

            //CHECK_ZSTD(remaining);
            const int inRemaining = inSize - input.pos;

            // move de volta o restante
            memmove(in, in + input.pos, inRemaining);

            inSize = inRemaining; // TODO: FIXME: é o input.pos?
            outSize = output.pos;

            if (inSize > inBuffSize_)
                _RET_ERR;
            if (outSize > outBuffSize_)
                _RET_ERR;

            if (outSize >= outSizeFlush_) {

                if (outSize > (0b111111111111 * BLOCK_SIZE)) // TODO: FIXME: não incluir o tamanho deste header u16 :/
                    _RET_ERR;

                const int outSizeAligned = (outSize / BLOCK_SIZE) * BLOCK_SIZE;

                if (outSizeAligned % BLOCK_SIZE)
                    _RET_ERR;

                *(u16*)out = (threadID << 12) | (outSizeAligned / BLOCK_SIZE);

                if (pthread_mutex_lock(&outLock))
                    _RET_ERR;
                if (write(outFD, out, outSizeAligned) != outSizeAligned) // TODO: FIXME: HANDLE SIGNALS
                    outFD = inFD = FD_ERR; // SE O OUT ESTÁ RUIM, PARA DE CONSUMIR O IN
                if (pthread_mutex_unlock(&outLock))
                    _RET_ERR;

                // TIRA O QUE FOI, FICANDO O REMAINING
                outSize -= outSizeAligned;
                // MOVE O REMAINING PARA DEPOIS DO COMPRESSED SIZE
                memmove(out + sizeof(u16), out + outSizeAligned, outSize);
                // CONSIDERA O RESTANTE
                outSize += sizeof(u16);
            }
        }
    }

    // NÃO TEM MAIS NADA PARA LER

    // FLUSH O QUE TEM QUE COMPRIMIR
    if (inSize) {

        ZSTD_inBuffer input = { in, inSize, 0 }; // BUFF, BUFF_SIZE, OFFSET
        ZSTD_outBuffer output = { out, outBuffSize_, outSize };

        const int remaining = ZSTD_compressStream2(cctx, &output , &input, ZSTD_e_end);

        if (ZSTD_isError(remaining))
            _RET_ERR;

        // CHECK_ZSTD(remaining);

        // TEM QUE TER CONSUMIDO TUDO
        if (inSize != (int)input.pos)
            _RET_ERR;

        //
        outSize = output.pos;
    }

    int outSizeAligned = outSize;

    // FLUSH O QUE TEM QUE ESCREVER
    if (outSizeAligned != sizeof(u16)) {
        outSizeAligned /= BLOCK_SIZE;
        outSizeAligned *= BLOCK_SIZE;

        if (outSizeAligned) {

            *(u16*)out = (threadID << 12) | (outSizeAligned / BLOCK_SIZE);

            if (pthread_mutex_lock(&outLock))
                _RET_ERR;
            if (write(outFD, out, outSizeAligned) != outSizeAligned) // TODO: FIXME: HANDLE SIGNALS
                outFD = inFD = FD_ERR; // SE O OUT ESTÁ RUIM, PARA DE CONSUMIR O IN
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

    dbg("THREAD %d SUCCESS - RESULT @ %p REMAINING SIZE %d CHECKSUM %016llX OFFSET LAST %lld", threadID, (void*)result, result->remainingSize, (uintll)result->checksum, (uintll)offsetLast);

    return THREAD_SUCCESS;
}

static void* compressor (void* threadID_) {
    if (compressor_((int)(intptr_t)threadID_)) {
        dbg("THREAD %d FAILED", (int)(intptr_t)threadID_);
        inFD = FD_ERR;
    } return NULL;
}

// TODO: FIXME:
//      szstd THREADS_N IN_BUFF_SIZE outBuffSize OUTPUT_FILENAME READ_FD
//      szstd THREADS_N IN_BUFF_SIZE outBuffSize OUTPUT_FILENAME WRITE_FD COMMAND
//          READ_FD - fd que temos de usar para ler - se não for especificado, é o standard input
//          WRITE_FD - fd que o comando usará para escrever para nós
// if (fd <= STDERR_FILENO) close(fd), abrir o arquivo if(fd != filefd) dup2(,)  executar o comando
int main (void) {


    // INSTALA O SIGNAL HANDLER
    inFD = FD_ERR;
    outFD = FD_ERR;

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
        ) return 2; // fatal("FAILED TO INSTALL SIGNAL HANDLER")

    int threadsN = 8;

    int inBuffSize = 128*1024*1024;
    int outBuffSize = 96*1024*1024;

    // PER THREAD SIZES
    inBuffSize_  = ALIGNED(inBuffSize  / threadsN, BLOCK_SIZE);
    outBuffSize_ = ALIGNED(outBuffSize / threadsN, BLOCK_SIZE);

    // TODO: FIXME:
    outSizeFlush_ = (0.9 * outBuffSize_) - BLOCK_SIZE;

    buffSize_ = inBuffSize_ + outBuffSize_;

    // BUFFER SIZE
    const int buffSize = threadsN*buffSize_;

    // CHECKS
    if (inBuffSize  % BLOCK_SIZE) return 2;
    if (outBuffSize % BLOCK_SIZE) return 2;

    if (buffSize_    % BLOCK_SIZE) return 2;
    if (inBuffSize_  % BLOCK_SIZE) return 2;
    if (outBuffSize_ % BLOCK_SIZE) return 2;

    if (buffSize % BLOCK_SIZE) return 2;

    if (BLOCK_SIZE % HEADER_SIZE) return 2;

    if ((outSizeFlush_ + BLOCK_SIZE) > outBuffSize_) return 2;
    if (outSizeFlush_ >= outBuffSize_) return 2;
    if (outSizeFlush_ < (2*BLOCK_SIZE)) return 2;

    if (outBuffSize_ > 0xFFFFFF) return 2;
    if (threadsN > 0b1111111) return 2;

    //if (outSizeFlush_ % HEADER_SIZE) return 2;

    if ((512*1024 + 64) >= inBuffSize_) return 2;

    // offset chunksize

    char fname[64];

    // GENERATE A FILENAME WITH TIMESTAMP AND RANDOM
    // TODO: FIXME: em milésimos de segundo
    const u64 time_ = (u64)time(NULL) * 1000;
    const u64 random = rdtsc();

    sprintf(fname, "%016llX%016llX", (uintll)time_, (uintll)random);

    dbg("OUTPUT %s\n", fname);
    dbg("threadsN %d", threadsN);
    dbg("inBuffSize %d", inBuffSize);
    dbg("outBuffSize %d", outBuffSize);
    dbg("buffSize_ %d", buffSize_);
    dbg("inBuffSize_ %d", inBuffSize_);
    dbg("outBuffSize_ %d", outBuffSize_);
    dbg("buffSize %d", buffSize);
    dbg("outSizeFlush_ %d", outSizeFlush_);
    dbg("BLOCK_SIZE %d", BLOCK_SIZE);

    // CRIA E ABRE O ARQUIVO COMPRESSED
    if ((outFD = open(fname, O_WRONLY | O_CREAT | O_EXCL | O_DIRECT | O_SYNC | O_NOCTTY | O_CLOEXEC, 0444)) == -1)
        return 2;

    // BUFFER
    void* const buff = malloc_aligned(HEADER_SIZE + buffSize, BLOCK_SIZE);

    if (buff == NULL)
        return 2;

    //
    buff_ = buff + HEADER_SIZE;

    // DEIXA O ESPAÇO RESERVADO PARA O HEADER, E JÁ CONFIRMA QUE O ALINHAMENTO, ESCRITA ETC ESTÁ FUNCIONANDO
    if (write(outFD, buff, HEADER_SIZE) != HEADER_SIZE) // TODO: FIXME: escrever um header mínimo
        return 2;

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

    int threadID;

    if (pthread_mutex_lock(&inLock))
        return 2;

    threadID = threadsN;

    while (threadID--)
        if (pthread_create(&threads[threadID], NULL, compressor, (void*)(intptr_t)threadID))
            return 2;

    if (pthread_mutex_unlock(&inLock))
        return 2;

    // WAIT THEM TO FINISH
    threadID = threadsN;

    while (threadID--)
        if (pthread_join(threads[threadID], NULL))
            return 2;

    // ESCREVE O HEADER
    if (outFD != FD_ERR) {
        dbg("WRITING HEADER AND REMAININGS");

        Header* const header = buff;

        memset(header, 0, HEADER_SIZE);

        header->magic = MAGIC;
        header->check = CHECK;
        header->time = time_;
        header->random = random;
        header->blockSize = BLOCK_SIZE;
        header->blocks = lseek(outFD, 0, SEEK_CUR) / BLOCK_SIZE; // TODO: FIXME:
        header->compression = 0;
        header->dictOffset = 0;
        header->dictSize = 0;
        header->size = inOffset;
        header->threadsN = threadsN;

        //
        void* const remainings = buff_;
        void*       remainingsEnd = remainings;

        int threadID = 0;

        do {
            const ThreadResult* const result = buff_ + threadID*buffSize_;
            // COPIA POIS O PRIMEIRO VAI SOBRESCREVER
            const void* const remaining     = result->remaining;
            const int         remainingSize = result->remainingSize;
            const u64 checksum              = result->checksum;
            dbg("THREAD %d RESULT @ %p REMAINING SIZE %d CHECKSUM 0x%016llX", threadID, (void*)result, remainingSize, (uintll)checksum);
            // CONCATENA TODOS ELES
            remainingsEnd = memmove(remainingsEnd, remaining, remainingSize) + remainingSize;
            // REPASSA O REMAINING SIZE E CHECKSUM PARA O HEADER
            header->threads[threadID].remainingSize = remainingSize;
            header->threads[threadID].checksum      = checksum;
            // PRÓXIMA THREAD
        } while (++threadID != threadsN);

        header->checksum = 0; // TODO: FIXME: CHECKSUM DESTE HEADER

        int remainingsSize = remainingsEnd - remainings;

        dbg("REMAININGS SIZE %d", remainingsSize);

        remainingsSize = ALIGNED(remainingsSize, HEADER_SIZE);

        dbg("REMAININGS SIZE ALIGNED %d", remainingsSize);

        if (write(outFD, remainings, remainingsSize) != remainingsSize) { // TODO: FIXME: HANDLE SIGNALS - SÓ ENCAPSULAR NUMA FUNÇÃO
            dbg("FAILED TO WRITE REMAININGS");
            outFD = FD_ERR;
        } elif (pwrite(outFD, header, HEADER_SIZE, 0) != HEADER_SIZE) { // TODO: FIXME: HANDLE SIGNALS
            dbg("FAILED TO WRITE HEADER");
            outFD = FD_ERR;
        }
    } else {
        dbg("NOT WRITING HEADER");
    }

    //
    free(buff);

    // CLOSE BOTH AT THE SAME TIME
    // se output foi criado por nós, então fecharmos ele primeiro
    //close(outFD);
    //close(inFD);
    dbg("INPUT FD: %d %s", inFD, (
        inFD == FD_CLOSED ? "CLOSED":
        inFD == FD_ERR    ? "ERROR":
        inFD == 0         ? "STDIN" :
        inFD == 1         ? "STDOUT" :
        inFD == 2         ? "STDERR" :
        inFD <= 0         ? "INVALID" :
        ""));

    dbg("IN OFFSET: %lld", (uintll)inOffset);

    dbg("OUTPUT FD: %d", outFD);

    int status = inFD == FD_ERR || outFD == FD_ERR;
    dbg("EXIT STATUS: %d", status);
    return status;
}

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
