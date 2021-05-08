/*
    gcc -Wall -Wextra -Werror -fwhole-program -O2 ip6.c -o ip6
*/

#define _GNU_SOURCE 1

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/random.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

typedef unsigned int uint;
typedef unsigned long int uintl;
typedef unsigned long long int uintll;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define IPV6_ADDR_SIZE 16
#define MAC_SIZE 6
#define MAC_STR_SIZE 17

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

#define IP(fmt, ...) ({ char cmd[512]; snprintf(cmd, sizeof(cmd), "ip " fmt, ##__VA_ARGS__); printf(" -> %s\n", cmd); system(cmd); })

int main (int argsN, char** args) {

    if (argsN % 7 != 1) {
        printf("USAGE: ip6 ITFC TABLE GW_IP GW_MAC ADDRS_N PREFIX PREFIX_LEN ... \n");
        return 1;
    }

    const int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    if (sock == -1) {
        printf("ERROR: FAILED TO OPEN SOCKET: %s\n", strerror(errno));
        return 1;
    }

    u8 ipGenerated[IPV6_ADDR_SIZE]; getrandom(ipGenerated, sizeof(ipGenerated), 0);

    ((u64*)ipGenerated)[0] += rdtsc()    + 0xAABBCCDD00112233ULL;
    ((u64*)ipGenerated)[1] += time(NULL) + 0x4455667700119988ULL;

    const uint linksN = (argsN - 1) / 7;

    args++;

    for (uint i = 0; i != linksN; i++) {

        const char* const _itfc   = args[0];
        const char* const _table  = args[1];
        const char* const _gwIP   = args[2];
              char* const _gwMAC  = args[3];
        const char* const _addrsN = args[4];
        const char* const _prefix = args[5];
        const char* const _pLen   = args[6]; args += 7;

        uint table = atoi(_table);
        const uint addrsN = atoi(_addrsN);

        struct ifreq ifr = { 0 }; strncpy(ifr.ifr_name, _itfc, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
            printf("BAD INTERFACE %s\n", _itfc);
            return 1;
        }

        if (table == 0 ||
            table > 32000) {
            printf("BAD TABLE %s\n", _table);
            return 1;
        }

        if (addrsN == 0 ||
            addrsN > 1000) {
            printf("BAD TABLE %s\n", _addrsN);
            return 1;
        }

        if (strlen(_gwMAC) != MAC_STR_SIZE) {
            printf("BAD GW MAC %s\n", _gwMAC);
            return 1;
        }

        const char* const itfc = _itfc;

        const uint prefixLen = atoi(_pLen);

        const uint mtu = 0;

        const char* const gwIP = _gwIP;

        _gwMAC[ 2] = '\x00';
        _gwMAC[ 5] = '\x00';
        _gwMAC[ 8] = '\x00';
        _gwMAC[11] = '\x00';
        _gwMAC[14] = '\x00';

        const u8 gwMAC[MAC_SIZE] = {
            strtoul(_gwMAC +  0, NULL, 16),
            strtoul(_gwMAC +  3, NULL, 16),
            strtoul(_gwMAC +  6, NULL, 16),
            strtoul(_gwMAC +  9, NULL, 16),
            strtoul(_gwMAC + 12, NULL, 16),
            strtoul(_gwMAC + 15, NULL, 16),
            };  (void)gwMAC;

        u8 prefix[IPV6_ADDR_SIZE];

        if (inet_pton(AF_INET6, _prefix, &prefix) != 1) {
            printf("BAD PREFIX %s\n", _prefix);
            return 1;
        }

        //uint prefixValidLT = 0;
        //uint prefixPreferredLT = 0;

        (void)mtu;

        IP("-6 addr flush dev %s", itfc);

        //
        ((u64*)ipGenerated)[0] +=  ((u64*)ipGenerated)[1] + time(NULL);
        ((u64*)ipGenerated)[1] += ~((u64*)ipGenerated)[0] + rdtsc();

        for (uint i = 0; i != addrsN; i++) {

            u8 r[IPV6_ADDR_SIZE]; getrandom(r, sizeof(r), 0);

            ((u64*)ipGenerated)[0] +=
            ((u64*)ipGenerated)[1];

            ((u64*)ipGenerated)[0] += ((u64*)r)[0] + 0x12E4A91CULL;
            ((u64*)ipGenerated)[1] += ((u64*)r)[1] + 0x3402AAACULL;

            // OVERWRITE O PREFIXO
            uint remaining = prefixLen;
            uint offset = 0;

            while (remaining) {
                const uint amount = (remaining < 8) ? remaining : 8;
                const uint mask = (0xFFU << (8 - amount)) & 0xFFU;
                ipGenerated[offset] &= ~mask;
                ipGenerated[offset] |= prefix[offset] & mask;
                offset++;
                remaining -= amount;
            }

            char ip[64];

            snprintf(ip, sizeof(ip), "%X:%X:%X:%X:%X:%X:%X:%X",
                ntohs(((u16*)ipGenerated)[0]),
                ntohs(((u16*)ipGenerated)[1]),
                ntohs(((u16*)ipGenerated)[2]),
                ntohs(((u16*)ipGenerated)[3]),
                ntohs(((u16*)ipGenerated)[4]),
                ntohs(((u16*)ipGenerated)[5]),
                ntohs(((u16*)ipGenerated)[6]),
                ntohs(((u16*)ipGenerated)[7])
                );

            IP("-6 addr add dev %s %s", itfc, ip);
            IP("-6 route flush table %u", table);
            IP("-6 route add table %u src %s dev %s %s", table, ip, itfc, gwIP);
            IP("-6 route add table %u src %s dev %s default via %s", table, ip, itfc, gwIP);

            table++;
        }

        system("ip -6 route flush cache");
    }

    return 0;
}
