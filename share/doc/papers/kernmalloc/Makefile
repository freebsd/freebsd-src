#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.2.6.1 1997/03/15 23:34:20 jkh Exp $

VOLUME=	papers
DOC=	kernmalloc
SRCS=	kernmalloc.ms appendix.ms
SRCDIR=	${.OBJDIR}	# ack!
MACROS=	-ms
USE_TBL=	yes
USE_PIC=	yes
USE_EQN=	yes
CLEANFILES+=	kernmalloc.ms appendix.ms

kernmalloc.ms:	kernmalloc.t alloc.fig usage.tbl
	(cd ${.CURDIR}; ${SOELIM} kernmalloc.t) > ${.TARGET}

appendix.ms:	appendix.t
	vgrind -f < ${.CURDIR}/appendix.t > ${.TARGET}

.include <bsd.doc.mk>
