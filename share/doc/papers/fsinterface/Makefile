#	@(#)Makefile	5.3 (Berkeley) 6/8/93

DIR=	papers/fsinterface
SRCS=	fsinterface.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
