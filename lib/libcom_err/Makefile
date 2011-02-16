# $FreeBSD: src/lib/libcom_err/Makefile,v 1.12.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	${COM_ERRDIR}/com_err.h ${COM_ERRDIR}/com_right.h
MAN=	com_err.3
COM_ERRDIR=	${.CURDIR}/../../contrib/com_err
CFLAGS+=	-I${COM_ERRDIR}

SUBDIR=	doc

.include <bsd.lib.mk>

.PATH:	${COM_ERRDIR}
