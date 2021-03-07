#!/bin/sh

# python-docutils

# git clone --depth 1 https://github.com/OpenVPN/openvpn.git
# autoreconf -i
# ./configure --prefix=/usr --disable-selinux --disable-systemd --disable-pf --enable-iproute2 --disable-lz4 --disable-lzo --disable--plugins --enable-shared --disable-static --disable-debug
# make -j 16
# sudo make install

#!/bin/sh

ITFC0_ADDR=192.168.1.3
ITFC1_ADDR=192.168.200.250
ITFC2_ADDR=100.64.71.53

taskset -pac 0-1 $$

chrt --all-tasks --fifo --pid 99 $$

openvpn --mktun --dev-type tun --dev openvpn-0
openvpn --mktun --dev-type tun --dev openvpn-1
openvpn --mktun --dev-type tun --dev openvpn-2
openvpn --mktun --dev-type tun --dev openvpn-3
openvpn --mktun --dev-type tun --dev openvpn-4
openvpn --mktun --dev-type tun --dev openvpn-5
openvpn --mktun --dev-type tun --dev openvpn-6
openvpn --mktun --dev-type tun --dev openvpn-7
openvpn --mktun --dev-type tun --dev openvpn-8
openvpn --mktun --dev-type tun --dev openvpn-9

ln -s -f -n br-sao.prod.surfshark.com_udp.ovpn config-0
ln -s -f -n us-nyc.prod.surfshark.com_udp.ovpn config-1
ln -s -f -n de-fra.prod.surfshark.com_udp.ovpn config-2
ln -s -f -n de-ber.prod.surfshark.com_udp.ovpn config-3
ln -s -f -n nl-free-09.protonvpn.com.udp.ovpn  config-4
ln -s -f -n py-asu.prod.surfshark.com_udp.ovpn config-5
ln -s -f -n us-tpa.prod.surfshark.com_udp.ovpn config-6
ln -s -f -n ca-tor.prod.surfshark.com_udp.ovpn config-7
ln -s -f -n us-free-05.protonvpn.com.udp.ovpn  config-8
ln -s -f -n ar-bua.prod.surfshark.com_udp.ovpn config-9

(while : ; do ip route flush dev openvpn-0 ; sleep 1 ; ip addr flush dev openvpn-0 ; sleep 1 ; openvpn --config config-0 --bind --local ${ITFC0_ADDR} --lport 500 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-0 --writepid openvpn-0-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-1 ; sleep 1 ; ip addr flush dev openvpn-1 ; sleep 1 ; openvpn --config config-1 --bind --local ${ITFC0_ADDR} --lport 501 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-1 --writepid openvpn-1-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-2 ; sleep 1 ; ip addr flush dev openvpn-2 ; sleep 1 ; openvpn --config config-2 --bind --local ${ITFC0_ADDR} --lport 502 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-2 --writepid openvpn-2-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-3 ; sleep 1 ; ip addr flush dev openvpn-3 ; sleep 1 ; openvpn --config config-3 --bind --local ${ITFC0_ADDR} --lport 503 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-3 --writepid openvpn-3-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-4 ; sleep 1 ; ip addr flush dev openvpn-4 ; sleep 1 ; openvpn --config config-4 --bind --local ${ITFC0_ADDR} --lport 504 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-4 --writepid openvpn-4-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-5 ; sleep 1 ; ip addr flush dev openvpn-5 ; sleep 1 ; openvpn --config config-5 --bind --local ${ITFC2_ADDR} --lport 505 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-5 --writepid openvpn-5-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-6 ; sleep 1 ; ip addr flush dev openvpn-6 ; sleep 1 ; openvpn --config config-6 --bind --local ${ITFC2_ADDR} --lport 506 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-6 --writepid openvpn-6-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-7 ; sleep 1 ; ip addr flush dev openvpn-7 ; sleep 1 ; openvpn --config config-7 --bind --local ${ITFC2_ADDR} --lport 507 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-7 --writepid openvpn-7-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-8 ; sleep 1 ; ip addr flush dev openvpn-8 ; sleep 1 ; openvpn --config config-8 --bind --local ${ITFC0_ADDR} --lport 508 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-8 --writepid openvpn-8-pid --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-9 ; sleep 1 ; ip addr flush dev openvpn-9 ; sleep 1 ; openvpn --config config-9 --bind --local ${ITFC0_ADDR} --lport 509 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass pass --dev openvpn-9 --writepid openvpn-9-pid --iproute ./ip.py ; sleep 1 ; done) &
