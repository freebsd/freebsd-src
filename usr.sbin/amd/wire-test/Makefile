# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $Id: Makefile,v 1.2 1998/08/30 19:59:53 gpalmer Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/wire-test

PROG=		wire-test
MAN8=		wire-test.8

SRCS=		wire-test.c

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU}

.include <bsd.prog.mk>
