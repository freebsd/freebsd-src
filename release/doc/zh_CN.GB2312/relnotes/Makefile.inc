# The FreeBSD Simplified Chinese Project
# Original Revision: 1.2
#
# $FreeBSD$

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/zh_CN.GB2312/relnotes/${.CURDIR:T}
.endif
