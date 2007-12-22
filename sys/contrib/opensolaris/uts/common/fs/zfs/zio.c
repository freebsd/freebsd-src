/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/zfs_context.h>
#include <sys/fm/fs/zfs.h>
#include <sys/spa.h>
#include <sys/txg.h>
#include <sys/spa_impl.h>
#include <sys/vdev_impl.h>
#include <sys/zio_impl.h>
#include <sys/zio_compress.h>
#include <sys/zio_checksum.h>

/*
 * ==========================================================================
 * I/O priority table
 * ==========================================================================
 */
uint8_t zio_priority_table[ZIO_PRIORITY_TABLE_SIZE] = {
	0,	/* ZIO_PRIORITY_NOW		*/
	0,	/* ZIO_PRIORITY_SYNC_READ	*/
	0,	/* ZIO_PRIORITY_SYNC_WRITE	*/
	6,	/* ZIO_PRIORITY_ASYNC_READ	*/
	4,	/* ZIO_PRIORITY_ASYNC_WRITE	*/
	4,	/* ZIO_PRIORITY_FREE		*/
	0,	/* ZIO_PRIORITY_CACHE_FILL	*/
	0,	/* ZIO_PRIORITY_LOG_WRITE	*/
	10,	/* ZIO_PRIORITY_RESILVER	*/
	20,	/* ZIO_PRIORITY_SCRUB		*/
};

/*
 * ==========================================================================
 * I/O type descriptions
 * ==========================================================================
 */
char *zio_type_name[ZIO_TYPES] = {
	"null", "read", "write", "free", "claim", "ioctl" };

/* At or above this size, force gang blocking - for testing */
uint64_t zio_gang_bang = SPA_MAXBLOCKSIZE + 1;

/* Force an allocation failure when non-zero */
uint16_t zio_zil_fail_shift = 0;

typedef struct zio_sync_pass {
	int	zp_defer_free;		/* defer frees after this pass */
	int	zp_dontcompress;	/* don't compress after this pass */
	int	zp_rewrite;		/* rewrite new bps after this pass */
} zio_sync_pass_t;

zio_sync_pass_t zio_sync_pass = {
	1,	/* zp_defer_free */
	4,	/* zp_dontcompress */
	1,	/* zp_rewrite */
};

/*
 * ==========================================================================
 * I/O kmem caches
 * ==========================================================================
 */
kmem_cache_t *zio_cache;
#ifdef ZIO_USE_UMA
kmem_cache_t *zio_buf_cache[SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT];
kmem_cache_t *zio_data_buf_cache[SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT];
#endif

#ifdef _KERNEL
extern vmem_t *zio_alloc_arena;
#endif

void
zio_init(void)
{
#ifdef ZIO_USE_UMA
	size_t c;
#endif
#if 0
	vmem_t *data_alloc_arena = NULL;

#ifdef _KERNEL
	data_alloc_arena = zio_alloc_arena;
#endif
#endif

	zio_cache = kmem_cache_create("zio_cache", sizeof (zio_t), 0,
	    NULL, NULL, NULL, NULL, NULL, 0);

#ifdef ZIO_USE_UMA
	/*
	 * For small buffers, we want a cache for each multiple of
	 * SPA_MINBLOCKSIZE.  For medium-size buffers, we want a cache
	 * for each quarter-power of 2.  For large buffers, we want
	 * a cache for each multiple of PAGESIZE.
	 */
	for (c = 0; c < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT; c++) {
		size_t size = (c + 1) << SPA_MINBLOCKSHIFT;
		size_t p2 = size;
		size_t align = 0;

		while (p2 & (p2 - 1))
			p2 &= p2 - 1;

		if (size <= 4 * SPA_MINBLOCKSIZE) {
			align = SPA_MINBLOCKSIZE;
		} else if (P2PHASE(size, PAGESIZE) == 0) {
			align = PAGESIZE;
		} else if (P2PHASE(size, p2 >> 2) == 0) {
			align = p2 >> 2;
		}

		if (align != 0) {
			char name[36];
			(void) sprintf(name, "zio_buf_%lu", (ulong_t)size);
			zio_buf_cache[c] = kmem_cache_create(name, size,
			    align, NULL, NULL, NULL, NULL, NULL, KMC_NODEBUG);

			(void) sprintf(name, "zio_data_buf_%lu", (ulong_t)size);
			zio_data_buf_cache[c] = kmem_cache_create(name, size,
			    align, NULL, NULL, NULL, NULL, data_alloc_arena,
			    KMC_NODEBUG);

			dprintf("creating cache for size %5lx align %5lx\n",
			    size, align);
		}
	}

	while (--c != 0) {
		ASSERT(zio_buf_cache[c] != NULL);
		if (zio_buf_cache[c - 1] == NULL)
			zio_buf_cache[c - 1] = zio_buf_cache[c];

		ASSERT(zio_data_buf_cache[c] != NULL);
		if (zio_data_buf_cache[c - 1] == NULL)
			zio_data_buf_cache[c - 1] = zio_data_buf_cache[c];
	}
#endif

	zio_inject_init();
}

void
zio_fini(void)
{
#ifdef ZIO_USE_UMA
	size_t c;
	kmem_cache_t *last_cache = NULL;
	kmem_cache_t *last_data_cache = NULL;

	for (c = 0; c < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT; c++) {
		if (zio_buf_cache[c] != last_cache) {
			last_cache = zio_buf_cache[c];
			kmem_cache_destroy(zio_buf_cache[c]);
		}
		zio_buf_cache[c] = NULL;

		if (zio_data_buf_cache[c] != last_data_cache) {
			last_data_cache = zio_data_buf_cache[c];
			kmem_cache_destroy(zio_data_buf_cache[c]);
		}
		zio_data_buf_cache[c] = NULL;
	}
#endif

	kmem_cache_destroy(zio_cache);

	zio_inject_fini();
}

/*
 * ==========================================================================
 * Allocate and free I/O buffers
 * ==========================================================================
 */

/*
 * Use zio_buf_alloc to allocate ZFS metadata.  This data will appear in a
 * crashdump if the kernel panics, so use it judiciously.  Obviously, it's
 * useful to inspect ZFS metadata, but if possible, we should avoid keeping
 * excess / transient data in-core during a crashdump.
 */
void *
zio_buf_alloc(size_t size)
{
#ifdef ZIO_USE_UMA
	size_t c = (size - 1) >> SPA_MINBLOCKSHIFT;

	ASSERT(c < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT);

	return (kmem_cache_alloc(zio_buf_cache[c], KM_SLEEP));
#else
	return (kmem_alloc(size, KM_SLEEP));
#endif
}

/*
 * Use zio_data_buf_alloc to allocate data.  The data will not appear in a
 * crashdump if the kernel panics.  This exists so that we will limit the amount
 * of ZFS data that shows up in a kernel crashdump.  (Thus reducing the amount
 * of kernel heap dumped to disk when the kernel panics)
 */
void *
zio_data_buf_alloc(size_t size)
{
#ifdef ZIO_USE_UMA
	size_t c = (size - 1) >> SPA_MINBLOCKSHIFT;

	ASSERT(c < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT);

	return (kmem_cache_alloc(zio_data_buf_cache[c], KM_SLEEP));
#else
	return (kmem_alloc(size, KM_SLEEP));
#endif
}

void
zio_buf_free(void *buf, size_t size)
{
#ifdef ZIO_USE_UMA
	size_t c = (size - 1) >> SPA_MINBLOCKSHIFT;

	ASSERT(c < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT);

	kmem_cache_free(zio_buf_cache[c], buf);
#else
	kmem_free(buf, size);
#endif
}

void
zio_data_buf_free(void *buf, size_t size)
{
#ifdef ZIO_USE_UMA
	size_t c = (size - 1) >> SPA_MINBLOCKSHIFT;

	ASSERT(c < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT);

	kmem_cache_free(zio_data_buf_cache[c], buf);
#else
	kmem_free(buf, size);
#endif
}

/*
 * ==========================================================================
 * Push and pop I/O transform buffers
 * ==========================================================================
 */
static void
zio_push_transform(zio_t *zio, void *data, uint64_t size, uint64_t bufsize)
{
	zio_transform_t *zt = kmem_alloc(sizeof (zio_transform_t), KM_SLEEP);

	zt->zt_data = data;
	zt->zt_size = size;
	zt->zt_bufsize = bufsize;

	zt->zt_next = zio->io_transform_stack;
	zio->io_transform_stack = zt;

	zio->io_data = data;
	zio->io_size = size;
}

static void
zio_pop_transform(zio_t *zio, void **data, uint64_t *size, uint64_t *bufsize)
{
	zio_transform_t *zt = zio->io_transform_stack;

	*data = zt->zt_data;
	*size = zt->zt_size;
	*bufsize = zt->zt_bufsize;

	zio->io_transform_stack = zt->zt_next;
	kmem_free(zt, sizeof (zio_transform_t));

	if ((zt = zio->io_transform_stack) != NULL) {
		zio->io_data = zt->zt_data;
		zio->io_size = zt->zt_size;
	}
}

static void
zio_clear_transform_stack(zio_t *zio)
{
	void *data;
	uint64_t size, bufsize;

	ASSERT(zio->io_transform_stack != NULL);

	zio_pop_transform(zio, &data, &size, &bufsize);
	while (zio->io_transform_stack != NULL) {
		zio_buf_free(data, bufsize);
		zio_pop_transform(zio, &data, &size, &bufsize);
	}
}

/*
 * ==========================================================================
 * Create the various types of I/O (read, write, free)
 * ==========================================================================
 */
static zio_t *
zio_create(zio_t *pio, spa_t *spa, uint64_t txg, blkptr_t *bp,
    void *data, uint64_t size, zio_done_func_t *done, void *private,
    zio_type_t type, int priority, int flags, uint8_t stage, uint32_t pipeline)
{
	zio_t *zio;

	ASSERT3U(size, <=, SPA_MAXBLOCKSIZE);
	ASSERT(P2PHASE(size, SPA_MINBLOCKSIZE) == 0);

	zio = kmem_cache_alloc(zio_cache, KM_SLEEP);
	bzero(zio, sizeof (zio_t));
	zio->io_parent = pio;
	zio->io_spa = spa;
	zio->io_txg = txg;
	if (bp != NULL) {
		zio->io_bp = bp;
		zio->io_bp_copy = *bp;
		zio->io_bp_orig = *bp;
	}
	zio->io_done = done;
	zio->io_private = private;
	zio->io_type = type;
	zio->io_priority = priority;
	zio->io_stage = stage;
	zio->io_pipeline = pipeline;
	zio->io_async_stages = ZIO_ASYNC_PIPELINE_STAGES;
	zio->io_timestamp = lbolt64;
	zio->io_flags = flags;
	mutex_init(&zio->io_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&zio->io_cv, NULL, CV_DEFAULT, NULL);
	zio_push_transform(zio, data, size, size);

	/*
	 * Note on config lock:
	 *
	 * If CONFIG_HELD is set, then the caller already has the config
	 * lock, so we don't need it for this io.
	 *
	 * We set CONFIG_GRABBED to indicate that we have grabbed the
	 * config lock on behalf of this io, so it should be released
	 * in zio_done.
	 *
	 * Unless CONFIG_HELD is set, we will grab the config lock for
	 * any top-level (parent-less) io, *except* NULL top-level ios.
	 * The NULL top-level ios rarely have any children, so we delay
	 * grabbing the lock until the first child is added (but it is
	 * still grabbed on behalf of the top-level i/o, so additional
	 * children don't need to also grab it).  This greatly reduces
	 * contention on the config lock.
	 */
	if (pio == NULL) {
		if (type != ZIO_TYPE_NULL &&
		    !(flags & ZIO_FLAG_CONFIG_HELD)) {
			spa_config_enter(zio->io_spa, RW_READER, zio);
			zio->io_flags |= ZIO_FLAG_CONFIG_GRABBED;
		}
		zio->io_root = zio;
	} else {
		zio->io_root = pio->io_root;
		if (!(flags & ZIO_FLAG_NOBOOKMARK))
			zio->io_logical = pio->io_logical;
		mutex_enter(&pio->io_lock);
		if (pio->io_parent == NULL &&
		    pio->io_type == ZIO_TYPE_NULL &&
		    !(pio->io_flags & ZIO_FLAG_CONFIG_GRABBED) &&
		    !(pio->io_flags & ZIO_FLAG_CONFIG_HELD)) {
			pio->io_flags |= ZIO_FLAG_CONFIG_GRABBED;
			spa_config_enter(zio->io_spa, RW_READER, pio);
		}
		if (stage < ZIO_STAGE_READY)
			pio->io_children_notready++;
		pio->io_children_notdone++;
		zio->io_sibling_next = pio->io_child;
		zio->io_sibling_prev = NULL;
		if (pio->io_child != NULL)
			pio->io_child->io_sibling_prev = zio;
		pio->io_child = zio;
		zio->io_ndvas = pio->io_ndvas;
		mutex_exit(&pio->io_lock);
	}

	return (zio);
}

zio_t *
zio_null(zio_t *pio, spa_t *spa, zio_done_func_t *done, void *private,
	int flags)
{
	zio_t *zio;

	zio = zio_create(pio, spa, 0, NULL, NULL, 0, done, private,
	    ZIO_TYPE_NULL, ZIO_PRIORITY_NOW, flags, ZIO_STAGE_OPEN,
	    ZIO_WAIT_FOR_CHILDREN_PIPELINE);

	return (zio);
}

zio_t *
zio_root(spa_t *spa, zio_done_func_t *done, void *private, int flags)
{
	return (zio_null(NULL, spa, done, private, flags));
}

zio_t *
zio_read(zio_t *pio, spa_t *spa, blkptr_t *bp, void *data,
    uint64_t size, zio_done_func_t *done, void *private,
    int priority, int flags, zbookmark_t *zb)
{
	zio_t *zio;

	ASSERT3U(size, ==, BP_GET_LSIZE(bp));

	zio = zio_create(pio, spa, bp->blk_birth, bp, data, size, done, private,
	    ZIO_TYPE_READ, priority, flags | ZIO_FLAG_USER,
	    ZIO_STAGE_OPEN, ZIO_READ_PIPELINE);
	zio->io_bookmark = *zb;

	zio->io_logical = zio;

	/*
	 * Work off our copy of the bp so the caller can free it.
	 */
	zio->io_bp = &zio->io_bp_copy;

	if (BP_GET_COMPRESS(bp) != ZIO_COMPRESS_OFF) {
		uint64_t csize = BP_GET_PSIZE(bp);
		void *cbuf = zio_buf_alloc(csize);

		zio_push_transform(zio, cbuf, csize, csize);
		zio->io_pipeline |= 1U << ZIO_STAGE_READ_DECOMPRESS;
	}

	if (BP_IS_GANG(bp)) {
		uint64_t gsize = SPA_GANGBLOCKSIZE;
		void *gbuf = zio_buf_alloc(gsize);

		zio_push_transform(zio, gbuf, gsize, gsize);
		zio->io_pipeline |= 1U << ZIO_STAGE_READ_GANG_MEMBERS;
	}

	return (zio);
}

zio_t *
zio_write(zio_t *pio, spa_t *spa, int checksum, int compress, int ncopies,
    uint64_t txg, blkptr_t *bp, void *data, uint64_t size,
    zio_done_func_t *ready, zio_done_func_t *done, void *private, int priority,
    int flags, zbookmark_t *zb)
{
	zio_t *zio;

	ASSERT(checksum >= ZIO_CHECKSUM_OFF &&
	    checksum < ZIO_CHECKSUM_FUNCTIONS);

	ASSERT(compress >= ZIO_COMPRESS_OFF &&
	    compress < ZIO_COMPRESS_FUNCTIONS);

	zio = zio_create(pio, spa, txg, bp, data, size, done, private,
	    ZIO_TYPE_WRITE, priority, flags | ZIO_FLAG_USER,
	    ZIO_STAGE_OPEN, ZIO_WRITE_PIPELINE);

	zio->io_ready = ready;

	zio->io_bookmark = *zb;

	zio->io_logical = zio;

	zio->io_checksum = checksum;
	zio->io_compress = compress;
	zio->io_ndvas = ncopies;

	if (compress != ZIO_COMPRESS_OFF)
		zio->io_async_stages |= 1U << ZIO_STAGE_WRITE_COMPRESS;

	if (bp->blk_birth != txg) {
		/* XXX the bp usually (always?) gets re-zeroed later */
		BP_ZERO(bp);
		BP_SET_LSIZE(bp, size);
		BP_SET_PSIZE(bp, size);
	} else {
		/* Make sure someone doesn't change their mind on overwrites */
		ASSERT(MIN(zio->io_ndvas + BP_IS_GANG(bp),
		    spa_max_replication(spa)) == BP_GET_NDVAS(bp));
	}

	return (zio);
}

zio_t *
zio_rewrite(zio_t *pio, spa_t *spa, int checksum,
    uint64_t txg, blkptr_t *bp, void *data, uint64_t size,
    zio_done_func_t *done, void *private, int priority, int flags,
    zbookmark_t *zb)
{
	zio_t *zio;

	zio = zio_create(pio, spa, txg, bp, data, size, done, private,
	    ZIO_TYPE_WRITE, priority, flags | ZIO_FLAG_USER,
	    ZIO_STAGE_OPEN, ZIO_REWRITE_PIPELINE);

	zio->io_bookmark = *zb;
	zio->io_checksum = checksum;
	zio->io_compress = ZIO_COMPRESS_OFF;

	if (pio != NULL)
		ASSERT3U(zio->io_ndvas, <=, BP_GET_NDVAS(bp));

	return (zio);
}

static zio_t *
zio_write_allocate(zio_t *pio, spa_t *spa, int checksum,
    uint64_t txg, blkptr_t *bp, void *data, uint64_t size,
    zio_done_func_t *done, void *private, int priority, int flags)
{
	zio_t *zio;

	BP_ZERO(bp);
	BP_SET_LSIZE(bp, size);
	BP_SET_PSIZE(bp, size);
	BP_SET_COMPRESS(bp, ZIO_COMPRESS_OFF);

	zio = zio_create(pio, spa, txg, bp, data, size, done, private,
	    ZIO_TYPE_WRITE, priority, flags,
	    ZIO_STAGE_OPEN, ZIO_WRITE_ALLOCATE_PIPELINE);

	zio->io_checksum = checksum;
	zio->io_compress = ZIO_COMPRESS_OFF;

	return (zio);
}

zio_t *
zio_free(zio_t *pio, spa_t *spa, uint64_t txg, blkptr_t *bp,
    zio_done_func_t *done, void *private)
{
	zio_t *zio;

	ASSERT(!BP_IS_HOLE(bp));

	if (txg == spa->spa_syncing_txg &&
	    spa->spa_sync_pass > zio_sync_pass.zp_defer_free) {
		bplist_enqueue_deferred(&spa->spa_sync_bplist, bp);
		return (zio_null(pio, spa, NULL, NULL, 0));
	}

	zio = zio_create(pio, spa, txg, bp, NULL, 0, done, private,
	    ZIO_TYPE_FREE, ZIO_PRIORITY_FREE, ZIO_FLAG_USER,
	    ZIO_STAGE_OPEN, ZIO_FREE_PIPELINE);

	zio->io_bp = &zio->io_bp_copy;

	return (zio);
}

zio_t *
zio_claim(zio_t *pio, spa_t *spa, uint64_t txg, blkptr_t *bp,
    zio_done_func_t *done, void *private)
{
	zio_t *zio;

	/*
	 * A claim is an allocation of a specific block.  Claims are needed
	 * to support immediate writes in the intent log.  The issue is that
	 * immediate writes contain committed data, but in a txg that was
	 * *not* committed.  Upon opening the pool after an unclean shutdown,
	 * the intent log claims all blocks that contain immediate write data
	 * so that the SPA knows they're in use.
	 *
	 * All claims *must* be resolved in the first txg -- before the SPA
	 * starts allocating blocks -- so that nothing is allocated twice.
	 */
	ASSERT3U(spa->spa_uberblock.ub_rootbp.blk_birth, <, spa_first_txg(spa));
	ASSERT3U(spa_first_txg(spa), <=, txg);

	zio = zio_create(pio, spa, txg, bp, NULL, 0, done, private,
	    ZIO_TYPE_CLAIM, ZIO_PRIORITY_NOW, 0,
	    ZIO_STAGE_OPEN, ZIO_CLAIM_PIPELINE);

	zio->io_bp = &zio->io_bp_copy;

	return (zio);
}

zio_t *
zio_ioctl(zio_t *pio, spa_t *spa, vdev_t *vd, int cmd,
    zio_done_func_t *done, void *private, int priority, int flags)
{
	zio_t *zio;
	int c;

	if (vd->vdev_children == 0) {
		zio = zio_create(pio, spa, 0, NULL, NULL, 0, done, private,
		    ZIO_TYPE_IOCTL, priority, flags,
		    ZIO_STAGE_OPEN, ZIO_IOCTL_PIPELINE);

		zio->io_vd = vd;
		zio->io_cmd = cmd;
	} else {
		zio = zio_null(pio, spa, NULL, NULL, flags);

		for (c = 0; c < vd->vdev_children; c++)
			zio_nowait(zio_ioctl(zio, spa, vd->vdev_child[c], cmd,
			    done, private, priority, flags));
	}

	return (zio);
}

static void
zio_phys_bp_init(vdev_t *vd, blkptr_t *bp, uint64_t offset, uint64_t size,
    int checksum)
{
	ASSERT(vd->vdev_children == 0);

	ASSERT(size <= SPA_MAXBLOCKSIZE);
	ASSERT(P2PHASE(size, SPA_MINBLOCKSIZE) == 0);
	ASSERT(P2PHASE(offset, SPA_MINBLOCKSIZE) == 0);

	ASSERT(offset + size <= VDEV_LABEL_START_SIZE ||
	    offset >= vd->vdev_psize - VDEV_LABEL_END_SIZE);
	ASSERT3U(offset + size, <=, vd->vdev_psize);

	BP_ZERO(bp);

	BP_SET_LSIZE(bp, size);
	BP_SET_PSIZE(bp, size);

	BP_SET_CHECKSUM(bp, checksum);
	BP_SET_COMPRESS(bp, ZIO_COMPRESS_OFF);
	BP_SET_BYTEORDER(bp, ZFS_HOST_BYTEORDER);

	if (checksum != ZIO_CHECKSUM_OFF)
		ZIO_SET_CHECKSUM(&bp->blk_cksum, offset, 0, 0, 0);
}

zio_t *
zio_read_phys(zio_t *pio, vdev_t *vd, uint64_t offset, uint64_t size,
    void *data, int checksum, zio_done_func_t *done, void *private,
    int priority, int flags)
{
	zio_t *zio;
	blkptr_t blk;

	zio_phys_bp_init(vd, &blk, offset, size, checksum);

	zio = zio_create(pio, vd->vdev_spa, 0, &blk, data, size, done, private,
	    ZIO_TYPE_READ, priority, flags | ZIO_FLAG_PHYSICAL,
	    ZIO_STAGE_OPEN, ZIO_READ_PHYS_PIPELINE);

	zio->io_vd = vd;
	zio->io_offset = offset;

	/*
	 * Work off our copy of the bp so the caller can free it.
	 */
	zio->io_bp = &zio->io_bp_copy;

	return (zio);
}

zio_t *
zio_write_phys(zio_t *pio, vdev_t *vd, uint64_t offset, uint64_t size,
    void *data, int checksum, zio_done_func_t *done, void *private,
    int priority, int flags)
{
	zio_block_tail_t *zbt;
	void *wbuf;
	zio_t *zio;
	blkptr_t blk;

	zio_phys_bp_init(vd, &blk, offset, size, checksum);

	zio = zio_create(pio, vd->vdev_spa, 0, &blk, data, size, done, private,
	    ZIO_TYPE_WRITE, priority, flags | ZIO_FLAG_PHYSICAL,
	    ZIO_STAGE_OPEN, ZIO_WRITE_PHYS_PIPELINE);

	zio->io_vd = vd;
	zio->io_offset = offset;

	zio->io_bp = &zio->io_bp_copy;
	zio->io_checksum = checksum;

	if (zio_checksum_table[checksum].ci_zbt) {
		/*
		 * zbt checksums are necessarily destructive -- they modify
		 * one word of the write buffer to hold the verifier/checksum.
		 * Therefore, we must make a local copy in case the data is
		 * being written to multiple places.
		 */
		wbuf = zio_buf_alloc(size);
		bcopy(data, wbuf, size);
		zio_push_transform(zio, wbuf, size, size);

		zbt = (zio_block_tail_t *)((char *)wbuf + size) - 1;
		zbt->zbt_cksum = blk.blk_cksum;
	}

	return (zio);
}

/*
 * Create a child I/O to do some work for us.  It has no associated bp.
 */
zio_t *
zio_vdev_child_io(zio_t *zio, blkptr_t *bp, vdev_t *vd, uint64_t offset,
	void *data, uint64_t size, int type, int priority, int flags,
	zio_done_func_t *done, void *private)
{
	uint32_t pipeline = ZIO_VDEV_CHILD_PIPELINE;
	zio_t *cio;

	if (type == ZIO_TYPE_READ && bp != NULL) {
		/*
		 * If we have the bp, then the child should perform the
		 * checksum and the parent need not.  This pushes error
		 * detection as close to the leaves as possible and
		 * eliminates redundant checksums in the interior nodes.
		 */
		pipeline |= 1U << ZIO_STAGE_CHECKSUM_VERIFY;
		zio->io_pipeline &= ~(1U << ZIO_STAGE_CHECKSUM_VERIFY);
	}

	cio = zio_create(zio, zio->io_spa, zio->io_txg, bp, data, size,
	    done, private, type, priority,
	    (zio->io_flags & ZIO_FLAG_VDEV_INHERIT) | ZIO_FLAG_CANFAIL | flags,
	    ZIO_STAGE_VDEV_IO_START - 1, pipeline);

	cio->io_vd = vd;
	cio->io_offset = offset;

	return (cio);
}

/*
 * ==========================================================================
 * Initiate I/O, either sync or async
 * ==========================================================================
 */
int
zio_wait(zio_t *zio)
{
	int error;

	ASSERT(zio->io_stage == ZIO_STAGE_OPEN);

	zio->io_waiter = curthread;

	zio_next_stage_async(zio);

	mutex_enter(&zio->io_lock);
	while (zio->io_stalled != ZIO_STAGE_DONE)
		cv_wait(&zio->io_cv, &zio->io_lock);
	mutex_exit(&zio->io_lock);

	error = zio->io_error;
	cv_destroy(&zio->io_cv);
	mutex_destroy(&zio->io_lock);
	kmem_cache_free(zio_cache, zio);

	return (error);
}

void
zio_nowait(zio_t *zio)
{
	zio_next_stage_async(zio);
}

/*
 * ==========================================================================
 * I/O pipeline interlocks: parent/child dependency scoreboarding
 * ==========================================================================
 */
static void
zio_wait_for_children(zio_t *zio, uint32_t stage, uint64_t *countp)
{
	mutex_enter(&zio->io_lock);
	if (*countp == 0) {
		ASSERT(zio->io_stalled == 0);
		mutex_exit(&zio->io_lock);
		zio_next_stage(zio);
	} else {
		zio->io_stalled = stage;
		mutex_exit(&zio->io_lock);
	}
}

static void
zio_notify_parent(zio_t *zio, uint32_t stage, uint64_t *countp)
{
	zio_t *pio = zio->io_parent;

	mutex_enter(&pio->io_lock);
	if (pio->io_error == 0 && !(zio->io_flags & ZIO_FLAG_DONT_PROPAGATE))
		pio->io_error = zio->io_error;
	if (--*countp == 0 && pio->io_stalled == stage) {
		pio->io_stalled = 0;
		mutex_exit(&pio->io_lock);
		zio_next_stage_async(pio);
	} else {
		mutex_exit(&pio->io_lock);
	}
}

static void
zio_wait_children_ready(zio_t *zio)
{
	zio_wait_for_children(zio, ZIO_STAGE_WAIT_CHILDREN_READY,
	    &zio->io_children_notready);
}

void
zio_wait_children_done(zio_t *zio)
{
	zio_wait_for_children(zio, ZIO_STAGE_WAIT_CHILDREN_DONE,
	    &zio->io_children_notdone);
}

static void
zio_ready(zio_t *zio)
{
	zio_t *pio = zio->io_parent;

	if (zio->io_ready)
		zio->io_ready(zio);

	if (pio != NULL)
		zio_notify_parent(zio, ZIO_STAGE_WAIT_CHILDREN_READY,
		    &pio->io_children_notready);

	if (zio->io_bp)
		zio->io_bp_copy = *zio->io_bp;

	zio_next_stage(zio);
}

static void
zio_done(zio_t *zio)
{
	zio_t *pio = zio->io_parent;
	spa_t *spa = zio->io_spa;
	blkptr_t *bp = zio->io_bp;
	vdev_t *vd = zio->io_vd;

	ASSERT(zio->io_children_notready == 0);
	ASSERT(zio->io_children_notdone == 0);

	if (bp != NULL) {
		ASSERT(bp->blk_pad[0] == 0);
		ASSERT(bp->blk_pad[1] == 0);
		ASSERT(bp->blk_pad[2] == 0);
		ASSERT(bcmp(bp, &zio->io_bp_copy, sizeof (blkptr_t)) == 0);
		if (zio->io_type == ZIO_TYPE_WRITE && !BP_IS_HOLE(bp) &&
		    !(zio->io_flags & ZIO_FLAG_IO_REPAIR)) {
			ASSERT(!BP_SHOULD_BYTESWAP(bp));
			if (zio->io_ndvas != 0)
				ASSERT3U(zio->io_ndvas, <=, BP_GET_NDVAS(bp));
			ASSERT(BP_COUNT_GANG(bp) == 0 ||
			    (BP_COUNT_GANG(bp) == BP_GET_NDVAS(bp)));
		}
	}

	if (vd != NULL)
		vdev_stat_update(zio);

	if (zio->io_error) {
		/*
		 * If this I/O is attached to a particular vdev,
		 * generate an error message describing the I/O failure
		 * at the block level.  We ignore these errors if the
		 * device is currently unavailable.
		 */
		if (zio->io_error != ECKSUM && vd != NULL && !vdev_is_dead(vd))
			zfs_ereport_post(FM_EREPORT_ZFS_IO,
			    zio->io_spa, vd, zio, 0, 0);

		if ((zio->io_error == EIO ||
		    !(zio->io_flags & ZIO_FLAG_SPECULATIVE)) &&
		    zio->io_logical == zio) {
			/*
			 * For root I/O requests, tell the SPA to log the error
			 * appropriately.  Also, generate a logical data
			 * ereport.
			 */
			spa_log_error(zio->io_spa, zio);

			zfs_ereport_post(FM_EREPORT_ZFS_DATA,
			    zio->io_spa, NULL, zio, 0, 0);
		}

		/*
		 * For I/O requests that cannot fail, panic appropriately.
		 */
		if (!(zio->io_flags & ZIO_FLAG_CANFAIL)) {
			char *blkbuf;

			blkbuf = kmem_alloc(BP_SPRINTF_LEN, KM_NOSLEEP);
			if (blkbuf) {
				sprintf_blkptr(blkbuf, BP_SPRINTF_LEN,
				    bp ? bp : &zio->io_bp_copy);
			}
			panic("ZFS: %s (%s on %s off %llx: zio %p %s): error "
			    "%d", zio->io_error == ECKSUM ?
			    "bad checksum" : "I/O failure",
			    zio_type_name[zio->io_type],
			    vdev_description(vd),
			    (u_longlong_t)zio->io_offset,
			    zio, blkbuf ? blkbuf : "", zio->io_error);
		}
	}
	zio_clear_transform_stack(zio);

	if (zio->io_done)
		zio->io_done(zio);

	ASSERT(zio->io_delegate_list == NULL);
	ASSERT(zio->io_delegate_next == NULL);

	if (pio != NULL) {
		zio_t *next, *prev;

		mutex_enter(&pio->io_lock);
		next = zio->io_sibling_next;
		prev = zio->io_sibling_prev;
		if (next != NULL)
			next->io_sibling_prev = prev;
		if (prev != NULL)
			prev->io_sibling_next = next;
		if (pio->io_child == zio)
			pio->io_child = next;
		mutex_exit(&pio->io_lock);

		zio_notify_parent(zio, ZIO_STAGE_WAIT_CHILDREN_DONE,
		    &pio->io_children_notdone);
	}

	/*
	 * Note: this I/O is now done, and will shortly be freed, so there is no
	 * need to clear this (or any other) flag.
	 */
	if (zio->io_flags & ZIO_FLAG_CONFIG_GRABBED)
		spa_config_exit(spa, zio);

	if (zio->io_waiter != NULL) {
		mutex_enter(&zio->io_lock);
		ASSERT(zio->io_stage == ZIO_STAGE_DONE);
		zio->io_stalled = zio->io_stage;
		cv_broadcast(&zio->io_cv);
		mutex_exit(&zio->io_lock);
	} else {
		cv_destroy(&zio->io_cv);
		mutex_destroy(&zio->io_lock);
		kmem_cache_free(zio_cache, zio);
	}
}

/*
 * ==========================================================================
 * Compression support
 * ==========================================================================
 */
static void
zio_write_compress(zio_t *zio)
{
	int compress = zio->io_compress;
	blkptr_t *bp = zio->io_bp;
	void *cbuf;
	uint64_t lsize = zio->io_size;
	uint64_t csize = lsize;
	uint64_t cbufsize = 0;
	int pass;

	if (bp->blk_birth == zio->io_txg) {
		/*
		 * We're rewriting an existing block, which means we're
		 * working on behalf of spa_sync().  For spa_sync() to
		 * converge, it must eventually be the case that we don't
		 * have to allocate new blocks.  But compression changes
		 * the blocksize, which forces a reallocate, and makes
		 * convergence take longer.  Therefore, after the first
		 * few passes, stop compressing to ensure convergence.
		 */
		pass = spa_sync_pass(zio->io_spa);
		if (pass > zio_sync_pass.zp_dontcompress)
			compress = ZIO_COMPRESS_OFF;
	} else {
		ASSERT(BP_IS_HOLE(bp));
		pass = 1;
	}

	if (compress != ZIO_COMPRESS_OFF)
		if (!zio_compress_data(compress, zio->io_data, zio->io_size,
		    &cbuf, &csize, &cbufsize))
			compress = ZIO_COMPRESS_OFF;

	if (compress != ZIO_COMPRESS_OFF && csize != 0)
		zio_push_transform(zio, cbuf, csize, cbufsize);

	/*
	 * The final pass of spa_sync() must be all rewrites, but the first
	 * few passes offer a trade-off: allocating blocks defers convergence,
	 * but newly allocated blocks are sequential, so they can be written
	 * to disk faster.  Therefore, we allow the first few passes of
	 * spa_sync() to reallocate new blocks, but force rewrites after that.
	 * There should only be a handful of blocks after pass 1 in any case.
	 */
	if (bp->blk_birth == zio->io_txg && BP_GET_PSIZE(bp) == csize &&
	    pass > zio_sync_pass.zp_rewrite) {
		ASSERT(csize != 0);
		BP_SET_LSIZE(bp, lsize);
		BP_SET_COMPRESS(bp, compress);
		zio->io_pipeline = ZIO_REWRITE_PIPELINE;
	} else {
		if (bp->blk_birth == zio->io_txg)
			BP_ZERO(bp);
		if (csize == 0) {
			BP_ZERO(bp);
			zio->io_pipeline = ZIO_WAIT_FOR_CHILDREN_PIPELINE;
		} else {
			ASSERT3U(BP_GET_NDVAS(bp), ==, 0);
			BP_SET_LSIZE(bp, lsize);
			BP_SET_PSIZE(bp, csize);
			BP_SET_COMPRESS(bp, compress);
			zio->io_pipeline = ZIO_WRITE_ALLOCATE_PIPELINE;
		}
	}

	zio_next_stage(zio);
}

static void
zio_read_decompress(zio_t *zio)
{
	blkptr_t *bp = zio->io_bp;
	void *data;
	uint64_t size;
	uint64_t bufsize;
	int compress = BP_GET_COMPRESS(bp);

	ASSERT(compress != ZIO_COMPRESS_OFF);

	zio_pop_transform(zio, &data, &size, &bufsize);

	if (zio_decompress_data(compress, data, size,
	    zio->io_data, zio->io_size))
		zio->io_error = EIO;

	zio_buf_free(data, bufsize);

	zio_next_stage(zio);
}

/*
 * ==========================================================================
 * Gang block support
 * ==========================================================================
 */
static void
zio_gang_pipeline(zio_t *zio)
{
	/*
	 * By default, the pipeline assumes that we're dealing with a gang
	 * block.  If we're not, strip out any gang-specific stages.
	 */
	if (!BP_IS_GANG(zio->io_bp))
		zio->io_pipeline &= ~ZIO_GANG_STAGES;

	zio_next_stage(zio);
}

static void
zio_gang_byteswap(zio_t *zio)
{
	ASSERT(zio->io_size == SPA_GANGBLOCKSIZE);

	if (BP_SHOULD_BYTESWAP(zio->io_bp))
		byteswap_uint64_array(zio->io_data, zio->io_size);
}

static void
zio_get_gang_header(zio_t *zio)
{
	blkptr_t *bp = zio->io_bp;
	uint64_t gsize = SPA_GANGBLOCKSIZE;
	void *gbuf = zio_buf_alloc(gsize);

	ASSERT(BP_IS_GANG(bp));

	zio_push_transform(zio, gbuf, gsize, gsize);

	zio_nowait(zio_create(zio, zio->io_spa, bp->blk_birth, bp, gbuf, gsize,
	    NULL, NULL, ZIO_TYPE_READ, zio->io_priority,
	    zio->io_flags & ZIO_FLAG_GANG_INHERIT,
	    ZIO_STAGE_OPEN, ZIO_READ_PIPELINE));

	zio_wait_children_done(zio);
}

static void
zio_read_gang_members(zio_t *zio)
{
	zio_gbh_phys_t *gbh;
	uint64_t gsize, gbufsize, loff, lsize;
	int i;

	ASSERT(BP_IS_GANG(zio->io_bp));

	zio_gang_byteswap(zio);
	zio_pop_transform(zio, (void **)&gbh, &gsize, &gbufsize);

	for (loff = 0, i = 0; loff != zio->io_size; loff += lsize, i++) {
		blkptr_t *gbp = &gbh->zg_blkptr[i];
		lsize = BP_GET_PSIZE(gbp);

		ASSERT(BP_GET_COMPRESS(gbp) == ZIO_COMPRESS_OFF);
		ASSERT3U(lsize, ==, BP_GET_LSIZE(gbp));
		ASSERT3U(loff + lsize, <=, zio->io_size);
		ASSERT(i < SPA_GBH_NBLKPTRS);
		ASSERT(!BP_IS_HOLE(gbp));

		zio_nowait(zio_read(zio, zio->io_spa, gbp,
		    (char *)zio->io_data + loff, lsize, NULL, NULL,
		    zio->io_priority, zio->io_flags & ZIO_FLAG_GANG_INHERIT,
		    &zio->io_bookmark));
	}

	zio_buf_free(gbh, gbufsize);
	zio_wait_children_done(zio);
}

static void
zio_rewrite_gang_members(zio_t *zio)
{
	zio_gbh_phys_t *gbh;
	uint64_t gsize, gbufsize, loff, lsize;
	int i;

	ASSERT(BP_IS_GANG(zio->io_bp));
	ASSERT3U(zio->io_size, ==, SPA_GANGBLOCKSIZE);

	zio_gang_byteswap(zio);
	zio_pop_transform(zio, (void **)&gbh, &gsize, &gbufsize);

	ASSERT(gsize == gbufsize);

	for (loff = 0, i = 0; loff != zio->io_size; loff += lsize, i++) {
		blkptr_t *gbp = &gbh->zg_blkptr[i];
		lsize = BP_GET_PSIZE(gbp);

		ASSERT(BP_GET_COMPRESS(gbp) == ZIO_COMPRESS_OFF);
		ASSERT3U(lsize, ==, BP_GET_LSIZE(gbp));
		ASSERT3U(loff + lsize, <=, zio->io_size);
		ASSERT(i < SPA_GBH_NBLKPTRS);
		ASSERT(!BP_IS_HOLE(gbp));

		zio_nowait(zio_rewrite(zio, zio->io_spa, zio->io_checksum,
		    zio->io_txg, gbp, (char *)zio->io_data + loff, lsize,
		    NULL, NULL, zio->io_priority, zio->io_flags,
		    &zio->io_bookmark));
	}

	zio_push_transform(zio, gbh, gsize, gbufsize);
	zio_wait_children_ready(zio);
}

static void
zio_free_gang_members(zio_t *zio)
{
	zio_gbh_phys_t *gbh;
	uint64_t gsize, gbufsize;
	int i;

	ASSERT(BP_IS_GANG(zio->io_bp));

	zio_gang_byteswap(zio);
	zio_pop_transform(zio, (void **)&gbh, &gsize, &gbufsize);

	for (i = 0; i < SPA_GBH_NBLKPTRS; i++) {
		blkptr_t *gbp = &gbh->zg_blkptr[i];

		if (BP_IS_HOLE(gbp))
			continue;
		zio_nowait(zio_free(zio, zio->io_spa, zio->io_txg,
		    gbp, NULL, NULL));
	}

	zio_buf_free(gbh, gbufsize);
	zio_next_stage(zio);
}

static void
zio_claim_gang_members(zio_t *zio)
{
	zio_gbh_phys_t *gbh;
	uint64_t gsize, gbufsize;
	int i;

	ASSERT(BP_IS_GANG(zio->io_bp));

	zio_gang_byteswap(zio);
	zio_pop_transform(zio, (void **)&gbh, &gsize, &gbufsize);

	for (i = 0; i < SPA_GBH_NBLKPTRS; i++) {
		blkptr_t *gbp = &gbh->zg_blkptr[i];
		if (BP_IS_HOLE(gbp))
			continue;
		zio_nowait(zio_claim(zio, zio->io_spa, zio->io_txg,
		    gbp, NULL, NULL));
	}

	zio_buf_free(gbh, gbufsize);
	zio_next_stage(zio);
}

static void
zio_write_allocate_gang_member_done(zio_t *zio)
{
	zio_t *pio = zio->io_parent;
	dva_t *cdva = zio->io_bp->blk_dva;
	dva_t *pdva = pio->io_bp->blk_dva;
	uint64_t asize;
	int d;

	ASSERT3U(pio->io_ndvas, ==, zio->io_ndvas);
	ASSERT3U(BP_GET_NDVAS(zio->io_bp), <=, BP_GET_NDVAS(pio->io_bp));
	ASSERT3U(zio->io_ndvas, <=, BP_GET_NDVAS(zio->io_bp));
	ASSERT3U(pio->io_ndvas, <=, BP_GET_NDVAS(pio->io_bp));

	mutex_enter(&pio->io_lock);
	for (d = 0; d < BP_GET_NDVAS(pio->io_bp); d++) {
		ASSERT(DVA_GET_GANG(&pdva[d]));
		asize = DVA_GET_ASIZE(&pdva[d]);
		asize += DVA_GET_ASIZE(&cdva[d]);
		DVA_SET_ASIZE(&pdva[d], asize);
	}
	mutex_exit(&pio->io_lock);
}

static void
zio_write_allocate_gang_members(zio_t *zio)
{
	blkptr_t *bp = zio->io_bp;
	dva_t *dva = bp->blk_dva;
	spa_t *spa = zio->io_spa;
	zio_gbh_phys_t *gbh;
	uint64_t txg = zio->io_txg;
	uint64_t resid = zio->io_size;
	uint64_t maxalloc = P2ROUNDUP(zio->io_size >> 1, SPA_MINBLOCKSIZE);
	uint64_t gsize, loff, lsize;
	uint32_t gbps_left;
	int ndvas = zio->io_ndvas;
	int gbh_ndvas = MIN(ndvas + 1, spa_max_replication(spa));
	int error;
	int i, d;

	gsize = SPA_GANGBLOCKSIZE;
	gbps_left = SPA_GBH_NBLKPTRS;

	error = metaslab_alloc(spa, gsize, bp, gbh_ndvas, txg, NULL, B_FALSE);
	if (error == ENOSPC)
		panic("can't allocate gang block header");
	ASSERT(error == 0);

	for (d = 0; d < gbh_ndvas; d++)
		DVA_SET_GANG(&dva[d], 1);

	bp->blk_birth = txg;

	gbh = zio_buf_alloc(gsize);
	bzero(gbh, gsize);

	/* We need to test multi-level gang blocks */
	if (maxalloc >= zio_gang_bang && (lbolt & 0x1) == 0)
		maxalloc = MAX(maxalloc >> 2, SPA_MINBLOCKSIZE);

	for (loff = 0, i = 0; loff != zio->io_size;
	    loff += lsize, resid -= lsize, gbps_left--, i++) {
		blkptr_t *gbp = &gbh->zg_blkptr[i];
		dva = gbp->blk_dva;

		ASSERT(gbps_left != 0);
		maxalloc = MIN(maxalloc, resid);

		while (resid <= maxalloc * gbps_left) {
			error = metaslab_alloc(spa, maxalloc, gbp, ndvas,
			    txg, bp, B_FALSE);
			if (error == 0)
				break;
			ASSERT3U(error, ==, ENOSPC);
			if (maxalloc == SPA_MINBLOCKSIZE)
				panic("really out of space");
			maxalloc = P2ROUNDUP(maxalloc >> 1, SPA_MINBLOCKSIZE);
		}

		if (resid <= maxalloc * gbps_left) {
			lsize = maxalloc;
			BP_SET_LSIZE(gbp, lsize);
			BP_SET_PSIZE(gbp, lsize);
			BP_SET_COMPRESS(gbp, ZIO_COMPRESS_OFF);
			gbp->blk_birth = txg;
			zio_nowait(zio_rewrite(zio, spa,
			    zio->io_checksum, txg, gbp,
			    (char *)zio->io_data + loff, lsize,
			    zio_write_allocate_gang_member_done, NULL,
			    zio->io_priority, zio->io_flags,
			    &zio->io_bookmark));
		} else {
			lsize = P2ROUNDUP(resid / gbps_left, SPA_MINBLOCKSIZE);
			ASSERT(lsize != SPA_MINBLOCKSIZE);
			zio_nowait(zio_write_allocate(zio, spa,
			    zio->io_checksum, txg, gbp,
			    (char *)zio->io_data + loff, lsize,
			    zio_write_allocate_gang_member_done, NULL,
			    zio->io_priority, zio->io_flags));
		}
	}

	ASSERT(resid == 0 && loff == zio->io_size);

	zio->io_pipeline |= 1U << ZIO_STAGE_GANG_CHECKSUM_GENERATE;

	zio_push_transform(zio, gbh, gsize, gsize);
	/*
	 * As much as we'd like this to be zio_wait_children_ready(),
	 * updating our ASIZE doesn't happen until the io_done callback,
	 * so we have to wait for that to finish in order for our BP
	 * to be stable.
	 */
	zio_wait_children_done(zio);
}

/*
 * ==========================================================================
 * Allocate and free blocks
 * ==========================================================================
 */
static void
zio_dva_allocate(zio_t *zio)
{
	blkptr_t *bp = zio->io_bp;
	int error;

	ASSERT(BP_IS_HOLE(bp));
	ASSERT3U(BP_GET_NDVAS(bp), ==, 0);
	ASSERT3U(zio->io_ndvas, >, 0);
	ASSERT3U(zio->io_ndvas, <=, spa_max_replication(zio->io_spa));

	/* For testing, make some blocks above a certain size be gang blocks */
	if (zio->io_size >= zio_gang_bang && (lbolt & 0x3) == 0) {
		zio_write_allocate_gang_members(zio);
		return;
	}

	ASSERT3U(zio->io_size, ==, BP_GET_PSIZE(bp));

	error = metaslab_alloc(zio->io_spa, zio->io_size, bp, zio->io_ndvas,
	    zio->io_txg, NULL, B_FALSE);

	if (error == 0) {
		bp->blk_birth = zio->io_txg;
	} else if (error == ENOSPC) {
		if (zio->io_size == SPA_MINBLOCKSIZE)
			panic("really, truly out of space");
		zio_write_allocate_gang_members(zio);
		return;
	} else {
		zio->io_error = error;
	}
	zio_next_stage(zio);
}

static void
zio_dva_free(zio_t *zio)
{
	blkptr_t *bp = zio->io_bp;

	metaslab_free(zio->io_spa, bp, zio->io_txg, B_FALSE);

	BP_ZERO(bp);

	zio_next_stage(zio);
}

static void
zio_dva_claim(zio_t *zio)
{
	zio->io_error = metaslab_claim(zio->io_spa, zio->io_bp, zio->io_txg);

	zio_next_stage(zio);
}

/*
 * ==========================================================================
 * Read and write to physical devices
 * ==========================================================================
 */

static void
zio_vdev_io_start(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	vdev_t *tvd = vd ? vd->vdev_top : NULL;
	blkptr_t *bp = zio->io_bp;
	uint64_t align;

	if (vd == NULL) {
		/* The mirror_ops handle multiple DVAs in a single BP */
		vdev_mirror_ops.vdev_op_io_start(zio);
		return;
	}

	align = 1ULL << tvd->vdev_ashift;

	if (zio->io_retries == 0 && vd == tvd)
		zio->io_flags |= ZIO_FLAG_FAILFAST;

	if (!(zio->io_flags & ZIO_FLAG_PHYSICAL) &&
	    vd->vdev_children == 0) {
		zio->io_flags |= ZIO_FLAG_PHYSICAL;
		zio->io_offset += VDEV_LABEL_START_SIZE;
	}

	if (P2PHASE(zio->io_size, align) != 0) {
		uint64_t asize = P2ROUNDUP(zio->io_size, align);
		char *abuf = zio_buf_alloc(asize);
		ASSERT(vd == tvd);
		if (zio->io_type == ZIO_TYPE_WRITE) {
			bcopy(zio->io_data, abuf, zio->io_size);
			bzero(abuf + zio->io_size, asize - zio->io_size);
		}
		zio_push_transform(zio, abuf, asize, asize);
		ASSERT(!(zio->io_flags & ZIO_FLAG_SUBBLOCK));
		zio->io_flags |= ZIO_FLAG_SUBBLOCK;
	}

	ASSERT(P2PHASE(zio->io_offset, align) == 0);
	ASSERT(P2PHASE(zio->io_size, align) == 0);
	ASSERT(bp == NULL ||
	    P2ROUNDUP(ZIO_GET_IOSIZE(zio), align) == zio->io_size);
	ASSERT(zio->io_type != ZIO_TYPE_WRITE || (spa_mode & FWRITE));

	vdev_io_start(zio);

	/* zio_next_stage_async() gets called from io completion interrupt */
}

static void
zio_vdev_io_done(zio_t *zio)
{
	if (zio->io_vd == NULL)
		/* The mirror_ops handle multiple DVAs in a single BP */
		vdev_mirror_ops.vdev_op_io_done(zio);
	else
		vdev_io_done(zio);
}

/* XXPOLICY */
boolean_t
zio_should_retry(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;

	if (zio->io_error == 0)
		return (B_FALSE);
	if (zio->io_delegate_list != NULL)
		return (B_FALSE);
	if (vd && vd != vd->vdev_top)
		return (B_FALSE);
	if (zio->io_flags & ZIO_FLAG_DONT_RETRY)
		return (B_FALSE);
	if (zio->io_retries > 0)
		return (B_FALSE);

	return (B_TRUE);
}

static void
zio_vdev_io_assess(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	vdev_t *tvd = vd ? vd->vdev_top : NULL;

	ASSERT(zio->io_vsd == NULL);

	if (zio->io_flags & ZIO_FLAG_SUBBLOCK) {
		void *abuf;
		uint64_t asize;
		ASSERT(vd == tvd);
		zio_pop_transform(zio, &abuf, &asize, &asize);
		if (zio->io_type == ZIO_TYPE_READ)
			bcopy(abuf, zio->io_data, zio->io_size);
		zio_buf_free(abuf, asize);
		zio->io_flags &= ~ZIO_FLAG_SUBBLOCK;
	}

	if (zio_injection_enabled && !zio->io_error)
		zio->io_error = zio_handle_fault_injection(zio, EIO);

	/*
	 * If the I/O failed, determine whether we should attempt to retry it.
	 */
	/* XXPOLICY */
	if (zio_should_retry(zio)) {
		ASSERT(tvd == vd);

		zio->io_retries++;
		zio->io_error = 0;
		zio->io_flags &= ZIO_FLAG_VDEV_INHERIT |
		    ZIO_FLAG_CONFIG_GRABBED;
		/* XXPOLICY */
		zio->io_flags &= ~ZIO_FLAG_FAILFAST;
		zio->io_flags |= ZIO_FLAG_DONT_CACHE;
		zio->io_stage = ZIO_STAGE_VDEV_IO_START - 1;

		dprintf("retry #%d for %s to %s offset %llx\n",
		    zio->io_retries, zio_type_name[zio->io_type],
		    vdev_description(vd), zio->io_offset);

		zio_next_stage_async(zio);
		return;
	}

	if (zio->io_error != 0 && zio->io_error != ECKSUM &&
	    !(zio->io_flags & ZIO_FLAG_SPECULATIVE) && vd) {
		/*
		 * Poor man's hotplug support.  Even if we're done retrying this
		 * I/O, try to reopen the vdev to see if it's still attached.
		 * To avoid excessive thrashing, we only try it once a minute.
		 * This also has the effect of detecting when missing devices
		 * have come back, by polling the device once a minute.
		 *
		 * We need to do this asynchronously because we can't grab
		 * all the necessary locks way down here.
		 */
		if (gethrtime() - vd->vdev_last_try > 60ULL * NANOSEC) {
			vd->vdev_last_try = gethrtime();
			tvd->vdev_reopen_wanted = 1;
			spa_async_request(vd->vdev_spa, SPA_ASYNC_REOPEN);
		}
	}

	zio_next_stage(zio);
}

void
zio_vdev_io_reissue(zio_t *zio)
{
	ASSERT(zio->io_stage == ZIO_STAGE_VDEV_IO_START);
	ASSERT(zio->io_error == 0);

	zio->io_stage--;
}

void
zio_vdev_io_redone(zio_t *zio)
{
	ASSERT(zio->io_stage == ZIO_STAGE_VDEV_IO_DONE);

	zio->io_stage--;
}

void
zio_vdev_io_bypass(zio_t *zio)
{
	ASSERT(zio->io_stage == ZIO_STAGE_VDEV_IO_START);
	ASSERT(zio->io_error == 0);

	zio->io_flags |= ZIO_FLAG_IO_BYPASS;
	zio->io_stage = ZIO_STAGE_VDEV_IO_ASSESS - 1;
}

/*
 * ==========================================================================
 * Generate and verify checksums
 * ==========================================================================
 */
static void
zio_checksum_generate(zio_t *zio)
{
	int checksum = zio->io_checksum;
	blkptr_t *bp = zio->io_bp;

	ASSERT3U(zio->io_size, ==, BP_GET_PSIZE(bp));

	BP_SET_CHECKSUM(bp, checksum);
	BP_SET_BYTEORDER(bp, ZFS_HOST_BYTEORDER);

	zio_checksum(checksum, &bp->blk_cksum, zio->io_data, zio->io_size);

	zio_next_stage(zio);
}

static void
zio_gang_checksum_generate(zio_t *zio)
{
	zio_cksum_t zc;
	zio_gbh_phys_t *gbh = zio->io_data;

	ASSERT(BP_IS_GANG(zio->io_bp));
	ASSERT3U(zio->io_size, ==, SPA_GANGBLOCKSIZE);

	zio_set_gang_verifier(zio, &gbh->zg_tail.zbt_cksum);

	zio_checksum(ZIO_CHECKSUM_GANG_HEADER, &zc, zio->io_data, zio->io_size);

	zio_next_stage(zio);
}

static void
zio_checksum_verify(zio_t *zio)
{
	if (zio->io_bp != NULL) {
		zio->io_error = zio_checksum_error(zio);
		if (zio->io_error && !(zio->io_flags & ZIO_FLAG_SPECULATIVE))
			zfs_ereport_post(FM_EREPORT_ZFS_CHECKSUM,
			    zio->io_spa, zio->io_vd, zio, 0, 0);
	}

	zio_next_stage(zio);
}

/*
 * Called by RAID-Z to ensure we don't compute the checksum twice.
 */
void
zio_checksum_verified(zio_t *zio)
{
	zio->io_pipeline &= ~(1U << ZIO_STAGE_CHECKSUM_VERIFY);
}

/*
 * Set the external verifier for a gang block based on stuff in the bp
 */
void
zio_set_gang_verifier(zio_t *zio, zio_cksum_t *zcp)
{
	blkptr_t *bp = zio->io_bp;

	zcp->zc_word[0] = DVA_GET_VDEV(BP_IDENTITY(bp));
	zcp->zc_word[1] = DVA_GET_OFFSET(BP_IDENTITY(bp));
	zcp->zc_word[2] = bp->blk_birth;
	zcp->zc_word[3] = 0;
}

/*
 * ==========================================================================
 * Define the pipeline
 * ==========================================================================
 */
typedef void zio_pipe_stage_t(zio_t *zio);

static void
zio_badop(zio_t *zio)
{
	panic("Invalid I/O pipeline stage %u for zio %p", zio->io_stage, zio);
}

zio_pipe_stage_t *zio_pipeline[ZIO_STAGE_DONE + 2] = {
	zio_badop,
	zio_wait_children_ready,
	zio_write_compress,
	zio_checksum_generate,
	zio_gang_pipeline,
	zio_get_gang_header,
	zio_rewrite_gang_members,
	zio_free_gang_members,
	zio_claim_gang_members,
	zio_dva_allocate,
	zio_dva_free,
	zio_dva_claim,
	zio_gang_checksum_generate,
	zio_ready,
	zio_vdev_io_start,
	zio_vdev_io_done,
	zio_vdev_io_assess,
	zio_wait_children_done,
	zio_checksum_verify,
	zio_read_gang_members,
	zio_read_decompress,
	zio_done,
	zio_badop
};

/*
 * Move an I/O to the next stage of the pipeline and execute that stage.
 * There's no locking on io_stage because there's no legitimate way for
 * multiple threads to be attempting to process the same I/O.
 */
void
zio_next_stage(zio_t *zio)
{
	uint32_t pipeline = zio->io_pipeline;

	ASSERT(!MUTEX_HELD(&zio->io_lock));

	if (zio->io_error) {
		dprintf("zio %p vdev %s offset %llx stage %d error %d\n",
		    zio, vdev_description(zio->io_vd),
		    zio->io_offset, zio->io_stage, zio->io_error);
		if (((1U << zio->io_stage) & ZIO_VDEV_IO_PIPELINE) == 0)
			pipeline &= ZIO_ERROR_PIPELINE_MASK;
	}

	while (((1U << ++zio->io_stage) & pipeline) == 0)
		continue;

	ASSERT(zio->io_stage <= ZIO_STAGE_DONE);
	ASSERT(zio->io_stalled == 0);

	/*
	 * See the comment in zio_next_stage_async() about per-CPU taskqs.
	 */
	if (((1U << zio->io_stage) & zio->io_async_stages) &&
	    (zio->io_stage == ZIO_STAGE_WRITE_COMPRESS) &&
	    !(zio->io_flags & ZIO_FLAG_METADATA)) {
		taskq_t *tq = zio->io_spa->spa_zio_issue_taskq[zio->io_type];
		(void) taskq_dispatch(tq,
		    (task_func_t *)zio_pipeline[zio->io_stage], zio, TQ_SLEEP);
	} else {
		zio_pipeline[zio->io_stage](zio);
	}
}

void
zio_next_stage_async(zio_t *zio)
{
	taskq_t *tq;
	uint32_t pipeline = zio->io_pipeline;

	ASSERT(!MUTEX_HELD(&zio->io_lock));

	if (zio->io_error) {
		dprintf("zio %p vdev %s offset %llx stage %d error %d\n",
		    zio, vdev_description(zio->io_vd),
		    zio->io_offset, zio->io_stage, zio->io_error);
		if (((1U << zio->io_stage) & ZIO_VDEV_IO_PIPELINE) == 0)
			pipeline &= ZIO_ERROR_PIPELINE_MASK;
	}

	while (((1U << ++zio->io_stage) & pipeline) == 0)
		continue;

	ASSERT(zio->io_stage <= ZIO_STAGE_DONE);
	ASSERT(zio->io_stalled == 0);

	/*
	 * For performance, we'll probably want two sets of task queues:
	 * per-CPU issue taskqs and per-CPU completion taskqs.  The per-CPU
	 * part is for read performance: since we have to make a pass over
	 * the data to checksum it anyway, we want to do this on the same CPU
	 * that issued the read, because (assuming CPU scheduling affinity)
	 * that thread is probably still there.  Getting this optimization
	 * right avoids performance-hostile cache-to-cache transfers.
	 *
	 * Note that having two sets of task queues is also necessary for
	 * correctness: if all of the issue threads get bogged down waiting
	 * for dependent reads (e.g. metaslab freelist) to complete, then
	 * there won't be any threads available to service I/O completion
	 * interrupts.
	 */
	if ((1U << zio->io_stage) & zio->io_async_stages) {
		if (zio->io_stage < ZIO_STAGE_VDEV_IO_DONE)
			tq = zio->io_spa->spa_zio_issue_taskq[zio->io_type];
		else
			tq = zio->io_spa->spa_zio_intr_taskq[zio->io_type];
		(void) taskq_dispatch(tq,
		    (task_func_t *)zio_pipeline[zio->io_stage], zio, TQ_SLEEP);
	} else {
		zio_pipeline[zio->io_stage](zio);
	}
}

static boolean_t
zio_alloc_should_fail(void)
{
	static uint16_t	allocs = 0;

	return (P2PHASE(allocs++, 1U<<zio_zil_fail_shift) == 0);
}

/*
 * Try to allocate an intent log block.  Return 0 on success, errno on failure.
 */
int
zio_alloc_blk(spa_t *spa, uint64_t size, blkptr_t *new_bp, blkptr_t *old_bp,
    uint64_t txg)
{
	int error;

	spa_config_enter(spa, RW_READER, FTAG);

	if (zio_zil_fail_shift && zio_alloc_should_fail()) {
		spa_config_exit(spa, FTAG);
		return (ENOSPC);
	}

	/*
	 * We were passed the previous log blocks dva_t in bp->blk_dva[0].
	 */
	error = metaslab_alloc(spa, size, new_bp, 1, txg, old_bp, B_TRUE);

	if (error == 0) {
		BP_SET_LSIZE(new_bp, size);
		BP_SET_PSIZE(new_bp, size);
		BP_SET_COMPRESS(new_bp, ZIO_COMPRESS_OFF);
		BP_SET_CHECKSUM(new_bp, ZIO_CHECKSUM_ZILOG);
		BP_SET_TYPE(new_bp, DMU_OT_INTENT_LOG);
		BP_SET_LEVEL(new_bp, 0);
		BP_SET_BYTEORDER(new_bp, ZFS_HOST_BYTEORDER);
		new_bp->blk_birth = txg;
	}

	spa_config_exit(spa, FTAG);

	return (error);
}

/*
 * Free an intent log block.  We know it can't be a gang block, so there's
 * nothing to do except metaslab_free() it.
 */
void
zio_free_blk(spa_t *spa, blkptr_t *bp, uint64_t txg)
{
	ASSERT(!BP_IS_GANG(bp));

	spa_config_enter(spa, RW_READER, FTAG);

	metaslab_free(spa, bp, txg, B_FALSE);

	spa_config_exit(spa, FTAG);
}
