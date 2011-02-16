# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.33.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

PROG=	cc
MAN=	gcc.1
SRCS+=  gccspec.c

NO_SHARED?=yes

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 CC.1

.include <bsd.prog.mk>
