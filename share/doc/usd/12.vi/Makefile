#	From: @(#)Makefile	8.1 (Berkeley) 8/14/93
#	$Id: Makefile,v 1.2 1995/07/26 13:52:59 wollman Exp $

VOLUME=	usd/12.vi
SRCS=	vi.in vi.chars
MACROS=	-ms
CLEANFILES+=summary.* viapwh.*
SRCDIR=	${.CURDIR}/../../../../usr.bin/vi/USD.doc/vitut
USE_TBL=	yes

.include <bsd.doc.mk>

.if !defined(NODOCCOMPRESS)
SFILE=	summary.${PRINTER}.gz
VFILE=	viapwh.${PRINTER}.gz
.else
SFILE=	summary.${PRINTER}
VFILE=	viapwh.${PRINTER}
.endif

all:	${DFILE} ${SFILE} ${VFILE}

${SFILE}: vi.summary
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) | ${GZIPCMD} > ${.TARGET}

${VFILE}: vi.apwh.ms
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) | ${GZIPCMD} > ${.TARGET}

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SFILE} ${VFILE} \
		${DESTDIR}${BINDIR}/${VOLUME}
