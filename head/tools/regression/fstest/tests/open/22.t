#!/bin/sh
# $FreeBSD$

desc="open returns EEXIST when O_CREAT and O_EXCL were specified and the file exists"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

n0=`namegen`

expect 0 create ${n0} 0644
expect EEXIST open ${n0} O_CREAT,O_EXCL 0644
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
expect EEXIST open ${n0} O_CREAT,O_EXCL 0644
expect 0 rmdir ${n0}

expect 0 mkfifo ${n0} 0644
expect EEXIST open ${n0} O_CREAT,O_EXCL 0644
expect 0 unlink ${n0}

expect 0 symlink test ${n0}
expect EEXIST open ${n0} O_CREAT,O_EXCL 0644
expect 0 unlink ${n0}
