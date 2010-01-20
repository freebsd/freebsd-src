#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.2 2004/02/01 15:18:01 jmc Exp $

DOC=	dc
DIR=	usd/05.dc
SRCS=	dc
MACROS=	-ms
BINDIR=	/usr/share/doc/papers

paper.ps: ${SRCS}
	${EQN} ${SRCS} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${EQN} -Tascii ${SRCS} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
