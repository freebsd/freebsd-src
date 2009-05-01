# $FreeBSD: src/sbin/geom/class/eli/Makefile,v 1.3.10.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/eli ${.CURDIR}/../../../../sys/crypto/sha2

CLASS=	eli
SRCS=	g_eli_crypto.c
SRCS+=	g_eli_key.c
SRCS+=	pkcs5v2.c
SRCS+=	sha2.c

DPADD=	${LIBMD} ${LIBCRYPTO}
LDADD=	-lmd -lcrypto

WARNS?=	3

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
