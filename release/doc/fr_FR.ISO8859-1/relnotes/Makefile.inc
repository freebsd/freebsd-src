# $FreeBSD: src/release/doc/fr_FR.ISO8859-1/relnotes/Makefile.inc,v 1.2.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/relnotes/${.CURDIR:T}
.endif
