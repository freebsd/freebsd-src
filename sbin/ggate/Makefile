# $FreeBSD: src/sbin/ggate/Makefile,v 1.11.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
