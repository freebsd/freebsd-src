# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.7.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	dtrace \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
