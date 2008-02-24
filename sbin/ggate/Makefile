# $FreeBSD: src/sbin/ggate/Makefile,v 1.10 2007/10/09 23:31:10 obrien Exp $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
