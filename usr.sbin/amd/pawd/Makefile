# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/pawd/Makefile,v 1.2.2.1 1999/08/29 15:39:20 peter Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/amq

BINDIR=		/usr/bin

PROG=		pawd
MAN1=		pawd.1

SRCS=		pawd.c amq_clnt.c amq_xdr.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/amq

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU}

.include <bsd.prog.mk>
