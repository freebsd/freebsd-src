# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $Id: Makefile,v 1.10 1998/08/27 08:09:40 obrien Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/amq

PROG=		amq
MAN8=		amq.8

SRCS=		amq.c amq_clnt.c amq_xdr.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/amq

DPADD+=		${LIBAMU}
LDADD+=		${LIBAMU}

.include <bsd.prog.mk>
