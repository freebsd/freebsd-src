#!/bin/sh
# $FreeBSD$

desc="rename returns EPERM if the parent directory of the file pointed at by the 'to' argument has its immutable flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

case "${os}:${fs}" in
FreeBSD:ZFS)
	flags1="SF_IMMUTABLE"
	flags2="SF_NOUNLINK SF_APPEND"
	echo "1..110"
	;;
FreeBSD:UFS)
	flags1="SF_IMMUTABLE UF_IMMUTABLE"
	flags2="SF_NOUNLINK SF_APPEND UF_NOUNLINK UF_APPEND"
	echo "1..188"
	;;
*)
	quick_exit
esac

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755

expect 0 create ${n1} 0644
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect EPERM rename ${n1} ${n0}/${n2}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 mkdir ${n1} 0755
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect EPERM rename ${n1} ${n0}/${n2}
done
expect 0 chflags ${n0} none
expect 0 rmdir ${n1}

expect 0 mkfifo ${n1} 0644
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect EPERM rename ${n1} ${n0}/${n2}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 mknod ${n1} c 0644 1 2
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect EPERM rename ${n1} ${n0}/${n2}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 bind ${n1}
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect EPERM rename ${n1} ${n0}/${n2}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 symlink ${n2} ${n1}
for flag in ${flags1}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect EPERM rename ${n1} ${n0}/${n2}
done
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 create ${n1} 0644
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n1} ${n0}/${n2}
	expect 0 chflags ${n0} none
	expect 0 rename ${n0}/${n2} ${n1}
done
expect 0 unlink ${n1}

expect 0 mkdir ${n1} 0755
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n1} ${n0}/${n2}
	expect 0 chflags ${n0} none
	expect 0 rename ${n0}/${n2} ${n1}
done
expect 0 rmdir ${n1}

expect 0 mkfifo ${n1} 0644
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n1} ${n0}/${n2}
	expect 0 chflags ${n0} none
	expect 0 rename ${n0}/${n2} ${n1}
done
expect 0 unlink ${n1}

expect 0 mknod ${n1} c 0644 1 2
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n1} ${n0}/${n2}
	expect 0 chflags ${n0} none
	expect 0 rename ${n0}/${n2} ${n1}
done
expect 0 unlink ${n1}

expect 0 bind ${n1}
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n1} ${n0}/${n2}
	expect 0 chflags ${n0} none
	expect 0 rename ${n0}/${n2} ${n1}
done
expect 0 unlink ${n1}

expect 0 symlink ${n2} ${n1}
for flag in ${flags2}; do
	expect 0 chflags ${n0} ${flag}
	expect ${flag} stat ${n0} flags
	expect 0 rename ${n1} ${n0}/${n2}
	expect 0 chflags ${n0} none
	expect 0 rename ${n0}/${n2} ${n1}
done
expect 0 unlink ${n1}

expect 0 rmdir ${n0}
