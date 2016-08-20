#!/bin/sh

state=$1
vrid=$2
ifname=$3
priority=$4
adv_int=$5
naddr=$6
family=$7
ips=$8

echo "state\t\t$state
vrid\t\t$vrid
ifname\t\t$ifname
priority\t$priority
adv_int\t\t$adv_int
naddr\t\t$naddr
ips\t\t$ips" > /tmp/state.vrrp_${vrid}_${ifname}

interface=${ifname}_${vrid}

echo $ips

case "$state" in 

    "init" )
        # adjust sysctl
        sysctl -w net.ipv4.conf.all.rp_filter=0
        sysctl -w net.ipv4.conf.all.arp_ignore=1
        sysctl -w net.ipv4.conf.all.arp_announce=2
        sysctl -w net.ipv4.conf.default.rp_filter=0
        sysctl -w net.ipv4.conf.default.arp_ignore=1
        sysctl -w net.ipv4.conf.default.arp_announce=2

        if [ -f /proc/net/if_inet6 ]; then
            sysctl -w net.ipv6.conf.all.autoconf=0
            sysctl -w net.ipv6.conf.all.accept_ra=0
            sysctl -w net.ipv6.conf.all.forwarding=1
            sysctl -w net.ipv6.conf.default.autoconf=0
            sysctl -w net.ipv6.conf.default.accept_ra=0
            sysctl -w net.ipv6.conf.default.forwarding=1
        fi

        ;;

    "master" )
        # create macvlan interface
        HEXA_VRID=$(printf %x $vrid)
        ip link add link $ifname address 00:00:5E:00:01:$HEXA_VRID $interface type macvlan

        # set virtual ips addresses
        OIFS=$IFS
        IFS=','

        for ip in $ips; do
            ip -$family addr add $ip dev $interface 
        done
        ip link set dev $interface up
	

        IFS=$OIFS

        ;;

    "backup" )
        # destroy macvlan interface
        ip link del $interface

        ;;
esac

exit 0
