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

taskset -pac 2-64 $$

chrt --all-tasks --fifo --pid 99 $$

rm -f    -- config auth pid log status
rm -r -f -- config auth pid log status
mkdir    -- config auth pid log status

chmod 0700 auth

function VPN() {

    id=${1}
    itfc=${2}
    provider=${3}
    auth=${4}
    config=${5}

    # TODO: FIXME: E QUANTO AO MTU?
    #openvpn --mktun --dev-type tun --dev openvpn-${id}
    ip tuntap add mode tun openvpn-${id}

    ip rule add iif openvpn-${id} table $((500+${id}))
    ip rule add oif openvpn-${id} table $((500+${id}))

    cat providers/${provider}/auths/${auth} > auth/${id}

    chmod 0600 auth/${id}

    (
        echo 'pull-filter ignore "sndbuf"'
        echo 'pull-filter ignore "rcvbuf"'
        echo 'pull-filter ignore "script-security"'
        echo 'pull-filter ignore "dhcp-option DNS"'
        echo 'sndbuf 134217728'
        echo 'rcvbuf 134217728'
        echo 'script-security 2'
        grep -v -E '^\s*(bind|nobind|local|lport|block-outside-dns|up|down|sndbuf|rcvbuf|script-security|iproute|status|writepid|dev|dev-type|log|auth-user-pass)(\s|#|$)' providers/${provider}/configs/${config}
    ) > config/${id}

    (while : ; do
        ip route flush dev openvpn-${id} ; sleep 1
        ip addr  flush dev openvpn-${id} ; sleep 1
        #–socket-flags flags…
        #–mark value
        openvpn \
            --config config/${id} \
            --bind \
            --local ${itfc} \
            --lport $((500+${id})) \
            --sndbuf 134217728 \
            --rcvbuf 134217728 \
            --auth-user-pass auth/${id} \
            --dev-type tun \
            --dev openvpn-${id} \
            --writepid pid/${id} \
            --status status/${id} 60 \
            --iproute ./ip.py
        sleep 2
    done) > log/${id} 2>&1 &
}

#
ISP0_ADDR=192.168.1.3
ISP1_ADDR=192.168.200.250
ISP2_ADDR=100.64.71.53

#   ID  ADDRESS        PROVIDER    AUTH    CONFIG
VPN  0  ${ISP0_ADDR}   surfshark   will    br-sao.prod.surfshark.com_udp.ovpn
VPN  1  ${ISP0_ADDR}   surfshark   will    us-nyc.prod.surfshark.com_udp.ovpn
VPN  2  ${ISP0_ADDR}   surfshark   will    de-fra.prod.surfshark.com_udp.ovpn
VPN  3  ${ISP0_ADDR}   surfshark   will    de-ber.prod.surfshark.com_udp.ovpn
VPN  4  ${ISP0_ADDR}   protonvpn   will    nl-free-09.protonvpn.com.udp.ovpn
VPN  5  ${ISP0_ADDR}   surfshark   will    py-asu.prod.surfshark.com_udp.ovpn
VPN  6  ${ISP0_ADDR}   surfshark   will    us-tpa.prod.surfshark.com_udp.ovpn
VPN  7  ${ISP0_ADDR}   surfshark   will    es-mad.prod.surfshark.com_udp.ovpn
VPN  8  ${ISP0_ADDR}   surfshark   will    fr-par.prod.surfshark.com_udp.ovpn
VPN  9  ${ISP0_ADDR}   surfshark   will    ca-tor.prod.surfshark.com_udp.ovpn
VPN 10  ${ISP0_ADDR}   protonvpn   will    us-free-05.protonvpn.com.udp.ovpn
VPN 11  ${ISP0_ADDR}   surfshark   will    ar-bua.prod.surfshark.com_udp.ovpn
VPN 12  ${ISP0_ADDR}   surfshark   will    us-hou.prod.surfshark.com_udp.ovpn
