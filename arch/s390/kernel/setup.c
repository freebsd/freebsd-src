/*
 *  arch/s390/kernel/setup.c
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "arch/i386/kernel/setup.c"
 *    Copyright (C) 1995, Linus Torvalds
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
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/smp.h>
#include <asm/mmu_context.h>
#include <asm/cpcmd.h>

/*
 * Machine setup..
 */
unsigned int console_mode = 0;
unsigned int console_device = -1;
unsigned long memory_size = 0;
unsigned long machine_flags = 0;
struct { unsigned long addr, size, type; } memory_chunk[16] = { { 0 } };
#define CHUNK_READ_WRITE 0
#define CHUNK_READ_ONLY 1
__u16 boot_cpu_addr;
int cpus_initialized = 0;
unsigned long cpu_initialized = 0;
volatile int __cpu_logical_map[NR_CPUS]; /* logical cpu to cpu address */

unsigned long *pfix_table;
unsigned int __global_storage_key;

unsigned int get_storage_key(void) { return __global_storage_key; }

unsigned long pfix_get_page_addr(void *addr)
{
	if (!MACHINE_HAS_PFIX)
		return (unsigned long) addr;

	return pfix_table[(unsigned long)virt_to_phys(addr)>>PAGE_SHIFT];
}

unsigned long pfix_get_addr(void *addr)
{
	if (!MACHINE_HAS_PFIX)
		return (unsigned long) addr;

	return pfix_get_page_addr(addr) + ((unsigned long)addr&(PAGE_SIZE-1));
}

/*
 * These functions allow to lock and unlock pages in VM. They need
 * the absolute address of the page to be locked or unlocked. This will
 * only work if the diag98 option in the user directory is enabled for
 * the guest.
 */

/* if result is 0, addr will become the host absolute address */
static inline int pfix_lock_page(void *addr)
{
        int ret;

        __asm__ __volatile__(
		"   sske  %1,%2\n"    /* set storage key */
                "   l     0,0(%1)\n"  /* addr into gpr 0 */
                "   lhi   2,0\n"      /* function code is 0 */
                "   diag  2,0,0x98\n" /* result in gpr 1 */
                "   jnz   0f\n"
		"   lhi   %0,0\n"     /* if cc=0: return 0 */
	    	"   st    1,0(%1)\n"  /* gpr1 contains host absol. addr */
		"   j     1f\n"
		"0: lr    %0,1\n"     /* gpr1 contains error code */
		"   sske  %1,2\n"     /* reset storage key */
		"1:\n"
                : "=d" (ret), "+d" ((unsigned long)addr) : "d" (__global_storage_key) 
                : "0", "1", "2", "cc" );
        return ret;
}

static inline void pfix_unlock_page(unsigned long addr)
{
        __asm__ __volatile__(
                "   lr    0,%0\n"  /* parameter in gpr 0 */
                "   lhi   2,4\n"   /* function code is 4 */
                "   diag  2,0,0x98\n" /* result in gpr 1 */
		"   sske  %0,%1\n"
                : : "a" (addr), "d" (0)
                : "0", "1", "2", "cc" );
}

static void unpfix_all_pages(void)
{
        unsigned long i;

	if (!pfix_table)
		return;

	for (i=0; i<sizeof(pfix_table); i++) {
		if (pfix_table[i]) {
			pfix_unlock_page(i << PAGE_SHIFT);
		}
	}
}

static void pfix_all_pages(unsigned long start_pfn, unsigned long max_pfn)
{
        unsigned long i,r;
	unsigned long size;

	size = ((max_pfn - start_pfn) >> PAGE_SHIFT) * sizeof(long);
	pfix_table = alloc_bootmem(size);
	if (!pfix_table)
		return;

	__global_storage_key = 6 << 4;
	for (i = 0; i < 16 && memory_chunk[i].size > 0; i++) {
		unsigned long start;

		for(start = memory_chunk[i].addr; 
		    (start < memory_chunk[i].addr + memory_chunk[i].size &&
		     start < max_pfn); start += PAGE_SIZE) {
			unsigned long index;

			index = start >> PAGE_SHIFT;
			pfix_table[index] = start;
			r = pfix_lock_page(&pfix_table[index]);
			if (r) {
				pfix_table[index] = 0;
				unpfix_all_pages();
				free_bootmem((unsigned long)pfix_table, size);
				machine_flags &= ~128; /* MACHINE_HAS_PFIX=0 */
				__global_storage_key = 0;
				printk(KERN_WARNING "Error enabling PFIX at "
				       "address 0x%08lx.\n", start);
				return;
			}
		}
        }
	printk (KERN_INFO "PFIX enabled.\n");
}

/*
 * Setup options
 */
extern int _text,_etext, _edata, _end;

/*
 * This is set up by the setup-routine at boot-time
 * for S390 need to find out, what we have to setup
 * using address 0x10400 ...
 */

#include <asm/setup.h>

static char command_line[COMMAND_LINE_SIZE] = { 0, };
       char saved_command_line[COMMAND_LINE_SIZE];

static struct resource code_resource = { "Kernel code", 0x100000, 0 };
static struct resource data_resource = { "Kernel data", 0, 0 };

/*
 * cpu_init() initializes state that is per-CPU.
 */
void __init cpu_init (void)
{
        int nr = smp_processor_id();
        int addr = hard_smp_processor_id();

        if (test_and_set_bit(nr,&cpu_initialized)) {
                printk("CPU#%d ALREADY INITIALIZED!!!!!!!!!\n", nr);
                for (;;) __sti();
        }
        cpus_initialized++;

        /*
         * Store processor id in lowcore (used e.g. in timer_interrupt)
         */
        asm volatile ("stidp %0": "=m" (S390_lowcore.cpu_data.cpu_id));
        S390_lowcore.cpu_data.cpu_addr = addr;
        S390_lowcore.cpu_data.cpu_nr = nr;

        /*
         * Force FPU initialization:
         */
        current->flags &= ~PF_USEDFPU;
        current->used_math = 0;

        /* Setup active_mm for idle_task  */
        atomic_inc(&init_mm.mm_count);
        current->active_mm = &init_mm;
        if (current->mm)
                BUG();
        enter_lazy_tlb(&init_mm, current, nr);
}

/*
 * VM halt and poweroff setup routines
 */
char vmhalt_cmd[128] = "";
char vmpoff_cmd[128] = "";

static inline void strncpy_skip_quote(char *dst, char *src, int n)
{
        int sx, dx;

        dx = 0;
        for (sx = 0; src[sx] != 0; sx++) {
                if (src[sx] == '"') continue;
                dst[dx++] = src[sx];
                if (dx >= n) break;
        }
}

static int __init vmhalt_setup(char *str)
{
        strncpy_skip_quote(vmhalt_cmd, str, 127);
        vmhalt_cmd[127] = 0;
        return 1;
}

__setup("vmhalt=", vmhalt_setup);

static int __init vmpoff_setup(char *str)
{
        strncpy_skip_quote(vmpoff_cmd, str, 127);
        vmpoff_cmd[127] = 0;
        return 1;
}

__setup("vmpoff=", vmpoff_setup);

/*
 * condev= and conmode= setup parameter.
 */

static int __init condev_setup(char *str)
{
	int vdev;

	vdev = simple_strtoul(str, &str, 0);
	if (vdev >= 0 && vdev < 65536)
		console_device = vdev;
	return 1;
}

__setup("condev=", condev_setup);

static int __init conmode_setup(char *str)
{
#if defined(CONFIG_HWC_CONSOLE)
	if (strncmp(str, "hwc", 4) == 0)
                SET_CONSOLE_HWC;
#endif
#if defined(CONFIG_TN3215_CONSOLE)
	if (strncmp(str, "3215", 5) == 0)
		SET_CONSOLE_3215;
#endif
#if defined(CONFIG_TN3270_CONSOLE)
	if (strncmp(str, "3270", 5) == 0)
		SET_CONSOLE_3270;
#endif
        return 1;
}

__setup("conmode=", conmode_setup);

static void __init conmode_default(void)
{
	char query_buffer[1024];
	char *ptr;

        if (MACHINE_IS_VM) {
		cpcmd("QUERY TERM", query_buffer, 1024);
		ptr = strstr(query_buffer, "CONMODE");
		/*
		 * Set the conmode to 3215 so that the device recognition 
		 * will set the cu_type of the console to 3215. If the
		 * conmode is 3270 and we don't set it back then both
		 * 3215 and the 3270 driver will try to access the console
		 * device (3215 as console and 3270 as normal tty).
		 */
		cpcmd("TERM CONMODE 3215", NULL, 0);
		if (ptr == NULL) {
#if defined(CONFIG_HWC_CONSOLE)
			SET_CONSOLE_HWC;
#endif
			return;
		}
		if (strncmp(ptr + 8, "3270", 4) == 0) {
#if defined(CONFIG_TN3270_CONSOLE)
			SET_CONSOLE_3270;
#elif defined(CONFIG_TN3215_CONSOLE)
			SET_CONSOLE_3215;
#elif defined(CONFIG_HWC_CONSOLE)
			SET_CONSOLE_HWC;
#endif
		} else if (strncmp(ptr + 8, "3215", 4) == 0) {
#if defined(CONFIG_TN3215_CONSOLE)
			SET_CONSOLE_3215;
#elif defined(CONFIG_TN3270_CONSOLE)
			SET_CONSOLE_3270;
#elif defined(CONFIG_HWC_CONSOLE)
			SET_CONSOLE_HWC;
#endif
		}
        } else if (MACHINE_IS_P390) {
#if defined(CONFIG_TN3215_CONSOLE)
		SET_CONSOLE_3215;
#elif defined(CONFIG_TN3270_CONSOLE)
		SET_CONSOLE_3270;
#endif
	} else {
#if defined(CONFIG_HWC_CONSOLE)
		SET_CONSOLE_HWC;
#endif
	}
}

#ifdef CONFIG_SMP
extern void machine_restart_smp(char *);
extern void machine_halt_smp(void);
extern void machine_power_off_smp(void);

void (*_machine_restart)(char *command) = machine_restart_smp;
void (*_machine_halt)(void) = machine_halt_smp;
void (*_machine_power_off)(void) = machine_power_off_smp;
#else
/*
 * Reboot, halt and power_off routines for non SMP.
 */
static void do_machine_restart_nonsmp(char * __unused)
{
	reipl(S390_lowcore.ipl_device);
}

static void do_machine_halt_nonsmp(void)
{
        if (MACHINE_IS_VM && strlen(vmhalt_cmd) > 0)
                cpcmd(vmhalt_cmd, NULL, 0);
        signal_processor(smp_processor_id(), sigp_stop_and_store_status);
}

static void do_machine_power_off_nonsmp(void)
{
        if (MACHINE_IS_VM && strlen(vmpoff_cmd) > 0)
                cpcmd(vmpoff_cmd, NULL, 0);
        signal_processor(smp_processor_id(), sigp_stop_and_store_status);
}

void (*_machine_restart)(char *command) = do_machine_restart_nonsmp;
void (*_machine_halt)(void) = do_machine_halt_nonsmp;
void (*_machine_power_off)(void) = do_machine_power_off_nonsmp;
#endif

/*
 * Reboot, halt and power_off stubs. They just call _machine_restart,
 * _machine_halt or _machine_power_off. 
 */

void machine_restart(char *command)
{
	_machine_restart(command);
}

void machine_halt(void)
{
	_machine_halt();
}

void machine_power_off(void)
{
	_machine_power_off();
}

/*
 * Setup function called from init/main.c just after the banner
 * was printed.
 */
extern char _pstart, _pend, _stext;

void __init setup_arch(char **cmdline_p)
{
        unsigned long bootmap_size;
        unsigned long memory_start, memory_end;
        char c = ' ', cn, *to = command_line, *from = COMMAND_LINE;
	struct resource *res;
	unsigned long start_pfn, end_pfn, max_pfn=0;
        static unsigned int smptrap=0;
        unsigned long delay = 0;
	struct _lowcore *lowcore;
	int i;
	static int use_pfix;

        if (smptrap)
                return;
        smptrap=1;

        /*
         * print what head.S has found out about the machine 
         */
	printk((MACHINE_IS_VM) ?
	       "We are running under VM (31 bit mode)\n" :
	       "We are running native (31 bit mode)\n");
	if (MACHINE_IS_VM)
		printk((MACHINE_HAS_PFIX) ?
		       "This machine has PFIX support\n" :
		       "This machine has no PFIX support\n");
	printk((MACHINE_HAS_IEEE) ?
	       "This machine has an IEEE fpu\n" :
	       "This machine has no IEEE fpu\n");

        ROOT_DEV = to_kdev_t(0x0100);
        memory_start = (unsigned long) &_end;    /* fixit if use $CODELO etc*/
	memory_end = memory_size & ~0x400000UL;  /* align memory end to 4MB */
        /*
         * We need some free virtual space to be able to do vmalloc.
         * On a machine with 2GB memory we make sure that we have at
         * least 128 MB free space for vmalloc.
         */
        if (memory_end > 1920*1024*1024)
                memory_end = 1920*1024*1024;
        init_mm.start_code = PAGE_OFFSET;
        init_mm.end_code = (unsigned long) &_etext;
        init_mm.end_data = (unsigned long) &_edata;
        init_mm.brk = (unsigned long) &_end;

	code_resource.start = (unsigned long) &_text;
	code_resource.end = (unsigned long) &_etext - 1;
	data_resource.start = (unsigned long) &_etext;
	data_resource.end = (unsigned long) &_edata - 1;

        /* Save unparsed command line copy for /proc/cmdline */
        memcpy(saved_command_line, COMMAND_LINE, COMMAND_LINE_SIZE);
        saved_command_line[COMMAND_LINE_SIZE-1] = '\0';

        for (;;) {
                /*
                 * "mem=XXX[kKmM]" sets memsize 
                 */
                if (c == ' ' && strncmp(from, "mem=", 4) == 0) {
                        memory_end = simple_strtoul(from+4, &from, 0);
                        if ( *from == 'K' || *from == 'k' ) {
                                memory_end = memory_end << 10;
                                from++;
                        } else if ( *from == 'M' || *from == 'm' ) {
                                memory_end = memory_end << 20;
                                from++;
                        }
                }
                /*
                 * "ipldelay=XXX[sm]" sets ipl delay in seconds or minutes
                 */
                if (c == ' ' && strncmp(from, "ipldelay=", 9) == 0) {
                        delay = simple_strtoul(from+9, &from, 0);
			if (*from == 's' || *from == 'S') {
				delay = delay*1000000;
				from++;
			} else if (*from == 'm' || *from == 'M') {
				delay = delay*60*1000000;
				from++;
			}
			/* now wait for the requested amount of time */
			udelay(delay);
                }
		/*
		 * "use_pfix" specifies whether we want to fix all pages,
		 * if possible.
		 */
		if (c == ' ' && strncmp(from, "use_pfix", 8) == 0) {
			use_pfix = 1;
			from += 8;
		}
                cn = *(from++);
                if (!cn)
                        break;
                if (cn == '\n')
                        cn = ' ';  /* replace newlines with space */
		if (cn == 0x0d)
			cn = ' ';  /* replace 0x0d with space */
                if (cn == ' ' && c == ' ')
                        continue;  /* remove additional spaces */
                c = cn;
                if (to - command_line >= COMMAND_LINE_SIZE)
                        break;
                *(to++) = c;
        }

	if (!use_pfix)
		machine_flags &= ~128; /* MACHINE_HAS_PFIX = 0 */

        if (c == ' ' && to > command_line) to--;
        *to = '\0';
        *cmdline_p = command_line;

	/*
	 * partially used pages are not usable - thus
	 * we are rounding upwards:
	 */
	start_pfn = (__pa(&_end) + PAGE_SIZE - 1) >> PAGE_SHIFT;
	end_pfn = memory_end >> PAGE_SHIFT;

	/*
	 * Initialize the boot-time allocator (with low memory only):
	 */
	bootmap_size = init_bootmem(start_pfn, end_pfn);

	/*
	 * Register RAM areas with the bootmem allocator.
	 */
	for (i = 0; i < 16 && memory_chunk[i].size > 0; i++) {
		unsigned long start_chunk, end_chunk;

		if (memory_chunk[i].type != CHUNK_READ_WRITE)
			continue;
		start_chunk = (memory_chunk[i].addr + PAGE_SIZE - 1);
		start_chunk >>= PAGE_SHIFT;
		end_chunk = (memory_chunk[i].addr + memory_chunk[i].size);
		end_chunk >>= PAGE_SHIFT;
		if (start_chunk < start_pfn)
			start_chunk = start_pfn;
		if (end_chunk > end_pfn)
			end_chunk = end_pfn;
		if (start_chunk < end_chunk)
			free_bootmem(start_chunk << PAGE_SHIFT,
				     (end_chunk - start_chunk) << PAGE_SHIFT);
		if (start_chunk + memory_chunk[i].size > max_pfn)
			max_pfn = start_chunk + memory_chunk[i].size;
	}

        /*
         * Reserve the bootmem bitmap itself as well. We do this in two
         * steps (first step was init_bootmem()) because this catches
         * the (very unlikely) case of us accidentally initializing the
         * bootmem allocator with an invalid RAM area.
         */
        reserve_bootmem(start_pfn << PAGE_SHIFT, bootmap_size);

#ifdef CONFIG_BLK_DEV_INITRD
        if (INITRD_START) {
		if (INITRD_START + INITRD_SIZE <= memory_end) {
			reserve_bootmem(INITRD_START, INITRD_SIZE);
			initrd_start = INITRD_START;
			initrd_end = initrd_start + INITRD_SIZE;
		} else {
                        printk("initrd extends beyond end of memory "
                               "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
                               initrd_start + INITRD_SIZE, memory_end);
                        initrd_start = initrd_end = 0;
		}
        }
#endif

        /*
         * Setup lowcore for boot cpu
         */
	lowcore = (struct _lowcore *)
		__alloc_bootmem(PAGE_SIZE, PAGE_SIZE, 0);
	memset(lowcore, 0, PAGE_SIZE);
	lowcore->restart_psw.mask = _RESTART_PSW_MASK;
	lowcore->restart_psw.addr = _ADDR_31 + (addr_t) &restart_int_handler;
	lowcore->external_new_psw.mask = _EXT_PSW_MASK;
	lowcore->external_new_psw.addr = _ADDR_31 + (addr_t) &ext_int_handler;
	lowcore->svc_new_psw.mask = _SVC_PSW_MASK;
	lowcore->svc_new_psw.addr = _ADDR_31 + (addr_t) &system_call;
	lowcore->program_new_psw.mask = _PGM_PSW_MASK;
	lowcore->program_new_psw.addr = _ADDR_31 + (addr_t) &pgm_check_handler;
        lowcore->mcck_new_psw.mask = _MCCK_PSW_MASK;
	lowcore->mcck_new_psw.addr = _ADDR_31 + (addr_t) &mcck_int_handler;
	lowcore->io_new_psw.mask = _IO_PSW_MASK;
	lowcore->io_new_psw.addr = _ADDR_31 + (addr_t) &io_int_handler;
	lowcore->ipl_device = S390_lowcore.ipl_device;
	lowcore->kernel_stack = ((__u32) &init_task_union) + 8192;
	lowcore->async_stack = (__u32)
		__alloc_bootmem(2*PAGE_SIZE, 2*PAGE_SIZE, 0) + 8192;
	lowcore->jiffy_timer = -1LL;
	set_prefix((__u32) lowcore);
        cpu_init();
        boot_cpu_addr = S390_lowcore.cpu_data.cpu_addr;
        __cpu_logical_map[0] = boot_cpu_addr;

	/*
	 * Create kernel page tables and switch to virtual addressing.
	 */
        paging_init();

	if (MACHINE_HAS_PFIX)
		/* max_pfn may be smaller than memory_end. */
		pfix_all_pages(start_pfn, max_pfn);

	res = alloc_bootmem_low(sizeof(struct resource));
	res->start = 0;
	res->end = memory_end;
	res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
	request_resource(&iomem_resource, res);
	request_resource(res, &code_resource);
	request_resource(res, &data_resource);

        /* Setup default console */
	conmode_default();
}

void print_cpu_info(struct cpuinfo_S390 *cpuinfo)
{
   printk("cpu %d "
#ifdef CONFIG_SMP
           "phys_idx=%d "
#endif
           "vers=%02X ident=%06X machine=%04X unused=%04X\n",
           cpuinfo->cpu_nr,
#ifdef CONFIG_SMP
           cpuinfo->cpu_addr,
#endif
           cpuinfo->cpu_id.version,
           cpuinfo->cpu_id.ident,
           cpuinfo->cpu_id.machine,
           cpuinfo->cpu_id.unused);
}

/*
 * show_cpuinfo - Get information on one CPU for use by procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
        struct cpuinfo_S390 *cpuinfo;
	unsigned long n = (unsigned long) v - 1;

	if (!n) {
		seq_printf(m, "vendor_id       : IBM/S390\n"
			       "# processors    : %i\n"
			       "bogomips per cpu: %lu.%02lu\n",
			       smp_num_cpus, loops_per_jiffy/(500000/HZ),
			       (loops_per_jiffy/(5000/HZ))%100);
	}
	if (cpu_online_map & (1 << n)) {
		cpuinfo = &safe_get_cpu_lowcore(n)->cpu_data;
		seq_printf(m, "processor %li: "
			       "version = %02X,  "
			       "identification = %06X,  "
			       "machine = %04X\n",
			       n, cpuinfo->cpu_id.version,
			       cpuinfo->cpu_id.ident,
			       cpuinfo->cpu_id.machine);
	}
        return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos <= NR_CPUS ? (void *)((unsigned long) *pos + 1) : NULL;
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
