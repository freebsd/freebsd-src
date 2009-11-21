#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
