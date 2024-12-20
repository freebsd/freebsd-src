.PATH:		${ZFSSRC}
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${ZFSOSSRC}/spl
.PATH:		${OZFS}/module/zstd
.PATH:		${OZFS}/module/zstd/lib/common
.PATH:		${OZFS}/module/zstd/lib/compress
.PATH:		${OZFS}/module/zstd/lib/decompress
.PATH:		${OZFS}/module/icp/asm-aarch64/blake3
.PATH:		${OZFS}/module/icp/algs/blake3
ZFS_SRC=	zfs.c nvlist.c skein.c skein_block.c list.c
ZFS_SRC+=	zfs_zstd.c
ZFS_SRC+=	blake3.c blake3_generic.c blake3_impl.c
ZSTD_SRC+=	entropy_common.c error_private.c
ZSTD_SRC+=	fse_decompress.c hist.c
ZSTD_SRC+=	huf_decompress.c pool.c xxhash.c
ZSTD_SRC+=	zstd_common.c
ZSTD_SRC+=	zstd_ddict.c zstd_decompress.c zstd_decompress_block.c
ZSTD_SRC+=	zstd_double_fast.c zstd_fast.c zstd_lazy.c zstd_ldm.c

SRCS+=		${ZFS_SRC} ${ZSTD_SRC} ${ZFS_SRC_AS}

#
# Any file that needs the FreeBSD overrides that are in
# include/os/freebssd/spl/XXX needs to have these added to
# CFLAGS_EARLY.file.c. In general, we try to build out of the OpenZFS tree
# unaltered. There's a problem, though, that since we're building for a
# standalone environment that's neither userland nor kernel, we sometimes need
# special code and that's handled by the 'nested' includes where we either setup
# something just-so before we include the include/XXX file, or if we need to
# tweak something defined in that file.
#

ZFS_EARLY=	-I${ZFSSRC}/spl					\
		-I${ZFSOSINC}					\
		-I${ZFSOSINC}/spl				\
		-I${ZFSOSINC}/zfs

#
# For all files, though, we prepend the sys/ccompile.h file to the build which
# has a bunch of defines that are present in OpenSolaris / Illumos, but missing
# from FreeBSD.
#
.for i in ${ZFS_SRC} ${ZSTD_SRC}
CFLAGS.$i+=	-include ${ZFSOSINC}/spl/sys/ccompile.h -Wformat -Wall -I${OZFS}/include \
	-DNEED_SOLARIS_BOOLEAN
.endfor

CFLAGS_EARLY.blake3.c+= ${ZFS_EARLY} -DOMIT_SIMD
CFLAGS_EARLY.blake3_generic.c+= ${ZFS_EARLY} -DOMIT_SIMD
CFLAGS_EARLY.blake3_impl.c+= ${ZFS_EARLY} -DOMIT_SIMD
CFLAGS_EARLY.list.c+= ${ZFS_EARLY}
CFLAGS_EARLY.zfs_zstd.c+= ${ZFS_EARLY}
CFLAGS_EARLY.nvlist.c+= ${ZFS_EARLY}
CFLAGS_EARLY.zfs.c += ${ZFS_EARLY}

#
# zfs.c is special: we need to define HAS_ZSTD_ZFS to get zfssubr.c to initialize zstd
# properly. We need to have the cddl boot compat directory in the include path for zfssubr.c
# to be found, and we need a couple of other include paths for skein and lz4. Finally we
# temporarily need LDRSRC to pick up part.h until libsa has a way to look into partitions
# or enumerate them...
#
CFLAGS.zfs.c+=	-DHAS_ZSTD_ZFS					\
		-I${SYSDIR}/cddl/boot/zfs			\
		-I${LDRSRC}					\
		-I${SYSDIR}/crypto/skein			\
		-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4

#
# ZSTD coding style has some issues, so suppress clang's warnings. Also, zstd's
# use of BMI instrucitons is broken in this environment, so avoid them.
#
.for i in ${ZSTD_SRC}
CFLAGS.$i+=	-U__BMI__ ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
.endfor

CFLAGS.zfs_zstd.c+= -DIN_BASE -DIN_LIBSA

CFLAGS.blake3_impl.c+= -I${OZFS}/module/icp/algs/blake3 -I${OZFS}/module/icp/include -DIN_LIBSA

# Do not unroll skein loops, reduce code size
CFLAGS.skein_block.c+=	-DSKEIN_LOOP=111

CWARNFLAGS.zfs.c+= ${NO_WDANGLING_POINTER}
