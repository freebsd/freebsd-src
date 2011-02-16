# $FreeBSD: src/sbin/geom/class/journal/Makefile,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../misc

CLASS=	journal
SRCS+=	geom_journal_ufs.c

DPADD=	${LIBMD} ${LIBUFS}
LDADD=	-lmd -lufs

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
