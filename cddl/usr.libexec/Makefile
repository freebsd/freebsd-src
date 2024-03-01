
.include <src.opts.mk>

SUBDIR=	${_zfs_prepare_disk} \
	${_zpool_influxdb}

.if ${MK_ZFS} != "no"
_zpool_influxdb= zpool_influxdb
_zfs_prepare_disk= zfs_prepare_disk
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
