# $FreeBSD: src/gnu/usr.bin/binutils/gasp/Makefile,v 1.20 2004/03/17 18:38:07 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=	gasp
SRCS+=	gasp.c macro.c sb.c hash.c xregex.h
WARNS?=	2
CFLAGS+= -DBFD_ASSEMBLER
CFLAGS+= -I${.CURDIR}/../as/${TARGET_ARCH}-freebsd -I${.CURDIR}/../as
CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${SRCDIR} -I${SRCDIR}/gas -I${SRCDIR}/gas/config
DPADD=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}
CLEANFILES=	xregex.h

xregex.h:
	@echo '#include <regex.h>' >${.TARGET}

.include <bsd.prog.mk>
