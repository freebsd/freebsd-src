# $FreeBSD$

LIB=	vers

SRCS=	\
	print_version.c			\
	print_version.h

CFLAGS+=-I.

INTERNALLIB=	YES

print_version.h: ../../tools/make-print-version/make-print-version
	../../tools/make-print-version/make-print-version ${.TARGET}

../../tools/make-print-version/make-print-version:
	cd ${.CURDIR}/../../tools/make-print-version && ${MAKE}

CLEANFILES=	print_version.h

.include <bsd.lib.mk>

.PATH:		${KRB5DIR}/lib/vers
