# $FreeBSD: src/gnu/usr.bin/cc/c++/Makefile,v 1.16.2.2 2002/06/20 23:13:30 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	c++
SRCS=	gcc.c g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NOMAN=	1

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT} ${LIBCC_FBSD}
LDADD+=	${LIBCC_INT} ${LIBCC_FBSD}

.include <bsd.prog.mk>
