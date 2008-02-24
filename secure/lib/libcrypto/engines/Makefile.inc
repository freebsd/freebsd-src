# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1 2006/07/29 19:41:32 simon Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
