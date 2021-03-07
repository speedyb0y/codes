#!/usr/bin/python

# OBS.: SE PEGAR O MESMO ENDEREÇO EM MAIS DE UMA VPN, VAI DAR MERDA

# RETIRA O 'nobind'
# sndbuf 134217728
# rcvbuf 134217728

import sys
import os

print('###### ip', sys.argv[1:])

dev, table = {
    'br-sao.prod.surfshark.com_udp.ovpn' :  ('tunVPN0', 220),
    'us-nyc.prod.surfshark.com_udp.ovpn' :  ('tunVPN1', 221),
    'ar-bua.prod.surfshark.com_udp.ovpn' :  ('tunVPN2', 222),
    'de-fra.prod.surfshark.com_udp.ovpn' :  ('tunVPN3', 223),
    'de-ber.prod.surfshark.com_udp.ovpn' :  ('tunVPN4', 224),
    'py-asu.prod.surfshark.com_udp.ovpn' :  ('tunVPN5', 225),
    'us-tpa.prod.surfshark.com_udp.ovpn' :  ('tunVPN6', 226),
    'ca-tor.prod.surfshark.com_udp.ovpn' :  ('tunVPN7', 227),
    'us-free-05.protonvpn.com.udp.ovpn'  :  ('tunVPN8', 228),
    'nl-free-09.protonvpn.com.udp.ovpn'  :  ('tunVPN9', 229),
    }[os.getenv('config')]

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
        assert False

elif what == 'addr':

    if cmd == 'add':
        # 'ip addr add dev tunSurfsharkBR 10.8.8.8/24'
        assert len(args) == 3
        assert args[0] == 'dev'
        assert args[1] == dev
        ip, netmask = args[2].split('/')
        netmask = int(netmask)
        assert 1 <= netmask <= 32
        network = (sum(x << (24 - n*8) for n, x in enumerate(map(int, ip.split('.')))) >> (32 - netmask)) << (32 - netmask)
        assert 1 <= network <= 0xFFFFFFFF
        network = '%d.%d.%d.%d' % ((network >> 24), ((network >> 16) & 0xFF) , (network >> 8) & 0xFF, network & 0xFF)
        assert 0 == os.system(f'ip addr add {ip}/32 dev {dev}')
        assert 0 == os.system(f'ip route add {network}/{netmask} dev {dev} table {table}')
        assert 0 == os.system(f'ip rule add to   {ip}/32 table {table}')
        assert 0 == os.system(f'ip rule add from {ip}/32 table {table}')

    elif cmd == 'del':
        # 'ip addr del dev tunSurfsharkBR 10.8.8.5/24'
        assert len(args) == 3
        assert args[0] == 'dev'
        assert args[1] == dev
        ip, _ = args[2].split('/')
        os.system(f'ip addr del {ip}/32 dev {dev}')
    else:
        assert False

else:
    assert False
