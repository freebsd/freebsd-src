# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.22.14.1 2010/12/21 17:10:29 kensmith Exp $

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
