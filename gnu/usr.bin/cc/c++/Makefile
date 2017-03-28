# $FreeBSD$

MAN=
.include <src.opts.mk>

# gengtype can't handling __uintptr_t
WANT_CHERI=	none

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}/cp

PROG=	g++
SRCS+=	g++spec.c

DPADD=	${LIBCPP} ${LIBIBERTY}
LDADD=	${LIBCPP} ${LIBIBERTY}

.if ${MK_CLANG_IS_CC} == "no"
LINKS=	${BINDIR}/g++ ${BINDIR}/c++
LINKS+=	${BINDIR}/g++ ${BINDIR}/CC
.endif

.include <bsd.prog.mk>
