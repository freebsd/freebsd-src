# $FreeBSD: src/release/doc/en_US.ISO8859-1/installation/Makefile.inc,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/installation/${.CURDIR:T}
.endif
