#	from: @(#)Makefile	5.2 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:31:54 mycroft Exp

DIR=	smm/10.newsop
DOC=	10.newsop

SSRC=	tmac.n standard.mn
SRCS=	tmac.n install.mn

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} ${DOC}.standard.${PRINTER}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}

${DOC}.standard.${PRINTER}: ${SSRC}
	( cd ${.CURDIR} ; ${ROFF} ${SSRC} ) > ${.TARGET}
