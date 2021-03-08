#!/usr/bin/python

import os
import sys
from random import choice as CHOOSE
import socket

KNOWNS = '''
./download-file.py 8080 ping.online.net 80 /5000Mo.dat
./download-file.py 8080 ipv4.download.thinkbroadband.com 80 /1GB.zip
./download-file.py 8080 187.108.112.23 80 /testebw/file2.test
./download-file.py 8080 www.telmaxbr.net.br 80 /download/dwl/702MEGA.zip
./download-file.py 8080 ovh.net 80 /files/10Gio.dat
./download-file.py 8080 speed.hetzner.de 80 /10GB.bin
./download-file.py 8080 speedtest-nyc1.digitalocean.com 80 /5gb.test
'''

VPNS = [f'@openvpn-{vpn}' for vpn in range(13)]

if len(sys.argv) > 2:
    _, PROXY, HOSTNAME, PORT, URI = sys.argv
else:
    _, PROXY, HOSTNAME, PORT, URI = CHOOSE([line.strip().split() for line in KNOWNS.strip().split('\n')])

    if len(sys.argv) == 1:
        PROXY = CHOOSE(VPNS)

# TODO: FIXME: SE NAO FOR PROXY/VPN, DEIXAR QUE SEJA IPv6
IP = CHOOSE([ip for sockFamily, *_, (ip, *_) in socket.getaddrinfo(HOSTNAME, 443) if sockFamily == socket.AF_INET])

print(PROXY, IP, PORT, HOSTNAME, URI)

os.close(0)
os.close(1)

sock = socket.socket()

if PROXY.startswith('@'):
    if PROXY == '@':
        PROXY = CHOOSE(VPNS)
    else:
        PROXY = PROXY[1:]
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, PROXY.encode())
    sock.connect((IP, int(PORT)))
else:
    sock.connect(('127.0.0.1', int(PROXY)))

    # SEND PROXY CONNECT COMMAND
    os.write(0, b''.join((b'\x04\x01', int(PORT).to_bytes(length=2, byteorder='big'), *(int(x).to_bytes(length=1, byteorder='big') for x in IP.split('.')), b'\x00')))

    # WAIT CONNECTION TO BE ESTABLISHED
    assert os.read(0, 65536)

# SEND REQUEST
os.write(0, (
    f'GET {URI} HTTP/1.0\r\n'
    f'Connection: keep-alive\r\n'
    f'Host: {HOSTNAME}\r\n'
    f'\r\n'
    ).encode())

os.open('/dev/null', os.O_WRONLY)

# NOW RECEIVE IT
os.set_inheritable(0, True)
os.set_inheritable(1, True)

os.execve('/bin/pv', ('pv', '--buffer-size', '67108864', '--timer', '--bytes', '--interval', '1', '--average-rate', '--rate'), {})
