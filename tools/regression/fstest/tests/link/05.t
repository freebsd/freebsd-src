#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/link/05.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="link returns EMLINK if the link count of the file named by name1 would exceed 32767"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}:${fs}" in
FreeBSD:UFS)
	echo "1..5"

	n0=`namegen`
	n1=`namegen`
	n2=`namegen`

	expect 0 mkdir ${n0} 0755
	n=`mdconfig -a -n -t malloc -s 1m`
	newfs -i 1 /dev/md${n} >/dev/null
	mount /dev/md${n} ${n0}
	expect 0 create ${n0}/${n1} 0644
	i=1
	while :; do
		link ${n0}/${n1} ${n0}/${i} >/dev/null 2>&1
		if [ $? -ne 0 ]; then
			break
		fi
		i=`expr $i + 1`
	done
	test_check $i -eq 32767

	expect EMLINK link ${n0}/${n1} ${n0}/${n2}

	umount /dev/md${n}
	mdconfig -d -u ${n}
	expect 0 rmdir ${n0}
	;;
*)
	quick_exit
	;;
esac
