/*
 *  linux/arch/parisc/kernel/pdc_console.c
 *
 *  The PDC console is a simple console, which can be used for debugging 
 *  boot related problems on HP PA-RISC machines.
 *
 *  This code uses the ROM (=PDC) based functions to read and write characters
 *  from and to PDC's boot path.
 *  Since all character read from that path must be polled, this code never
 *  can or will be a fully functional linux console.
 */

/* Define EARLY_BOOTUP_DEBUG to debug kernel related boot problems. 
 * On production kernels EARLY_BOOTUP_DEBUG should be undefined. */
#undef EARLY_BOOTUP_DEBUG


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/system.h>
#include <asm/pdc.h>		/* for iodc_call() proto and friends */


static void pdc_console_write(struct console *co, const char *s, unsigned count)
{
	while(count--)
		pdc_iodc_putc(*s++);
}

void pdc_outc(unsigned char c)
{
	pdc_iodc_outc(c);
}


int pdc_console_poll_key(struct console *co)
{
	return pdc_iodc_getc();
}

static int pdc_console_setup(struct console *co, char *options)
{
	return 0;
}

#if defined(CONFIG_PDC_CONSOLE) || defined(CONFIG_SERIAL_MUX)
#define PDC_CONSOLE_DEVICE pdc_console_device
static kdev_t pdc_console_device (struct console *c)
{
        return MKDEV(MUX_MAJOR, 0);
}
#else 
#define PDC_CONSOLE_DEVICE NULL
#endif

static struct console pdc_cons = {
	name:		"ttyB",
	write:		pdc_console_write,
#warning UPSTREAM 2.4.19 removed the next 4 lines but we did not
	read:		NULL,
	device:		PDC_CONSOLE_DEVICE,
	unblank:	NULL,
	setup:		pdc_console_setup,
	flags:		CON_BOOT|CON_PRINTBUFFER|CON_ENABLED,
	index:		-1,
};

static int pdc_console_initialized;
extern unsigned long con_start;	/* kernel/printk.c */
extern unsigned long log_end;	/* kernel/printk.c */


static void pdc_console_init_force(void)
{
	if (pdc_console_initialized)
		return;
	++pdc_console_initialized;
	
	/* If the console is duplex then copy the COUT parameters to CIN. */
	if (PAGE0->mem_cons.cl_class == CL_DUPLEX)
		memcpy(&PAGE0->mem_kbd, &PAGE0->mem_cons, sizeof(PAGE0->mem_cons));

	/* register the pdc console */
	register_console(&pdc_cons);
}

void pdc_console_init(void)
{
#if defined(EARLY_BOOTUP_DEBUG) || defined(CONFIG_PDC_CONSOLE) || defined(CONFIG_SERIAL_MUX)
	pdc_console_init_force();
#endif
#ifdef EARLY_BOOTUP_DEBUG
	printk(KERN_INFO "Initialized PDC Console for debugging.\n");
#endif
}


/* Unregister the pdc console with the printk console layer */
void pdc_console_die(void)
{
	if (!pdc_console_initialized)
		return;
	--pdc_console_initialized;

	printk(KERN_INFO "Switching from PDC console\n");

	/* Don't repeat what we've already printed */
	con_start = log_end;

	unregister_console(&pdc_cons);
}


/*
 * Used for emergencies. Currently only used if an HPMC occurs. If an
 * HPMC occurs, it is possible that the current console may not be
 * properly initialed after the PDC IO reset. This routine unregisters all
 * of the current consoles, reinitializes the pdc console and
 * registers it.
 */

void pdc_console_restart(void)
{
	struct console *console;

	if (pdc_console_initialized)
		return;

	while ((console = console_drivers) != NULL)
		unregister_console(console_drivers);

	/* Don't repeat what we've already printed */
	con_start = log_end;
	
	/* force registering the pdc console */
	pdc_console_init_force();
}

