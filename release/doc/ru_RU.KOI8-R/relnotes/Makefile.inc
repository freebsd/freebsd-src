# The FreeBSD Russian Documentation Project
#
# $FreeBSD: src/release/doc/ru_RU.KOI8-R/relnotes/Makefile.inc,v 1.1.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/relnotes/Makefile.inc,v 1.1 2004/05/20 07:02:25 den Exp $
#
# Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ru_RU.KOI8-R/relnotes/${.CURDIR:T}
.endif
