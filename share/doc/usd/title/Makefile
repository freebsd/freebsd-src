# $FreeBSD: src/share/doc/usd/title/Makefile,v 1.5.2.1 2000/11/27 07:20:08 ru Exp $

VOLUME=	usd
SRCS=	Title
FILES=	${SRCS}
DOC=	Title

# unroff(1) requires a macro package as an argument
.if defined(PRINTERDEVICE) && ${PRINTERDEVICE} == "html"
MACROS=	-ms
.endif

.include <bsd.doc.mk>
