#	from: @(#)Makefile	6.3 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:30:59 mycroft Exp

VOLUME=	smm
DOC=	22.timed

SRCS=	timed.ms
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${SOELIM} ${SRCS} ) | ${TBL} | ${ROFF} > ${.TARGET}
