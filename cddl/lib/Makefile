
.include <src.opts.mk>

.if ${MK_DTRACE} == yes || ${MK_ZFS} == yes
SUBDIR=	libspl
.endif

SUBDIR.${MK_DTRACE}+= \
	drti \
	libctf \
	libdtrace

SUBDIR.${MK_TESTS}+= tests

SUBDIR.${MK_ZFS}+= \
	libavl \
	libicp \
	libicp_rescue \
	libnvpair \
	libtpool \
	libumem \
	libuutil \
	libzfs \
	libzfs_core \
	libzfsbootenv \
	libzpool \
	libzutil

SUBDIR.${MK_ZFS}.${MK_OPENSSL} = pam_zfs_key

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
