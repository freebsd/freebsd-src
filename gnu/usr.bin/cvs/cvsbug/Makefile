#	$Id: Makefile,v 1.2 1995/07/25 00:31:51 bde Exp $

MAN8=	cvsbug.8

CLEANFILES+= cvsbug

.SUFFIXES: .sh

all:	cvsbug

.sh:
	echo > ver cvs-`sed < ${.CURDIR}/../lib/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`
	sed -e "s,xVERSIONx,`cat ver`,g" ${.CURDIR}/$@.sh > $@


afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		cvsbug ${DESTDIR}${BINDIR}/cvsbug

.include "../../Makefile.inc"
.include <bsd.prog.mk>

