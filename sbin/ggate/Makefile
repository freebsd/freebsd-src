# $FreeBSD: src/sbin/ggate/Makefile,v 1.10.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
