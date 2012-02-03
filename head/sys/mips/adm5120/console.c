/* $NetBSD: uart.c,v 1.2 2007/03/23 20:05:47 dogcow Exp $ */

/*-
 * Copyright (c) 2007 Ruslan Ermilov and Vsevolod Lobko.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>

#include <sys/cons.h>
#include <sys/consio.h>

static cn_probe_t	uart_cnprobe;
static cn_init_t	uart_cninit;
static cn_term_t	uart_cnterm;
static cn_getc_t	uart_cngetc;
static cn_putc_t	uart_cnputc;
static cn_grab_t	uart_cngrab;
static cn_ungrab_t	uart_cnungrab;

static void
uart_cnprobe(struct consdev *cp)
{

        sprintf(cp->cn_name, "uart");
        cp->cn_pri = CN_NORMAL;
}

static void
uart_cninit(struct consdev *cp)
{

}


void
uart_cnputc(struct consdev *cp, int c)
{
	char chr;

	chr = c;
	while ((*((volatile unsigned long *)0xb2600018)) & 0x20) ;
	(*((volatile unsigned long *)0xb2600000)) = c;
	while ((*((volatile unsigned long *)0xb2600018)) & 0x20) ;
}

int
uart_cngetc(struct consdev * cp)
{

	while ((*((volatile unsigned long *)0xb2600018)) & 0x10) ;
	return (*((volatile unsigned long *)0xb2600000)) & 0xff;
}

static void
uart_cnterm(struct consdev * cp)
{

}

static void
uart_cngrab(struct consdev *cp)
{

}

static void
uart_cnungrab(struct consdev *cp)
{

}

CONSOLE_DRIVER(uart);
