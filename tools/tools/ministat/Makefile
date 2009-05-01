# $FreeBSD: src/tools/tools/ministat/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $
PROG=	ministat
NO_MAN=
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} < ${.CURDIR}/chameleon 
	./${PROG} ${.CURDIR}/chameleon 
	./${PROG} ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana ${.CURDIR}/iguana
