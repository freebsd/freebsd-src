# $FreeBSD$

.include <bsd.own.mk>
.include <src.opts.mk>

# gengtype can't handling __uintptr_t
WANT_CHERI=	none

.include "../Makefile.inc"
.include "../Makefile.ver"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c errors.c
MAN=	gcov.1

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
