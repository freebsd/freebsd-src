# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.7.12.1 2001/04/25 12:03:54 ru Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
