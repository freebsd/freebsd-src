# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $Id: Makefile,v 1.1 1998/08/27 08:09:42 obrien Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/wire-test

PROG=		wire-test
MAN8=		wire-test.8

SRCS=		wire-test.c

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU}

.include <bsd.prog.mk>
