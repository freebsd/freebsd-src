# $FreeBSD: src/release/doc/fr_FR.ISO8859-1/relnotes/Makefile.inc,v 1.1.2.2 2003/01/18 18:06:34 gioria Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/relnotes/${.CURDIR:T}
.endif
