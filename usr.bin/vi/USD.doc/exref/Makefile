#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	usd/13.ex
SRCS=	ex.rm
MACROS=	-msU
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${ROFF} ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
