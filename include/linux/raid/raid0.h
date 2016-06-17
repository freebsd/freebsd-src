#ifndef _RAID0_H
#define _RAID0_H

#include <linux/raid/md.h>

struct strip_zone
{
	unsigned long zone_offset;	/* Zone offset in md_dev */
	unsigned long dev_offset;	/* Zone offset in real dev */
	unsigned long size;		/* Zone size */
	int nb_dev;			/* # of devices attached to the zone */
	mdk_rdev_t *dev[MD_SB_DISKS]; /* Devices attached to the zone */
};

struct raid0_hash
{
	struct strip_zone *zone0, *zone1;
};

struct raid0_private_data
{
	struct raid0_hash *hash_table; /* Dynamically allocated */
	struct strip_zone *strip_zone; /* This one too */
	int nr_strip_zones;
	struct strip_zone *smallest;
	int nr_zones;
};

typedef struct raid0_private_data raid0_conf_t;

#define mddev_to_conf(mddev) ((raid0_conf_t *) mddev->private)

#endif
