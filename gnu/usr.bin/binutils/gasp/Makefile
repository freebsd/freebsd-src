#
# $FreeBSD: src/gnu/usr.bin/binutils/gasp/Makefile,v 1.6 2000/02/21 20:33:29 obrien Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=		gasp
SRCS+=		gasp.c macro.c sb.c hash.c
CFLAGS+=	-I${SRCDIR}
CFLAGS+=	-I${SRCDIR}/gas
CFLAGS+=	-I${SRCDIR}/gas/config
CFLAGS+=	-I${.CURDIR}/../as/${TARGET_ARCH}-freebsd
CFLAGS+=	-DBFD_ASSEMBLER
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libiberty/libiberty.a

.include <bsd.prog.mk>
