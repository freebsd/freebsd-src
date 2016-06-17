#include <linux/init.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/console.h>

#include <asm/sibyte/board.h>

#include "cfe_api.h"
#include "cfe_error.h"

extern int cfe_cons_handle;
static kdev_t cfe_consdev;

static void cfe_console_write(struct console *cons, const char *str,
		       unsigned int count)
{
	int i, last, written;

	for (i=0,last=0; i<count; i++) {
		if (!str[i])
			/* XXXKW can/should this ever happen? */
			return;
		if (str[i] == '\n') {
			do {
				written = cfe_write(cfe_cons_handle, &str[last], i-last);
				if (written < 0)
					;
				last += written;
			} while (last < i);
			while (cfe_write(cfe_cons_handle, "\r", 1) <= 0)
				;
		}
	}
	if (last != count) {
		do {
			written = cfe_write(cfe_cons_handle, &str[last], count-last);
			if (written < 0)
				;
			last += written;
		} while (last < count);
	}
			
}

static kdev_t cfe_console_device(struct console *c)
{
	return cfe_consdev;
}

static int cfe_console_setup(struct console *cons, char *str)
{
	char consdev[32];
	/* XXXKW think about interaction with 'console=' cmdline arg */
	/* If none of the console options are configured, the build will break. */
	if (cfe_getenv("BOOT_CONSOLE", consdev, 32) >= 0) {
#ifdef CONFIG_SIBYTE_SB1250_DUART
		if (!strcmp(consdev, "uart0")) {
			setleds("u0cn");
			cfe_consdev = MKDEV(TTY_MAJOR, SB1250_DUART_MINOR_BASE + 0);
		} else if (!strcmp(consdev, "uart1")) {
			setleds("u1cn");
			cfe_consdev = MKDEV(TTY_MAJOR, SB1250_DUART_MINOR_BASE + 1);
#endif
#ifdef CONFIG_VGA_CONSOLE
		} else if (!strcmp(consdev, "pcconsole0")) {
			setleds("pccn");
			cfe_consdev = MKDEV(TTY_MAJOR, 0);
#endif
		} else
			return -ENODEV;
	}
	return 0;
}

static struct console sb1250_cfe_cons = {
	name:		"cfe",
	write:		cfe_console_write,
	device:		cfe_console_device,
	setup:		cfe_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init sb1250_cfe_console_init(void)
{
	register_console(&sb1250_cfe_cons);
}
