# $FreeBSD$

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
