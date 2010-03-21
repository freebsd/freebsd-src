#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/papers/kernmalloc/Makefile,v 1.7.24.1 2010/02/10 00:26:20 kensmith Exp $

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
