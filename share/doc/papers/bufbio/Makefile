# $FreeBSD: src/share/doc/papers/bufbio/Makefile,v 1.4 2002/10/30 15:20:50 ru Exp $

VOLUME=		papers
DOC=		bio
SRCS=		bio.ms-patched
EXTRA=		bufsize.eps
MACROS=		-ms
USE_PIC=	YES
CLEANFILES=	bio.ms-patched

bio.ms-patched: bio.ms
	sed "s;bufsize\.eps;${.CURDIR}/&;" ${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
