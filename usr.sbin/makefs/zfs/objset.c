/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>

#include "zfs.h"

#define	DNODES_PER_CHUNK	(MAXBLOCKSIZE / sizeof(dnode_phys_t))

struct objset_dnode_chunk {
	dnode_phys_t	buf[DNODES_PER_CHUNK];
	unsigned int	nextfree;
	STAILQ_ENTRY(objset_dnode_chunk) next;
};

typedef struct zfs_objset {
	/* Physical object set. */
	objset_phys_t	*phys;
	off_t		osloc;
	off_t		osblksz;
	blkptr_t	osbp;		/* set in objset_write() */

	/* Accounting. */
	off_t		space;		/* bytes allocated to this objset */

	/* dnode allocator. */
	uint64_t	dnodecount;
	STAILQ_HEAD(, objset_dnode_chunk) dnodechunks;
} zfs_objset_t;

static void
dnode_init(dnode_phys_t *dnode, uint8_t type, uint8_t bonustype,
    uint16_t bonuslen)
{
	dnode->dn_indblkshift = MAXBLOCKSHIFT;
	dnode->dn_type = type;
	dnode->dn_bonustype = bonustype;
	dnode->dn_bonuslen = bonuslen;
	dnode->dn_checksum = ZIO_CHECKSUM_FLETCHER_4;
	dnode->dn_nlevels = 1;
	dnode->dn_nblkptr = 1;
	dnode->dn_flags = DNODE_FLAG_USED_BYTES;
}

zfs_objset_t *
objset_alloc(zfs_opt_t *zfs, uint64_t type)
{
	struct objset_dnode_chunk *chunk;
	zfs_objset_t *os;

	os = ecalloc(1, sizeof(*os));
	os->osblksz = sizeof(objset_phys_t);
	os->osloc = objset_space_alloc(zfs, os, &os->osblksz);

	/*
	 * Object ID zero is always reserved for the meta dnode, which is
	 * embedded in the objset itself.
	 */
	STAILQ_INIT(&os->dnodechunks);
	chunk = ecalloc(1, sizeof(*chunk));
	chunk->nextfree = 1;
	STAILQ_INSERT_HEAD(&os->dnodechunks, chunk, next);
	os->dnodecount = 1;

	os->phys = ecalloc(1, os->osblksz);
	os->phys->os_type = type;

	dnode_init(&os->phys->os_meta_dnode, DMU_OT_DNODE, DMU_OT_NONE, 0);
	os->phys->os_meta_dnode.dn_datablkszsec =
	    DNODE_BLOCK_SIZE >> MINBLOCKSHIFT;

	return (os);
}

/*
 * Write the dnode array and physical object set to disk.
 */
static void
_objset_write(zfs_opt_t *zfs, zfs_objset_t *os, struct dnode_cursor *c,
    off_t loc)
{
	struct objset_dnode_chunk *chunk, *tmp;
	unsigned int total;

	/*
	 * Write out the dnode array, i.e., the meta-dnode.  For some reason its
	 * data blocks must be 16KB in size no matter how large the array is.
	 */
	total = 0;
	STAILQ_FOREACH_SAFE(chunk, &os->dnodechunks, next, tmp) {
		unsigned int i;

		assert(chunk->nextfree > 0);
		assert(chunk->nextfree <= os->dnodecount);
		assert(chunk->nextfree <= DNODES_PER_CHUNK);

		for (i = 0; i < chunk->nextfree; i += DNODES_PER_BLOCK) {
			blkptr_t *bp;
			uint64_t fill;

			if (chunk->nextfree - i < DNODES_PER_BLOCK)
				fill = DNODES_PER_BLOCK - (chunk->nextfree - i);
			else
				fill = 0;
			bp = dnode_cursor_next(zfs, c,
			    (total + i) * sizeof(dnode_phys_t));
			vdev_pwrite_dnode_indir(zfs, &os->phys->os_meta_dnode,
			    0, fill, chunk->buf + i, DNODE_BLOCK_SIZE, loc, bp);
			loc += DNODE_BLOCK_SIZE;
		}
		total += i;

		free(chunk);
	}
	dnode_cursor_finish(zfs, c);
	STAILQ_INIT(&os->dnodechunks);

	/*
	 * Write the object set itself.  The saved block pointer will be copied
	 * into the referencing DSL dataset or the uberblocks.
	 */
	vdev_pwrite_data(zfs, DMU_OT_OBJSET, ZIO_CHECKSUM_FLETCHER_4, 0,
	    os->dnodecount - 1, os->phys, os->osblksz, os->osloc, &os->osbp);
}

void
objset_write(zfs_opt_t *zfs, zfs_objset_t *os)
{
	struct dnode_cursor *c;
	off_t dnodeloc, dnodesz;
	uint64_t dnodecount;

	/*
	 * There is a chicken-and-egg problem here when writing the MOS: we
	 * cannot write space maps before we're finished allocating space from
	 * the vdev, and we can't write the MOS without having allocated space
	 * for indirect dnode blocks.  Thus, rather than lazily allocating
	 * indirect blocks for the meta-dnode (which would be simpler), they are
	 * allocated up-front and before writing space maps.
	 */
	dnodecount = os->dnodecount;
	if (os == zfs->mos)
		dnodecount += zfs->mscount;
	dnodesz = dnodecount * sizeof(dnode_phys_t);
	c = dnode_cursor_init(zfs, os, &os->phys->os_meta_dnode, dnodesz,
	    DNODE_BLOCK_SIZE);
	dnodesz = roundup2(dnodesz, DNODE_BLOCK_SIZE);
	dnodeloc = objset_space_alloc(zfs, os, &dnodesz);

	if (os == zfs->mos) {
		vdev_spacemap_write(zfs);

		/*
		 * We've finished allocating space, account for it in $MOS and
		 * in the parent directory.
		 */
		dsl_dir_root_finalize(zfs, os->space);
	}
	_objset_write(zfs, os, c, dnodeloc);
}

dnode_phys_t *
objset_dnode_bonus_alloc(zfs_objset_t *os, uint8_t type, uint8_t bonustype,
    uint16_t bonuslen, uint64_t *idp)
{
	struct objset_dnode_chunk *chunk;
	dnode_phys_t *dnode;

	assert(bonuslen <= DN_OLD_MAX_BONUSLEN);
	assert(!STAILQ_EMPTY(&os->dnodechunks));

	chunk = STAILQ_LAST(&os->dnodechunks, objset_dnode_chunk, next);
	if (chunk->nextfree == DNODES_PER_CHUNK) {
		chunk = ecalloc(1, sizeof(*chunk));
		STAILQ_INSERT_TAIL(&os->dnodechunks, chunk, next);
	}
	*idp = os->dnodecount++;
	dnode = &chunk->buf[chunk->nextfree++];
	dnode_init(dnode, type, bonustype, bonuslen);
	dnode->dn_datablkszsec = os->osblksz >> MINBLOCKSHIFT;
	return (dnode);
}

dnode_phys_t *
objset_dnode_alloc(zfs_objset_t *os, uint8_t type, uint64_t *idp)
{
	return (objset_dnode_bonus_alloc(os, type, DMU_OT_NONE, 0, idp));
}

/*
 * Look up a physical dnode by ID.  This is not used often so a linear search is
 * fine.
 */
dnode_phys_t *
objset_dnode_lookup(zfs_objset_t *os, uint64_t id)
{
	struct objset_dnode_chunk *chunk;

	assert(id > 0);
	assert(id < os->dnodecount);

	STAILQ_FOREACH(chunk, &os->dnodechunks, next) {
		if (id < DNODES_PER_CHUNK)
			return (&chunk->buf[id]);
		id -= DNODES_PER_CHUNK;
	}
	assert(0);
	return (NULL);
}

off_t
objset_space_alloc(zfs_opt_t *zfs, zfs_objset_t *os, off_t *lenp)
{
	off_t loc;

	loc = vdev_space_alloc(zfs, lenp);
	os->space += *lenp;
	return (loc);
}

uint64_t
objset_space(const zfs_objset_t *os)
{
	return (os->space);
}

void
objset_root_blkptr_copy(const zfs_objset_t *os, blkptr_t *bp)
{
	memcpy(bp, &os->osbp, sizeof(blkptr_t));
}
