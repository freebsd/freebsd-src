# $FreeBSD: src/release/doc/en_US.ISO8859-1/hardware/Makefile.inc,v 1.2 2001/09/03 14:39:31 dd Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/hardware/${.CURDIR:T}
.endif
