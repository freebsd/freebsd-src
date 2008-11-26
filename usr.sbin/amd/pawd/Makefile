# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/pawd/Makefile,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/amd/amq

BINDIR=	/usr/bin

PROG=	pawd
SRCS=	pawd.c amq_clnt.c amq_xdr.c

CFLAGS+= -I${.CURDIR}/../../../contrib/amd/amq

DPADD=	${LIBAMU}
LDADD=	${LIBAMU}

.include <bsd.prog.mk>
