/*
 *  linux/fs/adfs/map.c
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/adfs_fs.h>
#include <linux/spinlock.h>

#include "adfs.h"

/*
 * For the future...
 */
static rwlock_t adfs_map_lock;

#define GET_FRAG_ID(_map,_start,_idmask)				\
	({								\
		unsigned long _v2, _frag;				\
		unsigned int _tmp;					\
		_tmp = _start >> 5;					\
		_frag = le32_to_cpu(_map[_tmp]);			\
		_v2   = le32_to_cpu(_map[_tmp + 1]);			\
		_tmp = start & 31;					\
		_frag = (_frag >> _tmp) | (_v2 << (32 - _tmp));		\
		_frag & _idmask;					\
	})

/*
 * return the map bit offset of the fragment frag_id in
 * the zone dm.
 * Note that the loop is optimised for best asm code -
 * look at the output of:
 *  gcc -D__KERNEL__ -O2 -I../../include -o - -S map.c
 */
static int
lookup_zone(const struct adfs_discmap *dm, const unsigned int idlen,
	    const unsigned int frag_id, unsigned int *offset)
{
	const unsigned int mapsize = dm->dm_endbit;
	const unsigned int idmask = (1 << idlen) - 1;
	unsigned long *map = ((unsigned long *)dm->dm_bh->b_data) + 1;
	unsigned int start = dm->dm_startbit;
	unsigned int mapptr;

	do {
		unsigned long frag;

		frag = GET_FRAG_ID(map, start, idmask);
		mapptr = start + idlen;

		/*
		 * find end of fragment
		 */
		{
			unsigned long v2;

			while ((v2 = map[mapptr >> 5] >> (mapptr & 31)) == 0) {
				mapptr = (mapptr & ~31) + 32;
				if (mapptr >= mapsize)
					goto error;
			}

			mapptr += 1 + ffz(~v2);
		}

		if (frag == frag_id)
			goto found;
again:
		start = mapptr;
	} while (mapptr < mapsize);

error:
	return -1;

found:
	{
		int length = mapptr - start;
		if (*offset >= length) {
			*offset -= length;
			goto again;
		}
	}
	return start + *offset;
}

/*
 * Scan the free space map, for this zone, calculating the total
 * number of map bits in each free space fragment.
 *
 * Note: idmask is limited to 15 bits [3.2]
 */
static unsigned int
scan_free_map(struct adfs_sb_info *asb, struct adfs_discmap *dm)
{
	const unsigned int mapsize = dm->dm_endbit + 32;
	const unsigned int idlen  = asb->s_idlen;
	const unsigned int frag_idlen = idlen <= 15 ? idlen : 15;
	const unsigned int idmask = (1 << frag_idlen) - 1;
	unsigned long *map = (unsigned long *)dm->dm_bh->b_data;
	unsigned int start = 8, mapptr;
	unsigned long frag;
	unsigned long total = 0;

	/*
	 * get fragment id
	 */
	frag = GET_FRAG_ID(map, start, idmask);

	/*
	 * If the freelink is null, then no free fragments
	 * exist in this zone.
	 */
	if (frag == 0)
		return 0;

	do {
		start += frag;

		/*
		 * get fragment id
		 */
		frag = GET_FRAG_ID(map, start, idmask);
		mapptr = start + idlen;

		/*
		 * find end of fragment
		 */
		{
			unsigned long v2;

			while ((v2 = map[mapptr >> 5] >> (mapptr & 31)) == 0) {
				mapptr = (mapptr & ~31) + 32;
				if (mapptr >= mapsize)
					goto error;
			}

			mapptr += 1 + ffz(~v2);
		}

		total += mapptr - start;
	} while (frag >= idlen + 1);

	if (frag != 0)
		printk(KERN_ERR "adfs: undersized free fragment\n");

	return total;
error:
	printk(KERN_ERR "adfs: oversized free fragment\n");
	return 0;
}

static int
scan_map(struct adfs_sb_info *asb, unsigned int zone,
	 const unsigned int frag_id, unsigned int mapoff)
{
	const unsigned int idlen = asb->s_idlen;
	struct adfs_discmap *dm, *dm_end;
	int result;

	dm	= asb->s_map + zone;
	zone	= asb->s_map_size;
	dm_end	= asb->s_map + zone;

	do {
		result = lookup_zone(dm, idlen, frag_id, &mapoff);

		if (result != -1)
			goto found;

		dm ++;
		if (dm == dm_end)
			dm = asb->s_map;
	} while (--zone > 0);

	return -1;
found:
	result -= dm->dm_startbit;
	result += dm->dm_startblk;

	return result;
}

/*
 * calculate the amount of free blocks in the map.
 *
 *              n=1
 *  total_free = E(free_in_zone_n)
 *              nzones
 */
unsigned int
adfs_map_free(struct super_block *sb)
{
	struct adfs_sb_info *asb = &sb->u.adfs_sb;
	struct adfs_discmap *dm;
	unsigned int total = 0;
	unsigned int zone;

	dm   = asb->s_map;
	zone = asb->s_map_size;

	do {
		total += scan_free_map(asb, dm++);
	} while (--zone > 0);

	return signed_asl(total, asb->s_map2blk);
}

int adfs_map_lookup (struct super_block *sb, int frag_id, int offset)
{
	struct adfs_sb_info *asb = &sb->u.adfs_sb;
	unsigned int zone, mapoff;
	int result;

	/*
	 * map & root fragment is special - it starts in the center of the
	 * disk.  The other fragments start at zone (frag / ids_per_zone)
	 */
	if (frag_id == ADFS_ROOT_FRAG)
		zone = asb->s_map_size >> 1;
	else
		zone = frag_id / asb->s_ids_per_zone;

	if (zone >= asb->s_map_size)
		goto bad_fragment;

	/* Convert sector offset to map offset */
	mapoff = signed_asl(offset, -asb->s_map2blk);

	read_lock(&adfs_map_lock);
	result = scan_map(asb, zone, frag_id, mapoff);
	read_unlock(&adfs_map_lock);

	if (result > 0) {
		unsigned int secoff;

		/* Calculate sector offset into map block */
		secoff = offset - signed_asl(mapoff, asb->s_map2blk);
		return secoff + signed_asl(result, asb->s_map2blk);
	}

	adfs_error(sb, "fragment %04X at offset %d not found in map",
		   frag_id, offset);
	return 0;

bad_fragment:
	adfs_error(sb, "fragment %X is invalid (zone = %d, max = %d)",
		   frag_id, zone, asb->s_map_size);
	return 0;
}
