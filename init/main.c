/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org> 
 */

#define __KERNEL_SYSCALLS__

#include <linux/config.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/utsname.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/blk.h>
#include <linux/hdreg.h>
#include <linux/iobuf.h>
#include <linux/bootmem.h>
#include <linux/file.h>
#include <linux/tty.h>

#include <asm/io.h>
#include <asm/bugs.h>

#if defined(CONFIG_ARCH_S390)
#include <asm/s390mach.h>
#include <asm/ccwcache.h>
#endif

#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#endif

#ifdef CONFIG_PCI
#include <linux/pci.h>
#endif

#ifdef CONFIG_DIO
#include <linux/dio.h>
#endif

#ifdef CONFIG_ZORRO
#include <linux/zorro.h>
#endif

#ifdef CONFIG_MTRR
#  include <asm/mtrr.h>
#endif

#ifdef CONFIG_NUBUS
#include <linux/nubus.h>
#endif

#ifdef CONFIG_ISAPNP
#include <linux/isapnp.h>
#endif

#ifdef CONFIG_IRDA
extern int irda_proto_init(void);
extern int irda_device_init(void);
#endif

#ifdef CONFIG_X86_LOCAL_APIC
#include <asm/smp.h>
#endif

/*
 * Versions of gcc older than that listed below may actually compile
 * and link okay, but the end product can have subtle run time bugs.
 * To avoid associated bogus bug reports, we flatly refuse to compile
 * with a gcc that is known to be too old from the very beginning.
 */
#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 91)
#error Sorry, your GCC is too old. It builds incorrect kernels.
#endif

extern char _stext, _etext;
extern char *linux_banner;

static int init(void *);

extern void init_IRQ(void);
extern void init_modules(void);
extern void sock_init(void);
extern void fork_init(unsigned long);
extern void mca_init(void);
extern void sbus_init(void);
extern void ppc_init(void);
extern void sysctl_init(void);
extern void signals_init(void);
extern int init_pcmcia_ds(void);

extern void free_initmem(void);

#ifdef CONFIG_TC
extern void tc_init(void);
#endif

extern void ecard_init(void);

#if defined(CONFIG_SYSVIPC)
extern void ipc_init(void);
#endif

/*
 * Boot command-line arguments
 */
#define MAX_INIT_ARGS 8
#define MAX_INIT_ENVS 8

extern void time_init(void);
extern void softirq_init(void);

int rows, cols;

char *execute_command;

static char * argv_init[MAX_INIT_ARGS+2] = { "init", NULL, };
char * envp_init[MAX_INIT_ENVS+2] = { "HOME=/", "TERM=linux", NULL, };

static int __init profile_setup(char *str)
{
    int par;
    if (get_option(&str,&par)) prof_shift = par;
	return 1;
}

__setup("profile=", profile_setup);

static int __init checksetup(char *line)
{
	struct kernel_param *p;

	p = &__setup_start;
	do {
		int n = strlen(p->str);
		if (!strncmp(line,p->str,n)) {
			if (p->setup_func(line+n))
				return 1;
		}
		p++;
	} while (p < &__setup_end);
	return 0;
}

/* this should be approx 2 Bo*oMips to start (note initial shift), and will
   still work even if initially too large, it will just take slightly longer */
unsigned long loops_per_jiffy = (1<<12);

/* This is the number of bits of precision for the loops_per_jiffy.  Each
   bit takes on average 1.5/HZ seconds.  This (like the original) is a little
   better than 1% */
#define LPS_PREC 8

void __init calibrate_delay(void)
{
	unsigned long ticks, loopbit;
	int lps_precision = LPS_PREC;

	loops_per_jiffy = (1<<12);

	printk("Calibrating delay loop... ");
	while (loops_per_jiffy <<= 1) {
		/* wait for "start of" clock tick */
		ticks = jiffies;
		while (ticks == jiffies)
			/* nothing */;
		/* Go .. */
		ticks = jiffies;
		__delay(loops_per_jiffy);
		ticks = jiffies - ticks;
		if (ticks)
			break;
	}

/* Do a binary approximation to get loops_per_jiffy set to equal one clock
   (up to lps_precision bits) */
	loops_per_jiffy >>= 1;
	loopbit = loops_per_jiffy;
	while ( lps_precision-- && (loopbit >>= 1) ) {
		loops_per_jiffy |= loopbit;
		ticks = jiffies;
		while (ticks == jiffies);
		ticks = jiffies;
		__delay(loops_per_jiffy);
		if (jiffies != ticks)	/* longer than 1 tick */
			loops_per_jiffy &= ~loopbit;
	}

/* Round the value and print it */	
	printk("%lu.%02lu BogoMIPS\n",
		loops_per_jiffy/(500000/HZ),
		(loops_per_jiffy/(5000/HZ)) % 100);
}

static int __init debug_kernel(char *str)
{
	if (*str)
		return 0;
	console_loglevel = 10;
	return 1;
}

static int __init quiet_kernel(char *str)
{
	if (*str)
		return 0;
	console_loglevel = 4;
	return 1;
}

__setup("debug", debug_kernel);
__setup("quiet", quiet_kernel);

/*
 * This is a simple kernel command line parsing function: it parses
 * the command line, and fills in the arguments/environment to init
 * as appropriate. Any cmd-line option is taken to be an environment
 * variable if it contains the character '='.
 *
 * This routine also checks for options meant for the kernel.
 * These options are not given to init - they are for internal kernel use only.
 */
static void __init parse_options(char *line)
{
	char *next,*quote;
	int args, envs;

	if (!*line)
		return;
	args = 0;
	envs = 1;	/* TERM is set to 'linux' by default */
	next = line;
	while ((line = next) != NULL) {
                quote = strchr(line,'"');
                next = strchr(line, ' ');
                while (next != NULL && quote != NULL && quote < next) {
                        /* we found a left quote before the next blank
                         * now we have to find the matching right quote
                         */
                        next = strchr(quote+1, '"');
                        if (next != NULL) {
                                quote = strchr(next+1, '"');
                                next = strchr(next+1, ' ');
                        }
                }
                if (next != NULL)
                        *next++ = 0;
		if (!strncmp(line,"init=",5)) {
			line += 5;
			execute_command = line;
			/* In case LILO is going to boot us with default command line,
			 * it prepends "auto" before the whole cmdline which makes
			 * the shell think it should execute a script with such name.
			 * So we ignore all arguments entered _before_ init=... [MJ]
			 */
			args = 0;
			continue;
		}
		if (checksetup(line))
			continue;
		
		/*
		 * Then check if it's an environment variable or
		 * an option.
		 */
		if (strchr(line,'=')) {
			if (envs >= MAX_INIT_ENVS)
				break;
			envp_init[++envs] = line;
		} else {
			if (args >= MAX_INIT_ARGS)
				break;
			if (*line)
				argv_init[++args] = line;
		}
	}
	argv_init[args+1] = NULL;
	envp_init[envs+1] = NULL;
}


extern void setup_arch(char **);
extern void cpu_idle(void);

unsigned long wait_init_idle;

#ifndef CONFIG_SMP

#ifdef CONFIG_X86_LOCAL_APIC
static void __init smp_init(void)
{
	APIC_init_uniprocessor();
}
#else
#define smp_init()	do { } while (0)
#endif

#else


/* Called by boot processor to activate the rest. */
static void __init smp_init(void)
{
	/* Get other processors into their bootup holding patterns. */
	smp_boot_cpus();
	wait_init_idle = cpu_online_map;
	clear_bit(current->processor, &wait_init_idle); /* Don't wait on me! */

	smp_threads_ready=1;
	smp_commence();

	/* Wait for the other cpus to set up their idle processes */
	printk("Waiting on wait_init_idle (map = 0x%lx)\n", wait_init_idle);
	while (wait_init_idle) {
		cpu_relax();
		barrier();
	}
	printk("All processors have done init_idle\n");
}

#endif

/*
 * We need to finalize in a non-__init function or else race conditions
 * between the root thread and the init thread may cause start_kernel to
 * be reaped by free_initmem before the root thread has proceeded to
 * cpu_idle.
 */

static void rest_init(void)
{
	kernel_thread(init, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	unlock_kernel();
	current->need_resched = 1;
 	cpu_idle();
} 

/*
 *	Activate the first processor.
 */

asmlinkage void __init start_kernel(void)
{
	char * command_line;
	extern char saved_command_line[];
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
	lock_kernel();
	printk(linux_banner);
	setup_arch(&command_line);
	printk("Kernel command line: %s\n", saved_command_line);
	parse_options(command_line);
	trap_init();
	init_IRQ();
	sched_init();
	softirq_init();
	time_init();

	/*
	 * HACK ALERT! This is early. We're enabling the console before
	 * we've done PCI setups etc, and console_init() must be aware of
	 * this. But we do want output early, in case something goes wrong.
	 */
	console_init();
#ifdef CONFIG_MODULES
	init_modules();
#endif
	if (prof_shift) {
		unsigned int size;
		/* only text is profiled */
		prof_len = (unsigned long) &_etext - (unsigned long) &_stext;
		prof_len >>= prof_shift;
		
		size = prof_len * sizeof(unsigned int) + PAGE_SIZE-1;
		prof_buffer = (unsigned int *) alloc_bootmem(size);
	}

	kmem_cache_init();
	sti();
	calibrate_delay();
#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start && !initrd_below_start_ok &&
			initrd_start < min_low_pfn << PAGE_SHIFT) {
		printk(KERN_CRIT "initrd overwritten (0x%08lx < 0x%08lx) - "
		    "disabling it.\n",initrd_start,min_low_pfn << PAGE_SHIFT);
		initrd_start = 0;
	}
#endif
	mem_init();
	kmem_cache_sizes_init();
	pgtable_cache_init();

	/*
	 * For architectures that have highmem, num_mappedpages represents
	 * the amount of memory the kernel can use.  For other architectures
	 * it's the same as the total pages.  We need both numbers because
	 * some subsystems need to initialize based on how much memory the
	 * kernel can use.
	 */
	if (num_mappedpages == 0)
		num_mappedpages = num_physpages;
  
	fork_init(num_mappedpages);
	proc_caches_init();
	vfs_caches_init(num_physpages);
	buffer_init(num_physpages);
	page_cache_init(num_physpages);
#if defined(CONFIG_ARCH_S390)
	ccwcache_init();
#endif
	signals_init();
#ifdef CONFIG_PROC_FS
	proc_root_init();
#endif
	check_bugs();
	printk("POSIX conformance testing by UNIFIX\n");

	/* 
	 *	We count on the initial thread going ok 
	 *	Like idlers init is an unlocked kernel thread, which will
	 *	make syscalls (and thus be locked).
	 */
	smp_init();
#if defined(CONFIG_SYSVIPC)
	ipc_init();
#endif
	rest_init();
}

struct task_struct *child_reaper = &init_task;

static void __init do_initcalls(void)
{
	initcall_t *call;

	call = &__initcall_start;
	do {
		(*call)();
		call++;
	} while (call < &__initcall_end);

	/* Make sure there is no pending stuff from the initcall sequence */
	flush_scheduled_tasks();
}

/*
 * Ok, the machine is now initialized. None of the devices
 * have been touched yet, but the CPU subsystem is up and
 * running, and memory and process management works.
 *
 * Now we can finally start doing some real work..
 */
static void __init do_basic_setup(void)
{

	/*
	 * Tell the world that we're going to be the grim
	 * reaper of innocent orphaned children.
	 *
	 * We don't want people to have to make incorrect
	 * assumptions about where in the task array this
	 * can be found.
	 */
	child_reaper = current;

#if defined(CONFIG_MTRR)	/* Do this after SMP initialization */
/*
 * We should probably create some architecture-dependent "fixup after
 * everything is up" style function where this would belong better
 * than in init/main.c..
 */
	mtrr_init();
#endif

#ifdef CONFIG_SYSCTL
	sysctl_init();
#endif

	/*
	 * Ok, at this point all CPU's should be initialized, so
	 * we can start looking into devices..
	 */
#if defined(CONFIG_ARCH_S390)
	s390_init_machine_check();
#endif
#ifdef CONFIG_ACPI_INTERPRETER
	acpi_init();
#endif
#ifdef CONFIG_PCI
	pci_init();
#endif
#ifdef CONFIG_SBUS
	sbus_init();
#endif
#if defined(CONFIG_PPC)
	ppc_init();
#endif
#ifdef CONFIG_MCA
	mca_init();
#endif
#ifdef CONFIG_ARCH_ACORN
	ecard_init();
#endif
#ifdef CONFIG_ZORRO
	zorro_init();
#endif
#ifdef CONFIG_DIO
	dio_init();
#endif
#ifdef CONFIG_NUBUS
	nubus_init();
#endif
#ifdef CONFIG_ISAPNP
	isapnp_init();
#endif
#ifdef CONFIG_TC
	tc_init();
#endif

	/* Networking initialization needs a process context */ 
	sock_init();

	start_context_thread();
	do_initcalls();

#ifdef CONFIG_IRDA
	irda_proto_init();
	irda_device_init(); /* Must be done after protocol initialization */
#endif
#ifdef CONFIG_PCMCIA
	init_pcmcia_ds();		/* Do this last */
#endif
}

static void run_init_process(char *init_filename)
{
	argv_init[0] = init_filename;
	execve(init_filename, argv_init, envp_init);
}

extern void prepare_namespace(void);

static int init(void * unused)
{
	struct files_struct *files;
	lock_kernel();
	do_basic_setup();

	prepare_namespace();

	/*
	 * Ok, we have completed the initial bootup, and
	 * we're essentially up and running. Get rid of the
	 * initmem segments and start the user-mode stuff..
	 */
	free_initmem();
	unlock_kernel();
	
	/*
	 * Right now we are a thread sharing with a ton of kernel
	 * stuff. We don't want to end up in user space in that state
	 */
	 
	files = current->files;
	if(unshare_files())
		panic("unshare");
	put_files_struct(files);
	
	if (open("/dev/console", O_RDWR, 0) < 0)
		printk("Warning: unable to open an initial console.\n");

	(void) dup(0);
	(void) dup(0);
	
	/*
	 * We try each of these until one succeeds.
	 *
	 * The Bourne shell can be used instead of init if we are 
	 * trying to recover a really broken machine.
	 */

	if (execute_command)
		run_init_process(execute_command);

	run_init_process("/sbin/init");
	run_init_process("/etc/init");
	run_init_process("/bin/init");
	run_init_process("/bin/sh");

	panic("No init found.  Try passing init= option to kernel.");
}
