# $FreeBSD: src/gnu/usr.bin/cvs/cvsbug/Makefile,v 1.11.2.4 2002/07/17 15:04:06 ru Exp $

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

SCRIPTS=	cvsbug
MAN=		cvsbug.8

CLEANFILES+=    cvsbug version.c
VERSION!=       sed < ${CVSDIR}/configure \
		-e '/^VERSION=/!d' -e 's/.*=\(.*\)/\1/' -e q

version.c: version.c.in
	sed -e "s,@VERSION@,${VERSION}-FreeBSD,g" ${.ALLSRC} > ${.TARGET}

cvsbug: cvsbug.in
	sed -e "s,@VERSION@,${VERSION}-FreeBSD,g" ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
