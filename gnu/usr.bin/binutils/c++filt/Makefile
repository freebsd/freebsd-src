#
# $Id: Makefile,v 1.2 1998/06/03 18:00:56 peter Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

PROG=		c++filt
SRCS=		cplus-dem.c underscore.c
NOMAN=		true
CFLAGS+=	-DMAIN
CFLAGS+=	-DVERSION=\"${VERSION}\"
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libiberty/libiberty.a

.include <bsd.prog.mk>
