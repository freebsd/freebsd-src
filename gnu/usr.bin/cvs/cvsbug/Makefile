#	$Id: Makefile,v 1.1 1995/12/10 23:05:53 peter Exp $

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


beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		cvsbug ${DESTDIR}${BINDIR}/cvsbug

.include "../../Makefile.inc"
.include <bsd.prog.mk>

