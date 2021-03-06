import os
import time

os.sched_setaffinity(0, (0,))
os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(1))

nsFD = os.open('/proc/net/netstat', os.O_RDONLY)
devFD = os.open('/proc/net/dev', os.O_RDONLY)

ITFCS = []
lasts = [time.clock_gettime(time.CLOCK_BOOTTIME)]

nsFields0, nsValues0, nsFields1, nsValues1, _ = (line.split()[1:] for line in os.pread(nsFD, 65536, 0).decode().split('\n'))

lasts.extend(nsValues0)
lasts.extend(nsValues1)

for itfc, *values in (x.split() for x in os.pread(devFD, 65536, 0)[:-1].decode().split('\n')[2:]):

    assert itfc.endswith(':') and len(values) == 16

    ITFCS.append(itfc)
    lasts.extend(values)

lasts = tuple(map(int, lasts))

os.write(1, int(time.time()).to_bytes(8, 'little', signed=False))
os.write(1, '|'.join((*nsFields0, *nsFields1)).encode() + b'\x00')
os.write(1, '|'.join(itfc[:-1] for itfc in ITFCS).encode() + b'\x00')
os.write(1, b''.join((int(x).to_bytes(8, 'little', signed=False) for x in lasts)))

while True:

    values = [time.clock_gettime(time.CLOCK_BOOTTIME)]

    fields0, nsValues0, fields1, nsValues1, _ = (line.split()[1:] for line in os.pread(nsFD, 65536, 0).decode().split('\n'))

    assert fields0 == nsFields0
    assert fields1 == nsFields1

    values.extend(nsValues0)
    values.extend(nsValues1)

    itfcs = []

    for itfc, *values_ in (x.split() for x in os.pread(devFD, 65536, 0)[:-1].decode().split('\n')[2:]):
        itfcs.append(itfc)
        values.extend(values_)

    values = tuple(map(int, values))

    assert itfcs == ITFCS
    assert len(values) == len(lasts)

    os.write(1, b''.join((n - o).to_bytes(4, 'little', signed=False) for n, o in zip(values, lasts)))

    lasts = values

    time.sleep(30)
