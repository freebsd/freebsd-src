#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/20.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns ETXTBSY when the file is a pure procedure (shared text) file that is being executed and the open() system call requests write access"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}" in
FreeBSD)
	echo "1..4"

	n0=`namegen`

	cp -pf `which sleep` ${n0}
	./${n0} 3 &
	expect ETXTBSY open ${n0} O_WRONLY
	expect ETXTBSY open ${n0} O_RDWR
	expect ETXTBSY open ${n0} O_RDONLY,O_TRUNC
	expect 0 unlink ${n0}
	;;
*)
	quick_exit
	;;
esac
