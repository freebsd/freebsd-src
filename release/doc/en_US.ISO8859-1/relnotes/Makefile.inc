# $FreeBSD: src/release/doc/en_US.ISO8859-1/relnotes/Makefile.inc,v 1.2 2001/09/03 14:39:32 dd Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/relnotes/${.CURDIR:T}
.endif
