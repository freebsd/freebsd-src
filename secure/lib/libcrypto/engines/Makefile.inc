# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
