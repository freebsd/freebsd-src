/*-
 * Copyright (c) 2007 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $DragonFly: src/sys/vfs/hammer/hammer_disk.h,v 1.55 2008/11/13 02:18:43 dillon Exp $
 */

#ifndef VFS_HAMMER_DISK_H_
#define VFS_HAMMER_DISK_H_

#include <sys/endian.h>

#ifndef _SYS_UUID_H_
#include <sys/uuid.h>
#endif

/*
 * The structures below represent the on-disk format for a HAMMER
 * filesystem.  Note that all fields for on-disk structures are naturally
 * aligned.  HAMMER uses little endian for fields in on-disk structures.
 * HAMMER doesn't support big endian arch, but is planned.
 *
 * Most of HAMMER revolves around the concept of an object identifier.  An
 * obj_id is a 64 bit quantity which uniquely identifies a filesystem object
 * FOR THE ENTIRE LIFE OF THE FILESYSTEM.  This uniqueness allows backups
 * and mirrors to retain varying amounts of filesystem history by removing
 * any possibility of conflict through identifier reuse.
 *
 * A HAMMER filesystem may span multiple volumes.
 *
 * A HAMMER filesystem uses a 16K filesystem buffer size.  All filesystem
 * I/O is done in multiples of 16K.
 *
 * 64K X-bufs are used for blocks >= a file's 1MB mark.
 *
 * Per-volume storage limit: 52 bits		4096 TB
 * Per-Zone storage limit: 60 bits		1 MTB
 * Per-filesystem storage limit: 60 bits	1 MTB
 */
#define HAMMER_BUFSIZE		16384
#define HAMMER_XBUFSIZE		65536
#define HAMMER_HBUFSIZE		(HAMMER_BUFSIZE / 2)
#define HAMMER_XDEMARC		(1024 * 1024)
#define HAMMER_BUFMASK		(HAMMER_BUFSIZE - 1)
#define HAMMER_XBUFMASK		(HAMMER_XBUFSIZE - 1)

#define HAMMER_BUFSIZE64	((uint64_t)HAMMER_BUFSIZE)
#define HAMMER_BUFMASK64	((uint64_t)HAMMER_BUFMASK)

#define HAMMER_XBUFSIZE64	((uint64_t)HAMMER_XBUFSIZE)
#define HAMMER_XBUFMASK64	((uint64_t)HAMMER_XBUFMASK)

#define HAMMER_OFF_ZONE_MASK	0xF000000000000000ULL /* zone portion */
#define HAMMER_OFF_VOL_MASK	0x0FF0000000000000ULL /* volume portion */
#define HAMMER_OFF_SHORT_MASK	0x000FFFFFFFFFFFFFULL /* offset portion */
#define HAMMER_OFF_LONG_MASK	0x0FFFFFFFFFFFFFFFULL /* offset portion */

#define HAMMER_OFF_BAD		((hammer_off_t)-1)

#define HAMMER_BUFSIZE_DOALIGN(offset)				\
	(((offset) + HAMMER_BUFMASK) & ~HAMMER_BUFMASK)
#define HAMMER_BUFSIZE64_DOALIGN(offset)			\
	(((offset) + HAMMER_BUFMASK64) & ~HAMMER_BUFMASK64)

#define HAMMER_XBUFSIZE_DOALIGN(offset)				\
	(((offset) + HAMMER_XBUFMASK) & ~HAMMER_XBUFMASK)
#define HAMMER_XBUFSIZE64_DOALIGN(offset)			\
	(((offset) + HAMMER_XBUFMASK64) & ~HAMMER_XBUFMASK64)

/*
 * The current limit of volumes that can make up a HAMMER FS
 */
#define HAMMER_MAX_VOLUMES	256

/*
 * Reserved space for (future) header junk after the volume header.
 */
#define HAMMER_MIN_VOL_JUNK	(HAMMER_BUFSIZE * 16)	/* 256 KB */
#define HAMMER_MAX_VOL_JUNK	HAMMER_MIN_VOL_JUNK
#define HAMMER_VOL_JUNK_SIZE	HAMMER_MIN_VOL_JUNK

/*
 * Hammer transaction ids are 64 bit unsigned integers and are usually
 * synchronized with the time of day in nanoseconds.
 *
 * Hammer offsets are used for FIFO indexing and embed a cycle counter
 * and volume number in addition to the offset.  Most offsets are required
 * to be 16 KB aligned.
 */
typedef uint64_t hammer_tid_t;
typedef uint64_t hammer_off_t;
typedef uint32_t hammer_crc_t;
typedef uuid_t hammer_uuid_t;

#define HAMMER_MIN_TID		0ULL			/* unsigned */
#define HAMMER_MAX_TID		0xFFFFFFFFFFFFFFFFULL	/* unsigned */
#define HAMMER_MIN_KEY		-0x8000000000000000LL	/* signed */
#define HAMMER_MAX_KEY		0x7FFFFFFFFFFFFFFFLL	/* signed */
#define HAMMER_MIN_OBJID	HAMMER_MIN_KEY		/* signed */
#define HAMMER_MAX_OBJID	HAMMER_MAX_KEY		/* signed */
#define HAMMER_MIN_RECTYPE	0x0U			/* unsigned */
#define HAMMER_MAX_RECTYPE	0xFFFFU			/* unsigned */
#define HAMMER_MIN_OFFSET	0ULL			/* unsigned */
#define HAMMER_MAX_OFFSET	0xFFFFFFFFFFFFFFFFULL	/* unsigned */

/*
 * hammer_off_t has several different encodings.  Note that not all zones
 * encode a vol_no.  Zone bits are not a part of filesystem capacity as
 * the zone bits aren't directly or indirectly mapped to physical volumes.
 *
 * In other words, HAMMER's logical filesystem offset consists of 64 bits,
 * but the filesystem is considered 60 bits filesystem, not 64 bits.
 * The maximum filesystem capacity is 1EB, not 16EB.
 *
 * zone 0:		available, a big-block that contains the offset is unused
 * zone 1 (z,v,o):	raw volume relative (offset 0 is the volume header)
 * zone 2 (z,v,o):	raw buffer relative (offset 0 is the first buffer)
 * zone 3 (z,o):	undo/redo fifo	- fixed zone-2 offset array in volume header
 * zone 4 (z,v,o):	freemap		- only real blockmap
 * zone 8 (z,v,o):	B-Tree		- actually zone-2 address
 * zone 9 (z,v,o):	meta		- actually zone-2 address
 * zone 10 (z,v,o):	large-data	- actually zone-2 address
 * zone 11 (z,v,o):	small-data	- actually zone-2 address
 * zone 15:		unavailable, usually the offset is beyond volume size
 *
 * layer1/layer2 direct map:
 *	     Maximum HAMMER filesystem capacity from volume aspect
 *	     2^8(max volumes) * 2^52(max volume size) = 2^60 = 1EB (long offset)
 *	    <------------------------------------------------------------->
 *	     8bits   52bits (short offset)
 *	    <------><----------------------------------------------------->
 *	zzzzvvvvvvvvoooo oooooooooooooooo oooooooooooooooo oooooooooooooooo
 *	----111111111111 1111112222222222 222222222ooooooo oooooooooooooooo
 *	    <-----------------><------------------><---------------------->
 *	     18bits             19bits              23bits
 *	    <------------------------------------------------------------->
 *	     2^18(layer1) * 2^19(layer2) * 2^23(big-block) = 2^60 = 1EB
 *	     Maximum HAMMER filesystem capacity from blockmap aspect
 *
 * volume#0 layout
 *	+-------------------------> offset 0 of a device/partition
 *	| volume header (1928 bytes)
 *	| the rest of header junk space (HAMMER_BUFSIZE aligned)
 *	+-------------------------> vol_bot_beg
 *	| boot area (HAMMER_BUFSIZE aligned)
 *	+-------------------------> vol_mem_beg
 *	| memory log (HAMMER_BUFSIZE aligned)
 *	+-------------------------> vol_buf_beg (physical offset of zone-2)
 *	| zone-4 big-block for layer1
 *	+-------------------------> vol_buf_beg + HAMMER_BIGBLOCK_SIZE
 *	| zone-4 big-blocks for layer2
 *	| ... (1 big-block per 4TB space)
 *	+-------------------------> vol_buf_beg + HAMMER_BIGBLOCK_SIZE * ...
 *	| zone-3 big-blocks for UNDO/REDO FIFO
 *	| ... (max 128 big-blocks)
 *	+-------------------------> vol_buf_beg + HAMMER_BIGBLOCK_SIZE * ...
 *	| zone-8 big-block for root B-Tree node/etc
 *	+-------------------------> vol_buf_beg + HAMMER_BIGBLOCK_SIZE * ...
 *	| zone-9 big-block for root inode/PFS/etc
 *	+-------------------------> vol_buf_beg + HAMMER_BIGBLOCK_SIZE * ...
 *	| zone-X big-blocks
 *	| ... (big-blocks for new zones after newfs_hammer)
 *	| ...
 *	| ...
 *	| ...
 *	| ...
 *	+-------------------------> vol_buf_end (HAMMER_BUFSIZE aligned)
 *	+-------------------------> end of a device/partition
 *
 * volume#N layout (0<N<256)
 *	+-------------------------> offset 0 of a device/partition
 *	| volume header (1928 bytes)
 *	| the rest of header junk space (HAMMER_BUFSIZE aligned)
 *	+-------------------------> vol_bot_beg
 *	| boot area (HAMMER_BUFSIZE aligned)
 *	+-------------------------> vol_mem_beg
 *	| memory log (HAMMER_BUFSIZE aligned)
 *	+-------------------------> vol_buf_beg (physical offset of zone-2)
 *	| zone-4 big-blocks for layer2
 *	| ... (1 big-block per 4TB space)
 *	+-------------------------> vol_buf_beg + HAMMER_BIGBLOCK_SIZE * ...
 *	| zone-X big-blocks
 *	| ... (unused until volume#(N-1) runs out of space)
 *	| ...
 *	| ...
 *	| ...
 *	| ...
 *	+-------------------------> vol_buf_end (HAMMER_BUFSIZE aligned)
 *	+-------------------------> end of a device/partition
 */

#define HAMMER_ZONE_RAW_VOLUME		0x1000000000000000ULL
#define HAMMER_ZONE_RAW_BUFFER		0x2000000000000000ULL
#define HAMMER_ZONE_UNDO		0x3000000000000000ULL
#define HAMMER_ZONE_FREEMAP		0x4000000000000000ULL
#define HAMMER_ZONE_RESERVED05		0x5000000000000000ULL  /* not used */
#define HAMMER_ZONE_RESERVED06		0x6000000000000000ULL  /* not used */
#define HAMMER_ZONE_RESERVED07		0x7000000000000000ULL  /* not used */
#define HAMMER_ZONE_BTREE		0x8000000000000000ULL
#define HAMMER_ZONE_META		0x9000000000000000ULL
#define HAMMER_ZONE_LARGE_DATA		0xA000000000000000ULL
#define HAMMER_ZONE_SMALL_DATA		0xB000000000000000ULL
#define HAMMER_ZONE_RESERVED0C		0xC000000000000000ULL  /* not used */
#define HAMMER_ZONE_RESERVED0D		0xD000000000000000ULL  /* not used */
#define HAMMER_ZONE_RESERVED0E		0xE000000000000000ULL  /* not used */
#define HAMMER_ZONE_UNAVAIL		0xF000000000000000ULL

#define HAMMER_ZONE_RAW_VOLUME_INDEX	1
#define HAMMER_ZONE_RAW_BUFFER_INDEX	2
#define HAMMER_ZONE_UNDO_INDEX		3
#define HAMMER_ZONE_FREEMAP_INDEX	4
#define HAMMER_ZONE_BTREE_INDEX		8
#define HAMMER_ZONE_META_INDEX		9
#define HAMMER_ZONE_LARGE_DATA_INDEX	10
#define HAMMER_ZONE_SMALL_DATA_INDEX	11
#define HAMMER_ZONE_UNAVAIL_INDEX	15

#define HAMMER_MAX_ZONES		16

#define HAMMER_ZONE(offset)		((offset) & HAMMER_OFF_ZONE_MASK)

#define hammer_is_zone_raw_volume(offset)		\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_RAW_VOLUME)
#define hammer_is_zone_raw_buffer(offset)		\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_RAW_BUFFER)
#define hammer_is_zone_undo(offset)			\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_UNDO)
#define hammer_is_zone_freemap(offset)			\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_FREEMAP)
#define hammer_is_zone_btree(offset)			\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_BTREE)
#define hammer_is_zone_meta(offset)			\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_META)
#define hammer_is_zone_large_data(offset)		\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_LARGE_DATA)
#define hammer_is_zone_small_data(offset)		\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_SMALL_DATA)
#define hammer_is_zone_unavail(offset)			\
	(HAMMER_ZONE(offset) == HAMMER_ZONE_UNAVAIL)
#define hammer_is_zone_data(offset)			\
	(hammer_is_zone_large_data(offset) || hammer_is_zone_small_data(offset))

#define hammer_is_index_record(zone)			\
	((zone) >= HAMMER_ZONE_BTREE_INDEX &&		\
	 (zone) < HAMMER_MAX_ZONES)

#define hammer_is_zone_record(offset)			\
	hammer_is_index_record(HAMMER_ZONE_DECODE(offset))

#define hammer_is_index_direct_xlated(zone)		\
	(((zone) == HAMMER_ZONE_RAW_BUFFER_INDEX) ||	\
	 ((zone) == HAMMER_ZONE_FREEMAP_INDEX) ||	\
	 hammer_is_index_record(zone))

#define hammer_is_zone_direct_xlated(offset)		\
	hammer_is_index_direct_xlated(HAMMER_ZONE_DECODE(offset))

#define HAMMER_ZONE_ENCODE(zone, ham_off)		\
	(((hammer_off_t)(zone) << 60) | (ham_off))
#define HAMMER_ZONE_DECODE(ham_off)			\
	((int)(((hammer_off_t)(ham_off) >> 60)))

#define HAMMER_VOL_ENCODE(vol_no)			\
	((hammer_off_t)((vol_no) & 255) << 52)
#define HAMMER_VOL_DECODE(ham_off)			\
	((int)(((hammer_off_t)(ham_off) >> 52) & 255))

#define HAMMER_OFF_SHORT_ENCODE(offset)			\
	((hammer_off_t)(offset) & HAMMER_OFF_SHORT_MASK)
#define HAMMER_OFF_LONG_ENCODE(offset)			\
	((hammer_off_t)(offset) & HAMMER_OFF_LONG_MASK)

#define HAMMER_ENCODE(zone, vol_no, offset)		\
	(((hammer_off_t)(zone) << 60) |			\
	HAMMER_VOL_ENCODE(vol_no) |			\
	HAMMER_OFF_SHORT_ENCODE(offset))
#define HAMMER_ENCODE_RAW_VOLUME(vol_no, offset)	\
	HAMMER_ENCODE(HAMMER_ZONE_RAW_VOLUME_INDEX, vol_no, offset)
#define HAMMER_ENCODE_RAW_BUFFER(vol_no, offset)	\
	HAMMER_ENCODE(HAMMER_ZONE_RAW_BUFFER_INDEX, vol_no, offset)
#define HAMMER_ENCODE_UNDO(offset)			\
	HAMMER_ENCODE(HAMMER_ZONE_UNDO_INDEX, HAMMER_ROOT_VOLNO, offset)
#define HAMMER_ENCODE_FREEMAP(vol_no, offset)		\
	HAMMER_ENCODE(HAMMER_ZONE_FREEMAP_INDEX, vol_no, offset)

/*
 * Translate a zone address to zone-X address.
 */
#define hammer_xlate_to_zoneX(zone, offset)		\
	HAMMER_ZONE_ENCODE((zone), (offset) & ~HAMMER_OFF_ZONE_MASK)
#define hammer_xlate_to_zone2(offset)			\
	hammer_xlate_to_zoneX(HAMMER_ZONE_RAW_BUFFER_INDEX, (offset))

#define hammer_data_zone(data_len)			\
	(((data_len) >= HAMMER_BUFSIZE) ?		\
	 HAMMER_ZONE_LARGE_DATA :			\
	 HAMMER_ZONE_SMALL_DATA)
#define hammer_data_zone_index(data_len)		\
	(((data_len) >= HAMMER_BUFSIZE) ?		\
	 HAMMER_ZONE_LARGE_DATA_INDEX :			\
	 HAMMER_ZONE_SMALL_DATA_INDEX)

/*
 * Big-Block backing store
 *
 * A blockmap is a two-level map which translates a blockmap-backed zone
 * offset into a raw zone 2 offset.  The layer 1 handles 18 bits and the
 * layer 2 handles 19 bits.  The 8M big-block size is 23 bits so two
 * layers gives us 18+19+23 = 60 bits of address space.
 *
 * When using hinting for a blockmap lookup, the hint is lost when the
 * scan leaves the HINTBLOCK, which is typically several BIGBLOCK's.
 * HINTBLOCK is a heuristic.
 */
#define HAMMER_HINTBLOCK_SIZE		(HAMMER_BIGBLOCK_SIZE * 4)
#define HAMMER_HINTBLOCK_MASK64		((uint64_t)HAMMER_HINTBLOCK_SIZE - 1)
#define HAMMER_BIGBLOCK_SIZE		(8192 * 1024)
#define HAMMER_BIGBLOCK_SIZE64		((uint64_t)HAMMER_BIGBLOCK_SIZE)
#define HAMMER_BIGBLOCK_MASK		(HAMMER_BIGBLOCK_SIZE - 1)
#define HAMMER_BIGBLOCK_MASK64		((uint64_t)HAMMER_BIGBLOCK_SIZE - 1)
#define HAMMER_BIGBLOCK_BITS		23
#if 0
#define HAMMER_BIGBLOCK_OVERFILL	(6144 * 1024)
#endif
#if (1 << HAMMER_BIGBLOCK_BITS) != HAMMER_BIGBLOCK_SIZE
#error "HAMMER_BIGBLOCK_BITS BROKEN"
#endif

#define HAMMER_BUFFERS_PER_BIGBLOCK			\
	(HAMMER_BIGBLOCK_SIZE / HAMMER_BUFSIZE)
#define HAMMER_BUFFERS_PER_BIGBLOCK_MASK		\
	(HAMMER_BUFFERS_PER_BIGBLOCK - 1)
#define HAMMER_BUFFERS_PER_BIGBLOCK_MASK64		\
	((hammer_off_t)HAMMER_BUFFERS_PER_BIGBLOCK_MASK)

#define HAMMER_BIGBLOCK_DOALIGN(offset)				\
	(((offset) + HAMMER_BIGBLOCK_MASK64) & ~HAMMER_BIGBLOCK_MASK64)

/*
 * Maximum number of mirrors operating in master mode (multi-master
 * clustering and mirroring). Note that HAMMER1 does not support
 * multi-master clustering as of 2015.
 */
#define HAMMER_MAX_MASTERS		16

/*
 * The blockmap is somewhat of a degenerate structure.  HAMMER only actually
 * uses it in its original incarnation to implement the freemap.
 *
 * zone:1	raw volume (no blockmap)
 * zone:2	raw buffer (no blockmap)
 * zone:3	undomap    (direct layer2 array in volume header)
 * zone:4	freemap    (the only real blockmap)
 * zone:8-15	zone id used to classify big-block only, address is actually
 *		a zone-2 address.
 */
typedef struct hammer_blockmap {
	hammer_off_t	phys_offset;  /* zone-2 offset only used by zone-4 */
	hammer_off_t	first_offset; /* zone-X offset only used by zone-3 */
	hammer_off_t	next_offset;  /* zone-X offset for allocation */
	hammer_off_t	alloc_offset; /* zone-X offset only used by zone-3 */
	uint32_t	reserved01;
	hammer_crc_t	entry_crc;
} *hammer_blockmap_t;

#define HAMMER_BLOCKMAP_CRCSIZE	\
	offsetof(struct hammer_blockmap, entry_crc)

/*
 * The blockmap is a 2-layer entity made up of big-blocks.  The first layer
 * contains 262144 32-byte entries (18 bits), the second layer contains
 * 524288 16-byte entries (19 bits), representing 8MB (23 bit) blockmaps.
 * 18+19+23 = 60 bits.  The top four bits are the zone id.
 *
 * Currently only the freemap utilizes both layers in all their glory.
 * All primary data/meta-data zones actually encode a zone-2 address
 * requiring no real blockmap translation.
 *
 * The freemap uses the upper 8 bits of layer-1 to identify the volume,
 * thus any space allocated via the freemap can be directly translated
 * to a zone:2 (or zone:8-15) address.
 *
 * zone-X blockmap offset: [zone:4][layer1:18][layer2:19][big-block:23]
 */

/*
 * 32 bytes layer1 entry for 8MB big-block.
 * A big-block can hold 2^23 / 2^5 = 2^18 layer1 entries,
 * which equals bits assigned for layer1 in zone-2 address.
 */
typedef struct hammer_blockmap_layer1 {
	hammer_off_t	blocks_free;	/* big-blocks free */
	hammer_off_t	phys_offset;	/* UNAVAIL or zone-2 */
	hammer_off_t	reserved01;
	hammer_crc_t	layer2_crc;	/* xor'd crc's of HAMMER_BLOCKSIZE */
					/* (not yet used) */
	hammer_crc_t	layer1_crc;	/* MUST BE LAST FIELD OF STRUCTURE*/
} *hammer_blockmap_layer1_t;

#define HAMMER_LAYER1_CRCSIZE	\
	offsetof(struct hammer_blockmap_layer1, layer1_crc)

/*
 * 16 bytes layer2 entry for 8MB big-blocks.
 * A big-block can hold 2^23 / 2^4 = 2^19 layer2 entries,
 * which equals bits assigned for layer2 in zone-2 address.
 *
 * NOTE: bytes_free is signed and can legally go negative if/when data
 *	 de-dup occurs.  This field will never go higher than
 *	 HAMMER_BIGBLOCK_SIZE.  If exactly HAMMER_BIGBLOCK_SIZE
 *	 the big-block is completely free.
 */
typedef struct hammer_blockmap_layer2 {
	uint8_t		zone;		/* typed allocation zone */
	uint8_t		reserved01;
	uint16_t	reserved02;
	uint32_t	append_off;	/* allocatable space index */
	int32_t		bytes_free;	/* bytes free within this big-block */
	hammer_crc_t	entry_crc;
} *hammer_blockmap_layer2_t;

#define HAMMER_LAYER2_CRCSIZE	\
	offsetof(struct hammer_blockmap_layer2, entry_crc)

#define HAMMER_BLOCKMAP_UNAVAIL	((hammer_off_t)-1LL)

#define HAMMER_BLOCKMAP_RADIX1	/* 2^18 = 262144 */	\
	((int)(HAMMER_BIGBLOCK_SIZE / sizeof(struct hammer_blockmap_layer1)))
#define HAMMER_BLOCKMAP_RADIX2	/* 2^19 = 524288 */	\
	((int)(HAMMER_BIGBLOCK_SIZE / sizeof(struct hammer_blockmap_layer2)))

#define HAMMER_BLOCKMAP_LAYER1	/* 2^(18+19+23) = 1EB */	\
	(HAMMER_BLOCKMAP_RADIX1 * HAMMER_BLOCKMAP_LAYER2)
#define HAMMER_BLOCKMAP_LAYER2	/* 2^(19+23) = 4TB */		\
	(HAMMER_BLOCKMAP_RADIX2 * HAMMER_BIGBLOCK_SIZE64)

#define HAMMER_BLOCKMAP_LAYER1_MASK	(HAMMER_BLOCKMAP_LAYER1 - 1)
#define HAMMER_BLOCKMAP_LAYER2_MASK	(HAMMER_BLOCKMAP_LAYER2 - 1)

#define HAMMER_BLOCKMAP_LAYER2_DOALIGN(offset)			\
	(((offset) + HAMMER_BLOCKMAP_LAYER2_MASK) &		\
	 ~HAMMER_BLOCKMAP_LAYER2_MASK)

/*
 * Index within layer1 or layer2 big-block for the entry representing
 * a zone-2 physical offset.
 */
#define HAMMER_BLOCKMAP_LAYER1_INDEX(zone2_offset)		\
	((int)(((zone2_offset) & HAMMER_BLOCKMAP_LAYER1_MASK) /	\
	 HAMMER_BLOCKMAP_LAYER2))

#define HAMMER_BLOCKMAP_LAYER2_INDEX(zone2_offset)		\
	((int)(((zone2_offset) & HAMMER_BLOCKMAP_LAYER2_MASK) /	\
	HAMMER_BIGBLOCK_SIZE64))

/*
 * Byte offset within layer1 or layer2 big-block for the entry representing
 * a zone-2 physical offset.  Multiply the index by sizeof(blockmap_layer).
 */
#define HAMMER_BLOCKMAP_LAYER1_OFFSET(zone2_offset)		\
	(HAMMER_BLOCKMAP_LAYER1_INDEX(zone2_offset) *		\
	 sizeof(struct hammer_blockmap_layer1))

#define HAMMER_BLOCKMAP_LAYER2_OFFSET(zone2_offset)		\
	(HAMMER_BLOCKMAP_LAYER2_INDEX(zone2_offset) *		\
	 sizeof(struct hammer_blockmap_layer2))

/*
 * Move on to offset 0 of the next layer1 or layer2.
 */
#define HAMMER_ZONE_LAYER1_NEXT_OFFSET(offset)			\
	(((offset) + HAMMER_BLOCKMAP_LAYER2) & ~HAMMER_BLOCKMAP_LAYER2_MASK)

#define HAMMER_ZONE_LAYER2_NEXT_OFFSET(offset)			\
	(((offset) + HAMMER_BIGBLOCK_SIZE) & ~HAMMER_BIGBLOCK_MASK64)

/*
 * HAMMER UNDO parameters.  The UNDO fifo is mapped directly in the volume
 * header with an array of zone-2 offsets.  A maximum of (128x8MB) = 1GB,
 * and minimum of (64x8MB) = 512MB may be reserved.  The size of the undo
 * fifo is usually set a newfs time.
 */
#define HAMMER_MIN_UNDO_BIGBLOCKS		64
#define HAMMER_MAX_UNDO_BIGBLOCKS		128

/*
 * All on-disk HAMMER structures which make up elements of the UNDO FIFO
 * contain a hammer_fifo_head and hammer_fifo_tail structure.  This structure
 * contains all the information required to validate the fifo element
 * and to scan the fifo in either direction.  The head is typically embedded
 * in higher level hammer on-disk structures while the tail is typically
 * out-of-band.  hdr_size is the size of the whole mess, including the tail.
 *
 * All undo structures are guaranteed to not cross a 16K filesystem
 * buffer boundary.  Most undo structures are fairly small.  Data spaces
 * are not immediately reused by HAMMER so file data is not usually recorded
 * as part of an UNDO.
 *
 * PAD elements are allowed to take up only 8 bytes of space as a special
 * case, containing only hdr_signature, hdr_type, and hdr_size fields,
 * and with the tail overloaded onto the head structure for 8 bytes total.
 *
 * Every undo record has a sequence number.  This number is unrelated to
 * transaction ids and instead collects the undo transactions associated
 * with a single atomic operation.  A larger transactional operation, such
 * as a remove(), may consist of several smaller atomic operations
 * representing raw meta-data operations.
 *
 *				HAMMER VERSION 4 CHANGES
 *
 * In HAMMER version 4 the undo structure alignment is reduced from 16384
 * to 512 bytes in order to ensure that each 512 byte sector begins with
 * a header.  The hdr_seq field in the header is a 32 bit sequence number
 * which allows the recovery code to detect missing sectors
 * without relying on the 32-bit crc and to definitively identify the current
 * undo sequence space without having to rely on information from the volume
 * header.  In addition, new REDO entries in the undo space are used to
 * record write, write/extend, and transaction id updates.
 *
 * The grand result is:
 *
 * (1) The volume header no longer needs to be synchronized for most
 *     flush and fsync operations.
 *
 * (2) Most fsync operations need only lay down REDO records
 *
 * (3) Data overwrite for nohistory operations covered by REDO records
 *     can be supported (instead of rolling a new block allocation),
 *     by rolling UNDO for the prior contents of the data.
 *
 *				HAMMER VERSION 5 CHANGES
 *
 * Hammer version 5 contains a minor adjustment making layer2's bytes_free
 * field signed, allowing dedup to push it into the negative domain.
 */
#define HAMMER_HEAD_ALIGN		8
#define HAMMER_HEAD_ALIGN_MASK		(HAMMER_HEAD_ALIGN - 1)
#define HAMMER_HEAD_DOALIGN(bytes)	\
	(((bytes) + HAMMER_HEAD_ALIGN_MASK) & ~HAMMER_HEAD_ALIGN_MASK)

#define HAMMER_UNDO_ALIGN		512
#define HAMMER_UNDO_ALIGN64		((uint64_t)512)
#define HAMMER_UNDO_MASK		(HAMMER_UNDO_ALIGN - 1)
#define HAMMER_UNDO_MASK64		(HAMMER_UNDO_ALIGN64 - 1)
#define HAMMER_UNDO_DOALIGN(offset)	\
	(((offset) + HAMMER_UNDO_MASK) & ~HAMMER_UNDO_MASK64)

typedef struct hammer_fifo_head {
	uint16_t hdr_signature;
	uint16_t hdr_type;
	uint32_t hdr_size;	/* Aligned size of the whole mess */
	uint32_t hdr_seq;	/* Sequence number */
	hammer_crc_t hdr_crc;	/* XOR crc up to field w/ crc after field */
} *hammer_fifo_head_t;

#define HAMMER_FIFO_HEAD_CRCOFF	offsetof(struct hammer_fifo_head, hdr_crc)

typedef struct hammer_fifo_tail {
	uint16_t tail_signature;
	uint16_t tail_type;
	uint32_t tail_size;	/* aligned size of the whole mess */
} *hammer_fifo_tail_t;

/*
 * Fifo header types.
 *
 * NOTE: 0x8000U part of HAMMER_HEAD_TYPE_PAD can be removed if the HAMMER
 * version ever gets bumped again. It exists only to keep compatibility with
 * older versions.
 */
#define HAMMER_HEAD_TYPE_PAD	(0x0040U | 0x8000U)
#define HAMMER_HEAD_TYPE_DUMMY	0x0041U		/* dummy entry w/seqno */
#define HAMMER_HEAD_TYPE_UNDO	0x0043U		/* random UNDO information */
#define HAMMER_HEAD_TYPE_REDO	0x0044U		/* data REDO / fast fsync */

#define HAMMER_HEAD_SIGNATURE	0xC84EU
#define HAMMER_TAIL_SIGNATURE	0xC74FU

/*
 * Misc FIFO structures.
 *
 * UNDO - Raw meta-data media updates.
 */
typedef struct hammer_fifo_undo {
	struct hammer_fifo_head	head;
	hammer_off_t		undo_offset;	/* zone-1,2 offset */
	int32_t			undo_data_bytes;
	int32_t			undo_reserved01;
	/* followed by data */
} *hammer_fifo_undo_t;

/*
 * REDO (HAMMER version 4+) - Logical file writes/truncates.
 *
 * REDOs contain information which will be duplicated in a later meta-data
 * update, allowing fast write()+fsync() operations.  REDOs can be ignored
 * without harming filesystem integrity but must be processed if fsync()
 * semantics are desired.
 *
 * Unlike UNDOs which are processed backwards within the recovery span,
 * REDOs must be processed forwards starting further back (starting outside
 * the recovery span).
 *
 *	WRITE	- Write logical file (with payload).  Executed both
 *		  out-of-span and in-span.  Out-of-span WRITEs may be
 *		  filtered out by TERMs.
 *
 *	TRUNC	- Truncate logical file (no payload).  Executed both
 *		  out-of-span and in-span.  Out-of-span WRITEs may be
 *		  filtered out by TERMs.
 *
 *	TERM_*	- Indicates meta-data was committed (if out-of-span) or
 *		  will be rolled-back (in-span).  Any out-of-span TERMs
 *		  matching earlier WRITEs remove those WRITEs from
 *		  consideration as they might conflict with a later data
 *		  commit (which is not being rolled-back).
 *
 *	SYNC	- The earliest in-span SYNC (the last one when scanning
 *		  backwards) tells the recovery code how far out-of-span
 *		  it must go to run REDOs.
 *
 * NOTE: WRITEs do not always have matching TERMs even under
 *	 perfect conditions because truncations might remove the
 *	 buffers from consideration.  I/O problems can also remove
 *	 buffers from consideration.
 *
 *	 TRUNCSs do not always have matching TERMs because several
 *	 truncations may be aggregated together into a single TERM.
 */
typedef struct hammer_fifo_redo {
	struct hammer_fifo_head	head;
	int64_t			redo_objid;	/* file being written */
	hammer_off_t		redo_offset;	/* logical offset in file */
	int32_t			redo_data_bytes;
	uint32_t		redo_flags;
	uint32_t		redo_localization;
	uint32_t		redo_reserved01;
	uint64_t		redo_reserved02;
	/* followed by data */
} *hammer_fifo_redo_t;

#define HAMMER_REDO_WRITE	0x00000001
#define HAMMER_REDO_TRUNC	0x00000002
#define HAMMER_REDO_TERM_WRITE	0x00000004
#define HAMMER_REDO_TERM_TRUNC	0x00000008
#define HAMMER_REDO_SYNC	0x00000010

typedef union hammer_fifo_any {
	struct hammer_fifo_head	head;
	struct hammer_fifo_undo	undo;
	struct hammer_fifo_redo	redo;
} *hammer_fifo_any_t;

/*
 * Volume header types
 */
#define HAMMER_FSBUF_VOLUME	0xC8414D4DC5523031ULL	/* HAMMER01 */
#define HAMMER_FSBUF_VOLUME_REV	0x313052C54D4D41C8ULL	/* (reverse endian) */

/*
 * HAMMER Volume header
 *
 * A HAMMER filesystem can be built from 1-256 block devices, each block
 * device contains a volume header followed by however many buffers fit
 * into the volume.
 *
 * One of the volumes making up a HAMMER filesystem is the root volume.
 * The root volume is always volume #0 which is the first block device path
 * specified by newfs_hammer(8).  All HAMMER volumes have a volume header,
 * however the root volume may be the only volume that has valid values for
 * some fields in the header.
 *
 * Special field notes:
 *
 *	vol_bot_beg - offset of boot area (mem_beg - bot_beg bytes)
 *	vol_mem_beg - offset of memory log (buf_beg - mem_beg bytes)
 *	vol_buf_beg - offset of the first buffer in volume
 *	vol_buf_end - offset of volume EOF (on buffer boundary)
 *
 *	The memory log area allows a kernel to cache new records and data
 *	in memory without allocating space in the actual filesystem to hold
 *	the records and data.  In the event that a filesystem becomes full,
 *	any records remaining in memory can be flushed to the memory log
 *	area.  This allows the kernel to immediately return success.
 *
 *	The buffer offset is a physical offset of zone-2 offset. The lower
 *	52 bits of the zone-2 offset is added to the buffer offset of each
 *	volume to generate an actual I/O offset within the block device.
 *
 *	NOTE: boot area and memory log are currently not used.
 */

/*
 * Filesystem type string
 */
#define HAMMER_FSTYPE_STRING		"DragonFly HAMMER"

/*
 * These macros are only used by userspace when userspace commands either
 * initialize or add a new HAMMER volume.
 */
#define HAMMER_BOOT_MINBYTES		(32*1024)
#define HAMMER_BOOT_NOMBYTES		(64LL*1024*1024)
#define HAMMER_BOOT_MAXBYTES		(256LL*1024*1024)

#define HAMMER_MEM_MINBYTES		(256*1024)
#define HAMMER_MEM_NOMBYTES		(1LL*1024*1024*1024)
#define HAMMER_MEM_MAXBYTES		(64LL*1024*1024*1024)

typedef struct hammer_volume_ondisk {
	uint64_t vol_signature;	/* HAMMER_FSBUF_VOLUME for a valid header */

	/*
	 * These are relative to block device offset, not zone offsets.
	 */
	int64_t vol_bot_beg;	/* offset of boot area */
	int64_t vol_mem_beg;	/* offset of memory log */
	int64_t vol_buf_beg;	/* offset of the first buffer in volume */
	int64_t vol_buf_end;	/* offset of volume EOF (on buffer boundary) */
	int64_t vol_reserved01;

	hammer_uuid_t vol_fsid;	/* identify filesystem */
	hammer_uuid_t vol_fstype; /* identify filesystem type */
	char vol_label[64];	/* filesystem label */

	int32_t vol_no;		/* volume number within filesystem */
	int32_t vol_count;	/* number of volumes making up filesystem */

	uint32_t vol_version;	/* version control information */
	hammer_crc_t vol_crc;	/* header crc */
	uint32_t vol_flags;	/* volume flags */
	uint32_t vol_rootvol;	/* the root volume number (must be 0) */

	uint32_t vol_reserved[8];

	/*
	 * These fields are initialized and space is reserved in every
	 * volume making up a HAMMER filesystem, but only the root volume
	 * contains valid data.  Note that vol0_stat_bigblocks does not
	 * include big-blocks for freemap and undomap initially allocated
	 * by newfs_hammer(8).
	 */
	int64_t vol0_stat_bigblocks;	/* total big-blocks when fs is empty */
	int64_t vol0_stat_freebigblocks;/* number of free big-blocks */
	int64_t	vol0_reserved01;
	int64_t vol0_stat_inodes;	/* for statfs only */
	int64_t vol0_reserved02;
	hammer_off_t vol0_btree_root;	/* B-Tree root offset in zone-8 */
	hammer_tid_t vol0_next_tid;	/* highest partially synchronized TID */
	hammer_off_t vol0_reserved03;

	/*
	 * Blockmaps for zones.  Not all zones use a blockmap.  Note that
	 * the entire root blockmap is cached in the hammer_mount structure.
	 */
	struct hammer_blockmap	vol0_blockmap[HAMMER_MAX_ZONES];

	/*
	 * Array of zone-2 addresses for undo FIFO.
	 */
	hammer_off_t		vol0_undo_array[HAMMER_MAX_UNDO_BIGBLOCKS];
} *hammer_volume_ondisk_t;

#define HAMMER_ROOT_VOLNO		0

#define HAMMER_VOLF_NEEDFLUSH		0x0004	/* volume needs flush */

#define HAMMER_VOL_CRCSIZE1	\
	offsetof(struct hammer_volume_ondisk, vol_crc)
#define HAMMER_VOL_CRCSIZE2	\
	(sizeof(struct hammer_volume_ondisk) - HAMMER_VOL_CRCSIZE1 -	\
	 sizeof(hammer_crc_t))

#define HAMMER_VOL_VERSION_MIN		1	/* minimum supported version */
#define HAMMER_VOL_VERSION_DEFAULT	7	/* newfs default version */
#define HAMMER_VOL_VERSION_WIP		8	/* version >= this is WIP */
#define HAMMER_VOL_VERSION_MAX		7	/* maximum supported version */

#define HAMMER_VOL_VERSION_ONE		1
#define HAMMER_VOL_VERSION_TWO		2	/* new dirent layout (2.3+) */
#define HAMMER_VOL_VERSION_THREE	3	/* new snapshot layout (2.5+) */
#define HAMMER_VOL_VERSION_FOUR		4	/* new undo/flush (2.5+) */
#define HAMMER_VOL_VERSION_FIVE		5	/* dedup (2.9+) */
#define HAMMER_VOL_VERSION_SIX		6	/* DIRHASH_ALG1 */
#define HAMMER_VOL_VERSION_SEVEN	7	/* use the faster iscsi_crc */

/*
 * Translate a zone-2 address to physical address
 */
#define hammer_xlate_to_phys(volume, zone2_offset)	\
	((volume)->vol_buf_beg + HAMMER_OFF_SHORT_ENCODE(zone2_offset))

/*
 * Translate a zone-3 address to zone-2 address
 */
#define HAMMER_UNDO_INDEX(zone3_offset)			\
	(HAMMER_OFF_SHORT_ENCODE(zone3_offset) / HAMMER_BIGBLOCK_SIZE)

#define hammer_xlate_to_undo(volume, zone3_offset)			\
	((volume)->vol0_undo_array[HAMMER_UNDO_INDEX(zone3_offset)] +	\
	 (zone3_offset & HAMMER_BIGBLOCK_MASK64))

/*
 * Effective per-volume filesystem capacity including big-blocks for layer1/2
 */
#define HAMMER_VOL_BUF_SIZE(volume)			\
	((volume)->vol_buf_end - (volume)->vol_buf_beg)

/*
 * Record types are fairly straightforward.  The B-Tree includes the record
 * type in its index sort.
 */
#define HAMMER_RECTYPE_UNKNOWN		0x0000
#define HAMMER_RECTYPE_INODE		0x0001	/* inode in obj_id space */
#define HAMMER_RECTYPE_DATA		0x0010
#define HAMMER_RECTYPE_DIRENTRY		0x0011
#define HAMMER_RECTYPE_DB		0x0012
#define HAMMER_RECTYPE_EXT		0x0013	/* ext attributes */
#define HAMMER_RECTYPE_FIX		0x0014	/* fixed attribute */
#define HAMMER_RECTYPE_PFS		0x0015	/* PFS management */
#define HAMMER_RECTYPE_SNAPSHOT		0x0016	/* Snapshot management */
#define HAMMER_RECTYPE_CONFIG		0x0017	/* hammer cleanup config */
#define HAMMER_RECTYPE_MAX		0xFFFF

#define HAMMER_RECTYPE_ENTRY_START	(HAMMER_RECTYPE_INODE + 1)
#define HAMMER_RECTYPE_CLEAN_START	HAMMER_RECTYPE_EXT

#define HAMMER_FIXKEY_SYMLINK		1

#define HAMMER_OBJTYPE_UNKNOWN		0	/* never exists on-disk as unknown */
#define HAMMER_OBJTYPE_DIRECTORY	1
#define HAMMER_OBJTYPE_REGFILE		2
#define HAMMER_OBJTYPE_DBFILE		3
#define HAMMER_OBJTYPE_FIFO		4
#define HAMMER_OBJTYPE_CDEV		5
#define HAMMER_OBJTYPE_BDEV		6
#define HAMMER_OBJTYPE_SOFTLINK		7
#define HAMMER_OBJTYPE_PSEUDOFS		8	/* pseudo filesystem obj */
#define HAMMER_OBJTYPE_SOCKET		9

/*
 * HAMMER inode attribute data
 *
 * The data reference for a HAMMER inode points to this structure.  Any
 * modifications to the contents of this structure will result in a
 * replacement operation.
 *
 * parent_obj_id is only valid for directories (which cannot be hard-linked),
 * and specifies the parent directory obj_id.  This field will also be set
 * for non-directory inodes as a recovery aid, but can wind up holding
 * stale information.  However, since object id's are not reused, the worse
 * that happens is that the recovery code is unable to use it.
 * A parent_obj_id of 0 means it's a root inode of root or non-root PFS.
 *
 * NOTE: Future note on directory hardlinks.  We can implement a record type
 * which allows us to point to multiple parent directories.
 */
typedef struct hammer_inode_data {
	uint16_t version;	/* inode data version */
	uint16_t mode;		/* basic unix permissions */
	uint32_t uflags;	/* chflags */
	uint32_t rmajor;	/* used by device nodes */
	uint32_t rminor;	/* used by device nodes */
	uint64_t ctime;
	int64_t parent_obj_id;	/* parent directory obj_id */
	hammer_uuid_t uid;
	hammer_uuid_t gid;

	uint8_t obj_type;
	uint8_t cap_flags;	/* capability support flags (extension) */
	uint16_t reserved01;
	uint32_t reserved02;
	uint64_t nlinks;	/* hard links */
	uint64_t size;		/* filesystem object size */
	union {
		char	symlink[24];	/* HAMMER_INODE_BASESYMLEN */
	} ext;
	uint64_t mtime;	/* mtime must be second-to-last */
	uint64_t atime;	/* atime must be last */
} *hammer_inode_data_t;

/*
 * Neither mtime nor atime updates are CRCd by the B-Tree element.
 * mtime updates have UNDO, atime updates do not.
 */
#define HAMMER_INODE_CRCSIZE	\
	offsetof(struct hammer_inode_data, mtime)

#define HAMMER_INODE_DATA_VERSION	1
#define HAMMER_OBJID_ROOT		1	/* root inodes # */
#define HAMMER_INODE_BASESYMLEN		24	/* see ext.symlink */

/*
 * Capability & implementation flags.
 *
 * HAMMER_INODE_CAP_DIR_LOCAL_INO - Use inode B-Tree localization
 * for directory entries.  Also see HAMMER_DIR_INODE_LOCALIZATION().
 */
#define HAMMER_INODE_CAP_DIRHASH_MASK	0x03	/* directory: hash algorithm */
#define HAMMER_INODE_CAP_DIRHASH_ALG0	0x00
#define HAMMER_INODE_CAP_DIRHASH_ALG1	0x01
#define HAMMER_INODE_CAP_DIRHASH_ALG2	0x02
#define HAMMER_INODE_CAP_DIRHASH_ALG3	0x03
#define HAMMER_INODE_CAP_DIR_LOCAL_INO	0x04	/* use inode localization */

#define HAMMER_DATA_DOALIGN(offset)				\
	(((offset) + 15) & ~15)
#define HAMMER_DATA_DOALIGN_WITH(type, offset)			\
	(((type)(offset) + 15) & (~(type)15))

/*
 * A HAMMER directory entry associates a HAMMER filesystem object with a
 * namespace.  It is hooked into a pseudo-filesystem (with its own inode
 * numbering space) in the filesystem by setting the high 16 bits of the
 * localization field.  The low 16 bits must be 0 and are reserved for
 * future use.
 *
 * Directory entries are indexed with a 128 bit namekey rather then an
 * offset.  A portion of the namekey is an iterator/randomizer to deal
 * with collisions.
 *
 * NOTE: leaf.base.obj_type from the related B-Tree leaf entry holds
 * the filesystem object type of obj_id, e.g. a den_type equivalent.
 * It is not stored in hammer_direntry_data.
 *
 * NOTE: name field / the filename data reference is NOT terminated with \0.
 */
typedef struct hammer_direntry_data {
	int64_t obj_id;			/* object being referenced */
	uint32_t localization;		/* identify pseudo-filesystem */
	uint32_t reserved01;
	char	name[16];		/* name (extended) */
} *hammer_direntry_data_t;

#define HAMMER_ENTRY_NAME_OFF	offsetof(struct hammer_direntry_data, name[0])
#define HAMMER_ENTRY_SIZE(nlen)	offsetof(struct hammer_direntry_data, name[nlen])

/*
 * Symlink data which does not fit in the inode is stored in a separate
 * FIX type record.
 */
typedef struct hammer_symlink_data {
	char	name[16];		/* name (extended) */
} *hammer_symlink_data_t;

#define HAMMER_SYMLINK_NAME_OFF	offsetof(struct hammer_symlink_data, name[0])

/*
 * The root inode for the primary filesystem and root inode for any
 * pseudo-fs may be tagged with an optional data structure using
 * HAMMER_RECTYPE_PFS and localization id.  This structure allows
 * the node to be used as a mirroring master or slave.
 *
 * When operating as a slave CD's into the node automatically become read-only
 * and as-of sync_end_tid.
 *
 * When operating as a master the read PFSD info sets sync_end_tid to
 * the most recently flushed TID.
 *
 * sync_low_tid is not yet used but will represent the highest pruning
 * end-point, after which full history is available.
 *
 * We need to pack this structure making it equally sized on both 32-bit and
 * 64-bit machines as it is part of struct hammer_ioc_mrecord_pfs which is
 * send over the wire in hammer mirror operations. Only on 64-bit machines
 * the size of this struct differ when packed or not. This leads us to the
 * situation where old 64-bit systems (using the non-packed structure),
 * which were never able to mirror to/from 32-bit systems, are now no longer
 * able to mirror to/from newer 64-bit systems (using the packed structure).
 */
struct hammer_pseudofs_data {
	hammer_tid_t	sync_low_tid;	/* full history beyond this point */
	hammer_tid_t	sync_beg_tid;	/* earliest tid w/ full history avail */
	hammer_tid_t	sync_end_tid;	/* current synchronizatoin point */
	uint64_t	sync_beg_ts;	/* real-time of last completed sync */
	uint64_t	sync_end_ts;	/* initiation of current sync cycle */
	hammer_uuid_t	shared_uuid;	/* shared uuid (match required) */
	hammer_uuid_t	unique_uuid;	/* unique uuid of this master/slave */
	int32_t		reserved01;	/* reserved for future master_id */
	int32_t		mirror_flags;	/* misc flags */
	char		label[64];	/* filesystem space label */
	char		snapshots[64];	/* softlink dir for pruning */
	int32_t		reserved02;	/* was prune_{time,freq} */
	int32_t		reserved03;	/* was reblock_{time,freq} */
	int32_t		reserved04;	/* was snapshot_freq */
	int32_t		prune_min;	/* do not prune recent history */
	int32_t		prune_max;	/* do not retain history beyond here */
	int32_t		reserved[16];
} __packed;

typedef struct hammer_pseudofs_data *hammer_pseudofs_data_t;

#define HAMMER_PFSD_SLAVE	0x00000001
#define HAMMER_PFSD_DELETED	0x80000000

#define hammer_is_pfs_slave(pfsd)			\
	(((pfsd)->mirror_flags & HAMMER_PFSD_SLAVE) != 0)
#define hammer_is_pfs_master(pfsd)			\
	(!hammer_is_pfs_slave(pfsd))
#define hammer_is_pfs_deleted(pfsd)			\
	(((pfsd)->mirror_flags & HAMMER_PFSD_DELETED) != 0)

#define HAMMER_MAX_PFS		65536
#define HAMMER_MAX_PFSID	(HAMMER_MAX_PFS - 1)
#define HAMMER_ROOT_PFSID	0

/*
 * Snapshot meta-data { Objid = HAMMER_OBJID_ROOT, Key = tid, rectype = SNAPSHOT }.
 *
 * Snapshot records replace the old <fs>/snapshots/<softlink> methodology.  Snapshot
 * records are mirrored but may be independently managed once they are laid down on
 * a slave.
 *
 * NOTE: The b-tree key is signed, the tid is not, so callers must still sort the
 *	 results.
 *
 * NOTE: Reserved fields must be zero (as usual)
 */
typedef struct hammer_snapshot_data {
	hammer_tid_t	tid;		/* the snapshot TID itself (== key) */
	uint64_t	ts;		/* real-time when snapshot was made */
	uint64_t	reserved01;
	uint64_t	reserved02;
	char		label[64];	/* user-supplied description */
	uint64_t	reserved03[4];
} *hammer_snapshot_data_t;

/*
 * Config meta-data { ObjId = HAMMER_OBJID_ROOT, Key = 0, rectype = CONFIG }.
 *
 * Used to store the hammer cleanup config.  This data is not mirrored.
 */
typedef struct hammer_config_data {
	char		text[1024];
} *hammer_config_data_t;

/*
 * Rollup various structures embedded as record data
 */
typedef union hammer_data_ondisk {
	struct hammer_direntry_data entry;
	struct hammer_inode_data inode;
	struct hammer_symlink_data symlink;
	struct hammer_pseudofs_data pfsd;
	struct hammer_snapshot_data snap;
	struct hammer_config_data config;
} *hammer_data_ondisk_t;

/*
 * Ondisk layout of B-Tree related structures
 */
#if 0	 /* Not needed for fstype(8) */
#include "hammer_btree.h"
#endif

#define HAMMER_DIR_INODE_LOCALIZATION(ino_data)				\
	(((ino_data)->cap_flags & HAMMER_INODE_CAP_DIR_LOCAL_INO) ?	\
	 HAMMER_LOCALIZE_INODE :					\
	 HAMMER_LOCALIZE_MISC)

#endif /* !VFS_HAMMER_DISK_H_ */
