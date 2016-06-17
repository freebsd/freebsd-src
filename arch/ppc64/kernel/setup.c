/*
 * 
 * Common boot and setup code.
 *
 * Copyright (C) 2001 PPC64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/blk.h>
#include <linux/ide.h>
#include <linux/seq_file.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/version.h>
#include <asm/init.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/bootinfo.h>
#include <asm/smp.h>
#include <asm/elf.h>
#include <asm/machdep.h>
#include <asm/iSeries/LparData.h>
#include <asm/naca.h>
#include <asm/paca.h>
#include <asm/ppcdebug.h>
#include <asm/time.h>
#include <asm/cputable.h>

extern unsigned long klimit;
/* extern void *stab; */
extern HTAB htab_data;
extern unsigned long loops_per_jiffy;
extern int blk_nohighio;

extern unsigned long embedded_sysmap_start;
extern unsigned long embedded_sysmap_end;

int have_of = 1;

extern void  chrp_init(unsigned long r3,
		       unsigned long r4,
		       unsigned long r5,
		       unsigned long r6,
		       unsigned long r7);

extern void chrp_init_map_io_space( void );
extern void iSeries_init( void );
extern void iSeries_init_early( void );
extern void pSeries_init_early( void );
extern void pSeriesLP_init_early(void);
extern void mm_init_ppc64( void ); 
extern void pseries_secondary_smp_init(unsigned long);

unsigned long decr_overclock = 1;
unsigned long decr_overclock_proc0 = 1;
unsigned long decr_overclock_set = 0;
unsigned long decr_overclock_proc0_set = 0;

#ifdef CONFIG_XMON
extern void xmon_map_scc(void);
#endif

char saved_command_line[256];
unsigned char aux_device_present;

void parse_cmd_line(unsigned long r3, unsigned long r4, unsigned long r5,
		    unsigned long r6, unsigned long r7);
int parse_bootinfo(void);

#ifdef CONFIG_MAGIC_SYSRQ
unsigned long SYSRQ_KEY;
#endif /* CONFIG_MAGIC_SYSRQ */

struct machdep_calls ppc_md;

/*
 * These are used in binfmt_elf.c to put aux entries on the stack
 * for each elf executable being started.
 */
int dcache_bsize;
int icache_bsize;
int ucache_bsize;

struct console udbg_console = {
	name:	"udbg",
	write:	udbg_console_write,
	flags:	CON_PRINTBUFFER,
	index:	-1,
};

/*
 * Do some initial setup of the system.  The parameters are those which 
 * were passed in from the bootloader.
 */
void setup_system(unsigned long r3, unsigned long r4, unsigned long r5,
		  unsigned long r6, unsigned long r7)
{
	unsigned int ret, i;

	/* This should be fixed properly in kernel/resource.c */
	iomem_resource.end = MEM_SPACE_LIMIT;

	/* pSeries systems are identified in prom.c via OF. */
	if ( itLpNaca.xLparInstalled == 1 )
		systemcfg->platform = PLATFORM_ISERIES_LPAR;
	
	switch (systemcfg->platform) {
	case PLATFORM_ISERIES_LPAR:
		iSeries_init_early();
		break;

#ifdef CONFIG_PPC_PSERIES
	case PLATFORM_PSERIES:
		pSeries_init_early();
#ifdef CONFIG_BLK_DEV_INITRD
		initrd_start = initrd_end = 0;
#endif
		parse_bootinfo();
		break;

	case PLATFORM_PSERIES_LPAR:
		pSeriesLP_init_early();
#ifdef CONFIG_BLK_DEV_INITRD
		initrd_start = initrd_end = 0;
#endif
		parse_bootinfo();
		break;
#endif
	}

	if (systemcfg->platform & PLATFORM_PSERIES) {
		register_console(&udbg_console);
		udbg_printf("---- start early boot console ----\n");
	}

	if (systemcfg->platform & PLATFORM_PSERIES) {
		finish_device_tree();
		chrp_init(r3, r4, r5, r6, r7);

		/* Start secondary threads on SMT systems */
		for (i = 0; i < NR_CPUS; i++) {
			if(cpu_available(i)  && !cpu_possible(i)) {
				printk("%16.16lx : starting thread\n", i);
				rtas_call(rtas_token("start-cpu"), 3, 1,
					  (void *)&ret,
					  i, *((unsigned long *)pseries_secondary_smp_init), i);
				paca[i].active = 1;
				systemcfg->processorCount++;
			}
		}
	}

	printk("-----------------------------------------------------\n");
	printk("naca                          = 0x%p\n", naca);
	printk("naca->pftSize                 = 0x%lx\n", naca->pftSize);
	printk("naca->paca                    = 0x%p\n\n", naca->paca);
	printk("systemcfg                     = 0x%p\n", systemcfg);
	printk("systemcfg->platform           = 0x%x\n", systemcfg->platform);
	printk("systemcfg->processor          = 0x%x\n", systemcfg->processor);
	printk("systemcfg->processorCount     = 0x%lx\n", systemcfg->processorCount);
	printk("systemcfg->physicalMemorySize = 0x%lx\n", systemcfg->physicalMemorySize);
	printk("systemcfg->dCacheL1LineSize   = 0x%x\n", systemcfg->dCacheL1LineSize);
	printk("systemcfg->iCacheL1LineSize   = 0x%x\n", systemcfg->iCacheL1LineSize);
	printk("htab_data.htab                = 0x%p\n", htab_data.htab);
	printk("htab_data.num_ptegs           = 0x%lx\n", htab_data.htab_num_ptegs);
	printk("-----------------------------------------------------\n");

	printk("Starting Linux PPC64 %s\n", UTS_RELEASE);

	mm_init_ppc64();

	if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
		vpa_init(0);
	}

	idle_setup();

	switch (systemcfg->platform) {
	    case PLATFORM_ISERIES_LPAR:
		iSeries_init();
		break;
	    default:
		/* The following relies on the device tree being */
		/* fully configured.                             */
		parse_cmd_line(r3, r4, r5, r6, r7);
	}
	ppc64_boot_msg(0x10, "Setup System");
}

/* This is called just before console_init().
 * It will be obsolete when Linux gets real early console support (2.5?)
 * We need to hack preferred_console to retain the correct behavior
 */
void setup_before_console_init(void)
{
	if (systemcfg->platform & PLATFORM_PSERIES) {
		unregister_console(&udbg_console);
		udbg_console.next = NULL;
		udbg_printf("---- end early boot console ----\n");
	}
}

void machine_restart(char *cmd)
{
	ppc_md.restart(cmd);
}
  
void machine_power_off(void)
{
	ppc_md.power_off();
}
  
void machine_halt(void)
{
	ppc_md.halt();
}

unsigned long ppc_proc_freq;
unsigned long ppc_tb_freq;

static int show_cpuinfo(struct seq_file *m, void *v)
{
	unsigned long cpu_id = (unsigned long)v - 1;
	unsigned int pvr;
	unsigned short maj;
	unsigned short min;

#ifdef CONFIG_SMP
	if (cpu_id == NR_CPUS) {

		if (ppc_md.get_cpuinfo != NULL)
			ppc_md.get_cpuinfo(m);

		return 0;
	}

	if (!(cpu_online_map & (1<<cpu_id)))
		return 0;
#endif

	pvr = paca[cpu_id].pvr;
	maj = (pvr >> 8) & 0xFF;
	min = pvr & 0xFF;

	seq_printf(m, "processor\t: %lu\n", cpu_id);
	seq_printf(m, "cpu\t\t: ");

	pvr = paca[cpu_id].pvr;

	if (cur_cpu_spec->pvr_mask)
		seq_printf(m, "%s", cur_cpu_spec->cpu_name);
	else
		seq_printf(m, "unknown (%08x)", pvr);


	seq_printf(m, "\n");

	/*
	 * Assume here that all clock rates are the same in a
	 * smp system.  -- Cort
	 */
	if (systemcfg->platform != PLATFORM_ISERIES_LPAR) {
		struct device_node *cpu_node;
		int *fp;

		cpu_node = find_type_devices("cpu");
		if (cpu_node) {
			fp = (int *) get_property(cpu_node, "clock-frequency",
						  NULL);
			if (fp)
				seq_printf(m, "clock\t\t: %dMHz\n",
					   *fp / 1000000);
		}
	}

	if (ppc_md.setup_residual != NULL)
		ppc_md.setup_residual(m, cpu_id);

	seq_printf(m, "revision\t: %hd.%hd\n\n", maj, min);
	
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos <= NR_CPUS ? (void *)((*pos)+1) : NULL;
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
	.start =c_start,
	.next =	c_next,
	.stop =	c_stop,
	.show =	show_cpuinfo,
};

/*
 * Fetch the cmd_line from open firmware. 
 */
void parse_cmd_line(unsigned long r3, unsigned long r4, unsigned long r5,
		  unsigned long r6, unsigned long r7)
{
	char *p;

#ifdef CONFIG_BLK_DEV_INITRD
	if ((initrd_start == 0) && r3 && r4 && r4 != 0xdeadbeef) {
		initrd_start = (r3 >= KERNELBASE) ? r3 : (unsigned long)__va(r3);
		initrd_end = initrd_start + r4;
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
		initrd_below_start_ok = 1;
		lmb_reserve(__pa(initrd_start),r4);
	}
#endif

	/* Hack -- add console=ttySn,9600 if necessary */
	if(strstr(cmd_line, "console=") == NULL) {
		struct device_node *prom_stdout = find_path_device(of_stdout_device);
		u32 *reg;
		int i;
		char *name, *val = NULL;
		printk("of_stdout_device %s\n", of_stdout_device);
		if (prom_stdout) {
			name = (char *)get_property(prom_stdout, "name", NULL);
			if (name) {
				if (strcmp(name, "serial") == 0) {
					reg = (u32 *)get_property(prom_stdout, "reg", &i);
					if (i > 8) {
						switch (reg[1]) {
						    case 0x3f8: val = "ttyS0,9600"; break; 
						    case 0x2f8: val = "ttyS1,9600"; break; 
						    case 0x898: val = "ttyS2,9600"; break; 
						    case 0x890: val = "ttyS3,9600"; break; 
						}
					}
				} else if (strcmp(name, "vty") == 0) {
					/* pSeries LPAR virtual console */
					val = "hvc0";
				}
				if (val) {
					char tmp_cmd_line[512];
					snprintf(tmp_cmd_line, 512, 
						 "AUTOCONSOLE console=%s %s",
						 val, cmd_line);
					memcpy(cmd_line, tmp_cmd_line, 512);
					printk("console= not found, add console=%s\ncmd_line is now %s\n",
					       val, cmd_line);
				}
			}
		}
	}	

	/* Look for mem= option on command line */
	if (strstr(cmd_line, "mem=")) {
		char *q;
		unsigned long maxmem = 0;
		extern unsigned long __max_memory;

		for (q = cmd_line; (p = strstr(q, "mem=")) != 0; ) {
			q = p + 4;
			if (p > cmd_line && p[-1] != ' ')
				continue;
			maxmem = simple_strtoul(q, &q, 0);
			if (*q == 'k' || *q == 'K') {
				maxmem <<= 10;
				++q;
			} else if (*q == 'm' || *q == 'M') {
				maxmem <<= 20;
				++q;
			}
		}
		__max_memory = maxmem;
	}
}

#if 0
char *bi_tag2str(unsigned long tag)
{
	switch (tag) {
	case BI_FIRST:
		return "BI_FIRST";
	case BI_LAST:
		return "BI_LAST";
	case BI_CMD_LINE:
		return "BI_CMD_LINE";
	case BI_BOOTLOADER_ID:
		return "BI_BOOTLOADER_ID";
	case BI_INITRD:
		return "BI_INITRD";
	case BI_SYSMAP:
		return "BI_SYSMAP";
	case BI_MACHTYPE:
		return "BI_MACHTYPE";
	default:
		return "BI_UNKNOWN";
	}
}
#endif

int parse_bootinfo(void)
{
	struct bi_record *rec;
	extern char *sysmap;
	extern unsigned long sysmap_size;

	rec = prom.bi_recs;

	if ( rec == NULL || rec->tag != BI_FIRST )
		return -1;

	for ( ; rec->tag != BI_LAST ; rec = bi_rec_next(rec) ) {
		switch (rec->tag) {
		case BI_CMD_LINE:
			memcpy(cmd_line, (void *)rec->data, rec->size);
			break;
		case BI_SYSMAP:
			sysmap = (char *)((rec->data[0] >= (KERNELBASE))
					? rec->data[0] : (unsigned long)__va(rec->data[0]));
			sysmap_size = rec->data[1];
			break;
#ifdef CONFIG_BLK_DEV_INITRD
		case BI_INITRD:
			initrd_start = (unsigned long)__va(rec->data[0]);
			initrd_end = initrd_start + rec->data[1];
			ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
			initrd_below_start_ok = 1;
			break;
#endif /* CONFIG_BLK_DEV_INITRD */
		}
	}

	return 0;
}

void __init ppc_init(void)
{
	/* clear the progress line */
	ppc_md.progress(" ", 0xffff);

	if (ppc_md.init != NULL) {
		ppc_md.init();
	}
}

void __init ppc64_calibrate_delay(void)
{
	loops_per_jiffy = tb_ticks_per_jiffy;

	printk("Calibrating delay loop... %lu.%02lu BogoMips\n",
			       loops_per_jiffy/(500000/HZ),
			       loops_per_jiffy/(5000/HZ) % 100);
}	

extern void (*calibrate_delay)(void);
extern void sort_exception_table(void);

/*
 * Called into from start_kernel, after lock_kernel has been called.
 * Initializes bootmem, which is unsed to manage page allocation until
 * mem_init is called.
 */
void __init setup_arch(char **cmdline_p)
{
	extern int panic_timeout;
	extern char _etext[], _edata[];
	extern void do_init_bootmem(void);

	blk_nohighio = 1;

	calibrate_delay = ppc64_calibrate_delay;

	ppc64_boot_msg(0x12, "Setup Arch");
#ifdef CONFIG_XMON
	xmon_map_scc();
	if (strstr(cmd_line, "xmon"))
		xmon(0);
#endif /* CONFIG_XMON */

#if defined(CONFIG_KGDB)
	kgdb_map_scc();
	set_debug_traps();
	breakpoint();
#endif
	/*
	 * Set cache line size based on type of cpu as a default.
	 * Systems with OF can look in the properties on the cpu node(s)
	 * for a possibly more accurate value.
	 */
	dcache_bsize = systemcfg->dCacheL1LineSize; 
	icache_bsize = systemcfg->iCacheL1LineSize; 

	/* reboot on panic */
	panic_timeout = 180;

	init_mm.start_code = PAGE_OFFSET;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = (unsigned long) klimit;
	
	/* Save unparsed command line copy for /proc/cmdline */
	strcpy(saved_command_line, cmd_line);
	*cmdline_p = cmd_line;

	/* set up the bootmem stuff with available memory */
	do_init_bootmem();

	ppc_md.setup_arch();

	paging_init();
	sort_exception_table();
	ppc64_boot_msg(0x15, "Setup Done");
}


/* ToDo: do something useful if ppc_md is not yet setup. */
#define PPC64_LINUX_FUNCTION 0x0f000000
#define PPC64_IPL_MESSAGE 0xc0000000
#define PPC64_TERM_MESSAGE 0xb0000000
#define PPC64_ATTN_MESSAGE 0xa0000000
#define PPC64_DUMP_MESSAGE 0xd0000000

static void ppc64_do_msg(unsigned int src, const char *msg)
{
	if (ppc_md.progress) {
		char buf[32];

		sprintf(buf, "%08x        \n", src);
		ppc_md.progress(buf, 0);
		sprintf(buf, "%-16s", msg);
		ppc_md.progress(buf, 0);
	}
}

/* Print a boot progress message. */
void ppc64_boot_msg(unsigned int src, const char *msg)
{
	ppc64_do_msg(PPC64_LINUX_FUNCTION|PPC64_IPL_MESSAGE|src, msg);
	udbg_printf("[boot]%04x %s\n", src, msg);
}

/* Print a termination message (print only -- does not stop the kernel) */
void ppc64_terminate_msg(unsigned int src, const char *msg)
{
	ppc64_do_msg(PPC64_LINUX_FUNCTION|PPC64_TERM_MESSAGE|src, msg);
	udbg_printf("[terminate]%04x %s\n", src, msg);
}

/* Print something that needs attention (device error, etc) */
void ppc64_attention_msg(unsigned int src, const char *msg)
{
	ppc64_do_msg(PPC64_LINUX_FUNCTION|PPC64_ATTN_MESSAGE|src, msg);
	udbg_printf("[attention]%04x %s\n", src, msg);
}

/* Print a dump progress message. */
void ppc64_dump_msg(unsigned int src, const char *msg)
{
	ppc64_do_msg(PPC64_LINUX_FUNCTION|PPC64_DUMP_MESSAGE|src, msg);
	udbg_printf("[dump]%04x %s\n", src, msg);
}


void exception_trace(unsigned long trap)
{
	unsigned long x, srr0, srr1, reg20, reg1, reg21;

	asm("mflr %0" : "=r" (x) :);
	asm("mfspr %0,0x1a" : "=r" (srr0) :);
	asm("mfspr %0,0x1b" : "=r" (srr1) :);
	asm("mr %0,1" : "=r" (reg1) :);
	asm("mr %0,20" : "=r" (reg20) :);
	asm("mr %0,21" : "=r" (reg21) :);

	udbg_puts("\n");
	udbg_puts("Took an exception : "); udbg_puthex(x); udbg_puts("\n");
	udbg_puts("   "); udbg_puthex(reg1); udbg_puts("\n");
	udbg_puts("   "); udbg_puthex(reg20); udbg_puts("\n");
	udbg_puts("   "); udbg_puthex(reg21); udbg_puts("\n");
	udbg_puts("   "); udbg_puthex(srr0); udbg_puts("\n");
	udbg_puts("   "); udbg_puthex(srr1); udbg_puts("\n");
}

void do_spread_lpevents(unsigned long n)
{
	unsigned long i,m;

	if (n < MAX_PACAS)
		m = n;
	else
		m = MAX_PACAS;

	for (i=1; i<m; ++i)
		paca[i].lpQueuePtr = paca[0].lpQueuePtr;

	printk("lpevent processing spread over %ld processors\n", n);
}

/*
 * The parameter is the number of processors to share in
 * processing lp events
 */
int set_spread_lpevents(char * str)
{
	unsigned long val = simple_strtoul( str, NULL, 0 );
	if ((val > 0) && (val <= MAX_PACAS)) {
		do_spread_lpevents(val);
	} else
		printk("invalid spreaqd_lpevents %ld\n", val);

	return 1;
}

/* This should only be called on processor 0 during calibrate decr */
void setup_default_decr(void)
{
	struct paca_struct *lpaca = get_paca();

	if ( decr_overclock_set && !decr_overclock_proc0_set )
		decr_overclock_proc0 = decr_overclock;

	lpaca->default_decr = tb_ticks_per_jiffy / decr_overclock_proc0;	
	lpaca->next_jiffy_update_tb = get_tb() + tb_ticks_per_jiffy;
}

int set_decr_overclock_proc0( char * str )
{
	unsigned long val = simple_strtoul( str, NULL, 0 );
	if ( ( val >= 1 ) && ( val <= 48 ) ) {
		decr_overclock_proc0_set = 1;
		decr_overclock_proc0 = val;
		printk("proc 0 decrementer overclock factor of %ld\n", val);
	}
	else
		printk("invalid proc 0 decrementer overclock factor of %ld\n", val);
	return 1;
}

int set_decr_overclock( char * str )
{
	unsigned long val = simple_strtoul( str, NULL, 0 );
	if ( ( val >= 1 ) && ( val <= 48 ) ) {
		decr_overclock_set = 1;
		decr_overclock = val;
		printk("decrementer overclock factor of %ld\n", val);
	}
	else
		printk("invalid decrementer overclock factor of %ld\n", val);
	return 1;

}

__setup("spread_lpevents=", set_spread_lpevents );
__setup("decr_overclock_proc0=", set_decr_overclock_proc0 );
__setup("decr_overclock=", set_decr_overclock );
