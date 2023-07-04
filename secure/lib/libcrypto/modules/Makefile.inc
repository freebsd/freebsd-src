# $FreeBSD$

SHLIBDIR=	${LIBDIR}/ossl-modules

LCRYPTO_SRC=	${SRCTOP}/crypto/openssl

CFLAGS+=	-I${LCRYPTO_SRC}
CFLAGS+=	-I${LCRYPTO_SRC}/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/common/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/implementations/include

.include <bsd.endian.mk>

.PATH: ${LCRYPTO_SRC}/providers

WARNS?=		0
