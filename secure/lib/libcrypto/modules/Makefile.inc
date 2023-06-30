# $FreeBSD$

SHLIBDIR=	${LIBDIR}/ossl-modules

LCRYPTO_SRC=	${SRCTOP}/crypto/openssl

CFLAGS+=	-I${LCRYPTO_SRC}
CFLAGS+=	-I${LCRYPTO_SRC}/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/common/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/implementations/include

# common
SRCS+=	provider_err.c provider_ctx.c
SRCS+=	provider_util.c

.PATH:	${LCRYPTO_SRC}/providers \
	${LCRYPTO_SRC}/providers/common

WARNS?=		0
