#	@(#)Makefile	5.3 (Berkeley) 6/8/93

DIR=	papers/px
SRCS=	pxin0.n pxin1.n pxin2.n pxin3.n pxin4.n
EXTRA=	fig1.1.n fig1.2.n fig1.3.n fig2.3.raw fig2.4.n fig3.2.n \
	fig3.3.n table2.1.n table2.2.n table2.3.n table3.1.n tmac.p
CLEANFILES+=fig2.3.n

paper.ps: ${SRCS} fig2.3.n
	${SOELIM} ${SRCS} | ${TBL} | ${ROFF} > ${.TARGET}

fig2.3.n: fig2.3.raw
	sort fig2.3.raw >fig2.3.n

.include <bsd.doc.mk>
