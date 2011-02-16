#	$NetBSD: Makefile,v 1.7 2000/06/14 20:22:19 matt Exp $
# $FreeBSD: src/usr.bin/xlint/llib/Makefile,v 1.13.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIBS=	llib-lposix.ln llib-lstdc.ln

FILES=	${LIBS}
FILESDIR= ${LINTLIBDIR}

CLEANFILES+= ${LIBS}

llib-lposix.ln: llib-lposix
	${LINT} ${LINTFLAGS} -Cposix ${.ALLSRC}

llib-lstdc.ln: llib-lstdc
	${LINT} ${LINTFLAGS} -Cstdc ${.ALLSRC}

.include <bsd.prog.mk>
