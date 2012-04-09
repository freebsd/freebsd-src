# $FreeBSD: src/release/doc/ja_JP.eucJP/relnotes/Makefile.inc,v 1.2.40.1.8.1 2012/03/03 06:15:13 kensmith Exp $
# Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ja_JP.eucJP/relnotes/${.CURDIR:T}
.endif

