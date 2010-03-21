# $FreeBSD: src/cddl/sbin/Makefile,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

.include <bsd.subdir.mk>
