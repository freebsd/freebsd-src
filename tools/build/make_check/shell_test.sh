#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
