# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/src_wild1/Makefile,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $

test1:	TEST1.b
	cat TEST1.a TEST2.a | diff -u - TEST1.b

TEST1.b: *.a
	cat ${.ALLSRC} >${.TARGET}
