#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:41:15 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		size
SRCS=		size.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty

.include <bsd.prog.mk>
