# $FreeBSD: src/gnu/usr.bin/rcs/rcs/Makefile,v 1.8 2001/03/27 14:59:05 ru Exp $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
