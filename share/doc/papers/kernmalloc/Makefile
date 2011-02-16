#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/papers/kernmalloc/Makefile,v 1.7.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

VOLUME=		papers
DOC=		kernmalloc
SRCS=		kernmalloc.t appendix.ms
EXTRA=		alloc.fig usage.tbl
MACROS=		-ms
USE_EQN=
USE_PIC=
USE_SOELIM=
USE_TBL=
CLEANFILES=	appendix.ms

appendix.ms: appendix.t
	${GRIND} < ${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
