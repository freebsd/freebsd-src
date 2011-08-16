# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_zfs} ${_zpool} ${_zfsd}

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
. if ${MK_CXX} != "no"
_zfsd=	zfsd
. endif
.endif

.include <bsd.subdir.mk>
