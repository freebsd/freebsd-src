# $FreeBSD: src/kerberos5/lib/libvers/Makefile,v 1.8.32.1 2010/02/10 00:26:20 kensmith Exp $

LIB=	vers
INTERNALLIB=
SRCS=	print_version.c print_version.h
CFLAGS+=-I.

CLEANFILES= print_version.h

print_version.h: ../../tools/make-print-version/make-print-version
	../../tools/make-print-version/make-print-version ${.TARGET}

../../tools/make-print-version/make-print-version:
	cd ${.CURDIR}/../../tools/make-print-version && ${MAKE}

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/vers
