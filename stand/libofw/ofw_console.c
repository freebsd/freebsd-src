/* $NetBSD: prom.c,v 1.3 1997/09/06 14:03:58 drochner Exp $ */

/*-
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/types.h>

#include "bootstrap.h"
#include "openfirm.h"

static void ofw_cons_probe(struct console *cp);
static int ofw_cons_init(int);
void ofw_cons_putchar(int);
int ofw_cons_getchar(void);
int ofw_cons_poll(void);

static ihandle_t stdin;
static ihandle_t stdout;

struct console ofwconsole = {
	.c_name = "ofw",
	.c_desc = "Open Firmware console",
	.c_probe = ofw_cons_probe,
	.c_init = ofw_cons_init,
	.c_out = ofw_cons_putchar,
	.c_in = ofw_cons_getchar,
	.c_ready = ofw_cons_poll,
};

static void
ofw_cons_probe(struct console *cp)
{

	OF_getprop(chosen, "stdin", &stdin, sizeof(stdin));
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	cp->c_flags |= C_PRESENTIN|C_PRESENTOUT;
}

static int
ofw_cons_init(int arg)
{
	return (0);
}

void
ofw_cons_putchar(int c)
{
	char cbuf;

	if (c == '\n') {
		cbuf = '\r';
		OF_write(stdout, &cbuf, 1);
	}

	cbuf = c;
	OF_write(stdout, &cbuf, 1);
}

static int saved_char = -1;

int
ofw_cons_getchar(void)
{
	unsigned char ch = '\0';
	int l;

	if (saved_char != -1) {
		l = saved_char;
		saved_char = -1;
		return (l);
	}

	/* At least since version 4.0.0, QEMU became bug-compatible
	 * with PowerVM's vty, by inserting a \0 after every \r.
	 * As this confuses loader's interpreter and as a \0 coming
	 * from the console doesn't seem reasonable, it's filtered here. */
	if (OF_read(stdin, &ch, 1) > 0 && ch != '\0')
		return (ch);

	return (-1);
}

int
ofw_cons_poll(void)
{
	unsigned char ch;

	if (saved_char != -1)
		return (1);

	if (OF_read(stdin, &ch, 1) > 0) {
		saved_char = ch;
		return (1);
	}

	return (0);
}
