#!/usr/bin/python

'''
#!/bin/sh

while : ; do
    sleep 60
    echo $(date +%s) $(grep eth0 /proc/net/dev)
done
'''

import matplotlib.pyplot

x = [(int(t), int(ib), int(ip), int(ob), int(op)) 
        for t, itfc, ib, ip, iA, iB, iC, iD, iE, iF, ob, op, oA, oB, oC, oD, oE, oF in 
            [line.split() for line in  open('SIZES').read().strip().split('\n') if line]      
    ]

(tLast, ibLast, ipLast, obLast, opLast), *x = x

t0 = tLast
ib0 = ibLast
ob0 = obLast

ts = []
ibs = []
ips = []
obs = []
ops = []

itotals = []
ototals = []

ipsizes = []
opsizes = []

for t, ib, ip, ob, op in x:

    seconds = t - tLast    
    
    ipkts = ip - ipLast
    opkts = op - opLast    
    
    ibytes = ib - ibLast
    obytes = ob - obLast
    
    ts.append((t - t0)/3600)
    
    ibs.append(ibytes/(seconds*1000*1000))
    obs.append(obytes/(seconds*1000*1000))
    
    ips.append(ipkts/seconds)
    ops.append(opkts/seconds)
                
    ipsizes.append(ibytes/ipkts if ipkts else 0)
    opsizes.append(obytes/opkts if opkts else 0)    
    
    itotals.append((ib - ib0)/(1000*1000*1000))
    ototals.append((ob - ob0)/(1000*1000*1000))

    tLast = t        
    ibLast = ib
    ipLast = ip
    obLast = ob
    opLast = op

figure, ((a, b), (c, d), (e, f), (g, h)) = matplotlib.pyplot.subplots(nrows=4, ncols=2)

#matplotlib.pyplot.title('BANDWITDH', fontsize=12)
#matplotlib.pyplot.xlabel('HOURS', fontsize=12)

# left=0, bottom=0, right=0, top=0
matplotlib.pyplot.subplots_adjust(wspace=0, hspace=0)

def piroca(axis, array):

    avg = sum(array) / len(array)
       
    #metade = sum(abs(x - avg) for x in array) / len(array)
    #sum((x != avg) for x in array)
    
    #metade = sum((avg - x) for x in array if avg > x) / sum(avg > x for x in array) 
    
    #base = avg - metade

    axis.scatter(ts, array, s=1, color='black')
    #axis.plot((ts[0], ts[-1]), (metade, metade))
    axis.plot((ts[0], ts[-1]), (avg, avg))
    axis.set_xlim(0)
    axis.set_ylim(min(array))
 
a.title.set_text('IN')
b.title.set_text('OUT')

a.set_ylabel('MB/S', fontsize=12)
c.set_ylabel('PKT/S', fontsize=12)
e.set_ylabel('AVG PKT SIZE', fontsize=12)
g.set_ylabel('TOTAL GB', fontsize=12)

e.set_xlabel('HOURS', fontsize=12)

b.yaxis.set_label_position("right")
d.yaxis.set_label_position("right")
f.yaxis.set_label_position("right")
h.yaxis.set_label_position("right")

b.yaxis.tick_right()
d.yaxis.tick_right()
f.yaxis.tick_right()
h.yaxis.tick_right()

piroca(a, ibs)
piroca(b, obs)
piroca(c, ips)
piroca(d, ops)
piroca(e, ipsizes)
piroca(f, opsizes)
piroca(g, itotals)
piroca(h, ototals)

matplotlib.pyplot.show()

# inTotal/outTotal
