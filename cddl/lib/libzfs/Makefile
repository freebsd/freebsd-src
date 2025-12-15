.PATH: ${ZFSTOP}/module/icp
.PATH: ${ZFSTOP}/module/zcommon
.PATH: ${ZFSTOP}/lib/libzfs
.PATH: ${ZFSTOP}/lib/libzfs/os/freebsd
.PATH: ${ZFSTOP}/lib/libshare
.PATH: ${ZFSTOP}/include
.PATH: ${ZFSTOP}/module/zstd
.PATH: ${ZFSTOP}/module/zstd/lib

PACKAGE=	zfs
LIB_PACKAGE=

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
	rt \
	umem \
	util \
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
        libzfs_zmount.c

# libshare
USER_C += \
	libshare.c \
	nfs.c \
	os/freebsd/nfs.c \
	os/freebsd/smb.c

KERNEL_C = \
        cityhash.c \
        zfeature_common.c \
        zfs_comutil.c \
        zfs_deleg.c \
        zfs_fletcher.c \
        zfs_fletcher_superscalar.c \
        zfs_fletcher_superscalar4.c \
        zfs_namecheck.c \
        zfs_prop.c \
        zfs_valstr.c \
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
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/include/os/freebsd
CFLAGS+= -I${ZFSTOP}/lib/libspl/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${ZFSTOP}/lib/libshare
CFLAGS+= -I${ZFSTOP}/lib/libzpool/include
CFLAGS+= -I${SRCTOP}/sys/contrib/ck/include
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${ZFSTOP}/module/icp/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -DHAVE_EXECVPE
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS+= -DSYSCONFDIR=\"/etc\"
CFLAGS+= -DPKGDATADIR=\"/usr/share/zfs\"
CFLAGS+= -DZFSEXECDIR=\"${LIBEXECDIR}/zfs\"

.include <bsd.lib.mk>
