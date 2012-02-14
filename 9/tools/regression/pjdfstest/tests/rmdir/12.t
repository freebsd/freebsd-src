#!/bin/sh
# $FreeBSD$

desc="rmdir returns EINVAL if the last component of the path is '.' and EEXIST or ENOTEMPTY if the last component of the path is '..'"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n0}/${n1} 0755
expect EINVAL rmdir ${n0}/${n1}/.
todo FreeBSD "According to POSIX: EEXIST or ENOTEMPTY - The path argument names a directory that is not an empty directory, or there are hard links to the directory other than dot or a single entry in dot-dot."
expect "ENOTEMPTY|EEXIST" rmdir ${n0}/${n1}/..
expect 0 rmdir ${n0}/${n1}
expect 0 rmdir ${n0}
