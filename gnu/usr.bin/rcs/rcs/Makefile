# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.8.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
