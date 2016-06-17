/*
 *  linux/arch/cris/traps.c
 *
 *  Here we handle the break vectors not used by the system call
 *  mechanism, as well as some general stack/register dumping
 *  things.
 *
 *  Copyright (C) 2000, 2001, 2002, 2003 Axis Communications AB
 *
 *  Authors:   Bjorn Wesen
 *  	       Hans-Peter Nilsson
 *
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/pgtable.h>

int kstack_depth_to_print = 24;

void show_trace(unsigned long * stack)
{
	unsigned long addr, module_start, module_end;
	extern char _stext, _etext;
	int i;

        printk("\nCall Trace: ");

        i = 1;
        module_start = VMALLOC_START;
        module_end = VMALLOC_END;

        while (((long) stack & (THREAD_SIZE-1)) != 0) {
		if (__get_user (addr, stack)) {
			/* This message matches "failing address" marked
			   s390 in ksymoops, so lines containing it will
			   not be filtered out by ksymoops.  */
			printk ("Failing address 0x%lx\n", (unsigned long)stack);
			break;
		}
		stack++;

                /*
                 * If the address is either in the text segment of the
                 * kernel, or in the region which contains vmalloc'ed
                 * memory, it *may* be the address of a calling
                 * routine; if so, print it so that someone tracing
                 * down the cause of the crash will be able to figure
                 * out the call path that was taken.
                 */
                if (((addr >= (unsigned long) &_stext) &&
                     (addr <= (unsigned long) &_etext)) ||
                    ((addr >= module_start) && (addr <= module_end))) {
                        if (i && ((i % 8) == 0))
                                printk("\n       ");
                        printk("[<%08lx>] ", addr);
                        i++;
                }
        }
}

void show_trace_task(struct task_struct *tsk)
{
	/* TODO, this is not really useful since its called from
	 * SysRq-T and we don't have a keyboard.. :) 
	 */
}


/*
 * These constants are for searching for possible module text
 * segments. MODULE_RANGE is a guess of how much space is likely
 * to be vmalloced.
 */

#define MODULE_RANGE (8*1024*1024)

/*
 * The output (format, strings and order) is adjusted to be usable with
 * ksymoops-2.4.1 with some necessary CRIS-specific patches.  Please don't
 * change it unless you're serious about adjusting ksymoops and syncing
 * with the ksymoops maintainer.
 */

void 
show_stack(unsigned long *sp)
{
        unsigned long *stack, addr;
        int i;

	/*
	 * debugging aid: "show_stack(NULL);" prints a
	 * back trace.
	 */

        if(sp == NULL)
                sp = (unsigned long*)rdsp();

        stack = sp;

	printk("\nStack from %08lx:\n       ", (unsigned long)stack);
        for(i = 0; i < kstack_depth_to_print; i++) {
                if (((long) stack & (THREAD_SIZE-1)) == 0)
                        break;
                if (i && ((i % 8) == 0))
                        printk("\n       ");
		if (__get_user (addr, stack)) {
			/* This message matches "failing address" marked
			   s390 in ksymoops, so lines containing it will
			   not be filtered out by ksymoops.  */
			printk ("Failing address 0x%lx\n", (unsigned long)stack);
			break;
		}
		stack++;
		printk("%08lx ", addr);
        }
	show_trace(sp);
}

#if 0
/* displays a short stack trace */

int 
show_stack()
{
	unsigned long *sp = (unsigned long *)rdusp();
	int i;
	printk("Stack dump [0x%08lx]:\n", (unsigned long)sp);
	for(i = 0; i < 16; i++)
		printk("sp + %d: 0x%08lx\n", i*4, sp[i]);
	return 0;
}
#endif

void 
show_registers(struct pt_regs * regs)
{
	/* We either use rdusp() - the USP register, which might not
	   correspond to the current process for all cases we're called,
	   or we use the current->thread.usp, which is not up to date for
	   the current process.  Experience shows we want the USP
	   register.  */
	unsigned long usp = rdusp();

	printk("IRP: %08lx SRP: %08lx DCCR: %08lx USP: %08lx MOF: %08lx\n",
	       regs->irp, regs->srp, regs->dccr, usp, regs->mof );
	printk(" r0: %08lx  r1: %08lx   r2: %08lx  r3: %08lx\n",
	       regs->r0, regs->r1, regs->r2, regs->r3);
	printk(" r4: %08lx  r5: %08lx   r6: %08lx  r7: %08lx\n",
	       regs->r4, regs->r5, regs->r6, regs->r7);
	printk(" r8: %08lx  r9: %08lx  r10: %08lx r11: %08lx\n",
	       regs->r8, regs->r9, regs->r10, regs->r11);
	printk("r12: %08lx r13: %08lx oR10: %08lx\n",
	       regs->r12, regs->r13, regs->orig_r10);
	printk("R_MMU_CAUSE: %08lx\n", (unsigned long)*R_MMU_CAUSE);
	printk("Process %s (pid: %d, stackpage=%08lx)\n",
	       current->comm, current->pid, (unsigned long)current);

	/*
         * When in-kernel, we also print out the stack and code at the
         * time of the fault..
         */
        if (! user_mode(regs)) {
	  	int i;

                show_stack((unsigned long*)usp);

		/* Dump kernel stack if the previous dump wasn't one.  */
		if (usp != 0)
			show_stack (NULL);

                printk("\nCode: ");
                if(regs->irp < PAGE_OFFSET)
                        goto bad;

		/* Often enough the value at regs->irp does not point to
		   the interesting instruction, which is most often the
		   _previous_ instruction.  So we dump at an offset large
		   enough that instruction decoding should be in sync at
		   the interesting point, but small enough to fit on a row
		   (sort of).  We point out the regs->irp location in a
		   ksymoops-friendly way by wrapping the byte for that
		   address in parentheses.  */
                for(i = -12; i < 12; i++)
                {
                        unsigned char c;
                        if(__get_user(c, &((unsigned char*)regs->irp)[i])) {
bad:
                                printk(" Bad IP value.");
                                break;
                        }

			if (i == 0)
			  printk("(%02x) ", c);
			else
			  printk("%02x ", c);
                }
		printk("\n");
        }
}

/* Called from entry.S when the watchdog has bitten
 * We print out something resembling an oops dump, and if
 * we have the nice doggy development flag set, we halt here
 * instead of rebooting.
 */
extern void reset_watchdog(void);
extern void stop_watchdog(void);

void
watchdog_bite_hook(struct pt_regs *regs)
{
#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	cli();
	stop_watchdog();
	show_registers(regs);
	while(1) /* nothing */;
#else
	show_registers(regs);
#endif	
}

void dump_stack(void)
{
	show_stack(NULL);
}

/* This is normally the 'Oops' routine */
void 
die_if_kernel(const char * str, struct pt_regs * regs, long err)
{
	if(user_mode(regs))
		return;

#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	/* This printout might take too long and trigger the 
	 * watchdog normally. If we're in the nice doggy
	 * development mode, stop the watchdog during printout.
	 */
	stop_watchdog();
#endif

	printk("%s: %04lx\n", str, err & 0xffff);

	show_registers(regs);

#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	reset_watchdog();
#endif
	do_exit(SIGSEGV);
}

void __init 
trap_init(void)
{
	/* Nothing needs to be done */
}
