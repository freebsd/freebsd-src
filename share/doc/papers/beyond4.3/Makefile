#	@(#)Makefile	5.2 (Berkeley) 6/8/93

SRCS=	beyond43.ms
DOC=	beyond43
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
