# $FreeBSD: src/sbin/ggate/Makefile,v 1.2.12.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if !defined(NO_LIBPTHREAD)
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
