# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	c++
SRCS=	gcc.c g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NOMAN=	1

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
