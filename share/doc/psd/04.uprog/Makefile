#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

DIR=	psd/04.uprog
SRCS=	p.mac p0 p1 p2 p3 p4 p5 p6 p8 p9
MACROS=	-ms

paper.ps: ${SRCS}
	${ROFF} ${MAC} ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
