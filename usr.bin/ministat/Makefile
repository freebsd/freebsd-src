# $FreeBSD: src/usr.bin/ministat/Makefile,v 1.8.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
PROG=	ministat
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} < ${.CURDIR}/chameleon 
	./${PROG} ${.CURDIR}/chameleon 
	./${PROG} ${.CURDIR}/iguana ${.CURDIR}/chameleon
	./${PROG} -c 80 ${.CURDIR}/iguana ${.CURDIR}/chameleon
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana ${.CURDIR}/iguana
