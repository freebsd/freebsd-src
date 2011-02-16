# $FreeBSD: src/tools/regression/usr.bin/make/syntax/semi/Makefile,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $
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
