# $FreeBSD: src/sbin/geom/class/journal/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../misc

CLASS=	journal
SRCS+=	geom_journal_ufs.c

DPADD=	${LIBMD} ${LIBUFS}
LDADD=	-lmd -lufs

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
