# $FreeBSD: src/sbin/geom/class/journal/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../misc

CLASS=	journal
SRCS+=	geom_journal_ufs.c

DPADD=	${LIBMD} ${LIBUFS}
LDADD=	-lmd -lufs

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
