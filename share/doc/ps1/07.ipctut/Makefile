#	from: @(#)Makefile	6.5 (Berkeley) 1/2/91
#	Makefile,v 1.5 1993/07/31 15:33:50 mycroft Exp

DIR=	ps1/07.ipctut
DOC=	07.ipctut

SRCS=	tutor.me
MACROS=	-me
EXTRA=	accept.grn dgramread.c dgramsend.c gremlins pipe.c pipe.grn \
	socketpair.c socketpair.grn strchkread.c streamread.c streamwrite.c \
	udgramread.c udgramsend.c ustreamread.c ustreamwrite.c

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} ${EXTRA}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${GREMLIN} | ${ROFF} > ${.TARGET}
