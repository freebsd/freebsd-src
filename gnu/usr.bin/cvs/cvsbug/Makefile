# $FreeBSD$

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

SCRIPTS=	cvsbug
MAN=		cvsbug.8

CLEANFILES+= cvsbug ver

ver: configure
	echo > ${.TARGET} `sed < ${.ALLSRC} \
		    -e '/^VERSION=/!d' \
		    -e 's/.*=\(.*\)/\1/' \
		    -e q`

cvsbug: ver cvsbug.in
	sed -e "s,@VERSION@,`cat ver`,g" ${CVSDIR}/src/cvsbug.in > ${.TARGET}

.include <bsd.prog.mk>
