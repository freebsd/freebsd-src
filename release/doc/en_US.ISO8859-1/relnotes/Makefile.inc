# $FreeBSD: src/release/doc/en_US.ISO8859-1/relnotes/Makefile.inc,v 1.1.2.2 2001/09/04 12:12:14 dd Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/relnotes/${.CURDIR:T}
.endif
