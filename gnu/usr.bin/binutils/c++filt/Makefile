#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:40:51 jdp Exp $
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
