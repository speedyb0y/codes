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

    if (argsN != 6)
        return 1;

    const uint cltIP   = str2ip(args[1]);
    const uint srvIP   = str2ip(args[2]);
    const uint srvPort = htons(atoi(args[3]));
    const uint connsN  = atoi(args[4]);
    const uint pktsN   = atoi(args[5]);

    u16 conns[connsN];

    SockAddrIP4 cltAddr = { .sin_family = AF_INET, .sin_port = 0,       .sin_addr = { .s_addr = cltIP } };
    SockAddrIP4 srvAddr = { .sin_family = AF_INET, .sin_port = srvPort, .sin_addr = { .s_addr = srvIP } };

    const int opt = 1;

    uint i = 0;

    while (i != connsN) {

        const int sock = socket(AF_INET, SOCK_STREAM, 0);

        if (sock != -1) {
            if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0 &&
                bind(sock, (SockAddr*)&cltAddr, sizeof(SockAddrIP4)) == 0 &&
                connect(sock, (SockAddr*)&srvAddr, sizeof(SockAddrIP4)) == 0) {
                conns[i++] = (uint)sock;
            } else
                close(sock);
        }
    }

    printf("TRANSFERRING\n");

    char buff[1400] =
        "e32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgk"
        "jdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggew"
        "Egjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghre"
        "ioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t765"
        "7kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@"
        "we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!"
        "23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32"
        "453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlkeg9043890t347890539"
        "809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoighweooyihnfgkjdjlke"
        "g9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we#@@4ergaqw54t7657kuymhg;dfwekjghreioht5r9jIewgweggewEgjwoig"
        "hweooyihnfgkjdjlkeg9043890t347890539809(*&*(*@$*(ewe32453425gww325e321!!23tge23tgewwgegge@we"
        ;

    uint errors = 0;

    for (uint count = pktsN; count; count--) {

        for (uint i = 0; i != connsN; i++)
            if (conns[i])
                if (write(conns[i], buff, sizeof(buff)) != sizeof(buff)) {
                    close(conns[i]);
                    conns[i] = 0;
                    errors++;
                }

        for (uint i = 0; i != connsN; i++)
            if (conns[i])
                if (read (conns[i], buff, sizeof(buff)) != sizeof(buff)) {
                    close(conns[i]);
                    conns[i] = 0;
                    errors++;
                }
    }

    printf("ERRORS: %u\n", errors);

    // TODO: FIXME: SALVAR NUMERO DE PACOTES, BANDWIDTH, TEMPOS TOTAIS
    // TESTAR O TIMEOUT/KEEP ALIVE, VAI AUMENTANDO O TEMPO ENTRE OS ENVIOS

    return 0;
}
