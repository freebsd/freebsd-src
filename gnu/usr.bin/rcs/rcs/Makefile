# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.8.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
