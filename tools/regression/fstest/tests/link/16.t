#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/link/16.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="link returns EROFS if the requested link requires writing in a directory on a read-only file system"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}" in
FreeBSD)
	echo "1..9"

	n0=`namegen`
	n1=`namegen`
	n2=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}
	expect 0 create ${n0}/${n1} 0644

	expect 0 link ${n0}/${n1} ${n0}/${n2}
	expect 0 unlink ${n0}/${n2}
	mount -ur /dev/md${n}
	expect EROFS link ${n0}/${n1} ${n0}/${n2}
	mount -uw /dev/md${n}
	expect 0 link ${n0}/${n1} ${n0}/${n2}
	expect 0 unlink ${n0}/${n2}

	expect 0 unlink ${n0}/${n1}
	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
