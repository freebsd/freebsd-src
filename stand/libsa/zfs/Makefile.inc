# $FreeBSD$

.PATH:		${ZFSSRC}
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${ZFSOSSRC}/spl
.PATH:		${OZFS}/module/zstd
.PATH:		${OZFS}/module/zstd/lib/common
.PATH:		${OZFS}/module/zstd/lib/compress
.PATH:		${OZFS}/module/zstd/lib/decompress
.PATH:		${OZFS}/module/icp/algs/blake3
ZFS_SRC=	zfs.c nvlist.c skein.c skein_block.c list.c
ZFS_SRC+=	zfs_zstd.c
ZFS_SRC+=	blake3.c blake3_generic.c blake3_impl_hack.c
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

CFLAGS_EARLY.blake3.c+= ${ZFS_EARLY}
CFLAGS_EARLY.blake3_generic.c+= ${ZFS_EARLY}
CFLAGS_EARLY.blake3_impl_hack.c+= ${ZFS_EARLY}
CFLAGS_EARLY.list.c+= ${ZFS_EARLY}
CFLAGS_EARLY.zfs_zstd.c+= ${ZFS_EARLY}
CFLAGS_EARLY.nvlist.c+= ${ZFS_EARLY}

# Can't use the early flags because there's two conflicting definitions of boolean_t in
# the zfs code that need to be unified, as well as a number of other hacks for pre-openzfs
# code still in the tree that needs to be fixed.
CFLAGS.zfs.c+=	-I${ZFSOSINC}/spl				\
		-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4 \
		-I${ZFSOSINC}/zfs				\
		-I${OZFS}/include

#
# ZSTD coding style has some issues, so suppress clang's warnings. Also, zstd's
# use of BMI instrucitons is broken in this environment, so avoid them.
#
.for i in ${ZSTD_SRC}
CFLAGS.$i+=	-U__BMI__ ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
.endfor

CFLAGS.zfs_zstd.c+= -DIN_BASE -DIN_LIBSA

# Do not unroll skein loops, reduce code size
CFLAGS.skein_block.c+=	-DSKEIN_LOOP=111

# To puck up blake3_impl.c...
CFLAGS.blake3_impl_hack.c+= -I${OZFS}/module/icp/algs/blake3
