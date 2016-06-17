/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/process.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 *
 * Started from SH3/4 version:
 *   Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 *
 *   In turn started from i386 version:
 *     Copyright (C) 1995  Linus Torvalds
 *
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

/* Temporary flags/tests. All to be removed/undefined. BEGIN */
#define IDLE_TRACE
#define VM_SHOW_TABLES
#define VM_TEST_FAULT
#define VM_TEST_RTLBMISS
#define VM_TEST_WTLBMISS

#undef VM_SHOW_TABLES
#undef IDLE_TRACE
/* Temporary flags/tests. All to be removed/undefined. END */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/unistd.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>		/* includes also <asm/registers.h> */
#include <asm/mmu_context.h>
#include <asm/elf.h>
#include <asm/page.h>

#include <linux/irq.h>

struct task_struct *last_task_used_math = NULL;

#ifdef IDLE_TRACE
#ifdef VM_SHOW_TABLES
/* For testing */
static void print_PTE(long base)
{
	int i, skip=0;
	long long x, y, *p = (long long *) base;

	for (i=0; i< 512; i++, p++){
		if (*p == 0) {
			if (!skip) {	
				skip++;
				printk("(0s) ");
			}
		} else {	
			skip=0;
			x = (*p) >> 32;
			y = (*p) & 0xffffffff;
			printk("%08Lx%08Lx ", x, y);
			if (!((i+1)&0x3)) printk("\n");
		}
	}
}

/* For testing */
static void print_DIR(long base)
{
	int i, skip=0;
	long *p = (long *) base;

	for (i=0; i< 512; i++, p++){
		if (*p == 0) {
			if (!skip) {	
				skip++;
				printk("(0s) ");
			}
		} else {	
			skip=0;
			printk("%08lx ", *p);
			if (!((i+1)&0x7)) printk("\n");
		}
	}
}

/* For testing */
static void print_vmalloc_first_tables(void)
{

#define PRESENT	0x800	/* Bit 11 */

	/*
	 * Do it really dirty by looking at raw addresses,
         * raw offsets, no types. If we used pgtable/pgalloc
	 * macros/definitions we could hide potential bugs.
	 *
	 * Note that pointers are 32-bit for CDC.
	 */
	long pgdt, pmdt, ptet;

	pgdt = (long) &swapper_pg_dir;
	printk("-->PGD (0x%08lx):\n", pgdt);
	print_DIR(pgdt);
	printk("\n");

	/* VMALLOC pool is mapped at 0xc0000000, second (pointer) entry in PGD */
	pgdt += 4;
	pmdt = (long) (* (long *) pgdt);
	if (!(pmdt & PRESENT)) {
		printk("No PMD\n");
		return;
	} else pmdt &= 0xfffff000;

	printk("-->PMD (0x%08lx):\n", pmdt);
	print_DIR(pmdt);
	printk("\n");

	/* Get the pmdt displacement for 0xc0000000 */
	pmdt += 2048;

	/* just look at first two address ranges ... */
        /* ... 0xc0000000 ... */
	ptet = (long) (* (long *) pmdt);
	if (!(ptet & PRESENT)) {
		printk("No PTE0\n");
		return;
	} else ptet &= 0xfffff000;

	printk("-->PTE0 (0x%08lx):\n", ptet);
	print_PTE(ptet);
	printk("\n");

        /* ... 0xc0001000 ... */
	ptet += 4;
	if (!(ptet & PRESENT)) {
		printk("No PTE1\n");
		return;
	} else ptet &= 0xfffff000;
	printk("-->PTE1 (0x%08lx):\n", ptet);
	print_PTE(ptet);
	printk("\n");
}
#else
#define print_vmalloc_first_tables()
#endif	/* VM_SHOW_TABLES */

static void test_VM(void)
{
	void *a, *b, *c;

#ifdef VM_SHOW_TABLES
	printk("Initial PGD/PMD/PTE\n");
#endif
        print_vmalloc_first_tables();

	printk("Allocating 2 bytes\n");
	a = vmalloc(2);
        print_vmalloc_first_tables();
        
	printk("Allocating 4100 bytes\n");
	b = vmalloc(4100);
        print_vmalloc_first_tables();

	printk("Allocating 20234 bytes\n");
	c = vmalloc(20234);
        print_vmalloc_first_tables();

#ifdef VM_TEST_FAULT
	/* Here you may want to fault ! */

#ifdef VM_TEST_RTLBMISS
	printk("Ready to fault upon read.\n");
	if (* (char *) a) {
		printk("RTLBMISSed on area a !\n");
	}
	printk("RTLBMISSed on area a !\n");
#endif

#ifdef VM_TEST_WTLBMISS
	printk("Ready to fault upon write.\n");
	*((char *) b) = 'L';
	printk("WTLBMISSed on area b !\n");
#endif

#endif	/* VM_TEST_FAULT */

	printk("Deallocating the 4100 byte chunk\n");
	vfree(b);
        print_vmalloc_first_tables();

	printk("Deallocating the 2 byte chunk\n");
	vfree(a);
        print_vmalloc_first_tables();

	printk("Deallocating the last chunk\n");
	vfree(c);
        print_vmalloc_first_tables();
}

extern unsigned long volatile jiffies;
int once = 0;
unsigned long old_jiffies;
int pid = -1, pgid = -1;

void idle_trace(void)
{

	_syscall0(int, getpid)
	_syscall1(int, getpgid, int, pid)

	if (!once) {
        	/* VM allocation/deallocation simple test */
		test_VM();
		pid = getpid();

        	printk("Got all through to Idle !!\n");
        	printk("I'm now going to loop forever ...\n");
        	printk("Any ! below is a timer tick.\n");
		printk("Any . below is a getpgid system call from pid = %d.\n", pid);


        	old_jiffies = jiffies;
		once++;
	}

	if (old_jiffies != jiffies) {
		old_jiffies = jiffies - old_jiffies;
		switch (old_jiffies) {
		case 1:
			printk("!");
			break;
		case 2:
			printk("!!");
			break;
		case 3:
			printk("!!!");
			break;
		case 4:
			printk("!!!!");
			break;
		default:
			printk("(%d!)", (int) old_jiffies);
		}
		old_jiffies = jiffies;
	}
	pgid = getpgid(pid);
	printk(".");
}
#else
#define idle_trace()	do { } while (0)
#endif	/* IDLE_TRACE */

static int hlt_counter = 1;

#define HARD_IDLE_TIMEOUT (HZ / 3)

void disable_hlt(void)
{
	hlt_counter++;
}

void enable_hlt(void)
{
	hlt_counter--;
}

static int __init nohlt_setup(char *__unused)
{
	hlt_counter = 1;
	return 1;
}

static int __init hlt_setup(char *__unused)
{
	hlt_counter = 0;
	return 1;
}

__setup("nohlt", nohlt_setup);
__setup("hlt", hlt_setup);

static inline void hlt(void)
{
	if (hlt_counter)
		return;

	/* 
	 * FIXME: Is there any reason why we can't just do a "sleep"
	 * instead of this crap?
	 */
	__asm__ __volatile__ (
		".int %0\n\t"
		: /* no outputs */
		: "g" (le32_to_cpu(0x6ff7fff0))
		: "memory"
	);
}

/*
 * The idle loop on a uniprocessor SH..
 */ 
void cpu_idle(void *unused)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;

	while (1) {
		while (!current->need_resched) {
			if (hlt_counter)
				continue;
			__sti();
			idle_trace();
			hlt();
		}
		schedule();
		check_pgt_cache();
	}
}

void machine_restart(char * __unused)
{
	extern void phys_stext(void);

	phys_stext();
}

void machine_halt(void)
{
	for (;;);
}

void machine_power_off(void)
{
	enter_deep_standby();
}

void show_regs(struct pt_regs * regs)
{
	unsigned long long ah, al, bh, bl, ch, cl;

	printk("\n");

	ah = (regs->pc) >> 32;
	al = (regs->pc) & 0xffffffff;
	bh = (regs->regs[18]) >> 32;
	bl = (regs->regs[18]) & 0xffffffff;
	ch = (regs->regs[15]) >> 32;
	cl = (regs->regs[15]) & 0xffffffff;
	printk("PC  : %08Lx%08Lx LINK: %08Lx%08Lx SP  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->sr) >> 32;
	al = (regs->sr) & 0xffffffff;
        asm volatile ("getcon   " __c13 ", %0" : "=r" (bh));
        asm volatile ("getcon   " __c13 ", %0" : "=r" (bl));
	bh = (bh) >> 32;
	bl = (bl) & 0xffffffff;
        asm volatile ("getcon   " __c17 ", %0" : "=r" (ch));
        asm volatile ("getcon   " __c17 ", %0" : "=r" (cl));
	ch = (ch) >> 32;
	cl = (cl) & 0xffffffff;
	printk("SR  : %08Lx%08Lx TEA : %08Lx%08Lx KCR0: %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[0]) >> 32;
	al = (regs->regs[0]) & 0xffffffff;
	bh = (regs->regs[1]) >> 32;
	bl = (regs->regs[1]) & 0xffffffff;
	ch = (regs->regs[2]) >> 32;
	cl = (regs->regs[2]) & 0xffffffff;
	printk("R0  : %08Lx%08Lx R1  : %08Lx%08Lx R2  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[3]) >> 32;
	al = (regs->regs[3]) & 0xffffffff;
	bh = (regs->regs[4]) >> 32;
	bl = (regs->regs[4]) & 0xffffffff;
	ch = (regs->regs[5]) >> 32;
	cl = (regs->regs[5]) & 0xffffffff;
	printk("R3  : %08Lx%08Lx R4  : %08Lx%08Lx R5  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[6]) >> 32;
	al = (regs->regs[6]) & 0xffffffff;
	bh = (regs->regs[7]) >> 32;
	bl = (regs->regs[7]) & 0xffffffff;
	ch = (regs->regs[8]) >> 32;
	cl = (regs->regs[8]) & 0xffffffff;
	printk("R6  : %08Lx%08Lx R7  : %08Lx%08Lx R8  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[9]) >> 32;
	al = (regs->regs[9]) & 0xffffffff;
	bh = (regs->regs[10]) >> 32;
	bl = (regs->regs[10]) & 0xffffffff;
	ch = (regs->regs[11]) >> 32;
	cl = (regs->regs[11]) & 0xffffffff;
	printk("R9  : %08Lx%08Lx R10 : %08Lx%08Lx R11 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[12]) >> 32;
	al = (regs->regs[12]) & 0xffffffff;
	bh = (regs->regs[13]) >> 32;
	bl = (regs->regs[13]) & 0xffffffff;
	ch = (regs->regs[14]) >> 32;
	cl = (regs->regs[14]) & 0xffffffff;
	printk("R12 : %08Lx%08Lx R13 : %08Lx%08Lx R14 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[16]) >> 32;
	al = (regs->regs[16]) & 0xffffffff;
	bh = (regs->regs[17]) >> 32;
	bl = (regs->regs[17]) & 0xffffffff;
	ch = (regs->regs[19]) >> 32;
	cl = (regs->regs[19]) & 0xffffffff;
	printk("R16 : %08Lx%08Lx R17 : %08Lx%08Lx R19 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[20]) >> 32;
	al = (regs->regs[20]) & 0xffffffff;
	bh = (regs->regs[21]) >> 32;
	bl = (regs->regs[21]) & 0xffffffff;
	ch = (regs->regs[22]) >> 32;
	cl = (regs->regs[22]) & 0xffffffff;
	printk("R20 : %08Lx%08Lx R21 : %08Lx%08Lx R22 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[23]) >> 32;
	al = (regs->regs[23]) & 0xffffffff;
	bh = (regs->regs[24]) >> 32;
	bl = (regs->regs[24]) & 0xffffffff;
	ch = (regs->regs[25]) >> 32;
	cl = (regs->regs[25]) & 0xffffffff;
	printk("R23 : %08Lx%08Lx R24 : %08Lx%08Lx R25 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[26]) >> 32;
	al = (regs->regs[26]) & 0xffffffff;
	bh = (regs->regs[27]) >> 32;
	bl = (regs->regs[27]) & 0xffffffff;
	ch = (regs->regs[28]) >> 32;
	cl = (regs->regs[28]) & 0xffffffff;
	printk("R26 : %08Lx%08Lx R27 : %08Lx%08Lx R28 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[29]) >> 32;
	al = (regs->regs[29]) & 0xffffffff;
	bh = (regs->regs[30]) >> 32;
	bl = (regs->regs[30]) & 0xffffffff;
	ch = (regs->regs[31]) >> 32;
	cl = (regs->regs[31]) & 0xffffffff;
	printk("R29 : %08Lx%08Lx R30 : %08Lx%08Lx R31 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[32]) >> 32;
	al = (regs->regs[32]) & 0xffffffff;
	bh = (regs->regs[33]) >> 32;
	bl = (regs->regs[33]) & 0xffffffff;
	ch = (regs->regs[34]) >> 32;
	cl = (regs->regs[34]) & 0xffffffff;
	printk("R32 : %08Lx%08Lx R33 : %08Lx%08Lx R34 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[35]) >> 32;
	al = (regs->regs[35]) & 0xffffffff;
	bh = (regs->regs[36]) >> 32;
	bl = (regs->regs[36]) & 0xffffffff;
	ch = (regs->regs[37]) >> 32;
	cl = (regs->regs[37]) & 0xffffffff;
	printk("R35 : %08Lx%08Lx R36 : %08Lx%08Lx R37 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[38]) >> 32;
	al = (regs->regs[38]) & 0xffffffff;
	bh = (regs->regs[39]) >> 32;
	bl = (regs->regs[39]) & 0xffffffff;
	ch = (regs->regs[40]) >> 32;
	cl = (regs->regs[40]) & 0xffffffff;
	printk("R38 : %08Lx%08Lx R39 : %08Lx%08Lx R40 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[41]) >> 32;
	al = (regs->regs[41]) & 0xffffffff;
	bh = (regs->regs[42]) >> 32;
	bl = (regs->regs[42]) & 0xffffffff;
	ch = (regs->regs[43]) >> 32;
	cl = (regs->regs[43]) & 0xffffffff;
	printk("R41 : %08Lx%08Lx R42 : %08Lx%08Lx R43 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[44]) >> 32;
	al = (regs->regs[44]) & 0xffffffff;
	bh = (regs->regs[45]) >> 32;
	bl = (regs->regs[45]) & 0xffffffff;
	ch = (regs->regs[46]) >> 32;
	cl = (regs->regs[46]) & 0xffffffff;
	printk("R44 : %08Lx%08Lx R45 : %08Lx%08Lx R46 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[47]) >> 32;
	al = (regs->regs[47]) & 0xffffffff;
	bh = (regs->regs[48]) >> 32;
	bl = (regs->regs[48]) & 0xffffffff;
	ch = (regs->regs[49]) >> 32;
	cl = (regs->regs[49]) & 0xffffffff;
	printk("R47 : %08Lx%08Lx R48 : %08Lx%08Lx R49 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[50]) >> 32;
	al = (regs->regs[50]) & 0xffffffff;
	bh = (regs->regs[51]) >> 32;
	bl = (regs->regs[51]) & 0xffffffff;
	ch = (regs->regs[52]) >> 32;
	cl = (regs->regs[52]) & 0xffffffff;
	printk("R50 : %08Lx%08Lx R51 : %08Lx%08Lx R52 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[53]) >> 32;
	al = (regs->regs[53]) & 0xffffffff;
	bh = (regs->regs[54]) >> 32;
	bl = (regs->regs[54]) & 0xffffffff;
	ch = (regs->regs[55]) >> 32;
	cl = (regs->regs[55]) & 0xffffffff;
	printk("R53 : %08Lx%08Lx R54 : %08Lx%08Lx R55 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[56]) >> 32;
	al = (regs->regs[56]) & 0xffffffff;
	bh = (regs->regs[57]) >> 32;
	bl = (regs->regs[57]) & 0xffffffff;
	ch = (regs->regs[58]) >> 32;
	cl = (regs->regs[58]) & 0xffffffff;
	printk("R56 : %08Lx%08Lx R57 : %08Lx%08Lx R58 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[59]) >> 32;
	al = (regs->regs[59]) & 0xffffffff;
	bh = (regs->regs[60]) >> 32;
	bl = (regs->regs[60]) & 0xffffffff;
	ch = (regs->regs[61]) >> 32;
	cl = (regs->regs[61]) & 0xffffffff;
	printk("R59 : %08Lx%08Lx R60 : %08Lx%08Lx R61 : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->regs[62]) >> 32;
	al = (regs->regs[62]) & 0xffffffff;
	bh = (regs->tregs[0]) >> 32;
	bl = (regs->tregs[0]) & 0xffffffff;
	ch = (regs->tregs[1]) >> 32;
	cl = (regs->tregs[1]) & 0xffffffff;
	printk("R62 : %08Lx%08Lx T0  : %08Lx%08Lx T1  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->tregs[2]) >> 32;
	al = (regs->tregs[2]) & 0xffffffff;
	bh = (regs->tregs[3]) >> 32;
	bl = (regs->tregs[3]) & 0xffffffff;
	ch = (regs->tregs[4]) >> 32;
	cl = (regs->tregs[4]) & 0xffffffff;
	printk("T2  : %08Lx%08Lx T3  : %08Lx%08Lx T4  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	ah = (regs->tregs[5]) >> 32;
	al = (regs->tregs[5]) & 0xffffffff;
	bh = (regs->tregs[6]) >> 32;
	bl = (regs->tregs[6]) & 0xffffffff;
	ch = (regs->tregs[7]) >> 32;
	cl = (regs->tregs[7]) & 0xffffffff;
	printk("T5  : %08Lx%08Lx T6  : %08Lx%08Lx T7  : %08Lx%08Lx\n",
	       ah, al, bh, bl, ch, cl);

	/*
	 * If we're in kernel mode, dump the stack too..
	 */
	if (!user_mode(regs)) {
		extern void show_task(unsigned long *sp);
		unsigned long sp = regs->regs[15] & 0xffffffff;

		show_task((unsigned long *)sp);
	}
}

struct task_struct * alloc_task_struct(void)
{
	/* Get task descriptor pages */
	return (struct task_struct *)
		__get_free_pages(GFP_KERNEL, get_order(THREAD_SIZE));
}

void free_task_struct(struct task_struct *p)
{
	free_pages((unsigned long) p, get_order(THREAD_SIZE));
}

/*
 * Create a kernel thread
 */

/*
 * This is the mechanism for creating a new kernel thread.
 *
 * NOTE! Only a kernel-only process(ie the swapper or direct descendants
 * who haven't done an "execve()") should use this: it will work within
 * a system call from a "real" process, but the process memory space will
 * not be free'd until both the parent and the child have exited.
 */
int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	/* A bit less processor dependent than older sh ... */

	unsigned int reply;

static __inline__ _syscall2(int,clone,unsigned long,flags,unsigned long,newsp)
static __inline__ _syscall1(int,exit,int,ret)

	reply = clone(flags | CLONE_VM, 0);
	if (!reply) {
		/* Child */
		reply = exit(fn(arg));
	}

	return reply;
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	/* See arch/sparc/kernel/process.c for the precedent for doing this -- RPC.
	
	   The SH-5 FPU save/restore approach relies on last_task_used_math
	   pointing to a live task_struct.  When another task tries to use the
	   FPU for the 1st time, the FPUDIS trap handling (see
	   arch/sh64/kernel/fpu.c) will save the existing FPU state to the
	   FP regs field within last_task_used_math before re-loading the new
	   task's FPU state (or initialising it if the FPU has been used
	   before).  So if last_task_used_math is stale, and its page has already been
	   re-allocated for another use, the consequences are rather grim. Unless we
	   null it here, there is no other path through which it would get safely
	   nulled. */

#ifndef CONFIG_NOFPU_SUPPORT
	if (last_task_used_math == current) {
		last_task_used_math = NULL;
	}
#endif
}

void flush_thread(void)
{

	/* As far as I can tell, this function isn't actually called from anywhere.
	   So why does it have a non-null body for most architectures?? -- RPC */
	/* Look closer, this is used in fs/exec.c by flush_old_exec() which is
	   used by binfmt_elf and friends to remove leftover traces of the
	   previously running executable. -- PFM */
#ifndef CONFIG_NOFPU_SUPPORT
	if (last_task_used_math == current) {
		last_task_used_math = NULL;
	}
#endif

	/* if we are a kernel thread, about to change to user thread,
         * update kreg 
         */
	if(current->thread.kregs==&fake_swapper_regs) {
          current->thread.kregs= 
             ((struct pt_regs *)(THREAD_SIZE + (unsigned long) current) - 1);
	}
}

void release_thread(struct task_struct *dead_task)
{
	/* do nothing */
}

/* Fill in the fpu structure for a core dump.. */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
#ifndef CONFIG_NOFPU_SUPPORT
	int fpvalid;
	struct task_struct *tsk = current;

	fpvalid = tsk->used_math;
	if (fpvalid) {
		if (current == last_task_used_math) {
			grab_fpu();
			fpsave(&tsk->thread.fpu.hard);
			release_fpu();
			last_task_used_math = 0;
			regs->sr |= SR_FD;
		}
		
		memcpy(fpu, &tsk->thread.fpu.hard, sizeof(*fpu));
	}

	return fpvalid;
#else
	return 0; /* Task didn't use the fpu at all. */
#endif
}

asmlinkage void ret_from_fork(void);

int copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
	unsigned long long se;			/* Sign extension */
#ifndef CONFIG_NOFPU_SUPPORT
	if(last_task_used_math == current) {		
		grab_fpu();
		fpsave(&current->thread.fpu.hard);
		release_fpu();
		last_task_used_math = NULL;
		regs->sr |= SR_FD;
	}
#endif
	childregs = ((struct pt_regs *)(THREAD_SIZE + (unsigned long) p)) - 1;
	*childregs = *regs;

	if (user_mode(regs)) {
		childregs->regs[15] = usp;
		p->thread.kregs = childregs;
	} else {
		childregs->regs[15] = (unsigned long)p+THREAD_SIZE;
		p->thread.kregs = &fake_swapper_regs;
	}

	childregs->regs[9] = 0; /* Set return value for child */
	childregs->sr |= SR_FD; /* Invalidate FPU flag */

	p->thread.sp = (unsigned long) childregs;
	p->thread.pc = (unsigned long) ret_from_fork;

	/*
	 * Sign extend the edited stack.
         * Note that thread.pc and thread.pc will stay
	 * 32-bit wide and context switch must take care
	 * of NEFF sign extension.
	 */
      
	se = childregs->regs[15];
	se = (se & NEFF_SIGN) ? (se | NEFF_MASK) : se;
	childregs->regs[15] = se;

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	dump->magic = CMAGIC;
	dump->start_code = current->mm->start_code;
	dump->start_data  = current->mm->start_data;
	dump->start_stack = regs->regs[15] & ~(PAGE_SIZE - 1);
	dump->u_tsize = (current->mm->end_code - dump->start_code) >> PAGE_SHIFT;
	dump->u_dsize = (current->mm->brk + (PAGE_SIZE-1) - dump->start_data) >> PAGE_SHIFT;
	dump->u_ssize = (current->mm->start_stack - dump->start_stack +
			 PAGE_SIZE - 1) >> PAGE_SHIFT;
	/* Debug registers will come here. */

	dump->regs = *regs;

	dump->u_fpvalid = dump_fpu(regs, &dump->fpu);
}

/*
 *	switch_to(x,y) should switch tasks from x to y.
 *
 */
struct task_struct * __switch_to(struct task_struct *prev, struct task_struct *next)
{
	/*
	 * Restore the kernel mode register
	 *   	KCR0 =  __c17
	 */
	asm volatile("putcon	%0, " __c17 "\n"
		     : /* no output */
		     :"r" (next));
	return prev;

}

asmlinkage int sys_fork(unsigned long r2, unsigned long r3,
			unsigned long r4, unsigned long r5,
			unsigned long r6, unsigned long r7,
			struct pt_regs *pregs)
{
	return do_fork(SIGCHLD, pregs->regs[15], pregs,0);
}

asmlinkage int sys_clone(unsigned long clone_flags, unsigned long newsp,
			 unsigned long r4, unsigned long r5,
			 unsigned long r6, unsigned long r7,
			 struct pt_regs *pregs)
{
	if (!newsp)
		newsp = pregs->regs[15];
	return do_fork(clone_flags, newsp, pregs,0);
}

/*
 * This is trivial, and on the face of it looks like it
 * could equally well be done in user mode.
 *
 * Not so, for quite unobvious reasons - register pressure.
 * In user mode vfork() cannot have a stack frame, and if
 * done by calling the "clone()" system call directly, you
 * do not have enough call-clobbered registers to hold all
 * the information you need.
 */
asmlinkage int sys_vfork(unsigned long r2, unsigned long r3,
			 unsigned long r4, unsigned long r5,
			 unsigned long r6, unsigned long r7,
			 struct pt_regs *pregs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, pregs->regs[15], pregs,0);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(char *ufilename, char **uargv,
			  char **uenvp, unsigned long r5,
			  unsigned long r6, unsigned long r7,
			  struct pt_regs *pregs)
{
	int error;
	char *filename;

	lock_kernel();
	filename = getname(ufilename);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	error = do_execve(filename, uargv, uenvp, pregs);
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
out:
	unlock_kernel();
	return error;
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
extern void interruptible_sleep_on(wait_queue_head_t *q);

#define first_sched	((unsigned long) scheduling_functions_start_here)
#define mid_sched	((unsigned long) interruptible_sleep_on)
#define last_sched	((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long schedule_frame;
	unsigned long pc;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	/*
	 * The same comment as on the Alpha applies here, too ...
	 */
	pc = thread_saved_pc(&p->thread);

	if (pc >= first_sched && pc < last_sched) {

		schedule_frame = (long) p->thread.sp;

		/* Should we unwind schedule_timeout() ? */
		if (pc < mid_sched)
			/* according to disasm:
			**     48 bytes in case of RH toolchain
		        */
			schedule_frame += 48;
			
		/*
		** Unwind schedule(). According to disasm:
		**    72 bytes in case of RH toolchain
		** plus 304 bytes of switch_to additional frame.
		*/
		schedule_frame += 72 + 304;

#ifdef CS_SAVE_ALL
		schedule_frame += 256;
#endif
		/*
		 * schedule_frame now according to SLEEP_ON_VAR.
	 	 * Bad thing is that we have no trace of the waiting
		 * address (the classical WCHAN). SLEEP_ON_VAR should
		 * have saved q. From the linked list only we can't get
		 * the object and first parameter is not saved on stack
		 * by the ABI. The best we can tell is who called the
		 * *sleep_on* by returning LINK, which is saved at
		 * offset 64 on all flavours.
		 */
		return (unsigned long)((unsigned long *)schedule_frame)[16];
	}
	return pc;
}

/* Provide a /proc/asids file that lists out the
   ASIDs currently associated with the processes.  (If the DM.PC register is
   examined through the debug link, this shows ASID + PC.  To make use of this,
   the PID->ASID relationship needs to be known.  This is primarily for
   debugging.)
   */

#if defined(CONFIG_SH64_PROC_ASIDS)
#include <linux/init.h>
#include <linux/proc_fs.h>

static int
asids_proc_info(char *buf, char **start, off_t fpos, int length, int *eof, void *data)
{
	int len=0;
	struct task_struct *p;
	read_lock(&tasklist_lock);
	for_each_task(p) {
		int pid = p->pid;
		struct mm_struct *mm;
		if (!pid) continue;
		mm = p->mm;
		if (mm) {
			unsigned long asid, context;
			context = mm->context;
			asid = (context & 0xff);
			len += sprintf(buf+len, "%5d : %02x\n", pid, asid);
		} else {
			len += sprintf(buf+len, "%5d : (none)\n", pid);
		}
	}
	read_unlock(&tasklist_lock);
	*eof = 1;
	return len;
}

static int __init register_proc_asids(void)
{
  create_proc_read_entry("asids", 0, NULL, asids_proc_info, NULL);
  return 0;
}

__initcall(register_proc_asids);
#endif

