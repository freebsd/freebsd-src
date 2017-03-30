# $FreeBSD$

LCRYPTO_SRC=	${SRCTOP}/crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines ${LCRYPTO_SRC}/engines/ccgost

SHLIBDIR?= /usr/lib/engines
