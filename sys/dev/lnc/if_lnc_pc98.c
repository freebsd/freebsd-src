/*-
 * Copyright (c) 2000
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifdef PC98
#include "lnc.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <machine/clock.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <i386/isa/isa_device.h>

#include <dev/lnc/if_lncvar.h>
#include <dev/lnc/if_lncreg.h>

int pcnet_probe __P((lnc_softc_t *sc));
static int cnet98s_probe __P((lnc_softc_t *sc, unsigned iobase));

/* C-NET(98)S port addresses */
#define CNET98S_RDP    0x400     /* Register Data Port */
#define CNET98S_RAP    0x402     /* Register Address Port */
#define CNET98S_RESET  0x404
#define CNET98S_IDP    0x406
#define CNET98S_EEPROM 0x40e
/*
 * XXX - The I/O address range is fragmented in the C-NET(98)S.
 *       This is the number of regs at iobase.
 */
#define CNET98S_IOSIZE    16     /* # of i/o addresses used. */

/* ISA Bus Configuration Registers */
/* XXX - Should be in ic/Am7990.h */
#define	MSRDA	0x0000	/* ISACSR0: Master Mode Read Activity */
#define	MSWRA	0x0001	/* ISACSR1: Master Mode Write Activity */
#define	MC	0x0002	/* ISACSR2: Miscellaneous Configuration */

#define	LED1	0x0005	/* ISACSR5: LED1 Status */
#define	LED2	0x0006	/* ISACSR6: LED2 Status */
#define	LED3	0x0007	/* ISACSR7: LED3 Status */

#define	LED_PSE		0x0080	/* Pulse Stretcher */
#define	LED_XMTE	0x0010	/* Transmit Status */
#define	LED_RVPOLE	0x0008	/* Receive Polarity */
#define	LED_RCVE	0x0004	/* Receive Status */
#define	LED_JABE	0x0002	/* Jabber */
#define	LED_COLE	0x0001	/* Collision */

static int
cnet98s_probe(lnc_softc_t *sc, unsigned iobase)
{
	int i;
	ushort tmp;

	sc->rap = iobase + CNET98S_RAP;
	sc->rdp = iobase + CNET98S_RDP;

	/* Reset */
	tmp = inw(iobase + CNET98S_RESET);
	outw(iobase + CNET98S_RESET, tmp);
	DELAY(500);

	sc->nic.ic = pcnet_probe(sc);
	if ((sc->nic.ic == UNKNOWN) || (sc->nic.ic > PCnet_32)) {
		return (0);
	}

	sc->nic.ident = CNET98S;
	sc->nic.mem_mode = DMA_FIXED;

	/* XXX - For now just use the defines */
	sc->nrdre = NRDRE;
	sc->ntdre = NTDRE;

	/* Extract MAC address from PROM */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sc->arpcom.ac_enaddr[i] = inb(iobase + (i * 2));
	}

	/*
	 * ISA Configuration
	 *
	 * XXX - Following parameters are Contec C-NET(98)S only.
	 *       So, check the Ethernet address here.
	 *
	 *       Contec uses 00 80 4c ?? ?? ??
	 */ 
	if (sc->arpcom.ac_enaddr[0] == (u_char)0x00
	&&  sc->arpcom.ac_enaddr[1] == (u_char)0x80
	&&  sc->arpcom.ac_enaddr[2] == (u_char)0x4c) {
        	outw(sc->rap, MSRDA);
        	outw(iobase + CNET98S_IDP, 0x0006);
        	outw(sc->rap, MSWRA);
        	outw(iobase + CNET98S_IDP, 0x0006);
#ifdef DIAGNOSTIC
        	outw(sc->rap, MC);
		printf("ISACSR2 = %x\n", inw(iobase + CNET98S_IDP));
#endif
        	outw(sc->rap, LED1);
        	outw(iobase + CNET98S_IDP, LED_PSE | LED_XMTE);
        	outw(sc->rap, LED2);
        	outw(iobase + CNET98S_IDP, LED_PSE | LED_RCVE);
        	outw(sc->rap, LED3);
        	outw(iobase + CNET98S_IDP, LED_PSE | LED_COLE);
	}
		
	return (CNET98S_IOSIZE);
}
#endif
