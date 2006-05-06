# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.31 2004/12/21 09:59:41 ru Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}

PROG=	cc
MAN=	gcc.1
SRCS=	gcc.c gccspec.c
NO_SHARED?=yes

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 CC.1

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
