#	from: @(#)Makefile	6.3 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:29:41 mycroft Exp

VOLUME=	usd
DOC=	33.rogue

SRCS=	rogue.me
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${ROFF} ${COMPAT} > ${.TARGET}
