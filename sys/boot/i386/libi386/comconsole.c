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
 * $FreeBSD: src/sys/boot/i386/libi386/comconsole.c,v 1.7 1999/08/28 00:40:14 peter Exp $
 */

#include <stand.h>
#include <bootstrap.h>
#include <machine/cpufunc.h>
#include "libi386.h"

/* selected defines from ns16550.h */
#define	com_data	0	/* data register (R/W) */
#define	com_dlbl	0	/* divisor latch low (W) */
#define	com_dlbh	1	/* divisor latch high (W) */
#define	com_ier		1	/* interrupt enable (W) */
#define	com_iir		2	/* interrupt identification (R) */
#define	com_fifo	2	/* FIFO control (W) */
#define	com_lctl	3	/* line control register (R/W) */
#define	com_cfcr	3	/* line control register (R/W) */
#define	com_mcr		4	/* modem control register (R/W) */
#define	com_lsr		5	/* line status register (R/W) */
#define	com_msr		6	/* modem status register (R/W) */

/* selected defines from sioreg.h */
#define	CFCR_DLAB	0x80
#define	MCR_RTS		0x02
#define	MCR_DTR		0x01
#define	LSR_TXRDY	0x20
#define	LSR_RXRDY	0x01

#define COMC_FMT	0x3		/* 8N1 */
#define COMC_TXWAIT	0x40000		/* transmit timeout */
#define COMC_BPS(x)	(115200 / (x))	/* speed to DLAB divisor */

#ifndef	COMPORT
#define COMPORT		0x3f8
#endif
#ifndef	COMSPEED
#define COMSPEED	9600
#endif

static void	comc_probe(struct console *cp);
static int	comc_init(int arg);
static void	comc_putchar(int c);
static int	comc_getchar(void);
static int	comc_ischar(void);

static int	comc_started;

struct console comconsole = {
    "comconsole",
    "serial port",
    0,
    comc_probe,
    comc_init,
    comc_putchar,
    comc_getchar,
    comc_ischar
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
    if (comc_started && arg == 0)
	return 0;
    comc_started = 1;

    outb(COMPORT + com_cfcr, CFCR_DLAB | COMC_FMT);
    outb(COMPORT + com_dlbl, COMC_BPS(COMSPEED) & 0xff);
    outb(COMPORT + com_dlbh, COMC_BPS(COMSPEED) >> 8);
    outb(COMPORT + com_cfcr, COMC_FMT);
    outb(COMPORT + com_mcr, MCR_RTS | MCR_DTR);

    do
        inb(COMPORT + com_data);
    while (inb(COMPORT + com_lsr) & LSR_RXRDY);

    return(0);
}

static void
comc_putchar(int c)
{
    int wait;

    for (wait = COMC_TXWAIT; wait > 0; wait--)
        if (inb(COMPORT + com_lsr) & LSR_TXRDY) {
	    outb(COMPORT + com_data, c);
	    break;
	}
}

static int
comc_getchar(void)
{
    return(comc_ischar() ? inb(COMPORT + com_data) : -1);
}

static int
comc_ischar(void)
{
    return(inb(COMPORT + com_lsr) & LSR_RXRDY);
}
