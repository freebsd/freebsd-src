#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

DIR=	psd/03.iosys
SRCS=	iosys
MACROS=	-ms

paper.ps: ${SRCS}
	${ROFF} ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
