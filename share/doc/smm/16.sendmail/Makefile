#	from: @(#)Makefile	5.3 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:31:17 mycroft Exp

VOLUME=	smm
DOC=	16.sendmail

SRCS=	intro.me
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${PIC} ${SRCS} ) | ${ROFF} ${COMPAT} > ${.TARGET}
