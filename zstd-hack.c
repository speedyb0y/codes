/*
    zstd-hack

    gcc -Wall -Wextra -fwhole-program -O2 -march=native -pthread -lzstd zstd-hack.c -o zstd-hack
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

#if 0
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

static inline u64 rdtsc(void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void* malloc_aligned (const uintll alignment, const uintll size) {

    void* ptr = NULL;

    if (posix_memalign(&ptr, alignment, size) || ptr == NULL)
        return NULL;

    return  ptr;
}

#define ALIGNED(value, alignment) ((((value) + (alignment) - 1) / (alignment)) * (alignment))

#define HEADER_SIZE 4096
#define BLOCK_SIZE 4096

#define MAGIC 0xFFFFFFFFFFFFFFFFULL
#define CHECK (sizeof(Header) + offsetof(Header, checksum))

typedef struct Header {
    u64 magic;
    u64 check;
    u64 time; // em milésimos de segundo
    u64 random;
    u64 blockSize;
    u64 blocks;
    u64 threadsN; // NUMBER OF THREADS
    u64 compression; // COMPRESSION FORMAT
    u64 dictOffset; // APÓS TODA A COMPRESSÃO
    u64 dictSize; // DICTIONARY SIZE
    u64 size; // ORIGINAL SIZE
    u64 checksum; // DESTE HEADER
} Header; // seguido pelo dict

static void* buff;

static u64 inOffset;

static int inFD;
static int outFD;

static int buffSize_;
static int inBuffSize_; // per thread
static int outBuffSize_;

static int outSizeWrite;

static pthread_mutex_t inLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t outLock = PTHREAD_MUTEX_INITIALIZER;

#define FD_CLOSED -1
#define FD_ERR -2

#define _RET_ERR ({ printf("!!! THREAD ERR %d\n", __LINE__); return THREAD_ERROR; })
//#define _RET_ERR return THREAD_ERROR

#define THREAD_SUCCESS 0
#define THREAD_ERROR 1 // SÃO TODOS ERROS FATAIS

static inline int compressor_(const int threadID) {

    u64 checksum = 0;

    void* const in = buff + threadID*(inBuffSize_ + outBuffSize_);
    void* const out = buff + threadID*(inBuffSize_ + outBuffSize_) + inBuffSize_;

    int inSize = 0; // O QUANTO JÁ TEM NO BUFFER DE ENTRADA

    u32*  const outSize_      = out;
    u32*  const outChecksum   = out + sizeof(u32);
    u8*   const outThreadID   = out + sizeof(u32) + sizeof(u32);
    void* const outCompressed = out + sizeof(u32) + sizeof(u32) + sizeof(u8);

    // O QUANTO JÁ TEM NO BUFFER DE SAÍDA
    int outSize = sizeof(u32) + sizeof(u32) + sizeof(u8);

    //
    *outThreadID = threadID;

    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    // TODO: FIXME:
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 11);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

    do {

        // SE ESTÁ QUERENDO LER, TEM QUE TER ESPAÇO
        if ((int)(inSize + 16) >= inBuffSize_)
            _RET_ERR;
        if ((outSize + 512) >= outBuffSize_)
            _RET_ERR;

        // LÊ DA STREAM
        if(pthread_mutex_lock(&inLock))
            _RET_ERR;

        if (inFD >= 0) { int received;
            if ((received = read(inFD, in + inSize + sizeof(u64) + sizeof(u32), inBuffSize_ - inSize - sizeof(u64) - sizeof(u32))) > 0) {
                *(u64*)(in + inSize) = inOffset; inSize += sizeof(u64); // POE ONDE ESTÁ ESTE CHUNK
                *(u32*)(in + inSize) = received; inSize += sizeof(u32); // POE O TAMANHO QUE LEU
                inSize += received; // AGORA SIM CONSIDERA ISSO TUDO
                inOffset += received; // A PRÓXIMA LIDA ESTARÁ NESTE NOVO OFFSET
            } elif (received) {
                dbg("READ ERROR");
                inFD = FD_ERR;
            } else {
                dbg("READ CLOSED");
                inFD = FD_CLOSED;
            }
        }

        if (pthread_mutex_unlock(&inLock))
            _RET_ERR;

        if (inFD < 0)
            break;

        if (inSize >= (512*1024)) {
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

            if (outSize >= outSizeWrite) {

                const int outSizeAligned = (outSize / BLOCK_SIZE) * BLOCK_SIZE;

                if (outSizeAligned % BLOCK_SIZE)
                    _RET_ERR;

                *outSize_ = outSizeAligned;
                *outChecksum = checksum; // TODO: FIXME: checksum of the compressed

                if (pthread_mutex_lock(&outLock))
                    _RET_ERR;
                if (write(outFD, out, outSizeAligned) != outSizeAligned)
                    outFD = inFD = FD_ERR; // SE O OUT ESTÁ RUIM, PARA DE CONSUMIR O IN
                if (pthread_mutex_unlock(&outLock))
                    _RET_ERR;

                // TIRA O QUE FOI, FICANDO O REMAINING
                outSize -= outSizeAligned;
                // MOVE O REMAINING PARA DEPOIS DO COMPRESSED SIZE
                memmove(outCompressed, out + outSizeAligned, outSize);
                // CONSIDERA O RESTANTE
                outSize += outCompressed - out;
            }
        }

    } while (inFD >= 0);

    // NÃO TEM MAIS NADA PARA LER

    if (inSize) {
        // COMPRESS
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

        *outThreadID |= 0b10000000U; // MARCA QUE ESTE É O ÚLTIMO
        *outSize_ = outSize; // SÓ ESSE TAMANHO QUE IMPORTA
        *outChecksum = checksum; // NOTA: É COMPUTADO ALINHADO AO QUE FOI ESCRITO

        // PODE FICAR ALGO A MAIS NO FINAL
        const int outSizeAligned = ((outSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

        if (pthread_mutex_lock(&outLock))
            _RET_ERR;
        if (write(outFD, out, outSizeAligned) != outSizeAligned)
            outFD = inFD = FD_ERR; // SE O OUT ESTÁ RUIM, PARA DE CONSUMIR O IN
        if (pthread_mutex_unlock(&outLock))
            _RET_ERR;
    }

    return THREAD_SUCCESS;
}

static void* compressor (void* threadID_) {
    if (compressor_((intptr_t)threadID_))
        inFD = FD_ERR;
    return NULL;
}

// TODO: FIXME:
//      szstd THREADS_N IN_BUFF_SIZE outBuffSize OUTPUT_FILENAME READ_FD
//      szstd THREADS_N IN_BUFF_SIZE outBuffSize OUTPUT_FILENAME WRITE_FD COMMAND
//          READ_FD - fd que temos de usar para ler - se não for especificado, é o standard input
//          WRITE_FD - fd que o comando usará para escrever para nós
// if (fd <= STDERR_FILENO) close(fd), abrir o arquivo if(fd != filefd) dup2(,)  executar o comando
int main (void) {

    int threadsN = 5;

    int inBuffSize = 128*1024*1024;
    int outBuffSize = 64*1024*1024;

    // PER THREAD SIZES
    inBuffSize_  = ALIGNED(inBuffSize  / threadsN, BLOCK_SIZE);
    outBuffSize_ = ALIGNED(outBuffSize / threadsN, BLOCK_SIZE);

    buffSize_ = inBuffSize_ + outBuffSize_;

    // BUFFER SIZE
    const int buffSize = threadsN*buffSize_;

    // CHECKS
    if (inBuffSize  % BLOCK_SIZE) return 1;
    if (outBuffSize % BLOCK_SIZE) return 1;

    if (buffSize_    % BLOCK_SIZE) return 1;
    if (inBuffSize_  % BLOCK_SIZE) return 1;
    if (outBuffSize_ % BLOCK_SIZE) return 1;

    if (buffSize % BLOCK_SIZE) return 1;

    if (BLOCK_SIZE % HEADER_SIZE) return 1;

    if ((512*1024 + 64) >= inBuffSize_) return 1;

    // offset chunksize

    char fname[64];

    // GENERATE A FILENAME WITH TIMESTAMP AND RANDOM
    // TODO: FIXME: em milésimos de segundo
    const u64 time_ = (u64)time(NULL) * 1000;
    const u64 random = rdtsc();

    sprintf(fname, "%016llX%016llX", (uintll)time_, (uintll)random);

#if 1
    printf("%s\n", fname);
#endif

    // CRIA E ABRE O ARQUIVO COMPRESSED
    if ((outFD = open(fname, O_WRONLY | O_CREAT | O_EXCL | O_DIRECT | O_SYNC | O_NOCTTY | O_CLOEXEC, 0444)) == -1)
        return 1;

    // BUFFER
    if ((buff = malloc_aligned(BLOCK_SIZE, buffSize)) == NULL)
        return 1;

    // DEIXA O ESPAÇO RESERVADO PARA O HEADER, E JÁ CONFIRMA QUE O ALINHAMENTO, ESCRITA ETC ESTÁ FUNCIONANDO
    if (write(outFD, buff, HEADER_SIZE) != HEADER_SIZE)
        return 1;

    // TODO: FIXME:
    outSizeWrite = 0.4 * outBuffSize_;

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
        return 1;

    threadID = threadsN;

    while (threadID--)
        if (pthread_create(&threads[threadID], NULL, compressor, (void*)(intptr_t)threadID))
            return 1;

    if (pthread_mutex_unlock(&inLock))
        return 1;

    // WAIT THEM TO FINISH
    threadID = threadsN;

    while (threadID--)
        if (pthread_join(threads[threadID], NULL))
            return 1;

    // ESCREVE O HEADER
    if (outFD != FD_ERR) {

        memset(buff, 0, HEADER_SIZE);

        Header* const header = buff;

        header->magic = MAGIC;
        header->check = CHECK;
        header->time = time_;
        header->random = random;
        header->blockSize = BLOCK_SIZE;
        header->blocks = lseek(outFD, 0, SEEK_CUR) / BLOCK_SIZE; // TODO: FIXME:
        header->threadsN = threadsN;
        header->compression = 0;
        header->dictOffset = 0;
        header->dictSize = 0;
        header->size = inOffset;
        header->checksum = 0; // TODO: FIXME: CHECKSUM DESTE HEADER

        if (pwrite(outFD, header, HEADER_SIZE, 0) != HEADER_SIZE)
            outFD = FD_ERR;
    }

    //
    free(buff);

    // CLOSE BOTH AT THE SAME TIME
    // se output foi criado por nós, então fecharmos ele primeiro
    //close(outFD);
    //close(inFD);

    return inFD == FD_ERR || outFD == FD_ERR;
}
