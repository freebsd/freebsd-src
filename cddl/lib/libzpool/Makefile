# $FreeBSD$

ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

# ZFS_COMMON_SRCS
.PATH: ${ZFSTOP}/module/zfs
.PATH: ${ZFSTOP}/module/zcommon
.PATH: ${ZFSTOP}/module/unicode
# LUA_SRCS
.PATH: ${ZFSTOP}/module/lua
# ZSTD_SRCS
.PATH: ${ZFSTOP}/module/zstd
.PATH: ${ZFSTOP}/module/zstd/lib

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
	blkptr.c \
	bplist.c \
	bpobj.c \
	bptree.c \
	btree.c \
	bqueue.c \
	cityhash.c \
	dbuf.c \
	dbuf_stats.c \
	ddt.c \
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
	hkdf.c \
	fm.c \
	gzip.c \
	lzjb.c \
	lz4.c \
	metaslab.c \
	mmp.c \
	multilist.c \
	objlist.c \
	pathname.c \
	range_tree.c \
	refcount.c \
	rrwlock.c \
	sa.c \
	sha256.c \
	skein_zfs.c \
	spa.c \
	spa_boot.c \
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
	vdev_cache.c \
	vdev_draid.c \
	vdev_draid_rand.c \
	vdev_file.c \
	vdev_indirect_births.c \
	vdev_indirect.c \
	vdev_indirect_mapping.c \
	vdev_initialize.c \
	vdev_label.c \
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
	zfs_debug.c \
	zfs_fm.c \
	zfs_fuid.c \
	zfs_sa.c \
	zfs_znode.c \
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
	zstd.c \
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

CFLAGS.zfs_zstd.c=	-Wno-cast-qual -Wno-pointer-arith
CFLAGS.zstd.c+=	-fno-tree-vectorize

.include <bsd.lib.mk>
