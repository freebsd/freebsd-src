#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:32:41 mycroft Exp

DIR=	smm/04.quotas
DOC=	04.quotas

SRCS=	quotas.ms
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}
