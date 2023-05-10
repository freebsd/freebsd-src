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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <util.h>

#include "zfs.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "zfs/fletcher.c"
#include "zfs/sha256.c"
#pragma GCC diagnostic pop

static void
blkptr_set(blkptr_t *bp, off_t off, off_t size, uint8_t dntype, uint8_t level,
    uint64_t fill, enum zio_checksum cksumt, zio_cksum_t *cksum)
{
	dva_t *dva;

	assert(powerof2(size));

	BP_ZERO(bp);
	BP_SET_LSIZE(bp, size);
	BP_SET_PSIZE(bp, size);
	BP_SET_CHECKSUM(bp, cksumt);
	BP_SET_COMPRESS(bp, ZIO_COMPRESS_OFF);
	BP_SET_BYTEORDER(bp, ZFS_HOST_BYTEORDER);
	BP_SET_BIRTH(bp, TXG, TXG);
	BP_SET_LEVEL(bp, level);
	BP_SET_FILL(bp, fill);
	BP_SET_TYPE(bp, dntype);

	dva = BP_IDENTITY(bp);
	DVA_SET_VDEV(dva, 0);
	DVA_SET_OFFSET(dva, off);
	DVA_SET_ASIZE(dva, size);
	memcpy(&bp->blk_cksum, cksum, sizeof(*cksum));
}

/*
 * Write a block of data to the vdev.  The offset is always relative to the end
 * of the second leading vdev label.
 *
 * Consumers should generally use the helpers below, which provide block
 * pointers and update dnode accounting, rather than calling this function
 * directly.
 */
static void
vdev_pwrite(const zfs_opt_t *zfs, const void *buf, size_t len, off_t off)
{
	ssize_t n;

	assert(off >= 0 && off < zfs->asize);
	assert(powerof2(len));
	assert((off_t)len > 0 && off + (off_t)len > off &&
	    off + (off_t)len < zfs->asize);
	if (zfs->spacemap != NULL) {
		/*
		 * Verify that the blocks being written were in fact allocated.
		 *
		 * The space map isn't available once the on-disk space map is
		 * finalized, so this check doesn't quite catch everything.
		 */
		assert(bit_ntest(zfs->spacemap, off >> zfs->ashift,
		    (off + len - 1) >> zfs->ashift, 1));
	}

	off += VDEV_LABEL_START_SIZE;
	for (size_t sofar = 0; sofar < len; sofar += n) {
		n = pwrite(zfs->fd, (const char *)buf + sofar, len - sofar,
		    off + sofar);
		if (n < 0)
			err(1, "pwrite");
		assert(n > 0);
	}
}

void
vdev_pwrite_data(zfs_opt_t *zfs, uint8_t datatype, uint8_t cksumtype,
    uint8_t level, uint64_t fill, const void *data, off_t sz, off_t loc,
    blkptr_t *bp)
{
	zio_cksum_t cksum;

	assert(cksumtype == ZIO_CHECKSUM_FLETCHER_4);

	fletcher_4_native(data, sz, NULL, &cksum);
	blkptr_set(bp, loc, sz, datatype, level, fill, cksumtype, &cksum);
	vdev_pwrite(zfs, data, sz, loc);
}

void
vdev_pwrite_dnode_indir(zfs_opt_t *zfs, dnode_phys_t *dnode, uint8_t level,
    uint64_t fill, const void *data, off_t sz, off_t loc, blkptr_t *bp)
{
	vdev_pwrite_data(zfs, dnode->dn_type, dnode->dn_checksum, level, fill,
	    data, sz, loc, bp);

	assert((dnode->dn_flags & DNODE_FLAG_USED_BYTES) != 0);
	dnode->dn_used += sz;
}

void
vdev_pwrite_dnode_data(zfs_opt_t *zfs, dnode_phys_t *dnode, const void *data,
    off_t sz, off_t loc)
{
	vdev_pwrite_dnode_indir(zfs, dnode, 0, 1, data, sz, loc,
	    &dnode->dn_blkptr[0]);
}

static void
vdev_label_set_checksum(void *buf, off_t off, off_t size)
{
	zio_cksum_t cksum;
	zio_eck_t *eck;

	assert(size > 0 && (size_t)size >= sizeof(zio_eck_t));

	eck = (zio_eck_t *)((char *)buf + size) - 1;
	eck->zec_magic = ZEC_MAGIC;
	ZIO_SET_CHECKSUM(&eck->zec_cksum, off, 0, 0, 0);
	zio_checksum_SHA256(buf, size, NULL, &cksum);
	eck->zec_cksum = cksum;
}

/*
 * Set embedded checksums and write the label at the specified index.
 */
void
vdev_label_write(zfs_opt_t *zfs, int ind, const vdev_label_t *labelp)
{
	vdev_label_t *label;
	ssize_t n;
	off_t blksz, loff;

	assert(ind >= 0 && ind < VDEV_LABELS);

	/*
	 * Make a copy since we have to modify the label to set checksums.
	 */
	label = ecalloc(1, sizeof(*label));
	memcpy(label, labelp, sizeof(*label));

	if (ind < 2)
		loff = ind * sizeof(*label);
	else
		loff = zfs->vdevsize - (VDEV_LABELS - ind) * sizeof(*label);

	/*
	 * Set the verifier checksum for the boot block.  We don't use it, but
	 * the FreeBSD loader reads it and will complain if the checksum isn't
	 * valid.
	 */
	vdev_label_set_checksum(&label->vl_be,
	    loff + __offsetof(vdev_label_t, vl_be), sizeof(label->vl_be));

	/*
	 * Set the verifier checksum for the label.
	 */
	vdev_label_set_checksum(&label->vl_vdev_phys,
	    loff + __offsetof(vdev_label_t, vl_vdev_phys),
	    sizeof(label->vl_vdev_phys));

	/*
	 * Set the verifier checksum for the uberblocks.  There is one uberblock
	 * per sector; for example, with an ashift of 12 we end up with
	 * 128KB/4KB=32 copies of the uberblock in the ring.
	 */
	blksz = 1 << zfs->ashift;
	assert(sizeof(label->vl_uberblock) % blksz == 0);
	for (size_t roff = 0; roff < sizeof(label->vl_uberblock);
	    roff += blksz) {
		vdev_label_set_checksum(&label->vl_uberblock[0] + roff,
		    loff + __offsetof(vdev_label_t, vl_uberblock) + roff,
		    blksz);
	}

	n = pwrite(zfs->fd, label, sizeof(*label), loff);
	if (n < 0)
		err(1, "writing vdev label");
	assert(n == sizeof(*label));

	free(label);
}

/*
 * Find a chunk of contiguous free space of length *lenp, according to the
 * following rules:
 * 1. If the length is less than or equal to 128KB, the returned run's length
 *    will be the smallest power of 2 equal to or larger than the length.
 * 2. If the length is larger than 128KB, the returned run's length will be
 *    the smallest multiple of 128KB that is larger than the length.
 * 3. The returned run's length will be size-aligned up to 128KB.
 *
 * XXX-MJ the third rule isn't actually required, so this can just be a dumb
 * bump allocator.  Maybe there's some benefit to keeping large blocks aligned,
 * so let's keep it for now and hope we don't get too much fragmentation.
 * Alternately we could try to allocate all blocks of a certain size from the
 * same metaslab.
 */
off_t
vdev_space_alloc(zfs_opt_t *zfs, off_t *lenp)
{
	off_t len;
	int align, loc, minblksz, nbits;

	minblksz = 1 << zfs->ashift;
	len = roundup2(*lenp, minblksz);

	assert(len != 0);
	assert(len / minblksz <= INT_MAX);

	if (len < MAXBLOCKSIZE) {
		if ((len & (len - 1)) != 0)
			len = (off_t)1 << flsll(len);
		align = len / minblksz;
	} else {
		len = roundup2(len, MAXBLOCKSIZE);
		align = MAXBLOCKSIZE / minblksz;
	}

	for (loc = 0, nbits = len / minblksz;; loc = roundup2(loc, align)) {
		bit_ffc_area_at(zfs->spacemap, loc, zfs->spacemapbits, nbits,
		    &loc);
		if (loc == -1) {
			errx(1, "failed to find %ju bytes of space",
			    (uintmax_t)len);
		}
		if ((loc & (align - 1)) == 0)
			break;
	}
	assert(loc + nbits > loc);
	bit_nset(zfs->spacemap, loc, loc + nbits - 1);
	*lenp = len;

	return ((off_t)loc << zfs->ashift);
}

static void
vdev_spacemap_init(zfs_opt_t *zfs)
{
	uint64_t nbits;

	assert(powerof2(zfs->mssize));

	nbits = rounddown2(zfs->asize, zfs->mssize) >> zfs->ashift;
	if (nbits > INT_MAX) {
		/*
		 * With the smallest block size of 512B, the limit on the image
		 * size is 2TB.  That should be enough for anyone.
		 */
		errx(1, "image size is too large");
	}
	zfs->spacemapbits = (int)nbits;
	zfs->spacemap = bit_alloc(zfs->spacemapbits);
	if (zfs->spacemap == NULL)
		err(1, "bitstring allocation failed");
}

void
vdev_spacemap_write(zfs_opt_t *zfs)
{
	dnode_phys_t *objarr;
	bitstr_t *spacemap;
	uint64_t *objarrblk;
	off_t smblksz, objarrblksz, objarrloc;

	struct {
		dnode_phys_t	*dnode;
		uint64_t	dnid;
		off_t		loc;
	} *sma;

	objarrblksz = sizeof(uint64_t) * zfs->mscount;
	assert(objarrblksz <= MAXBLOCKSIZE);
	objarrloc = objset_space_alloc(zfs, zfs->mos, &objarrblksz);
	objarrblk = ecalloc(1, objarrblksz);

	objarr = objset_dnode_lookup(zfs->mos, zfs->objarrid);
	objarr->dn_datablkszsec = objarrblksz >> MINBLOCKSHIFT;

	/*
	 * Use the smallest block size for space maps.  The space allocation
	 * algorithm should aim to minimize the number of holes.
	 */
	smblksz = 1 << zfs->ashift;

	/*
	 * First allocate dnodes and space for all of our space maps.  No more
	 * space can be allocated from the vdev after this point.
	 */
	sma = ecalloc(zfs->mscount, sizeof(*sma));
	for (uint64_t i = 0; i < zfs->mscount; i++) {
		sma[i].dnode = objset_dnode_bonus_alloc(zfs->mos,
		    DMU_OT_SPACE_MAP, DMU_OT_SPACE_MAP_HEADER,
		    sizeof(space_map_phys_t), &sma[i].dnid);
		sma[i].loc = objset_space_alloc(zfs, zfs->mos, &smblksz);
	}
	spacemap = zfs->spacemap;
	zfs->spacemap = NULL;

	/*
	 * Now that the set of allocated space is finalized, populate each space
	 * map and write it to the vdev.
	 */
	for (uint64_t i = 0; i < zfs->mscount; i++) {
		space_map_phys_t *sm;
		uint64_t alloc, length, *smblk;
		int shift, startb, endb, srunb, erunb;

		/*
		 * We only allocate a single block for this space map, but
		 * OpenZFS assumes that a space map object with sufficient bonus
		 * space supports histograms.
		 */
		sma[i].dnode->dn_nblkptr = 3;
		sma[i].dnode->dn_datablkszsec = smblksz >> MINBLOCKSHIFT;

		smblk = ecalloc(1, smblksz);

		alloc = length = 0;
		shift = zfs->msshift - zfs->ashift;
		for (srunb = startb = i * (1 << shift),
		    endb = (i + 1) * (1 << shift);
		    srunb < endb; srunb = erunb) {
			uint64_t runlen, runoff;

			/* Find a run of allocated space. */
			bit_ffs_at(spacemap, srunb, zfs->spacemapbits, &srunb);
			if (srunb == -1 || srunb >= endb)
				break;

			bit_ffc_at(spacemap, srunb, zfs->spacemapbits, &erunb);
			if (erunb == -1 || erunb > endb)
				erunb = endb;

			/*
			 * The space represented by [srunb, erunb) has been
			 * allocated.  Add a record to the space map to indicate
			 * this.  Run offsets are relative to the beginning of
			 * the metaslab.
			 */
			runlen = erunb - srunb;
			runoff = srunb - startb;

			assert(length * sizeof(uint64_t) < (uint64_t)smblksz);
			smblk[length] = SM_PREFIX_ENCODE(SM2_PREFIX) |
			    SM2_RUN_ENCODE(runlen) | SM2_VDEV_ENCODE(0);
			smblk[length + 1] = SM2_TYPE_ENCODE(SM_ALLOC) |
			    SM2_OFFSET_ENCODE(runoff);

			alloc += runlen << zfs->ashift;
			length += 2;
		}

		sm = DN_BONUS(sma[i].dnode);
		sm->smp_length = length * sizeof(uint64_t);
		sm->smp_alloc = alloc;

		vdev_pwrite_dnode_data(zfs, sma[i].dnode, smblk, smblksz,
		    sma[i].loc);
		free(smblk);

		/* Record this space map in the space map object array. */
		objarrblk[i] = sma[i].dnid;
	}

	/*
	 * All of the space maps are written, now write the object array.
	 */
	vdev_pwrite_dnode_data(zfs, objarr, objarrblk, objarrblksz, objarrloc);
	free(objarrblk);

	assert(zfs->spacemap == NULL);
	free(spacemap);
	free(sma);
}

void
vdev_init(zfs_opt_t *zfs, const char *image)
{
	assert(zfs->ashift >= MINBLOCKSHIFT);

	zfs->fd = open(image, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (zfs->fd == -1)
		err(1, "Can't open `%s' for writing", image);
	if (ftruncate(zfs->fd, zfs->vdevsize) != 0)
		err(1, "Failed to extend image file `%s'", image);

	vdev_spacemap_init(zfs);
}

void
vdev_fini(zfs_opt_t *zfs)
{
	assert(zfs->spacemap == NULL);

	if (zfs->fd != -1) {
		if (close(zfs->fd) != 0)
			err(1, "close");
		zfs->fd = -1;
	}
}
