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

    static char buff[1*1024*1024] =
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

    if (argsN != 5)
        return 1;

    const uint cltIP   = str2ip(args[1]);
    const uint srvIP   = str2ip(args[2]);
    const uint srvPort = htons(atoi(args[3]));
    const uint EXP     = atoi(args[4]);

    SockAddrIP4 cltAddr = { .sin_family = AF_INET, .sin_port = 0,       .sin_addr = { .s_addr = cltIP } };
    SockAddrIP4 srvAddr = { .sin_family = AF_INET, .sin_port = srvPort, .sin_addr = { .s_addr = srvIP } };

    const int opt = 1;

    int conns[65536]; uint connsN = 0, connectRefused = 0, connectTimeout = 0, errors = 0, connectSuccess = 0;

    while (1) {

        if (connsN != EXP) {

            const int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

            if (sock == -1) {
                printf("FAILED TO OPEN SOCKET: %s\n", strerror(errno));
                return 1;
            }

            if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
                printf("FAILED TO SOCKET NO DELAY\n");
                return 1;
            }

            if (bind(sock, (SockAddr*)&cltAddr, sizeof(SockAddrIP4))) {
                printf("FAILED TO BIND SOCKET\n");
                return 1;
            }

            if (!(connect(sock, (SockAddr*)&srvAddr, sizeof(SockAddrIP4)) == -1 && errno == EINPROGRESS)) {
                printf("FAILED TO CONNECT()\n");
                return 1;
            }

            conns[connsN++] = -sock;

            if (connsN % 50)
                continue;
        }

        uint iPkts = 0; uintll iSize = 0;
        uint oPkts = 0; uintll oSize = 0;

        for (uint i = 0; i != connsN; i++) {

            if (i % 250 == 0)
                sleep(1);

            // TODO: FIXME: CONTAR QUANTOS DE FATO RECEBERAM NESTE LOOP
            // E DAR TIMEOUT APOS T TEMPO SEM RECEBER X BYTES
            // OU ENTAO SERVIDOR ENVIA A CADA 3 SEGUNDOS,EU RECEBO A CADA 7

            if (conns[i] == 0)
                continue;

            if (conns[i] < 0) {

                if (connect(-conns[i], (SockAddr*)&srvAddr, sizeof(SockAddrIP4)) == -1) {
                    switch (errno) {
                        case EALREADY:
                            continue; // TODO: FIXME: TIMEOUT?
                        case ETIMEDOUT:
                            connectTimeout++;
                            break;
                        case ECONNREFUSED:
                            connectRefused++;
                            break;
                        default:
                            printf("CONNECT FAILED - %s\n", strerror(errno));
                            return 1;
                    }
                    close(-conns[i]);
                    conns[i] = 0;
                    continue;
                }

                conns[i] *= -1;
                connectSuccess++;
            }

            const int in = read(conns[i], buff, sizeof(buff));

            if (in == 0) {
                close(conns[i]);
                conns[i] = 0;
                errors++; // TODO: FIXME: CLOSEDS
                continue;
            }

            if (in == -1) {
                if (errno != EAGAIN) {
                    close(conns[i]);
                    conns[i] = 0;
                    errors++;
                    continue;
                }
            } else {
                iSize += in;
                iPkts++;
            }

            const int out = write(conns[i], buff, 1400);

            if (out == -1) {
                if (errno != EAGAIN) {
                    close(conns[i]);
                    conns[i] = 0;
                    errors++;
                }
            } else {
                oSize += out + 20 + 20 + 8;
                oPkts++;
            }
        }

        printf("%5s | %12s | %12s | %12s | %5s | %7s | %-9s | %-14s | %-9s | %-14s\n", "CONNS", "CONN REFUSED", "CONN TIMEOUT", "CONN SUCCESS", "ERRS", "ACTIVES", "OUT PKTS", "OUT BYTES", "IN PKTS", "IN SIZE");
        printf("%5u | %12u | %12u | %12u | %5u | %7u | %9u | %14llu | %9u | %14llu \n", connsN, connectRefused, connectTimeout, connectSuccess, errors, connectSuccess - errors, oPkts, oSize, iPkts, iSize);
    }

    return 0;
}

// TODO: FIXME: UMA OUTRA VERSÃO, QUE VAI RECRIANDO TODOS OS FD == 0, E RECONNECTANDO OS < 0
