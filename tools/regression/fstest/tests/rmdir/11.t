#!/bin/sh
# $FreeBSD$

desc="rmdir returns EACCES or EPERM if the directory containing the directory to be removed is marked sticky, and neither the containing directory nor the directory to be removed are owned by the effective user ID"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..15"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

expect 0 mkdir ${n0} 0755
expect 0 chown ${n0} 65534 65534
expect 0 chmod ${n0} 01777

# User owns both: the sticky directory and the directory to be removed.
expect 0 -u 65534 -g 65534 mkdir ${n0}/${n1} 0755
expect 0 -u 65534 -g 65534 rmdir ${n0}/${n1}
# User owns the directory to be removed, but doesn't own the sticky directory.
expect 0 -u 65533 -g 65533 mkdir ${n0}/${n1} 0755
expect 0 -u 65533 -g 65533 rmdir ${n0}/${n1}
# User owns the sticky directory, but doesn't own the directory to be removed.
expect 0 -u 65533 -g 65533 mkdir ${n0}/${n1} 0755
expect 0 -u 65534 -g 65534 rmdir ${n0}/${n1}
# User doesn't own the sticky directory nor the directory to be removed.
expect 0 -u 65534 -g 65534 mkdir ${n0}/${n1} 0755
expect "EACCES|EPERM" -u 65533 -g 65533 rmdir ${n0}/${n1}
expect 0 rmdir ${n0}/${n1}

expect 0 rmdir ${n0}

cd ${cdir}
expect 0 rmdir ${n2}
