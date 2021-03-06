#!/usr/bin/python

import os
import time

os.sched_setaffinity(0, (0,))
os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(1))

nsFD = os.open('/proc/net/netstat', os.O_RDONLY)
devFD = os.open('/proc/net/dev', os.O_RDONLY)

FIELDS = []
ITFCS = []

lasts = [time.clock_gettime(time.CLOCK_BOOTTIME)]

fields0, values0, fields1, values1, _ = (line.split()[1:] for line in os.pread(nsFD, 65536, 0).split(b'\n'))

FIELDS.extend(fields0)
FIELDS.extend(fields1)

lasts.extend(values0)
lasts.extend(values1)

for itfc, *values0 in (x.split() for x in os.pread(devFD, 65536, 0)[:-1].split(b'\n')[2:]):

    assert itfc.endswith(b':') and len(values0) == 16

    ITFCS.append(itfc)
    lasts.extend(values0)

lasts = tuple(map(int, lasts))

os.write(1, int(time.time()).to_bytes(8, 'little', signed=False))
os.write(1, b'\x01'.join(FIELDS) + b'\x00')
os.write(1, b'\x01'.join(itfc[:-1] for itfc in ITFCS) + b'\x00')
os.write(1, b''.join((int(x).to_bytes(8, 'little', signed=False) for x in lasts)))

FIELDS.extend(ITFCS)

while True:

    fields = []
    values = [time.clock_gettime(time.CLOCK_BOOTTIME)]

    fields0, values0, fields1, values1, _ = (line.split()[1:] for line in os.pread(nsFD, 65536, 0).split(b'\n'))

    fields.extend(fields0)
    fields.extend(fields1)

    values.extend(values0)
    values.extend(values1)

    for itfc, *values0 in (x.split() for x in os.pread(devFD, 65536, 0)[:-1].split(b'\n')[2:]):
        fields.append(itfc)
        values.extend(values0)

    values = tuple(map(int, values))

    assert fields == FIELDS
    assert len(values) == len(lasts)

    os.write(1, b''.join((n - o).to_bytes(4, 'little', signed=False) for n, o in zip(values, lasts)))

    lasts = values

    time.sleep(30)
