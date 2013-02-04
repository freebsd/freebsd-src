# $FreeBSD$

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines ${LCRYPTO_SRC}/engines/ccgost

SHLIBDIR?= /usr/lib/engines
