#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
