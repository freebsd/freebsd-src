# $FreeBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.22 2002/06/05 21:30:45 obrien Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	gcc.c cppspec.c

CFLAGS+= -DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+= -DDEFAULT_TARGET_MACHINE=\"$(target)\"

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

CLEANFILES= cpp.1

cpp.1: cccp.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
