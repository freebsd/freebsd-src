# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.20.2.1 2000/07/04 05:34:53 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cc
MAN1=	gcc.1
SRCS=	gcc.c gccspec.c

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 CC.1

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT} ${LIBCC_FBSD}
LDADD+=	${LIBCC_INT} ${LIBCC_FBSD}

.include <bsd.prog.mk>
