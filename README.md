# uvrrpd

uvrrpd is a VRRP daemon written in C, providing a full implementation of
VRRPv2 (rfc3768) and VRRPv3 (rfc5798), with IPv4 and IPv6 support.

uvrrpd is a project hosted at [Evolix's forge](https://forge.evolix.org/projects/uvrrpd)

uvrrpd is written for GNU/Linux and use macvlan in order to derive multiple
virtual NICs (virtual VRRP mac) from a single physical NIC.

uvrrpd is a simply a VRRP state machine, and a script (*vrrp_switch.sh*) is in
charge to create or destroy Virtual VRRP interfaces.

uvrrpd is designed to run a single VRRP instance, but you can run multiple
instances of uvrrpd, each of them with a different VRRP id, on the same or
different physical NIC.

Simple text authentication from deprecated RFC2332 may be used while running
uvrrpd in version 2 (rfc3768), but not in version 3 (rfc5798).

It provides a network topology update by sending :
- an ARP gratuitous packet for each Virtual VRRP IPv4 address specified in the
  VRRP instance,
- an NDP neighbour advertisement for each Virtual VRRP IPv6 address.

## Building

uvrrpd uses the autotools, so to build it from the released tarball, follow the
usual procedure.

```bash
./configure
make
sudo make install
```

If building from the git sources, run:
```bash
autoreconf -i
```
before that.

That's all. You need the binary `uvrrpd` and the shell script *vrrp_switch.sh*
to start playing, they are installed in $prefix/sbin, the default prefix being
/usr/local.

## Usage

```bash
$ ./uvrrpd -h
Usage: uvrrpd -v vrid -i ifname [OPTIONS] VIP1 [… VIPn]

Mandatory options:
  -v, --vrid vrid           Virtual router identifier
  -i, --interface iface     Interface
  VIP                       Virtual IP(s), 1 to 255 VIPs

Optional arguments:
  -p, --priority prio       Priority of VRRP Instance, (0-255, default 100)
  -t, --time delay          Time interval between advertisements
                            Seconds in VRRPv2 (default 1s),
                            Centiseconds in VRRPv3 (default 100cs)
  -P, --preempt on|off      Switch preempt (default on)
  -r, --rfc version         Specify protocol 'version'
                            2 (VRRPv2, RFC3768) by default,
                            3 (VRRPv3, RFC5798)
  -6, --ipv6                IPv6 support, (only in VRRPv3)
  -a, --auth pass           Simple text password (only in VRRPv2)
  -f, --foreground          Execute uvrrpd in foreground
  -s, --script              Path of hook script (default /etc/uvrrpd/uvrrpd-switch.sh)
  -F  --pidfile		    Create pid file 'name'
                            Default /var/run/uvrrp_${vrid}.pid
  -d, --debug
  -h, --help
```

### Signals

* `SIGHUP` : force uvrrpd to switch to init state
* `SIGUSR1`|`SIGUSR2` : dump VRRP instance informations

### Log

LOG_DAEMON facility

*vrrp_switch.sh* maintain a state file of the current instance in /tmp/state.vrrp_${vrid}_${ifname}

## Examples

*uvrrpd must be run as root.*

* Start a VRRP instance on eth0 interface with VRID 42, default priority (100), 
with *vrrp_switch.sh* in */usr/share/uvrrpd* directory (arbitrary choice).

```bash
# ./uvrrpd -v 42 -i eth0 -s /usr/share/uvrrpd/vrrp_switch.sh 10.0.0.254
#
```

In our example, no other VRRP instance, we are the master and we can see the
new VRRP interface with the VIP *10.0.0.254* and the virtual VRRP mac address
*00:00:5e:00:01:2a* :

```bash
# ifconfig
eth0      Link encap:Ethernet  HWaddr 52:54:00:4f:48:3f  
          inet addr:10.0.0.1  Bcast:10.0.0.255  Mask:255.255.255.0
          inet6 addr: fe80::5054:ff:fe4f:483f/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:4935 errors:0 dropped:0 overruns:0 frame:0
          TX packets:3835 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000 
          RX bytes:965166 (942.5 KiB)  TX bytes:613308 (598.9 KiB)

eth0_42   Link encap:Ethernet  HWaddr 00:00:5e:00:01:2a  
          inet addr:10.0.0.254  Bcast:0.0.0.0  Mask:255.255.255.255
          inet6 addr: fe80::200:5eff:fe00:12a/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:12 errors:0 dropped:0 overruns:0 frame:0
          TX packets:6 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:0 
          RX bytes:3217 (3.1 KiB)  TX bytes:520 (520.0 B)
[...]
```

See logs :

```bash
# tail -f /var/log/daemon.log
[...]
Sep 12 09:04:55 debian uvrrpd[2966]: vrid 42 :: init
Sep 12 09:04:55 debian uvrrpd[2966]: vrid 42 :: init -> backup
Sep 12 09:04:58 debian uvrrpd[2966]: vrid 42 :: masterdown_timer expired
Sep 12 09:04:58 debian uvrrpd[2966]: vrid 42 :: backup -> master
```

and /tmp/state.vrrp_42_eth0 : 

```bash
# cat /tmp/state.vrrp_42_eth0 
state           master
vrid            42
ifname          eth0
priority        100
adv_int         1
naddr           1
ips             10.0.0.254
```

You can start an another VRRP instance on another GNU/Linux box or a router with VRRP support, with the same VRID and different priority.

* uvrrpd support IPv6 (RFC5798) :

```bash
#  ./uvrrpd -v 42 -i eth0 -p 90 -6 fe80::fada/64
```

* Multiple VIPs could be specified for a single VRRP instance (up to 255 VIPs) :

```bash
# ./uvrrpd -v 42 -i eth0 10.0.0.69 10.0.0.80
```

## TODOs

* make more tests
* autoconf/autohell
* add features like interface monitoring...
* init scripts
* packaging

Any suggestions, ideas, patches or whatever are welcome and will be greatly
appreciated !

