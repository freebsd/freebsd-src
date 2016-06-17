/*
 *  arch/ppc/platforms/setup.c
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Adapted for Power Macintosh by Paul Mackerras
 *    Copyright (C) 1996 Paul Mackerras (paulus@cs.anu.edu.au)
 *
 *  Derived from "arch/alpha/kernel/setup.c"
 *    Copyright (C) 1995 Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

/*
 * bootup setup stuff..
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/major.h>
#include <linux/blk.h>
#include <linux/vt_kern.h>
#include <linux/console.h>
#include <linux/ide.h>
#include <linux/pci.h>
#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>
#include <linux/irq.h>
#include <linux/seq_file.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>

#include <asm/processor.h>
#include <asm/sections.h>
#include <asm/prom.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/pci-bridge.h>
#include <asm/ohare.h>
#include <asm/mediabay.h>
#include <asm/machdep.h>
#include <asm/keyboard.h>
#include <asm/dma.h>
#include <asm/bootx.h>
#include <asm/cputable.h>
#include <asm/btext.h>
#include <asm/pmac_feature.h>
#include <asm/time.h>

#include "pmac_pic.h"
#include "mem_pieces.h"
#include "scsi.h" /* sd_find_target */
#include "sd.h"
#include "mac.h"

extern long pmac_time_init(void);
extern unsigned long pmac_get_rtc_time(void);
extern int pmac_set_rtc_time(unsigned long nowtime);
extern void pmac_read_rtc_time(void);
extern void pmac_calibrate_decr(void);
extern void pmac_pcibios_fixup(void);
extern void pmac_find_bridges(void);
extern ide_ioreg_t pmac_ide_get_base(int index);
extern void pmac_ide_init_hwif_ports(hw_regs_t *hw,
	ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq);

extern int mackbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int mackbd_getkeycode(unsigned int scancode);
extern int mackbd_translate(unsigned char keycode, unsigned char *keycodep,
		     char raw_mode);
extern char mackbd_unexpected_up(unsigned char keycode);
extern void mackbd_leds(unsigned char leds);
extern void __init mackbd_init_hw(void);
extern int mac_hid_kbd_translate(unsigned char scancode, unsigned char *keycode,
				 char raw_mode);
extern char mac_hid_kbd_unexpected_up(unsigned char keycode);
extern void mac_hid_init_hw(void);
extern unsigned char mac_hid_kbd_sysrq_xlate[];
extern unsigned char pckbd_sysrq_xlate[];
extern unsigned char mackbd_sysrq_xlate[];
extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern int keyboard_sends_linux_keycodes;
extern void pmac_nvram_update(void);

extern int pmac_pci_enable_device_hook(struct pci_dev *dev, int initial);
extern void pmac_pcibios_after_init(void);

struct device_node *memory_node;

unsigned char drive_info;

int ppc_override_l2cr = 0;
int ppc_override_l2cr_value;
int has_l2cache = 0;

static int current_root_goodness = -1;

extern char saved_command_line[];

extern int pmac_newworld;

#define DEFAULT_ROOT_DEVICE 0x0801	/* sda1 - slightly silly choice */

extern void zs_kgdb_hook(int tty_num);
static void ohare_init(void);
#ifdef CONFIG_BOOTX_TEXT
void pmac_progress(char *s, unsigned short hex);
#endif

sys_ctrler_t sys_ctrler = SYS_CTRLER_UNKNOWN;

#ifdef CONFIG_SMP
extern struct smp_ops_t psurge_smp_ops;
extern struct smp_ops_t core99_smp_ops;
#endif /* CONFIG_SMP */

/*
 * Assume here that all clock rates are the same in a
 * smp system.  -- Cort
 */
int __openfirmware
of_show_percpuinfo(struct seq_file *m, int i)
{
	struct device_node *cpu_node;
	int *fp, s;

	cpu_node = find_type_devices("cpu");
	if (!cpu_node)
		return 0;
	for (s = 0; s < i && cpu_node->next; s++)
		cpu_node = cpu_node->next;
	fp = (int *) get_property(cpu_node, "clock-frequency", NULL);
	if (fp)
		seq_printf(m, "clock\t\t: %dMHz\n", *fp / 1000000);
	return 0;
}

int __pmac
pmac_show_cpuinfo(struct seq_file *m)
{
	struct device_node *np;
	char *pp;
	int plen;
	int mbmodel = pmac_call_feature(PMAC_FTR_GET_MB_INFO,
		NULL, PMAC_MB_INFO_MODEL, 0);
	unsigned int mbflags = (unsigned int)pmac_call_feature(PMAC_FTR_GET_MB_INFO,
		NULL, PMAC_MB_INFO_FLAGS, 0);
	char* mbname;

	if (pmac_call_feature(PMAC_FTR_GET_MB_INFO, NULL, PMAC_MB_INFO_NAME, (int)&mbname) != 0)
		mbname = "Unknown";

	/* find motherboard type */
	seq_printf(m, "machine\t\t: ");
	np = find_devices("device-tree");
	if (np != NULL) {
		pp = (char *) get_property(np, "model", NULL);
		if (pp != NULL)
			seq_printf(m, "%s\n", pp);
		else
			seq_printf(m, "PowerMac\n");
		pp = (char *) get_property(np, "compatible", &plen);
		if (pp != NULL) {
			seq_printf(m, "motherboard\t:");
			while (plen > 0) {
				int l = strlen(pp) + 1;
				seq_printf(m, " %s", pp);
				plen -= l;
				pp += l;
			}
			seq_printf(m, "\n");
		}
	} else
		seq_printf(m, "PowerMac\n");

	/* print parsed model */
	seq_printf(m, "detected as\t: %d (%s)\n", mbmodel, mbname);
	seq_printf(m, "pmac flags\t: %08x\n", mbflags);

	/* find l2 cache info */
	np = find_devices("l2-cache");
	if (np == 0)
		np = find_type_devices("cache");
	if (np != 0) {
		unsigned int *ic = (unsigned int *)
			get_property(np, "i-cache-size", NULL);
		unsigned int *dc = (unsigned int *)
			get_property(np, "d-cache-size", NULL);
		seq_printf(m, "L2 cache\t:");
		has_l2cache = 1;
		if (get_property(np, "cache-unified", NULL) != 0 && dc) {
			seq_printf(m, " %dK unified", *dc / 1024);
		} else {
			if (ic)
				seq_printf(m, " %dK instruction", *ic / 1024);
			if (dc)
				seq_printf(m, "%s %dK data",
					   (ic? " +": ""), *dc / 1024);
		}
		pp = get_property(np, "ram-type", NULL);
		if (pp)
			seq_printf(m, " %s", pp);
		seq_printf(m, "\n");
	}

	/* find ram info */
	np = find_devices("memory");
	if (np != 0) {
		int n;
		struct reg_property *reg = (struct reg_property *)
			get_property(np, "reg", &n);

		if (reg != 0) {
			unsigned long total = 0;

			for (n /= sizeof(struct reg_property); n > 0; --n)
				total += (reg++)->size;
			seq_printf(m, "memory\t\t: %luMB\n", total >> 20);
		}
	}

	/* Checks "l2cr-value" property in the registry */
	np = find_devices("cpus");
	if (np == 0)
		np = find_type_devices("cpu");
	if (np != 0) {
		unsigned int *l2cr = (unsigned int *)
			get_property(np, "l2cr-value", NULL);
		if (l2cr != 0) {
			seq_printf(m, "l2cr override\t: 0x%x\n", *l2cr);
		}
	}

	/* Indicate newworld/oldworld */
	seq_printf(m, "pmac-generation\t: %s\n",
		   pmac_newworld ? "NewWorld" : "OldWorld");


	return 0;
}

#ifdef CONFIG_VT
/*
 * Dummy mksound function that does nothing.
 * The real one is in the dmasound driver.
 */
static void __pmac
pmac_mksound(unsigned int hz, unsigned int ticks)
{
}
#endif /* CONFIG_VT */

static volatile u32 *sysctrl_regs;

void __init
pmac_setup_arch(void)
{
	struct device_node *cpu;
	int *fp;
	unsigned long pvr;

	pvr = PVR_VER(mfspr(PVR));

	/* Set loops_per_jiffy to a half-way reasonable value,
	   for use until calibrate_delay gets called. */
	cpu = find_type_devices("cpu");
	if (cpu != 0) {
		fp = (int *) get_property(cpu, "clock-frequency", NULL);
		if (fp != 0) {
			if (pvr == 4 || pvr >= 8)
				/* 604, G3, G4 etc. */
				loops_per_jiffy = *fp / HZ;
			else
				/* 601, 603, etc. */
				loops_per_jiffy = *fp / (2*HZ);
		} else
			loops_per_jiffy = 50000000 / HZ;
	}

	/* this area has the CPU identification register
	   and some registers used by smp boards */
	sysctrl_regs = (volatile u32 *) ioremap(0xf8000000, 0x1000);
	ohare_init();

	/* Lookup PCI hosts */
	pmac_find_bridges();

	/* Checks "l2cr-value" property in the registry */
	if (cur_cpu_spec[0]->cpu_features & CPU_FTR_L2CR) {
		struct device_node *np = find_devices("cpus");
		if (np == 0)
			np = find_type_devices("cpu");
		if (np != 0) {
			unsigned int *l2cr = (unsigned int *)
				get_property(np, "l2cr-value", NULL);
			if (l2cr != 0) {
				ppc_override_l2cr = 1;
				ppc_override_l2cr_value = *l2cr;
				_set_L2CR(0);
				_set_L2CR(ppc_override_l2cr_value);
			}
		}
	}

	if (ppc_override_l2cr)
		printk(KERN_INFO "L2CR overriden (0x%x), backside cache is %s\n",
			ppc_override_l2cr_value, (ppc_override_l2cr_value & 0x80000000)
				? "enabled" : "disabled");

#ifdef CONFIG_KGDB
	zs_kgdb_hook(0);
#endif

#ifdef CONFIG_ADB_CUDA
	find_via_cuda();
#else
	if (find_devices("via-cuda")) {
		printk("WARNING ! Your machine is Cuda based but your kernel\n");
		printk("          wasn't compiled with CONFIG_ADB_CUDA option !\n");
	}
#endif
#ifdef CONFIG_ADB_PMU
	find_via_pmu();
#else
	if (find_devices("via-pmu")) {
		printk("WARNING ! Your machine is PMU based but your kernel\n");
		printk("          wasn't compiled with CONFIG_ADB_PMU option !\n");
	}
#endif
#ifdef CONFIG_NVRAM
	pmac_nvram_init();
#endif
#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif
#ifdef CONFIG_VT
	kd_mksound = pmac_mksound;
#endif
#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	else
#endif
		ROOT_DEV = to_kdev_t(DEFAULT_ROOT_DEVICE);

#ifdef CONFIG_SMP
	/* Check for Core99 */
	if (find_devices("uni-n"))
		ppc_md.smp_ops = &core99_smp_ops;
	else
		ppc_md.smp_ops = &psurge_smp_ops;
#endif /* CONFIG_SMP */

	pci_create_OF_bus_map();
}

static void __init ohare_init(void)
{
	/*
	 * Turn on the L2 cache.
	 * We assume that we have a PSX memory controller iff
	 * we have an ohare I/O controller.
	 */
	if (find_devices("ohare") != NULL) {
		if (((sysctrl_regs[2] >> 24) & 0xf) >= 3) {
			if (sysctrl_regs[4] & 0x10)
				sysctrl_regs[4] |= 0x04000020;
			else
				sysctrl_regs[4] |= 0x04000000;
			if(has_l2cache)
				printk(KERN_INFO "Level 2 cache enabled\n");
		}
	}
}

extern char *bootpath;
extern char *bootdevice;
void *boot_host;
int boot_target;
int boot_part;
extern kdev_t boot_dev;

void __init
pmac_init2(void)
{
#ifdef CONFIG_ADB_PMU
	via_pmu_start();
#endif
#ifdef CONFIG_ADB_CUDA
	via_cuda_start();
#endif
#ifdef CONFIG_PMAC_PBOOK
	media_bay_init();
#endif
	pmac_feature_late_init();
}

/* Borrowed from fs/partition/check.c */
static unsigned char* __init
read_one_block(struct block_device *bdev, unsigned long n, struct page **v)
{
	struct address_space *mapping = bdev->bd_inode->i_mapping;
	int sect = PAGE_CACHE_SIZE / 512;
	struct page *page;

	page = read_cache_page(mapping, n/sect,
			(filler_t *)mapping->a_ops->readpage, NULL);
	if (!IS_ERR(page)) {
		wait_on_page(page);
		if (!Page_Uptodate(page))
			goto fail;
		if (PageError(page))
			goto fail;
		*v = page;
		return (unsigned char *)page_address(page) + 512 * (n % sect);
fail:
		page_cache_release(page);
	}
	*v = NULL;
	return NULL;
}

#ifdef CONFIG_SCSI
void __init
note_scsi_host(struct device_node *node, void *host)
{
	int l;
	char *p;

	l = strlen(node->full_name);
	if (bootpath != NULL && bootdevice != NULL
	    && strncmp(node->full_name, bootdevice, l) == 0
	    && (bootdevice[l] == '/' || bootdevice[l] == 0)) {
		boot_host = host;
		/*
		 * There's a bug in OF 1.0.5.  (Why am I not surprised.)
		 * If you pass a path like scsi/sd@1:0 to canon, it returns
		 * something like /bandit@F2000000/gc@10/53c94@10000/sd@0,0
		 * That is, the scsi target number doesn't get preserved.
		 * So we pick the target number out of bootpath and use that.
		 */
		p = strstr(bootpath, "/sd@");
		if (p != NULL) {
			p += 4;
			boot_target = simple_strtoul(p, NULL, 10);
			p = strchr(p, ':');
			if (p != NULL)
				boot_part = simple_strtoul(p + 1, NULL, 10);
		}
	}
}
#endif /* CONFIG_SCSI */

#if defined(CONFIG_BLK_DEV_IDE) && defined(CONFIG_BLK_DEV_IDE_PMAC)
kdev_t __init
find_ide_boot(void)
{
	char *p;
	int n;
	kdev_t __init pmac_find_ide_boot(char *bootdevice, int n);

	if (bootdevice == NULL)
		return 0;
	p = strrchr(bootdevice, '/');
	if (p == NULL)
		return 0;
	n = p - bootdevice;

	return pmac_find_ide_boot(bootdevice, n);
}
#endif /* CONFIG_BLK_DEV_IDE && CONFIG_BLK_DEV_IDE_PMAC */

void __init
find_boot_device(void)
{
#if defined(CONFIG_SCSI) && defined(CONFIG_BLK_DEV_SD)
	if (boot_host != NULL) {
		boot_dev = sd_find_target(boot_host, boot_target);
		if (boot_dev != 0)
			return;
	}
#endif
#if defined(CONFIG_BLK_DEV_IDE) && defined(CONFIG_BLK_DEV_IDE_PMAC)
	boot_dev = find_ide_boot();
#endif
}

static void __init
check_bootable_part(kdev_t dev, int blk, struct mac_partition *part)
{
	int goodness = 0;

	macpart_fix_string(part->processor, 16);
	macpart_fix_string(part->name, 32);
	macpart_fix_string(part->type, 32);

	if ((be32_to_cpu(part->status) & MAC_STATUS_BOOTABLE)
	    && strcasecmp(part->processor, "powerpc") == 0)
		goodness++;

	if (strcasecmp(part->type, "Apple_UNIX_SVR2") == 0
	    || (strnicmp(part->type, "Linux", 5) == 0
	    && strcasecmp(part->type, "Linux_swap") != 0)) {
		int i, l;

		goodness++;
		l = strlen(part->name);
		if (strcmp(part->name, "/") == 0)
			goodness++;
		for (i = 0; i <= l - 4; ++i) {
			if (strnicmp(part->name + i, "root",
				     4) == 0) {
				goodness += 2;
				break;
			}
		}
		if (strnicmp(part->name, "swap", 4) == 0)
			goodness--;
	}

	if (goodness > current_root_goodness) {
		ROOT_DEV = MKDEV(MAJOR(dev), MINOR(dev) + blk);
		current_root_goodness = goodness;
	}
}

static void __init
check_bootable_disk(kdev_t dev, struct block_device *bdev)
{
	struct mac_partition *part;
	struct mac_driver_desc *md;
	struct page* pg;
	unsigned secsize, blocks_in_map, blk;
	unsigned char* data;

	/* Check driver descriptor */
	md = (struct mac_driver_desc *) read_one_block(bdev, 0, &pg);
	if (!md)
		return;
	if (be16_to_cpu(md->signature) != MAC_DRIVER_MAGIC)
		goto fail;
	secsize = be16_to_cpu(md->block_size);
	page_cache_release(pg);

	/* Check if it looks like a mac partition map */
	data = read_one_block(bdev, secsize/512, &pg);
	if (!data)
		goto fail;
	part = (struct mac_partition *) (data + secsize%512);
	if (be16_to_cpu(part->signature) != MAC_PARTITION_MAGIC)
		goto fail;

	/* Iterate the partition map */
	blocks_in_map = be32_to_cpu(part->map_count);
	for (blk = 1; blk <= blocks_in_map; ++blk) {
		int pos = blk * secsize;
		page_cache_release(pg);
		data = read_one_block(bdev, pos/512, &pg);
		if (!data)
			break;
		part = (struct mac_partition *) (data + pos%512);
		if (be16_to_cpu(part->signature) != MAC_PARTITION_MAGIC)
			break;
		check_bootable_part(dev, blk, part);
	}
fail:
	if (pg)
		page_cache_release(pg);
}

static int __init
walk_bootable(struct gendisk *hd, void *data)
{
	int drive;

	for (drive=0; drive<hd->nr_real; drive++) {
		kdev_t dev;
		struct block_device *bdev;
		int rc;

		dev = MKDEV(hd->major, drive << hd->minor_shift);
		if (boot_dev && boot_dev != dev)
			continue;
		bdev = bdget(kdev_t_to_nr(dev));
		if (bdev == NULL)
			continue;
		rc = blkdev_get(bdev, FMODE_READ, 0, BDEV_RAW);
		if (rc == 0) {
			check_bootable_disk(dev, bdev);
			blkdev_put(bdev, BDEV_RAW);
		}
	}

	return 0;
}

void __init
pmac_discover_root(void)
{
	char* p;

	/* Check if root devices already got selected by other ways */
	if (ROOT_DEV != to_kdev_t(DEFAULT_ROOT_DEVICE))
		return;
	p = strstr(saved_command_line, "root=");
	if (p != NULL && (p == saved_command_line || p[-1] == ' '))
		return;

	/* Find the device used for booting if we can */
	find_boot_device();

	/* Try to locate a partition */
	walk_gendisk(walk_bootable, NULL);
}

void __pmac
pmac_restart(char *cmd)
{
#ifdef CONFIG_ADB_CUDA
	struct adb_request req;
#endif /* CONFIG_ADB_CUDA */

#ifdef CONFIG_NVRAM
	pmac_nvram_update();
#endif

	switch (sys_ctrler) {
#ifdef CONFIG_ADB_CUDA
	case SYS_CTRLER_CUDA:
		cuda_request(&req, NULL, 2, CUDA_PACKET,
			     CUDA_RESET_SYSTEM);
		for (;;)
			cuda_poll();
		break;
#endif /* CONFIG_ADB_CUDA */
#ifdef CONFIG_ADB_PMU
	case SYS_CTRLER_PMU:
		pmu_restart();
		break;
#endif /* CONFIG_ADB_PMU */
	default: ;
	}
}

void __pmac
pmac_power_off(void)
{
#ifdef CONFIG_ADB_CUDA
	struct adb_request req;
#endif /* CONFIG_ADB_CUDA */

#ifdef CONFIG_NVRAM
	pmac_nvram_update();
#endif

	switch (sys_ctrler) {
#ifdef CONFIG_ADB_CUDA
	case SYS_CTRLER_CUDA:
		cuda_request(&req, NULL, 2, CUDA_PACKET,
			     CUDA_POWERDOWN);
		for (;;)
			cuda_poll();
		break;
#endif /* CONFIG_ADB_CUDA */
#ifdef CONFIG_ADB_PMU
	case SYS_CTRLER_PMU:
		pmu_shutdown();
		break;
#endif /* CONFIG_ADB_PMU */
	default: ;
	}
}

void __pmac
pmac_halt(void)
{
   pmac_power_off();
}


/*
 * Read in a property describing some pieces of memory.
 */

static int __init
get_mem_prop(char *name, struct mem_pieces *mp)
{
	struct reg_property *rp;
	int i, s;
	unsigned int *ip;
	int nac = prom_n_addr_cells(memory_node);
	int nsc = prom_n_size_cells(memory_node);

	ip = (unsigned int *) get_property(memory_node, name, &s);
	if (ip == NULL) {
		printk(KERN_ERR "error: couldn't get %s property on /memory\n",
		       name);
		return 0;
	}
	s /= (nsc + nac) * 4;
	rp = mp->regions;
	for (i = 0; i < s; ++i, ip += nac+nsc) {
		if (nac >= 2 && ip[nac-2] != 0)
			continue;
		rp->address = ip[nac-1];
		if (nsc >= 2 && ip[nac+nsc-2] != 0)
			rp->size = ~0U;
		else
			rp->size = ip[nac+nsc-1];
		++rp;
	}
	mp->n_regions = rp - mp->regions;

	/* Make sure the pieces are sorted. */
	mem_pieces_sort(mp);
	mem_pieces_coalesce(mp);
	return 1;
}

/*
 * On systems with Open Firmware, collect information about
 * physical RAM and which pieces are already in use.
 * At this point, we have (at least) the first 8MB mapped with a BAT.
 * Our text, data, bss use something over 1MB, starting at 0.
 * Open Firmware may be using 1MB at the 4MB point.
 */
unsigned long __init
pmac_find_end_of_memory(void)
{
	unsigned long a, total;
	struct mem_pieces phys_mem;

	/*
	 * Find out where physical memory is, and check that it
	 * starts at 0 and is contiguous.  It seems that RAM is
	 * always physically contiguous on Power Macintoshes.
	 *
	 * Supporting discontiguous physical memory isn't hard,
	 * it just makes the virtual <-> physical mapping functions
	 * more complicated (or else you end up wasting space
	 * in mem_map).
	 */
	memory_node = find_devices("memory");
	if (memory_node == NULL || !get_mem_prop("reg", &phys_mem)
	    || phys_mem.n_regions == 0)
		panic("No RAM??");
	a = phys_mem.regions[0].address;
	if (a != 0)
		panic("RAM doesn't start at physical address 0");
	total = phys_mem.regions[0].size;

	if (phys_mem.n_regions > 1) {
		printk("RAM starting at 0x%x is not contiguous\n",
		       phys_mem.regions[1].address);
		printk("Using RAM from 0 to 0x%lx\n", total-1);
	}

	return total;
}

void __init
select_adb_keyboard(void)
{
#ifdef CONFIG_VT
#ifdef CONFIG_INPUT
	ppc_md.kbd_init_hw       = mac_hid_init_hw;
	ppc_md.kbd_translate     = mac_hid_kbd_translate;
	ppc_md.kbd_unexpected_up = mac_hid_kbd_unexpected_up;
	ppc_md.kbd_setkeycode    = 0;
	ppc_md.kbd_getkeycode    = 0;
	ppc_md.kbd_leds		 = 0;
#ifdef CONFIG_MAGIC_SYSRQ
#ifdef CONFIG_MAC_ADBKEYCODES
	if (!keyboard_sends_linux_keycodes) {
		ppc_md.ppc_kbd_sysrq_xlate = mac_hid_kbd_sysrq_xlate;
		SYSRQ_KEY = 0x69;
	} else
#endif /* CONFIG_MAC_ADBKEYCODES */
	{
		ppc_md.ppc_kbd_sysrq_xlate = pckbd_sysrq_xlate;
		SYSRQ_KEY = 0x54;
	}
#endif /* CONFIG_MAGIC_SYSRQ */
#elif defined(CONFIG_ADB_KEYBOARD)
	ppc_md.kbd_setkeycode       = mackbd_setkeycode;
	ppc_md.kbd_getkeycode       = mackbd_getkeycode;
	ppc_md.kbd_translate        = mackbd_translate;
	ppc_md.kbd_unexpected_up    = mackbd_unexpected_up;
	ppc_md.kbd_leds             = mackbd_leds;
	ppc_md.kbd_init_hw          = mackbd_init_hw;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate  = mackbd_sysrq_xlate;
	SYSRQ_KEY = 0x69;
#endif /* CONFIG_MAGIC_SYSRQ */
#endif /* CONFIG_INPUT_ADBHID/CONFIG_ADB_KEYBOARD */
#endif /* CONFIG_VT */
}

void __init
pmac_init(unsigned long r3, unsigned long r4, unsigned long r5,
	  unsigned long r6, unsigned long r7)
{
	/* isa_io_base gets set in pmac_find_bridges */
	isa_mem_base = PMAC_ISA_MEM_BASE;
	pci_dram_offset = PMAC_PCI_DRAM_OFFSET;
	ISA_DMA_THRESHOLD = ~0L;
	DMA_MODE_READ = 1;
	DMA_MODE_WRITE = 2;

	ppc_md.setup_arch     = pmac_setup_arch;
	ppc_md.show_cpuinfo   = pmac_show_cpuinfo;
	ppc_md.show_percpuinfo = of_show_percpuinfo;
	ppc_md.irq_cannonicalize = NULL;
	ppc_md.init_IRQ       = pmac_pic_init;
	ppc_md.get_irq        = pmac_get_irq; /* Changed later on ... */
	ppc_md.init           = pmac_init2;

	ppc_md.pcibios_fixup  = pmac_pcibios_fixup;
	ppc_md.pcibios_enable_device_hook = pmac_pci_enable_device_hook;
	ppc_md.pcibios_after_init = pmac_pcibios_after_init;

	ppc_md.discover_root  = pmac_discover_root;

	ppc_md.restart        = pmac_restart;
	ppc_md.power_off      = pmac_power_off;
	ppc_md.halt           = pmac_halt;

	ppc_md.time_init      = pmac_time_init;
	ppc_md.set_rtc_time   = pmac_set_rtc_time;
	ppc_md.get_rtc_time   = pmac_get_rtc_time;
	ppc_md.calibrate_decr = pmac_calibrate_decr;

	ppc_md.find_end_of_memory = pmac_find_end_of_memory;

	ppc_md.feature_call   = pmac_do_feature_call;

	select_adb_keyboard();

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
#ifdef CONFIG_BLK_DEV_IDE_PMAC
        ppc_ide_md.ide_init_hwif	= pmac_ide_init_hwif_ports;
        ppc_ide_md.default_io_base	= pmac_ide_get_base;
#endif /* CONFIG_BLK_DEV_IDE_PMAC */
#endif /* defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE) */

#ifdef CONFIG_BOOTX_TEXT
	ppc_md.progress = pmac_progress;
#endif /* CONFIG_BOOTX_TEXT */

	if (ppc_md.progress) ppc_md.progress("pmac_init(): exit", 0);

}

#ifdef CONFIG_BOOTX_TEXT
void __init
pmac_progress(char *s, unsigned short hex)
{
	if (boot_text_mapped) {
		btext_drawstring(s);
		btext_drawchar('\n');
	}
}
#endif /* CONFIG_BOOTX_TEXT */
