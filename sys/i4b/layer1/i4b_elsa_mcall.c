/*
 *   Copyright (c) 1998 Martin Husemann. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	ELSA MicroLink MC/all card specific routines
 *	--------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_elsa_mcall.c,v 1.5 1999/08/28 00:45:38 peter Exp $
 *
 *      last edit-date: [Sun Feb 14 10:26:29 1999]
 *
 *	-mh	started support for ELSA MC/all
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC	1
#endif

#if NISIC > 0 && defined(ELSA_MCALL)

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#ifdef __FreeBSD__
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#endif

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>
#include <i4b/layer1/i4b_ipac.h>

#include <i4b/layer1/pcmcia_isic.h>

#ifndef __FreeBSD__
/* PCMCIA support routines */
static u_int8_t elsa_mcall_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void elsa_mcall_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void elsa_mcall_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void elsa_mcall_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
#endif

/*---------------------------------------------------------------------------*
 *	read fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static int PCMCIA_IO_BASE = 0;		/* ap: XXX hack */
static void		
elsa_mcall_read_fifo(void *buf, const void *base, size_t len)
{
}
#else
static void
elsa_mcall_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	/*
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	*/
}
#endif

/*---------------------------------------------------------------------------*
 *	write fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
elsa_mcall_write_fifo(void *base, const void *buf, size_t len)
{
}
#else
static void
elsa_mcall_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	/*
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	*/
}
#endif

/*---------------------------------------------------------------------------*
 *	write register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
elsa_mcall_write_reg(u_char *base, u_int offset, u_int v)
{
}
#else
static void
elsa_mcall_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	/*
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	*/
}
#endif

/*---------------------------------------------------------------------------*
 *	read register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static u_char
elsa_mcall_read_reg(u_char *base, u_int offset)
{
	return 0;
}
#else
static u_int8_t
elsa_mcall_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	/*
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	*/
	return 0;
}
#endif

#ifdef __FreeBSD__
#else

/*
 * XXX - one time only! Some of this has to go into an enable
 * function, with apropriate counterpart in disable, so a card
 * could be removed an inserted again. But never mind for now,
 * this won't work anyway for several reasons (both in NetBSD
 * and in I4B).
 */
int
isic_attach_elsamcall(struct pcmcia_isic_softc *psc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa)
{
	struct isic_softc *sc = &psc->sc_isic;
	bus_space_tag_t t;
	bus_space_handle_t h;

	/* Validate config info */
	if (cfe->num_memspace != 0)
		printf(": unexpected number of memory spaces %d should be 0\n",
			cfe->num_memspace);
	if (cfe->num_iospace != 1)
		printf(": unexpected number of memory spaces %d should be 1\n",
			cfe->num_iospace);

	/* Allocate pcmcia space */
	if (pcmcia_io_alloc(pa->pf, 0, cfe->iospace[0].length,
			    cfe->iospace[0].length, &psc->sc_pcioh))
		printf(": can't allocate i/o space\n");

	/* map them */
	if (pcmcia_io_map(pa->pf, ((cfe->flags & PCMCIA_CFE_IO16) ?
	    PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_IO8), 0,
	    cfe->iospace[0].length, &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return 0;
	}

	/* setup card type */
	sc->sc_cardtyp = CARD_TYPEP_ELSAMLMCALL;

	/* Setup bus space maps */
	sc->sc_num_mappings = 1;
	MALLOC_MAPS(sc);

	/* Copy our handles/tags to the MI maps */
	sc->sc_maps[0].t = psc->sc_pcioh.iot;
	sc->sc_maps[0].h = psc->sc_pcioh.ioh;
	sc->sc_maps[0].offset = 0;
	sc->sc_maps[0].size = 0;	/* not our mapping */

	t = sc->sc_maps[0].t;
	h = sc->sc_maps[0].h;

	sc->clearirq = NULL;
	sc->readreg = elsa_mcall_read_reg;
	sc->writereg = elsa_mcall_write_reg;

	sc->readfifo = elsa_mcall_read_fifo;
	sc->writefifo = elsa_mcall_write_fifo;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;

	return 1;
}
#endif

#endif /* NISIC > 0 */
