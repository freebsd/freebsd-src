/*
 *  linux/arch/parisc/kernel/process.c
 *	based on the work for i386
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/elf.h>
#include <linux/personality.h>

#include <asm/machdep.h>
#include <asm/offset.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/gsc.h>
#include <asm/processor.h>
#include <asm/pdc_chassis.h>

int hlt_counter;

/*
 * Power off function, if any
 */ 
void (*pm_power_off)(void);

void disable_hlt(void)
{
	hlt_counter++;
}

void enable_hlt(void)
{
	hlt_counter--;
}

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;

	while (1) {
		while (!current->need_resched) {
		}
		schedule();
		check_pgt_cache();
	}
}


#ifdef __LP64__
#define COMMAND_GLOBAL  0xfffffffffffe0030UL
#else
#define COMMAND_GLOBAL  0xfffe0030
#endif

#define CMD_RESET       5       /* reset any module */

/*
** The Wright Brothers and Gecko systems have a H/W problem
** (Lasi...'nuf said) may cause a broadcast reset to lockup
** the system. An HVERSION dependent PDC call was developed
** to perform a "safe", platform specific broadcast reset instead
** of kludging up all the code.
**
** Older machines which do not implement PDC_BROADCAST_RESET will
** return (with an error) and the regular broadcast reset can be
** issued. Obviously, if the PDC does implement PDC_BROADCAST_RESET
** the PDC call will not return (the system will be reset).
*/
void machine_restart(char *cmd)
{
#ifdef FASTBOOT_SELFTEST_SUPPORT
	/*
	 ** If user has modified the Firmware Selftest Bitmap,
	 ** run the tests specified in the bitmap after the
	 ** system is rebooted w/PDC_DO_RESET.
	 **
	 ** ftc_bitmap = 0x1AUL "Skip destructive memory tests"
	 **
	 ** Using "directed resets" at each processor with the MEM_TOC
	 ** vector cleared will also avoid running destructive
	 ** memory self tests. (Not implemented yet)
	 */
	if (ftc_bitmap) {
		pdc_do_firm_test_reset(ftc_bitmap);
	}
#endif

	/* "Normal" system reset */
	pdc_do_reset();

	/* set up a new led state on systems shipped with a LED State panel */
	pdc_chassis_send_status(PDC_CHASSIS_DIRECT_SHUTDOWN);
	
	/* Nope...box should reset with just CMD_RESET now */
	gsc_writel(CMD_RESET, COMMAND_GLOBAL);

	/* Wait for RESET to lay us to rest. */
	while (1) ;

}

void machine_halt(void)
{
	/*
	** The LED/ChassisCodes are updated by the led_halt()
	** function, called by the reboot notifier chain.
	*/
}


/*
 * This routine is called from sys_reboot to actually turn off the
 * machine 
 */
void machine_power_off(void)
{
	/* If there is a registered power off handler, call it. */
	if(pm_power_off)
		pm_power_off();

	/* Put the soft power button back under hardware control.
	 * If the user had already pressed the power button, the
	 * following call will immediately power off. */
	pdc_soft_power_button(0);

	pdc_chassis_send_status(PDC_CHASSIS_DIRECT_SHUTDOWN);
	
	/* It seems we have no way to power the system off via
	 * software. The user has to press the button himself. */

	printk(KERN_EMERG "System shut down completed.\n"
	       KERN_EMERG "Please power this system off now.");
}


/*
 * Create a kernel thread
 */

extern pid_t __kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
pid_t arch_kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{

	/*
	 * FIXME: Once we are sure we don't need any debug here,
	 *	  kernel_thread can become a #define.
	 */

	return __kernel_thread(fn, arg, flags);
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
}

void flush_thread(void)
{
	/* Only needs to handle fpu stuff or perf monitors.
	** REVISIT: several arches implement a "lazy fpu state".
	*/
	set_fs(USER_DS);
}

void release_thread(struct task_struct *dead_task)
{
}

/*
 * Fill in the FPU structure for a core dump.
 */

int dump_fpu (struct pt_regs * regs, elf_fpregset_t *r)
{
	memcpy(r, regs->fr, sizeof *r);
	return 1;
}

/* Note that "fork()" is implemented in terms of clone, with
   parameters (SIGCHLD, regs->gr[30], regs). */
int
sys_clone(unsigned long clone_flags, unsigned long usp,
	  struct pt_regs *regs)
{
	return do_fork(clone_flags, usp, regs, 0);
}

int
sys_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD,
		       regs->gr[30], regs, 0);
}

int
copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
	    unsigned long unused,	/* in ia64 this is "user_stack_size" */
	    struct task_struct * p, struct pt_regs * pregs)
{
	struct pt_regs * cregs = &(p->thread.regs);
	
	/* We have to use void * instead of a function pointer, because
	 * function pointers aren't a pointer to the function on 64-bit.
	 * Make them const so the compiler knows they live in .text */
	extern void * const ret_from_kernel_thread;
	extern void * const child_return;
	extern void * const hpux_child_return;

	*cregs = *pregs;

	/* Set the return value for the child.  Note that this is not
           actually restored by the syscall exit path, but we put it
           here for consistency in case of signals. */
	cregs->gr[28] = 0; /* child */

	/*
	 * We need to differentiate between a user fork and a
	 * kernel fork. We can't use user_mode, because the
	 * the syscall path doesn't save iaoq. Right now
	 * We rely on the fact that kernel_thread passes
	 * in zero for usp.
	 */
	if (usp == 0) {
		/* kernel thread */
		cregs->ksp = (((unsigned long)(p)) + TASK_SZ_ALGN);
		/* Must exit via ret_from_kernel_thread in order
		 * to call schedule_tail()
		 */
		cregs->kpc = (unsigned long) &ret_from_kernel_thread;
		/*
		 * Copy function and argument to be called from
		 * ret_from_kernel_thread.
		 */
#ifdef __LP64__
		cregs->gr[27] = pregs->gr[27];
#endif
		cregs->gr[26] = pregs->gr[26];
		cregs->gr[25] = pregs->gr[25];
	} else {
		/* user thread */
		/*
		 * Note that the fork wrappers are responsible
		 * for setting gr[21].
		 */

		/* Use same stack depth as parent */
		cregs->ksp = ((unsigned long)(p))
			+ (pregs->gr[21] & (INIT_TASK_SIZE - 1));
		cregs->gr[30] = usp;
		if (p->personality == PER_HPUX) {
			cregs->kpc = (unsigned long) &hpux_child_return;
		} else {
			cregs->kpc = (unsigned long) &child_return;
		}
	}

	return 0;
}

/*
 * sys_execve() executes a new program.
 */

asmlinkage int sys_execve(struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname((char *) regs->gr[26]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char **) regs->gr[25],
		(char **) regs->gr[24], regs);
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
out:

	return error;
}
