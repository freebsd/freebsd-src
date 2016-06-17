/*
   raid0.c : Multiple Devices driver for Linux
             Copyright (C) 1994-96 Marc ZYNGIER
	     <zyngier@ufr-info-p7.ibp.fr> or
	     <maz@gloups.fdn.fr>
             Copyright (C) 1999, 2000 Ingo Molnar, Red Hat


   RAID-0 management functions.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include <linux/module.h>
#include <linux/raid/raid0.h>

#define MAJOR_NR MD_MAJOR
#define MD_DRIVER
#define MD_PERSONALITY

static int create_strip_zones (mddev_t *mddev)
{
	int i, c, j, j1, j2;
	unsigned long current_offset, curr_zone_offset;
	raid0_conf_t *conf = mddev_to_conf(mddev);
	mdk_rdev_t *smallest, *rdev1, *rdev2, *rdev;
 
	/*
	 * The number of 'same size groups'
	 */
	conf->nr_strip_zones = 0;
 
	ITERATE_RDEV_ORDERED(mddev,rdev1,j1) {
		printk("raid0: looking at %s\n", partition_name(rdev1->dev));
		c = 0;
		ITERATE_RDEV_ORDERED(mddev,rdev2,j2) {
			printk("raid0:   comparing %s(%ld) with %s(%ld)\n", partition_name(rdev1->dev), rdev1->size, partition_name(rdev2->dev), rdev2->size);
			if (rdev2 == rdev1) {
				printk("raid0:   END\n");
				break;
			}
			if (rdev2->size == rdev1->size)
			{
				/*
				 * Not unique, dont count it as a new
				 * group
				 */
				printk("raid0:   EQUAL\n");
				c = 1;
				break;
			}
			printk("raid0:   NOT EQUAL\n");
		}
		if (!c) {
			printk("raid0:   ==> UNIQUE\n");
			conf->nr_strip_zones++;
			printk("raid0: %d zones\n", conf->nr_strip_zones);
		}
	}
		printk("raid0: FINAL %d zones\n", conf->nr_strip_zones);

	conf->strip_zone = vmalloc(sizeof(struct strip_zone)*
				conf->nr_strip_zones);
	if (!conf->strip_zone)
		return 1;


	conf->smallest = NULL;
	current_offset = 0;
	curr_zone_offset = 0;

	for (i = 0; i < conf->nr_strip_zones; i++)
	{
		struct strip_zone *zone = conf->strip_zone + i;

		printk("raid0: zone %d\n", i);
		zone->dev_offset = current_offset;
		smallest = NULL;
		c = 0;

		ITERATE_RDEV_ORDERED(mddev,rdev,j) {

			printk("raid0: checking %s ...", partition_name(rdev->dev));
			if (rdev->size > current_offset)
			{
				printk(" contained as device %d\n", c);
				zone->dev[c] = rdev;
				c++;
				if (!smallest || (rdev->size <smallest->size)) {
					smallest = rdev;
					printk("  (%ld) is smallest!.\n", rdev->size);
				}
			} else
				printk(" nope.\n");
		}

		zone->nb_dev = c;
		zone->size = (smallest->size - current_offset) * c;
		printk("raid0: zone->nb_dev: %d, size: %ld\n",zone->nb_dev,zone->size);

		if (!conf->smallest || (zone->size < conf->smallest->size))
			conf->smallest = zone;

		zone->zone_offset = curr_zone_offset;
		curr_zone_offset += zone->size;

		current_offset = smallest->size;
		printk("raid0: current zone offset: %ld\n", current_offset);
	}
	printk("raid0: done.\n");
	return 0;
}

static int raid0_run (mddev_t *mddev)
{
	unsigned long cur=0, i=0, size, zone0_size, nb_zone;
	raid0_conf_t *conf;

	MOD_INC_USE_COUNT;

	conf = vmalloc(sizeof (raid0_conf_t));
	if (!conf)
		goto out;
	mddev->private = (void *)conf;
 
	if (md_check_ordering(mddev)) {
		printk("raid0: disks are not ordered, aborting!\n");
		goto out_free_conf;
	}

	if (create_strip_zones (mddev)) 
		goto out_free_conf;

	printk("raid0 : md_size is %d blocks.\n", md_size[mdidx(mddev)]);
	printk("raid0 : conf->smallest->size is %ld blocks.\n", conf->smallest->size);
	nb_zone = md_size[mdidx(mddev)]/conf->smallest->size +
			(md_size[mdidx(mddev)] % conf->smallest->size ? 1 : 0);
	printk("raid0 : nb_zone is %ld.\n", nb_zone);
	conf->nr_zones = nb_zone;

	printk("raid0 : Allocating %ld bytes for hash.\n",
				nb_zone*sizeof(struct raid0_hash));

	conf->hash_table = vmalloc (sizeof (struct raid0_hash)*nb_zone);
	if (!conf->hash_table)
		goto out_free_zone_conf;
	size = conf->strip_zone[cur].size;

	i = 0;
	while (cur < conf->nr_strip_zones) {
		conf->hash_table[i].zone0 = conf->strip_zone + cur;

		/*
		 * If we completely fill the slot
		 */
		if (size >= conf->smallest->size) {
			conf->hash_table[i++].zone1 = NULL;
			size -= conf->smallest->size;

			if (!size) {
				if (++cur == conf->nr_strip_zones)
					continue;
				size = conf->strip_zone[cur].size;
			}
			continue;
		}
		if (++cur == conf->nr_strip_zones) {
			/*
			 * Last dev, set unit1 as NULL
			 */
			conf->hash_table[i].zone1=NULL;
			continue;
		}

		/*
		 * Here we use a 2nd dev to fill the slot
		 */
		zone0_size = size;
		size = conf->strip_zone[cur].size;
		conf->hash_table[i++].zone1 = conf->strip_zone + cur;
		size -= (conf->smallest->size - zone0_size);
	}
	return 0;

out_free_zone_conf:
	vfree(conf->strip_zone);
	conf->strip_zone = NULL;

out_free_conf:
	vfree(conf);
	mddev->private = NULL;
out:
	MOD_DEC_USE_COUNT;
	return 1;
}

static int raid0_stop (mddev_t *mddev)
{
	raid0_conf_t *conf = mddev_to_conf(mddev);

	vfree (conf->hash_table);
	conf->hash_table = NULL;
	vfree (conf->strip_zone);
	conf->strip_zone = NULL;
	vfree (conf);
	mddev->private = NULL;

	MOD_DEC_USE_COUNT;
	return 0;
}

/*
 * FIXME - We assume some things here :
 * - requested buffers NEVER bigger than chunk size,
 * - requested buffers NEVER cross stripes limits.
 * Of course, those facts may not be valid anymore (and surely won't...)
 * Hey guys, there's some work out there ;-)
 */
static int raid0_make_request (mddev_t *mddev,
			       int rw, struct buffer_head * bh)
{
	unsigned int sect_in_chunk, chunksize_bits,  chunk_size;
	raid0_conf_t *conf = mddev_to_conf(mddev);
	struct raid0_hash *hash;
	struct strip_zone *zone;
	mdk_rdev_t *tmp_dev;
	unsigned long chunk, block, rsect;

	chunk_size = mddev->param.chunk_size >> 10;
	chunksize_bits = ffz(~chunk_size);
	block = bh->b_rsector >> 1;
	hash = conf->hash_table + block / conf->smallest->size;

	/* Sanity check */
	if (chunk_size < (block % chunk_size) + (bh->b_size >> 10))
		goto bad_map;
 
	if (!hash)
		goto bad_hash;

	if (!hash->zone0)
		goto bad_zone0;
 
	if (block >= (hash->zone0->size + hash->zone0->zone_offset)) {
		if (!hash->zone1)
			goto bad_zone1;
		zone = hash->zone1;
	} else
		zone = hash->zone0;
    
	sect_in_chunk = bh->b_rsector & ((chunk_size<<1) -1);
	chunk = (block - zone->zone_offset) / (zone->nb_dev << chunksize_bits);
	tmp_dev = zone->dev[(block >> chunksize_bits) % zone->nb_dev];
	rsect = (((chunk << chunksize_bits) + zone->dev_offset)<<1)
		+ sect_in_chunk;
 
	/*
	 * The new BH_Lock semantics in ll_rw_blk.c guarantee that this
	 * is the only IO operation happening on this bh.
	 */
	bh->b_rdev = tmp_dev->dev;
	bh->b_rsector = rsect;

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
	return 1;

bad_map:
	printk ("raid0_make_request bug: can't convert block across chunks or bigger than %dk %ld %d\n", chunk_size, bh->b_rsector, bh->b_size >> 10);
	goto outerr;
bad_hash:
	printk("raid0_make_request bug: hash==NULL for block %ld\n", block);
	goto outerr;
bad_zone0:
	printk ("raid0_make_request bug: hash->zone0==NULL for block %ld\n", block);
	goto outerr;
bad_zone1:
	printk ("raid0_make_request bug: hash->zone1==NULL for block %ld\n", block);
 outerr:
	buffer_IO_error(bh);
	return 0;
}
			   
static void raid0_status (struct seq_file *seq, mddev_t *mddev)
{
#undef MD_DEBUG
#ifdef MD_DEBUG
	int j, k;
	raid0_conf_t *conf = mddev_to_conf(mddev);
  
	seq_printf(seq, "      ");
	for (j = 0; j < conf->nr_zones; j++) {
		seq_printf(seq, "[z%d",
				conf->hash_table[j].zone0 - conf->strip_zone);
		if (conf->hash_table[j].zone1)
			seq_printf(seq, "/z%d] ",
				conf->hash_table[j].zone1 - conf->strip_zone);
		else
			seq_printf(seq, "] ");
	}
  
	seq_printf(seq, "\n");
  
	for (j = 0; j < conf->nr_strip_zones; j++) {
		seq_printf(seq, "      z%d=[", j);
		for (k = 0; k < conf->strip_zone[j].nb_dev; k++)
			seq_printf (seq, "%s/", partition_name(
				conf->strip_zone[j].dev[k]->dev));

		seq_printf (seq, "] zo=%d do=%d s=%d\n",
				conf->strip_zone[j].zone_offset,
				conf->strip_zone[j].dev_offset,
				conf->strip_zone[j].size);
	}
#endif
	seq_printf(seq, " %dk chunks", mddev->param.chunk_size/1024);
	return;
}

static mdk_personality_t raid0_personality=
{
	name:		"raid0",
	make_request:	raid0_make_request,
	run:		raid0_run,
	stop:		raid0_stop,
	status:		raid0_status,
};

static int md__init raid0_init (void)
{
	return register_md_personality (RAID0, &raid0_personality);
}

static void raid0_exit (void)
{
	unregister_md_personality (RAID0);
}

module_init(raid0_init);
module_exit(raid0_exit);
MODULE_LICENSE("GPL");
