#
# $Id$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		objdump
SRCS=		objdump.c prdbg.c
CFLAGS+=	-I${SRCDIR}/binutils
LDADD+=		-L${RELTOP}/libbinutils -lbinutils
LDADD+=		-L${RELTOP}/libopcodes -lopcodes
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty

.include <bsd.prog.mk>
