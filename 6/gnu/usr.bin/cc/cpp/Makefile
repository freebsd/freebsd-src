# $FreeBSD$

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	cpp
SRCS=	gcc.c cppspec.c

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
