/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/pc98/libpc98/comconsole.c,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <stand.h>
#include <bootstrap.h>
#include <machine/cpufunc.h>
#include <dev/ic/ns16550.h>
#include "libi386.h"

#define COMC_FMT	0x3		/* 8N1 */
#define COMC_TXWAIT	0x40000		/* transmit timeout */
#define COMC_BPS(x)	(115200 / (x))	/* speed to DLAB divisor */
#define COMC_DIV2BPS(x)	(115200 / (x))	/* DLAB divisor to speed */

#ifndef	COMPORT
#define COMPORT		0x238
#endif
#ifndef	COMSPEED
#define COMSPEED	9600
#endif

static void	comc_probe(struct console *cp);
static int	comc_init(int arg);
static void	comc_putchar(int c);
static int	comc_getchar(void);
static int	comc_getspeed(void);
static int	comc_ischar(void);
static int	comc_parsespeed(const char *string);
static void	comc_setup(int speed);
static int	comc_speed_set(struct env_var *ev, int flags,
		    const void *value);

static int	comc_started;
static int	comc_curspeed;

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
    char speedbuf[16];
    char *cons, *speedenv;
    int speed;

    /* XXX check the BIOS equipment list? */
    cp->c_flags |= (C_PRESENTIN | C_PRESENTOUT);

    if (comc_curspeed == 0) {
	comc_curspeed = COMSPEED;
	/*
	 * Assume that the speed was set by an earlier boot loader if
	 * comconsole is already the preferred console.
	 */
	cons = getenv("console");
	if ((cons != NULL && strcmp(cons, comconsole.c_name) == 0) ||
	    getenv("boot_multicons") != NULL) {
		comc_curspeed = comc_getspeed();
	}
	speedenv = getenv("comconsole_speed");
	if (speedenv != NULL) {
	    speed = comc_parsespeed(speedenv);
	    if (speed > 0)
		comc_curspeed = speed;
	}

	sprintf(speedbuf, "%d", comc_curspeed);
	unsetenv("comconsole_speed");
	env_setenv("comconsole_speed", EV_VOLATILE, speedbuf, comc_speed_set,
	    env_nounset);
    }
}

static int
comc_init(int arg)
{
    if (comc_started && arg == 0)
	return 0;
    comc_started = 1;

    comc_setup(comc_curspeed);

    return(0);
}

static void
comc_putchar(int c)
{
    int wait;

    for (wait = COMC_TXWAIT; wait > 0; wait--)
        if (inb(COMPORT + com_lsr) & LSR_TXRDY) {
	    outb(COMPORT + com_data, (u_char)c);
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

static int
comc_speed_set(struct env_var *ev, int flags, const void *value)
{
    int speed;

    if (value == NULL || (speed = comc_parsespeed(value)) <= 0) {
	printf("Invalid speed\n");
	return (CMD_ERROR);
    }

    if (comc_started && comc_curspeed != speed)
	comc_setup(speed);

    env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);

    return (CMD_OK);
}

static void
comc_setup(int speed)
{

    comc_curspeed = speed;

    outb(COMPORT + com_cfcr, CFCR_DLAB | COMC_FMT);
    outb(COMPORT + com_dlbl, COMC_BPS(speed) & 0xff);
    outb(COMPORT + com_dlbh, COMC_BPS(speed) >> 8);
    outb(COMPORT + com_cfcr, COMC_FMT);
    outb(COMPORT + com_mcr, MCR_RTS | MCR_DTR);

    do
        inb(COMPORT + com_data);
    while (inb(COMPORT + com_lsr) & LSR_RXRDY);
}

static int
comc_parsespeed(const char *speedstr)
{
    char *p;
    int speed;

    speed = strtol(speedstr, &p, 0);
    if (p == speedstr || *p != '\0' || speed <= 0)
	return (-1);

    return (speed);
}

static int
comc_getspeed(void)
{
	u_int	divisor;
	u_char	dlbh;
	u_char	dlbl;
	u_char	cfcr;

	cfcr = inb(COMPORT + com_cfcr);
	outb(COMPORT + com_cfcr, CFCR_DLAB | cfcr);

	dlbl = inb(COMPORT + com_dlbl);
	dlbh = inb(COMPORT + com_dlbh);

	outb(COMPORT + com_cfcr, cfcr);

	divisor = dlbh << 8 | dlbl;

	/* XXX there should be more sanity checking. */
	if (divisor == 0)
		return (COMSPEED);
	return (COMC_DIV2BPS(divisor));
}
