#!/bin/sh
# $FreeBSD$

desc="chown returns EROFS if the named file resides on a read-only file system"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}:{fs}" in
FreeBSD:UFS)
	echo "1..10"

	n0=`namegen`
	n1=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}
	expect 0 create ${n0}/${n1} 0644
	expect 0 chown ${n0}/${n1} 65534 65534
	expect 65534,65534 stat ${n0}/${n1} uid,gid
	mount -ur /dev/md${n}
	expect EROFS chown ${n0}/${n1} 65533 65533
	expect 65534,65534 stat ${n0}/${n1} uid,gid
	mount -uw /dev/md${n}
	expect 0 chown ${n0}/${n1} 65533 65533
	expect 65533,65533 stat ${n0}/${n1} uid,gid
	expect 0 unlink ${n0}/${n1}
	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
