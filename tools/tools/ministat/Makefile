# $FreeBSD: src/tools/tools/ministat/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $
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
