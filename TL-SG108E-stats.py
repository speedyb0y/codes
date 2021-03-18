#!/usr/bin/python
#
# TL-SG108E
#

import sys
import re
import socket

sock = socket.socket()

sock.connect(('192.168.200.100', 80))
sock.send(b'GET /PortStatisticsRpm.htm HTTP/1.1\r\n\r\n')

response = sock.recv(65536)
response = response[response.index(b'all_info = {')+11:]
response = response[:response.index(b'}')]

(
    s0, s1, s2, s3, s4, s5, s6, s7, _, _,
    c0, c1, c2, c3, c4, c5, c6, c7, _, _,
    tg0, tb0, rg0, rb0,
    tg1, tb1, rg1, rb1,
    tg2, tb2, rg2, rb2,
    tg3, tb3, rg3, rb3,
    tg4, tb4, rg4, rb4,
    tg5, tb5, rg5, rb5,
    tg6, tb6, rg6, rb6,
    tg7, tb7, rg7, rb7,
    _, _
) = map(int, re.findall(b'[0-9]{1,}', response))

s0, s1, s2, s3, s4, s5, s6, s7 = (('DISABLED', 'ENABLED')[s] for s in (s0, s1, s2, s3, s4, s5, s6, s7))
c0, c1, c2, c3, c4, c5, c6, c7 = (('LINK DOWN', '?', '?', '?', '?', '100 FULL', '1000 FULL')[c] for c in (c0, c1, c2, c3, c4, c5, c6, c7))

print((
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d\n'
        '%15s %15s %14d %14d %14d %14d'
    ) % (
        s0, c0, tg0, tb0, rg0, rb0,
        s1, c1, tg1, tb1, rg1, rb1,
        s2, c2, tg2, tb2, rg2, rb2,
        s3, c3, tg3, tb3, rg3, rb3,
        s4, c4, tg4, tb4, rg4, rb4,
        s5, c5, tg5, tb5, rg5, rb5,
        s6, c6, tg6, tb6, rg6, rb6,
        s7, c7, tg7, tb7, rg7, rb7,
    ))
