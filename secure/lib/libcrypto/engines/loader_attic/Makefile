
SHLIB_NAME?=	loader_attic.so

SRCS=	e_loader_attic.c pvkfmt.c

CFLAGS+=	-DOPENSSL_NO_PROVIDER_CORE

LCRYPTO_SRC=	${SRCTOP}/crypto/openssl

.PATH: ${LCRYPTO_SRC}/crypto/pem \
	${LCRYPTO_SRC}/engines

.include <bsd.lib.mk>
