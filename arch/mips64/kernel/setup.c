/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 Linus Torvalds
 * Copyright (C) 1995 Waldorf Electronics
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001 Ralf Baechle
 * Copyright (C) 1996 Stoned Elipot
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2002  Maciej W. Rozycki
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/utsname.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/bootmem.h>
#ifdef CONFIG_BLK_DEV_RAM
#include <linux/blk.h>
#endif

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/system.h>
#include <asm/pgalloc.h>

struct cpuinfo_mips cpu_data[NR_CPUS];

#ifdef CONFIG_VT
struct screen_info screen_info;
#endif

/*
 * Set if box has EISA slots.
 */
#ifdef CONFIG_EISA
int EISA_bus = 0;
#endif

#if defined(CONFIG_BLK_DEV_FD) || defined(CONFIG_BLK_DEV_FD_MODULE)
#include <asm/floppy.h>
extern struct fd_ops no_fd_ops;
struct fd_ops *fd_ops;
#endif

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
extern struct ide_ops no_ide_ops;
struct ide_ops *ide_ops;
#endif

extern void * __rd_start, * __rd_end;

extern struct rtc_ops no_rtc_ops;
struct rtc_ops *rtc_ops;

struct kbd_ops *kbd_ops;

/*
 * Setup information
 *
 * These are initialized so they are in the .data section
 */
unsigned long mips_machtype = MACH_UNKNOWN;
unsigned long mips_machgroup = MACH_GROUP_UNKNOWN;

struct boot_mem_map boot_mem_map;

unsigned char aux_device_present;

extern char _ftext, _etext, _fdata, _edata, _end;

static char command_line[CL_SIZE] = { 0, };
       char saved_command_line[CL_SIZE];
extern char arcs_cmdline[CL_SIZE];

/*
 * mips_io_port_base is the begin of the address space to which x86 style
 * I/O ports are mapped.
 */
const unsigned long mips_io_port_base = -1;
EXPORT_SYMBOL(mips_io_port_base);

/*
 * isa_slot_offset is the address where E(ISA) busaddress 0 is mapped
 * for the processor.
 */
unsigned long isa_slot_offset;
EXPORT_SYMBOL(isa_slot_offset);

extern void SetUpBootInfo(void);
extern void load_mmu(void);
extern ATTRIB_NORET asmlinkage void start_kernel(void);
extern void prom_init(int, char **, char **, int *);

static struct resource code_resource = { "Kernel code" };
static struct resource data_resource = { "Kernel data" };

asmlinkage void __init init_arch(int argc, char **argv, char **envp,
	int *prom_vec)
{
	/* Determine which MIPS variant we are running on. */
	cpu_probe();

	prom_init(argc, argv, envp, prom_vec);

	cpu_report();

	/*
	 * Determine the mmu/cache attached to this machine, then flush the
	 * tlb and caches.  On the r4xx0 variants this also sets CP0_WIRED to
	 * zero.
	 */
	load_mmu();

	/*
	 * On IP27, I am seeing the TS bit set when the kernel is loaded.
	 * Maybe because the kernel is in ckseg0 and not xkphys? Clear it
	 * anyway ...
	 */
	clear_c0_status(ST0_BEV|ST0_TS|ST0_CU1|ST0_CU2|ST0_CU3);
	set_c0_status(ST0_CU0|ST0_KX|ST0_SX|ST0_FR);

	start_kernel();
}

void __init add_memory_region(phys_t start, phys_t size,
			      long type)
{
	int x = boot_mem_map.nr_map;

	if (x == BOOT_MEM_MAP_MAX) {
		printk("Ooops! Too many entries in the memory map!\n");
		return;
	}

	boot_mem_map.map[x].addr = start;
	boot_mem_map.map[x].size = size;
	boot_mem_map.map[x].type = type;
	boot_mem_map.nr_map++;
}

static void __init print_memory_map(void)
{
	int i;

	for (i = 0; i < boot_mem_map.nr_map; i++) {
		printk(" memory: %016Lx @ %016Lx ",
			(unsigned long long) boot_mem_map.map[i].size,
			(unsigned long long) boot_mem_map.map[i].addr);
		switch (boot_mem_map.map[i].type) {
		case BOOT_MEM_RAM:
			printk("(usable)\n");
			break;
		case BOOT_MEM_ROM_DATA:
			printk("(ROM data)\n");
			break;
		case BOOT_MEM_RESERVED:
			printk("(reserved)\n");
			break;
		default:
			printk("type %lu\n", boot_mem_map.map[i].type);
			break;
		}
	}
}

static inline void parse_mem_cmdline(void)
{
	char c = ' ', *to = command_line, *from = saved_command_line;
	unsigned long start_at, mem_size;
	int len = 0;
	int usermem = 0;

	printk("Determined physical RAM map:\n");
	print_memory_map();

	for (;;) {
		/*
		 * "mem=XXX[kKmM]" defines a memory region from
		 * 0 to <XXX>, overriding the determined size.
		 * "mem=XXX[KkmM]@YYY[KkmM]" defines a memory region from
		 * <YYY> to <YYY>+<XXX>, overriding the determined size.
		 */
		if (c == ' ' && !memcmp(from, "mem=", 4)) {
			if (to != command_line)
				to--;
			/*
			 * If a user specifies memory size, we
			 * blow away any automatically generated
			 * size.
			 */
			if (usermem == 0) {
				boot_mem_map.nr_map = 0;
				usermem = 1;
			}
			mem_size = memparse(from + 4, &from);
			if (*from == '@')
				start_at = memparse(from + 1, &from);
			else
				start_at = 0;
			add_memory_region(start_at, mem_size, BOOT_MEM_RAM);
		}
		c = *(from++);
		if (!c)
			break;
		if (CL_SIZE <= ++len)
			break;
		*(to++) = c;
	}
	*to = '\0';

	if (usermem) {
		printk("User-defined physical RAM map:\n");
		print_memory_map();
	}
}


#define PFN_UP(x)	(((x) + PAGE_SIZE - 1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((x) << PAGE_SHIFT)

static inline void bootmem_init(void)
{
#ifdef CONFIG_BLK_DEV_INITRD
	unsigned long tmp;
	unsigned long *initrd_header;
#endif
	unsigned long bootmap_size;
	unsigned long start_pfn, max_pfn;
	int i;

#ifdef CONFIG_BLK_DEV_INITRD
	tmp = (((unsigned long)&_end + PAGE_SIZE-1) & PAGE_MASK) - 8;
	if (tmp < (unsigned long)&_end)
		tmp += PAGE_SIZE;
	initrd_header = (unsigned long *)tmp;
	if (initrd_header[0] == 0x494E5244) {
		initrd_start = (unsigned long)&initrd_header[2];
		initrd_end = initrd_start + initrd_header[1];
	}
	start_pfn = PFN_UP(CPHYSADDR((&_end)+(initrd_end - initrd_start) + PAGE_SIZE));
#else
	/*
	 * Partially used pages are not usable - thus
	 * we are rounding upwards.
	 */
	start_pfn = PFN_UP(CPHYSADDR(&_end));
#endif	/* CONFIG_BLK_DEV_INITRD */

#ifndef CONFIG_SGI_IP27
	/* Find the highest page frame number we have available.  */
	max_pfn = 0;
	for (i = 0; i < boot_mem_map.nr_map; i++) {
		unsigned long start, end;

		if (boot_mem_map.map[i].type != BOOT_MEM_RAM)
			continue;

		start = PFN_UP(boot_mem_map.map[i].addr);
		end = PFN_DOWN(boot_mem_map.map[i].addr
		      + boot_mem_map.map[i].size);

		if (start >= end)
			continue;
		if (end > max_pfn)
			max_pfn = end;
	}

	/* Initialize the boot-time allocator.  */
	bootmap_size = init_bootmem(start_pfn, max_pfn);

	/*
	 * Register fully available low RAM pages with the bootmem allocator.
	 */
	for (i = 0; i < boot_mem_map.nr_map; i++) {
		unsigned long curr_pfn, last_pfn, size;

		/*
		 * Reserve usable memory.
		 */
		if (boot_mem_map.map[i].type != BOOT_MEM_RAM)
			continue;

		/*
		 * We are rounding up the start address of usable memory:
		 */
		curr_pfn = PFN_UP(boot_mem_map.map[i].addr);
		if (curr_pfn >= max_pfn)
			continue;
		if (curr_pfn < start_pfn)
			curr_pfn = start_pfn;

		/*
		 * ... and at the end of the usable range downwards:
		 */
		last_pfn = PFN_DOWN(boot_mem_map.map[i].addr
				    + boot_mem_map.map[i].size);

		if (last_pfn > max_pfn)
			last_pfn = max_pfn;

		/*
		 * ... finally, did all the rounding and playing
		 * around just make the area go away?
		 */
		if (last_pfn <= curr_pfn)
			continue;

		size = last_pfn - curr_pfn;
		free_bootmem(PFN_PHYS(curr_pfn), PFN_PHYS(size));
	}

	/* Reserve the bootmap memory.  */
	reserve_bootmem(PFN_PHYS(start_pfn), bootmap_size);
#endif

#ifdef CONFIG_BLK_DEV_INITRD
	/* Board specific code should have set up initrd_start and initrd_end */
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	if (&__rd_start != &__rd_end) {
		initrd_start = (unsigned long)&__rd_start;
		initrd_end = (unsigned long)&__rd_end;
	}
	initrd_below_start_ok = 1;
	if (initrd_start) {
		unsigned long initrd_size = ((unsigned char *)initrd_end) - ((unsigned char *)initrd_start);
		printk("Initial ramdisk at: 0x%p (%lu bytes)\n",
		       (void *)initrd_start,
		       initrd_size);
/* FIXME: is this right? */
#ifndef CONFIG_SGI_IP27
		if (CPHYSADDR(initrd_end) > PFN_PHYS(max_pfn)) {
			printk("initrd extends beyond end of memory "
			       "(0x%p > 0x%p)\ndisabling initrd\n",
			       (void *)CPHYSADDR(initrd_end),
			       (void *)PFN_PHYS(max_pfn));
			initrd_start = 0;
		}
#endif /* !CONFIG_SGI_IP27 */
	}
#endif
}

static inline void resource_init(void)
{
	int i;

	code_resource.start = virt_to_bus(&_ftext);
	code_resource.end = virt_to_bus(&_etext) - 1;
	data_resource.start = virt_to_bus(&_fdata);
	data_resource.end = virt_to_bus(&_edata) - 1;

	/*
	 * Request address space for all standard RAM.
	 */
	for (i = 0; i < boot_mem_map.nr_map; i++) {
		struct resource *res;

		res = alloc_bootmem(sizeof(struct resource));
		switch (boot_mem_map.map[i].type) {
		case BOOT_MEM_RAM:
		case BOOT_MEM_ROM_DATA:
			res->name = "System RAM";
			break;
		case BOOT_MEM_RESERVED:
		default:
			res->name = "reserved";
		}

		res->start = boot_mem_map.map[i].addr;
		res->end = boot_mem_map.map[i].addr +
			   boot_mem_map.map[i].size - 1;

		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		request_resource(&iomem_resource, res);

		/*
		 *  We dont't know which RAM region contains kernel data,
		 *  so we try it repeatedly and let the resource manager
		 *  test it.
		 */
		request_resource(res, &code_resource);
		request_resource(res, &data_resource);
	}
}

#undef PFN_UP
#undef PFN_DOWN
#undef PFN_PHYS


void __init setup_arch(char **cmdline_p)
{
	extern void atlas_setup(void);
	extern void decstation_setup(void);
	extern void ip22_setup(void);
	extern void ip27_setup(void);
	extern void malta_setup(void);
	extern void momenco_ocelot_setup(void);
	extern void momenco_ocelot_g_setup(void);
	extern void momenco_ocelot_c_setup(void);
	extern void momenco_jaguar_atx_setup(void);
	extern void sead_setup(void);
	extern void swarm_setup(void);
	extern void frame_info_init(void);

	frame_info_init();
#ifdef CONFIG_MIPS_ATLAS
	atlas_setup();
#endif
#ifdef CONFIG_DECSTATION
	decstation_setup();
#endif
#ifdef  CONFIG_PMC_YOSEMITE
	pmc_yosemite_setup();
#endif
#ifdef CONFIG_SGI_IP22
	ip22_setup();
#endif
#ifdef CONFIG_SGI_IP27
	ip27_setup();
#endif
#ifdef CONFIG_SIBYTE_BOARD
	swarm_setup();
#endif
#ifdef CONFIG_MIPS_MALTA
	malta_setup();
#endif
#ifdef CONFIG_MIPS_SEAD
	sead_setup();
#endif
#ifdef CONFIG_MOMENCO_OCELOT
	momenco_ocelot_setup();
#endif
#ifdef CONFIG_MOMENCO_OCELOT_G
	momenco_ocelot_g_setup();
#endif
#ifdef CONFIG_MOMENCO_OCELOT_C
	momenco_ocelot_c_setup();
#endif
#ifdef CONFIG_MOMENCO_JAGUAR_ATX
	momenco_jaguar_atx_setup();
#endif

	strncpy(command_line, arcs_cmdline, CL_SIZE);
	memcpy(saved_command_line, command_line, CL_SIZE);
	saved_command_line[CL_SIZE-1] = '\0';

	*cmdline_p = command_line;

	parse_mem_cmdline();

	bootmem_init();

	paging_init();

	resource_init();
}

int __init fpu_disable(char *s)
{
	cpu_data[0].options &= ~MIPS_CPU_FPU;

	return 1;
}

__setup("nofpu", fpu_disable);
