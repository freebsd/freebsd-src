#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:36:17 deraadt Exp $


DIR=	psd/17.m4
SRCS=	m4.ms
MACROS=	-msU

paper.ps: ${SRCS}
	${ROFF} ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
