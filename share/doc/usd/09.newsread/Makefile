#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.5 1993/07/31 15:30:35 mycroft Exp

DIR=	usd/09.newsread
DOC=	09.newsread

CSRC=	../../smm/10.newsop/tmac.n copyright.mn
SRCS=	../../smm/10.newsop/tmac.n howto.mn

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} ${DOC}.copyright.${PRINTER}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}

${DOC}.copyright.${PRINTER}: ${CSRC}
	( cd ${.CURDIR} ; ${ROFF} ${CSRC} ) > ${.TARGET}
