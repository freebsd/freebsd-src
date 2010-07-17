# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.22.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCLIB}/libiberty

PROG=	c++filt
SRCS=	cp-demangle.c
NO_MAN=

CFLAGS+= -DSTANDALONE_DEMANGLER -DVERSION=\"$(GCC_VERSION)\"

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
