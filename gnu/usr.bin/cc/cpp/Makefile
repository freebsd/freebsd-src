# $FreeBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.24.8.2.6.1 2008/10/02 02:57:24 kensmith Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	cpp
SRCS=	gcc.c cppspec.c

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
