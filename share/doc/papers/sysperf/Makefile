#	From: @(#)Makefile	1.6 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=	papers
DOC=	sysperf
MACROS= -ms
LSRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t 7.t
EXTRA=	a1.t a2.t
SRCS=	paper.tmp appendix.tmp
CLEANFILES+=${SRCS}
SRCDIR=	${.OBJDIR}
USE_EQN=	yes

paper.tmp: ${LSRCS}
	(cd ${.CURDIR}; ${TBL} ${LSRCS}) > paper.tmp

appendix.tmp: a1.t a2.t
	${GRIND} ${.CURDIR}/a1.t |  awk '/\.\(\)/{ cnt = 2 } \
	   { if (cnt) cnt -= 1; else print $$0; } ' > appendix.tmp
	${GRIND} -lcsh ${.CURDIR}/a2.t |  awk '/\.\(\)/{ cnt = 2 } \
	   { if (cnt) cnt -= 1; else print $$0; } ' >> appendix.tmp

.include <bsd.doc.mk>
