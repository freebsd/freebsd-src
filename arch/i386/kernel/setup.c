/*
 *  linux/arch/i386/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  Enhanced CPU type detection by Mike Jagdis, Patrick St. Jean
 *  and Martin Mares, November 1997.
 *
 *  Force Cyrix 6x86(MX) and M II processors to report MTRR capability
 *  and Cyrix "coma bug" recognition by
 *      Zoltán Böszörményi <zboszor@mail.externet.hu> February 1999.
 * 
 *  Force Centaur C6 processors to report MTRR capability.
 *      Bart Hartgers <bart@etpmod.phys.tue.nl>, May 1999.
 *
 *  Intel Mobile Pentium II detection fix. Sean Gilley, June 1999.
 *
 *  IDT Winchip tweaks, misc clean ups.
 *	Dave Jones <davej@suse.de>, August 1999
 *
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *
 *  Better detection of Centaur/IDT WinChip models.
 *      Bart Hartgers <bart@etpmod.phys.tue.nl>, August 1999.
 *
 *  Memory region support
 *	David Parsons <orc@pell.chi.il.us>, July-August 1999
 *
 *  Cleaned up cache-detection code
 *	Dave Jones <davej@suse.de>, October 1999
 *
 *	Added proper L2 cache detection for Coppermine
 *	Dragan Stancevic <visitor@valinux.com>, October 1999
 *
 *  Added the original array for capability flags but forgot to credit 
 *  myself :) (~1998) Fixed/cleaned up some cpu_model_info and other stuff
 *  	Jauder Ho <jauderho@carumba.com>, January 2000
 *
 *  Detection for Celeron coppermine, identify_cpu() overhauled,
 *  and a few other clean ups.
 *  Dave Jones <davej@suse.de>, April 2000
 *
 *  Pentium III FXSR, SSE support
 *  General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 *
 *  Added proper Cascades CPU and L2 cache detection for Cascades
 *  and 8-way type cache happy bunch from Intel:^)
 *  Dragan Stancevic <visitor@valinux.com>, May 2000 
 *
 *  Forward port AMD Duron errata T13 from 2.2.17pre
 *  Dave Jones <davej@suse.de>, August 2000
 *
 *  Forward port lots of fixes/improvements from 2.2.18pre
 *  Cyrix III, Pentium IV support.
 *  Dave Jones <davej@suse.de>, October 2000
 *
 *  Massive cleanup of CPU detection and bug handling;
 *  Transmeta CPU detection,
 *  H. Peter Anvin <hpa@zytor.com>, November 2000
 *
 *  Added E820 sanitization routine (removes overlapping memory regions);
 *  Brian Moyle <bmoyle@mvista.com>, February 2001
 *
 *  VIA C3 Support.
 *  Dave Jones <davej@suse.de>, March 2001
 *
 *  AMD Athlon/Duron/Thunderbird bluesmoke support.
 *  Dave Jones <davej@suse.de>, April 2001.
 *
 *  CacheSize bug workaround updates for AMD, Intel & VIA Cyrix.
 *  Dave Jones <davej@suse.de>, September, October 2001.
 *
 *  Provisions for empty E820 memory regions (reported by certain BIOSes).
 *  Alex Achenbach <xela@slit.de>, December 2002.
 *
 */

/*
 * This file handles the architecture-dependent parts of initialization
 */

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
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/apm_bios.h>
#ifdef CONFIG_BLK_DEV_RAM
#include <linux/blk.h>
#endif
#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/seq_file.h>
#include <asm/processor.h>
#include <linux/console.h>
#include <linux/module.h>
#include <asm/mtrr.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/cobalt.h>
#include <asm/msr.h>
#include <asm/desc.h>
#include <asm/e820.h>
#include <asm/dma.h>
#include <asm/mpspec.h>
#include <asm/mmu_context.h>
#include <asm/io_apic.h>
#include <asm/edd.h>
/*
 * Machine setup..
 */

char ignore_irq13;		/* set if exception 16 works */
struct cpuinfo_x86 boot_cpu_data = { 0, 0, 0, 0, -1, 1, 0, 0, -1 };

unsigned long mmu_cr4_features;
EXPORT_SYMBOL(mmu_cr4_features);

/*
 * Bus types ..
 */
#ifdef CONFIG_EISA
int EISA_bus;
#endif
int MCA_bus;

/* for MCA, but anyone else can use it if they want */
unsigned int machine_id;
unsigned int machine_submodel_id;
unsigned int BIOS_revision;
unsigned int mca_pentium_flag;

/* For PCI or other memory-mapped resources */
unsigned long pci_mem_start = 0x10000000;

/* user-defined highmem size */
static unsigned int highmem_pages __initdata = -1;

/*
 * Setup options
 */
struct drive_info_struct { char dummy[32]; } drive_info;
struct screen_info screen_info;
struct apm_info apm_info;
struct sys_desc_table_struct {
	unsigned short length;
	unsigned char table[0];
};

struct e820map e820;

unsigned char aux_device_present;

extern void mcheck_init(struct cpuinfo_x86 *c);
extern void dmi_scan_machine(void);
extern int root_mountflags;
extern char _text, _etext, _edata, _end;

static int have_cpuid_p(void) __init;

static int disable_x86_serial_nr __initdata = 1;
static u32 disabled_x86_caps[NCAPINTS] __initdata = { 0 };

#ifdef	CONFIG_ACPI_INTERPRETER
	int acpi_disabled = 0;
#else
	int acpi_disabled = 1;
#endif
EXPORT_SYMBOL(acpi_disabled);

#ifdef	CONFIG_ACPI_BOOT
extern	int __initdata acpi_ht;
int acpi_force __initdata = 0;
extern acpi_interrupt_flags	acpi_sci_flags;
#endif

extern int blk_nohighio;

/*
 * This is set up by the setup-routine at boot-time
 */
#define PARAM	((unsigned char *)empty_zero_page)
#define SCREEN_INFO (*(struct screen_info *) (PARAM+0))
#define EXT_MEM_K (*(unsigned short *) (PARAM+2))
#define ALT_MEM_K (*(unsigned long *) (PARAM+0x1e0))
#define E820_MAP_NR (*(char*) (PARAM+E820NR))
#define E820_MAP    ((struct e820entry *) (PARAM+E820MAP))
#define APM_BIOS_INFO (*(struct apm_bios_info *) (PARAM+0x40))
#define DRIVE_INFO (*(struct drive_info_struct *) (PARAM+0x80))
#define SYS_DESC_TABLE (*(struct sys_desc_table_struct*)(PARAM+0xa0))
#define MOUNT_ROOT_RDONLY (*(unsigned short *) (PARAM+0x1F2))
#define RAMDISK_FLAGS (*(unsigned short *) (PARAM+0x1F8))
#define ORIG_ROOT_DEV (*(unsigned short *) (PARAM+0x1FC))
#define AUX_DEVICE_INFO (*(unsigned char *) (PARAM+0x1FF))
#define LOADER_TYPE (*(unsigned char *) (PARAM+0x210))
#define KERNEL_START (*(unsigned long *) (PARAM+0x214))
#define INITRD_START (*(unsigned long *) (PARAM+0x218))
#define INITRD_SIZE (*(unsigned long *) (PARAM+0x21c))
#define DISK80_SIGNATURE_BUFFER (*(unsigned int*) (PARAM+DISK80_SIG_BUFFER))
#define EDD_NR     (*(unsigned char *) (PARAM+EDDNR))
#define EDD_BUF     ((struct edd_info *) (PARAM+EDDBUF))
#define COMMAND_LINE ((char *) (PARAM+2048))
#define COMMAND_LINE_SIZE 256

#define RAMDISK_IMAGE_START_MASK  	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000	

#ifdef	CONFIG_VISWS
char visws_board_type = -1;
char visws_board_rev = -1;

#define	PIIX_PM_START		0x0F80

#define	SIO_GPIO_START		0x0FC0

#define	SIO_PM_START		0x0FC8

#define	PMBASE			PIIX_PM_START
#define	GPIREG0			(PMBASE+0x30)
#define	GPIREG(x)		(GPIREG0+((x)/8))
#define	PIIX_GPI_BD_ID1		18
#define	PIIX_GPI_BD_REG		GPIREG(PIIX_GPI_BD_ID1)

#define	PIIX_GPI_BD_SHIFT	(PIIX_GPI_BD_ID1 % 8)

#define	SIO_INDEX	0x2e
#define	SIO_DATA	0x2f

#define	SIO_DEV_SEL	0x7
#define	SIO_DEV_ENB	0x30
#define	SIO_DEV_MSB	0x60
#define	SIO_DEV_LSB	0x61

#define	SIO_GP_DEV	0x7

#define	SIO_GP_BASE	SIO_GPIO_START
#define	SIO_GP_MSB	(SIO_GP_BASE>>8)
#define	SIO_GP_LSB	(SIO_GP_BASE&0xff)

#define	SIO_GP_DATA1	(SIO_GP_BASE+0)

#define	SIO_PM_DEV	0x8

#define	SIO_PM_BASE	SIO_PM_START
#define	SIO_PM_MSB	(SIO_PM_BASE>>8)
#define	SIO_PM_LSB	(SIO_PM_BASE&0xff)
#define	SIO_PM_INDEX	(SIO_PM_BASE+0)
#define	SIO_PM_DATA	(SIO_PM_BASE+1)

#define	SIO_PM_FER2	0x1

#define	SIO_PM_GP_EN	0x80

static void __init visws_get_board_type_and_rev(void)
{
	int raw;

	visws_board_type = (char)(inb_p(PIIX_GPI_BD_REG) & PIIX_GPI_BD_REG)
							 >> PIIX_GPI_BD_SHIFT;
/*
 * Get Board rev.
 * First, we have to initialize the 307 part to allow us access
 * to the GPIO registers.  Let's map them at 0x0fc0 which is right
 * after the PIIX4 PM section.
 */
	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_GP_DEV, SIO_DATA);	/* Talk to GPIO regs. */
    
	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_GP_MSB, SIO_DATA);	/* MSB of GPIO base address */

	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_GP_LSB, SIO_DATA);	/* LSB of GPIO base address */

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		/* Enable GPIO registers. */
    
/*
 * Now, we have to map the power management section to write
 * a bit which enables access to the GPIO registers.
 * What lunatic came up with this shit?
 */
	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_PM_DEV, SIO_DATA);	/* Talk to GPIO regs. */

	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_PM_MSB, SIO_DATA);	/* MSB of PM base address */
    
	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_PM_LSB, SIO_DATA);	/* LSB of PM base address */

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		/* Enable PM registers. */
    
/*
 * Now, write the PM register which enables the GPIO registers.
 */
	outb_p(SIO_PM_FER2, SIO_PM_INDEX);
	outb_p(SIO_PM_GP_EN, SIO_PM_DATA);
    
/*
 * Now, initialize the GPIO registers.
 * We want them all to be inputs which is the
 * power on default, so let's leave them alone.
 * So, let's just read the board rev!
 */
	raw = inb_p(SIO_GP_DATA1);
	raw &= 0x7f;	/* 7 bits of valid board revision ID. */

	if (visws_board_type == VISWS_320) {
		if (raw < 0x6) {
			visws_board_rev = 4;
		} else if (raw < 0xc) {
			visws_board_rev = 5;
		} else {
			visws_board_rev = 6;
	
		}
	} else if (visws_board_type == VISWS_540) {
			visws_board_rev = 2;
		} else {
			visws_board_rev = raw;
		}

		printk(KERN_INFO "Silicon Graphics %s (rev %d)\n",
			visws_board_type == VISWS_320 ? "320" :
			(visws_board_type == VISWS_540 ? "540" :
					"unknown"),
					visws_board_rev);
	}
#endif


static char command_line[COMMAND_LINE_SIZE];
       char saved_command_line[COMMAND_LINE_SIZE];

struct resource standard_io_resources[] = {
	{ "dma1", 0x00, 0x1f, IORESOURCE_BUSY },
	{ "pic1", 0x20, 0x3f, IORESOURCE_BUSY },
	{ "timer", 0x40, 0x5f, IORESOURCE_BUSY },
	{ "keyboard", 0x60, 0x6f, IORESOURCE_BUSY },
	{ "dma page reg", 0x80, 0x8f, IORESOURCE_BUSY },
	{ "pic2", 0xa0, 0xbf, IORESOURCE_BUSY },
	{ "dma2", 0xc0, 0xdf, IORESOURCE_BUSY },
	{ "fpu", 0xf0, 0xff, IORESOURCE_BUSY }
};

#define STANDARD_IO_RESOURCES (sizeof(standard_io_resources)/sizeof(struct resource))

static struct resource code_resource = { "Kernel code", 0x100000, 0 };
static struct resource data_resource = { "Kernel data", 0, 0 };
static struct resource vram_resource = { "Video RAM area", 0xa0000, 0xbffff, IORESOURCE_BUSY };

/* System ROM resources */
#define MAXROMS 6
static struct resource rom_resources[MAXROMS] = {
	{ "System ROM", 0xF0000, 0xFFFFF, IORESOURCE_BUSY },
	{ "Video ROM", 0xc0000, 0xc7fff, IORESOURCE_BUSY }
};

#define romsignature(x) (*(unsigned short *)(x) == 0xaa55)

static void __init probe_roms(void)
{
	int roms = 1;
	unsigned long base;
	unsigned char *romstart;

	request_resource(&iomem_resource, rom_resources+0);

	/* Video ROM is standard at C000:0000 - C7FF:0000, check signature */
	for (base = 0xC0000; base < 0xE0000; base += 2048) {
		romstart = bus_to_virt(base);
		if (!romsignature(romstart))
			continue;
		request_resource(&iomem_resource, rom_resources + roms);
		roms++;
		break;
	}

	/* Extension roms at C800:0000 - DFFF:0000 */
	for (base = 0xC8000; base < 0xE0000; base += 2048) {
		unsigned long length;

		romstart = bus_to_virt(base);
		if (!romsignature(romstart))
			continue;
		length = romstart[2] * 512;
		if (length) {
			unsigned int i;
			unsigned char chksum;

			chksum = 0;
			for (i = 0; i < length; i++)
				chksum += romstart[i];

			/* Good checksum? */
			if (!chksum) {
				rom_resources[roms].start = base;
				rom_resources[roms].end = base + length - 1;
				rom_resources[roms].name = "Extension ROM";
				rom_resources[roms].flags = IORESOURCE_BUSY;

				request_resource(&iomem_resource, rom_resources + roms);
				roms++;
				if (roms >= MAXROMS)
					return;
			}
		}
	}

	/* Final check for motherboard extension rom at E000:0000 */
	base = 0xE0000;
	romstart = bus_to_virt(base);

	if (romsignature(romstart)) {
		rom_resources[roms].start = base;
		rom_resources[roms].end = base + 65535;
		rom_resources[roms].name = "Extension ROM";
		rom_resources[roms].flags = IORESOURCE_BUSY;

		request_resource(&iomem_resource, rom_resources + roms);
	}
}

static void __init limit_regions (unsigned long long size)
{
	unsigned long long current_addr = 0;
	int i;

	for (i = 0; i < e820.nr_map; i++) {
		if (e820.map[i].type == E820_RAM) {
			current_addr = e820.map[i].addr + e820.map[i].size;
			if (current_addr >= size) {
				e820.map[i].size -= current_addr-size;
				e820.nr_map = i + 1;
				return;
			}
		}
	}
}
static void __init add_memory_region(unsigned long long start,
                                  unsigned long long size, int type)
{
	int x = e820.nr_map;

	if (x == E820MAX) {
	    printk(KERN_ERR "Ooops! Too many entries in the memory map!\n");
	    return;
	}

	e820.map[x].addr = start;
	e820.map[x].size = size;
	e820.map[x].type = type;
	e820.nr_map++;
} /* add_memory_region */

#define E820_DEBUG	1

static void __init print_memory_map(char *who)
{
	int i;

	for (i = 0; i < e820.nr_map; i++) {
		printk(" %s: %016Lx - %016Lx ", who,
			e820.map[i].addr,
			e820.map[i].addr + e820.map[i].size);
		switch (e820.map[i].type) {
		case E820_RAM:	printk("(usable)\n");
				break;
		case E820_RESERVED:
				printk("(reserved)\n");
				break;
		case E820_ACPI:
				printk("(ACPI data)\n");
				break;
		case E820_NVS:
				printk("(ACPI NVS)\n");
				break;
		default:	printk("type %lu\n", e820.map[i].type);
				break;
		}
	}
}

/*
 * Sanitize the BIOS e820 map.
 *
 * Some e820 responses include overlapping entries.  The following 
 * replaces the original e820 map with a new one, removing overlaps.
 *
 */
static int __init sanitize_e820_map(struct e820entry * biosmap, char * pnr_map)
{
	struct change_member {
		struct e820entry *pbios; /* pointer to original bios entry */
		unsigned long long addr; /* address for this change point */
	};
	struct change_member change_point_list[2*E820MAX];
	struct change_member *change_point[2*E820MAX];
	struct e820entry *overlap_list[E820MAX];
	struct e820entry new_bios[E820MAX];
	struct change_member *change_tmp;
	unsigned long current_type, last_type;
	unsigned long long last_addr;
	int chgidx, still_changing;
	int overlap_entries;
	int new_bios_entry;
	int old_nr, new_nr, chg_nr;
	int i;

	/*
		Visually we're performing the following (1,2,3,4 = memory types)...

		Sample memory map (w/overlaps):
		   ____22__________________
		   ______________________4_
		   ____1111________________
		   _44_____________________
		   11111111________________
		   ____________________33__
		   ___________44___________
		   __________33333_________
		   ______________22________
		   ___________________2222_
		   _________111111111______
		   _____________________11_
		   _________________4______

		Sanitized equivalent (no overlap):
		   1_______________________
		   _44_____________________
		   ___1____________________
		   ____22__________________
		   ______11________________
		   _________1______________
		   __________3_____________
		   ___________44___________
		   _____________33_________
		   _______________2________
		   ________________1_______
		   _________________4______
		   ___________________2____
		   ____________________33__
		   ______________________4_
	*/

	/* if there's only one memory region, don't bother */
	if (*pnr_map < 2)
		return -1;

	old_nr = *pnr_map;

	/* bail out if we find any unreasonable addresses in bios map */
	for (i=0; i<old_nr; i++)
		if (biosmap[i].addr + biosmap[i].size < biosmap[i].addr)
			return -1;

	/* create pointers for initial change-point information (for sorting) */
	for (i=0; i < 2*old_nr; i++)
		change_point[i] = &change_point_list[i];

	/* record all known change-points (starting and ending addresses),
	   omitting those that are for empty memory regions */
	chgidx = 0;
	for (i=0; i < old_nr; i++)	{
		if (biosmap[i].size != 0) {
			change_point[chgidx]->addr = biosmap[i].addr;
			change_point[chgidx++]->pbios = &biosmap[i];
			change_point[chgidx]->addr = biosmap[i].addr + biosmap[i].size;
			change_point[chgidx++]->pbios = &biosmap[i];
		}
	}
	chg_nr = chgidx;    	/* true number of change-points */

	/* sort change-point list by memory addresses (low -> high) */
	still_changing = 1;
	while (still_changing)	{
		still_changing = 0;
		for (i=1; i < chg_nr; i++)  {
			/* if <current_addr> > <last_addr>, swap */
			/* or, if current=<start_addr> & last=<end_addr>, swap */
			if ((change_point[i]->addr < change_point[i-1]->addr) ||
				((change_point[i]->addr == change_point[i-1]->addr) &&
				 (change_point[i]->addr == change_point[i]->pbios->addr) &&
				 (change_point[i-1]->addr != change_point[i-1]->pbios->addr))
			   )
			{
				change_tmp = change_point[i];
				change_point[i] = change_point[i-1];
				change_point[i-1] = change_tmp;
				still_changing=1;
			}
		}
	}

	/* create a new bios memory map, removing overlaps */
	overlap_entries=0;	 /* number of entries in the overlap table */
	new_bios_entry=0;	 /* index for creating new bios map entries */
	last_type = 0;		 /* start with undefined memory type */
	last_addr = 0;		 /* start with 0 as last starting address */
	/* loop through change-points, determining affect on the new bios map */
	for (chgidx=0; chgidx < chg_nr; chgidx++)
	{
		/* keep track of all overlapping bios entries */
		if (change_point[chgidx]->addr == change_point[chgidx]->pbios->addr)
		{
			/* add map entry to overlap list (> 1 entry implies an overlap) */
			overlap_list[overlap_entries++]=change_point[chgidx]->pbios;
		}
		else
		{
			/* remove entry from list (order independent, so swap with last) */
			for (i=0; i<overlap_entries; i++)
			{
				if (overlap_list[i] == change_point[chgidx]->pbios)
					overlap_list[i] = overlap_list[overlap_entries-1];
			}
			overlap_entries--;
		}
		/* if there are overlapping entries, decide which "type" to use */
		/* (larger value takes precedence -- 1=usable, 2,3,4,4+=unusable) */
		current_type = 0;
		for (i=0; i<overlap_entries; i++)
			if (overlap_list[i]->type > current_type)
				current_type = overlap_list[i]->type;
		/* continue building up new bios map based on this information */
		if (current_type != last_type)	{
			if (last_type != 0)	 {
				new_bios[new_bios_entry].size =
					change_point[chgidx]->addr - last_addr;
				/* move forward only if the new size was non-zero */
				if (new_bios[new_bios_entry].size != 0)
					if (++new_bios_entry >= E820MAX)
						break; 	/* no more space left for new bios entries */
			}
			if (current_type != 0)	{
				new_bios[new_bios_entry].addr = change_point[chgidx]->addr;
				new_bios[new_bios_entry].type = current_type;
				last_addr=change_point[chgidx]->addr;
			}
			last_type = current_type;
		}
	}
	new_nr = new_bios_entry;   /* retain count for new bios entries */

	/* copy new bios mapping into original location */
	memcpy(biosmap, new_bios, new_nr*sizeof(struct e820entry));
	*pnr_map = new_nr;

	return 0;
}

/*
 * Copy the BIOS e820 map into a safe place.
 *
 * Sanity-check it while we're at it..
 *
 * If we're lucky and live on a modern system, the setup code
 * will have given us a memory map that we can use to properly
 * set up memory.  If we aren't, we'll fake a memory map.
 *
 * We check to see that the memory map contains at least 2 elements
 * before we'll use it, because the detection code in setup.S may
 * not be perfect and most every PC known to man has two memory
 * regions: one from 0 to 640k, and one from 1mb up.  (The IBM
 * thinkpad 560x, for example, does not cooperate with the memory
 * detection code.)
 */
static int __init copy_e820_map(struct e820entry * biosmap, int nr_map)
{
	/* Only one memory region (or negative)? Ignore it */
	if (nr_map < 2)
		return -1;

	do {
		unsigned long long start = biosmap->addr;
		unsigned long long size = biosmap->size;
		unsigned long long end = start + size;
		unsigned long type = biosmap->type;

		/* Overflow in 64 bits? Ignore the memory map. */
		if (start > end)
			return -1;

		/*
		 * Some BIOSes claim RAM in the 640k - 1M region.
		 * Not right. Fix it up.
		 */
		if (type == E820_RAM) {
			if (start < 0x100000ULL && end > 0xA0000ULL) {
				if (start < 0xA0000ULL)
					add_memory_region(start, 0xA0000ULL-start, type);
				if (end <= 0x100000ULL)
					continue;
				start = 0x100000ULL;
				size = end - start;
			}
		}
		add_memory_region(start, size, type);
	} while (biosmap++,--nr_map);
	return 0;
}

#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)
unsigned char eddnr;
struct edd_info edd[EDDMAXNR];
unsigned int edd_disk80_sig;
/**
 * copy_edd() - Copy the BIOS EDD information
 *              from empty_zero_page into a safe place.
 *
 */
static inline void copy_edd(void)
{
     eddnr = EDD_NR;
     memcpy(edd, EDD_BUF, sizeof(edd));
     edd_disk80_sig = DISK80_SIGNATURE_BUFFER;
}
#else
static inline void copy_edd(void) {}
#endif

/*
 * Do NOT EVER look at the BIOS memory size location.
 * It does not work on many machines.
 */
#define LOWMEMSIZE()	(0x9f000)

static void __init setup_memory_region(void)
{
	char *who = "BIOS-e820";

	/*
	 * Try to copy the BIOS-supplied E820-map.
	 *
	 * Otherwise fake a memory map; one section from 0k->640k,
	 * the next section from 1mb->appropriate_mem_k
	 */
	sanitize_e820_map(E820_MAP, &E820_MAP_NR);
	if (copy_e820_map(E820_MAP, E820_MAP_NR) < 0) {
		unsigned long mem_size;

		/* compare results from other methods and take the greater */
		if (ALT_MEM_K < EXT_MEM_K) {
			mem_size = EXT_MEM_K;
			who = "BIOS-88";
		} else {
			mem_size = ALT_MEM_K;
			who = "BIOS-e801";
		}

		e820.nr_map = 0;
		add_memory_region(0, LOWMEMSIZE(), E820_RAM);
		add_memory_region(HIGH_MEMORY, mem_size << 10, E820_RAM);
  	}
	printk(KERN_INFO "BIOS-provided physical RAM map:\n");
	print_memory_map(who);
} /* setup_memory_region */


static void __init parse_cmdline_early (char ** cmdline_p)
{
	char c = ' ', *to = command_line, *from = COMMAND_LINE;
	int len = 0;
	int userdef = 0;

	/* Save unparsed command line copy for /proc/cmdline */
	memcpy(saved_command_line, COMMAND_LINE, COMMAND_LINE_SIZE);
	saved_command_line[COMMAND_LINE_SIZE-1] = '\0';

	for (;;) {
		if (c != ' ')
			goto nextchar;
		/*
		 * "mem=nopentium" disables the 4MB page tables.
		 * "mem=XXX[kKmM]" defines a memory region from HIGH_MEM
		 * to <mem>, overriding the bios size.
		 * "mem=XXX[KkmM]@XXX[KkmM]" defines a memory region from
		 * <start> to <start>+<mem>, overriding the bios size.
		 */
		if (!memcmp(from, "mem=", 4)) {
			if (to != command_line)
				to--;
			if (!memcmp(from+4, "nopentium", 9)) {
				from += 9+4;
				clear_bit(X86_FEATURE_PSE, &boot_cpu_data.x86_capability);
				set_bit(X86_FEATURE_PSE, &disabled_x86_caps);
			} else if (!memcmp(from+4, "exactmap", 8)) {
				from += 8+4;
				e820.nr_map = 0;
				userdef = 1;
			} else {
				/* If the user specifies memory size, we
				 * limit the BIOS-provided memory map to
				 * that size. exactmap can be used to specify
				 * the exact map. mem=number can be used to
				 * trim the existing memory map.
				 */
				unsigned long long start_at, mem_size;
 
				mem_size = memparse(from+4, &from);
				if (*from == '@') {
					start_at = memparse(from+1, &from);
					add_memory_region(start_at, mem_size, E820_RAM);
				} else if (*from == '#') {
					start_at = memparse(from+1, &from);
					add_memory_region(start_at, mem_size, E820_ACPI);
				} else if (*from == '$') {
					start_at = memparse(from+1, &from);
					add_memory_region(start_at, mem_size, E820_RESERVED);
				} else {
					limit_regions(mem_size);
					userdef=1;
				}
			}
		}
#ifdef	CONFIG_SMP
		/*
		 * If the BIOS enumerates physical processors before logical,
		 * maxcpus=N at enumeration-time can be used to disable HT.
		 */
		else if (!memcmp(from, "maxcpus=", 8)) {
			extern unsigned int max_cpus;

			max_cpus = simple_strtoul(from + 8, NULL, 0);
		}
#endif

#ifdef CONFIG_ACPI_BOOT
		/* "acpi=off" disables both ACPI table parsing and interpreter */
		else if (!memcmp(from, "acpi=off", 8)) {
			disable_acpi();
		}

		/* acpi=force to over-ride black-list */
		else if (!memcmp(from, "acpi=force", 10)) { 
			acpi_force = 1;
			acpi_ht = 1;
			acpi_disabled = 0;
		} 

		/* Limit ACPI to boot-time only, still enabled HT */
		else if (!memcmp(from, "acpi=ht", 7)) { 
			if (!acpi_force)
				disable_acpi();
			acpi_ht = 1; 
		} 

		/* acpi=strict disables out-of-spec workarounds */
		else if (!memcmp(from, "acpi=strict", 11)) {
			acpi_strict = 1;
		}

		else if (!memcmp(from, "pci=noacpi", 10)) { 
			acpi_noirq_set();
		}

                /* disable IO-APIC */
                else if (!memcmp(from, "noapic", 6))
                        disable_ioapic_setup();

		else if (!memcmp(from, "acpi_sci=edge", 13))
			acpi_sci_flags.trigger =  1;
		else if (!memcmp(from, "acpi_sci=level", 14))
			acpi_sci_flags.trigger = 3;
		else if (!memcmp(from, "acpi_sci=high", 13))
			acpi_sci_flags.polarity = 1;
		else if (!memcmp(from, "acpi_sci=low", 12))
			acpi_sci_flags.polarity = 3;

#endif
		/*
		 * highmem=size forces highmem to be exactly 'size' bytes.
		 * This works even on boxes that have no highmem otherwise.
		 * This also works to reduce highmem size on bigger boxes.
		 */
		else if (!memcmp(from, "highmem=", 8))
			highmem_pages = memparse(from+8, &from) >> PAGE_SHIFT;
nextchar:
		c = *(from++);
		if (!c)
			break;
		if (COMMAND_LINE_SIZE <= ++len)
			break;
		*(to++) = c;
	}
	*to = '\0';
	*cmdline_p = command_line;
	if (userdef) {
		printk(KERN_INFO "user-defined physical RAM map:\n");
		print_memory_map("user");
	}
}

#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((x) << PAGE_SHIFT)

/*
 * Reserved space for vmalloc and iomap - defined in asm/page.h
 */
#define MAXMEM_PFN	PFN_DOWN(MAXMEM)
#define MAX_NONPAE_PFN	(1 << 20)

/*
 * Find the highest page frame number we have available
 */
static void __init find_max_pfn(void)
{
	int i;

	max_pfn = 0;
	for (i = 0; i < e820.nr_map; i++) {
		unsigned long start, end;
		/* RAM? */
		if (e820.map[i].type != E820_RAM)
			continue;
		start = PFN_UP(e820.map[i].addr);
		end = PFN_DOWN(e820.map[i].addr + e820.map[i].size);
		if (start >= end)
			continue;
		if (end > max_pfn)
			max_pfn = end;
	}
}

/*
 * Determine low and high memory ranges:
 */
static unsigned long __init find_max_low_pfn(void)
{
	unsigned long max_low_pfn;

	max_low_pfn = max_pfn;
	if (max_low_pfn > MAXMEM_PFN) {
		if (highmem_pages == -1)
			highmem_pages = max_pfn - MAXMEM_PFN;
		if (highmem_pages + MAXMEM_PFN < max_pfn)
			max_pfn = MAXMEM_PFN + highmem_pages;
		if (highmem_pages + MAXMEM_PFN > max_pfn) {
			printk("only %luMB highmem pages available, ignoring highmem size of %uMB.\n", pages_to_mb(max_pfn - MAXMEM_PFN), pages_to_mb(highmem_pages));
			highmem_pages = 0;
		}
		max_low_pfn = MAXMEM_PFN;
#ifndef CONFIG_HIGHMEM
		/* Maximum memory usable is what is directly addressable */
		printk(KERN_WARNING "Warning only %ldMB will be used.\n",
					MAXMEM>>20);
		if (max_pfn > MAX_NONPAE_PFN)
			printk(KERN_WARNING "Use a PAE enabled kernel.\n");
		else
			printk(KERN_WARNING "Use a HIGHMEM enabled kernel.\n");
#else /* !CONFIG_HIGHMEM */
#ifndef CONFIG_X86_PAE
		if (max_pfn > MAX_NONPAE_PFN) {
			max_pfn = MAX_NONPAE_PFN;
			printk(KERN_WARNING "Warning only 4GB will be used.\n");
			printk(KERN_WARNING "Use a PAE enabled kernel.\n");
		}
#endif /* !CONFIG_X86_PAE */
#endif /* !CONFIG_HIGHMEM */
	} else {
		if (highmem_pages == -1)
			highmem_pages = 0;
#if CONFIG_HIGHMEM
		if (highmem_pages >= max_pfn) {
			printk(KERN_ERR "highmem size specified (%uMB) is bigger than pages available (%luMB)!.\n", pages_to_mb(highmem_pages), pages_to_mb(max_pfn));
			highmem_pages = 0;
		}
		if (highmem_pages) {
			if (max_low_pfn-highmem_pages < 64*1024*1024/PAGE_SIZE){
				printk(KERN_ERR "highmem size %uMB results in smaller than 64MB lowmem, ignoring it.\n", pages_to_mb(highmem_pages));
				highmem_pages = 0;
			}
			max_low_pfn -= highmem_pages;
		}
#else
		if (highmem_pages)
			printk(KERN_ERR "ignoring highmem size on non-highmem kernel!\n");
#endif
	}

	return max_low_pfn;
}

/*
 * Register fully available low RAM pages with the bootmem allocator.
 */
static void __init register_bootmem_low_pages(unsigned long max_low_pfn)
{
	int i;

	for (i = 0; i < e820.nr_map; i++) {
		unsigned long curr_pfn, last_pfn, size;
		/*
		 * Reserve usable low memory
		 */
		if (e820.map[i].type != E820_RAM)
			continue;
		/*
		 * We are rounding up the start address of usable memory:
		 */
		curr_pfn = PFN_UP(e820.map[i].addr);
		if (curr_pfn >= max_low_pfn)
			continue;
		/*
		 * ... and at the end of the usable range downwards:
		 */
		last_pfn = PFN_DOWN(e820.map[i].addr + e820.map[i].size);

		if (last_pfn > max_low_pfn)
			last_pfn = max_low_pfn;

		/*
		 * .. finally, did all the rounding and playing
		 * around just make the area go away?
		 */
		if (last_pfn <= curr_pfn)
			continue;

		size = last_pfn - curr_pfn;
		free_bootmem(PFN_PHYS(curr_pfn), PFN_PHYS(size));
	}
}

static unsigned long __init setup_memory(void)
{
	unsigned long bootmap_size, start_pfn, max_low_pfn;

	/*
	 * partially used pages are not usable - thus
	 * we are rounding upwards:
	 */
	start_pfn = PFN_UP(__pa(&_end));

	find_max_pfn();

	max_low_pfn = find_max_low_pfn();

#ifdef CONFIG_HIGHMEM
	highstart_pfn = highend_pfn = max_pfn;
	if (max_pfn > max_low_pfn) {
		highstart_pfn = max_low_pfn;
	}
	printk(KERN_NOTICE "%ldMB HIGHMEM available.\n",
		pages_to_mb(highend_pfn - highstart_pfn));
#endif
	printk(KERN_NOTICE "%ldMB LOWMEM available.\n",
			pages_to_mb(max_low_pfn));
	/*
	 * Initialize the boot-time allocator (with low memory only):
	 */
	bootmap_size = init_bootmem(start_pfn, max_low_pfn);

	register_bootmem_low_pages(max_low_pfn);

	/*
	 * Reserve the bootmem bitmap itself as well. We do this in two
	 * steps (first step was init_bootmem()) because this catches
	 * the (very unlikely) case of us accidentally initializing the
	 * bootmem allocator with an invalid RAM area.
	 */
	reserve_bootmem(HIGH_MEMORY, (PFN_PHYS(start_pfn) +
			 bootmap_size + PAGE_SIZE-1) - (HIGH_MEMORY));

	/*
	 * reserve physical page 0 - it's a special BIOS page on many boxes,
	 * enabling clean reboots, SMP operation, laptop functions.
	 */
	reserve_bootmem(0, PAGE_SIZE);

#ifdef CONFIG_SMP
	/*
	 * But first pinch a few for the stack/trampoline stuff
	 * FIXME: Don't need the extra page at 4K, but need to fix
	 * trampoline before removing it. (see the GDT stuff)
	 */
	reserve_bootmem(PAGE_SIZE, PAGE_SIZE);
#endif
#ifdef CONFIG_ACPI_SLEEP
	/*
	 * Reserve low memory region for sleep support.
	 */
	acpi_reserve_bootmem();
#endif
#ifdef CONFIG_X86_LOCAL_APIC
	/*
	 * Find and reserve possible boot-time SMP configuration.
	 */
	find_smp_config();
#endif
#ifdef CONFIG_BLK_DEV_INITRD
	if (LOADER_TYPE && INITRD_START) {
		if (INITRD_START + INITRD_SIZE <= (max_low_pfn << PAGE_SHIFT)) {
			reserve_bootmem(INITRD_START, INITRD_SIZE);
			initrd_start =
				INITRD_START ? INITRD_START + PAGE_OFFSET : 0;
			initrd_end = initrd_start+INITRD_SIZE;
		}
		else {
			printk(KERN_ERR "initrd extends beyond end of memory "
			    "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
			    INITRD_START + INITRD_SIZE,
			    max_low_pfn << PAGE_SHIFT);
			initrd_start = 0;
		}
	}
#endif

	return max_low_pfn;
}
 
/*
 * Request address space for all standard RAM and ROM resources
 * and also for regions reported as reserved by the e820.
 */
static void __init register_memory(unsigned long max_low_pfn)
{
	unsigned long low_mem_size;
	int i;
	probe_roms();
	for (i = 0; i < e820.nr_map; i++) {
		struct resource *res;
		if (e820.map[i].addr + e820.map[i].size > 0x100000000ULL)
			continue;
		res = alloc_bootmem_low(sizeof(struct resource));
		switch (e820.map[i].type) {
		case E820_RAM:	res->name = "System RAM"; break;
		case E820_ACPI:	res->name = "ACPI Tables"; break;
		case E820_NVS:	res->name = "ACPI Non-volatile Storage"; break;
		default:	res->name = "reserved";
		}
		res->start = e820.map[i].addr;
		res->end = res->start + e820.map[i].size - 1;
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		request_resource(&iomem_resource, res);
		if (e820.map[i].type == E820_RAM) {
			/*
			 *  We dont't know which RAM region contains kernel data,
			 *  so we try it repeatedly and let the resource manager
			 *  test it.
			 */
			request_resource(res, &code_resource);
			request_resource(res, &data_resource);
		}
	}
	request_resource(&iomem_resource, &vram_resource);

	/* request I/O space for devices used on all i[345]86 PCs */
	for (i = 0; i < STANDARD_IO_RESOURCES; i++)
		request_resource(&ioport_resource, standard_io_resources+i);

	/* Tell the PCI layer not to allocate too close to the RAM area.. */
	low_mem_size = ((max_low_pfn << PAGE_SHIFT) + 0xfffff) & ~0xfffff;
	if (low_mem_size > pci_mem_start)
		pci_mem_start = low_mem_size;
}

void __init setup_arch(char **cmdline_p)
{
	unsigned long max_low_pfn;

#ifdef CONFIG_VISWS
	visws_get_board_type_and_rev();
#endif

#ifndef CONFIG_HIGHIO
	blk_nohighio = 1;
#endif

 	ROOT_DEV = to_kdev_t(ORIG_ROOT_DEV);
 	drive_info = DRIVE_INFO;
 	screen_info = SCREEN_INFO;
	apm_info.bios = APM_BIOS_INFO;
	if( SYS_DESC_TABLE.length != 0 ) {
		MCA_bus = SYS_DESC_TABLE.table[3] &0x2;
		machine_id = SYS_DESC_TABLE.table[0];
		machine_submodel_id = SYS_DESC_TABLE.table[1];
		BIOS_revision = SYS_DESC_TABLE.table[2];
	}
	aux_device_present = AUX_DEVICE_INFO;

#ifdef CONFIG_BLK_DEV_RAM
	rd_image_start = RAMDISK_FLAGS & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((RAMDISK_FLAGS & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((RAMDISK_FLAGS & RAMDISK_LOAD_FLAG) != 0);
#endif
	setup_memory_region();
	copy_edd();

	if (!MOUNT_ROOT_RDONLY)
		root_mountflags &= ~MS_RDONLY;
	init_mm.start_code = (unsigned long) &_text;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
	init_mm.brk = (unsigned long) &_end;

	code_resource.start = virt_to_bus(&_text);
	code_resource.end = virt_to_bus(&_etext)-1;
	data_resource.start = virt_to_bus(&_etext);
	data_resource.end = virt_to_bus(&_edata)-1;

	parse_cmdline_early(cmdline_p);

	max_low_pfn = setup_memory();

	/*
	 * NOTE: before this point _nobody_ is allowed to allocate
	 * any memory using the bootmem allocator.
	 */

#ifdef CONFIG_SMP
	smp_alloc_memory(); /* AP processor realmode stacks in low memory*/
#endif
	paging_init();

	dmi_scan_machine();

	/*
	 * Parse the ACPI tables for possible boot-time SMP configuration.
	 */
	acpi_boot_init();

#ifdef CONFIG_X86_LOCAL_APIC
	/*
	 * get boot-time SMP configuration:
	 */
	if (smp_found_config)
		get_smp_config();
#endif

	register_memory(max_low_pfn);

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif
}

static int cachesize_override __initdata = -1;
static int __init cachesize_setup(char *str)
{
	get_option (&str, &cachesize_override);
	return 1;
}
__setup("cachesize=", cachesize_setup);


#ifndef CONFIG_X86_TSC
static int tsc_disable __initdata = 0;

static int __init notsc_setup(char *str)
{
	tsc_disable = 1;
	return 1;
}
#else
static int __init notsc_setup(char *str)
{
	printk("notsc: Kernel compiled with CONFIG_X86_TSC, cannot disable TSC.\n");
	return 1;
}
#endif
__setup("notsc", notsc_setup);

static int __init highio_setup(char *str)
{
	printk("i386: disabling HIGHMEM block I/O\n");
	blk_nohighio = 1;
	return 1;
}
__setup("nohighio", highio_setup);

static int __init get_model_name(struct cpuinfo_x86 *c)
{
	unsigned int *v;
	char *p, *q;

	if (cpuid_eax(0x80000000) < 0x80000004)
		return 0;

	v = (unsigned int *) c->x86_model_id;
	cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	c->x86_model_id[48] = 0;

	/* Intel chips right-justify this string for some dumb reason;
	   undo that brain damage */
	p = q = &c->x86_model_id[0];
	while ( *p == ' ' )
	     p++;
	if ( p != q ) {
	     while ( *p )
		  *q++ = *p++;
	     while ( q <= &c->x86_model_id[48] )
		  *q++ = '\0';	/* Zero-pad the rest */
	}

	return 1;
}


static void __init display_cacheinfo(struct cpuinfo_x86 *c)
{
	unsigned int n, dummy, ecx, edx, l2size;

	n = cpuid_eax(0x80000000);

	if (n >= 0x80000005) {
		cpuid(0x80000005, &dummy, &dummy, &ecx, &edx);
		printk(KERN_INFO "CPU: L1 I Cache: %dK (%d bytes/line), D cache %dK (%d bytes/line)\n",
			edx>>24, edx&0xFF, ecx>>24, ecx&0xFF);
		c->x86_cache_size=(ecx>>24)+(edx>>24);	
	}

	if (n < 0x80000006)	/* Some chips just has a large L1. */
		return;

	ecx = cpuid_ecx(0x80000006);
	l2size = ecx >> 16;

	/* AMD errata T13 (order #21922) */
	if ((c->x86_vendor == X86_VENDOR_AMD) && (c->x86 == 6)) {
		if (c->x86_model == 3 && c->x86_mask == 0)	/* Duron Rev A0 */
			l2size = 64;
		if (c->x86_model == 4 &&
			(c->x86_mask==0 || c->x86_mask==1))	/* Tbird rev A1/A2 */
			l2size = 256;
	}

	if (c->x86_vendor == X86_VENDOR_CENTAUR) {
		/* VIA C3 CPUs (670-68F) need further shifting. */
		if ((c->x86 == 6) &&
		    ((c->x86_model == 7) || (c->x86_model == 8))) {
			l2size >>= 8;
		}

		/* VIA also screwed up Nehemiah stepping 1, and made
		   it return '65KB' instead of '64KB'
		   - Note, it seems this may only be in engineering samples. */
		if ((c->x86==6) && (c->x86_model==9) &&
		    (c->x86_mask==1) && (l2size==65))
			l2size -= 1;
	}

	/* Allow user to override all this if necessary. */
	if (cachesize_override != -1)
		l2size = cachesize_override;

	if ( l2size == 0 )
		return;		/* Again, no L2 cache is possible */

	c->x86_cache_size = l2size;

	printk(KERN_INFO "CPU: L2 Cache: %dK (%d bytes/line)\n",
	       l2size, ecx & 0xFF);
}

/*
 *	B step AMD K6 before B 9730xxxx have hardware bugs that can cause
 *	misexecution of code under Linux. Owners of such processors should
 *	contact AMD for precise details and a CPU swap.
 *
 *	See	http://www.multimania.com/poulot/k6bug.html
 *		http://www.amd.com/K6/k6docs/revgd.html
 *
 *	The following test is erm.. interesting. AMD neglected to up
 *	the chip setting when fixing the bug but they also tweaked some
 *	performance at the same time..
 */
 
extern void vide(void);
__asm__(".align 4\nvide: ret");

static int __init init_amd(struct cpuinfo_x86 *c)
{
	u32 l, h;
	int mbytes = max_mapnr >> (20-PAGE_SHIFT);
	int r;

	/*
	 *	FIXME: We should handle the K5 here. Set up the write
	 *	range and also turn on MSR 83 bits 4 and 31 (write alloc,
	 *	no bus pipeline)
	 */

	/* Bit 31 in normal CPUID used for nonstandard 3DNow ID;
	   3DNow is IDd by bit 31 in extended CPUID (1*32+31) anyway */
	clear_bit(0*32+31, &c->x86_capability);
	
	r = get_model_name(c);

	switch(c->x86)
	{
		case 5:
			if( c->x86_model < 6 )
			{
				/* Based on AMD doc 20734R - June 2000 */
				if ( c->x86_model == 0 ) {
					clear_bit(X86_FEATURE_APIC, &c->x86_capability);
					set_bit(X86_FEATURE_PGE, &c->x86_capability);
				}
				break;
			}
			
			if ( c->x86_model == 6 && c->x86_mask == 1 ) {
				const int K6_BUG_LOOP = 1000000;
				int n;
				void (*f_vide)(void);
				unsigned long d, d2;
				
				printk(KERN_INFO "AMD K6 stepping B detected - ");
				
				/*
				 * It looks like AMD fixed the 2.6.2 bug and improved indirect 
				 * calls at the same time.
				 */

				n = K6_BUG_LOOP;
				f_vide = vide;
				rdtscl(d);
				while (n--) 
					f_vide();
				rdtscl(d2);
				d = d2-d;
				
				/* Knock these two lines out if it debugs out ok */
				printk(KERN_INFO "K6 BUG %ld %d (Report these if test report is incorrect)\n", d, 20*K6_BUG_LOOP);
				printk(KERN_INFO "AMD K6 stepping B detected - ");
				/* -- cut here -- */
				if (d > 20*K6_BUG_LOOP) 
					printk("system stability may be impaired when more than 32 MB are used.\n");
				else 
					printk("probably OK (after B9730xxxx).\n");
				printk(KERN_INFO "Please see http://www.mygale.com/~poulot/k6bug.html\n");
			}

			/* K6 with old style WHCR */
			if (c->x86_model < 8 ||
			   (c->x86_model== 8 && c->x86_mask < 8)) {
				/* We can only write allocate on the low 508Mb */
				if(mbytes>508)
					mbytes=508;

				rdmsr(MSR_K6_WHCR, l, h);
				if ((l&0x0000FFFF)==0) {
					unsigned long flags;
					l=(1<<0)|((mbytes/4)<<1);
					local_irq_save(flags);
					wbinvd();
					wrmsr(MSR_K6_WHCR, l, h);
					local_irq_restore(flags);
					printk(KERN_INFO "Enabling old style K6 write allocation for %d Mb\n",
						mbytes);
				}
				break;
			}

			if ((c->x86_model == 8 && c->x86_mask >7) ||
			     c->x86_model == 9 || c->x86_model == 13) {
				/* The more serious chips .. */

				if(mbytes>4092)
					mbytes=4092;

				rdmsr(MSR_K6_WHCR, l, h);
				if ((l&0xFFFF0000)==0) {
					unsigned long flags;
					l=((mbytes>>2)<<22)|(1<<16);
					local_irq_save(flags);
					wbinvd();
					wrmsr(MSR_K6_WHCR, l, h);
					local_irq_restore(flags);
					printk(KERN_INFO "Enabling new style K6 write allocation for %d Mb\n",
						mbytes);
				}

				/*  Set MTRR capability flag if appropriate */
				if (c->x86_model == 13 || c->x86_model == 9 ||
				   (c->x86_model == 8 && c->x86_mask >= 8))
					set_bit(X86_FEATURE_K6_MTRR, &c->x86_capability);
				break;
			}
			break;

		case 6: /* An Athlon/Duron */
 
			/* Bit 15 of Athlon specific MSR 15, needs to be 0
 			 * to enable SSE on Palomino/Morgan CPU's.
			 * If the BIOS didn't enable it already, enable it
			 * here.
			 */
			if (c->x86_model >= 6 && c->x86_model <= 10) {
				if (!test_bit(X86_FEATURE_XMM,
					      &c->x86_capability)) {
					printk(KERN_INFO
					       "Enabling Disabled K7/SSE Support...\n");
					rdmsr(MSR_K7_HWCR, l, h);
					l &= ~0x00008000;
					wrmsr(MSR_K7_HWCR, l, h);
					set_bit(X86_FEATURE_XMM,
                                                &c->x86_capability);
				}
			}

			/* It's been determined by AMD that Athlons since model 8 stepping 1
			 * are more robust with CLK_CTL set to 200xxxxx instead of 600xxxxx
			 * As per AMD technical note 27212 0.2
			 */
			if ((c->x86_model == 8 && c->x86_mask>=1) || (c->x86_model > 8)) {
				rdmsr(MSR_K7_CLK_CTL, l, h);
				if ((l & 0xfff00000) != 0x20000000) {
					printk ("CPU: CLK_CTL MSR was %x. Reprogramming to %x\n", l,
						((l & 0x000fffff)|0x20000000));
					wrmsr(MSR_K7_CLK_CTL, (l & 0x000fffff)|0x20000000, h);
				}
			}
			break;
	}

	display_cacheinfo(c);
	return r;
}

/*
 * Read NSC/Cyrix DEVID registers (DIR) to get more detailed info. about the CPU
 */
static void __init do_cyrix_devid(unsigned char *dir0, unsigned char *dir1)
{
	unsigned char ccr2, ccr3;
	unsigned long flags;
	
	/* we test for DEVID by checking whether CCR3 is writable */
	local_irq_save(flags);
	ccr3 = getCx86(CX86_CCR3);
	setCx86(CX86_CCR3, ccr3 ^ 0x80);
	getCx86(0xc0);   /* dummy to change bus */

	if (getCx86(CX86_CCR3) == ccr3) {       /* no DEVID regs. */
		ccr2 = getCx86(CX86_CCR2);
		setCx86(CX86_CCR2, ccr2 ^ 0x04);
		getCx86(0xc0);  /* dummy */

		if (getCx86(CX86_CCR2) == ccr2) /* old Cx486SLC/DLC */
			*dir0 = 0xfd;
		else {                          /* Cx486S A step */
			setCx86(CX86_CCR2, ccr2);
			*dir0 = 0xfe;
		}
	}
	else {
		setCx86(CX86_CCR3, ccr3);  /* restore CCR3 */

		/* read DIR0 and DIR1 CPU registers */
		*dir0 = getCx86(CX86_DIR0);
		*dir1 = getCx86(CX86_DIR1);
	}
	local_irq_restore(flags);
}

/*
 * Cx86_dir0_msb is a HACK needed by check_cx686_cpuid/slop in
 * order to identify the Cyrix CPU model after we're out of the
 * initial setup.
 */
static unsigned char Cx86_dir0_msb __initdata = 0;

static char Cx86_model[][9] __initdata = {
	"Cx486", "Cx486", "5x86 ", "6x86", "MediaGX ", "6x86MX ",
	"M II ", "Unknown"
};
static char Cx486_name[][5] __initdata = {
	"SLC", "DLC", "SLC2", "DLC2", "SRx", "DRx",
	"SRx2", "DRx2"
};
static char Cx486S_name[][4] __initdata = {
	"S", "S2", "Se", "S2e"
};
static char Cx486D_name[][4] __initdata = {
	"DX", "DX2", "?", "?", "?", "DX4"
};
static char Cx86_cb[] __initdata = "?.5x Core/Bus Clock";
static char cyrix_model_mult1[] __initdata = "12??43";
static char cyrix_model_mult2[] __initdata = "12233445";

/*
 * Reset the slow-loop (SLOP) bit on the 686(L) which is set by some old
 * BIOSes for compatability with DOS games.  This makes the udelay loop
 * work correctly, and improves performance.
 *
 * FIXME: our newer udelay uses the tsc. We dont need to frob with SLOP
 */

extern void calibrate_delay(void) __init;

static void __init check_cx686_slop(struct cpuinfo_x86 *c)
{
	unsigned long flags;
	
	if (Cx86_dir0_msb == 3) {
		unsigned char ccr3, ccr5;

		local_irq_save(flags);
		ccr3 = getCx86(CX86_CCR3);
		setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10); /* enable MAPEN  */
		ccr5 = getCx86(CX86_CCR5);
		if (ccr5 & 2)
			setCx86(CX86_CCR5, ccr5 & 0xfd);  /* reset SLOP */
		setCx86(CX86_CCR3, ccr3);                 /* disable MAPEN */
		local_irq_restore(flags);

		if (ccr5 & 2) { /* possible wrong calibration done */
			printk(KERN_INFO "Recalibrating delay loop with SLOP bit reset\n");
			calibrate_delay();
			c->loops_per_jiffy = loops_per_jiffy;
		}
	}
}

static void __init init_cyrix(struct cpuinfo_x86 *c)
{
	unsigned char dir0, dir0_msn, dir0_lsn, dir1 = 0;
	char *buf = c->x86_model_id;
	const char *p = NULL;

	/* Bit 31 in normal CPUID used for nonstandard 3DNow ID;
	   3DNow is IDd by bit 31 in extended CPUID (1*32+31) anyway */
	clear_bit(0*32+31, &c->x86_capability);

	/* Cyrix used bit 24 in extended (AMD) CPUID for Cyrix MMX extensions */
	if ( test_bit(1*32+24, &c->x86_capability) ) {
		clear_bit(1*32+24, &c->x86_capability);
		set_bit(X86_FEATURE_CXMMX, &c->x86_capability);
	}

	do_cyrix_devid(&dir0, &dir1);

	check_cx686_slop(c);

	Cx86_dir0_msb = dir0_msn = dir0 >> 4; /* identifies CPU "family"   */
	dir0_lsn = dir0 & 0xf;                /* model or clock multiplier */

	/* common case step number/rev -- exceptions handled below */
	c->x86_model = (dir1 >> 4) + 1;
	c->x86_mask = dir1 & 0xf;

	/* Now cook; the original recipe is by Channing Corn, from Cyrix.
	 * We do the same thing for each generation: we work out
	 * the model, multiplier and stepping.  Black magic included,
	 * to make the silicon step/rev numbers match the printed ones.
	 */
	 
	switch (dir0_msn) {
		unsigned char tmp;

	case 0: /* Cx486SLC/DLC/SRx/DRx */
		p = Cx486_name[dir0_lsn & 7];
		break;

	case 1: /* Cx486S/DX/DX2/DX4 */
		p = (dir0_lsn & 8) ? Cx486D_name[dir0_lsn & 5]
			: Cx486S_name[dir0_lsn & 3];
		break;

	case 2: /* 5x86 */
		Cx86_cb[2] = cyrix_model_mult1[dir0_lsn & 5];
		p = Cx86_cb+2;
		break;

	case 3: /* 6x86/6x86L */
		Cx86_cb[1] = ' ';
		Cx86_cb[2] = cyrix_model_mult1[dir0_lsn & 5];
		if (dir1 > 0x21) { /* 686L */
			Cx86_cb[0] = 'L';
			p = Cx86_cb;
			(c->x86_model)++;
		} else             /* 686 */
			p = Cx86_cb+1;
		/* Emulate MTRRs using Cyrix's ARRs. */
		set_bit(X86_FEATURE_CYRIX_ARR, &c->x86_capability);
		/* 6x86's contain this bug */
		c->coma_bug = 1;
		break;

	case 4: /* MediaGX/GXm */
#ifdef CONFIG_PCI
		/* It isnt really a PCI quirk directly, but the cure is the
		   same. The MediaGX has deep magic SMM stuff that handles the
		   SB emulation. It thows away the fifo on disable_dma() which
		   is wrong and ruins the audio. 
                   
		   Bug2: VSA1 has a wrap bug so that using maximum sized DMA 
		   causes bad things. According to NatSemi VSA2 has another
		   bug to do with 'hlt'. I've not seen any boards using VSA2
		   and X doesn't seem to support it either so who cares 8).
		   VSA1 we work around however.
		*/

		printk(KERN_INFO "Working around Cyrix MediaGX virtual DMA bugs.\n");
		isa_dma_bridge_buggy = 2;
#endif		
		c->x86_cache_size=16;	/* Yep 16K integrated cache thats it */

		/* GXm supports extended cpuid levels 'ala' AMD */
		if (c->cpuid_level == 2) {
			get_model_name(c);  /* get CPU marketing name */
			/*
	 		 *	The 5510/5520 companion chips have a funky PIT
			 *	that breaks the TSC synchronizing, so turn it off
			 */
			if(pci_find_device(PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5510, NULL) ||
			   pci_find_device(PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5520, NULL))
				clear_bit(X86_FEATURE_TSC, c->x86_capability);
			return;
		}
		else {  /* MediaGX */
			Cx86_cb[2] = (dir0_lsn & 1) ? '3' : '4';
			p = Cx86_cb+2;
			c->x86_model = (dir1 & 0x20) ? 1 : 2;
			if(pci_find_device(PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5510, NULL) ||
			   pci_find_device(PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5520, NULL))
				clear_bit(X86_FEATURE_TSC, &c->x86_capability);
		}
		break;

        case 5: /* 6x86MX/M II */
		if (dir1 > 7)
		{
			dir0_msn++;  /* M II */
			/* Enable MMX extensions (App note 108) */
			setCx86(CX86_CCR7, getCx86(CX86_CCR7)|1);
		}
		else
		{
			c->coma_bug = 1;      /* 6x86MX, it has the bug. */
		}
		tmp = (!(dir0_lsn & 7) || dir0_lsn & 1) ? 2 : 0;
		Cx86_cb[tmp] = cyrix_model_mult2[dir0_lsn & 7];
		p = Cx86_cb+tmp;
        	if (((dir1 & 0x0f) > 4) || ((dir1 & 0xf0) == 0x20))
			(c->x86_model)++;
		/* Emulate MTRRs using Cyrix's ARRs. */
		set_bit(X86_FEATURE_CYRIX_ARR, &c->x86_capability);
		break;

	case 0xf:  /* Cyrix 486 without DEVID registers */
		switch (dir0_lsn) {
		case 0xd:  /* either a 486SLC or DLC w/o DEVID */
			dir0_msn = 0;
			p = Cx486_name[(c->hard_math) ? 1 : 0];
			break;

		case 0xe:  /* a 486S A step */
			dir0_msn = 0;
			p = Cx486S_name[0];
			break;
		}
		break;

	default:  /* unknown (shouldn't happen, we know everyone ;-) */
		dir0_msn = 7;
		break;
	}
	strcpy(buf, Cx86_model[dir0_msn & 7]);
	if (p) strcat(buf, p);
	return;
}

#ifdef CONFIG_X86_OOSTORE

static u32 __init power2(u32 x)
{
	u32 s=1;
	while(s<=x)
		s<<=1;
	return s>>=1;
}

/*
 *	Set up an actual MCR
 */
 
static void __init winchip_mcr_insert(int reg, u32 base, u32 size, int key)
{
	u32 lo, hi;
	
	hi = base & ~0xFFF;
	lo = ~(size-1);		/* Size is a power of 2 so this makes a mask */
	lo &= ~0xFFF;		/* Remove the ctrl value bits */
	lo |= key;		/* Attribute we wish to set */
	wrmsr(reg+MSR_IDT_MCR0, lo, hi);
	mtrr_centaur_report_mcr(reg, lo, hi);	/* Tell the mtrr driver */
}

/*
 *	Figure what we can cover with MCR's
 *
 *	Shortcut: We know you can't put 4Gig of RAM on a winchip
 */

static u32 __init ramtop(void)		/* 16388 */
{
	int i;
	u32 top = 0;
	u32 clip = 0xFFFFFFFFUL;
	
	for (i = 0; i < e820.nr_map; i++) {
		unsigned long start, end;

		if (e820.map[i].addr > 0xFFFFFFFFUL)
			continue;
		/*
		 *	Don't MCR over reserved space. Ignore the ISA hole
		 *	we frob around that catastrophy already
		 */
		 			
		if (e820.map[i].type == E820_RESERVED)
		{
			if(e820.map[i].addr >= 0x100000UL && e820.map[i].addr < clip)
				clip = e820.map[i].addr;
			continue;
		}
		start = e820.map[i].addr;
		end = e820.map[i].addr + e820.map[i].size;
		if (start >= end)
			continue;
		if (end > top)
			top = end;
	}
	/* Everything below 'top' should be RAM except for the ISA hole.
	   Because of the limited MCR's we want to map NV/ACPI into our
	   MCR range for gunk in RAM 
	   
	   Clip might cause us to MCR insufficient RAM but that is an
	   acceptable failure mode and should only bite obscure boxes with
	   a VESA hole at 15Mb
	   
	   The second case Clip sometimes kicks in is when the EBDA is marked
	   as reserved. Again we fail safe with reasonable results
	*/
	
	if(top>clip)
		top=clip;
		
	return top;
}

/*
 *	Compute a set of MCR's to give maximum coverage
 */

static int __init winchip_mcr_compute(int nr, int key)
{
	u32 mem = ramtop();
	u32 root = power2(mem);
	u32 base = root;
	u32 top = root;
	u32 floor = 0;
	int ct = 0;
	
	while(ct<nr)
	{
		u32 fspace = 0;

		/*
		 *	Find the largest block we will fill going upwards
		 */

		u32 high = power2(mem-top);	

		/*
		 *	Find the largest block we will fill going downwards
		 */

		u32 low = base/2;

		/*
		 *	Don't fill below 1Mb going downwards as there
		 *	is an ISA hole in the way.
		 */		
		 
		if(base <= 1024*1024)
			low = 0;
			
		/*
		 *	See how much space we could cover by filling below
		 *	the ISA hole
		 */
		 
		if(floor == 0)
			fspace = 512*1024;
		else if(floor ==512*1024)
			fspace = 128*1024;

		/* And forget ROM space */
		
		/*
		 *	Now install the largest coverage we get
		 */
		 
		if(fspace > high && fspace > low)
		{
			winchip_mcr_insert(ct, floor, fspace, key);
			floor += fspace;
		}
		else if(high > low)
		{
			winchip_mcr_insert(ct, top, high, key);
			top += high;
		}
		else if(low > 0)
		{
			base -= low;
			winchip_mcr_insert(ct, base, low, key);
		}
		else break;
		ct++;
	}
	/*
	 *	We loaded ct values. We now need to set the mask. The caller
	 *	must do this bit.
	 */
	 
	return ct;
}

static void __init winchip_create_optimal_mcr(void)
{
	int i;
	/*
	 *	Allocate up to 6 mcrs to mark as much of ram as possible
	 *	as write combining and weak write ordered.
	 *
	 *	To experiment with: Linux never uses stack operations for 
	 *	mmio spaces so we could globally enable stack operation wc
	 *
	 *	Load the registers with type 31 - full write combining, all
	 *	writes weakly ordered.
	 */
	int used = winchip_mcr_compute(6, 31);

	/*
	 *	Wipe unused MCRs
	 */
	 
	for(i=used;i<8;i++)
		wrmsr(MSR_IDT_MCR0+i, 0, 0);
}

static void __init winchip2_create_optimal_mcr(void)
{
	u32 lo, hi;
	int i;

	/*
	 *	Allocate up to 6 mcrs to mark as much of ram as possible
	 *	as write combining, weak store ordered.
	 *
	 *	Load the registers with type 25
	 *		8	-	weak write ordering
	 *		16	-	weak read ordering
	 *		1	-	write combining
	 */

	int used = winchip_mcr_compute(6, 25);
	
	/*
	 *	Mark the registers we are using.
	 */
	 
	rdmsr(MSR_IDT_MCR_CTRL, lo, hi);
	for(i=0;i<used;i++)
		lo|=1<<(9+i);
	wrmsr(MSR_IDT_MCR_CTRL, lo, hi);
	
	/*
	 *	Wipe unused MCRs
	 */
	 
	for(i=used;i<8;i++)
		wrmsr(MSR_IDT_MCR0+i, 0, 0);
}

/*
 *	Handle the MCR key on the Winchip 2.
 */

static void __init winchip2_unprotect_mcr(void)
{
	u32 lo, hi;
	u32 key;
	
	rdmsr(MSR_IDT_MCR_CTRL, lo, hi);
	lo&=~0x1C0;	/* blank bits 8-6 */
	key = (lo>>17) & 7;
	lo |= key<<6;	/* replace with unlock key */
	wrmsr(MSR_IDT_MCR_CTRL, lo, hi);
}

static void __init winchip2_protect_mcr(void)
{
	u32 lo, hi;
	
	rdmsr(MSR_IDT_MCR_CTRL, lo, hi);
	lo&=~0x1C0;	/* blank bits 8-6 */
	wrmsr(MSR_IDT_MCR_CTRL, lo, hi);
}
	
#endif

static void __init init_c3(struct cpuinfo_x86 *c)
{
	u32  lo, hi;

	/* Test for Centaur Extended Feature Flags presence */
	if (cpuid_eax(0xC0000000) >= 0xC0000001) {
		/* store Centaur Extended Feature Flags as
		 * word 5 of the CPU capability bit array
		 */
		c->x86_capability[5] = cpuid_edx(0xC0000001);
	}

	switch (c->x86_model) {
		case 6 ... 8:		/* Cyrix III family */
			rdmsr (MSR_VIA_FCR, lo, hi);
			lo |= (1<<1 | 1<<7);	/* Report CX8 & enable PGE */
			wrmsr (MSR_VIA_FCR, lo, hi);

			set_bit(X86_FEATURE_CX8, c->x86_capability);
			set_bit(X86_FEATURE_3DNOW, c->x86_capability);

			/* fall through */

		case 9:	/* Nehemiah */
		default:
			get_model_name(c);
			display_cacheinfo(c);
			break;
	}
}

static void __init init_centaur(struct cpuinfo_x86 *c)
{
	enum {
		ECX8=1<<1,
		EIERRINT=1<<2,
		DPM=1<<3,
		DMCE=1<<4,
		DSTPCLK=1<<5,
		ELINEAR=1<<6,
		DSMC=1<<7,
		DTLOCK=1<<8,
		EDCTLB=1<<8,
		EMMX=1<<9,
		DPDC=1<<11,
		EBRPRED=1<<12,
		DIC=1<<13,
		DDC=1<<14,
		DNA=1<<15,
		ERETSTK=1<<16,
		E2MMX=1<<19,
		EAMD3D=1<<20,
	};

	char *name;
	u32  fcr_set=0;
	u32  fcr_clr=0;
	u32  lo,hi,newlo;
	u32  aa,bb,cc,dd;

	/* Bit 31 in normal CPUID used for nonstandard 3DNow ID;
	   3DNow is IDd by bit 31 in extended CPUID (1*32+31) anyway */
	clear_bit(0*32+31, &c->x86_capability);

	switch (c->x86) {

		case 5:
			switch(c->x86_model) {
			case 4:
				name="C6";
				fcr_set=ECX8|DSMC|EDCTLB|EMMX|ERETSTK;
				fcr_clr=DPDC;
				printk(KERN_NOTICE "Disabling bugged TSC.\n");
				clear_bit(X86_FEATURE_TSC, &c->x86_capability);
#ifdef CONFIG_X86_OOSTORE
				winchip_create_optimal_mcr();
				/* Enable
					write combining on non-stack, non-string
					write combining on string, all types
					weak write ordering 
					
				   The C6 original lacks weak read order 
				   
				   Note 0x120 is write only on Winchip 1 */
				   
				wrmsr(MSR_IDT_MCR_CTRL, 0x01F0001F, 0);
#endif				
				break;
			case 8:
				switch(c->x86_mask) {
				default:
					name="2";
					break;
				case 7 ... 9:
					name="2A";
					break;
				case 10 ... 15:
					name="2B";
					break;
				}
				fcr_set=ECX8|DSMC|DTLOCK|EMMX|EBRPRED|ERETSTK|E2MMX|EAMD3D;
				fcr_clr=DPDC;
#ifdef CONFIG_X86_OOSTORE
				winchip2_unprotect_mcr();
				winchip2_create_optimal_mcr();
				rdmsr(MSR_IDT_MCR_CTRL, lo, hi);
				/* Enable
					write combining on non-stack, non-string
					write combining on string, all types
					weak write ordering 
				*/
				lo|=31;				
				wrmsr(MSR_IDT_MCR_CTRL, lo, hi);
				winchip2_protect_mcr();
#endif
				break;
			case 9:
				name="3";
				fcr_set=ECX8|DSMC|DTLOCK|EMMX|EBRPRED|ERETSTK|E2MMX|EAMD3D;
				fcr_clr=DPDC;
#ifdef CONFIG_X86_OOSTORE
				winchip2_unprotect_mcr();
				winchip2_create_optimal_mcr();
				rdmsr(MSR_IDT_MCR_CTRL, lo, hi);
				/* Enable
					write combining on non-stack, non-string
					write combining on string, all types
					weak write ordering 
				*/
				lo|=31;				
				wrmsr(MSR_IDT_MCR_CTRL, lo, hi);
				winchip2_protect_mcr();
#endif
				break;
			case 10:
				name="4";
				/* no info on the WC4 yet */
				break;
			default:
				name="??";
			}

			rdmsr(MSR_IDT_FCR1, lo, hi);
			newlo=(lo|fcr_set) & (~fcr_clr);

			if (newlo!=lo) {
				printk(KERN_INFO "Centaur FCR was 0x%X now 0x%X\n", lo, newlo );
				wrmsr(MSR_IDT_FCR1, newlo, hi );
			} else {
				printk(KERN_INFO "Centaur FCR is 0x%X\n",lo);
			}
			/* Emulate MTRRs using Centaur's MCR. */
			set_bit(X86_FEATURE_CENTAUR_MCR, &c->x86_capability);
			/* Report CX8 */
			set_bit(X86_FEATURE_CX8, &c->x86_capability);
			/* Set 3DNow! on Winchip 2 and above. */
			if (c->x86_model >=8)
				set_bit(X86_FEATURE_3DNOW, &c->x86_capability);
			/* See if we can find out some more. */
			if ( cpuid_eax(0x80000000) >= 0x80000005 ) {
				/* Yes, we can. */
				cpuid(0x80000005,&aa,&bb,&cc,&dd);
				/* Add L1 data and code cache sizes. */
				c->x86_cache_size = (cc>>24)+(dd>>24);
			}
			sprintf( c->x86_model_id, "WinChip %s", name );
			break;

		case 6:
			init_c3(c);
			break;
	}
}


static void __init init_transmeta(struct cpuinfo_x86 *c)
{
	unsigned int cap_mask, uk, max, dummy;
	unsigned int cms_rev1, cms_rev2;
	unsigned int cpu_rev, cpu_freq, cpu_flags;
	char cpu_info[65];

	get_model_name(c);	/* Same as AMD/Cyrix */
	display_cacheinfo(c);

	/* Print CMS and CPU revision */
	max = cpuid_eax(0x80860000);
	if ( max >= 0x80860001 ) {
		cpuid(0x80860001, &dummy, &cpu_rev, &cpu_freq, &cpu_flags); 
		printk(KERN_INFO "CPU: Processor revision %u.%u.%u.%u, %u MHz\n",
		       (cpu_rev >> 24) & 0xff,
		       (cpu_rev >> 16) & 0xff,
		       (cpu_rev >> 8) & 0xff,
		       cpu_rev & 0xff,
		       cpu_freq);
	}
	if ( max >= 0x80860002 ) {
		cpuid(0x80860002, &dummy, &cms_rev1, &cms_rev2, &dummy);
		printk(KERN_INFO "CPU: Code Morphing Software revision %u.%u.%u-%u-%u\n",
		       (cms_rev1 >> 24) & 0xff,
		       (cms_rev1 >> 16) & 0xff,
		       (cms_rev1 >> 8) & 0xff,
		       cms_rev1 & 0xff,
		       cms_rev2);
	}
	if ( max >= 0x80860006 ) {
		cpuid(0x80860003,
		      (void *)&cpu_info[0],
		      (void *)&cpu_info[4],
		      (void *)&cpu_info[8],
		      (void *)&cpu_info[12]);
		cpuid(0x80860004,
		      (void *)&cpu_info[16],
		      (void *)&cpu_info[20],
		      (void *)&cpu_info[24],
		      (void *)&cpu_info[28]);
		cpuid(0x80860005,
		      (void *)&cpu_info[32],
		      (void *)&cpu_info[36],
		      (void *)&cpu_info[40],
		      (void *)&cpu_info[44]);
		cpuid(0x80860006,
		      (void *)&cpu_info[48],
		      (void *)&cpu_info[52],
		      (void *)&cpu_info[56],
		      (void *)&cpu_info[60]);
		cpu_info[64] = '\0';
		printk(KERN_INFO "CPU: %s\n", cpu_info);
	}

	/* Unhide possibly hidden capability flags */
	rdmsr(0x80860004, cap_mask, uk);
	wrmsr(0x80860004, ~0, uk);
	c->x86_capability[0] = cpuid_edx(0x00000001);
	wrmsr(0x80860004, cap_mask, uk);

	/* If we can run i686 user-space code, call us an i686 */
#define USER686 (X86_FEATURE_TSC|X86_FEATURE_CX8|X86_FEATURE_CMOV)
	if ( c->x86 == 5 && (c->x86_capability[0] & USER686) == USER686 )
	     c->x86 = 6;
}


static void __init init_rise(struct cpuinfo_x86 *c)
{
	printk("CPU: Rise iDragon");
	if (c->x86_model > 2)
		printk(" II");
	printk("\n");

	/* Unhide possibly hidden capability flags
	   The mp6 iDragon family don't have MSRs.
	   We switch on extra features with this cpuid weirdness: */
	__asm__ (
		"movl $0x6363452a, %%eax\n\t"
		"movl $0x3231206c, %%ecx\n\t"
		"movl $0x2a32313a, %%edx\n\t"
		"cpuid\n\t"
		"movl $0x63634523, %%eax\n\t"
		"movl $0x32315f6c, %%ecx\n\t"
		"movl $0x2333313a, %%edx\n\t"
		"cpuid\n\t" : : : "eax", "ebx", "ecx", "edx"
	);
	set_bit(X86_FEATURE_CX8, &c->x86_capability);
}


extern void trap_init_f00f_bug(void);

#define LVL_1_INST      1
#define LVL_1_DATA      2
#define LVL_2           3
#define LVL_3           4
#define LVL_TRACE       5

struct _cache_table
{
        unsigned char descriptor;
        char cache_type;
        short size;
};

/* all the cache descriptor types we care about (no TLB or trace cache entries) */
static struct _cache_table cache_table[] __initdata =
{
	{ 0x06, LVL_1_INST, 8 },
	{ 0x08, LVL_1_INST, 16 },
	{ 0x0A, LVL_1_DATA, 8 },
	{ 0x0C, LVL_1_DATA, 16 },
	{ 0x22, LVL_3,      512 },
	{ 0x23, LVL_3,      1024 },
	{ 0x25, LVL_3,      2048 },
	{ 0x29, LVL_3,      4096 },
	{ 0x2c, LVL_1_DATA, 32 },
	{ 0x30, LVL_1_INST, 32 },
	{ 0x39, LVL_2,      128 },
	{ 0x3b, LVL_2,      128 },
	{ 0x3C, LVL_2,      256 },
	{ 0x41, LVL_2,      128 },
	{ 0x42, LVL_2,      256 },
	{ 0x43, LVL_2,      512 },
	{ 0x44, LVL_2,      1024 },
	{ 0x45, LVL_2,      2048 },
	{ 0x66, LVL_1_DATA, 8 },
	{ 0x67, LVL_1_DATA, 16 },
	{ 0x68, LVL_1_DATA, 32 },
	{ 0x70, LVL_TRACE,  12 },
	{ 0x71, LVL_TRACE,  16 },
	{ 0x72, LVL_TRACE,  32 },
	{ 0x79, LVL_2,      128 },
	{ 0x7A, LVL_2,      256 },
	{ 0x7B, LVL_2,      512 },
	{ 0x7C, LVL_2,      1024 },
	{ 0x82, LVL_2,      256 },
	{ 0x83, LVL_2,      512 },
	{ 0x84, LVL_2,      1024 },
	{ 0x85, LVL_2,      2048 },
	{ 0x86, LVL_2,      512 },
	{ 0x87, LVL_2,      1024 },
	{ 0x00, 0, 0}
};

static void __init init_intel(struct cpuinfo_x86 *c)
{
	unsigned int trace = 0, l1i = 0, l1d = 0, l2 = 0, l3 = 0; /* Cache sizes */
	char *p = NULL;
#ifndef CONFIG_X86_F00F_WORKS_OK
	static int f00f_workaround_enabled = 0;

	/*
	 * All current models of Pentium and Pentium with MMX technology CPUs
	 * have the F0 0F bug, which lets nonpriviledged users lock up the system.
	 * Note that the workaround only should be initialized once...
	 */
	c->f00f_bug = 0;
	if (c->x86 == 5) {
		c->f00f_bug = 1;
		if (!f00f_workaround_enabled) {
			trap_init_f00f_bug();
			printk(KERN_NOTICE "Intel Pentium with F0 0F bug - workaround enabled.\n");
			f00f_workaround_enabled = 1;
		}
	}
#endif /* CONFIG_X86_F00F_WORKS_OK */

	if (c->cpuid_level > 1) {
		/* supports eax=2  call */
		int i, j, n;
		int regs[4];
		unsigned char *dp = (unsigned char *)regs;

		/* Number of times to iterate */
		n = cpuid_eax(2) & 0xFF;

		for ( i = 0 ; i < n ; i++ ) {
			cpuid(2, &regs[0], &regs[1], &regs[2], &regs[3]);
			
			/* If bit 31 is set, this is an unknown format */
			for ( j = 0 ; j < 3 ; j++ ) {
				if ( regs[j] < 0 ) regs[j] = 0;
			}

			/* Byte 0 is level count, not a descriptor */
			for ( j = 1 ; j < 16 ; j++ ) {
				unsigned char des = dp[j];
				unsigned char k = 0;

				/* look up this descriptor in the table */
				while (cache_table[k].descriptor != 0)
				{
					if (cache_table[k].descriptor == des) {
						switch (cache_table[k].cache_type) {
						case LVL_1_INST:
							l1i += cache_table[k].size;
							break;
						case LVL_1_DATA:
							l1d += cache_table[k].size;
							break;
						case LVL_2:
							l2 += cache_table[k].size;
							break;
						case LVL_3:
							l3 += cache_table[k].size;
							break;
						case LVL_TRACE:
							trace += cache_table[k].size;
							break;
						}
						break;
					}

					k++;
				}
			}
		}

		/* Intel PIII Tualatin. This comes in two flavours.
		 * One has 256kb of cache, the other 512. We have no way
		 * to determine which, so we use a boottime override
		 * for the 512kb model, and assume 256 otherwise.
		 */
		if ((c->x86 == 6) && (c->x86_model == 11) && (l2 == 0))
			l2 = 256;
		/* Allow user to override all this if necessary. */
		if (cachesize_override != -1)
			l2 = cachesize_override;

		if ( trace )
			printk (KERN_INFO "CPU: Trace cache: %dK uops", trace);
		else if ( l1i )
			printk (KERN_INFO "CPU: L1 I cache: %dK", l1i);
		if ( l1d )
			printk(", L1 D cache: %dK\n", l1d);

		if ( l2 )
			printk(KERN_INFO "CPU: L2 cache: %dK\n", l2);
		if ( l3 )
			printk(KERN_INFO "CPU: L3 cache: %dK\n", l3);

		/*
		 * This assumes the L3 cache is shared; it typically lives in
		 * the northbridge.  The L1 caches are included by the L2
		 * cache, and so should not be included for the purpose of
		 * SMP switching weights.
		 */
		c->x86_cache_size = l2 ? l2 : (l1i+l1d);
	}

	/* SEP CPUID bug: Pentium Pro reports SEP but doesn't have it */
	if ( c->x86 == 6 && c->x86_model < 3 && c->x86_mask < 3 )
		clear_bit(X86_FEATURE_SEP, &c->x86_capability);
	
	/* Names for the Pentium II/Celeron processors 
	   detectable only by also checking the cache size.
	   Dixon is NOT a Celeron. */
	if (c->x86 == 6) {
		switch (c->x86_model) {
		case 5:
			if (l2 == 0)
				p = "Celeron (Covington)";
			if (l2 == 256)
				p = "Mobile Pentium II (Dixon)";
			break;
			
		case 6:
			if (l2 == 128)
				p = "Celeron (Mendocino)";
			break;
			
		case 8:
			if (l2 == 128)
				p = "Celeron (Coppermine)";
			break;
		}
	}

	if ( p )
		strcpy(c->x86_model_id, p);
	
#ifdef CONFIG_SMP
	if (test_bit(X86_FEATURE_HT, &c->x86_capability)) {
		extern	int phys_proc_id[NR_CPUS];
		
		u32 	eax, ebx, ecx, edx;
		int 	index_lsb, index_msb, tmp;
		int	initial_apic_id;
		int 	cpu = smp_processor_id();

		cpuid(1, &eax, &ebx, &ecx, &edx);
		smp_num_siblings = (ebx & 0xff0000) >> 16;

		if (smp_num_siblings == 1) {
			printk(KERN_INFO  "CPU: Hyper-Threading is disabled\n");
		} else if (smp_num_siblings > 1 ) {
			index_lsb = 0;
			index_msb = 31;
			/*
			 * At this point we only support two siblings per
			 * processor package.
			 */
#define NR_SIBLINGS	2
			if (smp_num_siblings != NR_SIBLINGS) {
				printk(KERN_WARNING "CPU: Unsupported number of the siblings %d", smp_num_siblings);
				smp_num_siblings = 1;
				return;
			}
			tmp = smp_num_siblings;
			while ((tmp & 1) == 0) {
				tmp >>=1 ;
				index_lsb++;
			}
			tmp = smp_num_siblings;
			while ((tmp & 0x80000000 ) == 0) {
				tmp <<=1 ;
				index_msb--;
			}
			if (index_lsb != index_msb )
				index_msb++;
			initial_apic_id = ebx >> 24 & 0xff;
			phys_proc_id[cpu] = initial_apic_id >> index_msb;

			printk(KERN_INFO  "CPU: Physical Processor ID: %d\n",
                               phys_proc_id[cpu]);
		}

	}
#endif
}

void __init get_cpu_vendor(struct cpuinfo_x86 *c)
{
	char *v = c->x86_vendor_id;

	if (!strcmp(v, "GenuineIntel"))
		c->x86_vendor = X86_VENDOR_INTEL;
	else if (!strcmp(v, "AuthenticAMD"))
		c->x86_vendor = X86_VENDOR_AMD;
	else if (!strcmp(v, "CyrixInstead"))
		c->x86_vendor = X86_VENDOR_CYRIX;
	else if (!strcmp(v, "Geode by NSC"))
		c->x86_vendor = X86_VENDOR_NSC;
	else if (!strcmp(v, "UMC UMC UMC "))
		c->x86_vendor = X86_VENDOR_UMC;
	else if (!strcmp(v, "CentaurHauls"))
		c->x86_vendor = X86_VENDOR_CENTAUR;
	else if (!strcmp(v, "NexGenDriven"))
		c->x86_vendor = X86_VENDOR_NEXGEN;
	else if (!strcmp(v, "RiseRiseRise"))
		c->x86_vendor = X86_VENDOR_RISE;
	else if (!strcmp(v, "GenuineTMx86") ||
		 !strcmp(v, "TransmetaCPU"))
		c->x86_vendor = X86_VENDOR_TRANSMETA;
	else if (!strcmp(v, "SiS SiS SiS "))
		c->x86_vendor = X86_VENDOR_SIS;
	else
		c->x86_vendor = X86_VENDOR_UNKNOWN;
}

struct cpu_model_info {
	int vendor;
	int family;
	char *model_names[16];
};

/* Naming convention should be: <Name> [(<Codename>)] */
/* This table only is used unless init_<vendor>() below doesn't set it; */
/* in particular, if CPUID levels 0x80000002..4 are supported, this isn't used */
static struct cpu_model_info cpu_models[] __initdata = {
	{ X86_VENDOR_INTEL,	4,
	  { "486 DX-25/33", "486 DX-50", "486 SX", "486 DX/2", "486 SL", 
	    "486 SX/2", NULL, "486 DX/2-WB", "486 DX/4", "486 DX/4-WB", NULL, 
	    NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_INTEL,	5,
	  { "Pentium 60/66 A-step", "Pentium 60/66", "Pentium 75 - 200",
	    "OverDrive PODP5V83", "Pentium MMX", NULL, NULL,
	    "Mobile Pentium 75 - 200", "Mobile Pentium MMX", NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_INTEL,	6,
	  { "Pentium Pro A-step", "Pentium Pro", NULL, "Pentium II (Klamath)", 
	    NULL, "Pentium II (Deschutes)", "Mobile Pentium II",
	    "Pentium III (Katmai)", "Pentium III (Coppermine)", NULL,
	    "Pentium III (Cascades)", NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_AMD,	4,
	  { NULL, NULL, NULL, "486 DX/2", NULL, NULL, NULL, "486 DX/2-WB",
	    "486 DX/4", "486 DX/4-WB", NULL, NULL, NULL, NULL, "Am5x86-WT",
	    "Am5x86-WB" }},
	{ X86_VENDOR_AMD,	5, /* Is this this really necessary?? */
	  { "K5/SSA5", "K5",
	    "K5", "K5", NULL, NULL,
	    "K6", "K6", "K6-2",
	    "K6-3", NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_AMD,	6, /* Is this this really necessary?? */
	  { "Athlon", "Athlon",
	    "Athlon", NULL, "Athlon", NULL,
	    NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_UMC,	4,
	  { NULL, "U5D", "U5S", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_NEXGEN,	5,
	  { "Nx586", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_RISE,	5,
	  { "iDragon", NULL, "iDragon", NULL, NULL, NULL, NULL,
	    NULL, "iDragon II", "iDragon II", NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_SIS,	5,
	  { NULL, NULL, NULL, NULL, "SiS55x", NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
};

/* Look up CPU names by table lookup. */
static char __init *table_lookup_model(struct cpuinfo_x86 *c)
{
	struct cpu_model_info *info = cpu_models;
	int i;

	if ( c->x86_model >= 16 )
		return NULL;	/* Range check */

	for ( i = 0 ; i < sizeof(cpu_models)/sizeof(struct cpu_model_info) ; i++ ) {
		if ( info->vendor == c->x86_vendor &&
		     info->family == c->x86 ) {
			return info->model_names[c->x86_model];
		}
		info++;
	}
	return NULL;		/* Not found */
}

/*
 *	Detect a NexGen CPU running without BIOS hypercode new enough
 *	to have CPUID. (Thanks to Herbert Oppmann)
 */
 
static int __init deep_magic_nexgen_probe(void)
{
	int ret;
	
	__asm__ __volatile__ (
		"	movw	$0x5555, %%ax\n"
		"	xorw	%%dx,%%dx\n"
		"	movw	$2, %%cx\n"
		"	divw	%%cx\n"
		"	movl	$0, %%eax\n"
		"	jnz	1f\n"
		"	movl	$1, %%eax\n"
		"1:\n" 
		: "=a" (ret) : : "cx", "dx" );
	return  ret;
}

static void __init squash_the_stupid_serial_number(struct cpuinfo_x86 *c)
{
	if( test_bit(X86_FEATURE_PN, &c->x86_capability) &&
	    disable_x86_serial_nr ) {
		/* Disable processor serial number */
		unsigned long lo,hi;
		rdmsr(MSR_IA32_BBL_CR_CTL,lo,hi);
		lo |= 0x200000;
		wrmsr(MSR_IA32_BBL_CR_CTL,lo,hi);
		printk(KERN_NOTICE "CPU serial number disabled.\n");
		clear_bit(X86_FEATURE_PN, &c->x86_capability);

		/* Disabling the serial number may affect the cpuid level */
		c->cpuid_level = cpuid_eax(0);
	}
}


static int __init x86_serial_nr_setup(char *s)
{
	disable_x86_serial_nr = 0;
	return 1;
}
__setup("serialnumber", x86_serial_nr_setup);

static int __init x86_fxsr_setup(char * s)
{
	set_bit(X86_FEATURE_XMM, disabled_x86_caps); 
	set_bit(X86_FEATURE_FXSR, disabled_x86_caps);
	return 1;
}
__setup("nofxsr", x86_fxsr_setup);


/* Standard macro to see if a specific flag is changeable */
static inline int flag_is_changeable_p(u32 flag)
{
	u32 f1, f2;

	asm("pushfl\n\t"
	    "pushfl\n\t"
	    "popl %0\n\t"
	    "movl %0,%1\n\t"
	    "xorl %2,%0\n\t"
	    "pushl %0\n\t"
	    "popfl\n\t"
	    "pushfl\n\t"
	    "popl %0\n\t"
	    "popfl\n\t"
	    : "=&r" (f1), "=&r" (f2)
	    : "ir" (flag));

	return ((f1^f2) & flag) != 0;
}


/* Probe for the CPUID instruction */
static int __init have_cpuid_p(void)
{
	return flag_is_changeable_p(X86_EFLAGS_ID);
}

/*
 * Cyrix CPUs without cpuid or with cpuid not yet enabled can be detected
 * by the fact that they preserve the flags across the division of 5/2.
 * PII and PPro exhibit this behavior too, but they have cpuid available.
 */
 
/*
 * Perform the Cyrix 5/2 test. A Cyrix won't change
 * the flags, while other 486 chips will.
 */
static inline int test_cyrix_52div(void)
{
	unsigned int test;

	__asm__ __volatile__(
	     "sahf\n\t"		/* clear flags (%eax = 0x0005) */
	     "div %b2\n\t"	/* divide 5 by 2 */
	     "lahf"		/* store flags into %ah */
	     : "=a" (test)
	     : "0" (5), "q" (2)
	     : "cc");

	/* AH is 0x02 on Cyrix after the divide.. */
	return (unsigned char) (test >> 8) == 0x02;
}

/* Try to detect a CPU with disabled CPUID, and if so, enable.  This routine
   may also be used to detect non-CPUID processors and fill in some of
   the information manually. */
static int __init id_and_try_enable_cpuid(struct cpuinfo_x86 *c)
{
	/* First of all, decide if this is a 486 or higher */
	/* It's a 486 if we can modify the AC flag */
	if ( flag_is_changeable_p(X86_EFLAGS_AC) )
		c->x86 = 4;
	else
		c->x86 = 3;

	/* Detect Cyrix with disabled CPUID */
	if ( c->x86 == 4 && test_cyrix_52div() ) {
		unsigned char dir0, dir1;
		
		strcpy(c->x86_vendor_id, "CyrixInstead");
	        c->x86_vendor = X86_VENDOR_CYRIX;
	        
	        /* Actually enable cpuid on the older cyrix */
	    
	    	/* Retrieve CPU revisions */
	    	
		do_cyrix_devid(&dir0, &dir1);

		dir0>>=4;		
		
		/* Check it is an affected model */
		
   	        if (dir0 == 5 || dir0 == 3)
   	        {
			unsigned char ccr3, ccr4;
			unsigned long flags;
			printk(KERN_INFO "Enabling CPUID on Cyrix processor.\n");
			local_irq_save(flags);
			ccr3 = getCx86(CX86_CCR3);
			setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10); /* enable MAPEN  */
			ccr4 = getCx86(CX86_CCR4);
			setCx86(CX86_CCR4, ccr4 | 0x80);          /* enable cpuid  */
			setCx86(CX86_CCR3, ccr3);                 /* disable MAPEN */
			local_irq_restore(flags);
		}
	} else

	/* Detect NexGen with old hypercode */
	if ( deep_magic_nexgen_probe() ) {
		strcpy(c->x86_vendor_id, "NexGenDriven");
	}

	return have_cpuid_p();	/* Check to see if CPUID now enabled? */
}

/*
 * This does the hard work of actually picking apart the CPU stuff...
 */
void __init identify_cpu(struct cpuinfo_x86 *c)
{
	int junk, i;
	u32 xlvl, tfms;

	c->loops_per_jiffy = loops_per_jiffy;
	c->x86_cache_size = -1;
	c->x86_vendor = X86_VENDOR_UNKNOWN;
	c->cpuid_level = -1;	/* CPUID not detected */
	c->x86_model = c->x86_mask = 0;	/* So far unknown... */
	c->x86_vendor_id[0] = '\0'; /* Unset */
	c->x86_model_id[0] = '\0';  /* Unset */
	memset(&c->x86_capability, 0, sizeof c->x86_capability);

	if ( !have_cpuid_p() && !id_and_try_enable_cpuid(c) ) {
		/* CPU doesn't have CPUID */

		/* If there are any capabilities, they're vendor-specific */
		/* enable_cpuid() would have set c->x86 for us. */
	} else {
		/* CPU does have CPUID */

		/* Get vendor name */
		cpuid(0x00000000, &c->cpuid_level,
		      (int *)&c->x86_vendor_id[0],
		      (int *)&c->x86_vendor_id[8],
		      (int *)&c->x86_vendor_id[4]);
		
		get_cpu_vendor(c);
		/* Initialize the standard set of capabilities */
		/* Note that the vendor-specific code below might override */

		/* Intel-defined flags: level 0x00000001 */
		if ( c->cpuid_level >= 0x00000001 ) {
			u32 capability, excap;
			cpuid(0x00000001, &tfms, &junk, &excap, &capability);
			c->x86_capability[0] = capability;
			c->x86_capability[4] = excap;
			c->x86 = (tfms >> 8) & 15;
			c->x86_model = (tfms >> 4) & 15;
			if (c->x86 == 0xf) {
				c->x86 += (tfms >> 20) & 0xff;
				c->x86_model += ((tfms >> 16) & 0xF) << 4;
			} 
			c->x86_mask = tfms & 15;
		} else {
			/* Have CPUID level 0 only - unheard of */
			c->x86 = 4;
		}

		/* AMD-defined flags: level 0x80000001 */
		xlvl = cpuid_eax(0x80000000);
		if ( (xlvl & 0xffff0000) == 0x80000000 ) {
			if ( xlvl >= 0x80000001 )
				c->x86_capability[1] = cpuid_edx(0x80000001);
			if ( xlvl >= 0x80000004 )
				get_model_name(c); /* Default name */
		}

		/* Transmeta-defined flags: level 0x80860001 */
		xlvl = cpuid_eax(0x80860000);
		if ( (xlvl & 0xffff0000) == 0x80860000 ) {
			if (  xlvl >= 0x80860001 )
				c->x86_capability[2] = cpuid_edx(0x80860001);
		}
	}

	/*
	 * Vendor-specific initialization.  In this section we
	 * canonicalize the feature flags, meaning if there are
	 * features a certain CPU supports which CPUID doesn't
	 * tell us, CPUID claiming incorrect flags, or other bugs,
	 * we handle them here.
	 *
	 * At the end of this section, c->x86_capability better
	 * indicate the features this CPU genuinely supports!
	 */
	switch ( c->x86_vendor ) {
	case X86_VENDOR_UNKNOWN:
	default:
		/* Not much we can do here... */
		/* Check if at least it has cpuid */
		if (c->cpuid_level == -1)
		{
			/* No cpuid. It must be an ancient CPU */
			if (c->x86 == 4)
				strcpy(c->x86_model_id, "486");
			else if (c->x86 == 3)
				strcpy(c->x86_model_id, "386");
		}
		break;

	case X86_VENDOR_CYRIX:
		init_cyrix(c);
		break;

	case X86_VENDOR_NSC:
	        init_cyrix(c);
		break;

	case X86_VENDOR_AMD:
		init_amd(c);
		break;

	case X86_VENDOR_CENTAUR:
		init_centaur(c);
		break;

	case X86_VENDOR_INTEL:
		init_intel(c);
		break;

	case X86_VENDOR_NEXGEN:
		c->x86_cache_size = 256; /* A few had 1 MB... */
		break;

	case X86_VENDOR_TRANSMETA:
		init_transmeta(c);
		break;

	case X86_VENDOR_RISE:
		init_rise(c);
		break;
	}

	/*
	 * The vendor-specific functions might have changed features.  Now
	 * we do "generic changes."
	 */

	/* TSC disabled? */
#ifndef CONFIG_X86_TSC
	if ( tsc_disable )
		clear_bit(X86_FEATURE_TSC, &c->x86_capability);
#endif

	/* check for caps that have been disabled earlier */ 
	for (i = 0; i < NCAPINTS; i++) { 
	     c->x86_capability[i] &= ~disabled_x86_caps[i];
	}

	/* Disable the PN if appropriate */
	squash_the_stupid_serial_number(c);

	/* Init Machine Check Exception if available. */
	mcheck_init(c);

	/* If the model name is still unset, do table lookup. */
	if ( !c->x86_model_id[0] ) {
		char *p;
		p = table_lookup_model(c);
		if ( p )
			strcpy(c->x86_model_id, p);
		else
			/* Last resort... */
			sprintf(c->x86_model_id, "%02x/%02x",
				c->x86_vendor, c->x86_model);
	}

	/* Now the feature flags better reflect actual CPU features! */

	printk(KERN_DEBUG "CPU:     After generic, caps: %08x %08x %08x %08x\n",
	       c->x86_capability[0],
	       c->x86_capability[1],
	       c->x86_capability[2],
	       c->x86_capability[3]);

	/*
	 * On SMP, boot_cpu_data holds the common feature set between
	 * all CPUs; so make sure that we indicate which features are
	 * common between the CPUs.  The first time this routine gets
	 * executed, c == &boot_cpu_data.
	 */
	if ( c != &boot_cpu_data ) {
		/* AND the already accumulated flags with these */
		for ( i = 0 ; i < NCAPINTS ; i++ )
			boot_cpu_data.x86_capability[i] &= c->x86_capability[i];
	}

	printk(KERN_DEBUG "CPU:             Common caps: %08x %08x %08x %08x\n",
	       boot_cpu_data.x86_capability[0],
	       boot_cpu_data.x86_capability[1],
	       boot_cpu_data.x86_capability[2],
	       boot_cpu_data.x86_capability[3]);
}
/*
 *	Perform early boot up checks for a valid TSC. See arch/i386/kernel/time.c
 */
 
void __init dodgy_tsc(void)
{
	get_cpu_vendor(&boot_cpu_data);

	if ( boot_cpu_data.x86_vendor == X86_VENDOR_CYRIX ||
	     boot_cpu_data.x86_vendor == X86_VENDOR_NSC )
		init_cyrix(&boot_cpu_data);
}


/* These need to match <asm/processor.h> */
static char *cpu_vendor_names[] __initdata = {
	"Intel", "Cyrix", "AMD", "UMC", "NexGen", 
	"Centaur", "Rise", "Transmeta", "NSC"
};


void __init print_cpu_info(struct cpuinfo_x86 *c)
{
	char *vendor = NULL;

	if (c->x86_vendor < sizeof(cpu_vendor_names)/sizeof(char *))
		vendor = cpu_vendor_names[c->x86_vendor];
	else if (c->cpuid_level >= 0)
		vendor = c->x86_vendor_id;

	if (vendor && strncmp(c->x86_model_id, vendor, strlen(vendor)))
		printk("%s ", vendor);

	if (!c->x86_model_id[0])
		printk("%d86", c->x86);
	else
		printk("%s", c->x86_model_id);

	if (c->x86_mask || c->cpuid_level >= 0) 
		printk(" stepping %02x\n", c->x86_mask);
	else
		printk("\n");
}

/*
 *	Get CPU information for use by the procfs.
 */
static int show_cpuinfo(struct seq_file *m, void *v)
{
	/* 
	 * These flag bits must match the definitions in <asm/cpufeature.h>.
	 * NULL means this bit is undefined or reserved; either way it doesn't
	 * have meaning as far as Linux is concerned.  Note that it's important
	 * to realize there is a difference between this table and CPUID -- if
	 * applications want to get the raw CPUID data, they should access
	 * /dev/cpu/<cpu_nr>/cpuid instead.
	 */
	static char *x86_cap_flags[] = {
		/* Intel-defined */
	        "fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce",
	        "cx8", "apic", NULL, "sep", "mtrr", "pge", "mca", "cmov",
	        "pat", "pse36", "pn", "clflush", NULL, "dts", "acpi", "mmx",
	        "fxsr", "sse", "sse2", "ss", "ht", "tm", "ia64", "pbe",

		/* AMD-defined */
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, "syscall", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, "mp", NULL, NULL, "mmxext", NULL,
		NULL, NULL, NULL, NULL, NULL, "lm", "3dnowext", "3dnow",

		/* Transmeta-defined */
		"recovery", "longrun", NULL, "lrti", NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Other (Linux-defined) */
		"cxmmx", "k6_mtrr", "cyrix_arr", "centaur_mcr",
		NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* Intel-defined (#2) */
		"pni", NULL, NULL, "monitor", "ds_cpl", NULL, NULL, "tm2",
		"est", NULL, "cid", NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		/* VIA/Cyrix/Centaur-defined */
		NULL, NULL, "xstore", NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	};
	struct cpuinfo_x86 *c = v;
	int i, n = c - cpu_data;
	int fpu_exception;

#ifdef CONFIG_SMP
	if (!(cpu_online_map & (1<<n)))
		return 0;
#endif
	seq_printf(m, "processor\t: %d\n"
		"vendor_id\t: %s\n"
		"cpu family\t: %d\n"
		"model\t\t: %d\n"
		"model name\t: %s\n",
		n,
		c->x86_vendor_id[0] ? c->x86_vendor_id : "unknown",
		c->x86,
		c->x86_model,
		c->x86_model_id[0] ? c->x86_model_id : "unknown");

	if (c->x86_mask || c->cpuid_level >= 0)
		seq_printf(m, "stepping\t: %d\n", c->x86_mask);
	else
		seq_printf(m, "stepping\t: unknown\n");

	if ( test_bit(X86_FEATURE_TSC, &c->x86_capability) ) {
		seq_printf(m, "cpu MHz\t\t: %lu.%03lu\n",
			cpu_khz / 1000, (cpu_khz % 1000));
	}

	/* Cache size */
	if (c->x86_cache_size >= 0)
		seq_printf(m, "cache size\t: %d KB\n", c->x86_cache_size);
	
	/* We use exception 16 if we have hardware math and we've either seen it or the CPU claims it is internal */
	fpu_exception = c->hard_math && (ignore_irq13 || cpu_has_fpu);
	seq_printf(m, "fdiv_bug\t: %s\n"
			"hlt_bug\t\t: %s\n"
			"f00f_bug\t: %s\n"
			"coma_bug\t: %s\n"
			"fpu\t\t: %s\n"
			"fpu_exception\t: %s\n"
			"cpuid level\t: %d\n"
			"wp\t\t: %s\n"
			"flags\t\t:",
		     c->fdiv_bug ? "yes" : "no",
		     c->hlt_works_ok ? "no" : "yes",
		     c->f00f_bug ? "yes" : "no",
		     c->coma_bug ? "yes" : "no",
		     c->hard_math ? "yes" : "no",
		     fpu_exception ? "yes" : "no",
		     c->cpuid_level,
		     c->wp_works_ok ? "yes" : "no");

	for ( i = 0 ; i < 32*NCAPINTS ; i++ )
		if ( test_bit(i, &c->x86_capability) &&
		     x86_cap_flags[i] != NULL )
			seq_printf(m, " %s", x86_cap_flags[i]);

	seq_printf(m, "\nbogomips\t: %lu.%02lu\n\n",
		     c->loops_per_jiffy/(500000/HZ),
		     (c->loops_per_jiffy/(5000/HZ)) % 100);
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? cpu_data + *pos : NULL;
}
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}
static void c_stop(struct seq_file *m, void *v)
{
}
struct seq_operations cpuinfo_op = {
	start:	c_start,
	next:	c_next,
	stop:	c_stop,
	show:	show_cpuinfo,
};

unsigned long cpu_initialized __initdata = 0;

/*
 * cpu_init() initializes state that is per-CPU. Some data is already
 * initialized (naturally) in the bootstrap process, such as the GDT
 * and IDT. We reload them nevertheless, this function acts as a
 * 'CPU state barrier', nothing should get across.
 */
void __init cpu_init (void)
{
	int nr = smp_processor_id();
	struct tss_struct * t = &init_tss[nr];

	if (test_and_set_bit(nr, &cpu_initialized)) {
		printk(KERN_WARNING "CPU#%d already initialized!\n", nr);
		for (;;) __sti();
	}
	printk(KERN_INFO "Initializing CPU#%d\n", nr);

	if (cpu_has_vme || cpu_has_tsc || cpu_has_de)
		clear_in_cr4(X86_CR4_VME|X86_CR4_PVI|X86_CR4_TSD|X86_CR4_DE);
#ifndef CONFIG_X86_TSC
	if (tsc_disable && cpu_has_tsc) {
		printk(KERN_NOTICE "Disabling TSC...\n");
		/**** FIX-HPA: DOES THIS REALLY BELONG HERE? ****/
		clear_bit(X86_FEATURE_TSC, boot_cpu_data.x86_capability);
		set_in_cr4(X86_CR4_TSD);
	}
#endif

	__asm__ __volatile__("lgdt %0": "=m" (gdt_descr));
	__asm__ __volatile__("lidt %0": "=m" (idt_descr));

	/*
	 * Delete NT
	 */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");

	/*
	 * set up and load the per-CPU TSS and LDT
	 */
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	if(current->mm)
		BUG();
	enter_lazy_tlb(&init_mm, current, nr);

	t->esp0 = current->thread.esp0;
	set_tss_desc(nr,t);
	gdt_table[__TSS(nr)].b &= 0xfffffdff;
	load_TR(nr);
	load_LDT(&init_mm.context);

	/*
	 * Clear all 6 debug registers:
	 */

#define CD(register) __asm__("movl %0,%%db" #register ::"r"(0) );

	CD(0); CD(1); CD(2); CD(3); /* no db4 and db5 */; CD(6); CD(7);

#undef CD

	/*
	 * Force FPU initialization:
	 */
	current->flags &= ~PF_USEDFPU;
	current->used_math = 0;
	stts();
}

/*
 *	Early probe support logic for ppro memory erratum #50
 *
 *	This is called before we do cpu ident work
 */
 
int __init ppro_with_ram_bug(void)
{
	char vendor_id[16];
	int ident;

	/* Must have CPUID */
	if(!have_cpuid_p())
		return 0;
	if(cpuid_eax(0)<1)
		return 0;
	
	/* Must be Intel */
	cpuid(0, &ident, 
		(int *)&vendor_id[0],
		(int *)&vendor_id[8],
		(int *)&vendor_id[4]);
	
	if(memcmp(vendor_id, "IntelInside", 12))
		return 0;
	
	ident = cpuid_eax(1);

	/* Model 6 */

	if(((ident>>8)&15)!=6)
		return 0;
	
	/* Pentium Pro */

	if(((ident>>4)&15)!=1)
		return 0;
	
	if((ident&15) < 8)
	{
		printk(KERN_INFO "Pentium Pro with Errata#50 detected. Taking evasive action.\n");
		return 1;
	}
	printk(KERN_INFO "Your Pentium Pro seems ok.\n");
	return 0;
}
	
/*
 * Local Variables:
 * mode:c
 * c-file-style:"k&r"
 * c-basic-offset:8
 * End:
 */
