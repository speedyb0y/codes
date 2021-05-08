/*
    gcc -Wall -Wextra -Werror -fwhole-program -O2 ra.c -o ra
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
#include <net/if.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define loop while(1)
#define elif(c) else if (c)

typedef unsigned int uint;
typedef unsigned long int uintl;
typedef unsigned long long int uintll;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define clear(addr, size) memset(addr, 0, size)
#define copy(src, size, dst) memcpy(dst, src, size)

#define MSGS_N 64

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

//system(cmd);
#define IP(fmt, ...) ({ char cmd[512]; snprintf(cmd, sizeof(cmd), "ip " fmt, ##__VA_ARGS__); printf(" -> %s\n", cmd); })

typedef struct Link Link;

struct Link {
    char* itfc;
    char* gwIP;
    u16 table;
    u8 gwMAC[6];
    u8 prefix[16];
    u32 mtu;
    u32 prefixLen;
};

#define OPTION_GW_MAC              0x01U
#define OPTION_PREFIX_INFORMATION  0x03U
#define OPTION_MTU                 0x05U

int main (int argsN, char** args) {

    if (argsN % 4 != 1) {
        printf("USAGE: ra ITFC TABLE GW_IP GW_MAC ... \n");
        return 1;
    }

    const uint linksN = (argsN - 1) / 4;

    Link links[16];

    args++;

    for (uint i = 0; i != linksN; i++) {

        const int table = atoi(args[1]);

        if (table < 1 ||
            table > 32000) {
            printf("BAD TABLE %s\n", args[1]);
            return 1;
        }

        if (strlen(args[3]) != 17) {
            printf("BAD GW MAC %s\n", args[3]);
            return 1;
        }

        args[3][ 2] = '\x00';
        args[3][ 5] = '\x00';
        args[3][ 8] = '\x00';
        args[3][11] = '\x00';
        args[3][14] = '\x00';

        links[i].itfc = args[0];
        links[i].table = table;
        links[i].gwIP = args[2];
        links[i].gwMAC[0] = strtoul(args[3] +  0, NULL, 16);
        links[i].gwMAC[1] = strtoul(args[3] +  3, NULL, 16);
        links[i].gwMAC[2] = strtoul(args[3] +  6, NULL, 16);
        links[i].gwMAC[3] = strtoul(args[3] +  9, NULL, 16);
        links[i].gwMAC[4] = strtoul(args[3] + 12, NULL, 16);
        links[i].gwMAC[5] = strtoul(args[3] + 15, NULL, 16);
        links[i].prefixLen = 0;
        links[i].prefix[0] = 0;
        links[i].mtu = 0;

        args += 4;
    }

    const int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    if (sock == -1) {
        printf("ERROR: FAILED TO OPEN SOCKET: %s\n", strerror(errno));
        return 1;
    }

    u8 ipGenerated[16]; getrandom(ipGenerated, sizeof(ipGenerated), 0);

    ((u64*)ipGenerated)[0] += rdtsc()    + 0xAABBCCDD00112233ULL;
    ((u64*)ipGenerated)[1] += time(NULL) + 0x4455667700119988ULL;

    loop {

        struct mmsghdr msgs[MSGS_N];
        struct iovec iovs[MSGS_N];
        u8 buffs[MSGS_N][512];

        for (uint i = 0; i != MSGS_N; i++) {
            msgs[i].msg_hdr.msg_name = NULL;
            msgs[i].msg_hdr.msg_namelen = 0;
            msgs[i].msg_hdr.msg_iov = &iovs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
            msgs[i].msg_hdr.msg_control = NULL;
            msgs[i].msg_hdr.msg_controllen = 0;
            msgs[i].msg_hdr.msg_flags = 0;
            iovs[i].iov_base = &buffs[i];
            iovs[i].iov_len = sizeof(buffs[i]);
        }

        const int msgsN = recvmmsg(sock, msgs, MSGS_N, MSG_WAITFORONE, NULL);

        if (msgsN == -1) {
            if (errno != EINTR)
                return 1;
            continue;
        }

        for (int i = 0; i != msgsN; i++) {

            const void* const msg = buffs[i]; uint msgSize = msgs[i].msg_len;

            if (*(u8*)msg == 0x86) {
                // ROUTER ADVERTISEMENT

                if (0)
                    printf("ADVERTISEMENT!!!\n");

                int ok = 1;
                const u8* mac = NULL;
                const u8* prefix = NULL;
                uint mtu = 0;
                uint prefixLen = 0;
                uint prefixFlags = 0;
                uint prefixValidLT = 0;
                uint prefixPreferredLT = 0;
                uint prefixReserved = 0;

                const void* option = msg + 16; msgSize -= 16;

                while (msgSize) {

                    if (msgSize > 2048) { // OVERFLOW
                        printf("ERROR: LEU DEMAIS\n");
                        ok = 0;
                        break;
                    }

                    if (msgSize < 2) {
                        printf("OPTION SIZE < 2!!!\n");
                        ok = 0;
                        break;
                    }

                    const uint optionCode = *(u8*)option;
                    const uint optionSize = *(u8*)(option + 1) * 8;
                    const void* const optionValue = option + 2;

                    if ((msg + msgSize) < (option + optionSize)) {
                        printf("OPTION INCOMPLETE!!!\n");
                        ok = 0;
                        break;
                    }

                    switch (optionCode) {
                        case OPTION_PREFIX_INFORMATION: // TODO: FIXME: VALIDAR optionSize
                            // TYPE: PREFIX INFORMATION
                            prefixLen = *(u8*)optionValue;
                            prefixFlags = *(u8*)(optionValue + 1);
                            prefixValidLT = ntohl(*(u32*)(optionValue + 2));
                            prefixPreferredLT = ntohl(*(u32*)(optionValue + 6));
                            prefixReserved = *(u32*)(optionValue + 10);
                            prefix = optionValue + 14;
                            if (prefixLen < 32 ||
                                prefixLen > 90) {
                                printf("INVALID PREFIX SIZE\n");
                                ok = 0;
                            }
                            break;
                        case OPTION_MTU:
                            //mtuReserved = *(u16*)optionValue;
                            mtu = ntohl(*(u32*)(optionValue + 2));
                            break;
                        case OPTION_GW_MAC:
                            mac = optionValue;
                            break;
                        default:
                            if(0)
                                printf("OPTION 0x%02X - SIZE %u\n", optionCode, optionSize);
                    }

                    option += optionSize;
                    msgSize -= optionSize;
                }

                if (!mac) {
                    printf("MISSING MAC\n");
                    ok = 0;
                }

                if (!prefix) {
                    printf("MISSING PREFIX\n");
                    ok = 0;
                }

                if (ok) {
                    // AGORA LIDÁ COM A MENSAGEM QUE FOI CARREGADA

                    if (1) {
                        printf("OPTION PREFIX INFORMATION - FLAGS %02X VALIDLT %u PREFERREDLT %u RESERVED %u\n", prefixFlags, prefixValidLT, prefixPreferredLT, prefixReserved);
                        printf("OPTION GW MAC - %02X:%02X:%02X:%02X:%02X:%02X\n",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                        printf("OPTION MTU - MTU %u\n", mtu);
                    }

                    int unhandled = 1;

                    for (uint linkID = 0; linkID != linksN; linkID++) { Link* const link = &links[linkID];
                        if (memcmp(link->gwMAC, mac, 6))
                            continue;
                        unhandled = 0;
                        if (memcmp(link->prefix, prefix, 16) || link->prefixLen != prefixLen) {
                            // É DESTE LINK, E ELE MUDOU

                            // PASSA A USAR ELE
                            memcpy(link->prefix, prefix, 16); link->prefixLen = prefixLen;

                            //
                            char prefix[64]; char ip[64];

                            snprintf(prefix, sizeof(prefix), "%X:%X:%X:%X:%X:%X:%X:%X/%u",
                                ntohs(((u16*)link->prefix)[0]),
                                ntohs(((u16*)link->prefix)[1]),
                                ntohs(((u16*)link->prefix)[2]),
                                ntohs(((u16*)link->prefix)[3]),
                                ntohs(((u16*)link->prefix)[4]),
                                ntohs(((u16*)link->prefix)[5]),
                                ntohs(((u16*)link->prefix)[6]),
                                ntohs(((u16*)link->prefix)[7]),
                                (uint)link->prefixLen);

                            printf("PREFIX %s\n", prefix);

                            IP("-6 addr flush dev %s", link->itfc);

                            //
                            ((u64*)ipGenerated)[0] +=  ((u64*)ipGenerated)[1] + time(NULL);
                            ((u64*)ipGenerated)[1] += ~((u64*)ipGenerated)[0] + rdtsc();

                            for (uint i = 0; i != 32; i++) {

                                u64 r[2]; getrandom(r, sizeof(r), 0);

                                ((u64*)ipGenerated)[0] += r[0] + 0x12E4A91CULL;
                                ((u64*)ipGenerated)[1] += r[1] + 0x3402AAACULL;

                                // OVERWRITE O PREFIXO
                                uint remaining = prefixLen;
                                uint offset = 0;

                                while (remaining) {
                                    const uint amount = (remaining < 8) ? remaining : 8;
                                    const uint mask = (0xFFU << (8 - amount)) & 0xFFU;
                                    ipGenerated[offset] &= ~mask;
                                    ipGenerated[offset] |= link->prefix[offset] & mask;
                                    offset++;
                                    remaining -= amount;
                                }

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

                                const uint table = link->table + i;

                                IP("-6 addr add dev %s %s", link->itfc, ip);
                                IP("-6 route flush table %u", table);
                                IP("-6 route add table %u src %s dev %s %s", table, ip, link->itfc, link->gwIP);
                                IP("-6 route add table %u src %s dev %s default via %s", table, ip, link->itfc, link->gwIP);
                            }

                            system("ip -6 route flush cache");
                        }
                    }

                    if (unhandled)
                        printf("UNKNOWN RA\n");
                }
            }
        }
    }
}
