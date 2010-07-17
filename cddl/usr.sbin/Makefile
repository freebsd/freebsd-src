# $FreeBSD: src/cddl/usr.sbin/Makefile,v 1.10.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
