/*
 *  linux/kernel/panic.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * This function is used through-out the kernel (including mm and fs)
 * to indicate a major problem.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/interrupt.h>
#include <linux/console.h>

asmlinkage void sys_sync(void);	/* it's really int */

int panic_timeout;

struct notifier_block *panic_notifier_list;

static int __init panic_setup(char *str)
{
	panic_timeout = simple_strtoul(str, NULL, 0);
	return 1;
}

__setup("panic=", panic_setup);

int machine_paniced; 

/**
 *	panic - halt the system
 *	@fmt: The text string to print
 *
 *	Display a message, then perform cleanups. Functions in the panic
 *	notifier list are called after the filesystem cache is flushed (when possible).
 *
 *	This function never returns.
 */
 
NORET_TYPE void panic(const char * fmt, ...)
{
	static char buf[1024];
	va_list args;
#if defined(CONFIG_ARCH_S390)
        unsigned long caller = (unsigned long) __builtin_return_address(0);
#endif

#ifdef CONFIG_VT
	disable_console_blank();
#endif
	machine_paniced = 1;
	
	bust_spinlocks(1);
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	printk(KERN_EMERG "Kernel panic: %s\n",buf);
	if (in_interrupt())
		printk(KERN_EMERG "In interrupt handler - not syncing\n");
	else if (!current->pid)
		printk(KERN_EMERG "In idle task - not syncing\n");
	else
		sys_sync();
	bust_spinlocks(0);

#ifdef CONFIG_SMP
	smp_send_stop();
#endif

	notifier_call_chain(&panic_notifier_list, 0, NULL);

	if (panic_timeout > 0)
	{
		/*
	 	 * Delay timeout seconds before rebooting the machine. 
		 * We can't use the "normal" timers since we just panicked..
	 	 */
		printk(KERN_EMERG "Rebooting in %d seconds..",panic_timeout);
		mdelay(panic_timeout*1000);
		/*
		 *	Should we run the reboot notifier. For the moment Im
		 *	choosing not too. It might crash, be corrupt or do
		 *	more harm than good for other reasons.
		 */
		machine_restart(NULL);
	}
#ifdef __sparc__
	{
		extern int stop_a_enabled;
		/* Make sure the user can actually press L1-A */
		stop_a_enabled = 1;
		printk("Press L1-A to return to the boot prom\n");
	}
#endif
#if defined(CONFIG_ARCH_S390)
        disabled_wait(caller);
#endif
	sti();
	for(;;) {
#if defined(CONFIG_X86) && defined(CONFIG_VT) 
		extern void panic_blink(void);
		panic_blink(); 
#endif
		CHECK_EMERGENCY_SYNC
	}
}

/**
 *	print_tainted - return a string to represent the kernel taint state.
 *
 *	The string is overwritten by the next call to print_taint().
 */
 
const char *print_tainted()
{
	static char buf[20];
	if (tainted) {
		snprintf(buf, sizeof(buf), "Tainted: %c%c",
			tainted & 1 ? 'P' : 'G',
			tainted & 2 ? 'F' : ' ');
	}
	else
		snprintf(buf, sizeof(buf), "Not tainted");
	return(buf);
}

int tainted = 0;

/*
 * A BUG() call in an inline function in a header should be avoided,
 * because it can seriously bloat the kernel.  So here we have
 * helper functions.
 * We lose the BUG()-time file-and-line info this way, but it's
 * usually not very useful from an inline anyway.  The backtrace
 * tells us what we want to know.
 */

void __out_of_line_bug(int line)
{
	printk("kernel BUG in header file at line %d\n", line);

	BUG();

	/* Satisfy __attribute__((noreturn)) */
	for ( ; ; )
		;
}
