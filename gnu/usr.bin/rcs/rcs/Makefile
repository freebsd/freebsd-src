# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.8.34.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
