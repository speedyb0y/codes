/*
    interface unregister/down/no carrier
        remove todos os endereços ipv4 com == itfc
        remove todos os endereços ipv6 com == itfc

    endereço removido
        -> remove ele


    endereço adicionado
        -> adiciona ele

        IPV6
            device prefix prefixlen
            se nao for lo
            se nao for a rede local fe80::
            bind_non_local 1

        IPV4
            bind_non_local 0
            device

    interface register/up/carrier
        lista todos os endereços ipv4 dela
            adiciona eles
        lista todos os endereços ipv4 dela
            adiciona eles

    socket()
        IPV4
            -> def->iifindex
        IPV6
            -> def->iifindex
            -> addr
                gera um aleatório
                coloca o prefixo por cima


*/


/*
preferred_lft 30min valid_lft 2h

se o prefixo mudar
    -> deleta forçadamente o endereço
    -> reseta o AGAIN

if again
    de tempos em tempos adiciona novos endereços


struct addrs {
    u16 prev;
    u16 next;
    ATEQYABDIOIDEYSARPARABIVASCONEXOES
      QUANDOSERADELETADO
PREFIX LEN
PREFIX
};


int sock_create(int family, int type, int protocol, struct socket **res)
{
    return __sock_create(current->nsproxy->net_ns, family, type, protocol, res, 0);
}
EXPORT_SYMBOL(sock_create);


// O sock_setsockopt usa isso
//   sock_bindtoindex_locked(()
// que aí dá nisso
struct sock* sk;
sk->sk_bound_dev_if = ifindex;
if (sk->sk_prot->rehash)
    sk->sk_prot->rehash(sk);

int sock_setsockopt(struct socket *sock, int level, int optname, sockptr_t optval, unsigned int optlen)


itfcAddrs[4096]; //

oMYCODE Pode especificar se é para download, seeestavel, se é direto, etc
transforma o socket()
    em:
    socket(AF_INET, MYCODE, COUNTER)
    if (== MYCODE) {
        socket(AF_INET, SOCK_STREAM, 0);
        if (sock > 0) {
            setsockopt(SO_BINDTOIFINDEX, )
            seprefixlen naofor0, gera um aleatório
            bind();
        }
       } else
        socket()
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
#include <net/if_inet6.h>
#include <net/addrconf.h>

/*
net/ipv6/addrconf.c
struct inet6_ifaddr * (*ipv6_add_addr_USE)(struct inet6_dev *idev, struct ifa6_config *cfg, bool can_block, struct netlink_ext_ack *extack) = ipv6_add_addr_REAL;
EXPORT_SYMBOL(ipv6_add_addr_REAL);
EXPORT_SYMBOL(ipv6_add_addr_USE);

void (*ipv6_del_addr_USE)(struct inet6_ifaddr *ifp) = ipv6_del_addr_REAL;
EXPORT_SYMBOL(ipv6_del_addr_REAL);
EXPORT_SYMBOL(ipv6_del_addr_USE);

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

#define FMTIPV6(addr) (addr)[0], (addr)[1], (addr)[2], (addr)[3], (addr)[4], (addr)[5], (addr)[6], (addr)[7]
#define FMTIPV4(addr) (((addr) >> 24) & 0xFF), (((addr) >> 16) & 0xFF), (((addr) >> 8) & 0xFF), ((addr) & 0xFF)

typedef struct IP4Addr IP4Addr;
typedef struct IP6Addr IP6Addr;

struct IP4Addr {
    struct in_ifaddr* addr;
    u64 until;
    u32 reserved;
    u16 itfc;
    u16 prefixLen;
    u32 prefix;
};

struct IP6Addr {
    struct inet6_ifaddr* addr;
    u64 until;
    u32 reserved;
    u16 itfc;
    u16 prefixLen;
    u8 prefix[16];
};

#define IPV4_ADDRS_N 2048
#define IPV6_ADDRS_N 256

static uint ipv4AddrsN;
static uint ipv6AddrsN;

static struct IP4Addr ipv4Addrs[IPV4_ADDRS_N];
static struct IP6Addr ipv6Addrs[IPV6_ADDRS_N];

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

// TODO: NUNCA ADICIONAR REPETIDOS, SEMPRE REMOVER TODOS
static void igw_addrs6_add (struct inet6_ifaddr* const addr) {

    if (ipv6AddrsN != IPV6_ADDRS_N) {

        IP6Addr* const addr6 = &ipv6Addrs[ipv6AddrsN++];

        printk("ADDR6 ADD %u %02X%02X:%02X%02X:%02X%02X:%02X%02X/%u\n", ipv6AddrsN, FMTIPV6(addr->addr.in6_u.u6_addr8), addr->prefix_len);

        addr6->addr = addr;
        addr6->until = 0;
        addr6->reserved  = 0;
        addr6->itfc = addr->idev->dev->ifindex;
        addr6->prefixLen = addr->prefix_len;
        memcpy(addr6->prefix, addr->addr.in6_u.u6_addr8, 16);
    }
}

static void igw_addrs4_add (struct in_ifaddr* const addr) {

    if (ipv4AddrsN != IPV6_ADDRS_N) {

        IP4Addr* const addr4 = &ipv4Addrs[ipv4AddrsN++];

        printk("ADDR4 ADD %u %u.%u.%u.%u/%u\n", ipv4AddrsN, FMTIPV4(addr->ifa_address), addr->ifa_mask);

        addr4->addr      = addr;
        addr4->until     = 0;
        addr4->reserved  = 0;
        addr4->itfc      = addr->ifa_dev->dev->ifindex;
        addr4->prefixLen = addr->ifa_mask;
        addr4->prefix    = addr->ifa_address;
    }
}

static void igw_addrs6_del (const struct inet6_ifaddr* const addr) {

    uint i;

    for (i = 0; i != ipv6AddrsN; i++) {
        if (ipv6Addrs[i].addr == addr) {
            printk("ADDR6 DEL %u %02X%02X:%02X%02X:%02X%02X:%02X%02X/%u\n", i, FMTIPV6(ipv6Addrs[i].prefix), ipv6Addrs[i].prefixLen);
            if (i == --ipv6AddrsN)
                break;
            memcpy(&ipv6Addrs[i], &ipv6Addrs[ipv6AddrsN], sizeof(IP6Addr));
        }
    }
}

static void igw_addrs4_del (const struct in_ifaddr* const addr) {

    uint i = 0;

    while (i != ipv4AddrsN) {
        if (ipv4Addrs[i].addr == addr) {
            printk("ADDR4 DEL %u %u.%u.%u.%u/%u\n", i, FMTIPV4(ipv4Addrs[i].prefix), ipv4Addrs[i].prefixLen);
            if (i != --ipv4AddrsN)
                memcpy(&ipv4Addrs[i], &ipv4Addrs[ipv4AddrsN], sizeof(IP4Addr));
        } else
            i++;
    }
}

static int igw_sock_create (int family, int type, int protocol, struct socket **res) {

    printk("SOCKET CREATE\n");

//ASINCRONO!!!
    return sock_create_REAL(family, type, protocol, res);
}

static net_device* lo;

static int igw_addrs4_notify (notifier_block *nb, unsigned long action, void *addr) {

    // TODO: ifa_prefixlen OU ifa_mask?
    igw_acquire();

    //addr->valid_lft;
    //addr->prefered_lft;
    if (action == NETDEV_UP)
        igw_addrs4_add((struct in_ifaddr*)addr);
    elif (action == NETDEV_DOWN)
        igw_addrs4_del((struct in_ifaddr*)addr);

    igw_release();

    return 0;
}

static int igw_addrs6_notify (notifier_block *nb, unsigned long action, void* addr) {

    igw_acquire();

    //addr->valid_lft;
    //addr->prefered_lft;
    if (action == NETDEV_UP)
        igw_addrs6_add((struct inet6_ifaddr*)addr);
    elif (action == NETDEV_DOWN)
        igw_addrs6_del((struct inet6_ifaddr*)addr);

    igw_release();

    return 0;
}

static int igw_itfcs_notify (notifier_block *nb, unsigned long action, void *data) {

    net_device* dev;
    struct in_ifaddr* addr4;
    struct inet6_ifaddr* addr6;

    igw_acquire();

    dev = netdev_notifier_info_to_dev(data);

    printk("IGW: DEVICE %s ACTION %s\n", dev->name, netdev_cmd_to_name(action));

    // TODO: FIXME: MARCA OS ENDEREÇOS COMO USÁVEIS/NÃO USAVEIS CONFORME O action
    if (action == NETDEV_UP) {
        addr4 = rtnl_dereference(dev->ip_ptr->ifa_list);
        while (addr4) {
            igw_addrs4_add((struct in_ifaddr*)addr4);
            addr4 = rtnl_dereference(addr4->ifa_next);
        }
        list_for_each_entry(addr6, &dev->ip6_ptr->addr_list, if_list)
            igw_addrs6_add((struct inet6_ifaddr*)addr6);
    } elif (action == NETDEV_UNREGISTER ||
            action == NETDEV_GOING_DOWN ||
            action == NETDEV_DOWN) {
        addr4 = rtnl_dereference(dev->ip_ptr->ifa_list);
        while (addr4) {
            igw_addrs4_del((struct in_ifaddr*)addr4);
            addr4 = rtnl_dereference(addr4->ifa_next);
        }
        list_for_each_entry(addr6, &dev->ip6_ptr->addr_list, if_list)
            igw_addrs6_del((struct inet6_ifaddr*)addr6);
    }

    igw_release();

    return 0;
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

    lock = 0;

    //
    lo = NULL;

    ipv4AddrsN = 0;
    ipv6AddrsN = 0;

    igw_acquire();
    igw_release();

    // INSERT THE HOOKS
    //sock_create_USE = igw_sock_create;

    register_netdevice_notifier(&notifyItfcs);
    register_inetaddr_notifier(&notifyAddrs4);
    register_inet6addr_notifier(&notifyAddrs6);

    return 0;
}

static void igw_exit (void) {

    // REMOVE THE HOOKS
    //sock_create_USE = sock_create_REAL;

    unregister_netdevice_notifier(&notifyItfcs);
    unregister_inetaddr_notifier(&notifyAddrs4);
    unregister_inet6addr_notifier(&notifyAddrs6);

    // MEMORY BARRIER
    igw_acquire();
    igw_release();
}

module_init(igw_init);
module_exit(igw_exit);

MODULE_AUTHOR("speedyb0y");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_LICENSE("GPL");
MODULE_ALIAS("igw");
MODULE_VERSION("0.01");
