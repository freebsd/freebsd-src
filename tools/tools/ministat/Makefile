# $FreeBSD: src/tools/tools/ministat/Makefile,v 1.5.2.1.10.1 2008/10/02 02:57:24 kensmith Exp $
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
