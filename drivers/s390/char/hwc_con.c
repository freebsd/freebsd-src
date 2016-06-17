/*
 *  drivers/s390/char/hwc_con.c
 *    HWC line mode console driver
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Peschke <mpeschke@de.ibm.com>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/kdev_t.h>
#include <linux/string.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/init.h>

#include "hwc_rw.h"

#ifdef CONFIG_HWC_CONSOLE

#define  hwc_console_major 4
#define  hwc_console_minor 64
#define  hwc_console_name  "console"

void hwc_console_write (struct console *, const char *, unsigned int);
kdev_t hwc_console_device (struct console *);
void hwc_console_unblank (void);

#define  HWC_CON_PRINT_HEADER "hwc console driver: "

struct console hwc_console = {
	name:	hwc_console_name,
	write:	hwc_console_write,
	device:	hwc_console_device,
	unblank:hwc_console_unblank,
	flags:	CON_PRINTBUFFER,
};

void 
hwc_console_write (
			  struct console *console,
			  const char *message,
			  unsigned int count)
{

	if (console->device (console) != hwc_console.device (&hwc_console)) {

		hwc_printk (KERN_WARNING HWC_CON_PRINT_HEADER
			    "hwc_console_write() called with wrong "
			    "device number");
		return;
	}
	hwc_write (0, message, count);
}

kdev_t 
hwc_console_device (struct console * c)
{
	return MKDEV (hwc_console_major, hwc_console_minor);
}

void 
hwc_console_unblank (void)
{
	hwc_unblank ();
}

#endif

void __init 
hwc_console_init (void)
{
	if (!MACHINE_HAS_HWC)
		return;

	if (hwc_init () == 0) {
#ifdef CONFIG_HWC_CONSOLE

		if (CONSOLE_IS_HWC)
			register_console (&hwc_console);
#endif
	} else
		panic (HWC_CON_PRINT_HEADER "hwc initialisation failed !");

	return;
}
