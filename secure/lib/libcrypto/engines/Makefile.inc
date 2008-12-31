# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
