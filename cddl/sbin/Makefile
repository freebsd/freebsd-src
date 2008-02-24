# $FreeBSD: src/cddl/sbin/Makefile,v 1.2 2007/04/16 21:20:25 ru Exp $

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

.include <bsd.subdir.mk>
