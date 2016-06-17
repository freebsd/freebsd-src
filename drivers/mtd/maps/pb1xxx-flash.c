/*
 * Flash memory access on Alchemy Pb1xxx boards
 * 
 * (C) 2001 Pete Popov <ppopov@mvista.com>
 * 
 * $Id: pb1xxx-flash.c,v 1.4 2002/09/13 13:51:54 dwmw2 Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/au1000.h>

#ifdef 	DEBUG_RW
#define	DBG(x...)	printk(x)
#else
#define	DBG(x...)	
#endif

#ifdef CONFIG_MIPS_PB1000
#define WINDOW_ADDR 0x1F800000
#define WINDOW_SIZE 0x800000
#endif

__u8 physmap_read8(struct map_info *map, unsigned long ofs)
{
	__u8 ret;
	ret = __raw_readb(map->map_priv_1 + ofs);
	DBG("read8 from %x, %x\n", (unsigned)(map->map_priv_1 + ofs), ret);
	return ret;
}

__u16 physmap_read16(struct map_info *map, unsigned long ofs)
{
	__u16 ret;
	ret = __raw_readw(map->map_priv_1 + ofs);
	DBG("read16 from %x, %x\n", (unsigned)(map->map_priv_1 + ofs), ret);
	return ret;
}

__u32 physmap_read32(struct map_info *map, unsigned long ofs)
{
	__u32 ret;
	ret = __raw_readl(map->map_priv_1 + ofs);
	DBG("read32 from %x, %x\n", (unsigned)(map->map_priv_1 + ofs), ret);
	return ret;
}

void physmap_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	DBG("physmap_copy from %x to %x\n", (unsigned)from, (unsigned)to);
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void physmap_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	DBG("write8 at %x, %x\n", (unsigned)(map->map_priv_1 + adr), d);
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void physmap_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	DBG("write16 at %x, %x\n", (unsigned)(map->map_priv_1 + adr), d);
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void physmap_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	DBG("write32 at %x, %x\n", (unsigned)(map->map_priv_1 + adr), d);
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void physmap_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	DBG("physmap_copy_to %x from %x\n", (unsigned)to, (unsigned)from);
	memcpy_toio(map->map_priv_1 + to, from, len);
}



static struct map_info pb1xxx_map = {
	name:		"Pb1xxx flash",
	read8: physmap_read8,
	read16: physmap_read16,
	read32: physmap_read32,
	copy_from: physmap_copy_from,
	write8: physmap_write8,
	write16: physmap_write16,
	write32: physmap_write32,
	copy_to: physmap_copy_to,
};


#ifdef CONFIG_MIPS_PB1000

static unsigned long flash_size = 0x00800000;
static unsigned char flash_buswidth = 4;
static struct mtd_partition pb1xxx_partitions[] = {
        {
                name: "yamon env",
                size: 0x00020000,
                offset: 0,
                mask_flags: MTD_WRITEABLE
        },{
                name: "User FS",
                size: 0x003e0000,
                offset: 0x20000,
        },{
                name: "boot code",
                size: 0x100000,
                offset: 0x400000,
                mask_flags: MTD_WRITEABLE
        },{
                name: "raw/kernel",
                size: 0x300000,
                offset: 0x500000
        }
};

#elif defined(CONFIG_MIPS_PB1500) || defined(CONFIG_MIPS_PB1100)

static unsigned char flash_buswidth = 4;
#if defined(CONFIG_MTD_PB1500_BOOT) && defined(CONFIG_MTD_PB1500_USER)
/* both 32MiB banks will be used. Combine the first 32MiB bank and the
 * first 28MiB of the second bank together into a single jffs/jffs2
 * partition.
 */
static unsigned long flash_size = 0x04000000;
#define WINDOW_ADDR 0x1C000000
#define WINDOW_SIZE 0x4000000
static struct mtd_partition pb1xxx_partitions[] = {
        {
                name: "User FS",
                size:   0x3c00000,
                offset: 0x0000000
        },{
                name: "yamon",
                size: 0x0100000,
                offset: 0x3c00000,
                mask_flags: MTD_WRITEABLE
        },{
                name: "raw kernel",
                size: 0x02c0000,
                offset: 0x3d00000
        }
};
#elif defined(CONFIG_MTD_PB1500_BOOT) && !defined(CONFIG_MTD_PB1500_USER)
static unsigned long flash_size = 0x02000000;
#define WINDOW_ADDR 0x1E000000
#define WINDOW_SIZE 0x2000000
static struct mtd_partition pb1xxx_partitions[] = {
        {
                name: "User FS",
                size:   0x1c00000,
                offset: 0x0000000
        },{
                name: "yamon",
                size: 0x0100000,
                offset: 0x1c00000,
                mask_flags: MTD_WRITEABLE
        },{
                name: "raw kernel",
                size: 0x02c0000,
                offset: 0x1d00000
        }
};
#elif !defined(CONFIG_MTD_PB1500_BOOT) && defined(CONFIG_MTD_PB1500_USER)
static unsigned long flash_size = 0x02000000;
#define WINDOW_ADDR 0x1C000000
#define WINDOW_SIZE 0x2000000
static struct mtd_partition pb1xxx_partitions[] = {
        {
                name: "User FS",
                size:   0x1e00000,
                offset: 0x0000000
        },{
                name: "raw kernel",
                size: 0x0200000,
                offset: 0x1e00000,
        }
};
#else
#error MTD_PB1500 define combo error /* should never happen */
#endif
#else
#error Unsupported board
#endif


#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_partition *parsed_parts;
static struct mtd_info *mymtd;

int __init pb1xxx_mtd_init(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	char *part_type;
	
	/* Default flash buswidth */
	pb1xxx_map.buswidth = flash_buswidth;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";
	parts = pb1xxx_partitions;
	nb_parts = NB_OF(pb1xxx_partitions);
	pb1xxx_map.size = flash_size;

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "Pb1xxx flash: probing %d-bit flash bus\n", 
			pb1xxx_map.buswidth*8);
	pb1xxx_map.map_priv_1 = 
		(unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
	mymtd = do_map_probe("cfi_probe", &pb1xxx_map);
	if (!mymtd) return -ENXIO;
	mymtd->module = THIS_MODULE;

	add_mtd_partitions(mymtd, parts, nb_parts);
	return 0;
}

static void __exit pb1xxx_mtd_cleanup(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
}

module_init(pb1xxx_mtd_init);
module_exit(pb1xxx_mtd_cleanup);

MODULE_AUTHOR("Pete Popov");
MODULE_DESCRIPTION("Pb1xxx CFI map driver");
MODULE_LICENSE("GPL");
