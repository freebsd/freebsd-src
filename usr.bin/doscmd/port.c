/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI port.c,v 2.2 1996/04/08 19:33:03 bostic Exp
 *
 * $Id: port.c,v 1.2 1996/09/22 05:53:08 miff Exp $
 */

#include "doscmd.h"

#define	MINPORT		0x000
#define	MAXPORT_MASK	(MAXPORT - 1)

#include <sys/ioctl.h>
#include <machine/sysarch.h>
static int consfd = -1;

#define in(port) \
({ \
        register int _inb_result; \
\
        asm volatile ("xorl %%eax,%%eax; inb %%dx,%%al" : \
            "=a" (_inb_result) : "d" (port)); \
        _inb_result; \
})

#define out(port, data) \
        asm volatile ("outb %%al,%%dx" : : "a" (data), "d" (port))

FILE *iolog = 0;
u_long ioports[MAXPORT/32];
#ifdef __FreeBSD__
static void
iomap(int port, int cnt)
{
    fatal("iomap not supported");
}

static void
iounmap(int port, int cnt)
{
    fatal("iomap not supported");
}
    
#else
static void
iomap(int port, int cnt)
{

    if (port + cnt >= MAXPORT) {
	errno = ERANGE;
	goto bad;
    }
    while (cnt--) {
	ioports[port/32] |= (1 << (port%32));
	port++;
    }
    if (i386_set_ioperm(ioports) < 0) {
    bad:
	perror("iomap");
	quit(1);
    }
}

static void
iounmap(int port, int cnt)
{

    if (port + cnt >= MAXPORT) {
	errno = ERANGE;
	goto bad;
    }
    while (cnt--) {
	ioports[port/32] &= ~(1 << (port%32));
	port++;
    }
    if (i386_set_ioperm(ioports) < 0) {
    bad:
	perror("iounmap");
	quit(1);
    }
}
#endif
void
outb_traceport(int port, unsigned char byte)
{
/*
    if (!iolog && !(iolog = fopen("/tmp/iolog", "a")))
	iolog = stderr;

    fprintf(iolog, "0x%03X -> %02X\n", port, byte);
 */

    iomap(port, 1);
    out(port, byte);
    iounmap(port, 1);
}

unsigned char
inb_traceport(int port)
{
    unsigned char byte;

/*
    if (!iolog && !(iolog = fopen("/tmp/iolog", "a")))
	iolog = stderr;
 */

    iomap(port, 1);
    byte = in(port);
    iounmap(port, 1);

/*
    fprintf(iolog, "0x%03X <- %02X\n", port, byte);
    fflush(iolog);
 */
    return(byte);
}

/* 
 * Fake input/output ports
 */

static void
outb_nullport(int port, unsigned char byte)
{
/*
    debug(D_PORT, "outb_nullport called for port 0x%03X = 0x%02X.\n",
		   port, byte);
 */
}

static unsigned char
inb_nullport(int port)
{
/*
    debug(D_PORT, "inb_nullport called for port 0x%03X.\n", port);
 */
    return(0xff);
}

/*
 * configuration table for ports' emulators
 */

struct portsw {
	unsigned char	(*p_inb)(int port);
	void		(*p_outb)(int port, unsigned char byte);
} portsw[MAXPORT];

void
init_io_port_handlers(void)
{
    int i;

    for (i = 0; i < MAXPORT; i++) {
	if (portsw[i].p_inb == 0)
	    portsw[i].p_inb = inb_nullport;
	if (portsw[i].p_outb == 0)
	    portsw[i].p_outb = outb_nullport;
    }

}

void
define_input_port_handler(int port, unsigned char (*p_inb)(int port))
{
	if ((port >= MINPORT) && (port < MAXPORT)) {
		portsw[port].p_inb = p_inb;
	} else
		fprintf (stderr, "attempt to handle invalid port 0x%04x", port);
}

void
define_output_port_handler(int port, void (*p_outb)(int port, unsigned char byte))
{
	if ((port >= MINPORT) && (port < MAXPORT)) {
		portsw[port].p_outb = p_outb;
	} else
		fprintf (stderr, "attempt to handle invalid port 0x%04x", port);
}


void
inb(regcontext_t *REGS, int port)
{
	unsigned char (*in_handler)(int);

	if ((port >= MINPORT) && (port < MAXPORT))
		in_handler = portsw[port].p_inb;
	else
		in_handler = inb_nullport;
	R_AL = (*in_handler)(port);
	debug(D_PORT, "IN  on port %02x -> %02x\n", port, R_AL);
}

void
insb(regcontext_t *REGS, int port)
{
	unsigned char (*in_handler)(int);
	unsigned char data;

	if ((port >= MINPORT) && (port < MAXPORT))
		in_handler = portsw[port].p_inb;
	else
		in_handler = inb_nullport;
	data = (*in_handler)(port);
	*(u_char *)N_GETPTR(R_ES, R_DI) = data;
	debug(D_PORT, "INS on port %02x -> %02x\n", port, data);

	if (R_FLAGS & PSL_D)
	    R_DI--;
	else
	    R_DI++;
}

void
inx(regcontext_t *REGS, int port)
{
	unsigned char (*in_handler)(int);

	if ((port >= MINPORT) && (port < MAXPORT))
		in_handler = portsw[port].p_inb;
	else
		in_handler = inb_nullport;
	R_AL =  (*in_handler)(port);
	if ((port >= MINPORT) && (port < MAXPORT))
		in_handler = portsw[port + 1].p_inb;
	else
		in_handler = inb_nullport;
	R_AH = (*in_handler)(port + 1);
	debug(D_PORT, "IN  on port %02x -> %04x\n", port, R_AX);
}

void
insx(regcontext_t *REGS, int port)
{
	unsigned char (*in_handler)(int);
	unsigned char data;

	if ((port >= MINPORT) && (port < MAXPORT))
		in_handler = portsw[port].p_inb;
	else
		in_handler = inb_nullport;
	data = (*in_handler)(port);
	*(u_char *)N_GETPTR(R_ES, R_DI) = data;
	debug(D_PORT, "INS on port %02x -> %02x\n", port, data);

	if ((port >= MINPORT) && (port < MAXPORT))
		in_handler = portsw[port + 1].p_inb;
	else
		in_handler = inb_nullport;
	data = (*in_handler)(port + 1);
	((u_char *)N_GETPTR(R_ES, R_DI))[1] = data;
	debug(D_PORT, "INS on port %02x -> %02x\n", port, data);

	if (R_FLAGS & PSL_D)
	    R_DI -= 2;
	else
	    R_DI += 2;
}

void
outb(regcontext_t *REGS, int port)
{
	void (*out_handler)(int, unsigned char);

	if ((port >= MINPORT) && (port < MAXPORT))
		out_handler = portsw[port].p_outb;
	else
		out_handler = outb_nullport;
	(*out_handler)(port, R_AL);
	debug(D_PORT, "OUT on port %02x <- %02x\n", port, R_AL);
/*
  if (port == 0x3bc && R_AL == 0x55)
    tmode = 1;
*/
}

void
outx(regcontext_t *REGS, int port)
{
	void (*out_handler)(int, unsigned char);

	if ((port >= MINPORT) && (port < MAXPORT))
		out_handler = portsw[port].p_outb;
	else
		out_handler = outb_nullport;
	(*out_handler)(port, R_AL);
	debug(D_PORT, "OUT on port %02x <- %02x\n", port, R_AL);
	if ((port >= MINPORT) && (port < MAXPORT))
		out_handler = portsw[port + 1].p_outb;
	else
		out_handler = outb_nullport;
	(*out_handler)(port + 1, R_AH);
	debug(D_PORT, "OUT on port %02x <- %02x\n", port + 1, R_AH);
}

void
outsb(regcontext_t *REGS, int port)
{
	void (*out_handler)(int, unsigned char);
	unsigned char value;

	if ((port >= MINPORT) && (port < MAXPORT))
		out_handler = portsw[port].p_outb;
	else
		out_handler = outb_nullport;
	value = *(u_char *)N_GETPTR(R_ES, R_DI);
	debug(D_PORT, "OUT on port %02x <- %02x\n", port, value);
	(*out_handler)(port, value);

	if (R_FLAGS & PSL_D)
	    R_DI--;
	else
	    R_DI++;
}

void
outsx(regcontext_t *REGS, int port)
{
	void (*out_handler)(int, unsigned char);
	unsigned char value;

	if ((port >= MINPORT) && (port < MAXPORT))
		out_handler = portsw[port].p_outb;
	else
		out_handler = outb_nullport;
	value = *(u_char *)N_GETPTR(R_ES, R_DI);
	debug(D_PORT, "OUT on port %02x <- %02x\n", port, value);
	(*out_handler)(port, value);

	if ((port >= MINPORT) && (port < MAXPORT))
		out_handler = portsw[port + 1].p_outb;
	else
		out_handler = outb_nullport;
	value = ((u_char *)N_GETPTR(R_ES, R_DI))[1];
	debug(D_PORT, "OUT on port %02x <- %02x\n", port+1, value);
	(*out_handler)(port + 1, value);

	if (R_FLAGS & PSL_D)
	    R_DI -= 2;
	else
	    R_DI += 2;
}

unsigned char port_61 = 0x10;
int sound_on = 1;
int sound_freq = 1000;

void
outb_speaker(int port, unsigned char byte)
{
#if 0 /*XXXXX*/
    if (raw_kbd) {
	if ((port_61 & 3) != 3) {
	    if ((byte & 3) == 3 && /* prtim[2].gate && */ sound_on)
		ioctl(kbd_fd, PCCONIOCSTARTBEEP, &sound_freq);
	} else if ((byte & 3) != 3)
	    ioctl(kbd_fd, PCCONIOCSTOPBEEP);
    }
#endif
    port_61 = byte; 
}

unsigned char
inb_speaker(int port)
{
/*    port_61 = (port_61 + 1) & 0xff; */
    return(port_61);
}

void
speaker_init()
{
    define_input_port_handler(0x61, inb_speaker);
    define_output_port_handler(0x61, outb_speaker);

}
