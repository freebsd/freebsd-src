# $FreeBSD: src/share/doc/smm/title/Makefile,v 1.5.2.1 2000/11/24 16:59:16 ru Exp $

VOLUME=	smm
SRCS=	Title
FILES=	${SRCS}
DOC=	Title

# unroff(1) requires a macro package as an argument
.if defined(PRINTERDEVICE) && ${PRINTERDEVICE} == "html"
MACROS=	-ms
.endif

.include <bsd.doc.mk>
