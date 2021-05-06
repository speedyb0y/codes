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

#define IP4(a, b, c, d) (((uint)(d) << 24) | ((uint)(c) << 16) | ((uint)(b) << 8) | ((uint)a))

#define loop while(1)
#define elif(c) else if (c)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t i32;

typedef unsigned long long int uintll;

#define I_BUFFER_SIZE (64*1024*1024)
#define O_BUFFER_SIZE (2*64*1024*1024)

#define SEND_TIMEOUT 30000
#define RECEIVE_TIMEOUT 30000

#define LISTEN_PORT 7500

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

int main (void) {

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

    // BUFFERS
    u8* const oBuffer = malloc(O_BUFFER_SIZE);
    u8* const iBuffer = malloc(I_BUFFER_SIZE);

    // INITIALIZE SEND BUFFER
    for (uint i = 0; i != O_BUFFER_SIZE; i++)
        oBuffer[i] = i & 0xFFU;

    const int sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (sock == -1) {
        fprintf(stderr, "OPEN SOCKET - FAILED - %s\n", strerror(errno));
        return 1;
    }

    //
    const struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(LISTEN_PORT), .sin_addr = { .s_addr = IP4(127,0,0,1) } };

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr))) {
        fprintf(stderr, "CONNECT - FAILED - %s\n", strerror(errno));
        return 1;
    }

    loop {

        const int readen = read(sock, iBuffer, I_BUFFER_SIZE);

        if (readen == -1) {
            fprintf(stderr, "READ - FAILED - %s\n", strerror(errno));
            break;
        }

        if (readen == 0) {
            fprintf(stderr, "READ - CONNECTION ENDED\n");
            break;
        }

        const int written = write(sock, iBuffer, readen);

        if (written == -1) {
            fprintf(stderr, "WRITE - FAILED - %s\n", strerror(errno));
            break;
        }

        if (written != readen)
            break;
    }

    return 0;
}
