#!/bin/sh
# $FreeBSD$

name="test"
base=`basename $0`
us0=45
us1=`expr $us0 + 1`
us2=`expr $us0 + 2`
nblocks1=9
nblocks2=`expr $nblocks1 - 1`
nblocks3=`expr $nblocks2 / 2`

mdconfig -a -t malloc -s $nblocks1 -u $us0 || exit 1
mdconfig -a -t malloc -s $nblocks1 -u $us1 || exit 1
mdconfig -a -t malloc -s $nblocks1 -u $us2 || exit 1

dd if=/dev/random of=/dev/md${us0} count=$nblocks1 >/dev/null 2>&1
dd if=/dev/random of=/dev/md${us1} count=$nblocks1 >/dev/null 2>&1
dd if=/dev/random of=/dev/md${us2} count=$nblocks1 >/dev/null 2>&1

graid3 label -w $name /dev/md${us0} /dev/md${us1} /dev/md${us2} || exit 1
# Wait for synchronization.
sleep 2
graid3 stop $name
# Break one component.
dd if=/dev/random of=/dev/md${us1} count=$nblocks2 >/dev/null 2>&1
# Provoke retaste of the rest components.
true > /dev/md${us0}
true > /dev/md${us2}
sleep 1

dd if=/dev/raid3/${name} of=/dev/null bs=1k count=$nblocks3 >/dev/null 2>&1
ec=$?
if [ $ec -eq 0 ]; then
	echo "FAIL"
else
	echo "PASS"
fi

graid3 stop $name
mdconfig -d -u $us0
mdconfig -d -u $us1
mdconfig -d -u $us2
