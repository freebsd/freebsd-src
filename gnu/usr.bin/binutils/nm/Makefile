#
# $Id: Makefile,v 1.2 1998/03/12 02:55:34 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		nm
SRCS=		nm.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a

.include <bsd.prog.mk>
