/*
 *    $Id: zorro.c,v 1.1.2.1 1998/06/07 23:21:02 geert Exp $
 *
 *    Zorro Bus Services
 *
 *    Copyright (C) 1995-2000 Geert Uytterhoeven
 *
 *    This file is subject to the terms and conditions of the GNU General Public
 *    License.  See the file COPYING in the main directory of this archive
 *    for more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/zorro.h>
#include <asm/setup.h>
#include <asm/bitops.h>
#include <asm/amigahw.h>


    /*
     *  Zorro Expansion Devices
     */

u_int zorro_num_autocon = 0;
struct zorro_dev zorro_autocon[ZORRO_NUM_AUTO];


    /*
     *  Zorro Bus Resources
     *  Order _does_ matter! (see code below)
     */

static struct resource zorro_res[4] = {
    /* Zorro II regions (on Zorro II/III) */
    { "Zorro II exp", 0x00e80000, 0x00efffff },
    { "Zorro II mem", 0x00200000, 0x009fffff },
    /* Zorro III regions (on Zorro III only) */
    { "Zorro III exp", 0xff000000, 0xffffffff },
    { "Zorro III cfg", 0x40000000, 0x7fffffff }
};

static u_int zorro_num_res __initdata = 0;


    /*
     *  Find Zorro Devices
     */

struct zorro_dev *zorro_find_device(zorro_id id, struct zorro_dev *from)
{
    struct zorro_dev *dev;

    if (!MACH_IS_AMIGA || !AMIGAHW_PRESENT(ZORRO))
	return NULL;

    for (dev = from ? from+1 : &zorro_autocon[0];
	 dev < zorro_autocon+zorro_num_autocon;
	 dev++)
	if (id == ZORRO_WILDCARD || id == dev->id)
	    return dev;
    return NULL;
}


    /*
     *  Bitmask indicating portions of available Zorro II RAM that are unused
     *  by the system. Every bit represents a 64K chunk, for a maximum of 8MB
     *  (128 chunks, physical 0x00200000-0x009fffff).
     *
     *  If you want to use (= allocate) portions of this RAM, you should clear
     *  the corresponding bits.
     *
     *  Possible uses:
     *      - z2ram device
     *      - SCSI DMA bounce buffers
     *
     *  FIXME: use the normal resource management
     */

u32 zorro_unused_z2ram[4] = { 0, 0, 0, 0 };


static void __init mark_region(unsigned long start, unsigned long end,
			       int flag)
{
    if (flag)
	start += Z2RAM_CHUNKMASK;
    else
	end += Z2RAM_CHUNKMASK;
    start &= ~Z2RAM_CHUNKMASK;
    end &= ~Z2RAM_CHUNKMASK;

    if (end <= Z2RAM_START || start >= Z2RAM_END)
	return;
    start = start < Z2RAM_START ? 0x00000000 : start-Z2RAM_START;
    end = end > Z2RAM_END ? Z2RAM_SIZE : end-Z2RAM_START;
    while (start < end) {
	u32 chunk = start>>Z2RAM_CHUNKSHIFT;
	if (flag)
	    set_bit(chunk, zorro_unused_z2ram);
	else
	    clear_bit(chunk, zorro_unused_z2ram);
	start += Z2RAM_CHUNKSIZE;
    }
}


static struct resource __init *zorro_find_parent_resource(struct zorro_dev *z)
{
    int i;

    for (i = 0; i < zorro_num_res; i++)
	if (z->resource.start >= zorro_res[i].start &&
	    z->resource.end <= zorro_res[i].end)
		return &zorro_res[i];
    return &iomem_resource;
}


    /*
     *  Initialization
     */

void __init zorro_init(void)
{
    struct zorro_dev *dev;
    u_int i;

    if (!MACH_IS_AMIGA || !AMIGAHW_PRESENT(ZORRO))
	return;

    printk("Zorro: Probing AutoConfig expansion devices: %d device%s\n",
	   zorro_num_autocon, zorro_num_autocon == 1 ? "" : "s");

    /* Request the resources */
    zorro_num_res = AMIGAHW_PRESENT(ZORRO3) ? 4 : 2;
    for (i = 0; i < zorro_num_res; i++)
	request_resource(&iomem_resource, &zorro_res[i]);
    for (i = 0; i < zorro_num_autocon; i++) {
	dev = &zorro_autocon[i];
	dev->id = (dev->rom.er_Manufacturer<<16) | (dev->rom.er_Product<<8);
	if (dev->id == ZORRO_PROD_GVP_EPC_BASE) {
	    /* GVP quirk */
	    unsigned long magic = dev->resource.start+0x8000;
	    dev->id |= *(u16 *)ZTWO_VADDR(magic) & GVP_PRODMASK;
	}
	sprintf(dev->name, "Zorro device %08x", dev->id);
	zorro_name_device(dev);
	dev->resource.name = dev->name;
	if (request_resource(zorro_find_parent_resource(dev), &dev->resource))
	    printk(KERN_ERR "Zorro: Address space collision on device %s "
		   "[%lx:%lx]\n",
		   dev->name, dev->resource.start, dev->resource.end);
    }

    /* Mark all available Zorro II memory */
    for (i = 0; i < zorro_num_autocon; i++) {
	dev = &zorro_autocon[i];
	if (dev->rom.er_Type & ERTF_MEMLIST)
	    mark_region(dev->resource.start, dev->resource.end+1, 1);
    }

    /* Unmark all used Zorro II memory */
    for (i = 0; i < m68k_num_memory; i++)
	if (m68k_memory[i].addr < 16*1024*1024)
	    mark_region(m68k_memory[i].addr,
			m68k_memory[i].addr+m68k_memory[i].size, 0);
}


EXPORT_SYMBOL(zorro_find_device);
EXPORT_SYMBOL(zorro_unused_z2ram);

MODULE_LICENSE("GPL");
