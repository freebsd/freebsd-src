#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=		papers
DOC=		kernmalloc
SRCS=		kernmalloc.t appendix.ms
EXTRA=		alloc.fig usage.tbl
MACROS=		-ms
USE_EQN=	YES
USE_PIC=	YES
USE_SOELIM=	YES
USE_TBL=	YES
CLEANFILES=	appendix.ms

appendix.ms: appendix.t
	${GRIND} < ${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
