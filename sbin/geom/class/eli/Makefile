# $FreeBSD$

PACKAGE=runtime
.PATH: ${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/eli ${.CURDIR}/../../../../sys/crypto/sha2

GEOM_CLASS=	eli
SRCS=	g_eli_crypto.c
SRCS+=	g_eli_hmac.c
SRCS+=	g_eli_key.c
SRCS+=	pkcs5v2.c
SRCS+=	sha256c.c
SRCS+=	sha512c.c

LIBADD=	md crypto

WARNS?=	3

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
