# $FreeBSD: src/gnu/usr.bin/cc/f77/Makefile,v 1.12.2.1 2000/07/04 05:37:17 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f ${GCCDIR}

PROG=	f77
SRCS=	gcc.c g77spec.c version.c

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD=	${LIBCC_INT} ${LIBCC_FBSD}
LDADD=	${LIBCC_INT} ${LIBCC_FBSD}

CLEANFILES= f77.1

f77.1: g77.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
