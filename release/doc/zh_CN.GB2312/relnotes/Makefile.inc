# The FreeBSD Simplified Chinese Project
# Original Revision: 1.2
#
# $FreeBSD: src/release/doc/zh_CN.GB2312/relnotes/Makefile.inc,v 1.1 2004/10/15 06:43:47 delphij Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/zh_CN.GB2312/relnotes/${.CURDIR:T}
.endif
