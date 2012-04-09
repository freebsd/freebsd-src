# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
