# $FreeBSD: src/gnu/usr.bin/binutils/gasp/Makefile,v 1.6.2.2 2002/09/01 23:39:14 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=	gasp
SRCS+=	gasp.c macro.c sb.c hash.c
WARNS=	2
CFLAGS+= -DBFD_ASSEMBLER
CFLAGS+= -I${SRCDIR} -I${SRCDIR}/gas -I${SRCDIR}/gas/config
CFLAGS+= -I${.CURDIR}/../as/${TARGET_ARCH}-freebsd -I${.CURDIR}/../as
DPADD=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
