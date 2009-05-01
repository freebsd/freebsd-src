# $FreeBSD: src/cddl/sbin/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

.include <bsd.subdir.mk>
