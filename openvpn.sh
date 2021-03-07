#!/bin/sh

# python-docutils

# git clone --depth 1 https://github.com/OpenVPN/openvpn.git
# autoreconf -i
# ./configure --prefix=/usr --disable-selinux --disable-systemd --disable-pf --enable-iproute2 --disable-lz4 --disable-lzo --disable--plugins --enable-shared --disable-static --disable-debug
# make -j 16
# sudo make install

ITFC0_ADDR=192.168.1.3
ITFC1_ADDR=192.168.200.250
ITFC2_ADDR=100.64.71.53

taskset -pac 0-1 $$

chrt --all-tasks --fifo --pid 99 $$

rm -f -- config auth pid log
rm -r -f -- config auth pid log
mkdir config auth pid log

function profile() {
    id=${1}
    provider=${2}
    auth=${3}
    config=${4}
    openvpn --mktun --dev-type tun --dev openvpn-${id}
    cat \
        providers/${provider}/auths/${auth} > auth/${id}
    grep -v 'nobind' \
        providers/${provider}/configs/${config} > config/${id}
}

profile 0 surfshark will   br-sao.prod.surfshark.com_udp.ovpn
profile 1 surfshark will   us-nyc.prod.surfshark.com_udp.ovpn
profile 2 surfshark will   de-fra.prod.surfshark.com_udp.ovpn
profile 3 surfshark will   de-ber.prod.surfshark.com_udp.ovpn
profile 4 protonvpn will   nl-free-09.protonvpn.com.udp.ovpn
profile 5 surfshark will   py-asu.prod.surfshark.com_udp.ovpn
profile 6 surfshark will   us-tpa.prod.surfshark.com_udp.ovpn
profile 7 surfshark will   ca-tor.prod.surfshark.com_udp.ovpn
profile 8 protonvpn will   us-free-05.protonvpn.com.udp.ovpn
profile 9 surfshark will   ar-bua.prod.surfshark.com_udp.ovpn

(while : ; do ip route flush dev openvpn-0 ; sleep 1 ; ip addr flush dev openvpn-0 ; sleep 1 ; openvpn --config config/0 --bind --local ${ITFC0_ADDR} --lport 500 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/0 --dev openvpn-0 --writepid pid/0 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-1 ; sleep 1 ; ip addr flush dev openvpn-1 ; sleep 1 ; openvpn --config config/1 --bind --local ${ITFC0_ADDR} --lport 501 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/1 --dev openvpn-1 --writepid pid/1 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-2 ; sleep 1 ; ip addr flush dev openvpn-2 ; sleep 1 ; openvpn --config config/2 --bind --local ${ITFC0_ADDR} --lport 502 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/2 --dev openvpn-2 --writepid pid/2 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-3 ; sleep 1 ; ip addr flush dev openvpn-3 ; sleep 1 ; openvpn --config config/3 --bind --local ${ITFC0_ADDR} --lport 503 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/3 --dev openvpn-3 --writepid pid/3 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-4 ; sleep 1 ; ip addr flush dev openvpn-4 ; sleep 1 ; openvpn --config config/4 --bind --local ${ITFC0_ADDR} --lport 504 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/4 --dev openvpn-4 --writepid pid/4 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-5 ; sleep 1 ; ip addr flush dev openvpn-5 ; sleep 1 ; openvpn --config config/5 --bind --local ${ITFC2_ADDR} --lport 505 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/5 --dev openvpn-5 --writepid pid/5 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-6 ; sleep 1 ; ip addr flush dev openvpn-6 ; sleep 1 ; openvpn --config config/6 --bind --local ${ITFC2_ADDR} --lport 506 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/6 --dev openvpn-6 --writepid pid/6 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-7 ; sleep 1 ; ip addr flush dev openvpn-7 ; sleep 1 ; openvpn --config config/7 --bind --local ${ITFC2_ADDR} --lport 507 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/7 --dev openvpn-7 --writepid pid/7 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-8 ; sleep 1 ; ip addr flush dev openvpn-8 ; sleep 1 ; openvpn --config config/8 --bind --local ${ITFC0_ADDR} --lport 508 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/8 --dev openvpn-8 --writepid pid/8 --iproute ./ip.py ; sleep 1 ; done) &
(while : ; do ip route flush dev openvpn-9 ; sleep 1 ; ip addr flush dev openvpn-9 ; sleep 1 ; openvpn --config config/9 --bind --local ${ITFC0_ADDR} --lport 509 --sndbuf $[128*1024*1024] --rcvbuf $[128*1024*1024] --script-security 2 --auth-user-pass auth/9 --dev openvpn-9 --writepid pid/9 --iproute ./ip.py ; sleep 1 ; done) &
