# $FreeBSD$
# Original revision: 1.1.2.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ja_JP.eucJP/hardware/${.CURDIR:T}
.endif
