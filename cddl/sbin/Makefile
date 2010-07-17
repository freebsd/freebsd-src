# $FreeBSD: src/cddl/sbin/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

.include <bsd.subdir.mk>
