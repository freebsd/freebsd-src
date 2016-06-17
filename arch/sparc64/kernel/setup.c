/*  $Id: setup.c,v 1.71.2.1 2002/02/27 21:31:38 davem Exp $
 *  linux/arch/sparc64/kernel/setup.c
 *
 *  Copyright (C) 1995,1996  David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1997       Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <asm/smp.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <linux/console.h>

#include <asm/segment.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/oplib.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/idprom.h>
#include <asm/head.h>
#include <asm/starfire.h>
#include <asm/hardirq.h>
#include <asm/sections.h>

#ifdef CONFIG_IP_PNP
#include <net/ipconfig.h>
#endif

struct screen_info screen_info = {
	0, 0,			/* orig-x, orig-y */
	0,			/* unused */
	0,			/* orig-video-page */
	0,			/* orig-video-mode */
	128,			/* orig-video-cols */
	0, 0, 0,		/* unused, ega_bx, unused */
	54,			/* orig-video-lines */
	0,                      /* orig-video-isVGA */
	16                      /* orig-video-points */
};

/* Typing sync at the prom prompt calls the function pointed to by
 * the sync callback which I set to the following function.
 * This should sync all filesystems and return, for now it just
 * prints out pretty messages and returns.
 */

#if CONFIG_SUN_CONSOLE
void (*prom_palette)(int);
#endif
void (*prom_keyboard)(void);
asmlinkage void sys_sync(void);	/* it's really int */

static void
prom_console_write(struct console *con, const char *s, unsigned n)
{
	prom_printf("%s", s);
}

static struct console prom_console = {
	name:		"prom",
	write:		prom_console_write,
	flags:		CON_CONSDEV | CON_ENABLED,
	index:		-1,
};

#define PROM_TRUE	-1
#define PROM_FALSE	0

/* Pretty sick eh? */
int prom_callback(long *args)
{
	struct console *cons, *saved_console = NULL;
	unsigned long flags;
	char *cmd;
	extern spinlock_t prom_entry_lock;

	if (!args)
		return -1;
	if (!(cmd = (char *)args[0]))
		return -1;

	/*
	 * The callback can be invoked on the cpu that first dropped 
	 * into prom_cmdline after taking the serial interrupt, or on 
	 * a slave processor that was smp_captured() if the 
	 * administrator has done a switch-cpu inside obp. In either 
	 * case, the cpu is marked as in-interrupt. Drop IRQ locks.
	 */
	irq_exit(smp_processor_id(), 0);
	save_and_cli(flags);
	spin_unlock(&prom_entry_lock);
	cons = console_drivers;
	while (cons) {
		unregister_console(cons);
		cons->flags &= ~(CON_PRINTBUFFER);
		cons->next = saved_console;
		saved_console = cons;
		cons = console_drivers;
	}
	register_console(&prom_console);
	if (!strcmp(cmd, "sync")) {
		prom_printf("PROM `%s' command...\n", cmd);
		show_free_areas();
		if(current->pid != 0) {
			sti();
			sys_sync();
			cli();
		}
		args[2] = 0;
		args[args[1] + 3] = -1;
		prom_printf("Returning to PROM\n");
	} else if (!strcmp(cmd, "va>tte-data")) {
		unsigned long ctx, va;
		unsigned long tte = 0;
		long res = PROM_FALSE;

		ctx = args[3];
		va = args[4];
		if (ctx) {
			/*
			 * Find process owning ctx, lookup mapping.
			 */
			struct task_struct *p;
			struct mm_struct *mm = NULL;
			pgd_t *pgdp;
			pmd_t *pmdp;
			pte_t *ptep;

			for_each_task(p) {
				mm = p->mm;
				if (CTX_HWBITS(mm->context) == ctx)
					break;
			}
			if (!mm ||
			    CTX_HWBITS(mm->context) != ctx)
				goto done;

			pgdp = pgd_offset(mm, va);
			if (pgd_none(*pgdp))
				goto done;
			pmdp = pmd_offset(pgdp, va);
			if (pmd_none(*pmdp))
				goto done;
			ptep = pte_offset(pmdp, va);
			if (!pte_present(*ptep))
				goto done;
			tte = pte_val(*ptep);
			res = PROM_TRUE;
			goto done;
		}

		if ((va >= KERNBASE) && (va < (KERNBASE + (4 * 1024 * 1024)))) {
			/* Spitfire Errata #32 workaround */
			__asm__ __volatile__("stxa	%0, [%1] %2\n\t"
					     "flush	%%g6"
					     : /* No outputs */
					     : "r" (0),
					     "r" (PRIMARY_CONTEXT), "i" (ASI_DMMU));

			/*
			 * Locked down tlb entry.
			 */

			if (tlb_type == spitfire)
				tte = spitfire_get_dtlb_data(SPITFIRE_HIGHEST_LOCKED_TLBENT);
			else if (tlb_type == cheetah || tlb_type == cheetah_plus)
				tte = cheetah_get_ldtlb_data(CHEETAH_HIGHEST_LOCKED_TLBENT);

			res = PROM_TRUE;
			goto done;
		}

		if (va < PGDIR_SIZE) {
			/*
			 * vmalloc or prom_inherited mapping.
			 */
			pgd_t *pgdp;
			pmd_t *pmdp;
			pte_t *ptep;
			int error;

			if ((va >= LOW_OBP_ADDRESS) && (va < HI_OBP_ADDRESS)) {
				tte = prom_virt_to_phys(va, &error);
				if (!error)
					res = PROM_TRUE;
				goto done;
			}
			pgdp = pgd_offset_k(va);
			if (pgd_none(*pgdp))
				goto done;
			pmdp = pmd_offset(pgdp, va);
			if (pmd_none(*pmdp))
				goto done;
			ptep = pte_offset(pmdp, va);
			if (!pte_present(*ptep))
				goto done;
			tte = pte_val(*ptep);
			res = PROM_TRUE;
			goto done;
		}

		if (va < PAGE_OFFSET) {
			/*
			 * No mappings here.
			 */
			goto done;
		}

		if (va & (1UL << 40)) {
			/*
			 * I/O page.
			 */

			tte = (__pa(va) & _PAGE_PADDR) |
			      _PAGE_VALID | _PAGE_SZ4MB |
			      _PAGE_E | _PAGE_P | _PAGE_W;
			res = PROM_TRUE;
			goto done;
		}

		/*
		 * Normal page.
		 */
		tte = (__pa(va) & _PAGE_PADDR) |
		      _PAGE_VALID | _PAGE_SZ4MB |
		      _PAGE_CP | _PAGE_CV | _PAGE_P | _PAGE_W;
		res = PROM_TRUE;

	done:
		if (res == PROM_TRUE) {
			args[2] = 3;
			args[args[1] + 3] = 0;
			args[args[1] + 4] = res;
			args[args[1] + 5] = tte;
		} else {
			args[2] = 2;
			args[args[1] + 3] = 0;
			args[args[1] + 4] = res;
		}
	} else if (!strcmp(cmd, ".soft1")) {
		unsigned long tte;

		tte = args[3];
		prom_printf("%lx:\"%s%s%s%s%s\" ",
			    (tte & _PAGE_SOFT) >> 7,
			    tte & _PAGE_MODIFIED ? "M" : "-",
			    tte & _PAGE_ACCESSED ? "A" : "-",
			    tte & _PAGE_READ     ? "W" : "-",
			    tte & _PAGE_WRITE    ? "R" : "-",
			    tte & _PAGE_PRESENT  ? "P" : "-");

		args[2] = 2;
		args[args[1] + 3] = 0;
		args[args[1] + 4] = PROM_TRUE;
	} else if (!strcmp(cmd, ".soft2")) {
		unsigned long tte;

		tte = args[3];
		prom_printf("%lx ", (tte & 0x07FC000000000000) >> 50);

		args[2] = 2;
		args[args[1] + 3] = 0;
		args[args[1] + 4] = PROM_TRUE;
	} else {
		prom_printf("unknown PROM `%s' command...\n", cmd);
	}
	unregister_console(&prom_console);
	while (saved_console) {
		cons = saved_console;
		saved_console = cons->next;
		register_console(cons);
	}
	spin_lock(&prom_entry_lock);
	restore_flags(flags);
	/*
	 * Restore in-interrupt status for a resume from obp.
	 */
	irq_enter(smp_processor_id(), 0);
	return 0;
}

extern void rs_kgdb_hook(int tty_num); /* sparc/serial.c */

unsigned int boot_flags = 0;
#define BOOTME_DEBUG  0x1
#define BOOTME_SINGLE 0x2
#define BOOTME_KGDB   0x4

#ifdef CONFIG_SUN_CONSOLE
static int console_fb __initdata = 0;
#endif

/* Exported for mm/init.c:paging_init. */
unsigned long cmdline_memory_size = 0;

static struct console prom_debug_console = {
	name:		"debug",
	write:		prom_console_write,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

/* XXX Implement this at some point... */
void kernel_enter_debugger(void)
{
}

int obp_system_intr(void)
{
	if (boot_flags & BOOTME_DEBUG) {
		printk("OBP: system interrupted\n");
		prom_halt();
		return 1;
	}
	return 0;
}

/* 
 * Process kernel command line switches that are specific to the
 * SPARC or that require special low-level processing.
 */
static void __init process_switch(char c)
{
	switch (c) {
	case 'd':
		boot_flags |= BOOTME_DEBUG;
		break;
	case 's':
		boot_flags |= BOOTME_SINGLE;
		break;
	case 'h':
		prom_printf("boot_flags_init: Halt!\n");
		prom_halt();
		break;
	case 'p':
		/* Use PROM debug console. */
		register_console(&prom_debug_console);
		break;
	default:
		printk("Unknown boot switch (-%c)\n", c);
		break;
	}
}

static void __init boot_flags_init(char *commands)
{
	while (*commands) {
		/* Move to the start of the next "argument". */
		while (*commands && *commands == ' ')
			commands++;

		/* Process any command switches, otherwise skip it. */
		if (*commands == '\0')
			break;
		else if (*commands == '-') {
			commands++;
			while (*commands && *commands != ' ')
				process_switch(*commands++);
		} else if (strlen(commands) >= 9
			   && !strncmp(commands, "kgdb=tty", 8)) {
			boot_flags |= BOOTME_KGDB;
			switch (commands[8]) {
#ifdef CONFIG_SUN_SERIAL
			case 'a':
				rs_kgdb_hook(0);
				prom_printf("KGDB: Using serial line /dev/ttya.\n");
				break;
			case 'b':
				rs_kgdb_hook(1);
				prom_printf("KGDB: Using serial line /dev/ttyb.\n");
				break;
#endif
			default:
				printk("KGDB: Unknown tty line.\n");
				boot_flags &= ~BOOTME_KGDB;
				break;
			}
			commands += 9;
		} else {
#if CONFIG_SUN_CONSOLE
			if (!strncmp(commands, "console=", 8)) {
				commands += 8;
				if (!strncmp (commands, "ttya", 4)) {
					console_fb = 2;
					prom_printf ("Using /dev/ttya as console.\n");
				} else if (!strncmp (commands, "ttyb", 4)) {
					console_fb = 3;
					prom_printf ("Using /dev/ttyb as console.\n");
#if defined(CONFIG_PROM_CONSOLE)
				} else if (!strncmp (commands, "prom", 4)) {
					char *p;
					
					for (p = commands - 8; *p && *p != ' '; p++)
						*p = ' ';
					conswitchp = &prom_con;
					console_fb = 1;
#endif
				} else {
					console_fb = 1;
				}
			} else
#endif
			if (!strncmp(commands, "mem=", 4)) {
				/*
				 * "mem=XXX[kKmM]" overrides the PROM-reported
				 * memory size.
				 */
				cmdline_memory_size = simple_strtoul(commands + 4,
								     &commands, 0);
				if (*commands == 'K' || *commands == 'k') {
					cmdline_memory_size <<= 10;
					commands++;
				} else if (*commands=='M' || *commands=='m') {
					cmdline_memory_size <<= 20;
					commands++;
				}
			}
			while (*commands && *commands != ' ')
				commands++;
		}
	}
}

extern int prom_probe_memory(void);
extern unsigned long start, end;
extern void panic_setup(char *, int *);

extern unsigned short root_flags;
extern unsigned short root_dev;
extern unsigned short ram_flags;
#define RAMDISK_IMAGE_START_MASK	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000

extern int root_mountflags;

char saved_command_line[256];
char reboot_command[256];

extern unsigned long phys_base, kern_base, kern_size;

static struct pt_regs fake_swapper_regs = { { 0, }, 0, 0, 0, 0 };

void register_prom_callbacks(void)
{
	prom_setcallback(prom_callback);
	prom_feval(": linux-va>tte-data 2 \" va>tte-data\" $callback drop ; "
		   "' linux-va>tte-data to va>tte-data");
	prom_feval(": linux-.soft1 1 \" .soft1\" $callback 2drop ; "
		   "' linux-.soft1 to .soft1");
	prom_feval(": linux-.soft2 1 \" .soft2\" $callback 2drop ; "
		   "' linux-.soft2 to .soft2");
}

extern void paging_init(void);

void __init setup_arch(char **cmdline_p)
{
	extern int serial_console;  /* in console.c, of course */
	unsigned long highest_paddr;
	int i;

	/* Initialize PROM console and command line. */
	*cmdline_p = prom_getbootargs();
	strcpy(saved_command_line, *cmdline_p);

	printk("ARCH: SUN4U\n");

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#elif defined(CONFIG_PROM_CONSOLE)
	conswitchp = &prom_con;
#endif

#ifdef CONFIG_SMP
	i = (unsigned long)&irq_stat[1] - (unsigned long)&irq_stat[0];
	if ((i == SMP_CACHE_BYTES) || (i == (2 * SMP_CACHE_BYTES))) {
		extern unsigned int irqsz_patchme[1];
		irqsz_patchme[0] |= ((i == SMP_CACHE_BYTES) ? SMP_CACHE_BYTES_SHIFT : \
							SMP_CACHE_BYTES_SHIFT + 1);
		flushi((long)&irqsz_patchme[0]);
	} else {
		prom_printf("Unexpected size of irq_stat[] elements\n");
		prom_halt();
	}
#endif
	/* Work out if we are starfire early on */
	check_if_starfire();

	boot_flags_init(*cmdline_p);

	idprom_init();
	(void) prom_probe_memory();

	/* In paging_init() we tip off this value to see if we need
	 * to change init_mm.pgd to point to the real alias mapping.
	 */
	phys_base = 0xffffffffffffffffUL;
	highest_paddr = 0UL;
	for (i = 0; sp_banks[i].num_bytes != 0; i++) {
		unsigned long top;

		if (sp_banks[i].base_addr < phys_base)
			phys_base = sp_banks[i].base_addr;
		top = sp_banks[i].base_addr +
			sp_banks[i].num_bytes;
		if (highest_paddr < top)
			highest_paddr = top;
	}

	switch (tlb_type) {
	default:
	case spitfire:
		kern_base = spitfire_get_itlb_data(sparc64_highest_locked_tlbent());
		kern_base &= _PAGE_PADDR_SF;
		break;

	case cheetah:
	case cheetah_plus:
		kern_base = cheetah_get_litlb_data(sparc64_highest_locked_tlbent());
		kern_base &= _PAGE_PADDR;
		break;
	};

	kern_size = (unsigned long)&_end - (unsigned long)KERNBASE;

	if (!root_flags)
		root_mountflags &= ~MS_RDONLY;
	ROOT_DEV = to_kdev_t(root_dev);
#ifdef CONFIG_BLK_DEV_INITRD
	rd_image_start = ram_flags & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((ram_flags & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((ram_flags & RAMDISK_LOAD_FLAG) != 0);	
#endif

	init_task.thread.kregs = &fake_swapper_regs;

#ifdef CONFIG_IP_PNP
	if (!ic_set_manually) {
		int chosen = prom_finddevice ("/chosen");
		u32 cl, sv, gw;
		
		cl = prom_getintdefault (chosen, "client-ip", 0);
		sv = prom_getintdefault (chosen, "server-ip", 0);
		gw = prom_getintdefault (chosen, "gateway-ip", 0);
		if (cl && sv) {
			ic_myaddr = cl;
			ic_servaddr = sv;
			if (gw)
				ic_gateway = gw;
#if defined(CONFIG_IP_PNP_BOOTP) || defined(CONFIG_IP_PNP_RARP)
			ic_proto_enabled = 0;
#endif
		}
	}
#endif

#ifdef CONFIG_SUN_SERIAL
	switch (console_fb) {
	case 0: /* Let's get our io devices from prom */
		{
			int idev = prom_query_input_device();
			int odev = prom_query_output_device();
			if (idev == PROMDEV_IKBD && odev == PROMDEV_OSCREEN) {
				serial_console = 0;
			} else if (idev == PROMDEV_ITTYA && odev == PROMDEV_OTTYA) {
				serial_console = 1;
			} else if (idev == PROMDEV_ITTYB && odev == PROMDEV_OTTYB) {
				serial_console = 2;
			} else {
				prom_printf("Inconsistent console: "
					    "input %d, output %d\n",
					    idev, odev);
				prom_halt();
			}
		}
		break;
	case 1: /* Force one of the framebuffers as console */
		serial_console = 0;
		break;
	case 2: /* Force ttya as console */
		serial_console = 1;
		break;
	case 3: /* Force ttyb as console */
		serial_console = 2;
		break;
	}
#else
	serial_console = 0;
#endif
	if (serial_console)
		conswitchp = NULL;

	paging_init();
}

asmlinkage int sys_ioperm(unsigned long from, unsigned long num, int on)
{
	return -EIO;
}

/* BUFFER is PAGE_SIZE bytes long. */

extern char *sparc_cpu_type;
extern char *sparc_fpu_type;

extern void smp_info(struct seq_file *);
extern void smp_bogo(struct seq_file *);
extern void mmu_info(struct seq_file *);

#ifndef CONFIG_SMP
unsigned long up_clock_tick;
#endif

static int show_cpuinfo(struct seq_file *m, void *__unused)
{
	seq_printf(m, 
		   "cpu\t\t: %s\n"
		   "fpu\t\t: %s\n"
		   "promlib\t\t: Version 3 Revision %d\n"
		   "prom\t\t: %d.%d.%d\n"
		   "type\t\t: sun4u\n"
		   "ncpus probed\t: %d\n"
		   "ncpus active\t: %d\n"
#ifndef CONFIG_SMP
		   "Cpu0Bogo\t: %lu.%02lu\n"
		   "Cpu0ClkTck\t: %016lx\n"
#endif
		   ,
		   sparc_cpu_type,
		   sparc_fpu_type,
		   prom_rev,
		   prom_prev >> 16,
		   (prom_prev >> 8) & 0xff,
		   prom_prev & 0xff,
		   linux_num_cpus,
		   smp_num_cpus
#ifndef CONFIG_SMP
		   , loops_per_jiffy/(500000/HZ),
		   (loops_per_jiffy/(5000/HZ)) % 100,
		   up_clock_tick
#endif
		);
#ifdef CONFIG_SMP
	smp_bogo(m);
#endif
	mmu_info(m);
#ifdef CONFIG_SMP
	smp_info(m);
#endif
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	/* The pointer we are returning is arbitrary,
	 * it just has to be non-NULL and not IS_ERR
	 * in the success case.
	 */
	return *pos == 0 ? &c_start : NULL;
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
