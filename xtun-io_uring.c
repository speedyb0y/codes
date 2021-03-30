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

    // EPOLL
    const int epollFD = epoll_create1(EPOLL_CLOEXEC);

    // CONNECTIONS
    Connection conns[CONNECTIONS_N]; uint connsN = 0;

    // WAIT FOR CONNECTIONS
    const int sockListen = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (sockListen == -1)
        fatal("FAILED TO OPEN LISTENING SOCKET: %s", strerror(errno));

    epoll_event event = { .events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP, .data = { .fd = sockListen } };

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, sockListen, &event))
        fatal("FAILED TO ADD LISTEN SOCKET TO EPOLL: %s", strerror(errno));

    // PREVENT EADDRINUSE
    const int sockOptReuseAddr = 1;

    if (setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptReuseAddr, sizeof(sockOptReuseAddr)) < 0)
        fatal("FAILED TO SET REUSE ADDR: %s", strerror(errno));

    const sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = "\x00xtun\x00" };

    dbg("BINDING...");

    if (bind(sockListen, (sockaddr*)&addr, sizeof(addr)))
        fatal("FAILED TO BIND: %s", strerror(errno));

    dbg("LISTENING...");

    if (listen(sockListen, 512))
        fatal("FAILED TO LISTEN FOR CLIENTS: %s", strerror(errno));

    dbg("ENTERING LOOP...");

    while (!sigTERM) {

        { // POLL IO

        }

        // NEW CLIENTS
        while (1) {

            const int sock = accept4(sockListen, NULL, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);

            if (sock == -1) {
                if (errno != EAGAIN)
                    err("FAILED TO ACCEPT CLIENT: %s", strerror(errno));
                break;
            }

            // NEW CLIENT
            dbg("NEW CLIENT");

            conns[connsN].fd = sock;
            connsN++;
        }

        //
        uint i = 0;

        while (i != connsN) {

            Connection* const conn = &conns[i];

            int remove = 0;

            // DO WHAT NEEDS TO BE DONE
            if (!remove) {
                // SOME ERROR
                remove = 1;
            }

            if (!remove) {
                // SOME ERROR
                remove = 1;
            }

            // REMOVE CLIENT
            if (remove) {
                dbg("CONNECTION MUST BE DELETED");

                close(conn->fd);

                connsN--;

                if (connsN) {
                    conn->fd = conns[connsN].fd;
                } else
                    i++;
            } else
                i++;
        }
    }

    dbg("EXITING");

    // TODO: FIXME: CLOSE CONNECTIONS

    close(epollFD);

    return 0;
}
