# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $Id: Makefile,v 1.1 1998/08/27 08:09:41 obrien Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/hlfsd

PROG=		hlfsd
MAN8=		hlfsd.8

SRCS=		hlfsd.c homedir.c nfs_prot_svc.c stubs.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/hlfsd

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU} -lrpcsvc

.include <bsd.prog.mk>
