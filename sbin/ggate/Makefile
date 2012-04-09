# $FreeBSD: src/sbin/ggate/Makefile,v 1.11.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
