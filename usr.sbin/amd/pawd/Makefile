# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/pawd/Makefile,v 1.3.2.1 2001/04/25 12:09:15 ru Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/amq

BINDIR=		/usr/bin

PROG=		pawd
SRCS=		pawd.c amq_clnt.c amq_xdr.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/amq

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU}

.include <bsd.prog.mk>
