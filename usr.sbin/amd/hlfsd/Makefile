# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/hlfsd/Makefile,v 1.4 1999/08/28 01:15:15 peter Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/hlfsd

PROG=		hlfsd
MAN8=		hlfsd.8

SRCS=		hlfsd.c homedir.c nfs_prot_svc.c stubs.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/hlfsd

DPADD=		${LIBAMU} ${LIBRPCSVC}
LDADD=		${LIBAMU} -lrpcsvc

.include <bsd.prog.mk>
