# $FreeBSD: src/gnu/usr.bin/cvs/cvsbug/Makefile,v 1.11 2000/01/10 11:38:47 phantom Exp $

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man

MAN8=	cvsbug.8

CLEANFILES+= cvsbug ver

.SUFFIXES: .sh

all: cvsbug

ver: version.c
	echo > ${.TARGET} cvs-`sed < ${.ALLSRC} \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`

.sh: ver
	sed -e "s,xVERSIONx,`cat ver`,g" ${.IMPSRC} > ${.TARGET}

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		cvsbug ${DESTDIR}${BINDIR}/cvsbug

.include <bsd.prog.mk>
