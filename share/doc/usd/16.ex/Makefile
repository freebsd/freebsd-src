#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:30:05 mycroft Exp

DIR=	usd/16.ex
DOC=	16.ex

SRCS=	ex.rm
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} ${DOC}.summary.${PRINTER}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}

${DOC}.summary.${PRINTER}: ex.summary
	( cd ${.CURDIR} ; ${TBL} ex.summary ) | ${ROFF} > ${.TARGET}
