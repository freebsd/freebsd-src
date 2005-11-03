# Original Revision: 1.2
# $FreeBSD: src/release/doc/zh_CN.GB2312/installation/Makefile.inc,v 1.1 2004/10/15 06:43:45 delphij Exp $

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/zh_CN.GB2312/installation/${.CURDIR:T}
.endif
