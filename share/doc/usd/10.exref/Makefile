#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.4 1995/07/30 08:04:33 bde Exp $

VOLUME=	usd/10.exref
SRCS=	ex.rm
MACROS=	-ms
CLEANFILES=summary.*
SRCDIR=	${.CURDIR}/../../../../contrib/nvi/docs/USD.doc/exref
USE_TBL=	yes

.include <bsd.doc.mk>

.if !defined(NODOCCOMPRESS)
SFILE=	summary.${PRINTER}.gz
.else
SFILE=	summary.${PRINTER}
.endif

all:	${DFILE} ${SFILE}

${SFILE}: ex.summary
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) | ${GZIPCMD} > ${.TARGET}

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SFILE} ${DESTDIR}${BINDIR}/${VOLUME}
