# $FreeBSD: src/release/doc/ja_JP.eucJP/relnotes/Makefile.inc,v 1.2.2.1 2001/09/24 12:51:37 hrs Exp $
# Original revision: 1.1.2.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ja_JP.eucJP/relnotes/${.CURDIR:T}
.endif

