#!/bin/sh
# $FreeBSD$

desc="O_CREAT is specified, the file does not exist, and the directory in which it is to be created has its immutable flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

case "${os}:${fs}" in
FreeBSD:ZFS)
	echo "1..17"
	;;
FreeBSD:UFS)
	echo "1..30"
	;;
*)
	quick_exit
esac

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755

expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
expect 0 unlink ${n0}/${n1}

expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM open ${n0}/${n1} O_RDONLY,O_CREAT 0644
expect 0 chflags ${n0} none
expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
expect 0 unlink ${n0}/${n1}

expect 0 chflags ${n0} SF_NOUNLINK
expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 chflags ${n0} SF_APPEND
expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

case "${os}:${fs}" in
FreeBSD:UFS)
	expect 0 chflags ${n0} UF_IMMUTABLE
	expect EPERM open ${n0}/${n1} O_RDONLY,O_CREAT 0644
	expect 0 chflags ${n0} none
	expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
	expect 0 unlink ${n0}/${n1}

	expect 0 chflags ${n0} UF_NOUNLINK
	expect 0 symlink test ${n0}/${n1}
	expect 0 chflags ${n0} none
	expect 0 unlink ${n0}/${n1}

	expect 0 chflags ${n0} UF_APPEND
	expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
	expect 0 chflags ${n0} none
	expect 0 unlink ${n0}/${n1}
	;;
esac

expect 0 rmdir ${n0}
