#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2.24.1 2010/02/10 00:26:20 kensmith Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
