#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:41:14 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		ranlib
SRCS=		ar.o is-ranlib.o
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty

.include <bsd.prog.mk>
