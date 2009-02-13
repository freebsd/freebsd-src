#!/bin/sh
# $FreeBSD$

desc="chflags returns EPERM if non-super-user tries to set one of SF_IMMUTABLE, SF_APPEND, or SF_NOUNLINK"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..62"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
cdir=`pwd`
cd ${n0}

expect 0 create ${n1} 0644
expect 0 chown ${n1} 65534 65534
for flag in SF_IMMUTABLE SF_APPEND SF_NOUNLINK; do
	expect EPERM -u 65533 -g 65533 chflags ${n1} ${flag}
	expect none stat ${n1} flags
	expect EPERM -u 65534 -g 65534 chflags ${n1} ${flag}
	expect none stat ${n1} flags
done
expect 0 unlink ${n1}

expect 0 mkdir ${n1} 0755
expect 0 chown ${n1} 65534 65534
for flag in SF_IMMUTABLE SF_APPEND SF_NOUNLINK; do
	expect EPERM -u 65533 -g 65533 chflags ${n1} ${flag}
	expect none stat ${n1} flags
	expect EPERM -u 65534 -g 65534 chflags ${n1} ${flag}
	expect none stat ${n1} flags
done
expect 0 rmdir ${n1}

expect 0 mkfifo ${n1} 0644
expect 0 chown ${n1} 65534 65534
for flag in SF_IMMUTABLE SF_APPEND SF_NOUNLINK; do
	expect EPERM -u 65533 -g 65533 chflags ${n1} ${flag}
	expect none stat ${n1} flags
	expect EPERM -u 65534 -g 65534 chflags ${n1} ${flag}
	expect none stat ${n1} flags
done
expect 0 unlink ${n1}

expect 0 symlink ${n2} ${n1}
expect 0 lchown ${n1} 65534 65534
for flag in SF_IMMUTABLE SF_APPEND SF_NOUNLINK; do
	expect EPERM -u 65533 -g 65533 lchflags ${n1} ${flag}
	expect none lstat ${n1} flags
	expect EPERM -u 65534 -g 65534 lchflags ${n1} ${flag}
	expect none lstat ${n1} flags
done
expect 0 unlink ${n1}

cd ${cdir}
expect 0 rmdir ${n0}
