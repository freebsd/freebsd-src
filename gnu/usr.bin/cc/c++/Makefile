# $FreeBSD$

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}/cp

PROG=	g++
SRCS+=	g++spec.c
NO_MAN=

DPADD=	${LIBCPP} ${LIBIBERTY}
LDADD=	${LIBCPP} ${LIBIBERTY}

.include <bsd.own.mk>

.if ${MK_CLANG_IS_CC} == "no"
LINKS=	${BINDIR}/g++ ${BINDIR}/c++
LINKS+=	${BINDIR}/g++ ${BINDIR}/CC
.endif

.include <bsd.prog.mk>
