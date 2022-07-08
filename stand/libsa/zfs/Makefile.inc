# $FreeBSD$

.PATH:		${ZFSSRC}
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${ZFSOSSRC}/spl
.PATH:		${OZFS}/module/zstd/lib
ZFS_SRC=	zfs.c nvlist.c skein.c skein_block.c list.c
ZFS_SRC+=	zstd_shim.c zstd.c
CFLAGS+=	-DHAS_ZSTD_ZFS
SRCS+=		${ZFS_SRC}

CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

ZFS_EARLY=	-I${ZFSSRC}/spl					\
		-I${ZFSOSINC}					\
		-I${ZFSOSINC}/spl				\
		-I${ZFSOSINC}/zfs

.for i in ${ZFS_SRC}
CFLAGS.$i+=	-include ${ZFSOSINC}/spl/sys/ccompile.h
.endfor

CFLAGS_EARLY.list.c+= ${ZFS_EARLY}
CFLAGS_EARLY.zstd_shim.c+= ${ZFS_EARLY}

# Can't use the early flags because there's two conflicting definitions of boolean_t in
# the zfs code that need to be unified.
CFLAGS.nvlist.c+= -I${ZFSOSINC}/spl
CFLAGS.zfs.c+=	-I${ZFSOSINC}/spl 				\
		-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4
CFLAGS.zstd.c+=	-U__BMI__
CFLAGS.zstd.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_shim.c+= -DIN_BASE -I${OZFS}/include

# Do not unroll skein loops, reduce code size
CFLAGS.skein_block.c+=	-DSKEIN_LOOP=111

CFLAGS+=	-I${SYSDIR}/contrib/openzfs/include
CFLAGS+=	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
CFLAGS.zfs.c+=	-I${SYSDIR}/cddl/contrib/opensolaris/common/lz4

CFLAGS+=	-Wformat -Wall
