#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=		psd/02.implement
SRCS=		stubs implement
EXTRA=		ref.bib
MACROS=		-ms
USE_PIC=	YES
USE_REFER=	YES
USE_SOELIM=	YES
CLEANFILES=	stubs

stubs:
	@(echo .R1; echo database ${.CURDIR}/ref.bib; \
	    echo accumulate; echo .R2) > ${.TARGET}

.include <bsd.doc.mk>
