#	from: @(#)Makefile	5.1 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:30:49 mycroft Exp

DIR=	usd/04.csh
DOC=	04.csh

SRCS=	tabs csh.1 csh.2 csh.3 csh.4 csh.a csh.g

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${SRCS} ) > ${.TARGET}
