#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:33:32 mycroft Exp

VOLUME=	ps1
DOC=	14.sccs

SRCS=	sccs.me
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${ROFF} ${COMPAT} ${SRCS} ) > ${.TARGET}
