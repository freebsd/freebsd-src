#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

DIR=	psd/06.Clang
SRCS=	Clang.ms
MACROS=	-msU

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
