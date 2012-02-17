#!/bin/sh
# $FreeBSD$

desc="rmdir returns EINVAL if the last component of the path is '.' or '..'"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n0}/${n1} 0755
expect EINVAL rmdir ${n0}/${n1}/.
case "${os}" in
FreeBSD)
	expect EINVAL rmdir ${n0}/${n1}/..
	;;
*)
	expect EEXIST rmdir ${n0}/${n1}/..
	;;
esac
expect 0 rmdir ${n0}/${n1}
expect 0 rmdir ${n0}
