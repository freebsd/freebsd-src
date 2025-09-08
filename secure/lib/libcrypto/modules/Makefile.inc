PACKAGE?=	openssl
LIB_PACKAGE=
SHLIBDIR=	${LIBDIR}/ossl-modules

LCRYPTO_SRC=	${SRCTOP}/crypto/openssl

CFLAGS+=	-I${LCRYPTO_SRC}
CFLAGS+=	-I${LCRYPTO_SRC}/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/common/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/fips/include
CFLAGS+=	-I${LCRYPTO_SRC}/providers/implementations/include

WARNS?=		0
