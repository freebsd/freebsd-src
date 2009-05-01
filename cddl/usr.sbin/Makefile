# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.7.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	dtrace \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
