/*	$NetBSD: ingenic_com.c,v 1.4 2015/03/07 15:35:33 macallan Exp $ */

/*-
 * Copyright (c) 2014 Michael Lorenz
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ingenic_com.c,v 1.4 2015/03/07 15:35:33 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/termios.h>
#include <sys/ttydefaults.h>
#include <sys/types.h>

#include <sys/bus.h>

#include <dev/cons.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <mips/cpuregs.h>

#include <mips/ingenic/ingenic_regs.h>

#include "opt_com.h"

volatile int32_t *com0addr = (int32_t *)MIPS_PHYS_TO_KSEG1(JZ_UART0);

void	ingenic_putchar_init(void);
void	ingenic_puts(const char *);
void	ingenic_putchar(char);

#ifndef CONMODE
# define CONMODE ((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8)
#endif


static struct mips_bus_space	ingenic_com_mbst;
static int	mbst_valid = 0;
static void	ingenic_com_bus_mem_init(bus_space_tag_t, void *);
void		ingenic_com_cnattach(void);

static int	ingenic_com_match(device_t, cfdata_t , void *);
static void	ingenic_com_attach(device_t, device_t, void *);

struct ingenic_com_softc {
	struct com_softc sc_com;
};

CFATTACH_DECL_NEW(com_mainbus, sizeof(struct ingenic_com_softc),
    ingenic_com_match, ingenic_com_attach, NULL, NULL);

bus_space_handle_t regh = 0;

void
ingenic_putchar_init(void)
{
	/*
	 * XXX don't screw with the UART's speed until we know what clock
	 * we're on
	 */
#if 0
	int rate;
#endif
	extern int comspeed(long, long, int);

	com0addr = (uint32_t *)MIPS_PHYS_TO_KSEG1(JZ_UART0);
#if 0
	if (comcnfreq != -1) {
		rate = comspeed(comcnspeed, comcnfreq, COM_TYPE_INGENIC);
		if (rate < 0)
			return;					/* XXX */
#endif
		com0addr[com_ier] = 0;
		com0addr[com_lctl] = htole32(LCR_DLAB);
#if 0
		com0addr[com_dlbl] = htole32(rate & 0xff);
		com0addr[com_dlbh] = htole32(rate >> 8);
#endif
		com0addr[com_lctl] = htole32(LCR_8BITS);	/* XXX */
		com0addr[com_mcr]  = htole32(MCR_DTR|MCR_RTS);
		com0addr[com_fifo] = htole32(
		    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | 
		    FIFO_TRIGGER_1 | FIFO_UART_ON);
#if 0
	}
#endif
}


void
ingenic_putchar(char c)
{
	int timo = 150000;

	while ((le32toh(com0addr[com_lsr]) & LSR_TXRDY) == 0)
		if (--timo == 0)
			break;

	com0addr[com_data] = htole32((uint32_t)c);

	while ((le32toh(com0addr[com_lsr]) & LSR_TSRE) == 0)
		if (--timo == 0)
			break;
}

void
ingenic_puts(const char *restrict s)
{
	char c;

	while ((c = *s++) != 0)
		ingenic_putchar(c);
}

static void
ingenic_com_bus_init(void)
{
	if (mbst_valid) return;
	ingenic_com_bus_mem_init(&ingenic_com_mbst, NULL);
	mbst_valid = 1;
}

void
ingenic_com_cnattach(void)
{
	struct com_regs	regs;
	
	ingenic_com_bus_init();
	bus_space_map(&ingenic_com_mbst, 0, 0x1000, 0, &regh);

	memset(&regs, 0, sizeof(regs));
	COM_INIT_REGS(regs, &ingenic_com_mbst, regh, 0);

	/*
	 * XXX
	 * UART clock is either 6MHz or 12MHz, the manual is rather unclear
	 * so we just leave alone whatever u-boot set up
	 * my uplcom is too tolerant to show any difference
	 */
	comcnattach1(&regs, 115200, 48000000, COM_TYPE_INGENIC, CONMODE);
}

static int
ingenic_com_match(device_t parent, cfdata_t cfdata, void *args)
{
	struct mainbusdev {
		const char *md_name;
	} *aa = args;
	if (strcmp(aa->md_name, "com") == 0) return 1;
	return 0;
}


static void
ingenic_com_attach(device_t parent, device_t self, void *args)
{
	struct ingenic_com_softc *isc = device_private(self);
	struct com_softc *sc = &isc->sc_com;

	sc->sc_dev = self;
	sc->sc_frequency = 48000000;
	sc->sc_type = COM_TYPE_INGENIC;
	memset(&sc->sc_regs, 0, sizeof(sc->sc_regs));
	COM_INIT_REGS(sc->sc_regs, &ingenic_com_mbst, regh, 0);
	com_attach_subr(sc);
	printf("\n");
	evbmips_intr_establish(51, comintr, sc);
}

#define CHIP	   		ingenic_com
#define	CHIP_MEM		/* defined */
#define	CHIP_W1_BUS_START(v)	0x00000000UL
#define CHIP_W1_BUS_END(v)	0x00010000UL
#define	CHIP_W1_SYS_START(v)	0x10030000UL
#define	CHIP_W1_SYS_END(v)	0x10035000UL
#define	CHIP_ACCESS_SIZE	1
#define CHIP_ALIGN_STRIDE	2

#include <mips/mips/bus_space_alignstride_chipdep.c>
