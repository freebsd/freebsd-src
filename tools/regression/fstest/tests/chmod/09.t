#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chmod/09.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chmod returns EROFS if the named file resides on a read-only file system"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}:${fs}" in
FreeBSD:UFS)
	echo "1..10"

	n0=`namegen`
	n1=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}
	expect 0 create ${n0}/${n1} 0644
	expect 0 chmod ${n0}/${n1} 0640
	expect 0640 stat ${n0}/${n1} mode
	mount -ur /dev/md${n}
	expect EROFS chmod ${n0}/${n1} 0600
	expect 0640 stat ${n0}/${n1} mode
	mount -uw /dev/md${n}
	expect 0 chmod ${n0}/${n1} 0600
	expect 0600 stat ${n0}/${n1} mode
	expect 0 unlink ${n0}/${n1}
	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
