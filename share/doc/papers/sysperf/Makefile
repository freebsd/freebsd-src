#	@(#)Makefile	1.6 (Berkeley) 6/8/93

DIR=	papers/sysperf
MACROS= -ms
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t 7.t
EXTRA=	a1.t a2.t
OBJS=	paper.tmp appendix.tmp
CLEANFILES+=${OBJS}

paper.ps: ${OBJS}
	${ROFF} ${OBJS} > ${.TARGET}

paper.tmp: ${SRCS}
	${TBL} ${SRCS} | ${EQN} > paper.tmp

appendix.tmp: a1.t a2.t
	${GRIND} -f a1.t |  awk '/\.\(\)/{ cnt = 2 } \
	   { if (cnt) cnt -= 1; else print $$0; } ' > appendix.tmp
	${GRIND} -f -lcsh a2.t |  awk '/\.\(\)/{ cnt = 2 } \
	   { if (cnt) cnt -= 1; else print $$0; } ' >> appendix.tmp

.include <bsd.doc.mk>
