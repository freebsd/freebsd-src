
ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

# ZFS_COMMON_SRCS
.PATH: ${ZFSTOP}/module/zfs
.PATH: ${ZFSTOP}/module/zcommon
.PATH: ${ZFSTOP}/module/unicode
# LUA_SRCS
.PATH: ${ZFSTOP}/module/lua
# ZSTD_SRCS
.PATH: ${ZFSTOP}/module/zstd
.PATH: ${ZFSTOP}/module/zstd/lib/common
.PATH: ${ZFSTOP}/module/zstd/lib/compress
.PATH: ${ZFSTOP}/module/zstd/lib/decompress

.PATH: ${ZFSTOP}/module/os/linux/zfs

.PATH: ${ZFSTOP}/lib/libzpool

.if exists(${SRCTOP}/sys/cddl/contrib/opensolaris/common/atomic/${MACHINE_ARCH}/opensolaris_atomic.S)
.PATH: ${SRCTOP}/sys/cddl/contrib/opensolaris/common/atomic/${MACHINE_ARCH}
ATOMIC_SRCS=	opensolaris_atomic.S
ACFLAGS+=	-Wa,--noexecstack
.else
.PATH: ${SRCTOP}/sys/cddl/compat/opensolaris/kern
ATOMIC_SRCS=	opensolaris_atomic.c
.endif

.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpcspe"
# Don't waste GOT entries on small data.
PICFLAG=	-fPIC
.endif

PACKAGE=	zfs
LIB=		zpool

USER_C = \
	kernel.c \
	taskq.c \
	util.c

KERNEL_C = \
	zfeature_common.c \
	zfs_comutil.c \
	zfs_deleg.c \
	zfs_fletcher.c \
	zfs_fletcher_superscalar.c \
	zfs_fletcher_superscalar4.c \
	zfs_namecheck.c \
	zfs_prop.c \
	zfs_zstd.c \
	zpool_prop.c \
	zprop_common.c \
	abd.c \
	abd_os.c \
	aggsum.c \
	arc.c \
	arc_os.c \
        blake3_zfs.c \
	blkptr.c \
	bplist.c \
	bpobj.c \
	bptree.c \
	bqueue.c \
	btree.c \
	brt.c \
	cityhash.c \
	dbuf.c \
	dbuf_stats.c \
	ddt.c \
	ddt_stats.c \
	ddt_zap.c \
	dmu.c \
	dmu_diff.c \
	dmu_object.c \
	dmu_objset.c \
	dmu_recv.c \
	dmu_redact.c \
	dmu_send.c \
	dmu_traverse.c \
	dmu_tx.c \
	dmu_zfetch.c \
	dnode.c \
	dnode_sync.c \
	dsl_bookmark.c \
	dsl_dataset.c \
	dsl_deadlist.c \
	dsl_deleg.c \
	dsl_dir.c \
	dsl_crypt.c \
	dsl_pool.c \
	dsl_prop.c \
	dsl_scan.c \
	dsl_synctask.c \
	dsl_destroy.c \
	dsl_userhold.c \
	edonr_zfs.c \
	entropy_common.c \
	error_private.c \
	fm.c \
	fse_compress.c \
	fse_decompress.c \
	gzip.c \
	hist.c \
	hkdf.c \
	huf_compress.c \
	huf_decompress.c \
	lzjb.c \
	lz4.c \
	lz4_zfs.c \
	metaslab.c \
	mmp.c \
	multilist.c \
	objlist.c \
	pathname.c \
	pool.c \
	range_tree.c \
	refcount.c \
	rrwlock.c \
	sa.c \
	sha2_zfs.c \
	skein_zfs.c \
	spa.c \
	spa_checkpoint.c \
	spa_config.c \
	spa_errlog.c \
	spa_history.c \
	spa_log_spacemap.c \
	spa_misc.c \
	spa_stats.c \
	space_map.c \
	space_reftree.c \
	txg.c \
	trace.c \
	uberblock.c \
	unique.c \
	vdev.c \
	vdev_draid.c \
	vdev_draid_rand.c \
	vdev_file.c \
	vdev_indirect_births.c \
	vdev_indirect.c \
	vdev_indirect_mapping.c \
	vdev_initialize.c \
	vdev_label.c \
	vdev_label_os.c \
	vdev_mirror.c \
	vdev_missing.c \
	vdev_queue.c \
	vdev_raidz.c \
	vdev_raidz_math_aarch64_neon.c \
	vdev_raidz_math_aarch64_neonx2.c \
	vdev_raidz_math_avx2.c \
	vdev_raidz_math_avx512bw.c \
	vdev_raidz_math_avx512f.c \
	vdev_raidz_math.c \
	vdev_raidz_math_scalar.c \
	vdev_rebuild.c \
	vdev_removal.c \
	vdev_root.c \
	vdev_trim.c \
	xxhash.c \
	zap.c \
	zap_leaf.c \
	zap_micro.c \
	zcp.c \
	zcp_get.c \
	zcp_global.c \
	zcp_iter.c \
	zcp_set.c \
	zcp_synctask.c \
	zfeature.c \
	zfs_byteswap.c \
        zfs_chksum.c \
	zfs_debug.c \
	zfs_fm.c \
	zfs_fuid.c \
	zfs_sa.c \
	zfs_znode.c \
	zfs_racct.c \
	zfs_ratelimit.c \
	zfs_rlock.c \
	zil.c \
	zio.c \
	zio_checksum.c \
	zio_compress.c \
	zio_crypt.c \
	zio_inject.c \
	zle.c \
	zrlock.c \
	zstd_common.c \
	zstd_compress.c \
	zstd_compress_literals.c \
	zstd_compress_sequences.c \
	zstd_compress_superblock.c \
	zstd_ddict.c \
	zstd_decompress.c \
	zstd_decompress_block.c \
	zstd_double_fast.c \
	zstd_fast.c \
	zstd_lazy.c \
	zstd_ldm.c \
	zstd_opt.c \
	zthr.c

ARCH_C =
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
ARCH_C += 	vdev_raidz_math_sse2.c \
		vdev_raidz_math_ssse3.c \
		zfs_fletcher_intel.c \
		zfs_fletcher_sse.c 
CFLAGS +=  -DHAVE_SSE2 	-DHAVE_SSE3
.endif
.if ${MACHINE_ARCH} == "amd64"
ARCH_C +=	zfs_fletcher_avx512.c
CFLAGS+= -DHAVE_AVX2 -DHAVE_AVX -D__x86_64 -DHAVE_AVX512F \
	-DHAVE_AVX512BW
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
ARCH_C +=	zfs_fletcher_aarch64_neon.c
.endif

LUA_C = \
	lapi.c \
	lauxlib.c \
	lbaselib.c \
	lcode.c \
	lcompat.c \
	lcorolib.c \
	lctype.c \
	ldebug.c \
	ldo.c \
	lfunc.c \
	lgc.c \
	llex.c \
	lmem.c \
	lobject.c \
	lopcodes.c \
	lparser.c \
	lstate.c \
	lstring.c \
	lstrlib.c \
	ltable.c \
	ltablib.c \
	ltm.c \
	lvm.c \
	lzio.c

UNICODE_C = u8_textprep.c uconv.c

SRCS=		${USER_C} ${KERNEL_C} ${LUA_C} ${UNICODE_C} ${ARCH_C}

WARNS?=		2
CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${SRCTOP}/sys \
	-I${ZFSTOP}/include/os/freebsd/zfs \
	-I${SRCTOP}/cddl/compat/opensolaris/include \
	-I${ZFSTOP}/module/icp/include \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h \
	-I${SRCTOP}/sys/modules/zfs \
	-I${ZFSTOP}/include/os/freebsd/zfs \
	-DLIB_ZPOOL_BUILD -DZFS_DEBUG \

# XXX: pthread doesn't have mutex_owned() equivalent, so we need to look
#      into libthr private structures. That's sooo evil, but it's only for
#      ZFS debugging tools needs.
CFLAGS+=	-DWANTS_MUTEX_OWNED
CFLAGS+=	-I${SRCTOP}/lib/libpthread/thread
CFLAGS+=	-I${SRCTOP}/lib/libpthread/sys
CFLAGS+=	-I${SRCTOP}/lib/libthr/arch/${MACHINE_CPUARCH}/include
CFLAGS.gcc+=	-fms-extensions

LIBADD=		md pthread z spl icp nvpair avl umem

# atomic.S doesn't like profiling.
MK_PROFILE=	no

CSTD=	c99

# Since there are many asserts in this library, it makes no sense to compile
# it without debugging.

CFLAGS+=	-g -DDEBUG=1

# Pointer values are used as debugging "tags" to mark reference count
# ownerships and in some cases the tag reference is dropped after an
# object is freed.
CFLAGS.dbuf.c=			${NO_WUSE_AFTER_FREE}
CFLAGS.entropy_common.c=	-fno-tree-vectorize
CFLAGS.entropy_common.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.error_private.c=		-fno-tree-vectorize
CFLAGS.error_private.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.fse_compress.c=		-fno-tree-vectorize
CFLAGS.fse_compress.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.fse_decompress.c=	-fno-tree-vectorize
CFLAGS.fse_decompress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.hist.c=			-fno-tree-vectorize
CFLAGS.hist.c+=			${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.huf_compress.c=		-fno-tree-vectorize
CFLAGS.huf_compress.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.huf_decompress.c=	-fno-tree-vectorize
CFLAGS.huf_decompress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.pool.c=			-fno-tree-vectorize
CFLAGS.pool.c+=			${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.xxhash.c=		-fno-tree-vectorize
CFLAGS.xxhash.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress.c=		-fno-tree-vectorize
CFLAGS.zstd_compress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_literals.c=	-fno-tree-vectorize
CFLAGS.zstd_compress_literals.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_sequences.c=	-fno-tree-vectorize
CFLAGS.zstd_compress_sequences.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_superblock.c=	-fno-tree-vectorize
CFLAGS.zstd_compress_superblock.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_double_fast.c=	-fno-tree-vectorize
CFLAGS.zstd_double_fast.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_fast.c=		-fno-tree-vectorize
CFLAGS.zstd_fast.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_lazy.c=		-fno-tree-vectorize
CFLAGS.zstd_lazy.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_ldm.c=		-fno-tree-vectorize
CFLAGS.zstd_ldm.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_opt.c=		-fno-tree-vectorize
CFLAGS.zstd_opt.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_ddict.c=		-fno-tree-vectorize
CFLAGS.zstd_ddict.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_decompress.c=	-fno-tree-vectorize
CFLAGS.zstd_decompress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_decompress_block.c=	-fno-tree-vectorize
CFLAGS.zstd_decompress_block.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}

.include <bsd.lib.mk>
