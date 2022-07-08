# $FreeBSD$

.PATH:		${ZFSSRC}
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${ZFSOSSRC}/spl
.PATH:		${OZFS}/module/zstd/lib/common
.PATH:		${OZFS}/module/zstd/lib/compress
.PATH:		${OZFS}/module/zstd/lib/decompress
ZFS_SRC=	zfs.c nvlist.c skein.c skein_block.c list.c
ZFS_SRC+=	zstd_shim.c
ZSTD_SRC+=	entropy_common.c error_private.c
ZSTD_SRC+=	fse_compress.c fse_decompress.c hist.c
ZSTD_SRC+=	huf_compress.c huf_decompress.c pool.c xxhash.c
ZSTD_SRC+=	zstd_common.c zstd_compress.c zstd_compress_literals.c
ZSTD_SRC+=	zstd_compress_sequences.c zstd_compress_superblock.c
ZSTD_SRC+=	zstd_ddict.c zstd_decompress.c zstd_decompress_block.c
ZSTD_SRC+=	zstd_double_fast.c zstd_fast.c zstd_lazy.c zstd_ldm.c
ZSTD_SRC+=	zstd_opt.c

CFLAGS+=	-DHAS_ZSTD_ZFS
SRCS+=		${ZFS_SRC} ${ZSTD_SRC}

CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

ZFS_EARLY=	-I${ZFSOSINC}					\
		-I${ZFSOSINC}/spl				\
		-I${ZFSOSINC}/zfs

.for i in ${ZFS_SRC} ${ZSTD_SRC}
CFLAGS.$i+=	-include ${ZFSOSINC}/spl/sys/ccompile.h
.endfor

CFLAGS_EARLY.list.c+= ${ZFS_EARLY}
CFLAGS_EARLY.zstd_shim.c+= ${ZFS_EARLY}

# Can't use the early flags because there's two conflicting definitions of boolean_t in
# the zfs code that need to be unified.
CFLAGS.nvlist.c+= -I${ZFSOSINC}/spl
CFLAGS.zfs.c+=	-I${ZFSOSINC}/spl 				\
		-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4

#
# ZSTD coding style has some issues, so suppress clang's warnings. Also, zstd's
# use of BMI instrucitons is broken in this environment, so avoid them.
#
.for i in ${ZSTD_SRC}
CFLAGS.$i+=	-U__BMI__ ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
.endfor

CFLAGS.zstd_shim.c+= -DIN_BASE -DIN_LIBSA -I${OZFS}/include

# Do not unroll skein loops, reduce code size
CFLAGS.skein_block.c+=	-DSKEIN_LOOP=111

CFLAGS+=	-I${SYSDIR}/contrib/openzfs/include
CFLAGS+=	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
CFLAGS.zfs.c+=	-I${SYSDIR}/cddl/contrib/opensolaris/common/lz4

CFLAGS+=	-Wformat -Wall
