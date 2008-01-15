/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/ia64/ski/skiconsole.c,v 1.3 2004/09/24 03:53:50 marcel Exp $");

#include <stand.h>

#include "bootstrap.h"
#include "libski.h"

static void
ski_cons_probe(struct console *cp)
{
	cp->c_flags |= C_PRESENTIN | C_PRESENTOUT;
}

static int
ski_cons_init(int arg)
{
	ssc(0, 0, 0, 0, SSC_CONSOLE_INIT);
	return 0;
}

void
ski_cons_putchar(int c)
{
	ssc(c, 0, 0, 0, SSC_PUTCHAR);
}

static int pollchar = -1;

int
ski_cons_getchar()
{
	int c;

	if (pollchar > 0) {
		c = pollchar;
		pollchar = -1;
		return c;
	}

	do {
		c = ssc(0, 0, 0, 0, SSC_GETCHAR);
	} while (c == 0);

	return c;
}

int
ski_cons_poll()
{
	int c;
	if (pollchar > 0)
		return 1;
	c = ssc(0, 0, 0, 0, SSC_GETCHAR);
	if (!c)
		return 0;
	pollchar = c;
	return 1;
}

struct console ski_console = {
	"ski",
	"ia64 SKI console",
	0,
	ski_cons_probe,
	ski_cons_init,
	ski_cons_putchar,
	ski_cons_getchar,
	ski_cons_poll
};
