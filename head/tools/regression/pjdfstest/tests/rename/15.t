#!/bin/sh
# $FreeBSD$

desc="rename returns EXDEV if the link named by 'to' and the file named by 'from' are on different file systems"

dir=`dirname $0`
. ${dir}/../misc.sh

[ "${os}" = "FreeBSD" ] || quick_exit

echo "1..23"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
n=`mdconfig -a -n -t malloc -s 1m`
newfs /dev/md${n} >/dev/null
mount /dev/md${n} ${n0}

for type in regular dir fifo block char socket symlink; do
	create_file ${type} ${n0}/${n1}
	expect EXDEV rename ${n0}/${n1} ${n2}
	if [ "${type}" = "dir" ]; then
		expect 0 rmdir ${n0}/${n1}
	else
		expect 0 unlink ${n0}/${n1}
	fi
done

umount /dev/md${n}
mdconfig -d -u ${n}
expect 0 rmdir ${n0}
