# $FreeBSD: src/release/doc/fr_FR.ISO8859-1/relnotes/Makefile.inc,v 1.1 2002/02/19 09:37:04 gioria Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/relnotes/${.CURDIR:T}
.endif
