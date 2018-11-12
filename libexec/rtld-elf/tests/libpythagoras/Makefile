# $FreeBSD$

.include <bsd.own.mk>

LIB=		pythagoras
SHLIB_MAJOR=	0

LIBDIR=		${TESTSBASE}/libexec/rtld-elf
SHLIBDIR=	${TESTSBASE}/libexec/rtld-elf

SRCS=		pythagoras.c

DPADD=		${LIBM}
LDADD=		-lm

.include <bsd.lib.mk>
