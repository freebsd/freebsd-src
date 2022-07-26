# $FreeBSD$

SRCDIR=${SRCTOP}/sys/contrib/openzfs/module
INCDIR=${SRCTOP}/sys/contrib/openzfs/include

KMOD=	zfs

.PATH:	${SRCDIR}/avl \
	${SRCDIR}/lua \
	${SRCDIR}/nvpair \
        ${SRCDIR}/icp/algs/blake3 \
        ${SRCDIR}/icp/asm-aarch64/blake3 \
        ${SRCDIR}/icp/asm-ppc64/blake3 \
        ${SRCDIR}/icp/asm-x86_64/blake3 \
	${SRCDIR}/icp/algs/edonr \
	${SRCDIR}/os/freebsd/spl \
	${SRCDIR}/os/freebsd/zfs \
	${SRCDIR}/unicode \
	${SRCDIR}/zcommon \
	${SRCDIR}/zfs	\
	${SRCDIR}/zstd	\
	${SRCDIR}/zstd/lib/common \
	${SRCDIR}/zstd/lib/compress \
	${SRCDIR}/zstd/lib/decompress


CFLAGS+= -I${INCDIR}
CFLAGS+= -I${INCDIR}/os/freebsd
CFLAGS+= -I${INCDIR}/os/freebsd/spl
CFLAGS+= -I${INCDIR}/os/freebsd/zfs
CFLAGS+= -I${SRCDIR}/zstd/include
CFLAGS+= -I${SRCDIR}/icp/include
CFLAGS+= -I${.CURDIR}

CFLAGS+= -D__KERNEL__ -DFREEBSD_NAMECACHE -DBUILDING_ZFS \
	-DHAVE_UIO_ZEROCOPY -DWITHOUT_NETDUMP -D__KERNEL -D_SYS_CONDVAR_H_ \
	-D_SYS_VMEM_H_ -DIN_FREEBSD_BASE

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+= -D__x86_64 -DHAVE_SSE2 -DHAVE_SSSE3 -DHAVE_SSE4_1 -DHAVE_SSE4_2 \
	-DHAVE_AVX -DHAVE_AVX2 -DHAVE_AVX512F -DHAVE_AVX512VL -DHAVE_AVX512BW
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "powerpcspe" || ${MACHINE_ARCH} == "arm"
CFLAGS+= -DBITS_PER_LONG=32
.else
CFLAGS+= -DBITS_PER_LONG=64
.endif

SRCS=	vnode_if.h device_if.h bus_if.h

# avl
SRCS+=	avl.c

# icp
SRCS+=	edonr.c

#icp/algs/blake3
SRCS+=	blake3.c \
	blake3_generic.c \
	blake3_impl.c \
	blake3_x86-64.c

#icp/asm-aarch64/blake3
SRCS+=	b3_aarch64_sse2.S \
	b3_aarch64_sse41.S

#icp/asm-ppc64/blake3
SRCS+=	b3_ppc64le_sse2.S \
	b3_ppc64le_sse41.S

#icp/asm-x86_64/blake3
SRCS+=	blake3_avx2.S \
	blake3_avx512.S \
	blake3_sse2.S \
	blake3_sse41.S

#lua
SRCS+=	lapi.c \
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

#nvpair
SRCS+=	nvpair.c \
	fnvpair.c \
	nvpair_alloc_spl.c \
	nvpair_alloc_fixed.c

#os/freebsd/spl
SRCS+=	acl_common.c \
	btree.c \
	callb.c \
	list.c \
	spl_acl.c \
	spl_cmn_err.c \
	spl_dtrace.c \
	spl_kmem.c \
	spl_kstat.c \
	spl_misc.c \
	spl_policy.c \
	spl_string.c \
	spl_sunddi.c \
	spl_sysevent.c \
	spl_taskq.c \
	spl_uio.c \
	spl_vfs.c \
	spl_vm.c \
	spl_zone.c \
	sha256c.c \
	sha512c.c \
	spl_procfs_list.c \
	spl_zlib.c


.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "powerpcspe" || ${MACHINE_ARCH} == "arm"
SRCS+= spl_atomic.c
.endif

#os/freebsd/zfs
SRCS+=	abd_os.c \
	crypto_os.c \
	dmu_os.c \
	hkdf.c \
	kmod_core.c \
	spa_os.c \
	sysctl_os.c \
	vdev_file.c \
	vdev_label_os.c \
	vdev_geom.c \
	zfs_acl.c \
	zfs_ctldir.c \
	zfs_dir.c \
	zfs_ioctl_compat.c \
	zfs_ioctl_os.c \
	zfs_log.c \
	zfs_racct.c \
	zfs_replay.c \
	zfs_vfsops.c \
	zfs_vnops_os.c \
	zfs_znode.c \
	zio_crypt.c \
	zvol_os.c

#unicode
SRCS+=	uconv.c \
	u8_textprep.c

#zcommon
SRCS+=	zfeature_common.c \
	zfs_comutil.c \
	zfs_deleg.c \
	zfs_fletcher.c \
	zfs_fletcher_avx512.c \
	zfs_fletcher_intel.c \
	zfs_fletcher_sse.c \
	zfs_fletcher_superscalar.c \
	zfs_fletcher_superscalar4.c \
	zfs_namecheck.c \
	zfs_prop.c \
	zpool_prop.c \
	zprop_common.c

#zfs
SRCS+=	abd.c \
	aggsum.c \
	arc.c \
	arc_os.c \
        blake3_zfs.c \
	blkptr.c \
	bplist.c \
	bpobj.c \
	cityhash.c \
	dbuf.c \
	dbuf_stats.c \
	bptree.c \
	bqueue.c \
	dataset_kstats.c \
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
	dsl_dataset.c \
	dsl_deadlist.c \
	dsl_deleg.c \
	dsl_bookmark.c \
	dsl_dir.c \
	dsl_crypt.c \
	dsl_destroy.c \
	dsl_pool.c \
	dsl_prop.c \
	dsl_scan.c \
	dsl_synctask.c \
	dsl_userhold.c \
	edonr_zfs.c \
	fm.c \
	gzip.c \
	lzjb.c \
	lz4.c \
	lz4_zfs.c \
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
	uberblock.c \
	unique.c \
	vdev.c \
	vdev_cache.c \
	vdev_draid.c \
	vdev_draid_rand.c \
	vdev_indirect.c \
	vdev_indirect_births.c \
	vdev_indirect_mapping.c \
	vdev_initialize.c \
	vdev_label.c \
	vdev_mirror.c \
	vdev_missing.c \
	vdev_queue.c \
	vdev_raidz.c \
	vdev_raidz_math.c \
	vdev_raidz_math_scalar.c \
	vdev_raidz_math_avx2.c \
	vdev_raidz_math_avx512bw.c \
	vdev_raidz_math_avx512f.c \
	vdev_raidz_math_sse2.c \
	vdev_raidz_math_ssse3.c \
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
        zfs_chksum.c \
	zfs_debug.c \
	zfs_file_os.c \
	zfs_fm.c \
	zfs_fuid.c \
	zfs_ioctl.c \
	zfs_onexit.c \
	zfs_quota.c \
	zfs_ratelimit.c \
	zfs_rlock.c \
	zfs_sa.c \
	zfs_vnops.c \
	zil.c \
	zio.c \
	zio_checksum.c \
	zio_compress.c \
	zio_inject.c \
	zle.c \
	zrlock.c \
	zthr.c \
	zvol.c

SRCS+=	zfs_zstd.c \
	entropy_common.c \
	error_private.c \
	fse_compress.c \
	fse_decompress.c \
	hist.c \
	huf_compress.c \
	huf_decompress.c \
	pool.c \
	xxhash.c \
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
	zstd_opt.c

.include <bsd.kmod.mk>

CFLAGS+= -include ${SRCTOP}/sys/cddl/compat/opensolaris/sys/debug_compat.h
CFLAGS+= -include ${INCDIR}/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/static_ccompile.h

CWARNFLAGS+=	${OPENZFS_CWARNFLAGS}

CFLAGS.gcc+= -Wno-pointer-to-int-cast

CFLAGS.lapi.c= -Wno-cast-qual
CFLAGS.lcompat.c= -Wno-cast-qual
CFLAGS.lobject.c= -Wno-cast-qual
CFLAGS.ltable.c= -Wno-cast-qual
CFLAGS.lvm.c= -Wno-cast-qual
CFLAGS.nvpair.c= -Wno-cast-qual -DHAVE_RPC_TYPES
CFLAGS.spl_string.c= -Wno-cast-qual
CFLAGS.spl_vm.c= -Wno-cast-qual
CFLAGS.spl_zlib.c= -Wno-cast-qual
CFLAGS.abd.c= -Wno-cast-qual
CFLAGS.zfs_log.c= -Wno-cast-qual
CFLAGS.zfs_vnops_os.c= -Wno-pointer-arith
CFLAGS.u8_textprep.c= -Wno-cast-qual
CFLAGS.zfs_fletcher.c= -Wno-cast-qual -Wno-pointer-arith
CFLAGS.zfs_fletcher_intel.c= -Wno-cast-qual -Wno-pointer-arith
CFLAGS.zfs_fletcher_sse.c= -Wno-cast-qual -Wno-pointer-arith
CFLAGS.zfs_fletcher_avx512.c= -Wno-cast-qual -Wno-pointer-arith
CFLAGS.zprop_common.c= -Wno-cast-qual
CFLAGS.ddt.c= -Wno-cast-qual
CFLAGS.dmu.c= -Wno-cast-qual
CFLAGS.dmu_traverse.c= -Wno-cast-qual
CFLAGS.dnode.c+= ${NO_WUNUSED_BUT_SET_VARIABLE}
CFLAGS.dsl_dir.c= -Wno-cast-qual
CFLAGS.dsl_deadlist.c= -Wno-cast-qual
CFLAGS.dsl_prop.c= -Wno-cast-qual
CFLAGS.edonr.c= -Wno-cast-qual
CFLAGS.fm.c= -Wno-cast-qual
CFLAGS.lz4.c= -Wno-cast-qual
CFLAGS.lz4_zfs.c= -Wno-cast-qual
CFLAGS.spa.c= -Wno-cast-qual
CFLAGS.spa_misc.c= -Wno-cast-qual
CFLAGS.sysctl_os.c= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS.vdev_draid.c= -Wno-cast-qual
CFLAGS.vdev_raidz.c= -Wno-cast-qual
CFLAGS.vdev_raidz_math.c= -Wno-cast-qual
CFLAGS.vdev_raidz_math_scalar.c= -Wno-cast-qual
CFLAGS.vdev_raidz_math_avx2.c= -Wno-cast-qual -Wno-duplicate-decl-specifier
CFLAGS.vdev_raidz_math_avx512f.c= -Wno-cast-qual -Wno-duplicate-decl-specifier
CFLAGS.vdev_raidz_math_sse2.c= -Wno-cast-qual -Wno-duplicate-decl-specifier
CFLAGS.zap_leaf.c= -Wno-cast-qual
CFLAGS.zap_micro.c= -Wno-cast-qual
CFLAGS.zcp.c= -Wno-cast-qual
CFLAGS.zfs_fm.c= -Wno-cast-qual ${NO_WUNUSED_BUT_SET_VARIABLE}
CFLAGS.zfs_ioctl.c= -Wno-cast-qual
CFLAGS.zil.c= -Wno-cast-qual
CFLAGS.zio.c= -Wno-cast-qual
CFLAGS.zrlock.c= -Wno-cast-qual
CFLAGS.zfs_zstd.c= -Wno-cast-qual -Wno-pointer-arith

CFLAGS.zstd.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.entropy_common.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.error_private.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.fse_compress.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.fse_decompress.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.hist.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.huf_compress.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.huf_decompress.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.pool.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.xxhash.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_common.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_compress.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_compress_literals.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_compress_sequences.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_compress_superblock.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_ddict.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_decompress.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_decompress_block.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_double_fast.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_fast.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_lazy.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_ldm.c= -U__BMI__ -fno-tree-vectorize
CFLAGS.zstd_opt.c= -U__BMI__ -fno-tree-vectorize

.if ${MACHINE_CPUARCH} == "aarch64"
__ZFS_ZSTD_AARCH64_FLAGS= -include ${SRCDIR}/zstd/include/aarch64_compat.h
CFLAGS.zstd.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.entropy_common.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.error_private.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.fse_compress.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.fse_decompress.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.hist.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.huf_compress.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.huf_decompress.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.pool.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.xxhash.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_common.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_compress.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_compress_literals.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_compress_sequences.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_compress_superblock.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_ddict.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_decompress.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_decompress_block.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_double_fast.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_fast.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_lazy.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_ldm.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}
CFLAGS.zstd_opt.c+= ${__ZFS_ZSTD_AARCH64_FLAGS}

b3_aarch64_sse2.o: b3_aarch64_sse2.S
	${CC} -c ${CFLAGS:N-mgeneral-regs-only} ${WERROR} ${.IMPSRC} \
	     -o ${.TARGET}
	${CTFCONVERT_CMD}

b3_aarch64_sse41.o: b3_aarch64_sse41.S
	${CC} -c ${CFLAGS:N-mgeneral-regs-only} ${WERROR} ${.IMPSRC} \
	     -o ${.TARGET}
	${CTFCONVERT_CMD}

.endif
CFLAGS.zstd.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.entropy_common.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.error_private.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.fse_compress.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.fse_compress.c+= ${NO_WUNUSED_BUT_SET_VARIABLE}
CFLAGS.fse_decompress.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.hist.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.huf_compress.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.huf_decompress.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.pool.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.xxhash.c+= -include ${SRCTOP}/sys/sys/_null.h
CFLAGS.xxhash.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_common.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_literals.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_sequences.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_superblock.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_superblock.c+= ${NO_WUNUSED_BUT_SET_VARIABLE}
CFLAGS.zstd_ddict.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_decompress.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_decompress_block.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_double_fast.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_fast.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_lazy.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_ldm.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_opt.c+= ${NO_WBITWISE_INSTEAD_OF_LOGICAL}
