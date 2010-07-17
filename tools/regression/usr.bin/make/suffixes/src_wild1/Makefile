# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/src_wild1/Makefile,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

test1:	TEST1.b
	cat TEST1.a TEST2.a | diff -u - TEST1.b

TEST1.b: *.a
	cat ${.ALLSRC} >${.TARGET}
