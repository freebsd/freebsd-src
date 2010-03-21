#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/18.t,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

desc="open returns EWOULDBLOCK when O_NONBLOCK and one of O_SHLOCK or O_EXLOCK is specified and the file is locked"

dir=`dirname $0`
. ${dir}/../misc.sh

case "${os}" in
FreeBSD)
	echo "1..6"

	n0=`namegen`

	expect 0 create ${n0} 0644
	expect 0 open ${n0} O_RDONLY,O_SHLOCK : open ${n0} O_RDONLY,O_SHLOCK,O_NONBLOCK
	expect "EWOULDBLOCK|EAGAIN" open ${n0} O_RDONLY,O_EXLOCK : open ${n0} O_RDONLY,O_EXLOCK,O_NONBLOCK
	expect "EWOULDBLOCK|EAGAIN" open ${n0} O_RDONLY,O_SHLOCK : open ${n0} O_RDONLY,O_EXLOCK,O_NONBLOCK
	expect "EWOULDBLOCK|EAGAIN" open ${n0} O_RDONLY,O_EXLOCK : open ${n0} O_RDONLY,O_SHLOCK,O_NONBLOCK
	expect 0 unlink ${n0}
	;;
*)
	quick_exit
	;;
esac
