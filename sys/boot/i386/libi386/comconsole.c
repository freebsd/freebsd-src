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
 *	$Id$
 */

#include <stand.h>

#include "bootstrap.h"

/* in comconsole.S */
extern void	cominit(int s);
extern void	computc(int c);
extern int	comgetc(void);
extern int	comiskey(void);

static void	comc_probe(struct console *cp);
static int	comc_init(int arg);
static int	comc_in(void);

struct console comconsole = {
    "comconsole",
    "BIOS serial port",
    0,
    comc_probe,
    comc_init,
    computc,
    comc_in,
    comiskey
};

static void
comc_probe(struct console *cp)
{
    /* XXX check the BIOS equipment list? */
    cp->c_flags |= (C_PRESENTIN | C_PRESENTOUT);
}

static int
comc_init(int arg)
{
    /* XXX arg is unit number, should we use variables instead? */
    cominit(arg);
    return(0);
}

static int
comc_in(void)
{
    if (comiskey()) {
	return(comgetc());
    } else {
	return(-1);
    }
}
