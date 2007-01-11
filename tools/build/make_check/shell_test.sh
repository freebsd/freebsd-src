#!/bin/sh
# $FreeBSD: src/tools/build/make_check/shell_test.sh,v 1.2 2005/03/02 12:33:22 harti Exp $
echo $@
if ! test -t 0 ; then
	cat
fi
