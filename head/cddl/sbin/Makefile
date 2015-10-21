# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	${_tests} ${_zfs} ${_zfsd} ${_zpool}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
. if ${MK_CXX} != "no"
_zfsd=	zfsd
. endif
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
