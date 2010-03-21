# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.7.2.2.2.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	dtrace \
	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
