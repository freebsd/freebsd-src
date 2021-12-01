#!/bin/sh

# keep old /etc
mount tmpfs -t tmpfs /tmp
mkdir /tmp/etc
mount --bind /etc /tmp/etc
# mount all kinds of things
mount tmpfs -t tmpfs /etc
# we need our own /dev/rfkill, and don't want device access
mount tmpfs -t tmpfs /dev
# some sockets go into /var/run, and / is read-only
mount tmpfs -t tmpfs /var/run
mount proc -t proc /proc
mount sysfs -t sysfs /sys
# needed for tracing
mount debugfs -t debugfs /sys/kernel/debug

mkdir /tmp/wireshark-share
mount --bind /usr/share/wireshark /tmp/wireshark-share
mount tmpfs -t tmpfs /usr/share/wireshark

# for inside telnet
mkdir /dev/pts
mount devpts -t devpts /dev/pts

export PATH=/usr/sbin:$PATH
export HOME=/tmp

# reboot on any sort of crash
sysctl kernel.panic_on_oops=1
sysctl kernel.panic=1

# get extra command line variables from /proc/cmdline
TESTDIR=$(sed 's/.*testdir=\([^ ]*\) .*/\1/' /proc/cmdline)
TIMEWARP=$(sed 's/.*timewarp=\([^ ]*\) .*/\1/' /proc/cmdline)
EPATH=$(sed 's/.*EPATH=\([^ ]*\) .*/\1/' /proc/cmdline)
TELNET=$(sed 's/.*TELNET=\([^ ]*\) .*/\1/' /proc/cmdline)
ARGS=$(sed 's/.*ARGS=\([^ ]*\)\( \|$\).*/\1/' /proc/cmdline)
LOGDIR=$(sed 's/.*LOGDIR=\([^ ]*\)\( \|$\).*/\1/' /proc/cmdline)

mount --bind "$TESTDIR/vm/regdb/" /lib/firmware

# create /dev entries we need
mknod -m 660 /dev/ttyS0 c 4 64
mknod -m 666 /dev/ptmx c 5 2
mknod -m 660 /dev/random c 1 8
mknod -m 660 /dev/urandom c 1 9
mknod -m 666 /dev/null c 1 3
mknod -m 666 /dev/kmsg c 1 11
test -f /sys/class/misc/rfkill/dev && \
	mknod -m 660 /dev/rfkill c $(cat /sys/class/misc/rfkill/dev | tr ':' ' ')
ln -s /proc/self/fd/0 /dev/stdin
ln -s /proc/self/fd/1 /dev/stdout
ln -s /proc/self/fd/2 /dev/stderr

echo "VM has started up" > /dev/ttyS0

# create stub sudo - everything runs as uid 0
mkdir /tmp/bin
cat > /tmp/bin/sudo << EOF
#!/bin/bash

exec "\$@"
EOF
chmod +x /tmp/bin/sudo
# and put it into $PATH, as well as our extra-$PATH
export PATH=/tmp/bin:$EPATH:$PATH

# some tests assume adm/admin group(s) exist(s)
cat > /etc/group <<EOF
adm:x:0:
admin:x:0:
messagebus:x:106:
EOF
# root should exist
cat > /etc/passwd <<EOF
root:x:0:0:root:/tmp:/bin/bash
messagebus:x:102:106::/var/run/dbus:/bin/false
EOF
cat > /etc/ethertypes <<EOF
IPv4	 	0800  	ip ip4
ARP		0806	ether-arp
IPv6		86DD	ip6
EOF
cat > /etc/protocols <<EOF
ip      0       IP
icmp    1       ICMP
tcp     6       TCP
udp     17      UDP
ipv6-icmp 58	IPv6-ICMP
EOF

# we may need /etc/alternatives, at least on Debian-based systems
ln -s /tmp/etc/alternatives /etc/

# local network is needed for some tests
ip link set lo up

# create logs mountpoint and mount the logshare
mkdir /tmp/logs
if grep -q rootfstype=hostfs /proc/cmdline; then
    mount -t hostfs none /tmp/logs -o $LOGDIR
else
    mount -t 9p -o trans=virtio,rw logshare /tmp/logs
fi

# allow access to any outside directory (e.g. /tmp) we also have
mkdir /tmp/host
mount --bind / /tmp/host

if [ "$TIMEWARP" = "1" ] ; then
    (
        while sleep 1 ; do
            date --set "@$(($(date +%s) + 19))"
        done
    ) &
fi

echo hwsimvm > /proc/sys/kernel/hostname
echo 8 8 8 8 > /proc/sys/kernel/printk

cat > /tmp/bin/login <<EOF
#!/bin/sh

export PS1='\h:\w\$ '
exec bash
EOF
chmod +x /tmp/bin/login

if [ "$TELNET" = "1" ] ; then
  ip link set eth0 up
  ip addr add 172.16.0.15/24 dev eth0
  which in.telnetd >/dev/null && (
    while true ; do
      in.telnetd -debug 23 -L /tmp/bin/login
    done
  ) &
fi

# check if we're rebooting due to a kernel panic ...
if grep -q 'Kernel panic' /tmp/logs/console ; then
	echo "KERNEL CRASHED!" >/dev/ttyS0
else
	# finally run the tests
	export USER=0
	export LOGDIR=/tmp/logs
	export DBFILE=$LOGDIR/results.db
	export PREFILL_DB=y

	# some tests need CRDA, install a simple uevent helper
	# and preload the 00 domain it will have asked for already
	echo $TESTDIR/vm/uevent.sh > /sys/kernel/uevent_helper
	COUNTRY=00 crda

	mkdir -p /var/run/dbus
	touch /var/run/dbus/hwsim-test
	chown messagebus.messagebus /var/run/dbus
	dbus-daemon --config-file=$TESTDIR/vm/dbus.conf --fork

	cd $TESTDIR
	./run-all.sh --vm $(cat /tmp/host$ARGS) </dev/ttyS0 >/dev/ttyS0 2>&1
	if test -d /sys/kernel/debug/gcov ; then
		cp -ar /sys/kernel/debug/gcov /tmp/logs/
		# these are broken as they're updated while being read ...
		find /tmp/logs/gcov/ -wholename '*kernel/gcov/*' -print0 | xargs -0 rm
	fi
	#bash </dev/ttyS0 >/dev/ttyS0 2>&1
fi

# and shut down the machine again
halt -f -p
