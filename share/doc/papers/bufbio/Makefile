# $FreeBSD: src/share/doc/papers/bufbio/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

VOLUME=		papers
DOC=		bio
SRCS=		bio.ms-patched
EXTRA=		bufsize.eps
MACROS=		-ms
USE_PIC=
CLEANFILES=	bio.ms-patched

bio.ms-patched: bio.ms
	sed "s;bufsize\.eps;${.CURDIR}/&;" ${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
