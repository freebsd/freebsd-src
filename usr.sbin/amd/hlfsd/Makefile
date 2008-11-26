# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/hlfsd/Makefile,v 1.8.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/amd/hlfsd

PROG=	hlfsd
MAN=	hlfsd.8
SRCS=	hlfsd.c homedir.c nfs_prot_svc.c stubs.c

CFLAGS+= -I${.CURDIR}/../../../contrib/amd/hlfsd

DPADD=	${LIBAMU}
LDADD=	${LIBAMU}

.include <bsd.prog.mk>
