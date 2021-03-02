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

    if (argsN != 5)
        return 1;

    const uint cltIP   = str2ip(args[1]);
    const uint srvIP   = str2ip(args[2]);
    const uint srvPort = htons(atoi(args[3]));
    const uint EXP     = atoi(args[4]);

    SockAddrIP4 cltAddr = { .sin_family = AF_INET, .sin_port = 0,       .sin_addr = { .s_addr = cltIP } };
    SockAddrIP4 srvAddr = { .sin_family = AF_INET, .sin_port = srvPort, .sin_addr = { .s_addr = srvIP } };

    const int opt = 1;

    int conns[65536]; uint connsN = 0, errorsConnect = 0, errors = 0, connecteds = 0;

    while (1) {

        while (connsN != EXP) {

            const int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

            if (sock == -1) {
                printf("FAILED TO OPEN SOCKET: %s\n", strerror(errno));
                abort();
            }

            if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
                abort();
            }

            if (bind(sock, (SockAddr*)&cltAddr, sizeof(SockAddrIP4))) {
                printf("FAILED TO BIND SOCKET\n");
                abort();
            }

            if (!(connect(sock, (SockAddr*)&srvAddr, sizeof(SockAddrIP4)) == -1 && errno == EINPROGRESS)) {
                printf("FAILED TO CONNECT()\n");
                abort();
            }

            conns[connsN++] = -sock;

            if (connsN % 200 == 0)
                break;
        }

        for (uint i = 0; i != connsN; i++) {

            if (conns[i] < 0) {

                if (connect(-conns[i], (SockAddr*)&srvAddr, sizeof(SockAddrIP4)) == -1) {
                    if (errno == EALREADY)
                        continue; // TODO: FIXME: TIMEOUT?
                    close(-conns[i]);
                    conns[i] = 0;
                    errorsConnect++;
                    continue;
                }

                conns[i] *= -1;
                connecteds++;
            }


            // TODO: FIXME: CONTAR QUANTOS DE FATO RECEBERAM NESTE LOOP
            // E DAR TIMEOUT APOS T TEMPO SEM RECEBER X BYTES
            // OU ENTAO SERVIDOR ENVIA A CADA 3 SEGUNDOS,EU RECEBO A CADA 7

            if (conns[i])
                if (read(conns[i], buff, sizeof(buff)) == -1 && errno != EAGAIN) {
                    close(conns[i]);
                    conns[i] = 0;
                    errors++;
                }

            if (conns[i])
                if (write(conns[i], buff, 1400) == -1 && errno != EAGAIN) {
                    close(conns[i]);
                    conns[i] = 0;
                    errors++;
                }
        }

        printf("CONNS %u CONNECTEDS %u ERRORS %u ERRORS_CONNECT %u ACTIVES %u \n", connsN, connecteds, errors, errorsConnect, connsN - errorsConnect - errors);

        sleep(2);
    }

    // TODO: FIXME: SALVAR NUMERO DE PACOTES, BANDWIDTH, TEMPOS TOTAIS
    // TESTAR O TIMEOUT/KEEP ALIVE, VAI AUMENTANDO O TEMPO ENTRE OS ENVIOS
    // TODO: FIXME: CONTAR SEPARADAMENTE OS ERROS SZEOF() DE CONEXÃO

    return 0;
}
