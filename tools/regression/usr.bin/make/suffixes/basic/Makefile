# $FreeBSD: src/tools/regression/usr.bin/make/suffixes/basic/Makefile,v 1.1 2005/05/31 14:13:05 harti Exp $
#
# Check that a simple suffix rule is correctly processed.

test1:	TEST1.b
	diff -u TEST1.a TEST1.b

.SUFFIXES:

.SUFFIXES: .a .b

.a.b:
	cat ${.IMPSRC} >${.TARGET}
