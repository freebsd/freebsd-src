#	From: @(#)Makefile	8.1 (Berkeley) 8/14/93
#	Makefile,v 1.1 1995/01/11 03:24:14 wollman Exp

VOLUME=	usd/12.vi
SRCS=	vi.in vi.chars
MACROS=	-ms
CLEANFILES+=summary.* viapwh.*
SRCDIR=	${.CURDIR}/../../../../usr.bin/vi/USD.doc/vitut
USE_TBL=	yes

.include <bsd.doc.mk>

all:	paper.${PRINTER} summary.${PRINTER} viapwh.${PRINTER}

summary.${PRINTER}: vi.summary
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) > ${.TARGET}

viapwh.${PRINTER}: vi.apwh.ms
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) > ${.TARGET}

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		summary.${PRINTER} viapwh.${PRINTER} \
		${DESTDIR}${BINDIR}/${VOLUME}
