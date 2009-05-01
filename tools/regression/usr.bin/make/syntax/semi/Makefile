# $FreeBSD: src/tools/regression/usr.bin/make/syntax/semi/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $
#
# Test handling of escaped newlines.
#

.ifmake test1

FOO=
BAR=bar
FOO != echo ${FOO} ; echo ${BAR}

test1:
	echo ${FOO}

.endif

.ifmake test2

FOO=foo
BAR=bar
FOO != echo ${FOO} ; echo ${BAR}

test2:
	echo ${FOO}

.endif
