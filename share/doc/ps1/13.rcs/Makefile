#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.5 1993/07/31 15:33:38 mycroft Exp

DIR=	ps1/13.rcs
DOC=	13.rcs

MSRC=	rcs.man
SRCS=	rcs.ms
MACS=	-ms
MMACS=	-mandoc

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} ${DOC}.man.${PRINTER}
	( cd ${.CURDIR} ; ${ROFF} ${MACS} ${SRCS} ) > ${.TARGET}

${DOC}.man.${PRINTER}: ${MSRC}
	( cd ${.CURDIR} ; ${ROFF} ${MMACS} ${MSRC} ) > ${.TARGET}
