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
#include <sys/wait.h>
#include <sys/epoll.h>
#include <linux/if_tun.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
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
typedef struct epoll_event epoll_event;
typedef struct mmsghdr mmsghdr;
typedef struct msghdr msghdr;
typedef struct iovec iovec;

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
#define CONNECTIONS_N 65536

typedef struct Connection Connection;

struct Connection {
    u32 fd;
    u32 status;
};

int main (void) {

    sigTERM = sigUSR1 = sigUSR2 = 0;

    struct sigaction action = { 0 };

    action.sa_restorer = NULL;
    action.sa_flags = 0;
    action.sa_handler = SIG_IGN;

    for (int sig = 0; sig != NSIG; sig++)
        sigaction(sig, &action, NULL);

    action.sa_handler = xtun_signal_handler;

    sigaction(SIGINT,  &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    const int epollFD = epoll_create1(EPOLL_CLOEXEC);

    Connection conns[CONNECTIONS_N]; Connection* connsLmt = conns;

    u32 listenReady = EPOLLIN;

    const int listenFD = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (listenFD == -1)
        fatal("FAILED TO OPEN LISTENING SOCKET: %s", strerror(errno));

    epoll_event event = { .events = EPOLLET | EPOLLIN | EPOLLERR, .data = { .ptr = &listenReady } };

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, listenFD, &event))
        fatal("FAILED TO ADD LISTEN SOCKET TO EPOLL: %s", strerror(errno));

    //const int sockOptReuseAddr = 1;

    //if (setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptReuseAddr, sizeof(sockOptReuseAddr)) < 0)
        //fatal("FAILED TO SET REUSE ADDR: %s", strerror(errno));

    dbg("BIND");

    sockaddr_un addr;

    clear(&addr, sizeof(addr));

    addr.sun_family = AF_UNIX;
    addr.sun_path[1] = 'x';
    addr.sun_path[2] = 't';
    addr.sun_path[3] = 'u';
    addr.sun_path[4] = 'n';

    if (bind(listenFD, (sockaddr*)&addr, sizeof(addr)))
        fatal("BIND FAILED: %s", strerror(errno));

    dbg("LISTEN");

    if (listen(listenFD, 512))
        fatal("LISTEN FAILED: %s", strerror(errno));

    dbg("ENTERING LOOP");

    while (!sigTERM) {

        {
            epoll_event events[CONNECTIONS_N];

            dbg("EPOLL WAIT");

            int eventsN = epoll_wait(epollFD, events, CONNECTIONS_N, 5000);

            if (eventsN == -1) {
                dbg("EPOLL WAIT - INTERRUPTED");
                if (errno != EAGAIN)
                    fatal("EPOLL WAIT FAILED: %s", strerror(errno));
            } elif (eventsN) {
                dbg("EPOLL WAIT - EVENTS: %d", eventsN);
                while (eventsN--) {
                    dbg("EPOLL WAIT - EVENT");
                    *(u32*)(events[eventsN].data.ptr) |=
                        (EPOLLIN  * !!(events[eventsN].events & (EPOLLIN  | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP))) |
                        (EPOLLOUT * !!(events[eventsN].events & (EPOLLOUT | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP)));
                }
            } else {
                dbg("EPOLL WAIT - TIMED OUT");
            }
        }

        if (listenReady) {
            loop {
                dbg("ACCEPT");

                const int sock = accept4(listenFD, NULL, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);

                if (sock == -1) {
                    dbg("ACCEPT - NO MORE");
                    if (errno != EAGAIN)
                        fatal("ACCEPT FAILED: %s", strerror(errno));
                    listenReady = 0;
                    break;
                }

                epoll_event event = { .events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP, .data = { .ptr = &connsLmt->status } };

                connsLmt->fd = sock;
                connsLmt->status = EPOLLIN | EPOLLOUT;
                connsLmt++;

                if (epoll_ctl(epollFD, EPOLL_CTL_ADD, sock, &event))
                    fatal("FAILED TO ADD ACCEPTED SOCKET TO EPOLL: %s", strerror(errno));
            }
        }

        //
        Connection* conn = conns;

        while (conn != connsLmt) {

            dbg("HANDLING CONN");

            int remove = 0;

            if (conn->status & EPOLLIN) {

                dbg("HANDLING IN");

                char chunk[65536];

                const int size = read(conn->fd, chunk, sizeof(chunk));

                if (size == -1) {
                    conn->status &= ~EPOLLIN;
                    if (errno != EAGAIN) {
                        // SOME ERROR
                        dbg("FAILED TO READ ON CONNECTION: %s", strerror(errno));
                        remove = 1;
                    }
                } elif (size) {
                    dbg("RECEIVED %d BYTES", size);

                } else {
                    dbg("CONNECTION CLOSED");
                    remove = 0;
                }
            }

            if (conn->status & EPOLLOUT) {

                dbg("HANDLING OUT");

                char chunk[65536];

                const int size = write(conn->fd, chunk, sizeof(chunk));

                if (size == -1) {
                    conn->status &= ~EPOLLOUT;
                    if (errno != EAGAIN) {
                        // SOME ERROR
                        dbg("FAILED TO WRITE ON CONNECTION: %s", strerror(errno));
                        remove = 1;
                    }
                } elif (size) {
                    dbg("SENT %d BYTES", size);

                }
            }

            if (remove) {
                dbg("DELETING CONNECTION");

                close(conn->fd);

                connsLmt--;

                if (conn != connsLmt) {
                    conn->fd     = connsLmt->fd;
                    conn->status = connsLmt->status;
                }
            } else
                conn++;
        }
    }

    dbg("EXITING");

    { Connection* conn = conns;

        while (conn != connsLmt) {
            close(conn->fd);
            conn++;
        }
    }

    close(epollFD);

    return 0;
}
