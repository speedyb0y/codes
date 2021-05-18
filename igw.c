/*
    endereço adicionado
        -> adiciona ele

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

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;

#define elif(c) else if(c)

typedef struct notifier_block notifier_block;
typedef struct net_device net_device;
typedef struct sk_buff sk_buff;

// CUIDADO COM O sock_create_lite!!!
extern int sock_create_REAL (int family, int type, int protocol, struct socket **res);
extern int (*sock_create_USE) (int family, int type, int protocol, struct socket **res);

#define ITFC_INVALID 0xFFFFU

typedef struct Addr4 Addr4;
typedef struct Addr6 Addr6;

struct Addr4 {
    u32 itfc;
    u32 ip;
};

struct Addr6 {
    u64 itfc;
    u64 ip[2];
};

#define PROTO_ADDR 1024

#define ADDRS4_N 512
#define ADDRS6_N 128

static struct Addr4 addrs4[ADDRS4_N];
static struct Addr6 addrs6[ADDRS6_N];

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
        addr->rt_priority <= ADDRS6_N &&
        addr->idev &&
        addr->idev->dev) {

        Addr6* const addr6 = &addrs6[addr->rt_priority - 1];

        if (addr6->itfc >= ITFC_INVALID) {
            addr6->itfc  =        addr->idev->dev->flags & IFF_LOOPBACK ? 0 : addr->idev->dev->ifindex;
            addr6->ip[0] = ((u64*)addr->addr.in6_u.u6_addr8)[0];
            addr6->ip[1] = ((u64*)addr->addr.in6_u.u6_addr8)[1];
        }
    }
}

static void igw_addrs4_add (struct in_ifaddr* const addr) {

    if (addr->ifa_rt_priority &&
        addr->ifa_rt_priority <= ADDRS4_N &&
        addr->ifa_dev &&
        addr->ifa_dev->dev) {

        Addr4* const addr4 = &addrs4[addr->ifa_rt_priority - 1];

        if (addr4->itfc >= ITFC_INVALID) {
            addr4->itfc = addr->ifa_dev->dev->ifindex;
            addr4->ip   = addr->ifa_address;
        }
    }
}

static void igw_addrs6_del (const struct inet6_ifaddr* const addr) {

    if (addr->rt_priority &&
        addr->rt_priority <= ADDRS6_N) {

        Addr6* const addr6 = &addrs6[addr->rt_priority - 1];

        if (addr6->ip[0] == ((u64*)addr->addr.in6_u.u6_addr8)[0] &&
            addr6->ip[1] == ((u64*)addr->addr.in6_u.u6_addr8)[1] &&
            addr6->itfc  ==        addr->idev->dev->flags & IFF_LOOPBACK ? 0 : addr->idev->dev->ifindex)
            addr6->itfc = ITFC_INVALID;
    }
}

static void igw_addrs4_del (const struct in_ifaddr* const addr) {

    if (addr->ifa_rt_priority &&
        addr->ifa_rt_priority <= ADDRS4_N) {

        Addr4* const addr4 = &addrs4[addr->ifa_rt_priority - 1];

        if (addr4->ip   == addr->ifa_address &&
            addr4->itfc == addr->ifa_dev->dev->ifindex)
            addr4->itfc = ITFC_INVALID;
    }
}

static int igw_sock_create4 (uint i, struct socket **res) {

    igw_acquire();

    Addr4* addr4;

    if ((i -= PROTO_ADDR) >= ADDRS4_N) {
        uint count = ADDRS4_N;
        while ((addr4 = &addrs4[i %= ADDRS4_N])->itfc >= ITFC_INVALID) {
            if (--count == 0)
                goto BAD;
            i++;
        }
    } elif ((addr4 = &addrs4[i])->itfc >= ITFC_INVALID)
        goto BAD;

    // COPIA
    const uint itfc = addr4->itfc;
    const u32 sockAddr[2] = { 0x00000200U, addr4->ip }; // struct sockaddr_in

    igw_release();

    const int ret = sock_create_REAL(AF_INET, SOCK_STREAM, 0, res);

    if (ret >= 0) {
        // BIND TO INTERFACE
        (*res)->sk->sk_bound_dev_if = itfc;
        // BIND TO ADDRESS
        (void)inet_bind((*res), (struct sockaddr*)sockAddr, sizeof(struct sockaddr_in));
        // TCP_NODELAY
    }

    return ret;

BAD:
    igw_release();

    return -EINVAL;
}

static int igw_sock_create6 (uint i, struct socket **res) {

    igw_acquire();

    Addr6* addr6;

    if ((i -= PROTO_ADDR) >= ADDRS6_N) {
        uint count = ADDRS6_N;
        while ((addr6 = &addrs6[i %= ADDRS6_N])->itfc >= ITFC_INVALID) {
            if (--count == 0)
                goto BAD;
            i++;
        }
    } elif ((addr6 = &addrs6[i])->itfc >= ITFC_INVALID)
        goto BAD;

    // COPIA
    const uint itfc = addr6->itfc;

    const u64 sockAddr[4] = { // struct sockaddr_in6
        0x0000000000000A00ULL, // family port flowinfo
        addr6->ip[0],
        addr6->ip[1],
        0 // scope ID
        };

    igw_release();

    const int ret = sock_create_REAL(AF_INET6, SOCK_STREAM, 0, res);

    if (ret >= 0) {
        // BIND TO INTERFACE
        (*res)->sk->sk_bound_dev_if = itfc;
        // BIND TO ADDRESS
        (void)inet6_bind((*res), (struct sockaddr*)sockAddr, sizeof(struct sockaddr_in6));
    }

    return ret;

BAD:
    igw_release();

    return -EINVAL;
}

static int igw_sock_create (int fam, int type, int proto, struct socket **res) {

    if (proto < PROTO_ADDR)
        return sock_create_REAL(fam, type, proto, res);

    // TODO: FIXME: suportar UDP ? :S
    if (fam == AF_INET)
        return igw_sock_create4(proto, res);

    return igw_sock_create6(proto, res);
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

    igw_acquire();

    net_device* const dev = netdev_notifier_info_to_dev(data);
    struct in_ifaddr* addr4;
    struct inet6_ifaddr* addr6;

    // CONSIDERA QUE INTERFACE LOOPBACK NUNCA TEM EVENTO DOWN/UNREGISTER
    if ((dev->flags & IFF_LOOPBACK) || ((action == NETDEV_CHANGE || action == NETDEV_UP) && (dev->flags & IFF_UP) && (dev->operstate == IF_OPER_UP))) {
        addr4 = rtnl_dereference(dev->ip_ptr->ifa_list);
        while (addr4) { igw_addrs4_add(addr4);
            addr4 = rtnl_dereference(addr4->ifa_next);
        }
        list_for_each_entry(addr6, &dev->ip6_ptr->addr_list, if_list)
            igw_addrs6_add(addr6);
    } else {
        addr4 = rtnl_dereference(dev->ip_ptr->ifa_list);
        while (addr4) { igw_addrs4_del(addr4);
            addr4 = rtnl_dereference(addr4->ifa_next);
        }
        list_for_each_entry(addr6, &dev->ip6_ptr->addr_list, if_list)
            igw_addrs6_del(addr6);
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

    memset(addrs6, 0xFF, sizeof(addrs6));
    memset(addrs4, 0xFF, sizeof(addrs4));

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

    memset(addrs6, 0xFF, sizeof(addrs6));
    memset(addrs4, 0xFF, sizeof(addrs4));

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
