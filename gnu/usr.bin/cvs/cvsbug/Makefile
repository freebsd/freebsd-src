#	$Id: Makefile,v 1.2 1995/12/11 01:58:51 peter Exp $

MAN8=	cvsbug.8

CLEANFILES+= cvsbug ver

.SUFFIXES: .sh

all:	cvsbug

.sh:
	echo > ver cvs-`sed < ${.CURDIR}/../lib/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`
	sed -e "s,xVERSIONx,`cat ver`,g" ${.CURDIR}/$@.sh > $@


beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		cvsbug ${DESTDIR}${BINDIR}/cvsbug

.include "../../Makefile.inc"
.include <bsd.prog.mk>

