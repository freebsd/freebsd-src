# $FreeBSD: src/gnu/usr.bin/binutils/gasp/Makefile,v 1.6.2.1 2000/07/06 22:15:50 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=		gasp
SRCS+=		gasp.c macro.c sb.c hash.c
CFLAGS+=	-I${SRCDIR}
CFLAGS+=	-I${SRCDIR}/gas
CFLAGS+=	-I${SRCDIR}/gas/config
CFLAGS+=	-I${.CURDIR}/../as/${TARGET_ARCH}-freebsd
CFLAGS+=	-DBFD_ASSEMBLER
DPADD=		${RELTOP}/libiberty/libiberty.a
LDADD=		${DPADD}

.include <bsd.prog.mk>
