#
# $Id: Makefile,v 1.3 1998/06/03 18:00:58 peter Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		objcopy
SRCS=		objcopy.c not-strip.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a

.include <bsd.prog.mk>
