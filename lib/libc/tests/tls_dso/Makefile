# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

LIB=		h_tls_dynamic
SRCS=		h_tls_dynamic.c

LIBDIR=		${TESTSBASE}/lib/libc/tls
SHLIBDIR=	${TESTSBASE}/lib/libc/tls
SHLIB_MAJOR=	1

WITHOUT_STATIC=
WITHOUT_PROFILE=
WITHOUT_PIC=

MAN=

.include "../Makefile.netbsd-tests"

.include <bsd.lib.mk>
