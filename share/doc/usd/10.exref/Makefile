#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.1 1995/01/11 03:24:10 wollman Exp $

VOLUME=	usd/10.exref
SRCS=	ex.rm
MACROS=	-ms
CLEANFILES=summary.*
SRCDIR=	${.CURDIR}/../../../../usr.bin/vi/USD.doc/exref
USE_TBL=	yes

.include <bsd.doc.mk>

all:	paper.${PRINTER} summary.${PRINTER}

summary.${PRINTER}: ex.summary
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) > ${.TARGET}

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		summary.${PRINTER} ${DESTDIR}${BINDIR}/${VOLUME}
