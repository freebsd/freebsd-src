#	$Id: Makefile,v 1.4 1996/08/20 23:55:14 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man

MAN8=	cvsbug.8

CLEANFILES+= cvsbug ver

.SUFFIXES: .sh

all:	cvsbug

.sh:
	echo > ver cvs-`sed < ${CVSDIR}/src/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`
	sed -e "s,xVERSIONx,`cat ver`,g" $> > $@


beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		cvsbug ${DESTDIR}${BINDIR}/cvsbug

.include <bsd.prog.mk>
