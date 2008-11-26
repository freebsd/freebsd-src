#	@(#)Makefile	8.1 (Berkeley) 8/14/93
# $FreeBSD: src/share/doc/psd/15.yacc/Makefile,v 1.8.16.1 2008/10/02 02:57:24 kensmith Exp $

VOLUME=		psd/15.yacc
SRCS=		stubs ss.. ss0 ss1 ss2 ss3 ss4 ss5 ss6 ss7 ss8 ss9 \
		ssA ssB ssa ssb ssc ssd
EXTRA=		ref.bib
MACROS=		-ms
USE_REFER=
CLEANFILES=	stubs

stubs:
	@(echo .R1; echo database ${.CURDIR}/ref.bib; \
	    echo accumulate; echo .R2) > ${.TARGET}

.include <bsd.doc.mk>
