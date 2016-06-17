
/* Overhauled routines for dealing with different mmap regions of flash */
/* $Id: map.h,v 1.29 2002/10/21 13:20:52 jocke Exp $ */

#ifndef __LINUX_MTD_MAP_H__
#define __LINUX_MTD_MAP_H__

#include <linux/config.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>

/* The map stuff is very simple. You fill in your struct map_info with
   a handful of routines for accessing the device, making sure they handle
   paging etc. correctly if your device needs it. Then you pass it off
   to a chip driver which deals with a mapped device - generally either
   do_cfi_probe() or do_ram_probe(), either of which will return a 
   struct mtd_info if they liked what they saw. At which point, you
   fill in the mtd->module with your own module address, and register 
   it.
   
   The mtd->priv field will point to the struct map_info, and any further
   private data required by the chip driver is linked from the 
   mtd->priv->fldrv_priv field. This allows the map driver to get at 
   the destructor function map->fldrv_destroy() when it's tired
   of living.
*/

struct map_info {
	char *name;
	unsigned long size;
	int buswidth; /* in octets */
	__u8 (*read8)(struct map_info *, unsigned long);
	__u16 (*read16)(struct map_info *, unsigned long);
	__u32 (*read32)(struct map_info *, unsigned long);  
	__u64 (*read64)(struct map_info *, unsigned long);  
	/* If it returned a 'long' I'd call it readl.
	 * It doesn't.
	 * I won't.
	 * dwmw2 */
	
	void (*copy_from)(struct map_info *, void *, unsigned long, ssize_t);
	void (*write8)(struct map_info *, __u8, unsigned long);
	void (*write16)(struct map_info *, __u16, unsigned long);
	void (*write32)(struct map_info *, __u32, unsigned long);
	void (*write64)(struct map_info *, __u64, unsigned long);
	void (*copy_to)(struct map_info *, unsigned long, const void *, ssize_t);

	u_char * (*point) (struct map_info *, loff_t, size_t);
	void (*unpoint) (struct map_info *, u_char *, loff_t, size_t);

	void (*set_vpp)(struct map_info *, int);
	/* We put these two here rather than a single void *map_priv, 
	   because we want mappers to be able to have quickly-accessible
	   cache for the 'currently-mapped page' without the _extra_
	   redirection that would be necessary. If you need more than
	   two longs, turn the second into a pointer. dwmw2 */
	unsigned long map_priv_1;
	unsigned long map_priv_2;
	void *fldrv_priv;
	struct mtd_chip_driver *fldrv;
};


struct mtd_chip_driver {
	struct mtd_info *(*probe)(struct map_info *map);
	void (*destroy)(struct mtd_info *);
	struct module *module;
	char *name;
	struct list_head list;
};

void register_mtd_chip_driver(struct mtd_chip_driver *);
void unregister_mtd_chip_driver(struct mtd_chip_driver *);

struct mtd_info *do_map_probe(const char *name, struct map_info *map);


/*
 * Destroy an MTD device which was created for a map device.
 * Make sure the MTD device is already unregistered before calling this
 */
static inline void map_destroy(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;

	if (map->fldrv->destroy)
		map->fldrv->destroy(mtd);
#ifdef CONFIG_MODULES
	if (map->fldrv->module)
		__MOD_DEC_USE_COUNT(map->fldrv->module);
#endif
	kfree(mtd);
}

#define ENABLE_VPP(map) do { if(map->set_vpp) map->set_vpp(map, 1); } while(0)
#define DISABLE_VPP(map) do { if(map->set_vpp) map->set_vpp(map, 0); } while(0)

#endif /* __LINUX_MTD_MAP_H__ */
