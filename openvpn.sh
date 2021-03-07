#!/bin/sh

# python-docutils

#cipher
#--disable-ofb-cfb
#(git clone --depth 1 https://github.com/OpenVPN/openvpn.git &&
    #cd openvpn &&
    #autoreconf -i &&
    #./configure \
        #--prefix=/usr \
        #--disable-selinux \
        #--disable-systemd \
        #--disable-pf \
        #--enable-iproute2 \
        #--enable-lz4 \
        #--enable-lzo \
        #--disable-plugins \
        #--enable-shared \
        #--disable-static \
        #--disable-debug \
            #&&
    #make -j16 &&
    #sudo make install
#)

ITFC0_ADDR=192.168.1.3
ITFC1_ADDR=192.168.200.250
ITFC2_ADDR=100.64.71.53

taskset -pac 0-1 $$

chrt --all-tasks --fifo --pid 99 $$

rm -f -- config auth pid log status
rm -r -f -- config auth pid log status
mkdir config auth pid log status

function profile() {
    id=${1}
    provider=${2}
    auth=${3}
    config=${4}
    #openvpn --mktun --dev-type tun --dev openvpn-${id}
    ip tuntap add mode tun openvpn-${id}
    ip rule add iif openvpn-${id} table $((500+${id}))
    ip rule add oif openvpn-${id} table $((500+${id}))
    cat providers/${provider}/auths/${auth} > auth/${id}
    (
        echo 'pull-filter ignore "sndbuf"'
        echo 'pull-filter ignore "rcvbuf"'
        echo 'pull-filter ignore "script-security"'
        echo 'pull-filter ignore "dhcp-option DNS"'
        echo 'sndbuf 134217728'
        echo 'rcvbuf 134217728'
        echo 'script-security 2'
        grep -v -E '^\s*(nobind|block-outside-dns|up|down|sndbuf|rcvbuf|script-security)(\s|#|$)' providers/${provider}/configs/${config}
    ) > config/${id}
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

chmod 0700 auth
chmod 0600 auth/*

#–socket-flags flags…
#–mark value
(while : ; do ip route flush dev openvpn-0 ; sleep 1 ; ip addr flush dev openvpn-0 ; sleep 1 ; openvpn --config config/0 --bind --local ${ITFC0_ADDR} --lport 500 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/0 --dev-type tun --dev openvpn-0 --writepid pid/0 --status status/0 60 --iproute ./ip.py ; sleep 1 ; done) > log/0 2>&1 &
(while : ; do ip route flush dev openvpn-1 ; sleep 1 ; ip addr flush dev openvpn-1 ; sleep 1 ; openvpn --config config/1 --bind --local ${ITFC0_ADDR} --lport 501 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/1 --dev-type tun --dev openvpn-1 --writepid pid/1 --status status/1 60 --iproute ./ip.py ; sleep 1 ; done) > log/1 2>&1 &
(while : ; do ip route flush dev openvpn-2 ; sleep 1 ; ip addr flush dev openvpn-2 ; sleep 1 ; openvpn --config config/2 --bind --local ${ITFC0_ADDR} --lport 502 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/2 --dev-type tun --dev openvpn-2 --writepid pid/2 --status status/2 60 --iproute ./ip.py ; sleep 1 ; done) > log/2 2>&1 &
(while : ; do ip route flush dev openvpn-3 ; sleep 1 ; ip addr flush dev openvpn-3 ; sleep 1 ; openvpn --config config/3 --bind --local ${ITFC0_ADDR} --lport 503 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/3 --dev-type tun --dev openvpn-3 --writepid pid/3 --status status/3 60 --iproute ./ip.py ; sleep 1 ; done) > log/3 2>&1 &
(while : ; do ip route flush dev openvpn-4 ; sleep 1 ; ip addr flush dev openvpn-4 ; sleep 1 ; openvpn --config config/4 --bind --local ${ITFC0_ADDR} --lport 504 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/4 --dev-type tun --dev openvpn-4 --writepid pid/4 --status status/4 60 --iproute ./ip.py ; sleep 1 ; done) > log/4 2>&1 &
(while : ; do ip route flush dev openvpn-5 ; sleep 1 ; ip addr flush dev openvpn-5 ; sleep 1 ; openvpn --config config/5 --bind --local ${ITFC2_ADDR} --lport 505 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/5 --dev-type tun --dev openvpn-5 --writepid pid/5 --status status/5 60 --iproute ./ip.py ; sleep 1 ; done) > log/5 2>&1 &
(while : ; do ip route flush dev openvpn-6 ; sleep 1 ; ip addr flush dev openvpn-6 ; sleep 1 ; openvpn --config config/6 --bind --local ${ITFC2_ADDR} --lport 506 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/6 --dev-type tun --dev openvpn-6 --writepid pid/6 --status status/6 60 --iproute ./ip.py ; sleep 1 ; done) > log/6 2>&1 &
(while : ; do ip route flush dev openvpn-7 ; sleep 1 ; ip addr flush dev openvpn-7 ; sleep 1 ; openvpn --config config/7 --bind --local ${ITFC2_ADDR} --lport 507 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/7 --dev-type tun --dev openvpn-7 --writepid pid/7 --status status/7 60 --iproute ./ip.py ; sleep 1 ; done) > log/7 2>&1 &
(while : ; do ip route flush dev openvpn-8 ; sleep 1 ; ip addr flush dev openvpn-8 ; sleep 1 ; openvpn --config config/8 --bind --local ${ITFC0_ADDR} --lport 508 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/8 --dev-type tun --dev openvpn-8 --writepid pid/8 --status status/8 60 --iproute ./ip.py ; sleep 1 ; done) > log/8 2>&1 &
(while : ; do ip route flush dev openvpn-9 ; sleep 1 ; ip addr flush dev openvpn-9 ; sleep 1 ; openvpn --config config/9 --bind --local ${ITFC0_ADDR} --lport 509 --sndbuf 134217728 --rcvbuf 134217728 --auth-user-pass auth/9 --dev-type tun --dev openvpn-9 --writepid pid/9 --status status/9 60 --iproute ./ip.py ; sleep 1 ; done) > log/9 2>&1 &
