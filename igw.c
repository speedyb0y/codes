/*
    endereço adicionado
        -> adiciona ele

    IPV6
        bind_non_local 1

    IPV4
        bind_non_local 0

    TENTARRETIRAR O MODULO DUAS VEZEs:
    - A PRIMEIRA SÓ RETIRA OS HOOKS
    - A OUTRA SAI DE VEZ

    DÁ ERRO!!!
    strace ip -6 addr add dev eth0  ::212/24 scope host

    TODO: FIXME: uma syscall para registrar as rotas
            pois pode ser necessário testar elas primeiro

se o prefixo mudar
    -> deleta forçadamente o endereço
    -> reseta o AGAIN

if again
    de tempos em tempos adiciona novos endereços
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/net.h>
#include <linux/ipv6.h>
#include <net/ip.h>
#include <net/inet_common.h>
#include <net/if_inet6.h>
#include <net/addrconf.h>
#include <uapi/linux/in6.h>

/*
net/socket.c
static sock_create_REAL
int (*sock_create_USE)(int family, int type, int protocol, struct socket **res) = NULL;
int sock_create (int family, int type, int protocol, struct socket **res) {
    return (sock_create_USE ? : sock_create_REAL) (family, type, protocol, res);
}
EXPORT_SYMBOL(sock_create);
EXPORT_SYMBOL(sock_create_USE);

include/linux/net.h
sock_create() -> extern (*sock_create)

CUIDADO COM O sock_create_lite!!!

*/

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;

#define elif(c) else if(c)

typedef struct notifier_block notifier_block;
typedef struct net_device net_device;
typedef struct sk_buff sk_buff;

extern int sock_create_REAL (int family, int type, int protocol, struct socket **res);
extern int (*sock_create_USE) (int family, int type, int protocol, struct socket **res);

#define FMTIPV6(addr) \
    (addr)[0], (addr)[1], (addr)[2], (addr)[3], (addr)[4], (addr)[5], (addr)[6], (addr)[7], \
    (addr)[8], (addr)[9], (addr)[10], (addr)[11], (addr)[12], (addr)[13], (addr)[14], (addr)[15]

#if 0
#define FMTIPV4(addr) (((addr) >> 24) & 0xFF), (((addr) >> 16) & 0xFF), (((addr) >> 8) & 0xFF), ((addr) & 0xFF)
#else
#define FMTIPV4(addr) ((addr) & 0xFF), (((addr) >> 8) & 0xFF), (((addr) >> 16) & 0xFF), (((addr) >> 24) & 0xFF)
#endif

#define dbg(...) ({ })

typedef struct Addr4 Addr4;
typedef struct Addr6 Addr6;

struct Addr4 {
    struct in_ifaddr* addr;
    u64 until;
    u16 itfc;
    u16 prefixLen;
    u32 prefix;
};

struct Addr6 {
    struct inet6_ifaddr* addr;
    u64 until;
    u16 itfc;
    u16 prefixLen;
    u8 prefix[16];
};

#define IPV4_ADDRS_N 2048
#define IPV6_ADDRS_N 512

#define ITFC_INDEX_INVALID 0xFFFFF

static uint lo;

static uint addrs4Last;
static uint addrs6Last;

static struct Addr4 addrs4[IPV4_ADDRS_N];
static struct Addr6 addrs6[IPV6_ADDRS_N];

static int lock;

static inline void igw_acquire (void) {

    int locked = 0;

    // TODO: FIXME: IN ASSEMBLY COULD DO THIS WITHOUT HAVING TO REWRITE THE VARIABLE?
    while (!__atomic_compare_exchange_n(&lock, &locked, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        locked = 0;
}

static inline void igw_release (void) {

    __atomic_store_n(&lock, 0, __ATOMIC_SEQ_CST);
}

static void igw_addrs6_add (struct inet6_ifaddr* const addr) {

    if (addr->rt_priority &&
        addr->rt_priority <= IPV6_ADDRS_N &&
        addr->idev &&
        addr->idev->dev) {

        const uint i = addr->rt_priority - 1;

        if (addrs6Last < i)
            addrs6Last = i;

        Addr6* const addr6 = &addrs6[i];

        addr6->addr      = addr;
        addr6->until     = addr->prefered_lft;
        addr6->itfc      = addr->idev->dev->ifindex != lo ? addr->idev->dev->ifindex : 0;
        addr6->prefixLen = addr->prefix_len;
        memcpy(addr6->prefix, addr->addr.in6_u.u6_addr8, 16);

        printk("IGW: ADDR6 ADD #%u ITFC %u %s %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X/%u\n", i,
                addr6->itfc, addr->idev->dev->name,
        FMTIPV6(addr6->prefix),
                addr6->prefixLen
            );

        addrs6N++;
    }
}

static void igw_addrs4_add (struct in_ifaddr* const addr) {

    if (addr->ifa_rt_priority &&
        addr->ifa_rt_priority <= IPV4_ADDRS_N &&
        addr->idev &&
        addr->idev->dev) {  // PODERIA USAR O ifa_mask, CONTANDO OS BITS

        const uint i = addr->ifa_rt_priority - 1;

        if (addrs4Last < i)
            addrs4Last = i;

        Addr4* const addr4 = &addrs4[i];

        addr4->addr      = addr;
        addr4->until     = addr->ifa_preferred_lft; // Expiry is at tstamp + HZ * lft
        addr4->itfc      = addr->ifa_dev->dev->ifindex != lo ? addr->ifa_dev->dev->ifindex : 0;
        addr4->prefixLen = addr->ifa_prefixlen;
        addr4->prefix    = addr->ifa_address;

        // ifa_valid_lft
        //unsigned long     ifa_tstamp; /* updated timestamp */
        printk("IGW: ADDR4 ADD #%u ITFC %u %s %u.%u.%u.%u/%u\n", i,
            addr4->itfc, addr->ifa_dev->dev->name,
    FMTIPV4(addr4->prefix),
            addr4->prefixLen
            );

        addrs4N++;
    }
}

static void igw_addrs6_del (const struct inet6_ifaddr* const addr) {

    if (addr->ifa_rt_priority &&
        addr->ifa_rt_priority <= IPV4_ADDRS_N) {

        const uint i = addr->ifa_rt_priority - 1;

        Addr6* const addr6 = &addrs6[i];

        if (addr6->addr == addr) {
            printk("IGW: ADDR6 DEL #%u %s %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X/%u\n", i,
                addr6->itfc, addr->idev->dev->name,
        FMTIPV6(addr6->prefix),
                addr6->prefixLen);
            addr6->addr = 0;
        }
    }
}

static void igw_addrs4_del (const struct in_ifaddr* const addr) {

    if (addr->ifa_rt_priority &&
        addr->ifa_rt_priority <= IPV4_ADDRS_N) {

        const uint i = addr->ifa_rt_priority - 1;

        Addr4* const addr4 = &addrs4[i];

        if (addr4->addr == addr) {
            printk("IGW: ADDR4 DEL %s %u.%u.%u.%u/%u METRIC %u\n", i,
                addr4->itfc, addr->idev->dev->name,
        FMTIPV4(addr4->prefix),
                addr4->prefixLen);
            addr4->addr = 0;
        }
    }
}

// OVERWRITE O IP COM O PREFIXO
static void igw_prefixize6 (u8* ip, const u8* prefix, uint prefixLen) {

    while (prefixLen) {
        const uint amount = (prefixLen < 8) ? prefixLen : 8;
        const uint mask = (0xFFU << (8 - amount)) & 0xFFU;
        *ip &= ~mask;
        *ip++ |= *prefix++ & mask;
        prefixLen -= amount;
    }
}

static int igw_sock_create (int family, int type, int protocol, struct socket **res) {

    const int ret = sock_create_REAL(family, type, (protocol < IPPROTO_MAX) ? protocol : 0, res);

    // TODO: FIXME: O CERTO SERIA CONSIDERAR family, POIS O protocol É DENTRO DO DOMÍNIO DELE
    // MAS ASSUMINDO QUE NENHUM OUTRO DOMÍNIO TEM PROTOCOLOS >= QUE O DO IP
    if (ret >= 0 && protocol >= IPPROTO_MAX) {
        struct socket* sock = *res;
        igw_acquire();
        if (family == AF_INET) {
            if (addrs4N) {
                Addr4* addr = &addrs4[protocol % addrs4N];
                // TODO: FIXME: HANDLE PREFIX LEN
                struct sockaddr_in sockAddr = { .sin_family = AF_INET, .sin_port = 0, .sin_addr = { .s_addr = addr->prefix } };
                // O sock_setsockopt usa isso
                //   sock_bindtoindex_locked(()
                // que aí dá nisso
                sock->sk->sk_bound_dev_if = addr->itfc;
                //if (sk->sk_prot->rehash)
                    //sk->sk_prot->rehash(sk);
                // TODO: FIXME: HANDLE FAILURE HERE
                igw_release();
                dbg("BOUND IPV4 SOCKET TO FAMILY %d %u.%u.%u.%u\n", sockAddr.sin_family, FMTIPV4(sockAddr.sin_addr.s_addr));
                (void)inet_bind((*res), (struct sockaddr*)&sockAddr, sizeof(sockAddr));
            } else
                igw_release();
        } elif (addrs6N) {
            Addr6* addr = &addrs6[protocol % addrs6N];
            struct sockaddr_in6 sockAddr = { .sin6_family = AF_INET6, .sin6_port = 0, .sin6_flowinfo = 0, .sin6_scope_id = 0 };
            // GERA UM ALEATÓRIO
            ((u64*)sockAddr.sin6_addr.in6_u.u6_addr8)[0] = rdtsc();
            ((u64*)sockAddr.sin6_addr.in6_u.u6_addr8)[1] = rdtsc() + protocol; // (+ jiffies) << 32
            // INSERE O PREFIXO
            igw_prefixize6(sockAddr.sin6_addr.in6_u.u6_addr8, addr->prefix, addr->prefixLen);
            if (addr->itfc)
                sock->sk->sk_bound_dev_if = addr->itfc;
            igw_release();
            dbg("BOUND IPV6 SOCKET TO FAMILY %d %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X\n",
                sockAddr.sin6_family, FMTIPV6(sockAddr.sin6_addr.in6_u.u6_addr8));
            (void)inet6_bind((*res), (struct sockaddr*)&sockAddr, sizeof(sockAddr));
        } else
            igw_release();
    }

    return ret;
}

// TODO: FIXME: E OS DEMAIS EVENTOS?
static int igw_addrs4_notify (notifier_block *nb, unsigned long action, void *addr) {

    igw_acquire();

    if (action == NETDEV_UP)
        igw_addrs4_add((struct in_ifaddr*)addr);
    elif (action == NETDEV_DOWN)
        igw_addrs4_del((struct in_ifaddr*)addr);

    igw_release();

    return 0;
}

static int igw_addrs6_notify (notifier_block *nb, unsigned long action, void* addr) {

    igw_acquire();

    if (action == NETDEV_UP)
        igw_addrs6_add((struct inet6_ifaddr*)addr);
    elif (action == NETDEV_DOWN)
        igw_addrs6_del((struct inet6_ifaddr*)addr);

    igw_release();

    return NOTIFY_OK;
}

static int igw_itfcs_notify (notifier_block *nb, unsigned long action, void *data) {

    net_device* dev;
    struct in_ifaddr* addr4;
    struct inet6_ifaddr* addr6;

    igw_acquire();

    dev = netdev_notifier_info_to_dev(data);

    printk("IGW: DEVICE %s INDEX %d ACTION %s FLAGS 0x%08X OP STATE 0x%X\n", dev->name, dev->ifindex, netdev_cmd_to_name(action), dev->flags, (unsigned)dev->operstate);

    // TODO: VERIFICAR SE É LOOPBACK PELA FLAG
    if (dev->ifindex != lo) {
        if (!strcmp(dev->name, "lo"))
            lo = dev->ifindex;
        elif ((action == NETDEV_CHANGE || action == NETDEV_UP) && (dev->flags & IFF_UP) && (dev->operstate == IF_OPER_UP)) {
            addr4 = rtnl_dereference(dev->ip_ptr->ifa_list);
            while (addr4) {
                igw_addrs4_add((struct in_ifaddr*)addr4);
                addr4 = rtnl_dereference(addr4->ifa_next);
            }
            list_for_each_entry(addr6, &dev->ip6_ptr->addr_list, if_list)
                igw_addrs6_add((struct inet6_ifaddr*)addr6);
        } else {
            addr4 = rtnl_dereference(dev->ip_ptr->ifa_list);
            while (addr4) {
                igw_addrs4_del((struct in_ifaddr*)addr4);
                addr4 = rtnl_dereference(addr4->ifa_next);
            }
            list_for_each_entry(addr6, &dev->ip6_ptr->addr_list, if_list)
                igw_addrs6_del((struct inet6_ifaddr*)addr6);
        }
    }

    igw_release();

    return NOTIFY_OK;
}

static notifier_block notifyItfcs = {
    .notifier_call = igw_itfcs_notify,
    .next = NULL,
    .priority = 0,
};

static notifier_block notifyAddrs4 = {
    .notifier_call = igw_addrs4_notify,
    .next = NULL,
    .priority = 0,
};

static notifier_block notifyAddrs6 = {
    .notifier_call = igw_addrs6_notify,
    .next = NULL,
    .priority = 0,
};

static int igw_init (void) {

    printk("IGW: LOADING...\n");

    lock = 0;

    //
    lo = ITFC_INDEX_INVALID;

    addrs4Last = 0;
    addrs6Last = 0;

    igw_acquire();
    igw_release();

    // INSERT THE HOOKS
    sock_create_USE = igw_sock_create;

    register_netdevice_notifier(&notifyItfcs);
    register_inetaddr_notifier(&notifyAddrs4);
    register_inet6addr_notifier(&notifyAddrs6);

    printk("IGW: LOADED.\n");

    return 0;
}

static void igw_exit (void) {

    printk("IGW: UNLOADING...\n");

    // REMOVE THE HOOKS
    sock_create_USE = sock_create_REAL;

    // MEMORY BARRIER
    igw_acquire();
    igw_release();

    unregister_netdevice_notifier(&notifyItfcs);
    unregister_inetaddr_notifier(&notifyAddrs4);
    unregister_inet6addr_notifier(&notifyAddrs6);

    printk("IGW: UNLOADED.\n");
}

module_init(igw_init);
module_exit(igw_exit);

MODULE_AUTHOR("speedyb0y");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_LICENSE("GPL");
MODULE_ALIAS("igw");
MODULE_VERSION("0.01");
