/* $FreeBSD: src/sys/boot/arc/lib/arcconsole.c,v 1.2 1999/08/28 00:39:36 peter Exp $ */
/* $NetBSD: prom.c,v 1.3 1997/09/06 14:03:58 drochner Exp $ */

/*  
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
#include "arctypes.h"
#include "arcfuncs.h"

int console;

static void arc_probe(struct console *cp);
static int arc_init(int);
static void arc_putchar(int);
static int arc_getchar(void);
static int arc_poll(void);

struct console arcconsole = {
    "arc",
    "ARC firmware console",
    0,
    arc_probe,
    arc_init,
    arc_putchar,
    arc_getchar,
    arc_poll,
};

static void
arc_probe(struct console *cp)
{
    cp->c_flags |= C_PRESENTIN|C_PRESENTOUT;
}

static int
arc_init(int arg)
{
    return 0;
}

static void
arc_putchar(int c)
{
    char cbuf = c;
    u_int32_t count;

    Write(StandardOut, &cbuf, 1, &count);
}

static int saved_char = -1;

int
arc_getchar()
{
    char cbuf;
    u_int32_t count;

    arc_putchar('_');
    arc_putchar('\b');
    Read(StandardIn, &cbuf, 1, &count);
    arc_putchar(' ');
    arc_putchar('\b');
    if (count == 1)
	return cbuf;
    else
	return -1;
}

int
arc_poll()
{
    return GetReadStatus(StandardIn) == ESUCCESS;
}

int
arc_open(dev, len)
    char *dev;
    int len;
{
    return 0;
}
