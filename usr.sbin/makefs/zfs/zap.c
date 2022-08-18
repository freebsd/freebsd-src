/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/types.h>
#include <sys/endian.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>

#include "makefs.h"
#include "zfs.h"

typedef struct zfs_zap_entry {
	char		*name;		/* entry key, private copy */
	uint64_t	hash;		/* key hash */
	union {
		uint8_t	 *valp;
		uint16_t *val16p;
		uint32_t *val32p;
		uint64_t *val64p;
	};				/* entry value, an integer array */
	uint64_t	val64;		/* embedded value for a common case */
	size_t		intsz;		/* array element size; 1, 2, 4 or 8 */
	size_t		intcnt;		/* array size */
	STAILQ_ENTRY(zfs_zap_entry) next;
} zfs_zap_entry_t;

struct zfs_zap {
	STAILQ_HEAD(, zfs_zap_entry) kvps;
	uint64_t	hashsalt;	/* key hash input */
	unsigned long	kvpcnt;		/* number of key-value pairs */
	unsigned long	chunks;		/* count of chunks needed for fat ZAP */
	bool		micro;		/* can this be a micro ZAP? */

	dnode_phys_t	*dnode;		/* backpointer */
	zfs_objset_t	*os;		/* backpointer */
};

static uint16_t
zap_entry_chunks(zfs_zap_entry_t *ent)
{
	return (1 + howmany(strlen(ent->name) + 1, ZAP_LEAF_ARRAY_BYTES) +
	    howmany(ent->intsz * ent->intcnt, ZAP_LEAF_ARRAY_BYTES));
}

static uint64_t
zap_hash(uint64_t salt, const char *name)
{
	static uint64_t crc64_table[256];
	const uint64_t crc64_poly = 0xC96C5795D7870F42UL;
	const uint8_t *cp;
	uint64_t crc;
	uint8_t c;

	assert(salt != 0);
	if (crc64_table[128] == 0) {
		for (int i = 0; i < 256; i++) {
			uint64_t *t;

			t = crc64_table + i;
			*t = i;
			for (int j = 8; j > 0; j--)
				*t = (*t >> 1) ^ (-(*t & 1) & crc64_poly);
		}
	}
	assert(crc64_table[128] == crc64_poly);

	for (cp = (const uint8_t *)name, crc = salt; (c = *cp) != '\0'; cp++)
		crc = (crc >> 8) ^ crc64_table[(crc ^ c) & 0xFF];

	/*
	 * Only use 28 bits, since we need 4 bits in the cookie for the
	 * collision differentiator.  We MUST use the high bits, since
	 * those are the ones that we first pay attention to when
	 * choosing the bucket.
	 */
	crc &= ~((1ULL << (64 - ZAP_HASHBITS)) - 1);

	return (crc);
}

zfs_zap_t *
zap_alloc(zfs_objset_t *os, dnode_phys_t *dnode)
{
	zfs_zap_t *zap;

	zap = ecalloc(1, sizeof(*zap));
	STAILQ_INIT(&zap->kvps);
	zap->hashsalt = ((uint64_t)random() << 32) | random();
	zap->micro = true;
	zap->kvpcnt = 0;
	zap->chunks = 0;
	zap->dnode = dnode;
	zap->os = os;
	return (zap);
}

void
zap_add(zfs_zap_t *zap, const char *name, size_t intsz, size_t intcnt,
    const uint8_t *val)
{
	zfs_zap_entry_t *ent;

	assert(intsz == 1 || intsz == 2 || intsz == 4 || intsz == 8);
	assert(strlen(name) + 1 <= ZAP_MAXNAMELEN);
	assert(intcnt <= ZAP_MAXVALUELEN && intcnt * intsz <= ZAP_MAXVALUELEN);

	ent = ecalloc(1, sizeof(*ent));
	ent->name = estrdup(name);
	ent->hash = zap_hash(zap->hashsalt, ent->name);
	ent->intsz = intsz;
	ent->intcnt = intcnt;
	if (intsz == sizeof(uint64_t) && intcnt == 1) {
		/*
		 * Micro-optimization to elide a memory allocation in that most
		 * common case where this is a directory entry.
		 */
		ent->val64p = &ent->val64;
	} else {
		ent->valp = ecalloc(intcnt, intsz);
	}
	memcpy(ent->valp, val, intcnt * intsz);
	zap->kvpcnt++;
	zap->chunks += zap_entry_chunks(ent);
	STAILQ_INSERT_TAIL(&zap->kvps, ent, next);

	if (zap->micro && (intcnt != 1 || intsz != sizeof(uint64_t) ||
	    strlen(name) + 1 > MZAP_NAME_LEN || zap->kvpcnt > MZAP_ENT_MAX))
		zap->micro = false;
}

void
zap_add_uint64(zfs_zap_t *zap, const char *name, uint64_t val)
{
	zap_add(zap, name, sizeof(uint64_t), 1, (uint8_t *)&val);
}

void
zap_add_string(zfs_zap_t *zap, const char *name, const char *val)
{
	zap_add(zap, name, 1, strlen(val) + 1, val);
}

bool
zap_entry_exists(zfs_zap_t *zap, const char *name)
{
	zfs_zap_entry_t *ent;

	STAILQ_FOREACH(ent, &zap->kvps, next) {
		if (strcmp(ent->name, name) == 0)
			return (true);
	}
	return (false);
}

static void
zap_micro_write(zfs_opt_t *zfs, zfs_zap_t *zap)
{
	dnode_phys_t *dnode;
	zfs_zap_entry_t *ent;
	mzap_phys_t *mzap;
	mzap_ent_phys_t *ment;
	off_t bytes, loc;

	memset(zfs->filebuf, 0, sizeof(zfs->filebuf));
	mzap = (mzap_phys_t *)&zfs->filebuf[0];
	mzap->mz_block_type = ZBT_MICRO;
	mzap->mz_salt = zap->hashsalt;
	mzap->mz_normflags = 0;

	bytes = sizeof(*mzap) + (zap->kvpcnt - 1) * sizeof(*ment);
	assert(bytes <= (off_t)MZAP_MAX_BLKSZ);

	ment = &mzap->mz_chunk[0];
	STAILQ_FOREACH(ent, &zap->kvps, next) {
		memcpy(&ment->mze_value, ent->valp, ent->intsz * ent->intcnt);
		ment->mze_cd = 0; /* XXX-MJ */
		strlcpy(ment->mze_name, ent->name, sizeof(ment->mze_name));
		ment++;
	}

	loc = objset_space_alloc(zfs, zap->os, &bytes);

	dnode = zap->dnode;
	dnode->dn_maxblkid = 0;
	dnode->dn_datablkszsec = bytes >> MINBLOCKSHIFT;
	dnode->dn_flags = DNODE_FLAG_USED_BYTES;

	vdev_pwrite_dnode_data(zfs, dnode, zfs->filebuf, bytes, loc);
}

/*
 * Write some data to the fat ZAP leaf chunk starting at index "li".
 *
 * Note that individual integers in the value may be split among consecutive
 * leaves.
 */
static void
zap_fat_write_array_chunk(zap_leaf_t *l, uint16_t li, size_t sz,
    const uint8_t *val)
{
	struct zap_leaf_array *la;

	assert(sz <= ZAP_MAXVALUELEN);

	for (uint16_t n, resid = sz; resid > 0; resid -= n, val += n, li++) {
		n = MIN(resid, ZAP_LEAF_ARRAY_BYTES);

		la = &ZAP_LEAF_CHUNK(l, li).l_array;
		assert(la->la_type == ZAP_CHUNK_FREE);
		la->la_type = ZAP_CHUNK_ARRAY;
		memcpy(la->la_array, val, n);
		la->la_next = li + 1;
	}
	la->la_next = 0xffff;
}

/*
 * Find the shortest hash prefix length which lets us distribute keys without
 * overflowing a leaf block.  This is not (space) optimal, but is simple, and
 * directories large enough to overflow a single 128KB leaf block are uncommon.
 */
static unsigned int
zap_fat_write_prefixlen(zfs_zap_t *zap, zap_leaf_t *l)
{
	zfs_zap_entry_t *ent;
	unsigned int prefixlen;

	if (zap->chunks <= ZAP_LEAF_NUMCHUNKS(l)) {
		/*
		 * All chunks will fit in a single leaf block.
		 */
		return (0);
	}

	for (prefixlen = 1; prefixlen < (unsigned int)l->l_bs; prefixlen++) {
		uint32_t *leafchunks;

		leafchunks = ecalloc(1u << prefixlen, sizeof(*leafchunks));
		STAILQ_FOREACH(ent, &zap->kvps, next) {
			uint64_t li;
			uint16_t chunks;

			li = ZAP_HASH_IDX(ent->hash, prefixlen);

			chunks = zap_entry_chunks(ent);
			if (ZAP_LEAF_NUMCHUNKS(l) - leafchunks[li] < chunks) {
				/*
				 * Not enough space, grow the prefix and retry.
				 */
				break;
			}
			leafchunks[li] += chunks;
		}
		free(leafchunks);

		if (ent == NULL) {
			/*
			 * Everything fits, we're done.
			 */
			break;
		}
	}

	/*
	 * If this fails, then we need to expand the pointer table.  For now
	 * this situation is unhandled since it is hard to trigger.
	 */
	assert(prefixlen < (unsigned int)l->l_bs);

	return (prefixlen);
}

/*
 * Initialize a fat ZAP leaf block.
 */
static void
zap_fat_write_leaf_init(zap_leaf_t *l, uint64_t prefix, int prefixlen)
{
	zap_leaf_phys_t *leaf;

	leaf = l->l_phys;

	leaf->l_hdr.lh_block_type = ZBT_LEAF;
	leaf->l_hdr.lh_magic = ZAP_LEAF_MAGIC;
	leaf->l_hdr.lh_nfree = ZAP_LEAF_NUMCHUNKS(l);
	leaf->l_hdr.lh_prefix = prefix;
	leaf->l_hdr.lh_prefix_len = prefixlen;

	/* Initialize the leaf hash table. */
	assert(leaf->l_hdr.lh_nfree < 0xffff);
	memset(leaf->l_hash, 0xff,
	    ZAP_LEAF_HASH_NUMENTRIES(l) * sizeof(*leaf->l_hash));

	/* Initialize the leaf chunks. */
	for (uint16_t i = 0; i < ZAP_LEAF_NUMCHUNKS(l); i++) {
		struct zap_leaf_free *lf;

		lf = &ZAP_LEAF_CHUNK(l, i).l_free;
		lf->lf_type = ZAP_CHUNK_FREE;
		if (i + 1 == ZAP_LEAF_NUMCHUNKS(l))
			lf->lf_next = 0xffff;
		else
			lf->lf_next = i + 1;
	}
}

static void
zap_fat_write(zfs_opt_t *zfs, zfs_zap_t *zap)
{
	struct dnode_cursor *c;
	zap_leaf_t l;
	zap_phys_t *zaphdr;
	struct zap_table_phys *zt;
	zfs_zap_entry_t *ent;
	dnode_phys_t *dnode;
	uint8_t *leafblks;
	uint64_t lblkcnt, *ptrhasht;
	off_t loc, blksz;
	size_t blkshift;
	unsigned int prefixlen;
	int ptrcnt;

	/*
	 * For simplicity, always use the largest block size.  This should be ok
	 * since most directories will be micro ZAPs, but it's space inefficient
	 * for small ZAPs and might need to be revisited.
	 */
	blkshift = MAXBLOCKSHIFT;
	blksz = (off_t)1 << blkshift;

	/*
	 * Embedded pointer tables give up to 8192 entries.  This ought to be
	 * enough for anything except massive directories.
	 */
	ptrcnt = (blksz / 2) / sizeof(uint64_t);

	memset(zfs->filebuf, 0, sizeof(zfs->filebuf));
	zaphdr = (zap_phys_t *)&zfs->filebuf[0];
	zaphdr->zap_block_type = ZBT_HEADER;
	zaphdr->zap_magic = ZAP_MAGIC;
	zaphdr->zap_num_entries = zap->kvpcnt;
	zaphdr->zap_salt = zap->hashsalt;

	l.l_bs = blkshift;
	l.l_phys = NULL;

	zt = &zaphdr->zap_ptrtbl;
	zt->zt_blk = 0;
	zt->zt_numblks = 0;
	zt->zt_shift = flsll(ptrcnt) - 1;
	zt->zt_nextblk = 0;
	zt->zt_blks_copied = 0;

	/*
	 * How many leaf blocks do we need?  Initialize them and update the
	 * header.
	 */
	prefixlen = zap_fat_write_prefixlen(zap, &l);
	lblkcnt = 1 << prefixlen;
	leafblks = ecalloc(lblkcnt, blksz);
	for (unsigned int li = 0; li < lblkcnt; li++) {
		l.l_phys = (zap_leaf_phys_t *)(leafblks + li * blksz);
		zap_fat_write_leaf_init(&l, li, prefixlen);
	}
	zaphdr->zap_num_leafs = lblkcnt;
	zaphdr->zap_freeblk = lblkcnt + 1;

	/*
	 * For each entry, figure out which leaf block it belongs to based on
	 * the upper bits of its hash, allocate chunks from that leaf, and fill
	 * them out.
	 */
	ptrhasht = (uint64_t *)(&zfs->filebuf[0] + blksz / 2);
	STAILQ_FOREACH(ent, &zap->kvps, next) {
		struct zap_leaf_entry *le;
		uint16_t *lptr;
		uint64_t hi, li;
		uint16_t namelen, nchunks, nnamechunks, nvalchunks;

		hi = ZAP_HASH_IDX(ent->hash, zt->zt_shift);
		li = ZAP_HASH_IDX(ent->hash, prefixlen);
		assert(ptrhasht[hi] == 0 || ptrhasht[hi] == li + 1);
		ptrhasht[hi] = li + 1;
		l.l_phys = (zap_leaf_phys_t *)(leafblks + li * blksz);

		namelen = strlen(ent->name) + 1;

		/*
		 * How many leaf chunks do we need for this entry?
		 */
		nnamechunks = howmany(namelen, ZAP_LEAF_ARRAY_BYTES);
		nvalchunks = howmany(ent->intcnt,
		    ZAP_LEAF_ARRAY_BYTES / ent->intsz);
		nchunks = 1 + nnamechunks + nvalchunks;

		/*
		 * Allocate a run of free leaf chunks for this entry,
		 * potentially extending a hash chain.
		 */
		assert(l.l_phys->l_hdr.lh_nfree >= nchunks);
		l.l_phys->l_hdr.lh_nfree -= nchunks;
		l.l_phys->l_hdr.lh_nentries++;
		lptr = ZAP_LEAF_HASH_ENTPTR(&l, ent->hash);
		while (*lptr != 0xffff) {
			assert(*lptr < ZAP_LEAF_NUMCHUNKS(&l));
			le = ZAP_LEAF_ENTRY(&l, *lptr);
			assert(le->le_type == ZAP_CHUNK_ENTRY);
			le->le_cd++;
			lptr = &le->le_next;
		}
		*lptr = l.l_phys->l_hdr.lh_freelist;
		l.l_phys->l_hdr.lh_freelist += nchunks;
		assert(l.l_phys->l_hdr.lh_freelist <=
		    ZAP_LEAF_NUMCHUNKS(&l));
		if (l.l_phys->l_hdr.lh_freelist ==
		    ZAP_LEAF_NUMCHUNKS(&l))
			l.l_phys->l_hdr.lh_freelist = 0xffff;

		/*
		 * Integer values must be stored in big-endian format.
		 */
		switch (ent->intsz) {
		case 1:
			break;
		case 2:
			for (uint16_t *v = ent->val16p;
			    v - ent->val16p < (ptrdiff_t)ent->intcnt;
			    v++)
				*v = htobe16(*v);
			break;
		case 4:
			for (uint32_t *v = ent->val32p;
			    v - ent->val32p < (ptrdiff_t)ent->intcnt;
			    v++)
				*v = htobe32(*v);
			break;
		case 8:
			for (uint64_t *v = ent->val64p;
			    v - ent->val64p < (ptrdiff_t)ent->intcnt;
			    v++)
				*v = htobe64(*v);
			break;
		default:
			assert(0);
		}

		/*
		 * Finally, write out the leaf chunks for this entry.
		 */
		le = ZAP_LEAF_ENTRY(&l, *lptr);
		assert(le->le_type == ZAP_CHUNK_FREE);
		le->le_type = ZAP_CHUNK_ENTRY;
		le->le_next = 0xffff;
		le->le_name_chunk = *lptr + 1;
		le->le_name_numints = namelen;
		le->le_value_chunk = *lptr + 1 + nnamechunks;
		le->le_value_intlen = ent->intsz;
		le->le_value_numints = ent->intcnt;
		le->le_hash = ent->hash;
		zap_fat_write_array_chunk(&l, *lptr + 1, namelen, ent->name);
		zap_fat_write_array_chunk(&l, *lptr + 1 + nnamechunks,
		    ent->intcnt * ent->intsz, ent->valp);
	}

	/*
	 * Initialize unused slots of the pointer table.
	 */
	for (int i = 0; i < ptrcnt; i++)
		if (ptrhasht[i] == 0)
			ptrhasht[i] = (i >> (zt->zt_shift - prefixlen)) + 1;

	/*
	 * Write the whole thing to disk.
	 */
	dnode = zap->dnode;
	dnode->dn_nblkptr = 1;
	dnode->dn_datablkszsec = blksz >> MINBLOCKSHIFT;
	dnode->dn_maxblkid = lblkcnt + 1;
	dnode->dn_flags = DNODE_FLAG_USED_BYTES;

	c = dnode_cursor_init(zfs, zap->os, zap->dnode,
	    (lblkcnt + 1) * blksz, blksz);

	loc = objset_space_alloc(zfs, zap->os, &blksz);
	vdev_pwrite_dnode_indir(zfs, dnode, 0, 1, zfs->filebuf, blksz, loc,
	    dnode_cursor_next(zfs, c, 0));

	for (uint64_t i = 0; i < lblkcnt; i++) {
		loc = objset_space_alloc(zfs, zap->os, &blksz);
		vdev_pwrite_dnode_indir(zfs, dnode, 0, 1, leafblks + i * blksz,
		    blksz, loc, dnode_cursor_next(zfs, c, (i + 1) * blksz));
	}

	dnode_cursor_finish(zfs, c);

	free(leafblks);
}

void
zap_write(zfs_opt_t *zfs, zfs_zap_t *zap)
{
	zfs_zap_entry_t *ent;

	if (zap->micro) {
		zap_micro_write(zfs, zap);
	} else {
		assert(!STAILQ_EMPTY(&zap->kvps));
		assert(zap->kvpcnt > 0);
		zap_fat_write(zfs, zap);
	}

	while ((ent = STAILQ_FIRST(&zap->kvps)) != NULL) {
		STAILQ_REMOVE_HEAD(&zap->kvps, next);
		if (ent->val64p != &ent->val64)
			free(ent->valp);
		free(ent->name);
		free(ent);
	}
	free(zap);
}
