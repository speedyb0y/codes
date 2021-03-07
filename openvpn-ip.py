#!/usr/bin/python

# OBS.: SE PEGAR O MESMO ENDEREÇO EM MAIS DE UMA VPN, VAI DAR MERDA

import sys
import os

def str2ip (ip):

    return sum(x << (24 - n*8) for n, x in enumerate(map(int, ip.split('.'))))

def ip2str (ip):

    return '%d.%d.%d.%d' % (
        ( ip >> 24),
        ((ip >> 16) & 0xFF),
        ((ip >>  8) & 0xFF),
        ( ip        & 0xFF)
        )

def network_from_ip_netmask (ip, netmask):

    assert 1 <= ip <= 0xFFFFFFFF
    assert 1 <= netmask <= 32

    hostmask = 32 - netmask

    return (ip >> hostmask) << hostmask

print('###### ip', sys.argv[1:])

vpnID = int(os.getenv('config')[len('config-'):])

dev = f'openvpn-{vpnID}'

table = 500 + vpnID

what, cmd, *args = sys.argv[1:]

if what == 'link':

    assert cmd == 'set'
    assert args[0] == 'dev'
    assert args[1] == dev

    os.system(f'ip rule flush table {table}')
    os.system(f'ip route flush table {table}')
    os.system(f'ip route flush dev {dev}')
    os.system(f'ip addr flush dev {dev}')

    assert 0 == os.system('ip link set ' + ' '.join(args))

elif what == 'route':

    if cmd == 'add':
        # ip route add 191.96.15.86/32 via 192.168.200.254

        dst, _, gw = args

        assert _ == 'via'

        assert 0 == os.system(f'ip route add {dst} via {gw} table {table}')

    elif cmd == 'del':
        # ip route del 45.231.207.66/32

        assert len(args) == 1

        dst = args[0]

        assert 0 == os.system(f'ip route del {dst} table {table}')
    else:
        raise NotImplemented

elif what == 'addr':

    if cmd == 'add':
        # 'ip addr add dev openvpn-0 10.8.8.8/24'
        assert len(args) == 3
        assert args[0] == 'dev'
        assert args[1] == dev

        ip, netmask = args[2].split('/')

        netmask = int(netmask)

        assert 1 <= netmask <= 32

        network = network_from_ip_netmask(str2ip(ip), netmask)

        assert 1 <= network <= 0xFFFFFFFF

        network = ip2str(network)

        assert 0 == os.system(f'ip addr add {ip}/32 dev {dev}')
        assert 0 == os.system(f'ip route add {network}/{netmask} dev {dev} table {table}')
        assert 0 == os.system(f'ip rule add to {ip}/32 table {table}')
        assert 0 == os.system(f'ip rule add from {ip}/32 table {table}')

    elif cmd == 'del':

        # 'ip addr del dev openvpn-0 10.8.8.5/24'
        assert len(args) == 3
        assert args[0] == 'dev'
        assert args[1] == dev

        ip, netmask = args[2].split('/')

        assert 0 == os.system(f'ip addr del {ip}/32 dev {dev}')
    else:
        raise NotImplemented

else:
    raise NotImplemented
