# The FreeBSD Russian Documentation Project
#
# $FreeBSD$
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/hardware/Makefile.inc,v 1.1 2004/05/27 06:09:42 den Exp $
#
# Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ru_RU.KOI8-R/hardware/${.CURDIR:T}
.endif
