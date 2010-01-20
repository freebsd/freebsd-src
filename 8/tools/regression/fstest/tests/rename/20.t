#!/bin/sh
# $FreeBSD$

desc="rename returns EEXIST or ENOTEMPTY if the 'to' argument is a directory and is not empty"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..16"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n1} 0755

expect 0 create ${n1}/${n2} 0644
expect "EEXIST|ENOTEMPTY" rename ${n0} ${n1}
expect 0 unlink ${n1}/${n2}

expect 0 mkdir ${n1}/${n2} 0755
expect "EEXIST|ENOTEMPTY" rename ${n0} ${n1}
expect 0 rmdir ${n1}/${n2}

expect 0 mkfifo ${n1}/${n2} 0644
expect "EEXIST|ENOTEMPTY" rename ${n0} ${n1}
expect 0 unlink ${n1}/${n2}

expect 0 symlink test ${n1}/${n2}
expect "EEXIST|ENOTEMPTY" rename ${n0} ${n1}
expect 0 unlink ${n1}/${n2}

expect 0 rmdir ${n1}
expect 0 rmdir ${n0}
