# Original Revision: 1.2
# $FreeBSD$

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/zh_CN.GB2312/installation/${.CURDIR:T}
.endif
