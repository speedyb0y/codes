import os
import time

fd = os.open('/proc/net/dev', os.O_RDONLY)

while True:

    now = time.time()
    
    buff = os.pread(fd, 65536, 0).decode().split('\n')

    itfc, ib, ip, ie, iD, iC, iD, iE, iF, ob, op, oe, od, oC, oD, oE, oF = buff[3].split()

    assert itfc == 'enp1s0:'

    os.write(1, b''.join((int(x).to_bytes(length=8, byteorder='little') for x in (now, ib, ip, ie, iD, ob, op, oe, od))))

    time.sleep(30)
