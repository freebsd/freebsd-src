#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:30:41 mycroft Exp

DIR=	usd/07.Mail
DOC=	07.Mail

SRCS=	mail0.nr mail1.nr mail2.nr mail3.nr mail4.nr mail5.nr mail6.nr \
	mail7.nr mail8.nr mail9.nr maila.nr
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${ROFF} ${COMPAT} > ${.TARGET}
