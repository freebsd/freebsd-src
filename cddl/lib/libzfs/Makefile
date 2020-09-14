# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/module/icp
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/zcommon
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzfs
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzfs/os/freebsd
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libshare
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libshare/os/freebsd
.PATH: ${SRCTOP}/sys/contrib/openzfs/include
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/zstd
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/zstd/lib

PACKAGE=	runtime
LIB=	zfs
LIBADD= \
	avl \
	bsdxml \
	crypto \
	geom \
	m \
	md \
	nvpair \
	pthread \
	umem \
	util \
	uutil \
	z \
	zfs_core \
	zutil

INCS=	libzfs.h
USER_C = \
        libzfs_changelist.c \
        libzfs_config.c \
        libzfs_crypto.c \
        libzfs_dataset.c \
        libzfs_diff.c \
        libzfs_import.c \
        libzfs_iter.c \
        libzfs_mount.c \
        libzfs_pool.c \
        libzfs_sendrecv.c \
        libzfs_status.c \
        libzfs_util.c

# FreeBSD
USER_C += \
        libzfs_compat.c \
        libzfs_ioctl_compat.c \
        libzfs_zmount.c

# libshare
USER_C += \
	libshare.c \
	nfs.c \
	smb.c


KERNEL_C = \
        algs/sha2/sha2.c \
        cityhash.c \
        zfeature_common.c \
        zfs_comutil.c \
        zfs_deleg.c \
        zfs_fletcher.c \
        zfs_fletcher_superscalar.c \
        zfs_fletcher_superscalar4.c \
        zfs_namecheck.c \
        zfs_prop.c \
        zfs_uio.c \
        zpool_prop.c \
        zprop_common.c


ARCH_C =
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
ARCH_C += 	zfs_fletcher_intel.c \
		zfs_fletcher_sse.c 
CFLAGS +=  -DHAVE_SSE2
.endif
.if ${MACHINE_ARCH} == "amd64"
ARCH_C +=	zfs_fletcher_avx512.c
CFLAGS+= -DHAVE_AVX2 -DHAVE_AVX -D__x86_64 -DHAVE_AVX512F
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
ARCH_C +=	zfs_fletcher_aarch64_neon.c
.endif

SRCS= $(USER_C) $(KERNEL_C) $(ARCH_C)

WARNS?=	2
SHLIB_MAJOR= 4
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libshare
CFLAGS+= -I${SRCTOP}/sys/contrib/ck/include
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h


.include <bsd.lib.mk>
