#	$Id: Makefile,v 1.3 1995/12/10 23:06:37 peter Exp $

MAN8=	cvsinit.8

EXAMPDIR=	/usr/share/examples/cvs
CLEANFILES+=	cvsinit

.SUFFIXES: .sh

all:	cvsinit

.sh:
	echo > ver \
	  cvs-`sed < ${.CURDIR}/../lib/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`
	sed -e 's,xLIBDIRx,$(EXAMPDIR),g' \
	    -e "s,xVERSIONx,`cat ver`,g" ${.CURDIR}/$@.sh > $@

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		cvsinit ${DESTDIR}${BINDIR}/cvsinit

.include "../../Makefile.inc"
.include <bsd.prog.mk>

