#!/usr/bin/python

import os
import sys
from random import choice as CHOOSE
import socket

KNOWNS = '''
./download-file.py - ping.online.net                  80 5000Mo.dat
./download-file.py - ipv4.download.thinkbroadband.com 80 1GB.zip
./download-file.py - 187.108.112.23                   80 testebw/file2.test
./download-file.py - www.telmaxbr.net.br              80 download/dwl/702MEGA.zip
./download-file.py - ovh.net                          80 files/10Gio.dat
./download-file.py - speed.hetzner.de                 80 10GB.bin
./download-file.py - speedtest-nyc1.digitalocean.com  80 5gb.test
./download-file.py - speedtest.tpa.hivelocity.net     80 10GiB.file
./download-file.py - speedtest.nyc.hivelocity.net     80 10GiB.file
./download-file.py - ipv4.download.thinkbroadband.com 80 1GB.zip
./download-file.py - mirror.filearena.net             80 pub/speed/SpeedTest_2048MB.dat
./download-file.py - speed.hetzner.de                 80 10GB.bin
./download-file.py - speedtest-ams2.digitalocean.com  80 5gb.test
./download-file.py - speedtest-ams3.digitalocean.com  80 5gb.test
./download-file.py - speedtest-ca.turnkeyinternet.net 80 10000mb.bin
./download-file.py - speedtest-fra1.digitalocean.com  80 5gb.test
./download-file.py - speedtest-lon1.digitalocean.com  80 5gb.test
./download-file.py - speedtest-ny.turnkeyinternet.net 80 10000mb.bin
./download-file.py - speedtest-nyc1.digitalocean.com  80 5gb.test
./download-file.py - speedtest-nyc2.digitalocean.com  80 5gb.test
./download-file.py - speedtest-nyc3.digitalocean.com  80 5gb.test
./download-file.py - speedtest-sfo1.digitalocean.com  80 5gb.test
./download-file.py - speedtest-sfo2.digitalocean.com  80 5gb.test
./download-file.py - speedtest-sfo3.digitalocean.com  80 5gb.test
./download-file.py - speedtest-tor1.digitalocean.com  80 5gb.test
./download-file.py - speedtest.fra1.hivelocity.net    80 10GiB.file
./download-file.py - speedtest.nyc.hivelocity.net     80 10GiB.file
./download-file.py - speedtest.tele2.net              80 1000GB.zip
./download-file.py - speedtest.tpa.hivelocity.net     80 10GiB.file
'''

vpns = tuple(f'openvpn-{vpn}' for vpn in range(13))

proxies = tuple(range(8080, 8089))

if len(sys.argv) > 2:
    _, proxy, hostname, port, fpath = sys.argv
else:
    _, proxy, hostname, port, fpath = CHOOSE([line.strip().split() for line in KNOWNS.strip().split('\n')])

    if len(sys.argv) != 1:
        _, proxy = sys.argv

if proxy == '-':
    proxy = CHOOSE([*vpns, *proxies])
else:
    proxy = (proxy[1:] if proxy.startswith('@') else int(proxy))

# TODO: FIXME: SE NAO FOR PROXY/VPN, DEIXAR QUE SEJA IPv6
ip = CHOOSE([ip for sockFamily, *_, (ip, *_) in socket.getaddrinfo(hostname, 443) if sockFamily == socket.AF_INET])

print(proxy, ip, port, hostname, fpath)

os.close(0)
os.close(1)

sock = socket.socket()

if isinstance(proxy, int):
    sock.connect(('127.0.0.1', proxy))
    os.write(0, b''.join((b'\x04\x01', int(port).to_bytes(length=2, byteorder='big'), *(int(x).to_bytes(length=1, byteorder='big') for x in ip.split('.')), b'\x00')))
    os.read(0, 65536)
else:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, proxy.encode())
    sock.connect((ip, int(port)))

os.write(0, (
    f'GET /{fpath} HTTP/1.0\r\n'
    f'Connection: keep-alive\r\n'
    f'Host: {hostname}\r\n'
    f'\r\n'
    ).encode())
os.open('/dev/null', os.O_WRONLY)
os.set_inheritable(0, True)
os.set_inheritable(1, True)
os.execve('/bin/pv', ('pv', '--buffer-size', '67108864', '--timer', '--bytes', '--interval', '1', '--average-rate', '--rate'), {})
