#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	smm/05.fastfs
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${EQN} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
