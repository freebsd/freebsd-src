#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.3 2004/02/01 15:18:01 jmc Exp $

DOC=	bc
DIR=	usd/06.bc
SRCS=	bc
MACROS=	-ms
BINDIR=	/usr/share/doc/papers

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
