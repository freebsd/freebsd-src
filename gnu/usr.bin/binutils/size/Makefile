#
# $Id$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		size
SRCS=		size.c
CFLAGS+=	-I${SRCDIR}/binutils
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty

.include <bsd.prog.mk>
