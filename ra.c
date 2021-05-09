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
#include <sys/ioctl.h>
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

#define OPTION_GW_MAC              0x01U
#define OPTION_PREFIX_INFORMATION  0x03U
#define OPTION_MTU                 0x05U

#define IPV6_ADDR_SIZE 16
#define IPV6_ADDR_STR_SIZE   128
#define IPV6_PREFIX_STR_SIZE 164

#define MAC_SIZE 6
#define MAC_STR_SIZE 17

#define MSGS_N 64

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

#define IP(fmt, ...) ({ char cmd[512]; snprintf(cmd, sizeof(cmd), "true ip " fmt, ##__VA_ARGS__); printf(" -> %s\n", cmd); system(cmd); })

typedef struct Link Link;

struct Link {
    const char* itfc;
    const char* gwIP;
    u8 gwMAC[MAC_SIZE]; // 6
    u16 prefixLen;
    u32 table;
    u32 mtu;
    u32 addrsN;
    u32 mark;
    u32 ruleMark;
    u32 ruleFrom;
    u8 prefix[IPV6_ADDR_SIZE];
    void* addrs;
};

static void ip6_to_str (const u8* const restrict prefix, char* const restrict prefixStr) {
#if 0
    snprintf(prefixStr, IPV6_ADDR_STR_SIZE, "%X:%X:%X:%X:%X:%X:%X:%X",
        ntohs(((u16*)prefix)[0]),
        ntohs(((u16*)prefix)[1]),
        ntohs(((u16*)prefix)[2]),
        ntohs(((u16*)prefix)[3]),
        ntohs(((u16*)prefix)[4]),
        ntohs(((u16*)prefix)[5]),
        ntohs(((u16*)prefix)[6]),
        ntohs(((u16*)prefix)[7])
        );
#else
        if (inet_ntop(AF_INET6, prefix, prefixStr, IPV6_ADDR_STR_SIZE) != prefixStr)
            *prefixStr = '\x00';
#endif
}

static void prefix6_to_str (const u8* const restrict prefix, const uint prefixLen, char* const restrict prefixStr) {

    char prefixStr_[IPV6_ADDR_STR_SIZE];

    ip6_to_str(prefix, prefixStr_);

    snprintf(prefixStr, IPV6_PREFIX_STR_SIZE, "%s/%u", prefixStr_, prefixLen);
}

static u8 ip6Random[IPV6_ADDR_SIZE];

static void ip6_random_init (void) {

    getrandom(ip6Random, sizeof(ip6Random), 0);

    ((u64*)ip6Random)[0] += rdtsc()    + 0xAABBCCDD00112233ULL;
    ((u64*)ip6Random)[1] += time(NULL) + 0x4455667700119988ULL;
}

static void ip6_random_update (const u64 feed) {

    ((u64*)ip6Random)[1] += feed + time(NULL);
    ((u64*)ip6Random)[0] += feed + rdtsc();
}

static void ip6_random_gen (void* const ip) {

    u8 r[IPV6_ADDR_SIZE]; getrandom(r, sizeof(r), 0);

    ((u64*)ip)[0] +=
    ((u64*)ip)[1];

    ((u64*)ip)[0] += 0x12E4A91CULL;
    ((u64*)ip)[1] += 0x3402AAACULL;

    ((u64*)ip)[0] += ((u64*)r)[0];
    ((u64*)ip)[1] += ((u64*)r)[1];

    ((u64*)ip)[0] += ((u64*)ip6Random)[0];
    ((u64*)ip)[1] += ((u64*)ip6Random)[1];
}

static void ip6_prefix (u8* const restrict ip, const u8* const restrict prefix, const uint prefixLen) {

    // OVERWRITE O PREFIXO
    uint remaining = prefixLen;
    uint offset = 0;

    while (remaining) {
        const uint amount = (remaining < 8) ? remaining : 8;
        const uint mask = (0xFFU << (8 - amount)) & 0xFFU;
        ip[offset] &= ~mask;
        ip[offset] |= prefix[offset] & mask;
        offset++;
        remaining -= amount;
    }
}

int main (int argsN, char** args) {

    if (argsN % 8 != 1) {
        printf("USAGE: ra ITFC GW_IP GW_MAC TABLE MARK RULE_MARK RULE_FROM ADDRS_N ... \n");
        return 1;
    }

    const int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    if (sock == -1) {
        printf("ERROR: FAILED TO OPEN SOCKET: %s\n", strerror(errno));
        return 1;
    }

    const uint linksN = (argsN - 1) / 8;

    Link links[16];

    args++;

    for (uint i = 0; i != linksN; i++) {

        const char* const _itfc     = args[0];
        const char* const _gwIP     = args[1];
              char* const _gwMAC    = args[2];
        const char* const _table    = args[3];
        const char* const _mark     = args[4];
        const char* const _ruleMark = args[5];
        const char* const _ruleFrom = args[6];
        const char* const _addrsN   = args[7]; args += 8;

        const uint table    = atoi(_table);
        const uint mark     = atoi(_mark);
        const uint ruleMark = atoi(_ruleMark);
        const uint ruleFrom = atoi(_ruleFrom);
        const uint addrsN   = atoi(_addrsN);

        struct ifreq ifr = { 0 }; strncpy(ifr.ifr_name, _itfc, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
            printf("BAD INTERFACE %s\n", _itfc);
            return 1;
        }

       //short           ifr_flags;
       //int             ifr_ifindex;
       //int             ifr_metric;
       //int             ifr_mtu;
       //struct ifmap    ifr_map;

        if (table < 1 ||
            table > 32000) {
            printf("BAD TABLE %s\n", _table);
            return 1;
        }

        if (ruleMark < 1 ||
            ruleMark > 32000) {
            printf("BAD RULE MARK %s\n", _ruleMark);
            return 1;
        }

        if (ruleFrom < 1 ||
            ruleFrom > 32000) {
            printf("BAD RULE FROM %s\n", _ruleFrom);
            return 1;
        }

        if (mark < 1 ||
            mark > 1000) {
            printf("BAD MARK %s\n", _mark);
            return 1;
        }

        if (addrsN < 1 ||
            addrsN > 1000) {
            printf("BAD ADDRS N %s\n", _addrsN);
            return 1;
        }

        if (strlen(_gwMAC) != MAC_STR_SIZE) {
            printf("BAD GW MAC %s\n", _gwMAC);
            return 1;
        }

        _gwMAC[ 2] = '\x00';
        _gwMAC[ 5] = '\x00';
        _gwMAC[ 8] = '\x00';
        _gwMAC[11] = '\x00';
        _gwMAC[14] = '\x00';

        Link* const link = &links[i];

        link->itfc = _itfc;
        link->mtu = 0;
        link->table = table;
        link->mark = mark;
        link->ruleMark = ruleMark;
        link->ruleFrom = ruleFrom;
        link->prefixLen = 0;
        link->prefix[0] = 0;
        link->gwIP = _gwIP;
        link->gwMAC[0] = strtoul(_gwMAC +  0, NULL, 16);
        link->gwMAC[1] = strtoul(_gwMAC +  3, NULL, 16);
        link->gwMAC[2] = strtoul(_gwMAC +  6, NULL, 16);
        link->gwMAC[3] = strtoul(_gwMAC +  9, NULL, 16);
        link->gwMAC[4] = strtoul(_gwMAC + 12, NULL, 16);
        link->gwMAC[5] = strtoul(_gwMAC + 15, NULL, 16);
        link->addrsN = addrsN;
        link->addrs = malloc(sizeof(IPV6_ADDR_SIZE)*addrsN);

        memset(link->addrs, 0, sizeof(IPV6_ADDR_SIZE)*addrsN);
    }

    ip6_random_init();

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

        ip6_random_update(msgsN);

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

                    if (msgSize < optionSize) {
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

                    if (1)
                        printf("GW MAC %02X:%02X:%02X:%02X:%02X:%02X MTU %u PREFIX FLAGS 0x%02X VALIDLT %u PREFERREDLT %u PREFIX RESERVED 0x%08X\n",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mtu,
                            prefixFlags, prefixValidLT, prefixPreferredLT, prefixReserved
                            );

                    int unhandled = 1;

                    for (uint linkID = 0; linkID != linksN; linkID++) { Link* const link = &links[linkID];

                        if (memcmp(link->gwMAC, mac, 6))
                            continue;

                        unhandled = 0;

                        if (memcmp(link->prefix, prefix, IPV6_ADDR_SIZE) || link->prefixLen != prefixLen) {
                            // É DESTE LINK, E ELE MUDOU

                            char prefixStr    [IPV6_PREFIX_STR_SIZE]; prefix6_to_str(      prefix,       prefixLen,     prefixStr);
                            char linkPrefixStr[IPV6_PREFIX_STR_SIZE]; prefix6_to_str(link->prefix, link->prefixLen, linkPrefixStr);

                            printf("LINK #%u ITFC %s PREFIX CHANGED %s -> %s\n", linkID, link->itfc, linkPrefixStr, prefixStr);

                            for (uint i = 0; i != link->addrsN; i++) {

                                char ip[64];

                                //ip6_to_str(ip, ipGenerated);
                                const uint table    = link->table    + i;
                                const uint mark     = link->mark     + i;
                                const uint ruleMark = link->ruleMark + i;
                                const uint ruleFrom = link->ruleFrom + i;
                                u8* const addr      = link->addrs    + i*IPV6_ADDR_SIZE;

                                IP("-6 rule flush priority %u", ruleMark);
                                IP("-6 rule flush priority %u", ruleFrom);
                                IP("-6 route flush table %u", table);
                                IP("-6 addr del dev %s OLD_IP/128", link->itfc);

                                ip6_random_gen(addr);
                                ip6_prefix(addr, prefix, prefixLen);
                                ip6_to_str(addr, ip);

                                IP("-6 addr add dev %s %s/128 nodad", link->itfc, ip);
                                IP("-6 route add table %u src %s dev %s %s", table, ip, link->itfc, link->gwIP);
                                IP("-6 route add table %u src %s dev %s default via %s", table, ip, link->itfc, link->gwIP);
                                IP("-6 rule add priority %u table %u fwmark %u", ruleMark, table, mark);
                                IP("-6 rule add priority %u table %u from %s", ruleFrom, table, ip);
                            }

                            IP("-6 route flush cache");

                            // PASSA A USAR ELE
                            memcpy(link->prefix, prefix, IPV6_ADDR_SIZE); link->prefixLen = prefixLen;
                        }
                    }

                    if (unhandled)
                        printf("UNKNOWN RA\n");
                }
            }
        }
    }
}
