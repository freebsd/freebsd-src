# $FreeBSD$

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
