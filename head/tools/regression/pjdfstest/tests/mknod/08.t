#!/bin/sh
# $FreeBSD$

desc="mknod returns EEXIST if the named file exists"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..35"

n0=`namegen`

for type in regular dir fifo block char socket symlink; do
	create_file ${type} ${n0}
	expect EEXIST mknod ${n0} b 0644 0 0
	expect EEXIST mknod ${n0} c 0644 0 0
	expect EEXIST mknod ${n0} f 0644 0 0
	if [ "${type}" = "dir" ]; then
		expect 0 rmdir ${n0}
	else
		expect 0 unlink ${n0}
	fi
done
