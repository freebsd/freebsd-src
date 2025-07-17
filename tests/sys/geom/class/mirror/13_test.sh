#!/bin/sh

# Test handling of write errors.

. $(dirname $0)/conf.sh

echo 1..4

set -e

ddbs=2048
regwritefp="debug.fail_point.g_mirror_regular_request_write"
m1=$(mktemp $base.XXXXXX)
m2=$(mktemp $base.XXXXXX)

dd if=/dev/random of=$m1 bs=$ddbs count=1024 >/dev/null 2>&1
dd if=/dev/zero of=$m2 bs=$ddbs count=1024 >/dev/null 2>&1

attach_md us0 -t vnode -f $m1
attach_md us1 -t vnode -f $m2

gmirror label $name /dev/$us0 /dev/$us1
devwait

tmp1=$(mktemp $base.XXXXXX)
tmp2=$(mktemp $base.XXXXXX)

dd if=/dev/random of=$tmp1 bs=$ddbs count=1 >/dev/null 2>&1

ENXIO=6
# gmirror has special handling for ENXIO. It does not mark the failed component
# as broken, allowing it to rejoin the mirror automatically when it appears.
sysctl ${regwritefp}="1*return(${ENXIO})[pid $(gmirror_worker_pid)]"
dd if=$tmp1 of=/dev/mirror/$name bs=$ddbs count=1 >/dev/null 2>&1
dd if=/dev/mirror/$name of=$tmp2 bs=$ddbs count=1 >/dev/null 2>&1
sysctl ${regwritefp}='off'

if cmp -s $tmp1 $tmp2; then
	echo "ok 1"
else
	echo "not ok 1"
fi

# Verify that the genids still match after ENXIO.
genid1=$(gmirror dump /dev/$us0 | awk '/^[[:space:]]*genid: /{print $2}')
genid2=$(gmirror dump /dev/$us1 | awk '/^[[:space:]]*genid: /{print $2}')
if [ $genid1 -eq $genid2 ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi

# The ENXIO should have caused a syncid bump.
syncid1=$(gmirror dump /dev/$us0 | awk '/^[[:space:]]*syncid: /{print $2}')
syncid2=$(gmirror dump /dev/$us1 | awk '/^[[:space:]]*syncid: /{print $2}')
if [ $syncid1 -eq $(($syncid2 + 1)) -o $syncid2 -eq $(($syncid1 + 1)) ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

# Force a retaste of the disconnected component.
if [ $(gmirror status -s $name | awk '{print $3}') = $us0 ]; then
	detach_md $us1
	attach_md us1 -t vnode -f $m2
else
	detach_md $us0
	attach_md us0 -t vnode -f $m1
fi

# Make sure that the retaste caused the mirror to automatically be re-added.
if [ $(gmirror status -s $name | wc -l) -eq 2 ]; then
	echo "ok 4"
else
	echo "not ok 4"
fi

syncwait

rm -f $m1 $m2 $tmp1 $tmp2
