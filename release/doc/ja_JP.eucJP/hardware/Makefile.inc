# $FreeBSD: src/release/doc/ja_JP.eucJP/hardware/Makefile.inc,v 1.1 2002/03/03 14:31:32 hrs Exp $
# Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ja_JP.eucJP/hardware/${.CURDIR:T}
.endif
