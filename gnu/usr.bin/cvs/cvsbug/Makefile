# $FreeBSD$

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

SCRIPTS=	cvsbug
MAN=		cvsbug.8

CLEANFILES+=    cvsbug

cvsbug: cvsbug.in
	version=`sed < ${CVSDIR}/configure \
	    -e '/^[ 	]*VERSION=/!d' -e 's/.*=\(.*\)/\1/' -e q`; \
	sed -e "s,@VERSION@,$${version}-FreeBSD,g" ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
