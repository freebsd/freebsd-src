# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/src_wild1/Makefile,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

test1:	TEST1.b
	cat TEST1.a TEST2.a | diff -u - TEST1.b

TEST1.b: *.a
	cat ${.ALLSRC} >${.TARGET}
