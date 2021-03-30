/*
    gcc -Wall -Werror -fwhole-program xtun.c -o xtun
*/

#define WC_NO_HARDEN 1
#define _GNU_SOURCE  1

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#if 0
#define ASSERT(condition) ({ if (!(condition)) { write(STDOUT_FILENO, STRINGIFY(__LINE__) " " #condition "\n", sizeof(STRINGIFY(__LINE__) " " #condition "\n")); abort(); } })
#else
#define ASSERT(condition) ({})
#endif

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/if_tun.h>
#include <linux/io_uring.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sched.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t i32;

typedef uint8_t byte;

typedef long long intll;

typedef unsigned int uint;
typedef unsigned long long int uintll;

typedef struct sockaddr sockaddr;
typedef struct sockaddr_un sockaddr_un;
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr_in6 sockaddr_in6;

#define loop while(1)
#define elif else if
#define foreach(_i, _n) for (uint _i = 0; _i != (_n); _i++)

#define __unused __attribute__((unused))

#define _to_be16(x) __builtin_bswap16(x)
#define _to_be32(x) __builtin_bswap32(x)
#define _to_be64(x) __builtin_bswap64(x)

#define _from_be16(x) __builtin_bswap16(x)
#define _from_be32(x) __builtin_bswap32(x)
#define _from_be64(x) __builtin_bswap64(x)

typedef struct mmsghdr mmsghdr;
typedef struct msghdr msghdr;

typedef struct iovec iovec;

#define clear(addr, size) memset(addr, 0, size)
#define clear1(addr, size) memset(addr, 0xFF, size)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//
#if 1
#define dbg(fmt, ...) ({ fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__); fflush(stderr); })
#else
#define dbg(fmt, ...) ({ })
#endif

#if 1
#define log(fmt, ...) ({ fprintf(stderr, fmt "\n", ##__VA_ARGS__); fflush(stderr); })
#else
#define log(fmt, ...) ({ })
#endif

#if 1
#define err(fmt, ...) ({ fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); fflush(stderr); })
#else
#define err(fmt, ...) ({ })
#endif

#define fatal(fmt, ...) ({ fprintf(stderr, "FATAL: " fmt "\n", ##__VA_ARGS__); fflush(stderr); abort(); })

// IO-URING
#define IO_CLEAR   0xfffffbffU // NOT WAITING, NOT RESULT
#define IO_WAITING 0xfffffc00U
#define IO_ERR     0xfffffc01U

typedef struct io_uring_params IOURingParams;
typedef struct io_uring_sqe IOURingSQE;
typedef struct io_uring_cqe IOURingCQE;
typedef struct IOSubmission IOSubmission;

struct IOSubmission {
    u64 data;
    u64 addr;
    u64 off;
    u16 opcode;
    u16 fd;
    u32 len;
};

#define read_barrier()  __asm__ __volatile__("":::"memory")
#define write_barrier() __asm__ __volatile__("":::"memory")

static inline int io_uring_setup(uint entries, IOURingParams* p) {
    return syscall(__NR_io_uring_setup, entries, p);
}

static inline int io_uring_enter(int fd, uint to_submit, uint min_complete, uint flags) {
    return syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, NULL, 0);
}

#define IOU_FD 3

#define IOU_S_SQES ((IOURingSQE*)0x0002E00000000ULL)
#define IOU_S_SQES_SIZE 1048576

#define IOU_BASE ((void*)0x0002F00000000ULL)
#define IOU_BASE_SIZE 590144

#define IOU_S_HEAD    ((uint*)0x0000002F00000000ULL)
#define IOU_S_TAIL    ((uint*)0x0000002F00000040ULL)
#define IOU_S_MASK    ((uint*)0x0000002F00000100ULL)
#define IOU_S_ENTRIES ((uint*)0x0000002F00000108ULL)
#define IOU_S_FLAGS   ((uint*)0x0000002F00000114ULL)
#define IOU_S_ARRAY   ((uint*)0x0000002F00080140ULL)

#define IOU_C_HEAD    ((uint*)0x0000002F00000080ULL)
#define IOU_C_TAIL    ((uint*)0x0000002F000000C0ULL)
#define IOU_C_MASK    ((uint*)0x0000002F00000104ULL)
#define IOU_C_ENTRIES ((uint*)0x0000002F0000010CULL)
#define IOU_C_CQES    ((IOURingCQE*)0x0000002F00000140ULL)

#define IOU_S_MASK_CONST 0x3FFFU
#define IOU_C_MASK_CONST 0x7FFFU

static IOSubmission uSubmissions[65536];
static uint uSubmissionsNew;
static u64 uSubmissionsStart;
static u64 uSubmissionsEnd;
static uint uConsumePending;
static uint uConsumeHead;

//xtun_io_submit((u32*)&logBufferFlushing, IORING_OP_WRITE, STDOUT_FILENO, (u64)logBuffer, 0, logEnd - logBuffer);
//xtun_io_submit(&answer->result, IORING_OP_READ, dnsSockets[answer->server], (u64)answer->pkt, 0, sizeof(answer->pkt));
//xtun_io_submit(&conn->sslInRes, IORING_OP_CONNECT, conn->fd, (u64)&conn->dAddr, conn->v6 ? sizeof(SockAddrIP6) : sizeof(SockAddrIP4), 0);
static inline void xtun_io_submit (u32* const res, const uint opcode, const uint fd, const u64 addr, const u64 off, const uint len) {

    *res = IO_WAITING;

    uSubmissions[uSubmissionsEnd].data   = (uintptr_t)res;
    uSubmissions[uSubmissionsEnd].opcode = opcode;
    uSubmissions[uSubmissionsEnd].fd     = fd;
    uSubmissions[uSubmissionsEnd].addr   = addr;
    uSubmissions[uSubmissionsEnd].off    = off;
    uSubmissions[uSubmissionsEnd].len    = len;

    // TODO: FIXME: SETAR O ->result como IO_WAIT SOMENTE QUANDO ELE FOR SUBMETIDO?
    //ou entao o if()else e so colocar no enqueued quando lotar o principal

    //se tiverr algo enqueued ou nao couber mais->enqueuea

    uSubmissionsEnd++;
}

// SIGNALS
static volatile sig_atomic_t sigTERM;
static volatile sig_atomic_t sigUSR1;
static volatile sig_atomic_t sigUSR2;

static void xtun_signal_handler (int signal) {

    switch (signal) {
        case SIGUSR1:
            sigUSR1 = 1;
            break;
        case SIGUSR2:
            sigUSR2 = 1;
            break;
        default: // SIGTERM / SIGINT
            sigTERM = 1;
    }
}

//
#define TUNNEL_PORT 55555

// CONNECTIONS
#define CONNECTIONS_ACCEPT_N 128

typedef struct Connection Connection;

#define CONNECTION_BUFF_SIZE (512*1024)

struct Connection {
    Connection* next;
    Connection* prev;
    //
    u32 remove;
    u32 fd;
    u32 reserved;
    //
    u32 iReadRes;
    u32 oReadRes;
    u32 iWriteRes;
    u32 oWriteRes;
    //
    u32 fdCloseRes;
    u32 iReadCancel;
    u32 oReadCancel;
    u32 iWriteCancel;
    u32 oWriteCancel;
    //
    char iRead  [CONNECTION_BUFF_SIZE];
    char oRead  [CONNECTION_BUFF_SIZE];
    char iWrite [CONNECTION_BUFF_SIZE];
    char oWrite [CONNECTION_BUFF_SIZE];
};

int main (void) {

    // SIGNALS
    // NO SIGNAL CAUGHT YET
    sigTERM = sigUSR1 = sigUSR2 = 0;

    // IGNORE ALL SIGNALS
    struct sigaction action = { 0 };

    action.sa_restorer = NULL;
    action.sa_flags = 0;
    action.sa_handler = SIG_IGN;

    for (int sig = 0; sig != NSIG; sig++)
        sigaction(sig, &action, NULL);

    // HANDLE ONLY THESE
    action.sa_handler = xtun_signal_handler;

    sigaction(SIGINT,  &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    // IO_URING
    IOURingParams params;

    memset(&params, 0, sizeof(params));

    const int fd = io_uring_setup(16384, &params);

    if (fd == -1)
        fatal("FAILED TO OPEN IO_URING: %s", strerror(errno));

    if (params.sq_entries != 16384)
        fatal("NUMBER OF SQ ENTRIES MISMATCH: %llu", (uintll)params.sq_entries);

    uSubmissionsNew = 0;
    uSubmissionsStart = 0;
    uSubmissionsEnd = 0;

    uConsumePending = 0;

    // ASSUMING (params.features & IORING_FEAT_SINGLE_MMAP)
    uint sSize = params.sq_off.array + params.sq_entries * sizeof(uint);
    uint cSize = params.cq_off.cqes  + params.cq_entries * sizeof(IOURingCQE);

    if (sSize < cSize)
        sSize = cSize;

    dbg("IO_URING FD %d", fd);

    dbg("params.sq_entries = %llu", (uintll)params.sq_entries);

    dbg("#define IOU_BASE_SIZE %u", sSize);

    dbg("#define IOU_S_SQES_SIZE %llu", (uintll)params.sq_entries * sizeof(IOURingSQE));

    dbg("#define IOU_S_HEAD    ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.sq_off.head));
    dbg("#define IOU_S_TAIL    ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.sq_off.tail));
    dbg("#define IOU_S_MASK    ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.sq_off.ring_mask));
    dbg("#define IOU_S_ENTRIES ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.sq_off.ring_entries));
    dbg("#define IOU_S_FLAGS   ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.sq_off.flags));
    dbg("#define IOU_S_ARRAY   ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.sq_off.array));

    dbg("#define IOU_C_HEAD    ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.cq_off.head));
    dbg("#define IOU_C_TAIL    ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.cq_off.tail));
    dbg("#define IOU_C_MASK    ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.cq_off.ring_mask));
    dbg("#define IOU_C_ENTRIES ((uint*)0x%016llXULL)", (uintll)(IOU_BASE + params.cq_off.ring_entries));
    dbg("#define IOU_C_CQES    ((IOURingCQE*)0x%016llXULL)", (uintll)(IOU_BASE + params.cq_off.cqes));

    ASSERT(fd == IOU_FD);

    // CONNS_N*(in e out) + DNS PACKETS + 1024
    ASSERT(params.sq_entries == 16384);

    ASSERT((IOU_BASE + params.sq_off.head)         == (void*)IOU_S_HEAD);
    ASSERT((IOU_BASE + params.sq_off.tail)         == (void*)IOU_S_TAIL);
    ASSERT((IOU_BASE + params.sq_off.ring_mask)    == (void*)IOU_S_MASK);
    ASSERT((IOU_BASE + params.sq_off.ring_entries) == (void*)IOU_S_ENTRIES);
    ASSERT((IOU_BASE + params.sq_off.flags)        == (void*)IOU_S_FLAGS);
    ASSERT((IOU_BASE + params.sq_off.array)        == (void*)IOU_S_ARRAY);

    ASSERT((IOU_BASE + params.cq_off.head)         == (void*)IOU_C_HEAD);
    ASSERT((IOU_BASE + params.cq_off.tail)         == (void*)IOU_C_TAIL);
    ASSERT((IOU_BASE + params.cq_off.ring_mask)    == (void*)IOU_C_MASK);
    ASSERT((IOU_BASE + params.cq_off.ring_entries) == (void*)IOU_C_ENTRIES);
    ASSERT((IOU_BASE + params.cq_off.cqes)         == (void*)IOU_C_CQES);

    ASSERT(IOU_BASE_SIZE == sSize);

    if (mmap(IOU_S_SQES, IOU_S_SQES_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, IOU_FD, IORING_OFF_SQES) != IOU_S_SQES)
        fatal("FAILED TO MAP IOU_S_SQES");

    if (mmap(IOU_BASE, IOU_BASE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, IOU_FD, IORING_OFF_SQ_RING) != IOU_BASE)
        fatal("FAILED TO MAP IOU_BASE");

    dbg("#define IOU_S_MASK_CONST 0x%llXU", (uintll)*IOU_S_MASK);
    dbg("#define IOU_C_MASK_CONST 0x%llXU", (uintll)*IOU_C_MASK);

    ASSERT(IOU_S_MASK_CONST == *IOU_S_MASK);
    ASSERT(IOU_C_MASK_CONST == *IOU_C_MASK);

    uConsumeHead = *IOU_C_HEAD;

    read_barrier();

    Connection* clients = NULL;

    u32 acceptedFDs[CONNECTIONS_ACCEPT_N];

    // WAIT FOR CONNECTIONS
    const int sockListen = socket(AF_UNIX, SOCK_DGRAM, 0);

    if (sockListen == -1) {
        printf("ERROR: FAILED TO OPEN LISTENING SOCKET: %s\n", strerror(errno));
        return 1;
    }

    // PREVENT EADDRINUSE
    const int sockOptReuseAddr = 1;

    if (setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptReuseAddr, sizeof(sockOptReuseAddr)) < 0) {
        printf("ERROR: FAILED TO SET REUSE ADDR: %s\n", strerror(errno));
        return 1;
    }

    const sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = "\x00xtun\x00" };

    dbg("BINDING...");

    if (bind(sockListen, (sockaddr*)&addr, sizeof(addr))) {
        printf("ERROR: FAILED TO BIND: %s\n", strerror(errno));
        return 1;
    }

    dbg("LISTENING...");

    if (listen(sockListen, 65536)) {
        printf("ERROR: FAILED TO LISTEN FOR CLIENTS: %s\n", strerror(errno));
        return 1;
    }

    dbg("INITIALIZING ACCEPTS...");

    for (uint i = 0; i != CONNECTIONS_ACCEPT_N; i++)
        acceptedFDs[i] = accept(sockListen, NULL, 0);

    dbg("ENTERING LOOP...");

    while (!sigTERM) {

        { // POLL IO
            u64 q = uSubmissionsEnd - uSubmissionsStart;

            // MAS LIMITADO A QUANTOS TEM FREE
            if (q > (65536 - uSubmissionsNew))
                q = (65536 - uSubmissionsNew);

            //
            uSubmissionsNew += q;

            q += uSubmissionsStart;

            //
            uint tail = *IOU_S_TAIL; // TODO: FIXME: uSubmissionTail

            read_barrier();

            // COPIA DO START AO Q-ESIMO, PARA O KERNEL
            while (uSubmissionsStart != q) { // ADD OUR SQ ENTRY TO THE TAIL OF THE SQE RING BUFFER

                const IOSubmission* const uSubmission = &uSubmissions[uSubmissionsStart++ & 0xFFFFU];

                if (uSubmission->fd) {

                    const uint index = tail++ & IOU_S_MASK_CONST;

                    IOU_S_ARRAY[index] = index;

                    IOU_S_SQES[index].flags     = 0; // TODO: KEEP ALL MEMBERS OF THE STRUCTURE UP-TO-DATE
                    IOU_S_SQES[index].ioprio    = 0;
                    IOU_S_SQES[index].rw_flags  = 0;
                    IOU_S_SQES[index].opcode    = uSubmission->opcode;
                    IOU_S_SQES[index].fd        = uSubmission->fd;
                    IOU_S_SQES[index].off       = uSubmission->off;
                    IOU_S_SQES[index].addr      = uSubmission->addr;
                    IOU_S_SQES[index].len       = uSubmission->len;
                    IOU_S_SQES[index].user_data = uSubmission->data;

                } else // SE ESTE TIVER SIDO CANCELADO, IGNORA E DESCONSIDERA
                    uSubmissionsNew--;
            }

            // UPDATE THE TAIL SO THE KERNEL CAN SEE IT
            *IOU_S_TAIL = tail;

            write_barrier();

            // MANDA O KERNEL CONSUMIR O QUE JÁ FOI COLOCADO - TODO: FIXME: É PARA COLOCAR SÓ OS NOVOS OU TODOS MESMO?
            // NOTE: ASSUMINDO QUE O JAMAIS TERA ERROS
            uSubmissionsNew -= io_uring_enter(IOU_FD, uSubmissionsNew, 0, 0);

            //ASSERT(uSubmissionsNew <= 65536);

            //
            sched_yield();
#if 0
            sleep(1);
#endif

            // LÊ TODO O CQ E SETA OS RESULTADOS
            const uint uConsumeTail = *IOU_C_TAIL;

            read_barrier();

            while (uConsumeHead != uConsumeTail) {

                const IOURingCQE* const cqe = &IOU_C_CQES[uConsumeHead++ & IOU_C_MASK_CONST];

                *(u32*)(cqe->user_data) = cqe->res;
            }

            *IOU_C_HEAD = uConsumeHead;

            write_barrier();
        }

        // NEW CLIENTS
        for (uint i = 0; i != CONNECTIONS_ACCEPT_N; i++) {

            //acceptedFDs[i] = accept(sockListen, NULL, 0);

            if (acceptedFDs[i] < IO_ERR) {
                // NEW CLIENT
                dbg("NEW CLIENT");

                Connection* const conn = malloc(sizeof(Connection));

                conn->remove         = 0;
                conn->fd             = acceptedFDs[i];
                conn->iReadRes       = IO_CLEAR;
                conn->oReadRes       = IO_CLEAR;
                conn->iWriteRes      = IO_CLEAR;
                conn->oWriteRes      = IO_CLEAR;
                conn->fdCloseRes     = IO_CLEAR;
                conn->iReadCancel    = IO_CLEAR;
                conn->oReadCancel    = IO_CLEAR;
                conn->iWriteCancel   = IO_CLEAR;
                conn->oWriteCancel   = IO_CLEAR;
                conn->prev           = clients;
                conn->next           = NULL;

                if (clients)
                    clients->next = conn;

                clients = conn;

            } else {
                err("FAILED TO ACCEPT CLIENT: %s", strerror(acceptedFDs[i] - IO_ERR));
            }

            // TODO: FIXME: ENQUEUE ANOTHER ACCEPT() AT THIS SLOT
        }

        Connection* conn = clients;

        while (conn) {

            Connection* const prev = conn->prev;

            if (!conn->remove) {
                // REDIRECIONA

            }

            // REMOVE CLIENT
            if (conn->remove) {

                dbg("CONNECTION MUST BE DELETED");

                if (conn->fd &&
                    conn->fdCloseRes != IO_CLEAR &&
                    conn->fdCloseRes != IO_WAITING) {
                    dbg("CLOSE DONE");
                    // TODO: FIXME: COLOCAR COMO EM ALGUM CASO, PODE TER QUE MANDAR DE NOVO?
                    // SE FOR, FILTRAR NESTE IF
                    if (1) {
                        dbg("CLOSE DONE - RETRY");
                        conn->fd = 0;
                        conn->fdCloseRes = IO_CLEAR;
                    }
                }

                if (conn->iReadRes    == IO_WAITING &&
                    conn->iReadCancel != IO_WAITING) {
                    dbg("ENQUEUING CANCEL IN READ");
                    xtun_io_submit(&conn->iReadCancel, IORING_OP_ASYNC_CANCEL, conn->fd, (uintptr_t)&conn->iReadRes, 0, 0);
                }

                if (conn->oReadRes    == IO_WAITING &&
                    conn->oReadCancel != IO_WAITING) {
                    dbg("ENQUEUING CANCEL OUT READ");
                    xtun_io_submit(&conn->oReadCancel, IORING_OP_ASYNC_CANCEL, conn->fd, (uintptr_t)&conn->oReadRes, 0, 0);
                }

                if (conn->iWriteRes    == IO_WAITING &&
                    conn->iWriteCancel != IO_WAITING) {
                    dbg("ENQUEUING CANCEL IN WRITE");
                    xtun_io_submit(&conn->iWriteCancel, IORING_OP_ASYNC_CANCEL, conn->fd, (uintptr_t)&conn->iWriteRes, 0, 0);
                }

                if (conn->oWriteRes    == IO_WAITING &&
                    conn->oWriteCancel != IO_WAITING) {
                    dbg("ENQUEUING CANCEL OUT WRITE");
                    xtun_io_submit(&conn->oWriteCancel, IORING_OP_ASYNC_CANCEL, conn->fd, (uintptr_t)&conn->oWriteRes, 0, 0);
                }

                if (conn->fd &&
                    conn->fdCloseRes != IO_WAITING) {
                    dbg("ENQUEUING CLOSE");
                    xtun_io_submit(&conn->fdCloseRes, IORING_OP_CLOSE, conn->fd, 0, 0, 0);
                }

                if (conn->fd           == 0 &&
                    conn->fdCloseRes   != IO_WAITING &&
                    conn->iReadRes     != IO_WAITING &&
                    conn->iReadCancel  != IO_WAITING &&
                    conn->oReadRes     != IO_WAITING &&
                    conn->oReadCancel  != IO_WAITING &&
                    conn->iWriteRes    != IO_WAITING &&
                    conn->iWriteCancel != IO_WAITING &&
                    conn->oWriteRes    != IO_WAITING &&
                    conn->oWriteCancel != IO_WAITING)  {
                    // TERMINA DE REMOVER ELE
                    dbg("FINISHING CONNECTION DELETION");

                    if (conn->next)
                        conn->next->prev = conn->prev;

                    if (conn->prev)
                        conn->prev->next = conn->next;
                    else
                        clients = NULL;

                    free(conn);
                }
            }

            conn = prev;
        }
    }

    dbg("EXITING");

    //
    while (uConsumePending) {
        dbg("WAITING %u EVENTS", uConsumePending);
        sched_yield();
        uint head = *IOU_C_HEAD;
        loop {
            read_barrier();
            if (head == *IOU_C_TAIL)
                break;
            uConsumePending--;
            head++;
        }
        *IOU_C_HEAD = head;
        write_barrier(); // TODO: FIXME:
        break;
    }

    // TODO: FIXME: CUIDADO COM OS OBJETOS :S
    if (munmap(IOU_S_SQES, IOU_S_SQES_SIZE))
        fatal("FAILED TO UNMAP IOU_S_SQES");
    if (munmap(IOU_BASE, IOU_BASE_SIZE))
        fatal("FAILED TO UNMAP IOU_BASE");
    if (close(IOU_FD))
        fatal("FAILED TO CLOSE IOU_FD");

    return 0;
}
