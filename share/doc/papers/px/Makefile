#	From: @(#)Makefile	5.3 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=		papers
DOC=		px
SRCS=		tmac.p pxin0.n pxin1.n pxin2.n pxin3.n pxin4.n
EXTRA=		fig1.1.n fig1.2.n fig1.3.n fig2.3.n fig2.4.n fig3.2.n \
		fig3.3.n table2.1.n table2.2.n table2.3.n table3.1.n
MACROS=		-ms
USE_SOELIM=	YES
USE_TBL=	YES
CLEANFILES=	fig2.3.n

fig2.3.n: fig2.3.raw
	sort ${.ALLSRC} >${.TARGET}

.include <bsd.doc.mk>
