/* smp.c: Sparc SMP support.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <asm/head.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/cache.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>

#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/oplib.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#define IRQ_RESCHEDULE		13
#define IRQ_STOP_CPU		14
#define IRQ_CROSS_CALL		15

volatile int smp_processors_ready = 0;
unsigned long cpu_present_map = 0;
int smp_num_cpus = 1;
int smp_threads_ready=0;
unsigned char mid_xlate[NR_CPUS] = { 0, 0, 0, 0, };
volatile unsigned long cpu_callin_map[NR_CPUS] __initdata = {0,};
#ifdef NOTUSED
volatile unsigned long smp_spinning[NR_CPUS] = { 0, };
#endif
unsigned long smp_proc_in_lock[NR_CPUS] = { 0, };
struct cpuinfo_sparc cpu_data[NR_CPUS];
unsigned long cpu_offset[NR_CPUS];
unsigned char boot_cpu_id = 0;
unsigned char boot_cpu_id4 = 0; /* boot_cpu_id << 2 */
int smp_activated = 0;
volatile int __cpu_number_map[NR_CPUS];
volatile int __cpu_logical_map[NR_CPUS];
cycles_t cacheflush_time = 0; /* XXX */

/* The only guaranteed locking primitive available on all Sparc
 * processors is 'ldstub [%reg + immediate], %dest_reg' which atomically
 * places the current byte at the effective address into dest_reg and
 * places 0xff there afterwards.  Pretty lame locking primitive
 * compared to the Alpha and the Intel no?  Most Sparcs have 'swap'
 * instruction which is much better...
 */

/* Kernel spinlock */
spinlock_t kernel_flag __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;

/* Used to make bitops atomic */
unsigned char bitops_spinlock = 0;

volatile unsigned long ipi_count;

volatile int smp_process_available=0;
volatile int smp_commenced = 0;

/* Not supported on Sparc yet. */
void __init smp_setup(char *str, int *ints)
{
}

/*
 *	The bootstrap kernel entry code has set these up. Save them for
 *	a given CPU
 */

void __init smp_store_cpu_info(int id)
{
	cpu_data[id].udelay_val = loops_per_jiffy; /* this is it on sparc. */
}

void __init smp_commence(void)
{
	/*
	 *	Lets the callin's below out of their loop.
	 */
	local_flush_cache_all();
	local_flush_tlb_all();
	smp_commenced = 1;
	local_flush_cache_all();
	local_flush_tlb_all();
}

extern int cpu_idle(void);

/* Activate a secondary processor. */
int start_secondary(void *unused)
{
	prom_printf("Start secondary called. Should not happen\n");
	return cpu_idle();
}

void cpu_panic(void)
{
	printk("CPU[%d]: Returns from cpu_idle!\n", smp_processor_id());
	panic("SMP bolixed\n");
}

/*
 *	Cycle through the processors asking the PROM to start each one.
 */
 
extern struct prom_cpuinfo linux_cpus[NR_CPUS];
struct linux_prom_registers smp_penguin_ctable __initdata = { 0 };

void __init smp_boot_cpus(void)
{
	extern void smp4m_boot_cpus(void);
	extern void smp4d_boot_cpus(void);
	
	if (sparc_cpu_model == sun4m)
		smp4m_boot_cpus();
	else
		smp4d_boot_cpus();
}

void smp_flush_cache_all(void)
{
	xc0((smpfunc_t) BTFIXUP_CALL(local_flush_cache_all));
	local_flush_cache_all();
}

void smp_flush_tlb_all(void)
{
	xc0((smpfunc_t) BTFIXUP_CALL(local_flush_tlb_all));
	local_flush_tlb_all();
}

void smp_flush_cache_mm(struct mm_struct *mm)
{
	if(mm->context != NO_CONTEXT) {
		if(mm->cpu_vm_mask != (1 << smp_processor_id()))
			xc1((smpfunc_t) BTFIXUP_CALL(local_flush_cache_mm), (unsigned long) mm);
		local_flush_cache_mm(mm);
	}
}

void smp_flush_tlb_mm(struct mm_struct *mm)
{
	if(mm->context != NO_CONTEXT) {
		if(mm->cpu_vm_mask != (1 << smp_processor_id())) {
			xc1((smpfunc_t) BTFIXUP_CALL(local_flush_tlb_mm), (unsigned long) mm);
			if(atomic_read(&mm->mm_users) == 1 && current->active_mm == mm)
				mm->cpu_vm_mask = (1 << smp_processor_id());
		}
		local_flush_tlb_mm(mm);
	}
}

void smp_flush_cache_range(struct mm_struct *mm, unsigned long start,
			   unsigned long end)
{
	if(mm->context != NO_CONTEXT) {
		if(mm->cpu_vm_mask != (1 << smp_processor_id()))
			xc3((smpfunc_t) BTFIXUP_CALL(local_flush_cache_range), (unsigned long) mm, start, end);
		local_flush_cache_range(mm, start, end);
	}
}

void smp_flush_tlb_range(struct mm_struct *mm, unsigned long start,
			 unsigned long end)
{
	if(mm->context != NO_CONTEXT) {
		if(mm->cpu_vm_mask != (1 << smp_processor_id()))
			xc3((smpfunc_t) BTFIXUP_CALL(local_flush_tlb_range), (unsigned long) mm, start, end);
		local_flush_tlb_range(mm, start, end);
	}
}

void smp_flush_cache_page(struct vm_area_struct *vma, unsigned long page)
{
	struct mm_struct *mm = vma->vm_mm;

	if(mm->context != NO_CONTEXT) {
		if(mm->cpu_vm_mask != (1 << smp_processor_id()))
			xc2((smpfunc_t) BTFIXUP_CALL(local_flush_cache_page), (unsigned long) vma, page);
		local_flush_cache_page(vma, page);
	}
}

void smp_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	struct mm_struct *mm = vma->vm_mm;

	if(mm->context != NO_CONTEXT) {
		if(mm->cpu_vm_mask != (1 << smp_processor_id()))
			xc2((smpfunc_t) BTFIXUP_CALL(local_flush_tlb_page), (unsigned long) vma, page);
		local_flush_tlb_page(vma, page);
	}
}

void smp_flush_page_to_ram(unsigned long page)
{
	/* Current theory is that those who call this are the one's
	 * who have just dirtied their cache with the pages contents
	 * in kernel space, therefore we only run this on local cpu.
	 *
	 * XXX This experiment failed, research further... -DaveM
	 */
#if 1
	xc1((smpfunc_t) BTFIXUP_CALL(local_flush_page_to_ram), page);
#endif
	local_flush_page_to_ram(page);
}

void smp_flush_sig_insns(struct mm_struct *mm, unsigned long insn_addr)
{
	if(mm->cpu_vm_mask != (1 << smp_processor_id()))
		xc2((smpfunc_t) BTFIXUP_CALL(local_flush_sig_insns), (unsigned long) mm, insn_addr);
	local_flush_sig_insns(mm, insn_addr);
}

/* Reschedule call back. */
void smp_reschedule_irq(void)
{
	current->need_resched = 1;
}

/* Stopping processors. */
void smp_stop_cpu_irq(void)
{
	__sti();
	while(1)
		barrier();
}

unsigned int prof_multiplier[NR_CPUS];
unsigned int prof_counter[NR_CPUS];
extern unsigned int lvl14_resolution;

int setup_profiling_timer(unsigned int multiplier)
{
	int i;
	unsigned long flags;

	/* Prevent level14 ticker IRQ flooding. */
	if((!multiplier) || (lvl14_resolution / multiplier) < 500)
		return -EINVAL;

	save_and_cli(flags);
	for(i = 0; i < NR_CPUS; i++) {
		if(cpu_present_map & (1 << i)) {
			load_profile_irq(mid_xlate[i], lvl14_resolution / multiplier);
			prof_multiplier[i] = multiplier;
		}
	}
	restore_flags(flags);

	return 0;
}

void smp_bogo_info(struct seq_file *m)
{
	int i;
	
	for (i = 0; i < NR_CPUS; i++) {
		if (cpu_present_map & (1 << i))
			seq_printf(m,
				   "Cpu%dBogo\t: %lu.%02lu\n", 
				   i,
				   cpu_data[i].udelay_val/(500000/HZ),
				   (cpu_data[i].udelay_val/(5000/HZ))%100);
	}
}

void smp_info(struct seq_file *m)
{
	int i;
	
	for (i = 0; i < NR_CPUS; i++) {
		if (cpu_present_map & (1 << i))
			seq_printf(m, "CPU%d\t\t: online\n", i);
	}
}
