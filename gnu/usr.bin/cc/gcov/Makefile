# $FreeBSD$

.include <bsd.own.mk>
.include <src.opts.mk>

# linked due to fake private libs
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
