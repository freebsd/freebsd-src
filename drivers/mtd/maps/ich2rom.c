/*
 * ich2rom.c
 *
 * Normal mappings of chips in physical memory
 * $Id: ich2rom.c,v 1.2 2002/10/18 22:45:48 eric Exp $
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/config.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

#define RESERVE_MEM_REGION 0

#define ICH2_FWH_REGION_START	0xFF000000UL
#define ICH2_FWH_REGION_SIZE	0x01000000UL
#define BIOS_CNTL	0x4e
#define FWH_DEC_EN1	0xE3
#define FWH_DEC_EN2	0xF0
#define FWH_SEL1	0xE8
#define FWH_SEL2	0xEE

struct ich2rom_map_info {
	struct map_info map;
	struct mtd_info *mtd;
	unsigned long window_addr;
};

static inline unsigned long addr(struct map_info *map, unsigned long ofs)
{
	unsigned long offset;
	offset = ((8*1024*1024) - map->size) + ofs;
	if (offset >= (4*1024*1024)) {
		offset += 0x400000;
	}
	return map->map_priv_1 + 0x400000 + offset;
}

static inline unsigned long dbg_addr(struct map_info *map, unsigned long addr)
{
	return addr - map->map_priv_1 + ICH2_FWH_REGION_START;
}
	
static __u8 ich2rom_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(addr(map, ofs));
}

static __u16 ich2rom_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(addr(map, ofs));
}

static __u32 ich2rom_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(addr(map, ofs));
}

static void ich2rom_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, addr(map, from), len);
}

static void ich2rom_write8(struct map_info *map, __u8 d, unsigned long ofs)
{
	__raw_writeb(d, addr(map,ofs));
	mb();
}

static void ich2rom_write16(struct map_info *map, __u16 d, unsigned long ofs)
{
	__raw_writew(d, addr(map, ofs));
	mb();
}

static void ich2rom_write32(struct map_info *map, __u32 d, unsigned long ofs)
{
	__raw_writel(d, addr(map, ofs));
	mb();
}

static void ich2rom_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(addr(map, to), from, len);
}

static struct ich2rom_map_info ich2rom_map = {
	map: {
		name: "ICH2 rom",
		size: 0,
		buswidth: 1,
		read8: ich2rom_read8,
		read16: ich2rom_read16,
		read32: ich2rom_read32,
		copy_from: ich2rom_copy_from,
		write8: ich2rom_write8,
		write16: ich2rom_write16,
		write32: ich2rom_write32,
		copy_to: ich2rom_copy_to,
		/* Firmware hubs only use vpp when being programmed
		 * in a factory setting.  So in place programming
		 * needs to use a different method.
		 */
	},
	mtd: 0,
	window_addr: 0,
};

enum fwh_lock_state {
	FWH_DENY_WRITE = 1,
	FWH_IMMUTABLE  = 2,
	FWH_DENY_READ  = 4,
};

static int ich2rom_set_lock_state(struct mtd_info *mtd, loff_t ofs, size_t len,
	enum fwh_lock_state state)
{
	struct map_info *map = mtd->priv;
	unsigned long start = ofs;
	unsigned long end = start + len -1;

	/* FIXME do I need to guard against concurrency here? */
	/* round down to 64K boundaries */
	start = start & ~0xFFFF;
	end = end & ~0xFFFF;
	while (start <= end) {
		unsigned long ctrl_addr;
		ctrl_addr = addr(map, start) - 0x400000 + 2;
		writeb(state, ctrl_addr);
		start = start + 0x10000;
	}
	return 0;
}

static int ich2rom_lock(struct mtd_info *mtd, loff_t ofs, size_t len)
{
	return ich2rom_set_lock_state(mtd, ofs, len, FWH_DENY_WRITE);
}

static int ich2rom_unlock(struct mtd_info *mtd, loff_t ofs, size_t len)
{
	return ich2rom_set_lock_state(mtd, ofs, len, 0);
}

static int __devinit ich2rom_init_one (struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	u16 word;
	struct ich2rom_map_info *info = &ich2rom_map;
	unsigned long map_size;

	/* For now I just handle the ich2 and I assume there
	 * are not a lot of resources up at the top of the address
	 * space.  It is possible to handle other devices in the
	 * top 16MB but it is very painful.  Also since
	 * you can only really attach a FWH to an ICH2 there
	 * a number of simplifications you can make.
	 *
	 * Also you can page firmware hubs if an 8MB window isn't enough 
	 * but don't currently handle that case either.
	 */

#if RESERVE_MEM_REGION
	/* Some boards have this reserved and I haven't found a good work
	 * around to say I know what I'm doing!
	 */
	if (!request_mem_region(ICH2_FWH_REGION_START, ICH2_FWH_REGION_SIZE, "ich2rom")) {
		printk(KERN_ERR "ich2rom: cannot reserve rom window\n");
		goto err_out_none;
	}
#endif /* RESERVE_MEM_REGION */
	
	/* Enable writes through the rom window */
	pci_read_config_word(pdev, BIOS_CNTL, &word);
	if (!(word & 1)  && (word & (1<<1))) {
		/* The BIOS will generate an error if I enable
		 * this device, so don't even try.
		 */
		printk(KERN_ERR "ich2rom: firmware access control, I can't enable writes\n");
		goto err_out_none;
	}
	pci_write_config_word(pdev, BIOS_CNTL, word | 1);


	/* Map the firmware hub into my address space. */
	/* Does this use to much virtual address space? */
	info->window_addr = (unsigned long)ioremap(
		ICH2_FWH_REGION_START, ICH2_FWH_REGION_SIZE);
	if (!info->window_addr) {
		printk(KERN_ERR "Failed to ioremap\n");
		goto err_out_free_mmio_region;
	}

	/* For now assume the firmware has setup all relavent firmware
	 * windows.  We don't have enough information to handle this case
	 * intelligently.
	 */

	/* FIXME select the firmware hub and enable a window to it. */

	info->mtd = 0;
	info->map.map_priv_1 = 	info->window_addr;

	map_size = ICH2_FWH_REGION_SIZE;
	while(!info->mtd && (map_size > 0)) {
		info->map.size = map_size;
		info->mtd = do_map_probe("jedec_probe", &ich2rom_map.map);
		map_size -= 512*1024;
	}
	if (!info->mtd) {
		goto err_out_iounmap;
	}
	/* I know I can only be a firmware hub here so put
	 * in the special lock and unlock routines.
	 */
	info->mtd->lock = ich2rom_lock;
	info->mtd->unlock = ich2rom_unlock;
		
	info->mtd->module = THIS_MODULE;
	add_mtd_device(info->mtd);
	return 0;

err_out_iounmap:
	iounmap((void *)(info->window_addr));
err_out_free_mmio_region:
#if RESERVE_MEM_REGION
	release_mem_region(ICH2_FWH_REGION_START, ICH2_FWH_REGION_SIZE);
#endif
err_out_none:
	return -ENODEV;
}


static void __devexit ich2rom_remove_one (struct pci_dev *pdev)
{
	struct ich2rom_map_info *info = &ich2rom_map;
	u16 word;

	del_mtd_device(info->mtd);
	map_destroy(info->mtd);
	info->mtd = 0;
	info->map.map_priv_1 = 0;

	iounmap((void *)(info->window_addr));
	info->window_addr = 0;

	/* Disable writes through the rom window */
	pci_read_config_word(pdev, BIOS_CNTL, &word);
	pci_write_config_word(pdev, BIOS_CNTL, word & ~1);

#if RESERVE_MEM_REGION	
	release_mem_region(ICH2_FWH_REGION_START, ICH2_FWH_REGION_SIZE);
#endif
}

static struct pci_device_id ich2rom_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801BA_0, 
	  PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801CA_0, 
	  PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, ich2rom_pci_tbl);

#if 0
static struct pci_driver ich2rom_driver = {
	name:	  "ich2rom",
	id_table: ich2rom_pci_tbl,
	probe:    ich2rom_init_one,
	remove:   ich2rom_remove_one,
};
#endif

static struct pci_dev *mydev;
int __init init_ich2rom(void)
{
	struct pci_dev *pdev;
	struct pci_device_id *id;
	pdev = 0;
	for(id = ich2rom_pci_tbl; id->vendor; id++) {
		pdev = pci_find_device(id->vendor, id->device, 0);
		if (pdev) {
			break;
		}
	}
	if (pdev) {
		mydev = pdev;
		return ich2rom_init_one(pdev, &ich2rom_pci_tbl[0]);
	}
	return -ENXIO;
#if 0
	return pci_module_init(&ich2rom_driver);
#endif
}

static void __exit cleanup_ich2rom(void)
{
	ich2rom_remove_one(mydev);
}

module_init(init_ich2rom);
module_exit(cleanup_ich2rom);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Biederman <ebiederman@lnxi.com>");
MODULE_DESCRIPTION("MTD map driver for BIOS chips on the ICH2 southbridge");
