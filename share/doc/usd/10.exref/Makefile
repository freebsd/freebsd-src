#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.2.4.1 1995/09/17 11:19:50 davidg Exp $

VOLUME=	usd/10.exref
SRCS=	ex.rm
MACROS=	-ms
CLEANFILES=summary.*
SRCDIR=	${.CURDIR}/../../../../usr.bin/vi/USD.doc/exref
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
