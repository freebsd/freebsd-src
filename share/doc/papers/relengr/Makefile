#	From: @(#)Makefile	1.6 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/papers/relengr/Makefile,v 1.10.26.1 2010/12/21 17:10:29 kensmith Exp $

VOLUME=		papers
DOC=		releng
SRCS=		stubs 0.t 1.t 2.t 3.t
EXTRA=		ref.bib
MACROS=		-ms
USE_REFER=
CLEANFILES=	stubs

stubs:
	@(echo .R1; echo database ${.CURDIR}/ref.bib; echo .R2) > ${.TARGET}

.include <bsd.doc.mk>
