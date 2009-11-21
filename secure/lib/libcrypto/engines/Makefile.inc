# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
