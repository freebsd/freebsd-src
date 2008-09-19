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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#ifdef POWERMAC_SCREEN_HACK
static ihandle_t stdout1;
static int do_stdout1 = 0;
#endif

struct console ofwconsole = {
	"ofw",
	"Open Firmware console",
	0,
	ofw_cons_probe,
	ofw_cons_init,
	ofw_cons_putchar,
	ofw_cons_getchar,
	ofw_cons_poll,
};

static void
ofw_cons_probe(struct console *cp)
{
#ifdef POWERMAC_SCREEN_HACK
	char path1[128], path2[128];
#endif

	OF_getprop(chosen, "stdin", &stdin, sizeof(stdin));
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
#ifdef POWERMAC_SCREEN_HACK
	stdout1 = OF_open("screen");
	if (stdout1 != -1) {
		if (OF_instance_to_path(stdout, path1, sizeof(path1)) == -1)
			path1[0] = '\0';
		if (OF_instance_to_path(stdout1, path2, sizeof(path2)) == -1)
			path2[0] = '\0';
		if (strcmp(path1, path2) == 0) {
			OF_close(stdout1);
		} else {
			do_stdout1 = 1;
		}
	}
#endif
	cp->c_flags |= C_PRESENTIN|C_PRESENTOUT;
}

static int
ofw_cons_init(int arg)
{
	return 0;
}

void
ofw_cons_putchar(int c)
{
	char cbuf;

	if (c == '\n') {
		cbuf = '\r';
		OF_write(stdout, &cbuf, 1);
#ifdef POWERMAC_SCREEN_HACK
		if (do_stdout1 != 0)
			OF_write(stdout1, &cbuf, 1);
#endif
	}

	cbuf = c;
	OF_write(stdout, &cbuf, 1);
#ifdef POWERMAC_SCREEN_HACK
	if (do_stdout1 != 0)
		OF_write(stdout1, &cbuf, 1);
#endif
}

static int saved_char = -1;

int
ofw_cons_getchar()
{
	unsigned char ch = '\0';
	int l;

	if (saved_char != -1) {
		l = saved_char;
		saved_char = -1;
		return l;
	}

	if (OF_read(stdin, &ch, 1) > 0)
		return (ch);

	return (-1);
}

int
ofw_cons_poll()
{
	unsigned char ch;

	if (saved_char != -1)
		return 1;

	if (OF_read(stdin, &ch, 1) > 0) {
		saved_char = ch;
		return 1;
	}

	return 0;
}
