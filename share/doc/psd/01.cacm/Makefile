#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/psd/01.cacm/Makefile,v 1.7.8.1 2005/02/13 07:23:22 obrien Exp $

VOLUME=		psd/01.cacm
SRCS=		stubs p.mac p1 p2 p3 p4 p5 p6
EXTRA=		ref.bib
MACROS=		-ms
USE_REFER=
USE_TBL=
CLEANFILES=	stubs

stubs:
	@(echo .R1; echo database ${.CURDIR}/ref.bib; \
	    echo accumulate; echo .R2) > ${.TARGET}

.include <bsd.doc.mk>
