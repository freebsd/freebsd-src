# $FreeBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.24.8.2 2006/09/29 01:44:25 obrien Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	cpp
SRCS=	gcc.c cppspec.c

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
