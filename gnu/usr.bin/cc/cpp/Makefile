# $FreeBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.18.2.1 2000/07/04 05:34:54 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	gcc.c cppspec.c
NOMAN=	1

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT} ${LIBCC_FBSD}
LDADD+=	${LIBCC_INT} ${LIBCC_FBSD}

.include <bsd.prog.mk>
