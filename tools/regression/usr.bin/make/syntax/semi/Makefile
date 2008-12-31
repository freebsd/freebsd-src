# $FreeBSD: src/tools/regression/usr.bin/make/syntax/semi/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
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
