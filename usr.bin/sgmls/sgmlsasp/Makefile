#
# Bmakefile for sgmlsasp
#
# $Id$
#

PROG= sgmlsasp

SRCS+= sgmlsasp.c replace.c

CFLAGS+= -I${.CURDIR}/../libsgmls -I${.CURDIR}/../sgmls

LDADD=  ${LIBSGMLS}
DPADD=  ${LIBSGMLS}

.include "../Makefile.inc"
.include <bsd.prog.mk>

