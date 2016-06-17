/*======================================================================

    drivers/mtd/maps/armflash.c: ARM Flash Layout/Partitioning
  
    Copyright (C) 2000 ARM Limited
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  
   This is access code for flashes using ARM's flash partitioning 
   standards.

   $Id: integrator-flash.c,v 1.7 2001/11/01 20:55:47 rmk Exp $

======================================================================*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/init.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>

extern int parse_afs_partitions(struct mtd_info *, struct mtd_partition **);

// board specific stuff - sorry, it should be in arch/arm/mach-*.
#ifdef CONFIG_ARCH_INTEGRATOR

#define FLASH_BASE	INTEGRATOR_FLASH_BASE
#define FLASH_SIZE	INTEGRATOR_FLASH_SIZE

#define FLASH_PART_SIZE 0x400000

#define SC_CTRLC	(IO_ADDRESS(INTEGRATOR_SC_BASE) + INTEGRATOR_SC_CTRLC_OFFSET)
#define SC_CTRLS	(IO_ADDRESS(INTEGRATOR_SC_BASE) + INTEGRATOR_SC_CTRLS_OFFSET)
#define EBI_CSR1	(IO_ADDRESS(INTEGRATOR_EBI_BASE) + INTEGRATOR_EBI_CSR1_OFFSET)
#define EBI_LOCK	(IO_ADDRESS(INTEGRATOR_EBI_BASE) + INTEGRATOR_EBI_LOCK_OFFSET)

/*
 * Initialise the flash access systems:
 *  - Disable VPP
 *  - Assert WP
 *  - Set write enable bit in EBI reg
 */
static void armflash_flash_init(void)
{
	unsigned int tmp;

	__raw_writel(INTEGRATOR_SC_CTRL_nFLVPPEN | INTEGRATOR_SC_CTRL_nFLWP, SC_CTRLC);

	tmp = __raw_readl(EBI_CSR1) | INTEGRATOR_EBI_WRITE_ENABLE;
	__raw_writel(tmp, EBI_CSR1);

	if (!(__raw_readl(EBI_CSR1) & INTEGRATOR_EBI_WRITE_ENABLE)) {
		__raw_writel(0xa05f, EBI_LOCK);
		__raw_writel(tmp, EBI_CSR1);
		__raw_writel(0, EBI_LOCK);
	}
}

/*
 * Shutdown the flash access systems:
 *  - Disable VPP
 *  - Assert WP
 *  - Clear write enable bit in EBI reg
 */
static void armflash_flash_exit(void)
{
	unsigned int tmp;

	__raw_writel(INTEGRATOR_SC_CTRL_nFLVPPEN | INTEGRATOR_SC_CTRL_nFLWP, SC_CTRLC);

	/*
	 * Clear the write enable bit in system controller EBI register.
	 */
	tmp = __raw_readl(EBI_CSR1) & ~INTEGRATOR_EBI_WRITE_ENABLE;
	__raw_writel(tmp, EBI_CSR1);

	if (__raw_readl(EBI_CSR1) & INTEGRATOR_EBI_WRITE_ENABLE) {
		__raw_writel(0xa05f, EBI_LOCK);
		__raw_writel(tmp, EBI_CSR1);
		__raw_writel(0, EBI_LOCK);
	}
}

static void armflash_flash_wp(int on)
{
	unsigned int reg;

	if (on)
		reg = SC_CTRLC;
	else
		reg = SC_CTRLS;

	__raw_writel(INTEGRATOR_SC_CTRL_nFLWP, reg);
}

static void armflash_set_vpp(struct map_info *map, int on)
{
	unsigned int reg;

	if (on)
		reg = SC_CTRLS;
	else
		reg = SC_CTRLC;

	__raw_writel(INTEGRATOR_SC_CTRL_nFLVPPEN, reg);
}
#endif

#ifdef CONFIG_ARCH_P720T

#define FLASH_BASE		(0x04000000)
#define FLASH_SIZE		(64*1024*1024)

#define FLASH_PART_SIZE 	(4*1024*1024)
#define FLASH_BLOCK_SIZE	(128*1024)

static void armflash_flash_init(void)
{
}

static void armflash_flash_exit(void)
{
}

static void armflash_flash_wp(int on)
{
}

static void armflash_set_vpp(struct map_info *map, int on)
{
}
#endif

static __u8 armflash_read8(struct map_info *map, unsigned long ofs)
{
	return readb(ofs + map->map_priv_2);
}

static __u16 armflash_read16(struct map_info *map, unsigned long ofs)
{
	return readw(ofs + map->map_priv_2);
}

static __u32 armflash_read32(struct map_info *map, unsigned long ofs)
{
	return readl(ofs + map->map_priv_2);
}

static void armflash_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *) (from + map->map_priv_2), len);
}

static void armflash_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, adr + map->map_priv_2);
}

static void armflash_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, adr + map->map_priv_2);
}

static void armflash_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, adr + map->map_priv_2);
}

static void armflash_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *) (to + map->map_priv_2), from, len);
}

static struct map_info armflash_map =
{
	name:		"AFS",
	read8:		armflash_read8,
	read16:		armflash_read16,
	read32:		armflash_read32,
	copy_from:	armflash_copy_from,
	write8:		armflash_write8,
	write16:	armflash_write16,
	write32:	armflash_write32,
	copy_to:	armflash_copy_to,
	set_vpp:	armflash_set_vpp,
};

static struct mtd_info *mtd;
static struct mtd_partition *parts;

static int __init armflash_cfi_init(void *base, u_int size)
{
	int ret;

	armflash_flash_init();
	armflash_flash_wp(1);

	/*
	 * look for CFI based flash parts fitted to this board
	 */
	armflash_map.size       = size;
	armflash_map.buswidth   = 4;
	armflash_map.map_priv_2 = (unsigned long) base;

	/*
	 * Also, the CFI layer automatically works out what size
	 * of chips we have, and does the necessary identification
	 * for us automatically.
	 */
	mtd = do_map_probe("cfi_probe", &armflash_map);
	if (!mtd)
		return -ENXIO;

	mtd->module = THIS_MODULE;

	ret = parse_afs_partitions(mtd, &parts);
	if (ret > 0) {
		ret = add_mtd_partitions(mtd, parts, ret);
		if (ret)
			printk(KERN_ERR "mtd partition registration "
				"failed: %d\n", ret);
	}

	/*
	 * If we got an error, free all resources.
	 */
	if (ret < 0) {
		del_mtd_partitions(mtd);
		map_destroy(mtd);
	}

	return ret;
}

static void armflash_cfi_exit(void)
{
	if (mtd) {
		del_mtd_partitions(mtd);
		map_destroy(mtd);
	}
	if (parts)
		kfree(parts);
}

static int __init armflash_init(void)
{
	int err = -EBUSY;
	void *base;

	if (request_mem_region(FLASH_BASE, FLASH_SIZE, "flash") == NULL)
		goto out;

	base = ioremap(FLASH_BASE, FLASH_SIZE);
	err = -ENOMEM;
	if (base == NULL)
		goto release;

	err = armflash_cfi_init(base, FLASH_SIZE);
	if (err) {
		iounmap(base);
release:
		release_mem_region(FLASH_BASE, FLASH_SIZE);
	}
out:
	return err;
}

static void __exit armflash_exit(void)
{
	armflash_cfi_exit();
	iounmap((void *)armflash_map.map_priv_2);
	release_mem_region(FLASH_BASE, FLASH_SIZE);
	armflash_flash_exit();
}

module_init(armflash_init);
module_exit(armflash_exit);

MODULE_AUTHOR("ARM Ltd");
MODULE_DESCRIPTION("ARM Integrator CFI map driver");
MODULE_LICENSE("GPL");
