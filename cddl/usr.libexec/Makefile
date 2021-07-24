# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	${_zpool_influxdb}

.if ${MK_ZFS} != "no"
_zpool_influxdb= zpool_influxdb
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
