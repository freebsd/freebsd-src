/*
 * Flash memory access on SA11x0 based devices
 * 
 * (C) 2000 Nicolas Pitre <nico@cam.org>
 * 
 * $Id: sa1100-flash.c,v 1.29 2002/09/06 14:36:19 abz Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>


#ifndef CONFIG_ARCH_SA1100
#error This is for SA1100 architecture only
#endif


#define WINDOW_ADDR 0xe8000000

static __u8 sa1100_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 sa1100_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 sa1100_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void sa1100_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void sa1100_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void sa1100_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void sa1100_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void sa1100_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info sa1100_map = {
	name:		"SA1100 flash",
	read8:		sa1100_read8,
	read16:		sa1100_read16,
	read32:		sa1100_read32,
	copy_from:	sa1100_copy_from,
	write8:		sa1100_write8,
	write16:	sa1100_write16,
	write32:	sa1100_write32,
	copy_to:	sa1100_copy_to,

	map_priv_1:	WINDOW_ADDR,
	map_priv_2:	-1,
};


/*
 * Here are partition information for all known SA1100-based devices.
 * See include/linux/mtd/partitions.h for definition of the mtd_partition
 * structure.
 *
 * The *_max_flash_size is the maximum possible mapped flash size which
 * is not necessarily the actual flash size.  It must be no more than
 * the value specified in the "struct map_desc *_io_desc" mapping
 * definition for the corresponding machine.
 *
 * Please keep these in alphabetical order, and formatted as per existing
 * entries.  Thanks.
 */

#ifdef CONFIG_SA1100_ADSBITSY
#define ADSBITSY_FLASH_SIZE		0x02000000
static struct mtd_partition adsbitsy_partitions[] = {
	{
		name:		"bootROM",
		size:		0x80000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"zImage",
		size:		0x100000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"ramdisk.gz",
		size:		0x300000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"User FS",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#ifdef CONFIG_SA1100_ASSABET
/* Phase 4 Assabet has two 28F160B3 flash parts in bank 0: */
#define ASSABET4_FLASH_SIZE		0x00400000
static struct mtd_partition assabet4_partitions[] = {
	{
		name:		"bootloader",
		size:		0x00020000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"bootloader params",
		size:		0x00020000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"jffs",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};

/* Phase 5 Assabet has two 28F128J3A flash parts in bank 0: */
#define ASSABET5_FLASH_SIZE		0x02000000
static struct mtd_partition assabet5_partitions[] = {
	{
		name:		"bootloader",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"bootloader params",
		size:		0x00040000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"jffs",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};

#define ASSABET_FLASH_SIZE	ASSABET5_FLASH_SIZE
#define assabet_partitions	assabet5_partitions
#endif

#ifdef CONFIG_SA1100_BADGE4

/*
 * 1 x Intel 28F320C3BA100 Advanced+ Boot Block Flash (32 Mi bit)
 *   Eight 4 KiW Parameter Bottom Blocks (64 KiB)
 *   Sixty-three 32 KiW Main Blocks (4032 Ki b)
 */
#define BADGE4_FLASH_SIZE		0x00400000
static struct mtd_partition badge4_partitions[] = {
	{
		name:		"BLOB boot loader",
		offset:		0,
		size:		0x0000A000
	}, {
		name:		"params",
		offset:		MTDPART_OFS_APPEND,
		size:		0x00006000
	}, {
		name:		"kernel",
		offset:		MTDPART_OFS_APPEND,
		size:		0x00100000
	}, {
		name:		"root",
		offset:		MTDPART_OFS_APPEND,
		size:		MTDPART_SIZ_FULL
	}
};

#endif


#ifdef CONFIG_SA1100_CERF
#ifdef CONFIG_SA1100_CERF_FLASH_32MB
#define CERF_FLASH_SIZE			0x02000000
static struct mtd_partition cerf_partitions[] = {
	{
		name:		"firmware",
		size:		0x00040000,
		offset:		0,
	}, {
		name:		"params",
		size:		0x00040000,
		offset:		0x00040000,
	}, {
		name:		"kernel",
		size:		0x00100000,
		offset:		0x00080000,
	}, {
		name:		"rootdisk",
		size:		0x01E80000,
		offset:		0x00180000,
	}
};
#elif defined CONFIG_SA1100_CERF_FLASH_16MB
#define CERF_FLASH_SIZE			0x01000000
static struct mtd_partition cerf_partitions[] = {
	{
		name:		"firmware",
		size:		0x00020000,
		offset:		0,
	}, {
		name:		"params",
		size:		0x00020000,
		offset:		0x00020000,
	}, {
		name:		"kernel",
		size:		0x00100000,
		offset:		0x00040000,
	}, {
		name:		"rootdisk",
		size:		0x00EC0000,
		offset:		0x00140000,
	}
};
#elif defined CONFIG_SA1100_CERF_FLASH_8MB
#   error "Unwritten type definition"
#else
#   error "Undefined memory orientation for CERF in sa1100-flash.c"
#endif
#endif

#ifdef CONFIG_SA1100_CONSUS
#define CONSUS_FLASH_SIZE		0x02000000
static struct mtd_partition consus_partitions[] = {
	{
		name:		"Consus boot firmware",
		offset:		0,
		size:		0x00040000,
		mask_flags:	MTD_WRITABLE, /* force read-only */
	}, {
		name:		"Consus kernel",
		offset:		0x00040000,
		size:		0x00100000,
		mask_flags:	0,
	}, {
		name:		"Consus disk",
		offset:		0x00140000,
		/* The rest (up to 16M) for jffs.  We could put 0 and
		   make it find the size automatically, but right now
		   i have 32 megs.  jffs will use all 32 megs if given
		   the chance, and this leads to horrible problems
		   when you try to re-flash the image because blob
		   won't erase the whole partition. */
		size:		0x01000000 - 0x00140000,
		mask_flags:	0,
	}, {
		/* this disk is a secondary disk, which can be used as
		   needed, for simplicity, make it the size of the other
		   consus partition, although realistically it could be
		   the remainder of the disk (depending on the file
		   system used) */
		 name:		"Consus disk2",
		 offset:	0x01000000,
		 size:		0x01000000 - 0x00140000,
		 mask_flags:	0,
	}
};
#endif

#ifdef CONFIG_SA1100_FLEXANET
/* Flexanet has two 28F128J3A flash parts in bank 0: */
#define FLEXANET_FLASH_SIZE		0x02000000
static struct mtd_partition flexanet_partitions[] = {
	{
		name:		"bootloader",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"bootloader params",
		size:		0x00040000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"kernel",
		size:		0x000C0000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"altkernel",
		size:		0x000C0000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"root",
		size:		0x00400000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"free1",
		size:		0x00300000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"free2",
		size:		0x00300000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}, {
		name:		"free3",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,
	}
};
#endif

#ifdef CONFIG_SA1100_FREEBIRD
#define FREEBIRD_FLASH_SIZE		0x02000000
static struct mtd_partition freebird_partitions[] = {
#if CONFIG_SA1100_FREEBIRD_NEW
	{
		name:		"firmware",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"kernel",
		size:		0x00080000,
		offset:		0x00040000,
	}, {
		name:		"params",
		size:		0x00040000,
		offset:		0x000C0000,
	}, {
		name:		"initrd",
		size:		0x00100000,
		offset:		0x00100000,
	}, {
		name:		"root cramfs",
		size:		0x00300000,
		offset:		0x00200000,
	}, {
		name:		"usr cramfs",
		size:		0x00C00000,
		offset:		0x00500000,
	}, {
		name:		"local",
		size:		MTDPART_SIZ_FULL,
		offset:		0x01100000,
	}
#else
	{
		size:		0x00040000,
		offset:		0,
	}, {
		size:		0x000c0000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		size:		0x00400000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
#endif
};
#endif

#ifdef CONFIG_SA1100_FRODO
/* Frodo has 2 x 16M 28F128J3A flash chips in bank 0: */
#define FRODO_FLASH_SIZE		0x02000000
static struct mtd_partition frodo_partitions[] =
{
	{
		name:		"Boot Loader",
		size:		0x00040000,
		offset:		0x00000000
	}, {
		name:		"Parameter Block",
		size:		0x00040000,
		offset:		MTDPART_OFS_APPEND
	}, {
		name:		"Linux Kernel",
		size:		0x00100000,
		offset:		MTDPART_OFS_APPEND
	}, {
		name:		"Ramdisk",
		size:		0x00680000,
		offset:		MTDPART_OFS_APPEND
	}, {
		name:		"Flash File System",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND
	}
};
#endif

#ifdef CONFIG_SA1100_GRAPHICSCLIENT
#define GRAPHICSCLIENT_FLASH_SIZE	0x02000000
static struct mtd_partition graphicsclient_partitions[] = {
	{
		name:		"zImage",
		size:		0x100000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"ramdisk.gz",
		size:		0x300000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"User FS",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#ifdef CONFIG_SA1100_GRAPHICSMASTER
#define GRAPHICSMASTER_FLASH_SIZE	0x01000000
static struct mtd_partition graphicsmaster_partitions[] = {
	{
		name:		"zImage",
		size:		0x100000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	},
	{
		name:		"ramdisk.gz",
		size:		0x300000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	},
	{
		name:		"User FS",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#ifdef CONFIG_SA1100_H3600
#define H3600_FLASH_SIZE		0x02000000
static struct mtd_partition h3600_partitions[] = {
	{
		name:		"H3600 boot firmware",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"H3600 kernel",
		size:		0x00080000,
		offset:		0x00040000,
	}, {
		name:		"H3600 params",
		size:		0x00040000,
		offset:		0x000C0000,
	}, {
#ifdef CONFIG_JFFS2_FS
		name:		"H3600 root jffs2",
		size:		MTDPART_SIZ_FULL,
		offset:		0x00100000,
#else
		name:		"H3600 initrd",
		size:		0x00100000,
		offset:		0x00100000,
	}, {
		name:		"H3600 root cramfs",
		size:		0x00300000,
		offset:		0x00200000,
	}, {
		name:		"H3600 usr cramfs",
		size:		0x00800000,
		offset:		0x00500000,
	}, {
		name:		"H3600 usr local",
		size:		MTDPART_SIZ_FULL,
		offset:		0x00d00000,
#endif
	}
};

static void h3600_set_vpp(struct map_info *map, int vpp)
{
	assign_h3600_egpio(IPAQ_EGPIO_VPP_ON, vpp);
}
#endif

#ifdef CONFIG_SA1100_HUW_WEBPANEL
#define HUW_WEBPANEL_FLASH_SIZE		0x01000000
static struct mtd_partition huw_webpanel_partitions[] = {
	{
		name:		"Loader",
		size:		0x00040000,
		offset:		0,
	}, {
		name:		"Sector 1",
		size:		0x00040000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#ifdef CONFIG_SA1100_JORNADA720
#define JORNADA720_FLASH_SIZE		0x02000000
static struct mtd_partition jornada720_partitions[] = {
	{
		name:		"JORNADA720 boot firmware",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"JORNADA720 kernel",
		size:		0x000c0000,
		offset:		0x00040000,
	}, {
		name:		"JORNADA720 params",
		size:		0x00040000,
		offset:		0x00100000,
	}, {
		name:		"JORNADA720 initrd",
		size:		0x00100000,
		offset:		0x00140000,
	}, {
		name:		"JORNADA720 root cramfs",
		size:		0x00300000,
		offset:		0x00240000,
	}, {
		name:		"JORNADA720 usr cramfs",
		size:		0x00800000,
		offset:		0x00540000,
	}, {
		name:		"JORNADA720 usr local",
		size:		0  /* will expand to the end of the flash */
		offset:		0x00d00000,
	}
};

static void jornada720_set_vpp(int vpp)
{
	if (vpp)
		PPSR |= 0x80;
	else
		PPSR &= ~0x80;
	PPDR |= 0x80;
}

#endif

#ifdef CONFIG_SA1100_PANGOLIN
#define PANGOLIN_FLASH_SIZE		0x04000000
static struct mtd_partition pangolin_partitions[] = {
	{
		name:		"boot firmware",
		size:		0x00080000,
		offset:		0x00000000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"kernel",
		size:		0x00100000,
		offset:		0x00080000,
	}, {
		name:		"initrd",
		size:		0x00280000,
		offset:		0x00180000,
	}, {
		name:		"initrd-test",
		size:		0x03C00000,
		offset:		0x00400000,
	}
};
#endif

#ifdef CONFIG_SA1100_PT_SYSTEM3
/* erase size is 0x40000 == 256k partitions have to have this boundary */
#define SYSTEM3_FLASH_SIZE		0x01000000
static struct mtd_partition system3_partitions[] = {
	{
		name:		"BLOB",
		size:		0x00040000,
		offset:		0x00000000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"config",
		size:		0x00040000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		name:		"kernel",
		size:		0x00100000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		name:		"root",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#ifdef CONFIG_SA1100_SHANNON
#define SHANNON_FLASH_SIZE		0x00400000
static struct mtd_partition shannon_partitions[] = {
	{
		name: "BLOB boot loader",
		offset: 0,
		size: 0x20000
	},
	{
		name: "kernel",
		offset: MTDPART_OFS_APPEND,
		size: 0xe0000
	},
	{ 
		name: "initrd",
		offset: MTDPART_OFS_APPEND,	
		size: MTDPART_SIZ_FULL
	}
};

#endif

#ifdef CONFIG_SA1100_SHERMAN
#define SHERMAN_FLASH_SIZE		0x02000000
static struct mtd_partition sherman_partitions[] = {
	{
		size:		0x50000,
		offset:		0,
	}, {
		size:		0x70000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		size:		0x600000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		size:		0xA0000,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#ifdef CONFIG_SA1100_SIMPAD
#define SIMPAD_FLASH_SIZE		0x02000000
static struct mtd_partition simpad_partitions[] = {
	{
		name:		"SIMpad boot firmware",
		size:		0x00080000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"SIMpad kernel",
		size:		0x00100000,
		offset:		0x00080000,
	}, {
#ifdef CONFIG_JFFS2_FS
		name:		"SIMpad root jffs2",
		size:		MTDPART_SIZ_FULL,
		offset:		0x00180000,
#else
		name:		"SIMpad initrd",
		size:		0x00300000,
		offset:		0x00180000,
	}, {
		name:		"SIMpad root cramfs",
		size:		0x00300000,
		offset:		0x00480000,
	}, {
		name:		"SIMpad usr cramfs",
		size:		0x005c0000,
		offset:		0x00780000,
	}, {
		name:		"SIMpad usr local",
		size:		MTDPART_SIZ_FULL,
		offset:		0x00d40000,
#endif
	}
};
#endif /* CONFIG_SA1100_SIMPAD */

#ifdef CONFIG_SA1100_STORK
#define STORK_FLASH_SIZE		0x02000000
static struct mtd_partition stork_partitions[] = {
	{
		name:		"STORK boot firmware",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"STORK params",
		size:		0x00040000,
		offset:		0x00040000,
	}, {
		name:		"STORK kernel",
		size:		0x00100000,
		offset:		0x00080000,
	}, {
#ifdef CONFIG_JFFS2_FS
		name:		"STORK root jffs2",
		offset:		0x00180000,
		size:		MTDPART_SIZ_FULL,
#else
		name:		"STORK initrd",
		size:		0x00100000,
		offset:		0x00180000,
	}, {
		name:		"STORK root cramfs",
		size:		0x00300000,
		offset:		0x00280000,
	}, {
		name:		"STORK usr cramfs",
		size:		0x00800000,
		offset:		0x00580000,
	}, {
		name:		"STORK usr local",
		offset:		0x00d80000,
		size:		MTDPART_SIZ_FULL,
#endif
	}
};
#endif

#ifdef CONFIG_SA1100_YOPY
#define YOPY_FLASH_SIZE			0x08000000
static struct mtd_partition yopy_partitions[] = {
	{
		name:		"boot firmware",
		size:		0x00040000,
		offset:		0x00000000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"kernel",
		size:		0x00080000,
		offset:		0x00080000,
	}, {
		name:		"initrd",
		size:		0x00300000,
		offset:		0x00100000,
	}, {
		name:		"root",
		size:		0x01000000,
		offset:		0x00400000,
	}
};
#endif

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
extern int parse_bootldr_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static struct mtd_partition *parsed_parts;
static struct mtd_info *mymtd;

int __init sa1100_mtd_init(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0, ret;
	int parsed_nr_parts = 0;
	const char *part_type;
	unsigned long base = -1UL;

	/* Default flash buswidth */
	sa1100_map.buswidth = (MSC0 & MSC_RBW) ? 2 : 4;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";

#ifdef CONFIG_SA1100_ADSBITSY
	if (machine_is_adsbitsy()) {
		parts = adsbitsy_partitions;
		nb_parts = ARRAY_SIZE(adsbitsy_partitions);
		sa1100_map.size = ADSBITSY_FLASH_SIZE;
		sa1100_map.buswidth = (MSC1 & MSC_RBW) ? 2 : 4;
	}
#endif
#ifdef CONFIG_SA1100_ASSABET
	if (machine_is_assabet()) {
		parts = assabet_partitions;
		nb_parts = ARRAY_SIZE(assabet_partitions);
		sa1100_map.size = ASSABET_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_BADGE4
	if (machine_is_badge4()) {
		parts = badge4_partitions;
		nb_parts = ARRAY_SIZE(badge4_partitions);
		sa1100_map.size = BADGE4_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_CERF
	if (machine_is_cerf()) {
		parts = cerf_partitions;
		nb_parts = ARRAY_SIZE(cerf_partitions);
		sa1100_map.size = CERF_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_CONSUS
	if (machine_is_consus()) {
		parts = consus_partitions;
		nb_parts = ARRAY_SIZE(consus_partitions);
		sa1100_map.size = CONSUS_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_FLEXANET
	if (machine_is_flexanet()) {
		parts = flexanet_partitions;
		nb_parts = ARRAY_SIZE(flexanet_partitions);
		sa1100_map.size = FLEXANET_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_FREEBIRD
	if (machine_is_freebird()) {
		parts = freebird_partitions;
		nb_parts = ARRAY_SIZE(freebird_partitions);
		sa1100_map.size = FREEBIRD_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_FRODO
	if (machine_is_frodo()) {
		parts = frodo_partitions;
		nb_parts = ARRAY_SIZE(frodo_partitions);
		sa1100_map.size = FRODO_FLASH_SIZE;
		base = 0x00000000;
	}
#endif
#ifdef CONFIG_SA1100_GRAPHICSCLIENT
	if (machine_is_graphicsclient()) {
		parts = graphicsclient_partitions;
		nb_parts = ARRAY_SIZE(graphicsclient_partitions);
		sa1100_map.size = GRAPHICSCLIENT_FLASH_SIZE;
		sa1100_map.buswidth = (MSC1 & MSC_RBW) ? 2:4;
	}
#endif
#ifdef CONFIG_SA1100_GRAPHICSMASTER
	if (machine_is_graphicsmaster()) {
		parts = graphicsmaster_partitions;
		nb_parts = ARRAY_SIZE(graphicsmaster_partitions);
		sa1100_map.size = GRAPHICSMASTER_FLASH_SIZE;
		sa1100_map.buswidth = (MSC1 & MSC_RBW) ? 2:4;
	}
#endif
#ifdef CONFIG_SA1100_H3600
	if (machine_is_h3600()) {
		parts = h3600_partitions;
		nb_parts = ARRAY_SIZE(h3600_partitions);
		sa1100_map.size = H3600_FLASH_SIZE;
		sa1100_map.set_vpp = h3600_set_vpp;
	}
#endif
#ifdef CONFIG_SA1100_HUW_WEBPANEL
	if (machine_is_huw_webpanel()) {
		parts = huw_webpanel_partitions;
		nb_parts = ARRAY_SIZE(huw_webpanel_partitions);
		sa1100_map.size = HUW_WEBPANEL_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_JORNADA720
	if (machine_is_jornada720()) {
		parts = jornada720_partitions;
		nb_parts = ARRAY_SIZE(jornada720_partitions);
		sa1100_map.size = JORNADA720_FLASH_SIZE;
		sa1100_map.set_vpp = jornada720_set_vpp;
	}
#endif
#ifdef CONFIG_SA1100_PANGOLIN
	if (machine_is_pangolin()) {
		parts = pangolin_partitions;
		nb_parts = ARRAY_SIZE(pangolin_partitions);
		sa1100_map.size = PANGOLIN_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_PT_SYSTEM3
	if (machine_is_pt_system3()) {
		parts = system3_partitions;
		nb_parts = ARRAY_SIZE(system3_partitions);
		sa1100_map.size = SYSTEM3_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_SHANNON
	if (machine_is_shannon()) {
		parts = shannon_partitions;
		nb_parts = ARRAY_SIZE(shannon_partitions);
		sa1100_map.size = SHANNON_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_SHERMAN
	if (machine_is_sherman()) {
		parts = sherman_partitions;
		nb_parts = ARRAY_SIZE(sherman_partitions);
		sa1100_map.size = SHERMAN_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_SIMPAD
	if (machine_is_simpad()) {
		parts = simpad_partitions;
		nb_parts = ARRAY_SIZE(simpad_partitions);
		sa1100_map.size = SIMPAD_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_STORK
	if (machine_is_stork()) {
		parts = stork_partitions;
		nb_parts = ARRAY_SIZE(stork_partitions);
		sa1100_map.size = STORK_FLASH_SIZE;
	}
#endif
#ifdef CONFIG_SA1100_YOPY
	if (machine_is_yopy()) {
		parts = yopy_partitions;
		nb_parts = ARRAY_SIZE(yopy_partitions);
		sa1100_map.size = YOPY_FLASH_SIZE;
	}
#endif

	/*
	 * For simple flash devices, use ioremap to map the flash.
	 */
	if (base != (unsigned long)-1) {
		if (!request_mem_region(base, sa1100_map.size, "flash"))
			return -EBUSY;
		sa1100_map.map_priv_2 = base;
		sa1100_map.map_priv_1 = (unsigned long)
				ioremap(base, sa1100_map.size);
		ret = -ENOMEM;
		if (!sa1100_map.map_priv_1)
			goto out_err;
	}

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "SA1100 flash: probing %d-bit flash bus\n", sa1100_map.buswidth*8);
	mymtd = do_map_probe("cfi_probe", &sa1100_map);
	ret = -ENXIO;
	if (!mymtd)
		goto out_err;
	mymtd->module = THIS_MODULE;

	/*
	 * Dynamic partition selection stuff (might override the static ones)
	 */
#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif
#ifdef CONFIG_MTD_CMDLINE_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_cmdline_partitions(mymtd, &parsed_parts, "sa1100");
		if (ret > 0) {
			part_type = "Command Line";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	}

	if (nb_parts == 0) {
		printk(KERN_NOTICE "SA1100 flash: no partition info available, registering whole flash at once\n");
		add_mtd_device(mymtd);
	} else {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	}
	return 0;

 out_err:
	if (sa1100_map.map_priv_2 != -1) {
		iounmap((void *)sa1100_map.map_priv_1);
		release_mem_region(sa1100_map.map_priv_2, sa1100_map.size);
	}
	return ret;
}

static void __exit sa1100_mtd_cleanup(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (sa1100_map.map_priv_2 != -1) {
		iounmap((void *)sa1100_map.map_priv_1);
		release_mem_region(sa1100_map.map_priv_2, sa1100_map.size);
	}
}

module_init(sa1100_mtd_init);
module_exit(sa1100_mtd_cleanup);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("SA1100 CFI map driver");
MODULE_LICENSE("GPL");
