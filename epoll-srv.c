/*

*/

#define _GNU_SOURCE

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#if EAGAIN != EWOULDBLOCK
#error
#endif

#define loop while
#define elif(c) else if (c)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t i32;

typedef unsigned long long int uintll;

#define R_BUFF_SIZE (64*1024*1024)
#define W_BUFF_SIZE (2*64*1024*1024)

#define CONN_READ  0b01U
#define CONN_WRITE 0b10U

#define WRITE_TIMEOUT 30000
#define READ_TIMEOUT 30000

#define CONNS_N 8192

#define POLL_EVENTS_N 2048
#define POLL_TIME 2000

#define LISTEN_PORT 7500

typedef struct Conn Conn;

typedef u32 rw_t;

struct Conn {
    u64 id;
    u32 rw; // rw_t
    u32 sock;
    u64 rTotal;
    u64 rTime;
    u64 wTotal;
    u64 wTime;
};

static volatile sig_atomic_t sigTERM;

static void signal_handler (int sig) {

    switch (sig) {
        case SIGINT:
        case SIGTERM:
            sigTERM = 1;
    }
}

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

static inline uint diff_at (const u8* restrict a, const u8* restrict b, uint size) {

    const u8* const b_ = b;

    while (size-- && *a == *b)
        { a++; b++; }

    return b - b_;
}

#define log_conn(msg, ...) printf("%12llu CONNECTION %llu: " msg "\n", (uintll)now, (uintll)conn->id, ##__VA_ARGS__)

int main (void) {

    if (R_BUFF_SIZE % 256)
        return 1;
    if (W_BUFF_SIZE % 256)
        return 1;

    // SIGNALS
    sigTERM = 0;

    // IGNORE ALL
    struct sigaction action = { 0 };

    action.sa_restorer = NULL;
    action.sa_flags = 0;
    action.sa_handler = SIG_IGN;

    for (int sig = 0; sig != NSIG; sig++)
        sigaction(sig, &action, NULL);

    // HANDLE ONLY THESE
    action.sa_handler = signal_handler;

    sigaction(SIGINT,  &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    // MAX OPEN SOCKETS
    { struct rlimit limit = { 65536, 65536 };

        if (setrlimit(RLIMIT_NOFILE, &limit) < 0)
            return 1;
    }

    // BUFFERS
    u8* const wBuff = malloc(W_BUFF_SIZE);
    u8* const rBuff = malloc(R_BUFF_SIZE);

    // INITIALIZE SEND BUFFER
    for (uint i = 0; i != W_BUFF_SIZE; i++)
        wBuff[i] = i & 0xFFU;

    // EPOLL
    const int epollFD = epoll_create1(EPOLL_CLOEXEC);

    if (epollFD == -1) {
        fprintf(stderr, "EPOLL CREATE - FAILED - %s\n", strerror(errno));
        return 1;
    }

    u32 listenIO = 0;

    const int listenSock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);

    if (listenSock == -1) {
        fprintf(stderr, "OPEN LISTEN SOCKET - FAILED - %s\n", strerror(errno));
        return 1;
    }

    //
    const struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(LISTEN_PORT), .sin_addr = { .s_addr = 0 } };

    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr))) {
        fprintf(stderr, "BIND - FAILED - %s\n", strerror(errno));
        return 1;
    }

    if (listen(listenSock, 2048)) {
        fprintf(stderr, "LISTEN - FAILED - %s\n", strerror(errno));
        return 1;
    }

    struct epoll_event event = { .events = EPOLLET | EPOLLIN | EPOLLERR | EPOLLHUP, .data = { .ptr = &listenIO } };

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, listenSock, &event)) {
        fprintf(stderr, "????? - FAILED - %s\n", strerror(errno));
        return 1;
    }

    Conn* conns[CONNS_N]; uint connsN = 0; u64 connsCounter = 0;

    while (!sigTERM) {

        // EPOLL
        struct epoll_event events[POLL_EVENTS_N];

        int eventsFD = epoll_wait(epollFD, events, POLL_EVENTS_N, POLL_TIME);

        if (eventsFD == -1) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "EPOLL WAIT - FAILED - %s\n", strerror(errno));
            return 1;
        }

        while (eventsFD--)
            *(rw_t*)events[eventsFD].data.ptr |=
                (!!(events[eventsFD].events & (EPOLLIN  | EPOLLERR | EPOLLHUP | EPOLLRDHUP))) | // CONN_READ
               ((!!(events[eventsFD].events & (EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP))) < 1) // CONN_WRITE
                ;

        //
        struct timespec now_;

        if (clock_gettime(CLOCK_BOOTTIME, &now_)) {
            fprintf(stderr, "CLOCK GET TIME - FAILED - %s\n", strerror(errno));
            return 1;
        }

        const u64 now = (u64)now_.tv_sec * 1000 + (u64)now_.tv_nsec / 1000000;

        // ACCEPT
        if (listenIO) {
            listenIO = 0;

            while (!sigTERM) {

                const int sock = accept4(listenSock, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);

                if (sock == -1) {
                    if (errno == EAGAIN)
                        break;
                    fprintf(stderr, "ACCEPT - FAILED - %s\n", strerror(errno));
                    return 1;
                }

                if (connsN == CONNS_N) {
                    fprintf(stderr, "TOO MANY CONNECTIONS\n");
                    close(sock);
                    continue;
                }

                Conn* const conn = malloc(sizeof(Conn));

                conn->id = connsCounter++;
                conn->sock = sock;
                conn->rw = CONN_READ | CONN_WRITE;
                conn->rTotal = 0;
                conn->wTotal = 0;
                conn->rTime = now;
                conn->wTime = now;

                log_conn("CREATED - SOCKET %d", (int)conn->sock);

                struct epoll_event event = { .events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP, .data = { .ptr = &conn->rw } };

                if (epoll_ctl(epollFD, EPOLL_CTL_ADD, conn->sock, &event)) {
                    log_conn("FAILED TO ADD SOCKET TO EPOLL - %s", strerror(errno));
                    return 1;
                }

                conns[connsN++] = conn;
            }
        }

        // HANDLE
        uint connID = 0;

        while (connID != connsN) {

            Conn* const conn = conns[connID];

            if (conn->rw & CONN_READ) {

                const int size = read(conn->sock, rBuff, R_BUFF_SIZE);

                if (size == -1) {
                    if (errno != EAGAIN) {
                        log_conn("READ - FAILED - %s", strerror(errno));
                        goto ERR;
                    }
                    log_conn("READ - EAGAIN - @%llu", (uintll)conn->rTotal);
                    conn->rw ^= CONN_READ;
                } elif (size) {
                    log_conn("READ - @%llu +%llu", (uintll)conn->rTotal, (uintll)size);
                    const uint mismatch = diff_at(wBuff + (conn->rTotal % 256), rBuff, size);
                    if (mismatch != (uint)size) {
                        log_conn("READ - INVALID - OFFSET %llu - EXPECTED %02X - RECEIVED %02X", (uintll)(conn->rTotal + mismatch), wBuff[(conn->rTotal % 256) + mismatch], rBuff[mismatch]);
                        goto ERR;
                    }
                    conn->rTotal += size;
                    conn->rTime = now;
                } else {
                    log_conn("READ - CONNECTION ENDED");
                    goto ERR;
                }
            }

            if (conn->rw & CONN_WRITE) {

                const uint offset = conn->wTotal % 256;
                const uint available = W_BUFF_SIZE - offset;
                const uint howmuch = rdtsc() % available;

                //log_conn("WRITE - @%llu TRY [%u:+%u] ...", (uintll)conn->wTotal, offset, howmuch);

                const int size = write(conn->sock, wBuff + offset, howmuch);

                if (size == -1) {
                    if (errno != EAGAIN) {
                        log_conn("WRITE - FAILED - %s", strerror(errno));
                        goto ERR;
                    }
                    log_conn("WRITE - EAGAIN - @%llu", (uintll)conn->wTotal);
                    conn->rw ^= CONN_WRITE;
                } elif (size) {
                    log_conn("WRITE - @%llu +%llu", (uintll)conn->wTotal, (uintll)size);
                    conn->wTotal += size;
                    conn->wTime = now;
                } else
                    log_conn("WRITE - NOTHING");
            }

            if ((conn->wTime + WRITE_TIMEOUT) <= now) {
                log_conn("WRITE TIMEOUT");
                goto ERR;
            }

            if ((conn->rTime + READ_TIMEOUT) <= now) {
                log_conn("READ TIMEOUT");
                goto ERR;
            }

            connID++;

            continue;
ERR:
            log_conn("CLOSING - WRITTEN %llu READEN %llu",  (uintll)conn->wTotal, (uintll)conn->rTotal);

            if (conn->sock)
                close(conn->sock);

            free(conn);

            if (connID != --connsN)
                conns[connID] = conns[connsN];
        }
    }

    return 0;
}
