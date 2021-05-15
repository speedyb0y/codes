/*
    gcc -Wall -Wextra -Werror -fwhole-program -O2 ra.c -o ra

    TODO: FIXME: APÓS CERTO TEMPO SEM RECEBER O RA, DELETAR E DESMARCAR
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
#include <signal.h>
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

#define MSGS_N 512

static inline u64 rdtsc (void) {
    uint lo;
    uint hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

#define IP6(fmt, ...) ({ \
    char cmd[512]; \
    snprintf(cmd, sizeof(cmd), "ip -6 " fmt, ##__VA_ARGS__); \
    printf(" -> %s\n", cmd); \
    fflush(stdout); \
    fflush(stderr); \
    system(cmd); \
    fflush(stdout); \
    fflush(stderr); \
    })

typedef struct Link Link;

struct Link {
    const char* itfc;
    const char* itfcOut;
    const char* gwIP;
    const char* gwOutIP;
    u8 gwMAC[MAC_SIZE]; // 6
    u16 prefixLen;
    u32 mtu;
    u32 addrsN;
    u8 prefix[IPV6_ADDR_SIZE];
    u8* addrs;
};

static void ip6_to_str (const u8* const restrict ip, char* const restrict ipStr) {
#if 0
    snprintf(ipStr, IPV6_ADDR_STR_SIZE, "%X:%X:%X:%X:%X:%X:%X:%X",
        ntohs(((u16*)ip)[0]),
        ntohs(((u16*)ip)[1]),
        ntohs(((u16*)ip)[2]),
        ntohs(((u16*)ip)[3]),
        ntohs(((u16*)ip)[4]),
        ntohs(((u16*)ip)[5]),
        ntohs(((u16*)ip)[6]),
        ntohs(((u16*)ip)[7])
        );
#else
        if (inet_ntop(AF_INET6, ip, ipStr, IPV6_ADDR_STR_SIZE) != ipStr)
            *ipStr = '\x00';
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

// OVERWRITE O IP COM O PREFIXO
static void ip6_prefix (u8* restrict ip, const u8* restrict prefix, uint prefixLen) {

    while (prefixLen) {
        const uint amount = (prefixLen < 8) ? prefixLen : 8;
        const uint mask = (0xFFU << (8 - amount)) & 0xFFU;
        *ip &= ~mask;
        *ip++ |= *prefix++ & mask;
        prefixLen -= amount;
    }
}

static volatile sig_atomic_t sigTERM;

static void signal_handler (int sig) {

    switch (sig) {
        case SIGINT:
        case SIGTERM:
            sigTERM = 1;
    }
}

int main (int argsN, char** args) {

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

    args++;
    argsN--;

    if (argsN % 6 != 1) {
        printf("USAGE: ra METRIC0 ITFC GW_MAC GW_IP ADDRS_N ITFC_OUT GW_OUT ... \n");
        return 1;
    }

    const char* const _ruleFrom = *args++; argsN--;

    const uint ruleFrom = atoi(_ruleFrom);

    if (ruleFrom < 1 ||
        ruleFrom > 32000) {
        printf("BAD RULE FROM %s\n", _ruleFrom);
        return 1;
    }

    const int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6); //, IPPROTO_ICMPV6

    if (sock == -1) {
        printf("ERROR: FAILED TO OPEN SOCKET: %s\n", strerror(errno));
        return 1;
    }

    const uint linksN = argsN / 6;

    Link links[16];

    for (uint i = 0; i != linksN; i++) {

        const char* const _itfc     = args[0];
              char* const _gwMAC    = args[1];
        const char* const _gwIP     = args[2];
        const char* const _addrsN   = args[3];
        const char* const _outItfc  = args[4];
        const char* const _outGW  = args[5]; args += 6;

        const uint addrsN   = atoi(_addrsN);

        { struct ifreq ifr = { 0 }; strncpy(ifr.ifr_name, _itfc, IFNAMSIZ - 1);

            if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
                printf("BAD INTERFACE %s\n", _itfc);
                return 1;
            }
        }

        { struct ifreq ifr = { 0 }; strncpy(ifr.ifr_name, _outItfc, IFNAMSIZ - 1);

            if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
                printf("BAD INTERFACE OUT %s\n", _outItfc);
                return 1;
            }
        }

       //short           ifr_flags;
       //int             ifr_ifindex;
       //int             ifr_metric;
       //int             ifr_mtu;
       //struct ifmap    ifr_map;

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

        link->itfc      = _itfc;
        link->itfcOut   = _outItfc;
        link->mtu       = 0;
        link->prefixLen = 0;
        link->prefix[0] = 0;
        link->gwOutIP   = _outGW;
        link->gwIP      = _gwIP;
        link->gwMAC[0]  = strtoul(_gwMAC +  0, NULL, 16);
        link->gwMAC[1]  = strtoul(_gwMAC +  3, NULL, 16);
        link->gwMAC[2]  = strtoul(_gwMAC +  6, NULL, 16);
        link->gwMAC[3]  = strtoul(_gwMAC +  9, NULL, 16);
        link->gwMAC[4]  = strtoul(_gwMAC + 12, NULL, 16);
        link->gwMAC[5]  = strtoul(_gwMAC + 15, NULL, 16);
        link->addrsN    = addrsN;
        link->addrs     = malloc(addrsN * IPV6_ADDR_SIZE);

        memset(link->addrs, 0, addrsN * IPV6_ADDR_SIZE);
    }

    ip6_random_init();

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

    while (!sigTERM) {

        const int msgsN = recvmmsg(sock, msgs, MSGS_N, MSG_WAITFORONE, NULL);

        if (msgsN == -1) {
            if (errno != EINTR)
                break;
            continue;
        }

        ip6_random_update(msgsN);

        for (int i = 0; i != msgsN; i++) {

            const void* const msg = buffs[i]; uint msgSize = msgs[i].msg_len;

            if (*(u8*)msg == 0x86) {
                // ROUTER ADVERTISEMENT

                if (0)
                    printf("ADVERTISEMENT!!!\n");

                const u8* mac = NULL;
                const u8* prefix = NULL;
                uint mtu = 0;
                uint prefixLen = 0;
                uint prefixFlags = 0;
                uint prefixValidLT = 0;
                uint prefixPreferredLT = 0;

                const void* option = msg + 16; msgSize -= 16;

                while (msgSize) {

                    if (msgSize > 2048)
                        break; // OVERFLOW - MESSAGE INCOMPLETE

                    if (msgSize < 2)
                        break; // LESS THAN A MINIMAL OPTION SIZE

                    const uint optionCode = *(u8*)option;
                    const uint optionSize = *(u8*)(option + 1) * 8;
                    const void* const optionValue = option + 2;

                    if (msgSize < optionSize)
                        break; // OPTION INCOMPLETE

                    switch (optionCode) {
                        case OPTION_PREFIX_INFORMATION: // TODO: FIXME: VALIDAR optionSize
                            prefixLen = *(u8*)optionValue;
                            prefixFlags = *(u8*)(optionValue + 1);
                            prefixValidLT = ntohl(*(u32*)(optionValue + 2));
                            prefixPreferredLT = ntohl(*(u32*)(optionValue + 6));
                            prefix = optionValue + 14; // OBS.: tem também um u32 RESERVED
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

                if (msgSize) {
                    printf("MESSAGE INCOMPLETE\n");
                } elif (prefixLen < 32 ||
                        prefixLen > 90) {
                    printf("INVALID PREFIX SIZE\n");
                } elif (!mac) {
                    printf("MISSING MAC\n");
                } elif (!prefix) {
                    printf("MISSING PREFIX\n");
                } else {

                    char prefixStr[IPV6_PREFIX_STR_SIZE]; prefix6_to_str(prefix, prefixLen, prefixStr);

                    uint linkID = 0;

                    while (linkID != linksN && memcmp(links[linkID].gwMAC, mac, MAC_SIZE))
                        linkID++;

                    if (linkID != linksN) {

                        Link* const link = &links[linkID];

                        if (memcmp(link->prefix, prefix, IPV6_ADDR_SIZE) || link->prefixLen != prefixLen) {

                            // É DESTE LINK, E ELE MUDOU
                            char linkPrefixStr[IPV6_PREFIX_STR_SIZE]; prefix6_to_str(link->prefix, link->prefixLen, linkPrefixStr);

                            printf("GW MAC %02X:%02X:%02X:%02X:%02X:%02X MTU %u PREFIX %s FLAGS 0x%02X VALIDLT %u PREFERREDLT %u | "
                                "LINK #%u ITFC %s CHANGED FROM %s\n",
                                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mtu, prefixStr,
                                prefixFlags, prefixValidLT, prefixPreferredLT, linkID, link->itfc, linkPrefixStr
                                );

                            for (uint i = 0; i != link->addrsN; i++) {

                                u8* const addr = link->addrs + i*IPV6_ADDR_SIZE;

                                // REMOVE O ENDEREÇO ATUAL
                                // SÓ SE REALMENTE HAVIA COLOCADO UM ANTES
                                if (link->prefixLen) {
                                    char ip[IPV6_ADDR_STR_SIZE]; ip6_to_str(addr, ip);
                                    IP6("addr del dev %s %s/128", link->itfc, ip);
                                }

                                // GERA UM ENDEREÇO NOVO
                                ip6_random_gen(addr);
                                ip6_prefix(addr, prefix, prefixLen);

                                // PÕE O ENDEREÇO NOVO
                                char ip[IPV6_ADDR_STR_SIZE]; ip6_to_str(addr, ip);

                                if (link->itfc == link->itfcOut)
                                    IP6("addr add nodad dev %s %s/128 metric %u", link->itfc, ip, 1U + linkID); // TODO: FIXME: VAI TER QUE ESPECIFICAR O METRIC
                                else
                                    IP6("addr add nodad dev %s %s/128", link->itfc, ip);
                            }

                            IP6("route flush cache");

                            sleep(3);

                            // PASSA A USAR ELE
                            memcpy(link->prefix, prefix, IPV6_ADDR_SIZE); link->prefixLen = prefixLen;

                            printf("   -- DONE\n\n");
                        }
                    } else
                        printf("GW MAC %02X:%02X:%02X:%02X:%02X:%02X MTU %u PREFIX %s FLAGS 0x%02X VALIDLT %u PREFERREDLT %u | "
                            "UNKNOWN RA\n",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mtu, prefixStr,
                            prefixFlags, prefixValidLT, prefixPreferredLT
                            );
                }
            }
        }
    }

    for (uint linkID = 0; linkID != linksN; linkID++) {

        Link* const link = &links[linkID];

        printf("CLEANING LINK #%u ITFC %s\n", linkID, link->itfc);

        if (link->prefixLen) {
            for (uint i = 0; i != link->addrsN; i++) {
                char ip[IPV6_ADDR_STR_SIZE]; ip6_to_str(link->addrs + i*IPV6_ADDR_SIZE, ip);
                IP6("addr del dev %s %s/128", link->itfc, ip);
            }
        }
    }

    IP6("route flush cache");

    return 0;
}
