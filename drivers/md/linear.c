/*
   linear.c : Multiple Devices driver for Linux
              Copyright (C) 1994-96 Marc ZYNGIER
	      <zyngier@ufr-info-p7.ibp.fr> or
	      <maz@gloups.fdn.fr>

   Linear mode management functions.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include <linux/module.h>

#include <linux/raid/md.h>
#include <linux/slab.h>

#include <linux/raid/linear.h>

#define MAJOR_NR MD_MAJOR
#define MD_DRIVER
#define MD_PERSONALITY

static int linear_run (mddev_t *mddev)
{
	linear_conf_t *conf;
	struct linear_hash *table;
	mdk_rdev_t *rdev;
	int size, i, j, nb_zone;
	unsigned int curr_offset;

	MOD_INC_USE_COUNT;

	conf = kmalloc (sizeof (*conf), GFP_KERNEL);
	if (!conf)
		goto out;
	mddev->private = conf;

	if (md_check_ordering(mddev)) {
		printk("linear: disks are not ordered, aborting!\n");
		goto out;
	}
	/*
	 * Find the smallest device.
	 */

	conf->smallest = NULL;
	curr_offset = 0;
	ITERATE_RDEV_ORDERED(mddev,rdev,j) {
		dev_info_t *disk = conf->disks + j;

		disk->dev = rdev->dev;
		disk->size = rdev->size;
		disk->offset = curr_offset;

		curr_offset += disk->size;

		if (!conf->smallest || (disk->size < conf->smallest->size))
			conf->smallest = disk;
	}

	nb_zone = conf->nr_zones =
		md_size[mdidx(mddev)] / conf->smallest->size +
		((md_size[mdidx(mddev)] % conf->smallest->size) ? 1 : 0);
  
	conf->hash_table = kmalloc (sizeof (struct linear_hash) * nb_zone,
					GFP_KERNEL);
	if (!conf->hash_table)
		goto out;

	/*
	 * Here we generate the linear hash table
	 */
	table = conf->hash_table;
	i = 0;
	size = 0;
	for (j = 0; j < mddev->nb_dev; j++) {
		dev_info_t *disk = conf->disks + j;

		if (size < 0) {
			table[-1].dev1 = disk;
		}
		size += disk->size;

		while (size>0) {
			table->dev0 = disk;
			table->dev1 = NULL;
			size -= conf->smallest->size;
			table++;
		}
	}
	if (table-conf->hash_table != nb_zone)
		BUG();

	return 0;

out:
	if (conf)
		kfree(conf);
	MOD_DEC_USE_COUNT;
	return 1;
}

static int linear_stop (mddev_t *mddev)
{
	linear_conf_t *conf = mddev_to_conf(mddev);
  
	kfree(conf->hash_table);
	kfree(conf);

	MOD_DEC_USE_COUNT;

	return 0;
}

static int linear_make_request (mddev_t *mddev,
			int rw, struct buffer_head * bh)
{
        linear_conf_t *conf = mddev_to_conf(mddev);
        struct linear_hash *hash;
        dev_info_t *tmp_dev;
        long block;

	block = bh->b_rsector >> 1;
	hash = conf->hash_table + (block / conf->smallest->size);
  
	if (block >= (hash->dev0->size + hash->dev0->offset)) {
		if (!hash->dev1) {
			printk ("linear_make_request : hash->dev1==NULL for block %ld\n",
						block);
			buffer_IO_error(bh);
			return 0;
		}
		tmp_dev = hash->dev1;
	} else
		tmp_dev = hash->dev0;
    
	if (block >= (tmp_dev->size + tmp_dev->offset)
				|| block < tmp_dev->offset) {
		printk ("linear_make_request: Block %ld out of bounds on dev %s size %ld offset %ld\n", block, kdevname(tmp_dev->dev), tmp_dev->size, tmp_dev->offset);
		buffer_IO_error(bh);
		return 0;
	}
	bh->b_rdev = tmp_dev->dev;
	bh->b_rsector = bh->b_rsector - (tmp_dev->offset << 1);

	return 1;
}

static void linear_status (struct seq_file *seq, mddev_t *mddev)
{

#undef MD_DEBUG
#ifdef MD_DEBUG
	int j;
	linear_conf_t *conf = mddev_to_conf(mddev);
  
	seq_printf(seq, "      ");
	for (j = 0; j < conf->nr_zones; j++)
	{
		seq_printf(seq, "[%s",
			partition_name(conf->hash_table[j].dev0->dev));

		if (conf->hash_table[j].dev1)
			seq_printf(seq, "/%s] ",
			  partition_name(conf->hash_table[j].dev1->dev));
		else
			seq_printf(seq, "] ");
	}
	seq_printf(seq, "\n");
#endif
	seq_printf(seq, " %dk rounding", mddev->param.chunk_size/1024);
}


static mdk_personality_t linear_personality=
{
	name:		"linear",
	make_request:	linear_make_request,
	run:		linear_run,
	stop:		linear_stop,
	status:		linear_status,
};

static int md__init linear_init (void)
{
	return register_md_personality (LINEAR, &linear_personality);
}

static void linear_exit (void)
{
	unregister_md_personality (LINEAR);
}


module_init(linear_init);
module_exit(linear_exit);
MODULE_LICENSE("GPL");
