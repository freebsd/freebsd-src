#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:40:59 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas ${SRCDIR}/gas/config

PROG=		as
MAN1=		doc/as.1
SRCS=		app.c as.c atof-generic.c atof-ieee.c bignum-copy.c \
		cond.c ecoff.c expr.c flonum-copy.c flonum-konst.c \
		flonum-mult.c frags.c hash.c input-file.c \
		input-scrub.c listing.c literal.c macro.c messages.c \
		obj-elf.c output-file.c read.c sb.c stabs.c \
		subsegs.c symbols.c tc-alpha.c write.c
CFLAGS+=	-I${SRCDIR}/gas
CFLAGS+=	-I${SRCDIR}/gas/config
CFLAGS+=	-I${SRCDIR}
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
LDADD+=		-L${RELTOP}/libopcodes -lopcodes

.include <bsd.prog.mk>
