#!/bin/sh

set -e -x

VPNS_DIR=/home/speedyb0y/surfshark

cd ${VPNS_DIR}

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

[ ! -h config ]
[ ! -h pid ]
[ ! -h log ]
[ ! -h status ]

rm -r -f -- config pid log status
mkdir    -- config pid log status

id=0
isp=0

function VPN () {

    laddr=${1}
    provider=${2}
    auth=${3}
    config=${4}

    if [ ${id} == 300 ] ; then
        return
    fi

    if [ ${laddr} == "-" ] ; then

        isp=$((isp+1))

        if [ $[${isp} % 10] -le 4 ] ; then
            laddr=192.168.1.3
        elif [ $[${isp} % 10] -le 7 ] ; then
            laddr=192.168.200.250
        else
            laddr=100.64.66.15
        fi
    fi

    vpn_itfc=openvpn-${id}

    table=$[5000+${id}]

    # TODO: FIXME: E QUANTO AO MTU?
    #openvpn --mktun --dev-type tun --dev ${vpn_itfc}
    ip tuntap add mode tun ${vpn_itfc} || :

    echo 0 > /proc/sys/net/ipv6/conf/${vpn_itfc}/autoconf
    echo 1 > /proc/sys/net/ipv6/conf/${vpn_itfc}/disable_ipv6

    ip link set dev ${vpn_itfc} up

    ip rule flush table ${table}
    ip route flush table ${table} || :
    ip route flush dev ${vpn_itfc}
    ip addr flush dev ${vpn_itfc}

    ip rule add iif ${vpn_itfc} table ${table}
    ip rule add oif ${vpn_itfc} table ${table}

    config=providers/${provider}/configs/${config}

    if ! grep -q -E '^\s*proto\s*(tcp|udp)\s*$' ${config} ; then
        echo "PROTOCOL NOT SPECIFIED IN ${config}"
        exit 1
    fi

    path_log=log/${id}
    path_config=config/${id}
    path_status=status/${id}
    path_pid=pid/${id}

    lport=$[65000+${id}]

    # TODO: FIXME: NAO PODE TER COMP-LZO

    (
        echo 'pull-filter ignore "sndbuf"'
        echo 'pull-filter ignore "rcvbuf"'
        echo 'pull-filter ignore "script-security"'
        echo 'pull-filter ignore "dhcp-option DNS"'
        echo 'sndbuf 134217728'
        echo 'rcvbuf 134217728'
        echo 'script-security 2'
        echo 'verb 4'
        echo 'bind'
        echo 'dev-type tun'
        echo 'ping-restart 120' # TODO: FIXME: NAO PODE SER MENOR DO QUE O PING INTERVAL
        if [ ${auth} != "-" ] ; then
            echo "auth-user-pass config/${id}-auth"
            cat providers/${provider}/auths/${auth} > ${path_config}-auth
            chmod 0600 ${path_config}-auth
        fi
        grep -v -E '^\s*(comp-lzo|verb|bind|nobind|local|lport|block-outside-dns|up|down|sndbuf|rcvbuf|script-security|iproute|status|writepid|dev|dev-type|log|auth-user-pass|tls-exit)(\s|#|$)' ${config} || :
    ) > ${path_config}

    ( set +e
        while : ; do
            openvpn --config ${path_config} --local ${laddr} --lport ${lport} --dev ${vpn_itfc} --writepid ${path_pid} --status ${path_status} 60 --iproute ./ip.py
            rm -f -- ${path_pid} ${path_status}
            ip route flush table ${table}
            ip route flush dev ${vpn_itfc}
            ip addr  flush dev ${vpn_itfc}
            sleep 5
        done
    ) > ${path_log} 2>&1 &

    id=$[${id}+1]
}

#   ADDRESS     PROVIDER    AUTH    CONFIG
VPN -           surfshark   will    ae-dub.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    al-tia.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ar-bua.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    at-vie.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    au-adl.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    au-bne.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    au-mel.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    au-per.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    au-syd.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    az-bak.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ba-sjj.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    be-bru.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    bg-sof.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    br-sao.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ca-mon.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ca-tor-mp001.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ca-tor.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ca-van.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ch-zur.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    cl-san.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    co-bog.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    cr-sjn.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    cy-nic.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    cz-prg.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    de-ber.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    de-fra.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    de-muc.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    de-nue.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    dk-cph.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    ee-tll.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    es-bcn.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    us-sfo-mp001.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    us-sfo.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    us-slc.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    us-stl.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    us-tpa.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    vn-hcm.prod.surfshark.com_udp.ovpn
VPN -           surfshark   will    za-jnb.prod.surfshark.com_udp.ovpn
#VPN 25  ${ISP0_ADDR}   protonvpn   will    nl-free-01.protonvpn.com.udp.ovpn
#VPN 26  ${ISP0_ADDR}   protonvpn   will    nl-free-02.protonvpn.com.udp.ovpn
VPN -           protonvpn   will    nl-free-03.protonvpn.com.udp.ovpn
#VPN 28  ${ISP0_ADDR}   protonvpn   will    nl-free-04.protonvpn.com.udp.ovpn
#VPN 29  ${ISP0_ADDR}   protonvpn   will    nl-free-05.protonvpn.com.udp.ovpn
#VPN 30  ${ISP0_ADDR}   protonvpn   will    nl-free-06.protonvpn.com.udp.ovpn
#VPN 31  -           protonvpn   will    nl-free-07.protonvpn.com.udp.ovpn
#VPN 32  -           protonvpn   will    nl-free-08.protonvpn.com.udp.ovpn
#VPN 33  -           protonvpn   will    nl-free-09.protonvpn.com.udp.ovpn
#VPN 34  -           protonvpn   will    us-free-03.protonvpn.com.udp.ovpn
#VPN 35  -           protonvpn   will    us-free-05.protonvpn.com.udp.ovpn
VPN -           vpn-gate   -       C60D24B3
VPN -           vpn-gate   -       DB642585

# PARA VPNS INSTÁVEIS
#echo 'tls-exit'
#echo "ping-exit 180"
