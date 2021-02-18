# $FreeBSD$

.include <src.opts.mk>

SUBDIR= ${_zfs}

.if ${MK_ZFS} != "no"
_zfs=   zfs
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
