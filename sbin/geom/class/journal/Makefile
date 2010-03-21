# $FreeBSD: src/sbin/geom/class/journal/Makefile,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../misc

CLASS=	journal
SRCS+=	geom_journal_ufs.c

DPADD=	${LIBMD} ${LIBUFS}
LDADD=	-lmd -lufs

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
