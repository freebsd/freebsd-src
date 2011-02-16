# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.8.40.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
