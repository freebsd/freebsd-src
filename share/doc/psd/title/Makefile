# $FreeBSD: src/share/doc/psd/title/Makefile,v 1.5.2.1 2000/11/27 07:20:07 ru Exp $

VOLUME=	psd
SRCS=	Title
FILES=	${SRCS}
DOC=	Title

# unroff(1) requires a macro package as an argument
.if defined(PRINTERDEVICE) && ${PRINTERDEVICE} == "html"
MACROS=	-ms
.endif

.include <bsd.doc.mk>
