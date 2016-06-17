/* $Id: setup.c,v 1.1.1.1.2.7 2003/06/20 13:13:25 trent Exp $
 *
 *  linux/arch/sh/kernel/setup.c
 *
 *  Copyright (C) 1999  Niibe Yutaka
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
#ifdef CONFIG_BLK_DEV_RAM
#include <linux/blk.h>
#endif
#include <linux/bootmem.h>
#include <linux/console.h>
#include <linux/ctype.h>
#include <linux/seq_file.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/io_generic.h>
#include <asm/machvec.h>
#ifdef CONFIG_SH_EARLY_PRINTK
#include <asm/sh_bios.h>
#endif

#ifdef CONFIG_SH_KGDB
#include <asm/kgdb.h>
static int kgdb_parse_options(char *options);
#endif

/*
 * Machine setup..
 */

/*
 * Initialize loops_per_jiffy as 10000000 (1000MIPS).
 * This value will be used at the very early stage of serial setup.
 * The bigger value means no problem.
 */
struct sh_cpuinfo boot_cpu_data = { CPU_SH_NONE, 0, 10000000, };
struct screen_info screen_info;
unsigned char aux_device_present = 0xaa;

#if defined(CONFIG_SH_GENERIC) || defined(CONFIG_SH_UNKNOWN)
struct sh_machine_vector sh_mv;
#endif

/* We need this to satisfy some external references. */
struct screen_info screen_info = {
        0, 25,                  /* orig-x, orig-y */
        0,                      /* unused */
        0,                      /* orig-video-page */
        0,                      /* orig-video-mode */
        80,                     /* orig-video-cols */
        0,0,0,                  /* ega_ax, ega_bx, ega_cx */
        25,                     /* orig-video-lines */
        0,                      /* orig-video-isVGA */
        16                      /* orig-video-points */
};

extern void fpu_init(void);
extern int root_mountflags;
extern int _text, _etext, _edata, _end;

#define MV_NAME_SIZE 32

static struct sh_machine_vector* __init get_mv_byname(const char* name);

/*
 * This is set up by the setup-routine at boot-time
 */
#define PARAM	((unsigned char *)empty_zero_page)

#define MOUNT_ROOT_RDONLY (*(unsigned long *) (PARAM+0x000))
#define RAMDISK_FLAGS (*(unsigned long *) (PARAM+0x004))
#define ORIG_ROOT_DEV (*(unsigned long *) (PARAM+0x008))
#define LOADER_TYPE (*(unsigned long *) (PARAM+0x00c))
#define INITRD_START (*(unsigned long *) (PARAM+0x010))
#define INITRD_SIZE (*(unsigned long *) (PARAM+0x014))
/* ... */
#define COMMAND_LINE ((char *) (PARAM+0x100))
#define COMMAND_LINE_SIZE 256

#define RAMDISK_IMAGE_START_MASK  	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000	

static char command_line[COMMAND_LINE_SIZE] = { 0, };
       char saved_command_line[COMMAND_LINE_SIZE];

struct resource standard_io_resources[] = {
	{ "dma1", 0x00, 0x1f },
	{ "pic1", 0x20, 0x3f },
	{ "timer", 0x40, 0x5f },
	{ "keyboard", 0x60, 0x6f },
	{ "dma page reg", 0x80, 0x8f },
	{ "pic2", 0xa0, 0xbf },
	{ "dma2", 0xc0, 0xdf },
	{ "fpu", 0xf0, 0xff }
};

#define STANDARD_IO_RESOURCES (sizeof(standard_io_resources)/sizeof(struct resource))

/* System RAM - interrupted by the 640kB-1M hole */
#define code_resource (ram_resources[3])
#define data_resource (ram_resources[4])
static struct resource ram_resources[] = {
	{ "System RAM", 0x000000, 0x09ffff, IORESOURCE_BUSY },
	{ "System RAM", 0x100000, 0x100000, IORESOURCE_BUSY },
	{ "Video RAM area", 0x0a0000, 0x0bffff },
	{ "Kernel code", 0x100000, 0 },
	{ "Kernel data", 0, 0 }
};

unsigned long memory_start, memory_end;

#ifdef CONFIG_SH_EARLY_PRINTK
/*
 *	Print a string through the BIOS
 */
static void sh_console_write(struct console *co, const char *s,
				 unsigned count)
{
    	sh_bios_console_write(s, count);
}

static kdev_t sh_console_device(struct console *c)
{
    	/* TODO: this is totally bogus */
	/* return MKDEV(SCI_MAJOR, SCI_MINOR_START + c->index); */
	return 0;
}

/*
 *	Setup initial baud/bits/parity. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init sh_console_setup(struct console *co, char *options)
{
	int	cflag = CREAD | HUPCL | CLOCAL;

	/*
	 *	Now construct a cflag setting.
	 *  	TODO: this is a totally bogus cflag, as we have
	 *  	no idea what serial settings the BIOS is using, or
	 *  	even if its using the serial port at all.
	 */
    	cflag |= B115200 | CS8 | /*no parity*/0;

	co->cflag = cflag;

	return 0;
}

static struct console sh_console = {
	name:		"bios",
	write:		sh_console_write,
	device:		sh_console_device,
	setup:		sh_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void sh_console_init(void)
{
	register_console(&sh_console);
}

void sh_console_unregister(void)
{
	unregister_console(&sh_console);
}

#endif

static inline void parse_cmdline (char ** cmdline_p, char mv_name[MV_NAME_SIZE],
				  struct sh_machine_vector** mvp,
				  unsigned long *mv_io_base,
				  int *mv_mmio_enable)
{
	char c = ' ', *to = command_line, *from = COMMAND_LINE;
	int len = 0;

	/* Save unparsed command line copy for /proc/cmdline */
	memcpy(saved_command_line, COMMAND_LINE, COMMAND_LINE_SIZE);
	saved_command_line[COMMAND_LINE_SIZE-1] = '\0';

	memory_start = (unsigned long)PAGE_OFFSET+__MEMORY_START;
	memory_end = memory_start + __MEMORY_SIZE;

	for (;;) {
		/*
		 * "mem=XXX[kKmM]" defines a size of memory.
		 */
		if (c == ' ' && !memcmp(from, "mem=", 4)) {
			if (to != command_line)
				to--;
			{
				unsigned long mem_size;

				mem_size = memparse(from+4, &from);
				memory_end = memory_start + mem_size;
			}
		}
		if (c == ' ' && !memcmp(from, "sh_mv=", 6)) {
			char* mv_end;
			char* mv_comma;
			int mv_len;
			if (to != command_line)
				to--;
			from += 6;
			mv_end = strchr(from, ' ');
			if (mv_end == NULL)
				mv_end = from + strlen(from);

			mv_comma = strchr(from, ',');
			if ((mv_comma != NULL) && (mv_comma < mv_end)) {
				int ints[3];
				get_options(mv_comma+1, ARRAY_SIZE(ints), ints);
				*mv_io_base = ints[1];
				*mv_mmio_enable = ints[2];
				mv_len = mv_comma - from;
			} else {
				mv_len = mv_end - from;
			}
			if (mv_len > (MV_NAME_SIZE-1))
				mv_len = MV_NAME_SIZE-1;
			memcpy(mv_name, from, mv_len);
			mv_name[mv_len] = '\0';
			from = mv_end;

			*mvp = get_mv_byname(mv_name);
		}
		c = *(from++);
		if (!c)
			break;
		if (COMMAND_LINE_SIZE <= ++len)
			break;
		*(to++) = c;
	}
	*to = '\0';
	*cmdline_p = command_line;
}

void __init setup_arch(char **cmdline_p)
{
#if defined(CONFIG_SH_GENERIC) || defined(CONFIG_SH_UNKNOWN)
	extern struct sh_machine_vector mv_unknown;
#endif
	struct sh_machine_vector *mv = NULL;
	char mv_name[MV_NAME_SIZE] = "";
	unsigned long mv_io_base = 0;
	int mv_mmio_enable = 0;
	unsigned long bootmap_size;
	unsigned long start_pfn, max_pfn, max_low_pfn;

#ifdef CONFIG_SH_EARLY_PRINTK
	sh_console_init();
#endif
	
	ROOT_DEV = to_kdev_t(ORIG_ROOT_DEV);

#ifdef CONFIG_BLK_DEV_RAM
	rd_image_start = RAMDISK_FLAGS & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((RAMDISK_FLAGS & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((RAMDISK_FLAGS & RAMDISK_LOAD_FLAG) != 0);
#endif

	if (!MOUNT_ROOT_RDONLY)
		root_mountflags &= ~MS_RDONLY;
	init_mm.start_code = (unsigned long)&_text;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
	init_mm.brk = (unsigned long) &_end;

	code_resource.start = virt_to_bus(&_text);
	code_resource.end = virt_to_bus(&_etext)-1;
	data_resource.start = virt_to_bus(&_etext);
	data_resource.end = virt_to_bus(&_edata)-1;

	parse_cmdline(cmdline_p, mv_name, &mv, &mv_io_base, &mv_mmio_enable);

#ifdef CONFIG_CMDLINE_BOOL
	sprintf(*cmdline_p, CONFIG_CMDLINE);
#endif

#ifdef CONFIG_SH_GENERIC
	if (mv == NULL) {
		mv = &mv_unknown;
		if (*mv_name != '\0') {
			printk("Warning: Unsupported machine %s, using unknown\n",
			       mv_name);
		}
	}
	sh_mv = *mv;
#endif
#ifdef CONFIG_SH_UNKNOWN
	sh_mv = mv_unknown;
#endif

#if defined(CONFIG_SH_GENERIC) || defined(CONFIG_SH_UNKNOWN)
	if (mv_io_base != 0) {
		sh_mv.mv_inb = generic_inb;
		sh_mv.mv_inw = generic_inw;
		sh_mv.mv_inl = generic_inl;
		sh_mv.mv_outb = generic_outb;
		sh_mv.mv_outw = generic_outw;
		sh_mv.mv_outl = generic_outl;

		sh_mv.mv_inb_p = generic_inb_p;
		sh_mv.mv_inw_p = generic_inw_p;
		sh_mv.mv_inl_p = generic_inl_p;
		sh_mv.mv_outb_p = generic_outb_p;
		sh_mv.mv_outw_p = generic_outw_p;
		sh_mv.mv_outl_p = generic_outl_p;

		sh_mv.mv_insb = generic_insb;
		sh_mv.mv_insw = generic_insw;
		sh_mv.mv_insl = generic_insl;
		sh_mv.mv_outsb = generic_outsb;
		sh_mv.mv_outsw = generic_outsw;
		sh_mv.mv_outsl = generic_outsl;

		sh_mv.mv_isa_port2addr = generic_isa_port2addr;
		generic_io_base = mv_io_base;
	}
	if (mv_mmio_enable != 0) {
		sh_mv.mv_readb = generic_readb;
		sh_mv.mv_readw = generic_readw;
		sh_mv.mv_readl = generic_readl;
		sh_mv.mv_writeb = generic_writeb;
		sh_mv.mv_writew = generic_writew;
		sh_mv.mv_writel = generic_writel;
	}
#endif

#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((x) << PAGE_SHIFT)

#ifdef CONFIG_DISCONTIGMEM
	NODE_DATA(0)->bdata = &discontig_node_bdata[0];
	NODE_DATA(1)->bdata = &discontig_node_bdata[1];

	bootmap_size = init_bootmem_node(NODE_DATA(1), 
					 PFN_UP(__MEMORY_START_2ND),
					 PFN_UP(__MEMORY_START_2ND),
					 PFN_DOWN(__MEMORY_START_2ND+__MEMORY_SIZE_2ND));
	free_bootmem_node(NODE_DATA(1), __MEMORY_START_2ND, __MEMORY_SIZE_2ND);
	reserve_bootmem_node(NODE_DATA(1), __MEMORY_START_2ND, bootmap_size);
#endif

	/*
	 * Find the highest page frame number we have available
	 */
	max_pfn = PFN_DOWN(__pa(memory_end));

	/*
	 * Determine low and high memory ranges:
	 */
	max_low_pfn = max_pfn;

 	/*
	 * Partially used pages are not usable - thus
	 * we are rounding upwards:
 	 */
	start_pfn = PFN_UP(__pa(&_end));
	/*
	 * Find a proper area for the bootmem bitmap. After this
	 * bootstrap step all allocations (until the page allocator
	 * is intact) must be done via bootmem_alloc().
	 */
	bootmap_size = init_bootmem_node(NODE_DATA(0), start_pfn,
					 __MEMORY_START>>PAGE_SHIFT,
					 max_low_pfn);

	/*
	 * Register fully available low RAM pages with the bootmem allocator.
	 */
	{
		unsigned long curr_pfn, last_pfn, pages;

		/*
		 * We are rounding up the start address of usable memory:
		 */
		curr_pfn = PFN_UP(__MEMORY_START);
		/*
		 * ... and at the end of the usable range downwards:
		 */
		last_pfn = PFN_DOWN(__pa(memory_end));

		if (last_pfn > max_low_pfn)
			last_pfn = max_low_pfn;

		pages = last_pfn - curr_pfn;
		free_bootmem_node(NODE_DATA(0), PFN_PHYS(curr_pfn),
				  PFN_PHYS(pages));
	}

	/*
	 * Reserve the kernel text and
	 * Reserve the bootmem bitmap. We do this in two steps (first step
	 * was init_bootmem()), because this catches the (definitely buggy)
	 * case of us accidentally initializing the bootmem allocator with
	 * an invalid RAM area.
	 */
	reserve_bootmem_node(NODE_DATA(0), __MEMORY_START+PAGE_SIZE,
		(PFN_PHYS(start_pfn)+bootmap_size+PAGE_SIZE-1)-__MEMORY_START);

	/*
	 * reserve physical page 0 - it's a special BIOS page on many boxes,
	 * enabling clean reboots, SMP operation, laptop functions.
	 */
	reserve_bootmem_node(NODE_DATA(0), __MEMORY_START, PAGE_SIZE);

#ifdef CONFIG_BLK_DEV_INITRD
	if (LOADER_TYPE && INITRD_START) {
		if (INITRD_START + INITRD_SIZE <= (max_low_pfn << PAGE_SHIFT)) {
			reserve_bootmem_node(NODE_DATA(0), INITRD_START+__MEMORY_START, INITRD_SIZE);
			initrd_start =
				INITRD_START ? INITRD_START + PAGE_OFFSET + __MEMORY_START : 0;
			initrd_end = initrd_start + INITRD_SIZE;
		} else {
			printk("initrd extends beyond end of memory "
			    "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
				    INITRD_START + INITRD_SIZE,
				    max_low_pfn << PAGE_SHIFT);
			initrd_start = 0;
		}
	}
#endif

#if 0
	/*
	 * Request the standard RAM and ROM resources -
	 * they eat up PCI memory space
	 */
	request_resource(&iomem_resource, ram_resources+0);
	request_resource(&iomem_resource, ram_resources+1);
	request_resource(&iomem_resource, ram_resources+2);
	request_resource(ram_resources+1, &code_resource);
	request_resource(ram_resources+1, &data_resource);
	probe_roms();

	/* request I/O space for devices used on all i[345]86 PCs */
	for (i = 0; i < STANDARD_IO_RESOURCES; i++)
		request_resource(&ioport_resource, standard_io_resources+i);
#endif

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif

	/* Perform the machine specific initialisation */
	if (sh_mv.mv_init_arch != NULL) {
		sh_mv.mv_init_arch();
	}

#if defined(__SH4__)
	init_task.used_math = 0;
	init_task.flags &= ~PF_USEDFPU;
#endif

#ifdef CONFIG_UBC_WAKEUP
	/*
	 * Some brain-damaged loaders decided it would be a good idea to put
	 * the UBC to sleep. This causes some issues when it comes to things
	 * like PTRACE_SINGLESTEP or doing hardware watchpoints in GDB.  So ..
	 * we wake it up and hope that all is well.
	 */
	ubc_wakeup();
#endif

	paging_init();
}

struct sh_machine_vector* __init get_mv_byname(const char* name)
{
	extern int strcasecmp(const char *, const char *);
	extern long __machvec_start, __machvec_end;
	struct sh_machine_vector *all_vecs =
		(struct sh_machine_vector *)&__machvec_start;

	int i, n = ((unsigned long)&__machvec_end
		    - (unsigned long)&__machvec_start)/
		sizeof(struct sh_machine_vector);

	for (i = 0; i < n; ++i) {
		struct sh_machine_vector *mv = &all_vecs[i];
		if (mv == NULL)
			continue;
		if (strcasecmp(name, mv->mv_name) == 0) {
			return mv;
		}
	}
	return NULL;
}

/*
 *	Get CPU information for use by the procfs.
 */
#ifdef CONFIG_PROC_FS
static int show_cpuinfo(struct seq_file *m, void *v)
{
#if defined(__sh3__)
	seq_printf(m, "cpu family\t: SH-3\n"
		      "cache size\t: 8K-byte\n");
#elif defined(__SH4__)
	seq_printf(m, "cpu family\t: SH-4\n"
		      "cache size\t: 8K-byte/16K-byte\n");
#endif
	seq_printf(m, "bogomips\t: %lu.%02lu\n\n",
		     loops_per_jiffy/(500000/HZ),
		     (loops_per_jiffy/(5000/HZ)) % 100);
	seq_printf(m, "Machine: %s\n", sh_mv.mv_name);

#define PRINT_CLOCK(name, value) \
	seq_printf(m, name " clock: %d.%02dMHz\n", \
		     ((value) / 1000000), ((value) % 1000000)/10000)
	
	PRINT_CLOCK("CPU", boot_cpu_data.cpu_clock);
	PRINT_CLOCK("Bus", boot_cpu_data.bus_clock);
#ifdef CONFIG_CPU_SUBTYPE_ST40
	PRINT_CLOCK("Memory", boot_cpu_data.memory_clock);
#endif
	PRINT_CLOCK("Peripheral module", boot_cpu_data.module_clock);

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return (void*)(*pos == 0);
}
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	return NULL;
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
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_SH_KGDB
/*
 * Parse command-line kgdb options.  By default KGDB is enabled,
 * entered on error (or other action) using default serial info.
 * The command-line option can include a serial port specification
 * and an action to override default or configured behavior.
 */
struct kgdb_sermap kgdb_sci_sermap =
{ "ttySC", 5, kgdb_sci_setup, NULL };

struct kgdb_sermap *kgdb_serlist = &kgdb_sci_sermap;
struct kgdb_sermap *kgdb_porttype = &kgdb_sci_sermap;

void kgdb_register_sermap(struct kgdb_sermap *map)
{
	struct kgdb_sermap *last;

	for (last = kgdb_serlist; last->next; last = last->next)
		;
	last->next = map;
	if (!map->namelen) {
		map->namelen = strlen(map->name);
	}
}

static int __init kgdb_parse_options(char *options)
{
	char c;
	int baud;

	/* Check for port spec (or use default) */

	/* Determine port type and instance */
	if (!memcmp(options, "tty", 3)) {
		struct kgdb_sermap *map = kgdb_serlist;

		while (map && memcmp(options, map->name, map->namelen))
			map = map->next;

		if (!map) {
			KGDB_PRINTK("unknown port spec in %s\n", options);
			return -1;
		}

		kgdb_porttype = map;
		kgdb_serial_setup = map->setup_fn;
		kgdb_portnum = options[map->namelen] - '0';
		options += map->namelen + 1;

		options = (*options == ',') ? options+1 : options;
		
		/* Read optional parameters (baud/parity/bits) */
		baud = simple_strtoul(options, &options, 10);
		if (baud != 0) {
			kgdb_baud = baud;

			c = toupper(*options);
			if (c == 'E' || c == 'O' || c == 'N') {
				kgdb_parity = c;
				options++;
			}

			c = *options;
			if (c == '7' || c == '8') {
				kgdb_bits = c;
				options++;
			}
			options = (*options == ',') ? options+1 : options;
		}
	}

	/* Check for action specification */
	if (!memcmp(options, "halt", 4)) {
		kgdb_halt = 1;
		options += 4;
	} else if (!memcmp(options, "disabled", 8)) {
		kgdb_enabled = 0;
		options += 8;
	}

	if (*options) {
                KGDB_PRINTK("ignored unknown options: %s\n", options);
		return 0;
	}
	return 1;
}
__setup("kgdb=", kgdb_parse_options);
#endif /* CONFIG_SH_KGDB */

