#	from: @(#)Makefile	6.3 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:32:03 mycroft Exp

DIR=	smm/08.timedop
DOC=	08.timedop

SRCS=	timed.ms
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}
