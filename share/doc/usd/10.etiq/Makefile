#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.5 1993/07/31 15:30:28 mycroft Exp

DIR=	usd/10.etiq
DOC=	10.etiq

SRCS=	../../smm/10.newsop/tmac.n manner.mn

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}
