# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

LIB=		pythagoras
SHLIB_MAJOR=	0

LIBDIR=		${TESTSBASE}/libexec/rtld-elf
SHLIBDIR=	${TESTSBASE}/libexec/rtld-elf

SRCS=		pythagoras.c

LIBADD=		m

.include <bsd.lib.mk>
