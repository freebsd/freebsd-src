#
# $Id$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

PROG=		c++filt
SRCS=		cplus-dem.c underscore.c
NOMAN=		true
CFLAGS+=	-DMAIN
CFLAGS+=	-DVERSION=\"${VERSION}\"
LDADD+=		-L${RELTOP}/libiberty -liberty

.include <bsd.prog.mk>
