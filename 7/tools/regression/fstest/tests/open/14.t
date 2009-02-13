#!/bin/sh
# $FreeBSD$

desc="open returns EROFS if the named file resides on a read-only file system, and the file is to be modified"

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
	expect 0 open ${n0}/${n1} O_WRONLY
	expect 0 open ${n0}/${n1} O_RDWR
	expect 0 open ${n0}/${n1} O_RDONLY,O_TRUNC
	mount -ur /dev/md${n}
	expect EROFS open ${n0}/${n1} O_WRONLY
	expect EROFS open ${n0}/${n1} O_RDWR
	expect EROFS open ${n0}/${n1} O_RDONLY,O_TRUNC
	mount -uw /dev/md${n}
	expect 0 unlink ${n0}/${n1}
	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
