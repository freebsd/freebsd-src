#!/bin/sh
# $FreeBSD$

desc="open returns EROFS when O_CREAT is specified and the named file would reside on a read-only file system"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}:${fs}" in
FreeBSD:UFS)
	echo "1..5"

	n0=`namegen`
	n1=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}
	expect 0 open ${n0}/${n1} O_RDONLY,O_CREAT 0644
	expect 0 unlink ${n0}/${n1}
	mount -ur /dev/md${n}
	expect EROFS open ${n0}/${n1} O_RDONLY,O_CREAT 0644
	mount -uw /dev/md${n}
	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
