/* $Id: power.c,v 1.9.2.1 2001/12/11 01:57:49 davem Exp $
 * power.c: Power management driver.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/delay.h>

#include <asm/ebus.h>
#include <asm/auxio.h>

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#ifdef CONFIG_PCI
static unsigned long power_reg = 0UL;

static DECLARE_WAIT_QUEUE_HEAD(powerd_wait);
static int button_pressed;

static void power_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if (button_pressed == 0) {
		wake_up(&powerd_wait);
		button_pressed = 1;
	}
}
#endif /* CONFIG_PCI */

extern void machine_halt(void);
extern void machine_alt_power_off(void);
static void (*poweroff_method)(void) = machine_alt_power_off;

extern int serial_console;

void machine_power_off(void)
{
	if (!serial_console) {
#ifdef CONFIG_PCI
		if (power_reg != 0UL) {
			/* Both register bits seem to have the
			 * same effect, so until I figure out
			 * what the difference is...
			 */
			writel(AUXIO_PCIO_CPWR_OFF | AUXIO_PCIO_SPWR_OFF, power_reg);
		} else
#endif /* CONFIG_PCI */
			if (poweroff_method != NULL) {
				poweroff_method();
				/* not reached */
			}
	}
	machine_halt();
}

#ifdef CONFIG_PCI
static int powerd(void *__unused)
{
	static char *envp[] = { "HOME=/", "TERM=linux", "PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };
	char *argv[] = { "/sbin/shutdown", "-h", "now", NULL };

	daemonize();
	sprintf(current->comm, "powerd");

again:
	while (button_pressed == 0) {
		spin_lock_irq(&current->sigmask_lock);
		flush_signals(current);
		spin_unlock_irq(&current->sigmask_lock);
		interruptible_sleep_on(&powerd_wait);
	}

	/* Ok, down we go... */
	if (execve("/sbin/shutdown", argv, envp) < 0) {
		printk("powerd: shutdown execution failed\n");
		button_pressed = 0;
		goto again;
	}
	return 0;
}

static int __init has_button_interrupt(struct linux_ebus_device *edev)
{
	if (edev->irqs[0] == PCI_IRQ_NONE)
		return 0;
	if (!prom_node_has_property(edev->prom_node, "button"))
		return 0;

	return 1;
}

void __init power_init(void)
{
	struct linux_ebus *ebus;
	struct linux_ebus_device *edev;
	static int invoked;

	if (invoked)
		return;
	invoked = 1;

	for_each_ebus(ebus) {
		for_each_ebusdev(edev, ebus) {
			if (!strcmp(edev->prom_name, "power"))
				goto found;
		}
	}
	return;

found:
	power_reg = (unsigned long)ioremap(edev->resource[0].start, 0x4);
	printk("power: Control reg at %016lx ... ", power_reg);
	poweroff_method = machine_halt; /* able to use the standard poweroff */
	if (has_button_interrupt(edev)) {
		if (kernel_thread(powerd, 0, CLONE_FS) < 0) {
			printk("Failed to start power daemon.\n");
			return;
		}
		printk("powerd running.\n");

		if (request_irq(edev->irqs[0],
				power_handler, SA_SHIRQ, "power",
				(void *) power_reg) < 0)
			printk("power: Error, cannot register IRQ handler.\n");
	} else {
		printk("not using powerd.\n");
	}
}
#endif /* CONFIG_PCI */
