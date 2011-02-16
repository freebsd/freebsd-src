#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
