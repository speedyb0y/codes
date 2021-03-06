/*

*/

#define _GNU_SOURCE

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>

typedef long long intll;

typedef unsigned int uint;
typedef unsigned long long int uintll;

typedef uint8_t byte;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t i32;

typedef struct sockaddr SockAddr;
typedef struct sockaddr_in SockAddrIP4;
typedef struct sockaddr_in6 SockAddrIP6;

static uint str2ip (const char* const str) {

    uint a, b, c, d;

    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
        return
            ((uint)d << 24) |
            ((uint)c << 16) |
            ((uint)b <<  8) |
            ((uint)a);

    return 0;
}

int main (int argsN, char* args[]) {

    char buff[65536] =
        "e32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgk"
        "jdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggew"
        "Egjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghre"
        "ioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t765"
        "7kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@"
        "we#@@4ergaqw54t7657kuymhg;dfweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!"
        "23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32"
        "453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539"
        "809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlke"
        "g9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoig"
        "hweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gw564864640436774eggreg25e321!!23tge23tgewwgegge@we"
        ;

    if (argsN != 4) {
        printf("USAGE: %s CLIENT_IP SERVER_IP SERVER_PORT\n", args[0]);
        return 1;
    }

    const uint cltIP   = str2ip(args[1]);
    const uint srvIP   = str2ip(args[2]);
    const uint srvPort = htons(atoi(args[3]));

    const int listening = socket(AF_INET, SOCK_NONBLOCK | SOCK_STREAM, 0);

    if (listening == -1) {
        printf("FAILED TO OPEN SOCKET\n");
        return 1;
    }

    const SockAddrIP4 srvAddr = { .sin_family = AF_INET, .sin_port = srvPort, .sin_addr = { .s_addr = srvIP } };

    if (bind(listening, (SockAddr*)&srvAddr, sizeof(SockAddrIP4))) {
        printf("FAILED TO BIND\n");
        return 1;
    }

    if (listen(listening, 65536)) {
        printf("FAILED TO LISTEN\n");
        return 1;
    }

    const int opt = 1;

    u16 conns[65536]; uint connsN = 0;

    while (1) {

        while (connsN != 65536) {

            SockAddrIP4 addr; socklen_t addrSize = sizeof(addr);

            const int sock = accept4(listening, (SockAddr*)&addr, &addrSize, O_NONBLOCK | SOCK_CLOEXEC);

            if (sock == -1) {
                if (errno == EAGAIN)
                    break;
                abort();
            }

            if (cltIP && cltIP != addr.sin_addr.s_addr) {
                close(sock);
                continue;
            }

            setsockopt((conns[connsN++] = (uint)sock), IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        }

        for (uint i = 0; i != connsN; i++) {

            if (i % 250 == 0)
                sleep(1);

            if (conns[i] == 0)
                continue;

            const int in = read(conns[i], buff, sizeof(buff));

            if (in == 0) {
                close(conns[i]);
                conns[i] = 0;
                continue;
            }

            if (in == -1) {
                if (errno != EAGAIN) {
                    close(conns[i]);
                    conns[i] = 0;
                    continue;
                }
            }

            if (write(conns[i], buff, 1400) == -1) {
                if (errno != EAGAIN) {
                    close(conns[i]);
                    conns[i] = 0;
                }
            }
        }

        sleep(1);
    }

    return 0;
}
