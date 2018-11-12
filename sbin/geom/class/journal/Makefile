# $FreeBSD$

PACKAGE=runtime
.PATH: ${.CURDIR}/../../misc

GEOM_CLASS=	journal
SRCS+=	geom_journal_ufs.c

LIBADD=	ufs md

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
