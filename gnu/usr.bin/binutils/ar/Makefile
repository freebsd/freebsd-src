#
# $Id: Makefile,v 1.2 1998/03/12 02:54:55 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		ar
SRCS=		ar.c not-ranlib.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a

.include <bsd.prog.mk>
