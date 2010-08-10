#!/bin/sh
# $FreeBSD$

desc="mkdir returns EEXIST if the named file exists"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..21"

n0=`namegen`

expect 0 mkdir ${n0} 0755
expect EEXIST mkdir ${n0} 0755
expect 0 rmdir ${n0}

expect 0 create ${n0} 0644
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}

expect 0 symlink test ${n0}
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}

expect 0 mkfifo ${n0} 0644
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}

expect 0 mknod ${n0} b 0644 1 2
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}

expect 0 mknod ${n0} c 0644 1 2
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}

expect 0 bind ${n0}
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}
