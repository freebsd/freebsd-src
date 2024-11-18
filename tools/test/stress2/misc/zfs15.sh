#!/bin/sh

# Test scenario suggestion by: markj@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko; loaded=1; } ||
    exit 0

. ../default.cfg

here=`pwd`
level=`jot -r 1 1 3`	# Redundancy levels
mp1=/stress2_tank/test
s=0
u1=$mdstart
u2=$((u1 + 1))
u3=$((u1 + 2))
u4=$((u1 + 3))
u5=$((u1 + 4))

set -e
mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2
mdconfig -l | grep -q md$u3 && mdconfig -d -u $u3
mdconfig -l | grep -q md$u4 && mdconfig -d -u $u4
mdconfig -l | grep -q md$u5 && mdconfig -d -u $u5

mdconfig -s 512m -u $u1
mdconfig -s 512m -u $u2
mdconfig -s 512m -u $u3
mdconfig -s 512m -u $u4
mdconfig -s 512m -u $u5

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank raidz$level md$u1 md$u2 md$u3 md$u4
zfs create stress2_tank/test
set +e

export RUNDIR=/stress2_tank/test/stressX
export runRUNTIME=5m
export LOAD=80
export symlinkLOAD=80
export rwLOAD=80
export TESTPROGS="
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/mkfifo/mkfifo
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
testcases/swap/swap
"

(cd ..; ./testcases/run/run $TESTPROGS > /dev/null 2>&1) &

sleep 60
echo "zpool attach stress2_tank raidz$level-0 md$u5"
zpool attach stress2_tank raidz$level-0 md$u5
sleep 30
zfs snapshot stress2_tank/test@1
wait

while zpool status | grep -q "in progress"; do
	sleep 5
done
zpool scrub stress2_tank
zpool status | grep -q "errors: No known data errors" ||
    { zpool status; s=1; }

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u1
mdconfig -d -u $u2
mdconfig -d -u $u3
mdconfig -d -u $u4
mdconfig -d -u $u5
[ -n "$loaded" ] && kldunload zfs.ko
exit $s
