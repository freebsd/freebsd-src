# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/src_wild2/Makefile,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $
#
# Source expansion with the source in a macro
#

SRC=*.a

test1:	TEST1.b
	cat TEST1.a TEST2.a | diff -u - TEST1.b

TEST1.b: ${SRC}
	cat ${.ALLSRC} >${.TARGET}
