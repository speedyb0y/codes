import os
import time

fd = os.open('/proc/net/dev', os.O_RDONLY)

itfcs = tuple(line.split()[0] for line in os.pread(fd, 65536, 0)[:-1].decode().split('\n')[2:])

os.write(1, ('|'.join(itfc[:-1] for itfc in itfcs) + '\n').encode())

while True:

    # TODO: FIXME: USAR MONOTONIC TIME
    values = [time.time()]

    for itfc_, x in zip(itfcs, os.pread(fd, 65536, 0)[:-1].decode().split('\n')[2:]):

        itfc, ib, ip, ie, iD, iC, iD, iE, iF, ob, op, oe, od, oC, oD, oE, oF = x.split()

        assert itfc == itfc_

        values.extend((ib, ip, ie, iD, ob, op, oe, od))

    os.write(1, b''.join((int(x).to_bytes(length=8, byteorder='little') for x in values)))

    time.sleep(30)
