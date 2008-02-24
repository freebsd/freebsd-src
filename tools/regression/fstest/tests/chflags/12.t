#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chflags/12.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chflags returns EROFS if the named file resides on a read-only file system"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

case "${os}:${fs}" in
FreeBSD:UFS)
	echo "1..14"

	n0=`namegen`
	n1=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}
	expect 0 create ${n0}/${n1} 0644
	expect 0 chflags ${n0}/${n1} UF_IMMUTABLE
	expect UF_IMMUTABLE stat ${n0}/${n1} flags
	expect 0 chflags ${n0}/${n1} none
	expect none stat ${n0}/${n1} flags
	mount -ur /dev/md${n}
	expect EROFS chflags ${n0}/${n1} UF_IMMUTABLE
	expect none stat ${n0}/${n1} flags
	mount -uw /dev/md${n}
	expect 0 chflags ${n0}/${n1} UF_IMMUTABLE
	expect UF_IMMUTABLE stat ${n0}/${n1} flags
	expect 0 chflags ${n0}/${n1} none
	expect none stat ${n0}/${n1} flags
	expect 0 unlink ${n0}/${n1}
	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
