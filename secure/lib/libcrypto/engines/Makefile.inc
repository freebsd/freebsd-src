# $FreeBSD: src/secure/lib/libcrypto/engines/Makefile.inc,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
