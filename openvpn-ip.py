#!/usr/bin/python

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

vpn = int(os.getenv('config')[len('config-'):])

dev = f'openvpn-{vpn}'

table = 5000 + vpn

sys.argv[0] = 'ip'

cmdline, args = ' '.join(sys.argv), sys.argv[3:]

# TODO: NÃO DEIXAR SER UM DOS MEUS IPS
# TODO: FIXME: OS GATEWAYS TAMBÉM DEVEM SER IPS PRIVADOS
# TODO: FIXME: SÓ PODE SER IPS PRIVADOS

if cmdline.startswith(f'ip link set dev {dev} '):

    os.system(f'ip route flush table {table}')
    assert 0 == os.system(f'ip route flush dev {dev}')
    assert 0 == os.system(f'ip addr flush dev {dev}')
    assert 0 == os.system(cmdline)

elif cmdline.startswith('ip route add '):
    # ip route add 191.96.15.86/32 via 192.168.200.254

    dst, _, gw = args

    assert _ == 'via'
    assert 0 == os.system(f'ip route add {dst} via {gw} table {table}')

elif cmdline.startswith('ip route del '):
    # ip route del 45.231.207.66/32

    dst, = args

    assert 0 == os.system(f'ip route del {dst} table {table}')

elif cmdline.startswith(f'ip addr add dev {dev}'):
    # TEM QUE PEGAR O ENDEREÇO, RETIRAR O NETMASK, E ADICIONAR O ENDEREÇO /32
    # E AI A PARTIR DESTE ENDEREÇO E DO NETMASK, OBTER A REDE, E ADICIONAR A ROTA PARA A TAL REDE USANDO A TABELA

    if '/' in cmdline:
        # ip addr add dev openvpn-0 10.8.8.8/24

        assert len(args) == 3

        ip, netmask = args[2].split('/')

        netmask = int(netmask)

        assert 1 <= netmask <= 32

        network = network_from_ip_netmask(str2ip(ip), netmask)

        assert 1 <= network <= 0xFFFFFFFF

        network = ip2str(network)

        assert 0 == os.system(f'ip addr add {ip}/32 dev {dev}')
        assert 0 == os.system(f'ip route add {network}/{netmask} dev {dev} table {table}')

        # ASSIM PODE USAR A VPN USANDO BIND COM O ENDEREÇO
        # OBS.: SE PEGAR O MESMO ENDEREÇO EM MAIS DE UMA VPN, VAI DAR MERDA
        # assert 0 == os.system(f'ip rule add to {ip}/32 table {table}')
        # assert 0 == os.system(f'ip rule add from {ip}/32 table {table}')
    else:
        # ip addr add dev openvpn-200 local 10.211.1.145 peer 10.211.1.146

        _, _, _, _, _, local_, ip, peer_, peer = sys.argv

        assert local_ == 'local'
        assert peer_ == 'peer'

        assert 0 == os.system(f'ip addr add {ip}/32 dev {dev}')
        assert 0 == os.system(f'ip route add {peer} dev {dev} proto kernel scope link src {ip} table {table}')
        #adicionar ele como default peer entao?

elif cmdline.startswith(f'ip addr del dev {dev}'):

    if '/' in cmdline:
        # ip addr del dev openvpn-0 10.8.8.5/24
        assert len(args) == 3

        ip, netmask = args[2].split('/')

        assert 0 == os.system(f'ip addr del {ip}/32 dev {dev}')

    else:
        # ip addr del dev tun0 local 10.211.1.253 peer 10.211.1.254
        os.execve('/sbin/ip', sys.argv, {})

else:
    print('BAD COMMAND LINE')
    print(cmdline)
    os.exit(1)
