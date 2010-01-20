# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

.include <bsd.subdir.mk>
