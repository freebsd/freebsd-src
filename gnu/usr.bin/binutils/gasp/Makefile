#
# $FreeBSD$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas

PROG=		gasp
SRCS+=		gasp.c macro.c sb.c hash.c
CFLAGS+=	-I${SRCDIR}
CFLAGS+=	-I${SRCDIR}/gas
CFLAGS+=	-I${SRCDIR}/gas/config
CFLAGS+=	-I${.CURDIR}/../as/${MACHINE_ARCH}-freebsd
CFLAGS+=	-DBFD_ASSEMBLER
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libiberty/libiberty.a
NOMAN=		1

.include <bsd.prog.mk>
