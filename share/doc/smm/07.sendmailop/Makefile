#	from: @(#)Makefile	5.2 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:32:13 mycroft Exp

VOLUME=	smm
DOC=	07.sendmailop

SRCS=	op.me
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${PIC} ${SRCS} ) | ${EQN} | \
		${ROFF} ${COMPAT} > ${.TARGET}
