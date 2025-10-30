#!/bin/sh
# vim: filetype=sh noexpandtab ts=8 sw=8
# $FreeBSD: head/tools/regression/pjdfstest/tests/link/05.t 211352 2010-08-15 21:24:17Z pjd $

desc="link returns EMLINK if the link count of the file named by name1 would exceed {PC_LINK_MAX}"

dir=`dirname $0`
. ${dir}/../misc.sh

[ "${os}:${fs}" = "FreeBSD:UFS" ] || quick_exit

echo "1..5"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
n=`mdconfig -a -n -t malloc -s 2m` || exit
newfs -i 1 /dev/md${n} >/dev/null || exit
mount /dev/md${n} ${n0} || exit
link_max=`${fstest} pathconf ${n0} _PC_LINK_MAX`
expect 0 create ${n0}/${n1} 0644
i=1
while [ ${i} -le ${link_max} ]; do
	link ${n0}/${n1} ${n0}/${i} >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		break
	fi
	i=$((i + 1))
done
test_check $i -eq ${link_max}

expect EMLINK link ${n0}/${n1} ${n0}/${n2}

umount /dev/md${n}
mdconfig -d -u ${n} || exit
expect 0 rmdir ${n0}
