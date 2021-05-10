/*

*/

#define _GNU_SOURCE

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/epoll.h>
#include <sys/un.h>
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

#define CONNECT_TIMEOUT 3000

#define READ_TIMEOUT 30000

#define CONNS_N 8192

#define POLL_EVENTS_N 2048
#define POLL_TIME 2000

#define PORT 7501

#define IP4(a, b, c, d) (((uint)(d) << 24) | ((uint)(c) << 16) | ((uint)(b) << 8) | ((uint)a))

typedef u8 event_t;

#define CONN_STATUS_CONNECTING    0
#define CONN_STATUS_ESTABLISH     1 // ENVIA MEU ID
#define CONN_STATUS_ESTABLISHING  2 // RECEBE O ID DO PEER
#define CONN_STATUS_ESTABLISHED   3

typedef struct Conn Conn;

struct Conn {
    u32 id;
    u16 srv;
    u8 status;
    u8 r; // event_t
    u16 sock;
    u16 peerSock;
    u32 peerID;
    u64 rTotal;
    u64 wTotal;
    u64 timeout;
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

#define log_conn(msg, ...) printf("%12llu %s %llu: " msg "\n", (uintll)now, conn->srv ? "SERVER" : "CLIENT", (uintll)conn->id, ##__VA_ARGS__)

#define STAT(name) ((uintll)name[0]), ((uintll)name[1])
#define stat_inc(name) (name[conn->srv]++)

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

    event_t listenIO = 0;

    const int listenSock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);

    if (listenSock == -1) {
        fprintf(stderr, "OPEN LISTEN SOCKET - FAILED - %s\n", strerror(errno));
        return 1;
    }

    //
    struct sockaddr_un addrConnect = { 0 };
    strcpy(addrConnect.sun_path, "/shadow=socks");
    addrConnect.sun_family = AF_UNIX;
    addrConnect.sun_path[0] = 0;
    char cmd[8]; *(u32*)cmd = IP4(177,209,139,173); *(u16*)(cmd + 4) = htons(PORT);

    const struct sockaddr_in addrListen  = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr = { .s_addr = IP4(0,0,0,0) } };

    if (bind(listenSock, (struct sockaddr*)&addrListen, sizeof(addrListen))) {
        fprintf(stderr, "BIND - FAILED - %s\n", strerror(errno));
        return 1;
    }

    if (listen(listenSock, 65536)) {
        fprintf(stderr, "LISTEN - FAILED - %s\n", strerror(errno));
        return 1;
    }

    struct epoll_event event = { .events = EPOLLIN | EPOLLERR | EPOLLHUP, .data = { .ptr = &listenIO } };

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, listenSock, &event)) {
        fprintf(stderr, "????? - FAILED - %s\n", strerror(errno));
        return 1;
    }

    Conn conns[65536]; u64 connsCounter = 0;

    u16 socksByID[CONNS_N]; uint socksN = 0;

    u64 createds    [2] = { 0, 0 };
    u64 mismatches  [2] = { 0, 0 };
    u64 timeout     [2] = { 0, 0 };
    u64 endPeer     [2] = { 0, 0 };
    u64 endMe       [2] = { 0, 0 };

    uint counter = 0;

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
            *(event_t*)events[eventsFD].data.ptr = 1;

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

                if (socksN == CONNS_N) {
                    fprintf(stderr, "TOO MANY CONNECTIONS\n");
                    close(sock);
                    continue;
                }

                Conn* const conn = &conns[sock];

                conn->id = connsCounter++;
                conn->srv = 1;
                conn->sock = sock;
                conn->r = 1;
                conn->rTotal = 0;
                conn->wTotal = 0;
                conn->timeout = now + READ_TIMEOUT;
                conn->status = CONN_STATUS_ESTABLISH;
                conn->peerID = conn->id;
                conn->peerSock = conn->sock;

                stat_inc(createds);

                log_conn("CREATED - SOCKET %d", (int)conn->sock);

                struct epoll_event event = { .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP, .data = { .ptr = &conn->r } };

                if (epoll_ctl(epollFD, EPOLL_CTL_ADD, conn->sock, &event)) {
                    log_conn("FAILED TO ADD SOCKET TO EPOLL - %s", strerror(errno));
                    return 1;
                }

                socksByID[socksN++] = conn->sock;
            }
        }

        // CONNECT
        if (socksN != CONNS_N) {

            const int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

            Conn* const conn = &conns[sock];

            conn->id = connsCounter++;
            conn->srv = 0;
            conn->sock = sock;
            conn->r = 0;
            conn->rTotal = 0;
            conn->wTotal = 0;
            conn->timeout = now + CONNECT_TIMEOUT;
            conn->status = CONN_STATUS_CONNECTING;
            conn->peerID = conn->id;
            conn->peerSock = conn->sock;

            (void)connect(conn->sock, (struct sockaddr*)&addrConnect, sizeof(addrConnect));

            log_conn("CREATED - SOCKET %d", (int)conn->sock);

            stat_inc(createds);

            struct epoll_event event = { .events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP, .data = { .ptr = &conn->r } };

            if (epoll_ctl(epollFD, EPOLL_CTL_ADD, conn->sock, &event)) {
                log_conn("FAILED TO ADD SOCKET TO EPOLL - %s", strerror(errno));
                return 1;
            }

            socksByID[socksN++] = conn->sock;
        }

        // HANDLE
        uint sockID = 0;

        counter++;

        while (sockID != socksN) {

            Conn* const conn = &conns[socksByID[sockID]];

            if (conn->status == CONN_STATUS_CONNECTING) {
                if (conn->r) { // CONECTANDO AO PROXY

                    if (connect(conn->sock, (struct sockaddr*)&addrConnect, sizeof(addrConnect))) {
                        log_conn("CONNECTING - CONNECT FAILED - %s", strerror(errno));
                        goto CLOSE;
                    }

                    if (write(conn->sock, cmd, 6) != 6) {
                        log_conn("CONNECTING - WRITE CONNECT COMMAND - FAILED - %s", strerror(errno));
                        goto CLOSE;
                    }

                    struct epoll_event event = { .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP, .data = { .ptr = &conn->r } };

                    if (epoll_ctl(epollFD, EPOLL_CTL_MOD, conn->sock, &event)) {
                        log_conn("CONNECTING - FAILED TO MODIFY SOCKET TO EPOLL - %s", strerror(errno));
                        goto CLOSE;
                    }

                    log_conn("CONNECTING - DONE");
                    conn->timeout = now + READ_TIMEOUT;
                    conn->status = CONN_STATUS_ESTABLISH;
                } else {
                    log_conn("CONNECTING - NOT YET");
                    goto WAITING;
                }
            }

            if (conn->status == CONN_STATUS_ESTABLISH) {
                conn->status = CONN_STATUS_ESTABLISHING;
                // ENVIA MEU ID
                if (write(conn->sock, conn, sizeof(Conn)) != sizeof(Conn)) {
                    log_conn("ESTABLISHING - FAILED TO SEND MY ID");
                    goto CLOSE;
                }
            }

            if (conn->status == CONN_STATUS_ESTABLISHING) {
                if (conn->r) {

                    Conn peer;

                    const int size = read(conn->sock, &peer, sizeof(Conn));

                    if (size == -1) {
                        if (errno == EAGAIN) {
                            log_conn("ESTABLISHING - EAGAIN");
                            conn->r = 0;
                            goto WAITING;
                        } else {
                            log_conn("ESTABLISHING - FAILED - %s", strerror(errno));
                            goto CLOSE;
                        }
                    } elif (size == sizeof(Conn)) {
                        log_conn("ESTABLISHING - DONE");
                        //if (peer.sock > 65536) {
                            //goto CLOSE;
                        //}
                        //if (peer.id >= connsCounter) {
                            //goto CLOSE;
                        //}
                        conn->peerID = peer.id;
                        conn->peerSock = peer.sock;
                        conn->timeout = now + READ_TIMEOUT;
                        conn->status = CONN_STATUS_ESTABLISHED;
                    } elif (size) {
                        log_conn("ESTABLISHING - INCOMPLETE");
                        goto CLOSE;
                    } else {
                        log_conn("ESTABLISHING - CONNECTION ENDED");
                        goto CLOSE;
                    }
                } else
                    goto WAITING;
            }

            if (conn->r) {

                const int size = read(conn->sock, rBuff, R_BUFF_SIZE);

                if (size == -1) {
                    if (errno == EAGAIN) {
                        log_conn("READ - EAGAIN - @%llu", (uintll)conn->rTotal);
                        conn->r = 0;
                    } else {
                        log_conn("READ - FAILED - %s", strerror(errno));
                        goto CLOSE;
                    }
                } elif (size) {
                    log_conn("READ - @%llu +%llu", (uintll)conn->rTotal, (uintll)size);
                    const uint mismatch = diff_at(wBuff + (conn->rTotal % 256), rBuff, size);
                    if (mismatch == (uint)size) {
                        conn->rTotal += size;
                        conn->timeout = now + READ_TIMEOUT;
                    } else {
                        log_conn("READ - INVALID - OFFSET %llu - EXPECTED %02X - RECEIVED %02X", (uintll)(conn->rTotal + mismatch), wBuff[(conn->rTotal % 256) + mismatch], rBuff[mismatch]);
                        conn->rTotal += mismatch;
                        stat_inc(mismatches);
                        goto CLOSE;
                    }
                } else {
                    // TODO: FIXME: PODE ATÉ TERMINAR NO MOMENTO DE DAR O WRITE,MAS TER TERMINADO?
                    if (conn->rTotal % 256 == 0) {   // TODO: FIXME: LER DIRETAMENTE DO PEER CONN
                        log_conn("READ - NOTHING - OK");
                        stat_inc(endPeer); // readNothingOk
                    } else {
                        log_conn("READ - NOTHING - ABRUPT");
                        //stat_inc(readNothingAbrupt);
                    }
                    goto CLOSE;
                }
            }
                 // TODO: FIXME: SE DER OVERFLOW, VAI FUNCIONAR COMO QUEREMOS?
            if ((conn->wTotal - conn->rTotal) < 2*1024*1024 && (counter + conn->id) % 8 == 0) { // TODO: FIXME: SÓ COMEÇAR A ENVIAR APÓS CONECTADO E IDENTIFICADO O PEER

                const uint offset = conn->wTotal % 256;
                const uint available = W_BUFF_SIZE - offset;
                const uint howmuch = rdtsc() % available;

                const int size = write(conn->sock, wBuff + offset, howmuch);

                if (size == -1) {
                    if (errno == EAGAIN) {
                        log_conn("WRITE - EAGAIN - @%llu", (uintll)conn->wTotal);
                    } else {
                        log_conn("WRITE - FAILED - %s", strerror(errno));
                        goto CLOSE;
                    }
                } elif (size) {
                    log_conn("WRITE - @%llu +%llu", (uintll)conn->wTotal, (uintll)size);
                    conn->wTotal += size;
                } else
                    log_conn("WRITE - NOTHING");
            }

            if (conn->wTotal >= (4*1024*1024) &&
                conn->wTotal % 256 == 0) {
                log_conn("FINISHED");
                stat_inc(endMe);
                goto CLOSE;
            }

            // TODO: FIXME: CONFIRMAR QUE O PEER ID E SOCK CONTINUA O MESMO
            // conns[conn->sock].id == conn->peerID
            // conns[conn->sock].peerID == conn->id

WAITING:
            if (conn->timeout <= now) {
                log_conn("TIMEOUT");
                stat_inc(timeout);
                goto CLOSE;
            }
OK:
            sockID++;

            continue;
CLOSE:
            log_conn("CLOSING - WRITTEN %llu READEN %llu", (uintll)conn->wTotal, (uintll)conn->rTotal);

            close(conn->sock);

            if (sockID != --socksN)
                socksByID[sockID] = socksByID[socksN];
        }
    }

    printf(
        "             CLT   SRV"
        "CREATED     %12llu %llu\n"
        "MISMATCH    %12llu %llu\n"
        "TIMEOUT     %12llu %llu\n"
        "END PEER    %12llu %llu\n"
        "END ME      %12llu %llu\n"
        ,
        STAT(createds),
        STAT(mismatches),
        STAT(timeout),
        STAT(endPeer),
        STAT(endMe)
        );

    return 0;
}
