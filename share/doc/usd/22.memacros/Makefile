#	from: @(#)Makefile	6.4 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:29:52 mycroft Exp

VOLUME=	usd
DOC=	22.memacros

SRCS=	intro.me
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${COMPAT} ${SRCS} ) > ${.TARGET}
