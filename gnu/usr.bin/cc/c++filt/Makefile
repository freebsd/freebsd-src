# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.17.2.2 2005/02/13 07:23:07 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	c++filt
SRCS=	cp-demangle.c
NOMAN=

CFLAGS+= -DSTANDALONE_DEMANGLER -DIN_GCC -DVERSION=\"$(version)\"

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
