# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.10.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	dtrace \
	lockstat \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
