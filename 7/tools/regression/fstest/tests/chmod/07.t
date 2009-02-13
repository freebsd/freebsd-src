#!/bin/sh
# $FreeBSD$

desc="chmod returns EPERM if the operation would change the ownership, but the effective user ID is not the super-user"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..14"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
cdir=`pwd`
cd ${n0}
expect 0 mkdir ${n1} 0755
expect 0 chown ${n1} 65534 65534
expect 0 -u 65534 -g 65534 create ${n1}/${n2} 0644
expect 0 -u 65534 -g 65534 chmod ${n1}/${n2} 0642
expect 0642 stat ${n1}/${n2} mode
expect EPERM -u 65533 -g 65533 chmod ${n1}/${n2} 0641
expect 0642 stat ${n1}/${n2} mode
expect 0 chown ${n1}/${n2} 0 0
expect EPERM -u 65534 -g 65534 chmod ${n1}/${n2} 0641
expect 0642 stat ${n1}/${n2} mode
expect 0 unlink ${n1}/${n2}
expect 0 rmdir ${n1}
cd ${cdir}
expect 0 rmdir ${n0}
