# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	c++filt
SRCS=	cp-demangle.c
NOMAN=

CFLAGS+= -DSTANDALONE_DEMANGLER -DIN_GCC -DVERSION=\"$(version)\"

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
