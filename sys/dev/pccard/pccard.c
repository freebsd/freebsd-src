/*	$NetBSD: pcmcia.c,v 1.13 1998/12/24 04:51:59 marc Exp $	*/
/* $FreeBSD$ */

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardchip.h>
#include <dev/pccard/pccardvar.h>

#ifdef PCCARDDEBUG
int	pccard_debug = 0;
#define	DPRINTF(arg) if (pccard_debug) printf arg
int	pccardintr_debug = 0;
/* this is done this way to avoid doing lots of conditionals
   at interrupt level.  */
#define PCCARD_CARD_INTR (pccardintr_debug?pccard_card_intrdebug:pccard_card_intr)
#else
#define	DPRINTF(arg)
#define PCCARD_CARD_INTR (pccard_card_intr)
#endif

#ifdef PCCARDVERBOSE
int	pccard_verbose = 1;
#else
int	pccard_verbose = 0;
#endif

int	pccard_print __P((void *, const char *));

static __inline void pccard_socket_enable __P((pccard_chipset_tag_t,
					     pccard_chipset_handle_t *));
static __inline void pccard_socket_disable __P((pccard_chipset_tag_t,
					      pccard_chipset_handle_t *));

int pccard_card_intr __P((void *));
#ifdef PCCARDDEBUG
int pccard_card_intrdebug __P((void *));
#endif

int
pccard_ccr_read(pf, ccr)
	struct pccard_function *pf;
	int ccr;
{

	return (bus_space_read_1(pf->pf_ccrt, pf->pf_ccrh,
	    pf->pf_ccr_offset + ccr));
}

void
pccard_ccr_write(pf, ccr, val)
	struct pccard_function *pf;
	int ccr;
	int val;
{

	if ((pf->ccr_mask) & (1 << (ccr / 2))) {
		bus_space_write_1(pf->pf_ccrt, pf->pf_ccrh,
		    pf->pf_ccr_offset + ccr, val);
	}
}

static int
pccard_card_attach(device_t dev)
{
	struct pccard_softc *sc = (struct pccard_softc *) 
	    device_get_softc(dev);
	struct pccard_function *pf;
	struct pccard_attach_args paa;
	int attached;

	/*
	 * this is here so that when socket_enable calls gettype, trt happens
	 */
	STAILQ_INIT(&sc->card.pf_head);

	pccard_chip_socket_enable(sc->pct, sc->pch);

	pccard_read_cis(sc);

	pccard_chip_socket_disable(sc->pct, sc->pch);

	pccard_check_cis_quirks(dev);

	/*
	 * bail now if the card has no functions, or if there was an error in
	 * the cis.
	 */

	if (sc->card.error)
		return (1);
	if (STAILQ_EMPTY(&sc->card.pf_head))
		return (1);

	if (pccard_verbose)
		pccard_print_cis(dev);

	attached = 0;

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;

#ifdef DIAGNOSTIC
		if (pf->child != NULL) {
			printf("%s: %s still attached to function %d!\n",
			    sc->dev.dv_xname, pf->child->dv_xname,
			    pf->number);
			panic("pccard_card_attach");
		}
#endif
		pf->sc = sc;
		pf->child = NULL;
		pf->cfe = NULL;
		pf->ih_fct = NULL;
		pf->ih_arg = NULL;
	}

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;

		paa.manufacturer = sc->card.manufacturer;
		paa.product = sc->card.product;
		paa.card = &sc->card;
		paa.pf = pf;

#if XXX
		if (attach_child()) {
			attached++;

			DPRINTF(("%s: function %d CCR at %d "
			     "offset %lx: %x %x %x %x, %x %x %x %x, %x\n",
			     sc->dev.dv_xname, pf->number,
			     pf->pf_ccr_window, pf->pf_ccr_offset,
			     pccard_ccr_read(pf, 0x00),
			pccard_ccr_read(pf, 0x02), pccard_ccr_read(pf, 0x04),
			pccard_ccr_read(pf, 0x06), pccard_ccr_read(pf, 0x0A),
			pccard_ccr_read(pf, 0x0C), pccard_ccr_read(pf, 0x0E),
			pccard_ccr_read(pf, 0x10), pccard_ccr_read(pf, 0x12)));
		}
#endif
	}

	return (attached ? 0 : 1);
}

static void
pccard_card_detach(device_t dev, int flags)
{
	struct pccard_softc *sc = (struct pccard_softc *) 
	    device_get_softc(dev);
	struct pccard_function *pf;
#if XXX
	int error;
#endif

	/*
	 * We are running on either the PCCARD socket's event thread
	 * or in user context detaching a device by user request.
	 */
	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_FIRST(&pf->cfe_head) == NULL)
			continue;
		if (pf->child == NULL)
			continue;
		DPRINTF(("%s: detaching %s (function %d)\n",
		    sc->dev.dv_xname, pf->child->dv_xname, pf->number));
#if XXX
		if ((error = config_detach(pf->child, flags)) != 0) {
			printf("%s: error %d detaching %s (function %d)\n",
			    sc->dev.dv_xname, error, pf->child->dv_xname,
			    pf->number);
		} else
			pf->child = NULL;
#endif
	}
}

static void
pccard_card_deactivate(device_t dev)
{
	struct pccard_softc *sc = (struct pccard_softc *) 
	    device_get_softc(dev);
	struct pccard_function *pf;

	/*
	 * We're in the chip's card removal interrupt handler.
	 * Deactivate the child driver.  The PCCARD socket's
	 * event thread will run later to finish the detach.
	 */
	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_FIRST(&pf->cfe_head) == NULL)
			continue;
		if (pf->child == NULL)
			continue;
		DPRINTF(("%s: deactivating %s (function %d)\n",
		    sc->dev.dv_xname, pf->child->dv_xname, pf->number));
#if XXX
		config_deactivate(pf->child);
#endif
	}
}

static int 
pccard_card_gettype(device_t dev)
{
	struct pccard_softc *sc = (struct pccard_softc *)
	    device_get_softc(dev);
	struct pccard_function *pf;

	/*
	 * set the iftype to memory if this card has no functions (not yet
	 * probed), or only one function, and that is not initialized yet or
	 * that is memory.
	 */
	pf = STAILQ_FIRST(&sc->card.pf_head);
	if (pf == NULL ||
	    (STAILQ_NEXT(pf, pf_list) == NULL &&
	    (pf->cfe == NULL || pf->cfe->iftype == PCCARD_IFTYPE_MEMORY)))
		return (PCCARD_IFTYPE_MEMORY);
	else
		return (PCCARD_IFTYPE_IO);
}

/*
 * Initialize a PCCARD function.  May be called as long as the function is
 * disabled.
 */
void
pccard_function_init(pf, cfe)
	struct pccard_function *pf;
	struct pccard_config_entry *cfe;
{
	if (pf->pf_flags & PFF_ENABLED)
		panic("pccard_function_init: function is enabled");

	/* Remember which configuration entry we are using. */
	pf->cfe = cfe;
}

static __inline void pccard_socket_enable(pct, pch)
     pccard_chipset_tag_t pct;
     pccard_chipset_handle_t *pch;
{
	pccard_chip_socket_enable(pct, pch);
}

static __inline void pccard_socket_disable(pct, pch)
     pccard_chipset_tag_t pct;
     pccard_chipset_handle_t *pch;
{
	pccard_chip_socket_disable(pct, pch);
}

/* Enable a PCCARD function */
int
pccard_function_enable(pf)
	struct pccard_function *pf;
{
	struct pccard_function *tmp;
	int reg;

	if (pf->cfe == NULL)
		panic("pccard_function_enable: function not initialized");

	/*
	 * Increase the reference count on the socket, enabling power, if
	 * necessary.
	 */
	if (pf->sc->sc_enabled_count++ == 0)
		pccard_chip_socket_enable(pf->sc->pct, pf->sc->pch);
	DPRINTF(("%s: ++enabled_count = %d\n", pf->sc->dev.dv_xname,
		 pf->sc->sc_enabled_count));

	if (pf->pf_flags & PFF_ENABLED) {
		/*
		 * Don't do anything if we're already enabled.
		 */
		return (0);
	}

	/*
	 * it's possible for different functions' CCRs to be in the same
	 * underlying page.  Check for that.
	 */

	STAILQ_FOREACH(tmp, &pf->sc->card.pf_head, pf_list) {
		if ((tmp->pf_flags & PFF_ENABLED) &&
		    (pf->ccr_base >= (tmp->ccr_base - tmp->pf_ccr_offset)) &&
		    ((pf->ccr_base + PCCARD_CCR_SIZE) <=
		     (tmp->ccr_base - tmp->pf_ccr_offset +
		      tmp->pf_ccr_realsize))) {
			pf->pf_ccrt = tmp->pf_ccrt;
			pf->pf_ccrh = tmp->pf_ccrh;
			pf->pf_ccr_realsize = tmp->pf_ccr_realsize;

			/*
			 * pf->pf_ccr_offset = (tmp->pf_ccr_offset -
			 * tmp->ccr_base) + pf->ccr_base;
			 */
			pf->pf_ccr_offset =
			    (tmp->pf_ccr_offset + pf->ccr_base) -
			    tmp->ccr_base;
			pf->pf_ccr_window = tmp->pf_ccr_window;
			break;
		}
	}

	if (tmp == NULL) {
		if (pccard_mem_alloc(pf, PCCARD_CCR_SIZE, &pf->pf_pcmh))
			goto bad;

		if (pccard_mem_map(pf, PCCARD_MEM_ATTR, pf->ccr_base,
		    PCCARD_CCR_SIZE, &pf->pf_pcmh, &pf->pf_ccr_offset,
		    &pf->pf_ccr_window)) {
			pccard_mem_free(pf, &pf->pf_pcmh);
			goto bad;
		}
	}

	reg = (pf->cfe->number & PCCARD_CCR_OPTION_CFINDEX);
	reg |= PCCARD_CCR_OPTION_LEVIREQ;
	if (pccard_mfc(pf->sc)) {
		reg |= (PCCARD_CCR_OPTION_FUNC_ENABLE |
			PCCARD_CCR_OPTION_ADDR_DECODE);
		if (pf->ih_fct)
			reg |= PCCARD_CCR_OPTION_IREQ_ENABLE;

	}
	pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);

	reg = 0;

	if ((pf->cfe->flags & PCCARD_CFE_IO16) == 0)
		reg |= PCCARD_CCR_STATUS_IOIS8;
	if (pf->cfe->flags & PCCARD_CFE_AUDIO)
		reg |= PCCARD_CCR_STATUS_AUDIO;
	pccard_ccr_write(pf, PCCARD_CCR_STATUS, reg);

	pccard_ccr_write(pf, PCCARD_CCR_SOCKETCOPY, 0);

	if (pccard_mfc(pf->sc)) {
		long tmp, iosize;

		tmp = pf->pf_mfc_iomax - pf->pf_mfc_iobase;
		/* round up to nearest (2^n)-1 */
		for (iosize = 1; iosize < tmp; iosize <<= 1)
			;
		iosize--;

		pccard_ccr_write(pf, PCCARD_CCR_IOBASE0,
				 pf->pf_mfc_iobase & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE1,
				 (pf->pf_mfc_iobase >> 8) & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE2, 0);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE3, 0);

		pccard_ccr_write(pf, PCCARD_CCR_IOSIZE, iosize);
	}

#ifdef PCCARDDEBUG
	if (pccard_debug) {
		STAILQ_FOREACH(tmp, &pf->sc->card.pf_head, pf_list) {
			printf("%s: function %d CCR at %d offset %lx: "
			       "%x %x %x %x, %x %x %x %x, %x\n",
			       tmp->sc->dev.dv_xname, tmp->number,
			       tmp->pf_ccr_window, tmp->pf_ccr_offset,
			       pccard_ccr_read(tmp, 0x00),
			       pccard_ccr_read(tmp, 0x02),
			       pccard_ccr_read(tmp, 0x04),
			       pccard_ccr_read(tmp, 0x06),

			       pccard_ccr_read(tmp, 0x0A),
			       pccard_ccr_read(tmp, 0x0C), 
			       pccard_ccr_read(tmp, 0x0E),
			       pccard_ccr_read(tmp, 0x10),

			       pccard_ccr_read(tmp, 0x12));
		}
	}
#endif

	pf->pf_flags |= PFF_ENABLED;
	return (0);

 bad:
	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	if (--pf->sc->sc_enabled_count == 0)
		pccard_chip_socket_disable(pf->sc->pct, pf->sc->pch);
	DPRINTF(("%s: --enabled_count = %d\n", pf->sc->dev.dv_xname,
		 pf->sc->sc_enabled_count));

	return (1);
}

/* Disable PCCARD function. */
void
pccard_function_disable(pf)
	struct pccard_function *pf;
{
	struct pccard_function *tmp;

	if (pf->cfe == NULL)
		panic("pccard_function_enable: function not initialized");

	if ((pf->pf_flags & PFF_ENABLED) == 0) {
		/*
		 * Don't do anything if we're already disabled.
		 */
		return;
	}

	/*
	 * it's possible for different functions' CCRs to be in the same
	 * underlying page.  Check for that.  Note we mark us as disabled
	 * first to avoid matching ourself.
	 */

	pf->pf_flags &= ~PFF_ENABLED;
	STAILQ_FOREACH(tmp, &pf->sc->card.pf_head, pf_list) {
		if ((tmp->pf_flags & PFF_ENABLED) &&
		    (pf->ccr_base >= (tmp->ccr_base - tmp->pf_ccr_offset)) &&
		    ((pf->ccr_base + PCCARD_CCR_SIZE) <=
		(tmp->ccr_base - tmp->pf_ccr_offset + tmp->pf_ccr_realsize)))
			break;
	}

	/* Not used by anyone else; unmap the CCR. */
	if (tmp == NULL) {
		pccard_mem_unmap(pf, pf->pf_ccr_window);
		pccard_mem_free(pf, &pf->pf_pcmh);
	}

	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	if (--pf->sc->sc_enabled_count == 0)
		pccard_chip_socket_disable(pf->sc->pct, pf->sc->pch);
	DPRINTF(("%s: --enabled_count = %d\n", pf->sc->dev.dv_xname,
		 pf->sc->sc_enabled_count));
}

int
pccard_io_map(pf, width, offset, size, pcihp, windowp)
	struct pccard_function *pf;
	int width;
	bus_addr_t offset;
	bus_size_t size;
	struct pccard_io_handle *pcihp;
	int *windowp;
{
	int reg;

	if (pccard_chip_io_map(pf->sc->pct, pf->sc->pch,
	    width, offset, size, pcihp, windowp))
		return (1);

	/*
	 * XXX in the multifunction multi-iospace-per-function case, this
	 * needs to cooperate with io_alloc to make sure that the spaces
	 * don't overlap, and that the ccr's are set correctly
	 */

	if (pccard_mfc(pf->sc)) {
		long tmp, iosize;

		if (pf->pf_mfc_iomax == 0) {
			pf->pf_mfc_iobase = pcihp->addr + offset;
			pf->pf_mfc_iomax = pf->pf_mfc_iobase + size;
		} else {
			/* this makes the assumption that nothing overlaps */
			if (pf->pf_mfc_iobase > pcihp->addr + offset)
				pf->pf_mfc_iobase = pcihp->addr + offset;
			if (pf->pf_mfc_iomax < pcihp->addr + offset + size)
				pf->pf_mfc_iomax = pcihp->addr + offset + size;
		}

		tmp = pf->pf_mfc_iomax - pf->pf_mfc_iobase;
		/* round up to nearest (2^n)-1 */
		for (iosize = 1; iosize >= tmp; iosize <<= 1)
			;
		iosize--;

		pccard_ccr_write(pf, PCCARD_CCR_IOBASE0,
				 pf->pf_mfc_iobase & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE1,
				 (pf->pf_mfc_iobase >> 8) & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE2, 0);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE3, 0);

		pccard_ccr_write(pf, PCCARD_CCR_IOSIZE, iosize);

		reg = pccard_ccr_read(pf, PCCARD_CCR_OPTION);
		reg |= PCCARD_CCR_OPTION_ADDR_DECODE;
		pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);
	}
	return (0);
}

void
pccard_io_unmap(pf, window)
	struct pccard_function *pf;
	int window;
{

	pccard_chip_io_unmap(pf->sc->pct, pf->sc->pch, window);

	/* XXX Anything for multi-function cards? */
}

void *
pccard_intr_establish(pf, ipl, ih_fct, ih_arg)
	struct pccard_function *pf;
	int ipl;
	int (*ih_fct) __P((void *));
	void *ih_arg;
{
	void *ret;

	/* behave differently if this is a multifunction card */

	if (pccard_mfc(pf->sc)) {
		int s, ihcnt, hiipl, reg;
		struct pccard_function *pf2;

		/*
		 * mask all the ipl's which are already used by this card,
		 * and find the highest ipl number (lowest priority)
		 */

		ihcnt = 0;
		s = 0;		/* this is only here to keep the compiler
				   happy */
		hiipl = 0;	/* this is only here to keep the compiler
				   happy */

		STAILQ_FOREACH(pf2, &pf->sc->card.pf_head, pf_list) {
			if (pf2->ih_fct) {
				DPRINTF(("%s: function %d has ih_fct %p\n",
					 pf->sc->dev.dv_xname, pf2->number,
					 pf2->ih_fct));

				if (ihcnt == 0) {
					hiipl = pf2->ih_ipl;
				} else {
					if (pf2->ih_ipl > hiipl)
						hiipl = pf2->ih_ipl;
				}

				ihcnt++;
			}
		}

		/*
		 * establish the real interrupt, changing the ipl if
		 * necessary
		 */

		if (ihcnt == 0) {
#ifdef DIAGNOSTIC
			if (pf->sc->ih != NULL)
				panic("card has intr handler, but no function does");
#endif
			s = splhigh();

			/* set up the handler for the new function */

			pf->ih_fct = ih_fct;
			pf->ih_arg = ih_arg;
			pf->ih_ipl = ipl;

			pf->sc->ih = pccard_chip_intr_establish(pf->sc->pct,
			    pf->sc->pch, pf, ipl, PCCARD_CARD_INTR, pf->sc);
			splx(s);
		} else if (ipl > hiipl) {
#ifdef DIAGNOSTIC
			if (pf->sc->ih == NULL)
				panic("functions have ih, but the card does not");
#endif

			/* XXX need #ifdef for splserial on x86 */
			s = splhigh();

			pccard_chip_intr_disestablish(pf->sc->pct, pf->sc->pch,
						      pf->sc->ih);

			/* set up the handler for the new function */
			pf->ih_fct = ih_fct;
			pf->ih_arg = ih_arg;
			pf->ih_ipl = ipl;

			pf->sc->ih = pccard_chip_intr_establish(pf->sc->pct,
			    pf->sc->pch, pf, ipl, PCCARD_CARD_INTR, pf->sc);

			splx(s);
		} else {
			s = splhigh();

			/* set up the handler for the new function */

			pf->ih_fct = ih_fct;
			pf->ih_arg = ih_arg;
			pf->ih_ipl = ipl;

			splx(s);
		}

		ret = pf->sc->ih;

		if (ret != NULL) {
			reg = pccard_ccr_read(pf, PCCARD_CCR_OPTION);
			reg |= PCCARD_CCR_OPTION_IREQ_ENABLE;
			pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);

			reg = pccard_ccr_read(pf, PCCARD_CCR_STATUS);
			reg |= PCCARD_CCR_STATUS_INTRACK;
			pccard_ccr_write(pf, PCCARD_CCR_STATUS, reg);
		}
	} else {
		ret = pccard_chip_intr_establish(pf->sc->pct, pf->sc->pch,
		    pf, ipl, ih_fct, ih_arg);
	}

	return (ret);
}

void
pccard_intr_disestablish(pf, ih)
	struct pccard_function *pf;
	void *ih;
{
	/* behave differently if this is a multifunction card */

	if (pccard_mfc(pf->sc)) {
		int s, ihcnt, hiipl;
		struct pccard_function *pf2;

		/*
		 * mask all the ipl's which are already used by this card,
		 * and find the highest ipl number (lowest priority).  Skip
		 * the current function.
		 */

		ihcnt = 0;
		s = 0;		/* this is only here to keep the compipler
				   happy */
		hiipl = 0;	/* this is only here to keep the compipler
				   happy */

		STAILQ_FOREACH(pf2, &pf->sc->card.pf_head, pf_list) {
			if (pf2 == pf)
				continue;

			if (pf2->ih_fct) {
				if (ihcnt == 0) {
					hiipl = pf2->ih_ipl;
				} else {
					if (pf2->ih_ipl > hiipl)
						hiipl = pf2->ih_ipl;
				}
				ihcnt++;
			}
		}

		/*
		 * if the ih being removed is lower priority than the lowest
		 * priority remaining interrupt, up the priority.
		 */

		/* ihcnt is the number of interrupt handlers *not* including
		   the one about to be removed. */

		if (ihcnt == 0) {
			int reg;

#ifdef DIAGNOSTIC
			if (pf->sc->ih == NULL)
				panic("disestablishing last function, but card has no ih");
#endif
			pccard_chip_intr_disestablish(pf->sc->pct, pf->sc->pch,
			    pf->sc->ih);

			reg = pccard_ccr_read(pf, PCCARD_CCR_OPTION);
			reg &= ~PCCARD_CCR_OPTION_IREQ_ENABLE;
			pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);

			pf->ih_fct = NULL;
			pf->ih_arg = NULL;

			pf->sc->ih = NULL;
		} else if (pf->ih_ipl > hiipl) {
#ifdef DIAGNOSTIC
			if (pf->sc->ih == NULL)
				panic("changing ih ipl, but card has no ih");
#endif
			/* XXX need #ifdef for splserial on x86 */
			s = splhigh();

			pccard_chip_intr_disestablish(pf->sc->pct, pf->sc->pch,
			    pf->sc->ih);
			pf->sc->ih = pccard_chip_intr_establish(pf->sc->pct,
			    pf->sc->pch, pf, hiipl, PCCARD_CARD_INTR, pf->sc);

			/* null out the handler for this function */

			pf->ih_fct = NULL;
			pf->ih_arg = NULL;

			splx(s);
		} else {
			s = splhigh();

			pf->ih_fct = NULL;
			pf->ih_arg = NULL;

			splx(s);
		}
	} else {
		pccard_chip_intr_disestablish(pf->sc->pct, pf->sc->pch, ih);
	}
}

int 
pccard_card_intr(arg)
	void *arg;
{
	struct pccard_softc *sc = arg;
	struct pccard_function *pf;
	int reg, ret, ret2;

	ret = 0;

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (pf->ih_fct != NULL &&
		    (pf->ccr_mask & (1 << (PCCARD_CCR_STATUS / 2)))) {
			reg = pccard_ccr_read(pf, PCCARD_CCR_STATUS);
			if (reg & PCCARD_CCR_STATUS_INTR) {
				ret2 = (*pf->ih_fct)(pf->ih_arg);
				if (ret2 != 0 && ret == 0)
					ret = ret2;
				reg = pccard_ccr_read(pf, PCCARD_CCR_STATUS);
				pccard_ccr_write(pf, PCCARD_CCR_STATUS,
				    reg & ~PCCARD_CCR_STATUS_INTR);
			}
		}
	}

	return (ret);
}

#ifdef PCCARDDEBUG
int 
pccard_card_intrdebug(arg)
	void *arg;
{
	struct pccard_softc *sc = arg;
	struct pccard_function *pf;
	int reg, ret, ret2;

	ret = 0;

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		printf("%s: intr flags=%x fct=%d cor=%02x csr=%02x pin=%02x",
		       sc->dev.dv_xname, pf->pf_flags, pf->number,
		       pccard_ccr_read(pf, PCCARD_CCR_OPTION),
		       pccard_ccr_read(pf, PCCARD_CCR_STATUS),
		       pccard_ccr_read(pf, PCCARD_CCR_PIN));
		if (pf->ih_fct != NULL &&
		    (pf->ccr_mask & (1 << (PCCARD_CCR_STATUS / 2)))) {
			reg = pccard_ccr_read(pf, PCCARD_CCR_STATUS);
			if (reg & PCCARD_CCR_STATUS_INTR) {
				ret2 = (*pf->ih_fct)(pf->ih_arg);
				if (ret2 != 0 && ret == 0)
					ret = ret2;
				reg = pccard_ccr_read(pf, PCCARD_CCR_STATUS);
				printf("; csr %02x->%02x",
				    reg, reg & ~PCCARD_CCR_STATUS_INTR);
				pccard_ccr_write(pf, PCCARD_CCR_STATUS,
				    reg & ~PCCARD_CCR_STATUS_INTR);
			}
		}
		printf("\n");
	}

	return (ret);
}
#endif

static int
pccard_add_children(device_t dev, int busno)
{
	device_add_child(dev, NULL, -1, NULL);
	return 0;
}

static int
pccard_probe(device_t dev)
{
	device_set_desc(dev, "PC Card bus -- newconfig version");
	return pccard_add_children(dev, device_get_unit(dev));
}

static device_method_t pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
#if 0
	DEVMETHOD(bus_print_child,	pccard_print_child),
#endif
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
#if 0
	DEVMETHOD(bus_alloc_resource,	pccard_alloc_resource),
	DEVMETHOD(bus_release_resource,	pccard_release_resource),
#endif
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
#if 0
	DEVMETHOD(bus_set_resource,	pccard_set_resource),
	DEVMETHOD(bus_get_resource,	pccard_get_resource),
	DEVMETHOD(bus_delete_resource,	pccard_delete_resource),
#endif

	{ 0, 0 }
};

static driver_t pccard_driver = {
	"pccard",
	pccard_methods,
	1,			/* no softc */
};

devclass_t	pccard_devclass;

DRIVER_MODULE(pccard, pcicx, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, pc98pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, pccbb, pccard_driver, pccard_devclass, 0, 0);
