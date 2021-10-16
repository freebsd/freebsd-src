# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	drti \
	libavl \
	libctf \
	libdtrace \
	${_libicp} \
	${_libicp_rescue} \
	libnvpair \
	libspl \
	${_libtpool} \
	libumem \
	libuutil \
	${_libzfs_core} \
	${_libzfs} \
	${_libzfsbootenv} \
	${_libzpool} \
	${_libzutil} \
	${_pam_zfs_key}

SUBDIR.${MK_TESTS}+= tests

.if ${MK_ZFS} != "no"
_libzfs_core=	libzfs_core
_libicp=	libicp
_libicp_rescue=	libicp_rescue
_libzfs=	libzfs
_libzutil=	libzutil
_libzfsbootenv=	libzfsbootenv
_libzpool=	libzpool
_libtpool=	libtpool
.if ${MK_OPENSSL} != "no"
_pam_zfs_key=	pam_zfs_key
.endif
.endif

SUBDIR_DEPEND_libavl=		libspl
SUBDIR_DEPEND_libctf=		libspl
SUBDIR_DEPEND_libdtrace=	libctf
SUBDIR_DEPEND_libnvpair=	libspl
SUBDIR_DEPEND_libtpool=		libspl
SUBDIR_DEPEND_libuutil=		libavl libspl
SUBDIR_DEPEND_libzfs_core=	libnvpair libspl libzutil
SUBDIR_DEPEND_libzfs=	libavl libnvpair libumem libuutil libzfs_core libzutil
SUBDIR_DEPEND_libzpool=	libavl libnvpair libumem libicp
SUBDIR_DEPEND_libzutil=	libavl libtpool
SUBDIR_DEPEND_libzfsbootenv= libzfs libnvpair
SUBDIR_DEPEND_pam_zfs_key= libnvpair libuutil libzfs libzfs_core

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
