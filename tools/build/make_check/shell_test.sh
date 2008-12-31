#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
