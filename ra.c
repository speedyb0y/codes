/*
    gcc -Wall -Wextra -Werror -fwhole-program -O2 ra.c -o ra
*/

#define _GNU_SOURCE 1

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/if_packet.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <netinet/icmp6.h>

#define loop while(1)
#define elif(c) else if (c)

typedef intptr_t intp;
typedef uintptr_t uintp;

typedef unsigned int uint;
typedef unsigned long int uintl;
typedef unsigned long long int uintll;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct ifreq IfReq;
typedef struct sockaddr SockAddr;
typedef struct sockaddr_ll SockAddrLL;
typedef struct sockaddr_in6 SockAddrIP6;

#define clear(addr, size) memset(addr, 0, size)
#define copy(src, size, dst) memcpy(dst, src, size)

#define MSGS_N 64

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

int main (int argsN, char* args[]) {

    if (argsN != 2)
        return 1;

    char* const itfc = args[1];

    const int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    if (sock == -1) {
        printf("ERROR: FAILED TO OPEN SOCKET: %s\n", strerror(errno));
        return 1;
    }

    struct ifreq ifr = { 0 };

    strncpy(ifr.ifr_name, itfc, IFNAMSIZ - 1);

    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(struct ifreq)) == -1) {
        printf("ERROR: FAILED TO BIND SOCKET TO INTERFACE: %s\n", strerror(errno));
        return 1;
    }

    u8 prefixCurrent[16] = { 0 }; uint prefixLenCurrent = 0;

    u8 ipCurrent[16];

    ((u64*)ipCurrent)[0] = rdtsc() + time(NULL) + 0xAABBCCDD00112233ULL;
    ((u64*)ipCurrent)[1] = rdtsc() + time(NULL) + 0x4455667700119988ULL;

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

        const u8* prefixNew = prefixCurrent; uint prefixLenNew = prefixLenCurrent;

        for (int i = 0; i != msgsN; i++) {

            const void* const msg = buffs[i]; const uint msgSize = msgs[i].msg_len;

            if (*(u8*)msg == 0x86) {
                // ROUTER ADVERTISEMENT
                //printf("ADVERTISEMENT!!!\n");
                const void* option = msg + 16;

                while (option < (msg + msgSize)) {

                    const uint optionCode = *(u8*)option;
                    const uint optionSize = *(u8*)(option + 1) * 8;

                    //printf("OPTION 0x%02X SIZE %u!!!\n", optionCode,optionSize);
                    if (optionSize == 0) {
                        // INVALID
                        //printf("OPTION SIZE 0!!!\n");
                        break;
                    }
                    if ((msg + msgSize) < (option + optionSize)) {
                        // INCOMPLETE
                        //printf("OPTION INCOMPLETE!!!\n");
                        break;
                    }
                    if (optionCode == 0x03) { // TODO: FIXME: VALIDAR optionSize
                        // TYPE: PREFIX INFORMATION
                        const uint prefixLen = *(u8*)(option + 2);
                        const u8* const prefix = option + 16;
                        if (prefixLen < 128) {
                            prefixLenNew = prefixLen;
                            prefixNew = prefix;
                        } else {
                            // INVALID
                        }
                    }
                    option += optionSize;
                }
            }
        }

        if (memcmp(prefixCurrent, prefixNew, 16) || prefixLenCurrent != prefixLenNew) {

            printf("PREFIX %X:%X:%X:%X:%X:%X:%X:%X/%u\n",
                ntohs(((u16*)prefixNew)[0]),
                ntohs(((u16*)prefixNew)[1]),
                ntohs(((u16*)prefixNew)[2]),
                ntohs(((u16*)prefixNew)[3]),
                ntohs(((u16*)prefixNew)[4]),
                ntohs(((u16*)prefixNew)[5]),
                ntohs(((u16*)prefixNew)[6]),
                ntohs(((u16*)prefixNew)[7]),
                prefixLenNew);

            // PASSA A USAR ELE
            memcpy(prefixCurrent, prefixNew, 16);
            prefixLenCurrent = prefixLenNew;

            //
            ((u64*)ipCurrent)[0] += rdtsc() + ((u64)time(NULL) << 32) + 0x5E3621AEF0A1946ULL;
            ((u64*)ipCurrent)[1] += rdtsc() + ((u64)time(NULL) << 32) + 0x3EC4A114293E561ULL;

            // OVERWRITE O PREFIXO
            uint remaining = prefixLenNew;
            uint offset = 0;

            while (remaining) {
                const uint amount = (remaining < 8) ? remaining : 8;
                const uint mask = (0xFFU << (8 - amount)) & 0xFFU;
                ipCurrent[offset] &= ~mask;
                ipCurrent[offset] |= prefixCurrent[offset] & mask;
                offset++;
                remaining -= amount;
            }

            char ip[64]; char cmd[256];

            snprintf(ip, sizeof(ip), "%X:%X:%X:%X:%X:%X:%X:%X",
                ntohs(((u16*)ipCurrent)[0]),
                ntohs(((u16*)ipCurrent)[1]),
                ntohs(((u16*)ipCurrent)[2]),
                ntohs(((u16*)ipCurrent)[3]),
                ntohs(((u16*)ipCurrent)[4]),
                ntohs(((u16*)ipCurrent)[5]),
                ntohs(((u16*)ipCurrent)[6]),
                ntohs(((u16*)ipCurrent)[7])
                );

            printf("IP %s\n", ip);

            snprintf(cmd, sizeof(cmd), "ip -6 addr flush dev %s", itfc);      system(cmd);
            snprintf(cmd, sizeof(cmd), "ip -6 addr add dev %s %s", itfc, ip); system(cmd);
        }
    }
}
