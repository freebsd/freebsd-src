# $FreeBSD: src/tools/tools/ministat/Makefile,v 1.3 2004/02/05 22:01:28 ru Exp $
PROG=	ministat
NOMAN=	yes
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} < ${.CURDIR}/chameleon 
	./${PROG} ${.CURDIR}/chameleon 
	./${PROG} ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana
