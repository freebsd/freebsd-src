# $FreeBSD: src/share/doc/smm/title/Makefile,v 1.5.2.2 2002/02/18 12:29:45 ru Exp $

VOLUME=	smm
SRCS=	Title
DOC=	Title

# unroff(1) requires a macro package as an argument
.if defined(PRINTERDEVICE) && ${PRINTERDEVICE} == "html"
MACROS=	-ms
.endif

.include <bsd.doc.mk>
