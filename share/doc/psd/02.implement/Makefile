#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/psd/02.implement/Makefile,v 1.8 2004/10/24 15:32:41 ru Exp $

VOLUME=		psd/02.implement
SRCS=		stubs implement
EXTRA=		ref.bib
MACROS=		-ms
USE_PIC=
USE_REFER=
USE_SOELIM=
CLEANFILES=	stubs

stubs:
	@(echo .R1; echo database ${.CURDIR}/ref.bib; \
	    echo accumulate; echo .R2) > ${.TARGET}

.include <bsd.doc.mk>
