# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.20 2004/12/21 08:46:58 ru Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	c++filt
SRCS=	cp-demangle.c
NO_MAN=

CFLAGS+= -DSTANDALONE_DEMANGLER -DIN_GCC -DVERSION=\"$(version)\"

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
