/*
 * Copyright (c) 1998, Michael Smith
 * Copyright (c) 1996, Sujal M. Patel
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
 *
 *      $Id: isapnp.c,v 1.1 1998/09/18 00:24:25 msmith Exp $
 */

/*
 * Machine-independant ISA PnP enumerator implementing a subset of the
 * ISA PnP specification.
 */
#include <stand.h>
#include <string.h>
#include <bootstrap.h>
#include <isapnp.h>

#define inb(x)		(archsw.arch_isainb((x)))
#define outb(x,y)	(archsw.arch_isaoutb((x),(y)))

static void	isapnp_write(int d, u_char r);
static u_char	isapnp_read(int d);
static void	isapnp_send_Initiation_LFSR();
static int	isapnp_get_serial(u_int8_t *p);
static int	isapnp_isolation_protocol(struct pnpinfo **pnplist);
static void	isapnp_enumerate(struct pnpinfo **pnplist);

/* PnP read data port */
static int	pnp_rd_port;

#define _PNP_ID_LEN	9

struct pnphandler isapnphandler =
{
    "isapnp",
    isapnp_enumerate
};

static void
isapnp_write(int d, u_char r)
{
    outb (_PNP_ADDRESS, d);
    outb (_PNP_WRITE_DATA, r);
}

static u_char
isapnp_read(int d)
{
    outb (_PNP_ADDRESS, d);
    return (inb(3 | (pnp_rd_port <<2)));
}

/*
 * Send Initiation LFSR as described in "Plug and Play ISA Specification",
 * Intel May 94.
 */
static void
isapnp_send_Initiation_LFSR()
{
    int cur, i;

    /* Reset the LSFR */
    outb(_PNP_ADDRESS, 0);
    outb(_PNP_ADDRESS, 0); /* yes, we do need it twice! */

    cur = 0x6a;
    outb(_PNP_ADDRESS, cur);

    for (i = 1; i < 32; i++) {
	cur = (cur >> 1) | (((cur ^ (cur >> 1)) << 7) & 0xff);
	outb(_PNP_ADDRESS, cur);
    }
}

/*
 * Get the device's serial number.  Returns 1 if the serial is valid.
 */
static int
isapnp_get_serial(u_int8_t *data)
{
    int		i, bit, valid = 0, sum = 0x6a;

    bzero(data, _PNP_ID_LEN);
    outb(_PNP_ADDRESS, SERIAL_ISOLATION);
    for (i = 0; i < 72; i++) {
	bit = inb((pnp_rd_port << 2) | 0x3) == 0x55;
	delay(250);	/* Delay 250 usec */

	/* Can't Short Circuit the next evaluation, so 'and' is last */
	bit = (inb((pnp_rd_port << 2) | 0x3) == 0xaa) && bit;
	delay(250);	/* Delay 250 usec */

	valid = valid || bit;

	if (i < 64)
	    sum = (sum >> 1) |
		(((sum ^ (sum >> 1) ^ bit) << 7) & 0xff);

	data[i / 8] = (data[i / 8] >> 1) | (bit ? 0x80 : 0);
    }

    valid = valid && (data[8] == sum);

    return valid;
}

/*
 * Format a pnp id as a string in standard ISA PnP format, AAAIIRR
 * where 'AAA' is the EISA ID, II is the product ID and RR the revision ID.
 */
static char *
isapnp_format(u_int8_t *data)
{
    static char	idbuf[8];
    const char	hextoascii[] = "0123456789abcdef";

    idbuf[0] = '@' + ((data[0] & 0x7c) >> 2);
    idbuf[1] = '@' + (((data[0] & 0x3) << 3) + ((data[1] & 0xe0) >> 5));
    idbuf[2] = '@' + (data[1] & 0x1f);
    idbuf[3] = hextoascii[(data[2] >> 4)];
    idbuf[4] = hextoascii[(data[2] & 0xf)];
    idbuf[5] = hextoascii[(data[3] >> 4)];
    idbuf[6] = hextoascii[(data[3] & 0xf)];
    idbuf[7] = 0;
}

/*
 * Try to read a compatible device ID from the current device, return
 * 1 if we found one.
 */
#define READ_RSC(c)	{while ((isapnp_read(STATUS) & 1) == 0); (c) = isapnp_read(RESOURCE_DATA);}
static int
isapnp_getid(u_int8_t *data)
{
    int		discard, pos, len;
    u_int8_t	c, t;
    
    discard = 0;
    len = 0;
    pos = 0;
    
    for (;;) {
	READ_RSC(c);
	/* skipping junk? */
	if (discard > 0) {
	    discard--;
	    continue;
	}
	/* copying data? */
	if (len > 0) {
	    data[pos++] = c;
	    /* got all data? */
	    if (pos >= len)
		return(1);
	}
	/* resource type */
	if (c & 0x80) {		/* large resource, throw it away */
	    if (c == 0xff)
		return(0);	/* end of resources */
	    READ_RSC(c);
	    discard = c;
	    READ_RSC(c);
	    discard += ((int)c << 8);
	    continue;
	}
	/* small resource */
	t = (c >> 3) & 0xf;
	if (t == 0xf)
	    return(0);		/* end of resources */
	if ((t == LOG_DEVICE_ID) || (t == COMP_DEVICE_ID)) {
	    len = c & 7;
	    pos = 0;
	    continue;
	}
	discard = c & 7;	/* unwanted small resource */
    }
    
}


/*
 * Run the isolation protocol. Use pnp_rd_port as the READ_DATA port
 * value (caller should try multiple READ_DATA locations before giving
 * up). Upon exiting, all cards are aware that they should use
 * pnp_rd_port as the READ_DATA port.
 */
static int
isapnp_isolation_protocol(struct pnpinfo **pilist)
{
    int			csn;
    struct pnpinfo	*pi;
    u_int8_t		cardid[_PNP_ID_LEN];
    int			ndevs;

    isapnp_send_Initiation_LFSR();
    ndevs = 0;
    
    isapnp_write(CONFIG_CONTROL, 0x04);	/* Reset CSN for All Cards */

    for (csn = 1; ; csn++) {
	/* Wake up cards without a CSN (ie. all of them) */
	isapnp_write(WAKE, 0);
	isapnp_write(SET_RD_DATA, pnp_rd_port);
	outb(_PNP_ADDRESS, SERIAL_ISOLATION);
	delay(1000);	/* Delay 1 msec */

	if (isapnp_get_serial(cardid)) {
	    isapnp_write(SET_CSN, csn);
	    pi = malloc(sizeof(struct pnpinfo));
	    pi->pi_next = *pilist;
	    *pilist = pi;
	    ndevs++;
	    /* scan the card obtaining all the identifiers it holds */
	    while (isapnp_getid(cardid)) {
		printf("   %s\n", isapnp_format(cardid));
		pnp_addident(pi, isapnp_format(cardid));
	    }
	} else
	    break;
    }
    /* Move all cards to wait-for-key state */
    while (csn >= 0) {
	isapnp_send_Initiation_LFSR();
	isapnp_write(WAKE, csn);
	isapnp_write(CONFIG_CONTROL, 0x02);
	delay(1000); /* XXX is it really necessary ? */
    }
    return(ndevs);
}

/*
 * Locate ISA-PnP devices and populate the supplied list.
 */
static void
isapnp_enumerate(struct pnpinfo **pnplist) 
{
    int			devs;

    for (pnp_rd_port = 0x80; pnp_rd_port < 0xff; pnp_rd_port += 0x10) {
	
	/* Look for something, quit when we find it */
	if ((devs = isapnp_isolation_protocol(pnplist)) > 0)
	    break;
    }
    printf("Found %d ISA PnP devices\n", devs);
}


