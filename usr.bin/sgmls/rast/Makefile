#
# Bmakefile for rast
#
# $Id$
#

PROG= rast

SRCS+= rast.c

CFLAGS+= -I${.CURDIR}/../libsgmls -I${.CURDIR}/../sgmls

LDADD=  ${LIBSGMLS}
DPADD=  ${LIBSGMLS}

.include "../Makefile.inc"
.include <bsd.prog.mk>

