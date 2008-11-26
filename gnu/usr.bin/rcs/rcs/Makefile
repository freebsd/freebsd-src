# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.8.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
