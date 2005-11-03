# $FreeBSD: src/sbin/ggate/Makefile,v 1.2 2005/07/10 15:16:49 pjd Exp $

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if !defined(NO_LIBPTHREAD)
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
