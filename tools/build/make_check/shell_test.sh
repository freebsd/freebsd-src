#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
