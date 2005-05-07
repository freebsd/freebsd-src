# $FreeBSD: src/share/doc/papers/bufbio/Makefile,v 1.4.8.1 2005/02/13 07:23:20 obrien Exp $

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
