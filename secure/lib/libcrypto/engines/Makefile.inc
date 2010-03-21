# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
