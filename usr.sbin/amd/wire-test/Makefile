# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/wire-test/Makefile,v 1.2.2.1 1999/08/29 15:39:22 peter Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/wire-test

PROG=		wire-test
MAN8=		wire-test.8

SRCS=		wire-test.c

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU}

.include <bsd.prog.mk>
