# $FreeBSD$

.include <bsd.own.mk>
.include <src.opts.mk>

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI=	pure
.endif


.include "../Makefile.inc"
.include "../Makefile.ver"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c errors.c
MAN=	gcov.1

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
