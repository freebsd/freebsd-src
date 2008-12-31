# $FreeBSD: src/cddl/sbin/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

.include <bsd.subdir.mk>
