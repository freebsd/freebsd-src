# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD$
#

.PATH:		${.CURDIR}/../../../contrib/amd/hlfsd

PROG=		hlfsd
MAN=		hlfsd.8

SRCS=		hlfsd.c homedir.c nfs_prot_svc.c stubs.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/hlfsd

DPADD=		${LIBAMU} ${LIBRPCSVC}
LDADD=		${LIBAMU} -lrpcsvc

.include <bsd.prog.mk>
