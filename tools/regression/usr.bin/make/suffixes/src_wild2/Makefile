# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/src_wild2/Makefile,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $
#
# Source expansion with the source in a macro
#

SRC=*.a

test1:	TEST1.b
	cat TEST1.a TEST2.a | diff -u - TEST1.b

TEST1.b: ${SRC}
	cat ${.ALLSRC} >${.TARGET}
