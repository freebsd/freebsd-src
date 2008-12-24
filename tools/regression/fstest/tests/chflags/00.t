#!/bin/sh
# $FreeBSD$

desc="chflags changes flags"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

case "${os}:${fs}" in
FreeBSD:UFS)
	allflags="UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK"
	userflags="UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE"
	systemflags="SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK"

	echo "1..247"
	;;
FreeBSD:ZFS)
	allflags="UF_NODUMP,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK"
	userflags="UF_NODUMP"
	systemflags="SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK"

	echo "1..167"
	;;
*)
	quick_exit
	;;
esac

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

expect 0 create ${n0} 0644
expect none stat ${n0} flags
expect 0 chflags ${n0} ${allflags}
expect ${allflags} stat ${n0} flags
expect 0 chflags ${n0} ${userflags}
expect ${userflags} stat ${n0} flags
expect 0 chflags ${n0} ${systemflags}
expect ${systemflags} stat ${n0} flags
expect 0 chflags ${n0} none
expect none stat ${n0} flags
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0644
expect none stat ${n0} flags
expect 0 chflags ${n0} ${allflags}
expect ${allflags} stat ${n0} flags
expect 0 chflags ${n0} ${userflags}
expect ${userflags} stat ${n0} flags
expect 0 chflags ${n0} ${systemflags}
expect ${systemflags} stat ${n0} flags
expect 0 chflags ${n0} none
expect none stat ${n0} flags
expect 0 rmdir ${n0}

expect 0 mkfifo ${n0} 0644
expect none stat ${n0} flags
expect 0 chflags ${n0} ${allflags}
expect ${allflags} stat ${n0} flags
expect 0 chflags ${n0} ${userflags}
expect ${userflags} stat ${n0} flags
expect 0 chflags ${n0} ${systemflags}
expect ${systemflags} stat ${n0} flags
expect 0 chflags ${n0} none
expect none stat ${n0} flags
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 symlink ${n0} ${n1}
expect none stat ${n1} flags
expect none lstat ${n1} flags
expect 0 chflags ${n1} ${allflags}
expect ${allflags} stat ${n1} flags
expect none lstat ${n1} flags
expect 0 chflags ${n1} ${userflags}
expect ${userflags} stat ${n1} flags
expect none lstat ${n1} flags
expect 0 chflags ${n1} ${systemflags}
expect ${systemflags} stat ${n1} flags
expect none lstat ${n1} flags
expect 0 chflags ${n1} none
expect none stat ${n1} flags
expect none lstat ${n1} flags
expect 0 unlink ${n1}
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 symlink ${n0} ${n1}
expect none stat ${n1} flags
expect none lstat ${n1} flags
expect 0 lchflags ${n1} ${allflags}
expect ${allflags} lstat ${n1} flags
expect none stat ${n1} flags
expect 0 lchflags ${n1} ${userflags}
expect ${userflags} lstat ${n1} flags
expect none stat ${n1} flags
expect 0 lchflags ${n1} ${systemflags}
expect ${systemflags} lstat ${n1} flags
expect none stat ${n1} flags
expect 0 lchflags ${n1} none
expect none lstat ${n1} flags
expect none stat ${n1} flags
expect 0 unlink ${n1}
expect 0 unlink ${n0}

# successful chflags(2) updates ctime.
expect 0 create ${n0} 0644
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} stat ${n0} ctime`
	sleep 1
	expect 0 chflags ${n0} ${flag}
	ctime2=`${fstest} stat ${n0} ctime`
	test_check $ctime1 -lt $ctime2
done
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} stat ${n0} ctime`
	sleep 1
	expect 0 chflags ${n0} ${flag}
	ctime2=`${fstest} stat ${n0} ctime`
	test_check $ctime1 -lt $ctime2
done
expect 0 rmdir ${n0}

expect 0 mkfifo ${n0} 0644
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} stat ${n0} ctime`
	sleep 1
	expect 0 chflags ${n0} ${flag}
	ctime2=`${fstest} stat ${n0} ctime`
	test_check $ctime1 -lt $ctime2
done
expect 0 unlink ${n0}

expect 0 symlink ${n1} ${n0}
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} lstat ${n0} ctime`
	sleep 1
	expect 0 lchflags ${n0} ${flag}
	ctime2=`${fstest} lstat ${n0} ctime`
	test_check $ctime1 -lt $ctime2
done
expect 0 unlink ${n0}

# unsuccessful chflags(2) does not update ctime.
expect 0 create ${n0} 0644
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} stat ${n0} ctime`
	sleep 1
	expect EPERM -u 65534 chflags ${n0} ${flag}
	ctime2=`${fstest} stat ${n0} ctime`
	test_check $ctime1 -eq $ctime2
done
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} stat ${n0} ctime`
	sleep 1
	expect EPERM -u 65534 chflags ${n0} ${flag}
	ctime2=`${fstest} stat ${n0} ctime`
	test_check $ctime1 -eq $ctime2
done
expect 0 rmdir ${n0}

expect 0 mkfifo ${n0} 0644
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} stat ${n0} ctime`
	sleep 1
	expect EPERM -u 65534 chflags ${n0} ${flag}
	ctime2=`${fstest} stat ${n0} ctime`
	test_check $ctime1 -eq $ctime2
done
expect 0 unlink ${n0}

expect 0 symlink ${n1} ${n0}
for flag in `echo ${allflags},none | tr ',' ' '`; do
	ctime1=`${fstest} lstat ${n0} ctime`
	sleep 1
	expect EPERM -u 65534 lchflags ${n0} ${flag}
	ctime2=`${fstest} lstat ${n0} ctime`
	test_check $ctime1 -eq $ctime2
done
expect 0 unlink ${n0}

cd ${cdir}
expect 0 rmdir ${n2}
