# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	gcc.c cppspec.c
NOMAN=	1

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
