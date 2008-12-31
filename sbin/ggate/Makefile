# $FreeBSD: src/sbin/ggate/Makefile,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
