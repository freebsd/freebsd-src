# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.28 2002/06/05 21:30:43 obrien Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}

PROG=	cc
MAN=	gcc.1
SRCS=	gcc.c gccspec.c
NOSHARED?=yes

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 CC.1

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
