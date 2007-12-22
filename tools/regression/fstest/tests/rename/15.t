#!/bin/sh
# $FreeBSD$

desc="rename returns EXDEV if the link named by 'to' and the file named by 'from' are on different file systems"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}" in
FreeBSD)
	echo "1..14"

	n0=`namegen`
	n1=`namegen`
	n2=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}

	expect 0 create ${n0}/${n1} 0644
	expect EXDEV rename ${n0}/${n1} ${n2}
	expect 0 unlink ${n0}/${n1}

	expect 0 mkdir ${n0}/${n1} 0755
	expect EXDEV rename ${n0}/${n1} ${n2}
	expect 0 rmdir ${n0}/${n1}

	expect 0 mkfifo ${n0}/${n1} 0644
	expect EXDEV rename ${n0}/${n1} ${n2}
	expect 0 unlink ${n0}/${n1}

	expect 0 symlink test ${n0}/${n1}
	expect EXDEV rename ${n0}/${n1} ${n2}
	expect 0 unlink ${n0}/${n1}

	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
