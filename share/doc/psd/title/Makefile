# $FreeBSD$

VOLUME=	psd
SRCS=	Title
FILES=	${SRCS}
DOC=	Title

# unroff(1) requires a macro package as an argument
.if defined(PRINTERDEVICE) && ${PRINTERDEVICE} == "html"
MACROS=	-ms
.endif

.include <bsd.doc.mk>
