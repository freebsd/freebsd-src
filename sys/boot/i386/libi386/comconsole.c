/*
 * Copyright (c) 1998 Michael Smith (msmith@freebsd.org)
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
 *
 * 	From Id: probe_keyboard.c,v 1.13 1997/06/09 05:10:55 bde Exp
 *
 *	$Id: comconsole.c,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
 */

#include <stand.h>
#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

static void	comc_probe(struct console *cp);
static int	comc_init(int arg);
static void	comc_putchar(int c);
static int	comc_getchar(void);
static int	comc_ischar(void);

struct console comconsole = {
    "comconsole",
    "BIOS serial port",
    0,
    comc_probe,
    comc_init,
    comc_putchar,
    comc_getchar,
    comc_ischar
};

#define BIOS_COMPORT	0

static void
comc_probe(struct console *cp)
{
    /* XXX check the BIOS equipment list? */
    cp->c_flags |= (C_PRESENTIN | C_PRESENTOUT);
}

static int
comc_init(int arg)
{
    v86.ctl = V86_FLAGS;
    v86.addr = 0x14;
    v86.eax = 0xe3;		/* 9600N81 */
    v86.edx = BIOS_COMPORT;	/* XXX take as arg, or use env var? */
    v86int();
    return(v86.efl & 1);
}

static void
comc_putchar(int c)
{
    v86.ctl = 0;
    v86.addr = 0x14;
    v86.eax = 0x100 | c;
    v86int();
}

static int
comc_getchar(void)
{
    if (comc_ischar()) {
	v86.ctl = 0;
	v86.addr = 0x14;
	v86.eax = 0x300;
	v86int();
	return(v86.eax);
    } else {
	return(-1);
    }
}

static int
comc_ischar(void)
{
    v86.ctl = 0;
    v86.addr = 0x14;
    v86.eax = 0x200;
    v86int();
    return(v86.eax & 0x1);
}
