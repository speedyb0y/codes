/*
    zstd-hack

    ./a.out "[0,1,2,3,4,-4,-3,-2,-1,0,b'A'*10,b'$&$#%#','MEU GATINHO EH LINDO...',0xFFFFFFFF,'?', 1<<31, '898989', 1<<32, 'kmkmk',1,3,4, 0xAABBCCDD00112233, b'PIJAMA', 0x9988776655443, b'BANANADA', '-', b'@', None, None, False, True, 'CARRO', 0.5, 0, '?'*4096, 14]"
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
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>

#include <zstd.h>

#define FD_INVALID -1

#if 1
#define dbg(fmt, ...) ({ fprintf(stderr, fmt "\n", ##__VA_ARGS__); })
#else
#define dbg(fmt, ...) ({})
#endif

#define log(fmt, ...) ({ fprintf(stderr, fmt "\n", ##__VA_ARGS__); })
#define err(fmt, ...) ({ fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); })

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

#ifndef offsetof
#define offsetof __builtin_offsetof
#endif

#define loop while (1)

#define elif else if

typedef double f64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int64_t i64;

typedef unsigned int uint;
typedef unsigned long long int uintll;

typedef long long int intll;

#define ALIGNED(value, alignment) ((((value) + (alignment) - 1) / (alignment)) * (alignment))

// TEM QUE SER ALINHADO A 32 POR CAUSA DO CHECKSUM
#define BLOCK_SIZE 4096

static volatile sig_atomic_t status;

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
            status = 1;
    }

}

#define TYPE_POSITIVE  0b00000000
#define TYPE_NEGATIVE  0b00100000 // -500 -> 499 encode: |value+1| decode: -(value+1)
#define TYPE_BINARY    0b01000000 // LEN DATA
#define TYPE_STRING    0b01100000 // LEN DATA
#define TYPE_ARRAY     0b10000000 // N ITEMS ITEMS
#define TYPE_MAP       0b10100000 // N PAIRS PAIRS
#define TYPE_TAG       0b11000000
#define TYPE_SIMPLE    0b11100000 // 0 ... 23
#define TYPE_FALSE     0b11110100
#define TYPE_TRUE      0b11110101
#define TYPE_NULL      0b11110110
#define TYPE_INVALID   0b11110111
#define TYPE_SIMPLE2   0b11111000 // 24 Simple value (value 32..255 in following byte)
#define TYPE_FLOAT16   0b11111001 // IEEE 754 HALF-PRECISION FLOAT (16 BITS FOLLOW)
#define TYPE_FLOAT32   0b11111010 // IEEE 754 SINGLE-PRECISION FLOAT (32 BITS FOLLOW)
#define TYPE_FLOAT64   0b11111011 // IEEE 754 DOUBLE-PRECISION FLOAT (64 BITS FOLLOW)
#define TYPE_BREAK     0b11111111

typedef struct CBORMap CBORMap;
typedef struct CBORKey CBORKey;

typedef struct CBORIndex CBORIndex;

struct value {
    u64 typeSize;  // 0xFFFFF constante
    u64 hash; // hash
    u64 hash2;
    u64 rc; // reference count - toda vez que for uma key, um valor mapeado por uma key, uma entrada em uma array, uma entrada no cache
    union {
        u64 pos;
        u64 neg;
        double ft;
        char* str; // valor
        char* bin;
        int cst; // constant - null, invalid, neg, pos etc
    };
};

mapentry {
  void* value; //
}

struct CBORMap {
    u64 typeSize; // TIPO | TOTAL DE ENTRADAS
    CBORKey* keys[128]; // os heads
};

struct CBORKey {
    u64 hash;
    u64 hash2;
    CBORKey* prev;
    CBORKey* next;
    CBORKey** ptr; // array que o contém; mapa que tem este key
    CBORKey* childs[4];
    void* value; // valor ao qual esta key está mapeada
    char _[];
};

struct CBORIndex { // elemento de uma array
    CBORIndex* prev; // próximo nessa array; próximo  key->value pair no mapa
    CBORIndex* next; //
    char _[];
};

typedef struct CBORArray {
    u64 typeSize;
    u64 max; // máximo que cabe; alocado foi ISSO + 256     -> usar um countdown de increases e decreases; só permitir redimensionar após >= COUNT , aí resetas
    CBORIndex** indexes;  // PONTEIRO PARA A ARRAY //indexes[]; // sempre uma quantidade multipla de um block size, para evitar ficar toda hora dando um memmove etc?
} CBORArray;

#define TYPE_SIZE_TYPE(ts) ((ts) >> 60)
#define TYPE_SIZE_SIZE(ts) ((ts) & 0xFFFFFFFFFFFFFFFULL) // TODO: FIXME:
#define TYPE_SIZE(t, s) (((t) << 60) | (s))

#define TYPE_SIZE_NULL      0
#define TYPE_SIZE_INVALID   1
#define TYPE_SIZE_FALSE     2
#define TYPE_SIZE_TRUE      3
#define TYPE_SIZE_POSITIVE  4
#define TYPE_SIZE_NEGATIVE  5
#define TYPE_SIZE_BINARY    6
#define TYPE_SIZE_STRING    7
#define TYPE_SIZE_ARRAY     8
#define TYPE_SIZE_MAP       9

typedef struct CBORContext {
    int fd;
    int childPID;
    int childStatus;
    u64 offset;
    void* start;
    void* end;
    void* lmt;
    char buff[];
} CBORContext;

// END IT
static int cbor_more(CBORContext* const ctx, int n) {

    // TIRA DO NECESSÁRIO, O QUE JÁ TEM
    if ((n -= ctx->end - ctx->start) <= 0)
        return 1;

    // PRECISA LER MAIS

    //
    if (ctx->fd == FD_INVALID)
        return 0;

    // TODO: FIXME: só se start== end ou se tiver pouco ou se   n nao vai caber
    ctx->end -= ctx->start - memmove(ctx->buff, ctx->start, ctx->end - ctx->start);
    ctx->start = ctx->buff;

    int readen;

    while (n > 0) {
        while ((readen = read(ctx->fd, ctx->end, ctx->lmt - ctx->end)) == -1) {
            if (errno != EINTR)  {
                err("FAILED TO READ");
                goto ERR;
            }
        }

        if (readen == 0) {
            log("NOTHING READEN");
            goto ERR;
        }

        dbg("READEN %d BYTES", readen);

        ctx->end += readen;

        n -= readen;
    }

    return 1;

ERR:
    // CLOSE THE FILE DESCRIPTOR
    if (close(ctx->fd)) {
        err("FAILED TO CLOSE READ FD");
        abort();
    }

    // MARK IT AS CLOSED
    ctx->fd = FD_INVALID;

    // WAIT THE CHILD
    int childStatus = -1;

    if (waitpid(ctx->childPID, &childStatus, 0) != ctx->childPID) {
        err("WAITED NOT CHILD");
        abort();
    }

    log("CHILD EXITED WITH STATUS %d", childStatus);

    // POE O VALOR
    // SE DEU ERRO, O PROGRAMA DEU ERRO
    if ((ctx->childStatus = childStatus))
        status = 1;

    return 0;
}

static void cbor_do (CBORContext* const ctx) {

    uint tag = 0;

    while (cbor_more(ctx, 1)) { // PRECISA DE PELO MENOS UM BYTE PARA COMEÇAR ALGO

        const uint code = *(u8*)ctx->start++;

        //log("OFFSET %llu, CODE 0x%02X", (uintll)ctx->offset, code);

        ctx->offset++;

        if ((code >> 5) == 0b110) {
            log("TAG");
            // NÃO PODE TAGGEAR UMA TAG
            if (tag) {
                abort();
            } // MARCA ELA
            tag = code;
            continue;
        }


        if (code < TYPE_TAG) {

            uintll word = code & 0b11111U;

            if (word > 27) { // 31 stream // something else
                err("BAD WORD %llu", word);
                abort();
            }

            // NOTA: AQUI PODE HAVER FALHA NESTE CÁLCULO SE NÃO CHECAR ACIMA
            const uint wordSize = (word > 23) << (word - 24);

            if (wordSize) {
                // TER CERTEZA DE QUE JÁ CARREGOU A PALAVRA
                if (!cbor_more(ctx, wordSize))
                    return;
                // LÊ A PALAVRA
                word = (
                    (wordSize == 1) ?                   *( u8*)ctx->start  :
                    (wordSize == 2) ? __builtin_bswap16(*(u16*)ctx->start) :
                    (wordSize == 4) ? __builtin_bswap32(*(u32*)ctx->start) :
                                      __builtin_bswap64(*(u64*)ctx->start)
                    );
                // CONSOME A PALAVRA
                ctx->start += wordSize;
                ctx->offset += wordSize;
            }

            if (code >= TYPE_MAP) {
                log("MAP %llu", word);
            } elif (code >= TYPE_ARRAY) {
                log("ARRAY %llu", word);
            } elif (code >= TYPE_STRING) {
                if (!cbor_more(ctx, word))
                    return;
                log("STRING %llu %.*s", word, (int)word, (char*)ctx->start);
                ctx->start += word;
                ctx->offset += word;
            } elif (code >= TYPE_BINARY) {
                if (!cbor_more(ctx, word))
                    return;

                if (tag == 0xC2) {
                    uintll bigint;

                    if (word == 8) {
#if 1
                        bigint = __builtin_bswap64(*(u64*)ctx->start);
#else
                        bigint  = ((u8*)ctx->start)[0]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[1]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[2]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[3]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[4]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[5]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[6]; bigint <<= 8;
                        bigint |= ((u8*)ctx->start)[7];
#endif
                    } else {
                        abort();
                    }

                    log("BIGINT %llu %llu 0x%016llX", word, bigint, bigint);
                } else {
                    log("BINARY %llu %.*s", word, (int)word, (char*)ctx->start);
                }
                ctx->start += word;
                ctx->offset += word;

            } elif (code >= TYPE_NEGATIVE) {
                log("NEGATIVE %lld", -1LL - word);
            } else { // POSITIVE
                log("POSITIVE %llu 0x%016llX", word, word);
            }

        } elif (code == TYPE_FALSE) {
            log("FALSE");
        } elif (code == TYPE_TRUE) {
            log("TRUE");
        } elif (code == TYPE_NULL) {
            log("NULL");
        } elif (code == TYPE_INVALID) {
            log("INVALID");
        } elif (code == TYPE_BREAK) {
            log("BREAK");
        } elif (code == TYPE_FLOAT16) {
            log("FLOAT16");
        } elif (code == TYPE_FLOAT32) {
            log("FLOAT32");
        } elif (code == TYPE_FLOAT64) {
            //
            if (!cbor_more(ctx, 8))
                return ;
            // LÊ A PALAVRA
            union { f64 f64_; u64 u64_; } x;
            x.u64_ = __builtin_bswap64(*(u64*)ctx->start);
            // CONSOME A PALAVRA
            ctx->start += sizeof(f64);
            ctx->offset += sizeof(f64);

            log("FLOAT64 %f", x.f64_);

        } else {
            err("UNKNOWN");
            abort();
        }

        tag = 0;
    }
}

static CBORContext* cbor_open (char* const fpath) {

    //
    int fds[2] = { -1, -1 };

    //
    if (pipe2(fds, O_CLOEXEC)) {
        return NULL;
    }

    const int fdIN = fds[0];
    const int fdOut = fds[1];

    // FORK
    const int childPID = fork();

    if (childPID == 0) {
        if (close(1))
            return NULL;
        if (close(fdIN))
            return NULL;
        if (dup2(fdOut, 1) != 1)
            return NULL;
        // EXECUTE
        char pstr_[65536];
        snprintf(pstr_, sizeof(pstr_), "import os, cbor ; x = cbor.dumps(%s) ; open('DUMP', 'wb').write(x); os.write(1, x)", fpath);
        execve("/usr/bin/python", (char*[]) { "python", "-c", pstr_, NULL }, NULL);
        return NULL;
    }

    if (childPID == -1) {
        err("FAILED TO FORK");
        return NULL;
    }

    //
    if (close(fdOut)) {
        return NULL;
    }

    //
    const uint buffSize = 1024*1024;

    CBORContext* const ctx = malloc(sizeof(CBORContext) + buffSize);

    if (ctx == NULL) {
        err("FAILED TO ALLOCATE BUFFER");
        return NULL;
    }

    //
    ctx->fd = fdIN;
    ctx->childPID = childPID;
    ctx->childStatus = 0;
    ctx->offset = 0;
    ctx->start = ctx->buff;
    ctx->end = ctx->buff;
    ctx->lmt = ctx->buff + buffSize;

    return ctx;
}

int main (int argsN, char* args[]) {

    status = 0;

    if (argsN != 2) {
        return 1;
    }

    //
    char* const pstr = args[1];


    // INSTALL THE SIGNAL HANDLER
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
        ) return 1; // fatal("FAILED TO INSTALL SIGNAL HANDLER")

    CBORContext* const ctx = cbor_open(pstr);

    cbor_do(ctx);

    return status;
}
