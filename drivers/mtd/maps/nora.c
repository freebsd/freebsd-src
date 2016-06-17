/*
 * $Id: nora.c,v 1.21 2001/10/02 15:05:14 dwmw2 Exp $
 *
 * This is so simple I love it.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>


#define WINDOW_ADDR 0xd0000000
#define WINDOW_SIZE 0x04000000

static struct mtd_info *mymtd;

__u8 nora_read8(struct map_info *map, unsigned long ofs)
{
  return *(__u8 *)(WINDOW_ADDR + ofs);
}

__u16 nora_read16(struct map_info *map, unsigned long ofs)
{
  return *(__u16 *)(WINDOW_ADDR + ofs);
}

__u32 nora_read32(struct map_info *map, unsigned long ofs)
{
  return *(__u32 *)(WINDOW_ADDR + ofs);
}

void nora_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
  memcpy(to, (void *)(WINDOW_ADDR + from), len);
}

void nora_write8(struct map_info *map, __u8 d, unsigned long adr)
{
  *(__u8 *)(WINDOW_ADDR + adr) = d;
}

void nora_write16(struct map_info *map, __u16 d, unsigned long adr)
{
  *(__u16 *)(WINDOW_ADDR + adr) = d;
}

void nora_write32(struct map_info *map, __u32 d, unsigned long adr)
{
  *(__u32 *)(WINDOW_ADDR + adr) = d;
}

void nora_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
  memcpy((void *)(WINDOW_ADDR + to), from, len);
}

struct map_info nora_map = {
	name: "NORA",
	size: WINDOW_SIZE,
	buswidth: 2,
	read8: nora_read8,
	read16: nora_read16,
	read32: nora_read32,
	copy_from: nora_copy_from,
	write8: nora_write8,
	write16: nora_write16,
	write32: nora_write32,
	copy_to: nora_copy_to
};


static int nora_mtd_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	return mymtd->read(mymtd, from + (unsigned long)mtd->priv, len, retlen, buf);
}

static int nora_mtd_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	return mymtd->write(mymtd, to + (unsigned long)mtd->priv, len, retlen, buf);
}

static int nora_mtd_erase (struct mtd_info *mtd, struct erase_info *instr)
{
	instr->addr += (unsigned long)mtd->priv;
	return mymtd->erase(mymtd, instr);
}

static void nora_mtd_sync (struct mtd_info *mtd)
{
	mymtd->sync(mymtd);
}

static int nora_mtd_suspend (struct mtd_info *mtd)
{
	return mymtd->suspend(mymtd);
}

static void nora_mtd_resume (struct mtd_info *mtd)
{
	mymtd->resume(mymtd);
}


static struct mtd_info nora_mtds[4] = {  /* boot, kernel, ramdisk, fs */
	{
		type: MTD_NORFLASH,
		flags: MTD_CAP_NORFLASH,
		size: 0x60000,
		erasesize: 0x20000,
		name: "NORA boot firmware",
		module: THIS_MODULE,
		erase: nora_mtd_erase,
		read: nora_mtd_read,
		write: nora_mtd_write,
		suspend: nora_mtd_suspend,
		resume: nora_mtd_resume,
		sync: nora_mtd_sync,
		priv: (void *)0
	},
	{
		type: MTD_NORFLASH,
		flags: MTD_CAP_NORFLASH,
		size: 0x0a0000,
		erasesize: 0x20000,
		name: "NORA kernel",
		module: THIS_MODULE,
		erase: nora_mtd_erase,
		read: nora_mtd_read,
		write: nora_mtd_write,
		suspend: nora_mtd_suspend,
		resume: nora_mtd_resume,
		sync: nora_mtd_sync,
		priv: (void *)0x60000
	},
	{
		type: MTD_NORFLASH,
		flags: MTD_CAP_NORFLASH,
		size: 0x900000,
		erasesize: 0x20000,
		name: "NORA root filesystem",
		module: THIS_MODULE,
		erase: nora_mtd_erase,
		read: nora_mtd_read,
		write: nora_mtd_write,
		suspend: nora_mtd_suspend,
		resume: nora_mtd_resume,
		sync: nora_mtd_sync,
		priv: (void *)0x100000
	},
	{
		type: MTD_NORFLASH,
		flags: MTD_CAP_NORFLASH,
		size: 0x1600000,
		erasesize: 0x20000,
		name: "NORA second filesystem",
		module: THIS_MODULE,
		erase: nora_mtd_erase,
		read: nora_mtd_read,
		write: nora_mtd_write,
		suspend: nora_mtd_suspend,
		resume: nora_mtd_resume,
		sync: nora_mtd_sync,
		priv: (void *)0xa00000
	}
};

int __init init_nora(void)
{
       	printk(KERN_NOTICE "nora flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);

	mymtd = do_map_probe("cfi_probe", &nora_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;
		
		add_mtd_device(&nora_mtds[2]);
		add_mtd_device(&nora_mtds[0]);
		add_mtd_device(&nora_mtds[1]);
		add_mtd_device(&nora_mtds[3]);
		return 0;
	}

	return -ENXIO;
}

static void __exit cleanup_nora(void)
{
	if (mymtd) {
		del_mtd_device(&nora_mtds[3]);
		del_mtd_device(&nora_mtds[1]);
		del_mtd_device(&nora_mtds[0]);
		del_mtd_device(&nora_mtds[2]);
		map_destroy(mymtd);
	}
}

module_init(init_nora);
module_exit(cleanup_nora);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Red Hat, Inc. - David Woodhouse <dwmw2@cambridge.redhat.com>");
MODULE_DESCRIPTION("MTD map driver for Nora board");
