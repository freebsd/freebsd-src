# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $Id: Makefile,v 1.1 1998/08/27 08:09:41 obrien Exp $
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
