#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
