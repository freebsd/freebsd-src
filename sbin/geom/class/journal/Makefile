# $FreeBSD: src/sbin/geom/class/journal/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../misc

CLASS=	journal
SRCS+=	geom_journal_ufs.c

DPADD=	${LIBMD} ${LIBUFS}
LDADD=	-lmd -lufs

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
