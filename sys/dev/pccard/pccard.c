/*	$NetBSD: pcmcia.c,v 1.23 2000/07/28 19:17:02 drochner Exp $	*/
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
#include <sys/sysctl.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include "power_if.h"
#include "card_if.h"

#define PCCARDDEBUG

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, pccard, CTLFLAG_RD, 0, "PCCARD parameters");

int	pccard_debug = 0;
TUNABLE_INT("hw.pccard.debug", &pccard_debug);
SYSCTL_INT(_hw_pccard, OID_AUTO, debug, CTLFLAG_RW,
    &pccard_debug, 0,
  "pccard debug");

int	pccard_cis_debug = 0;
TUNABLE_INT("hw.pccard.cis_debug", &pccard_cis_debug);
SYSCTL_INT(_hw_pccard, OID_AUTO, cis_debug, CTLFLAG_RW,
    &pccard_cis_debug, 0, "pccard CIS debug");

#ifdef PCCARDDEBUG
#define	DPRINTF(arg) if (pccard_debug) printf arg
#define	DEVPRINTF(arg) if (pccard_debug) device_printf arg
#define PRVERBOSE(arg) printf arg
#define DEVPRVERBOSE(arg) device_printf arg
#else
#define	DPRINTF(arg)
#define	DEVPRINTF(arg)
#define PRVERBOSE(arg) if (bootverbose) printf arg
#define DEVPRVERBOSE(arg) if (bootverbose) device_printf arg
#endif

static int	pccard_ccr_read(struct pccard_function *pf, int ccr);
static void	pccard_ccr_write(struct pccard_function *pf, int ccr, int val);
static int	pccard_attach_card(device_t dev);
static int	pccard_detach_card(device_t dev, int flags);
static int	pccard_card_gettype(device_t dev, int *type);
static void	pccard_function_init(struct pccard_function *pf);
static void	pccard_function_free(struct pccard_function *pf);
static int	pccard_function_enable(struct pccard_function *pf);
static void	pccard_function_disable(struct pccard_function *pf);
static int	pccard_compat_do_probe(device_t bus, device_t dev);
static int	pccard_compat_do_attach(device_t bus, device_t dev);
static int	pccard_add_children(device_t dev, int busno);
static int	pccard_probe(device_t dev);
static int	pccard_attach(device_t dev);
static int	pccard_detach(device_t dev);
static void	pccard_print_resources(struct resource_list *rl,
		    const char *name, int type, int count, const char *format);
static int	pccard_print_child(device_t dev, device_t child);
static int	pccard_set_resource(device_t dev, device_t child, int type,
		    int rid, u_long start, u_long count);
static int	pccard_get_resource(device_t dev, device_t child, int type,
		    int rid, u_long *startp, u_long *countp);
static void	pccard_delete_resource(device_t dev, device_t child, int type,
		    int rid);
static int	pccard_set_res_flags(device_t dev, device_t child, int type,
		    int rid, u_int32_t flags);
static int	pccard_set_memory_offset(device_t dev, device_t child, int rid,
		    u_int32_t offset, u_int32_t *deltap);
static int	pccard_read_ivar(device_t bus, device_t child, int which,
		    u_char *result);
static void	pccard_driver_added(device_t dev, driver_t *driver);
static struct resource *pccard_alloc_resource(device_t dev,
		    device_t child, int type, int *rid, u_long start,
		    u_long end, u_long count, u_int flags);
static int	pccard_release_resource(device_t dev, device_t child, int type,
		    int rid, struct resource *r);
static void	pccard_child_detached(device_t parent, device_t dev);
static void	pccard_intr(void *arg);
static int	pccard_setup_intr(device_t dev, device_t child,
		    struct resource *irq, int flags, driver_intr_t *intr,
		    void *arg, void **cookiep);
static int	pccard_teardown_intr(device_t dev, device_t child,
		    struct resource *r, void *cookie);

static const struct pccard_product *
pccard_do_product_lookup(device_t bus, device_t dev,
			 const struct pccard_product *tab, size_t ent_size,
			 pccard_product_match_fn matchfn);


static int
pccard_ccr_read(struct pccard_function *pf, int ccr)
{
	return (bus_space_read_1(pf->pf_ccrt, pf->pf_ccrh,
	    pf->pf_ccr_offset + ccr));
}

static void
pccard_ccr_write(struct pccard_function *pf, int ccr, int val)
{
	if ((pf->ccr_mask) & (1 << (ccr / 2))) {
		bus_space_write_1(pf->pf_ccrt, pf->pf_ccrh,
		    pf->pf_ccr_offset + ccr, val);
	}
}

static int
pccard_attach_card(device_t dev)
{
	struct pccard_softc *sc = PCCARD_SOFTC(dev);
	struct pccard_function *pf;
	struct pccard_ivar *ivar;
	device_t child;

	/*
	 * this is here so that when socket_enable calls gettype, trt happens
	 */
	STAILQ_INIT(&sc->card.pf_head);

	DEVPRINTF((dev, "chip_socket_enable\n"));
	POWER_ENABLE_SOCKET(device_get_parent(dev), dev);

	DEVPRINTF((dev, "read_cis\n"));
	pccard_read_cis(sc);

	DEVPRINTF((dev, "check_cis_quirks\n"));
	pccard_check_cis_quirks(dev);

	/*
	 * bail now if the card has no functions, or if there was an error in
	 * the cis.
	 */

	if (sc->card.error) {
		device_printf (dev, "CARD ERROR!\n");
		return (1);
	}
	if (STAILQ_EMPTY(&sc->card.pf_head)) {
		device_printf (dev, "Card has no functions!\n");
		return (1);
	}

	if (bootverbose || pccard_debug)
		pccard_print_cis(dev);

	DEVPRINTF((dev, "functions scanning\n"));
	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;

		pf->sc = sc;
		pf->cfe = NULL;
		pf->dev = NULL;
	}

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;
		/*
		 * In NetBSD, the drivers are responsible for activating
		 * each function of a card.  I think that in FreeBSD we
		 * want to activate them enough for the usual bus_*_resource
		 * routines will do the right thing.  This many mean a
		 * departure from the current NetBSD model.
		 *
		 * This could get really ugly for multifunction cards.  But
		 * it might also just fall out of the FreeBSD resource model.
		 *
		 */
		ivar = malloc(sizeof(struct pccard_ivar), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		child = device_add_child(dev, NULL, -1);
		device_set_ivars(child, ivar);
		ivar->fcn = pf;
		pf->dev = child;
		/*
		 * XXX We might want to move the next two lines into
		 * XXX the pccard interface layer.  For the moment, this
		 * XXX is OK, but some drivers want to pick the config
		 * XXX entry to use as well as some address tweaks (mostly
		 * XXX due to bugs in decode logic that makes some
		 * XXX addresses illegal or broken).
		 */
		pccard_function_init(pf);
		if (sc->sc_enabled_count == 0)
			POWER_ENABLE_SOCKET(device_get_parent(dev), dev);
		if (pccard_function_enable(pf) == 0 &&
		    device_probe_and_attach(child) == 0) {
			DEVPRINTF((sc->dev, "function %d CCR at %d "
			    "offset %x: %x %x %x %x, %x %x %x %x, %x\n",
			    pf->number, pf->pf_ccr_window, pf->pf_ccr_offset,
			    pccard_ccr_read(pf, 0x00),
			pccard_ccr_read(pf, 0x02), pccard_ccr_read(pf, 0x04),
			pccard_ccr_read(pf, 0x06), pccard_ccr_read(pf, 0x0A),
			pccard_ccr_read(pf, 0x0C), pccard_ccr_read(pf, 0x0E),
			pccard_ccr_read(pf, 0x10), pccard_ccr_read(pf, 0x12)));
		} else {
			if (pf->cfe != NULL)
				pccard_function_disable(pf);
		}
	}
	if (sc->sc_enabled_count == 0)
		pccard_detach_card(dev, 0);
	return (0);
}

static int
pccard_detach_card(device_t dev, int flags)
{
	struct pccard_softc *sc = PCCARD_SOFTC(dev);
	struct pccard_function *pf;
	struct pccard_config_entry *cfe;

	/*
	 * We are running on either the PCCARD socket's event thread
	 * or in user context detaching a device by user request.
	 */
	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		int state = device_get_state(pf->dev);

		if (state == DS_ATTACHED || state == DS_BUSY)
			device_detach(pf->dev);
		if (pf->cfe != NULL)
			pccard_function_disable(pf);
		pccard_function_free(pf);
		if (pf->dev != NULL)
			device_delete_child(dev, pf->dev);
	}
	if (sc->sc_enabled_count == 0)
		POWER_DISABLE_SOCKET(device_get_parent(dev), dev);

	while (NULL != (pf = STAILQ_FIRST(&sc->card.pf_head))) {
		while (NULL != (cfe = STAILQ_FIRST(&pf->cfe_head))) {
			STAILQ_REMOVE_HEAD(&pf->cfe_head, cfe_list);
			free(cfe, M_DEVBUF);
		}
		STAILQ_REMOVE_HEAD(&sc->card.pf_head, pf_list);
		free(pf, M_DEVBUF);
	}
	return (0);
}

static const struct pccard_product *
pccard_do_product_lookup(device_t bus, device_t dev,
			 const struct pccard_product *tab, size_t ent_size,
			 pccard_product_match_fn matchfn)
{
	const struct pccard_product *ent;
	int matches;
	u_int32_t fcn;
	u_int32_t vendor;
	u_int32_t prod;
	char *vendorstr;
	char *prodstr;

#ifdef DIAGNOSTIC
	if (sizeof *ent > ent_size)
		panic("pccard_product_lookup: bogus ent_size %ld",
		    (long) ent_size);
#endif
	if (pccard_get_vendor(dev, &vendor))
		return (NULL);
	if (pccard_get_product(dev, &prod))
		return (NULL);
	if (pccard_get_function_number(dev, &fcn))
		return (NULL);
	if (pccard_get_vendor_str(dev, &vendorstr))
		return (NULL);
	if (pccard_get_product_str(dev, &prodstr))
		return (NULL);
	for (ent = tab; ent->pp_name != NULL; ent =
	    (const struct pccard_product *) ((const char *) ent + ent_size)) {
		matches = 1;
		if (ent->pp_vendor == PCCARD_VENDOR_ANY &&
		    ent->pp_product == PCCARD_VENDOR_ANY &&
		    ent->pp_cis[0] == NULL &&
		    ent->pp_cis[1] == NULL) {
			device_printf(dev,
			    "Total wildcard entry ignored for %s\n",
			    ent->pp_name);
			continue;
		}
		if (matches && ent->pp_vendor != PCCARD_VENDOR_ANY &&
		    vendor != ent->pp_vendor)
			matches = 0;
		if (matches && ent->pp_product != PCCARD_PRODUCT_ANY &&
		    prod != ent->pp_product)
			matches = 0;
		if (matches && fcn != ent->pp_expfunc)
			matches = 0;
		if (matches && ent->pp_cis[0] &&
		    strcmp(ent->pp_cis[0], vendorstr) != 0)
			matches = 0;
		if (matches && ent->pp_cis[1] &&
		    strcmp(ent->pp_cis[1], prodstr) != 0)
			matches = 0;
		/* XXX need to match cis[2] and cis[3] also XXX */
		if (matchfn != NULL)
			matches = (*matchfn)(dev, ent, matches);
		if (matches)
			return (ent);
	}
	return (NULL);
}

static int
pccard_card_gettype(device_t dev, int *type)
{
	struct pccard_softc *sc = PCCARD_SOFTC(dev);
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
		*type = PCCARD_IFTYPE_MEMORY;
	else
		*type = PCCARD_IFTYPE_IO;
	return (0);
}

/*
 * Initialize a PCCARD function.  May be called as long as the function is
 * disabled.
 *
 * Note: pccard_function_init should not keep resources allocated.  It should
 * only set them up ala isa pnp, set the values in the rl lists, and return.
 * Any resource held after pccard_function_init is called is a bug.  However,
 * the bus routines to get the resources also assume that pccard_function_init
 * does this, so they need to be fixed too.
 */
static void
pccard_function_init(struct pccard_function *pf)
{
	struct pccard_config_entry *cfe;
	int i;
	struct pccard_ivar *devi = PCCARD_IVAR(pf->dev);
	struct resource_list *rl = &devi->resources;
	struct resource_list_entry *rle;
	struct resource *r = 0;
	device_t bus;
	int start;
	int end;
	int spaces;

	if (pf->pf_flags & PFF_ENABLED) {
		printf("pccard_function_init: function is enabled");
		return;
	}
	bus = device_get_parent(pf->dev);
	/* Remember which configuration entry we are using. */
	STAILQ_FOREACH(cfe, &pf->cfe_head, cfe_list) {
		for (i = 0; i < cfe->num_iospace; i++)
			cfe->iores[i] = NULL;
		cfe->irqres = NULL;
		spaces = 0;
		for (i = 0; i < cfe->num_iospace; i++) {
			start = cfe->iospace[i].start;
			if (start)
				end = start + cfe->iospace[i].length - 1;
			else
				end = ~0;
			cfe->iorid[i] = i;
			DEVPRINTF((bus, "I/O rid %d start %x end %x\n",
			    i, start, end));
			r = cfe->iores[i] = bus_alloc_resource(bus,
			    SYS_RES_IOPORT, &cfe->iorid[i], start, end,
			    cfe->iospace[i].length,
			    rman_make_alignment_flags(cfe->iospace[i].length));
			if (cfe->iores[i] == NULL)
				goto not_this_one;
			resource_list_add(rl, SYS_RES_IOPORT, cfe->iorid[i],
			    rman_get_start(r), rman_get_end(r),
			    cfe->iospace[i].length);
			rle = resource_list_find(rl, SYS_RES_IOPORT,
			    cfe->iorid[i]);
			rle->res = r;
			spaces++;
		}
		if (cfe->num_memspace > 0) {
			/*
			 * Not implement yet, Fix me.
			 */
			DEVPRINTF((bus, "Memory space not yet implemented.\n"));
		}
		if (spaces == 0) {
			DEVPRINTF((bus, "Neither memory nor I/O mampped\n"));
			goto not_this_one;
		}
		if (cfe->irqmask) {
			cfe->irqrid = 0;
			r = cfe->irqres = bus_alloc_resource(bus, SYS_RES_IRQ,
			    &cfe->irqrid, 0, ~0, 1, 0);
			if (cfe->irqres == NULL)
				goto not_this_one;
			resource_list_add(rl, SYS_RES_IRQ, cfe->irqrid,
			    rman_get_start(r), rman_get_end(r), 1);
			rle = resource_list_find(rl, SYS_RES_IRQ,
			    cfe->irqrid);
			rle->res = r;
		}
		/* If we get to here, we've allocated all we need */
		pf->cfe = cfe;
		break;
	    not_this_one:;
		DEVPRVERBOSE((bus, "Allocation failed for cfe %d\n",
		    cfe->number));
		/*
		 * Release resources that we partially allocated
		 * from this config entry.
		 */
		for (i = 0; i < cfe->num_iospace; i++) {
			if (cfe->iores[i] != NULL) {
				bus_release_resource(bus, SYS_RES_IOPORT,
				    cfe->iorid[i], cfe->iores[i]);
				rle = resource_list_find(rl, SYS_RES_IOPORT,
				    cfe->iorid[i]);
				rle->res = NULL;
				resource_list_delete(rl, SYS_RES_IOPORT,
				    cfe->iorid[i]);
			}
			cfe->iores[i] = NULL;
		}
		if (cfe->irqmask && cfe->irqres != NULL) {
			bus_release_resource(bus, SYS_RES_IRQ,
			    cfe->irqrid, cfe->irqres);
			rle = resource_list_find(rl, SYS_RES_IRQ,
			    cfe->irqrid);
			rle->res = NULL;
			resource_list_delete(rl, SYS_RES_IRQ, cfe->irqrid);
			cfe->irqres = NULL;
		}
	}
}

/*
 * Free resources allocated by pccard_function_init(), May be called as long
 * as the function is disabled.
 *
 * NOTE: This function should be unnecessary.  pccard_function_init should
 * never keep resources initialized.
 */
static void
pccard_function_free(struct pccard_function *pf)
{
	struct pccard_ivar *devi = PCCARD_IVAR(pf->dev);
	struct resource_list_entry *rle;

	if (pf->pf_flags & PFF_ENABLED) {
		printf("pccard_function_init: function is enabled");
		return;
	}

	SLIST_FOREACH(rle, &devi->resources, link) {
		if (rle->res) {
			if (rle->res->r_dev != pf->sc->dev)
				device_printf(pf->sc->dev,
				    "function_free: Resource still owned by "
				    "child, oops. "
				    "(type=%d, rid=%d, addr=%lx)\n",
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
			BUS_RELEASE_RESOURCE(device_get_parent(pf->sc->dev),
			    rle->res->r_dev, rle->type, rle->rid, rle->res);
			rle->res = NULL;
		}
	}
	resource_list_free(&devi->resources);
}

/* Enable a PCCARD function */
static int
pccard_function_enable(struct pccard_function *pf)
{
	struct pccard_function *tmp;
	int reg;
	device_t dev = pf->sc->dev;

	if (pf->cfe == NULL) {
		DEVPRVERBOSE((dev, "No config entry could be allocated.\n"));
		return (ENOMEM);
	}

	/*
	 * Increase the reference count on the socket, enabling power, if
	 * necessary.
	 */
	pf->sc->sc_enabled_count++;

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
			/* pf->pf_ccr_offset =
			    (tmp->pf_ccr_offset + pf->ccr_base) -
			    tmp->ccr_base; */
			pf->pf_ccr_window = tmp->pf_ccr_window;
			break;
		}
	}
	if (tmp == NULL) {
		pf->ccr_rid = 0;
		pf->ccr_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
		    &pf->ccr_rid, 0, ~0, 1 << 10, RF_ACTIVE);
		if (!pf->ccr_res)
			goto bad;
		DEVPRINTF((dev, "ccr_res == %lx-%lx, base=%lx\n",
		    rman_get_start(pf->ccr_res), rman_get_end(pf->ccr_res),
		    pf->ccr_base));
		CARD_SET_RES_FLAGS(device_get_parent(dev), dev, SYS_RES_MEMORY,
		    pf->ccr_rid, PCCARD_A_MEM_ATTR);
		CARD_SET_MEMORY_OFFSET(device_get_parent(dev), dev,
		    pf->ccr_rid, pf->ccr_base, &pf->pf_ccr_offset);
		pf->pf_ccrt = rman_get_bustag(pf->ccr_res);
		pf->pf_ccrh = rman_get_bushandle(pf->ccr_res);
		pf->pf_ccr_realsize = 1;
	}

	reg = (pf->cfe->number & PCCARD_CCR_OPTION_CFINDEX);
	reg |= PCCARD_CCR_OPTION_LEVIREQ;
#ifdef COOKIE_FOR_WARNER
	if (pccard_mfc(pf->sc)) {
		reg |= (PCCARD_CCR_OPTION_FUNC_ENABLE |
			PCCARD_CCR_OPTION_ADDR_DECODE);
		/* PCCARD_CCR_OPTION_IRQ_ENABLE set elsewhere as needed */
	}
#endif
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
			device_printf(tmp->sc->dev,
			    "function %d CCR at %d offset %x: "
			    "%x %x %x %x, %x %x %x %x, %x\n",
			    tmp->number, tmp->pf_ccr_window,
			    tmp->pf_ccr_offset,
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
	pf->sc->sc_enabled_count--;
	DEVPRINTF((dev, "bad --enabled_count = %d\n", pf->sc->sc_enabled_count));

	return (1);
}

/* Disable PCCARD function. */
static void
pccard_function_disable(struct pccard_function *pf)
{
	struct pccard_function *tmp;
	device_t dev = pf->sc->dev;

	if (pf->cfe == NULL)
		panic("pccard_function_disable: function not initialized");

	if ((pf->pf_flags & PFF_ENABLED) == 0) {
		/*
		 * Don't do anything if we're already disabled.
		 */
		return;
	}

	if (pf->intr_handler != NULL) {
		struct pccard_ivar *devi = PCCARD_IVAR(pf->dev);
		struct resource_list_entry *rle =
		    resource_list_find(&devi->resources, SYS_RES_IRQ, 0);
		BUS_TEARDOWN_INTR(dev, pf->dev, rle->res,
		    pf->intr_handler_cookie);
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
		    (tmp->ccr_base - tmp->pf_ccr_offset +
		    tmp->pf_ccr_realsize)))
			break;
	}

	/* Not used by anyone else; unmap the CCR. */
	if (tmp == NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, pf->ccr_rid,
		    pf->ccr_res);
		pf->ccr_res = NULL;
	}

	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	pf->sc->sc_enabled_count--;
}

#if 0
/* XXX These functions are needed, but not like this XXX */
int
pccard_io_map(struct pccard_function *pf, int width, bus_addr_t offset,
    bus_size_t size, struct pccard_io_handle *pcihp, int *windowp)
{
	int reg;

	if (pccard_chip_io_map(pf->sc->pct, pf->sc->pch, width, offset, size,
	    pcihp, windowp))
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

#ifdef COOKIE_FOR_WARNER
		reg = pccard_ccr_read(pf, PCCARD_CCR_OPTION);
		reg |= PCCARD_CCR_OPTION_ADDR_DECODE;
		pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);
#endif
	}
	return (0);
}

void
pccard_io_unmap(struct pccard_function *pf, int window)
{

	pccard_chip_io_unmap(pf->sc->pct, pf->sc->pch, window);

	/* XXX Anything for multi-function cards? */
}
#endif

/*
 * simulate the old "probe" routine.  In the new world order, the driver
 * needs to grab devices while in the old they were assigned to the device by
 * the pccardd process.  These symbols are exported to the upper layers.
 */
static int
pccard_compat_do_probe(device_t bus, device_t dev)
{
	return (CARD_COMPAT_MATCH(dev));
}

static int
pccard_compat_do_attach(device_t bus, device_t dev)
{
	int err;

	err = CARD_COMPAT_PROBE(dev);
	if (err == 0)
		err = CARD_COMPAT_ATTACH(dev);
	return (err);
}

#define PCCARD_NPORT	2
#define PCCARD_NMEM	5
#define PCCARD_NIRQ	1
#define PCCARD_NDRQ	0

static int
pccard_add_children(device_t dev, int busno)
{
	/* Call parent to scan for any current children */
	return (0);
}

static int
pccard_probe(device_t dev)
{
	device_set_desc(dev, "16-bit PCCard bus");
	return (pccard_add_children(dev, device_get_unit(dev)));
}

static int
pccard_attach(device_t dev)
{
	struct pccard_softc *sc = PCCARD_SOFTC(dev);

	sc->dev = dev;
	sc->sc_enabled_count = 0;
	return (bus_generic_attach(dev));
}

static int
pccard_detach(device_t dev)
{
	pccard_detach_card(dev, 0);
	return 0;
}

static int
pccard_suspend(device_t self)
{
	pccard_detach_card(self, 0);
	return (0);
}

static
int
pccard_resume(device_t self)
{
	return (0);
}

static void
pccard_print_resources(struct resource_list *rl, const char *name, int type,
    int count, const char *format)
{
	struct resource_list_entry *rle;
	int printed;
	int i;

	printed = 0;
	for (i = 0; i < count; i++) {
		rle = resource_list_find(rl, type, i);
		if (rle != NULL) {
			if (printed == 0)
				printf(" %s ", name);
			else if (printed > 0)
				printf(",");
			printed++;
			printf(format, rle->start);
			if (rle->count > 1) {
				printf("-");
				printf(format, rle->start + rle->count - 1);
			}
		} else if (i > 3) {
			/* check the first few regardless */
			break;
		}
	}
}

static int
pccard_print_child(device_t dev, device_t child)
{
	struct pccard_ivar *devi = PCCARD_IVAR(child);
	struct resource_list *rl = &devi->resources;
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at");

	if (devi != NULL) {
		pccard_print_resources(rl, "port", SYS_RES_IOPORT,
		    PCCARD_NPORT, "%#lx");
		pccard_print_resources(rl, "iomem", SYS_RES_MEMORY,
		    PCCARD_NMEM, "%#lx");
		pccard_print_resources(rl, "irq", SYS_RES_IRQ, PCCARD_NIRQ,
		    "%ld");
		pccard_print_resources(rl, "drq", SYS_RES_DRQ, PCCARD_NDRQ,
		    "%ld");
		retval += printf(" function %d config %d", devi->fcn->number,
		    devi->fcn->cfe->number);
	}

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
pccard_set_resource(device_t dev, device_t child, int type, int rid,
		 u_long start, u_long count)
{
	struct pccard_ivar *devi = PCCARD_IVAR(child);
	struct resource_list *rl = &devi->resources;

	if (type != SYS_RES_IOPORT && type != SYS_RES_MEMORY
	    && type != SYS_RES_IRQ && type != SYS_RES_DRQ)
		return (EINVAL);
	if (rid < 0)
		return (EINVAL);
	if (type == SYS_RES_IOPORT && rid >= PCCARD_NPORT)
		return (EINVAL);
	if (type == SYS_RES_MEMORY && rid >= PCCARD_NMEM)
		return (EINVAL);
	if (type == SYS_RES_IRQ && rid >= PCCARD_NIRQ)
		return (EINVAL);
	if (type == SYS_RES_DRQ && rid >= PCCARD_NDRQ)
		return (EINVAL);

	resource_list_add(rl, type, rid, start, start + count - 1, count);
	if (NULL != resource_list_alloc(rl, device_get_parent(dev), dev,
	    type, &rid, start, start + count - 1, count, 0))
		return 0;
	else
		return ENOMEM;
}

static int
pccard_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct pccard_ivar *devi = PCCARD_IVAR(child);
	struct resource_list *rl = &devi->resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		return (ENOENT);

	if (startp != NULL)
		*startp = rle->start;
	if (countp != NULL)
		*countp = rle->count;

	return (0);
}

static void
pccard_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct pccard_ivar *devi = PCCARD_IVAR(child);
	struct resource_list *rl = &devi->resources;
	resource_list_delete(rl, type, rid);
}

static int
pccard_set_res_flags(device_t dev, device_t child, int type, int rid,
    u_int32_t flags)
{
	return (CARD_SET_RES_FLAGS(device_get_parent(dev), child, type,
	    rid, flags));
}

static int
pccard_set_memory_offset(device_t dev, device_t child, int rid,
    u_int32_t offset, u_int32_t *deltap)

{
	return (CARD_SET_MEMORY_OFFSET(device_get_parent(dev), child, rid,
	    offset, deltap));
}

static int
pccard_read_ivar(device_t bus, device_t child, int which, u_char *result)
{
	struct pccard_ivar *devi = PCCARD_IVAR(child);
	struct pccard_function *func = devi->fcn;
	struct pccard_softc *sc = PCCARD_SOFTC(bus);

	/* PCCARD_IVAR_ETHADDR unhandled from oldcard */
	switch (which) {
	default:
	case PCCARD_IVAR_ETHADDR:
		bcopy(func->pf_funce_lan_nid, result, ETHER_ADDR_LEN);
		break;
	case PCCARD_IVAR_VENDOR:
		*(u_int32_t *) result = sc->card.manufacturer;
		break;
	case PCCARD_IVAR_PRODUCT:
		*(u_int32_t *) result = sc->card.product;
		break;
	case PCCARD_IVAR_PRODEXT:
		*(u_int16_t *) result = sc->card.prodext;
		break;
	case PCCARD_IVAR_FUNCTION:
		*(u_int32_t *) result = func->function;
		break;
	case PCCARD_IVAR_FUNCTION_NUMBER:
		if (!func) {
			device_printf(bus, "No function number, bug!\n");
			return (ENOENT);
		}
		*(u_int32_t *) result = func->number;
		break;
	case PCCARD_IVAR_VENDOR_STR:
		*(char **) result = sc->card.cis1_info[0];
		break;
	case PCCARD_IVAR_PRODUCT_STR:
		*(char **) result = sc->card.cis1_info[1];
		break;
	case PCCARD_IVAR_CIS3_STR:
		*(char **) result = sc->card.cis1_info[2];
		break;
	case PCCARD_IVAR_CIS4_STR:
		*(char **) result = sc->card.cis1_info[2];
		break;
	}
	return (0);
}

static void
pccard_driver_added(device_t dev, driver_t *driver)
{
	struct pccard_softc *sc = PCCARD_SOFTC(dev);
	struct pccard_function *pf;
	device_t child;

	if (sc->sc_enabled_count == 0) {
		CARD_REPROBE_CARD(device_get_parent(dev), dev);
		return;
	}

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;
		child = pf->dev;
		if (device_get_state(child) != DS_NOTPRESENT)
			continue;
		if (pccard_function_enable(pf) == 0 &&
		    device_probe_and_attach(child) == 0) {
			DEVPRINTF((sc->dev, "function %d CCR at %d "
			    "offset %x: %x %x %x %x, %x %x %x %x, %x\n",
			    pf->number, pf->pf_ccr_window, pf->pf_ccr_offset,
			    pccard_ccr_read(pf, 0x00),
			pccard_ccr_read(pf, 0x02), pccard_ccr_read(pf, 0x04),
			pccard_ccr_read(pf, 0x06), pccard_ccr_read(pf, 0x0A),
			pccard_ccr_read(pf, 0x0C), pccard_ccr_read(pf, 0x0E),
			pccard_ccr_read(pf, 0x10), pccard_ccr_read(pf, 0x12)));
		} else {
			if (pf->cfe != NULL)
				pccard_function_disable(pf);
		}
	}
	return;
}

static struct resource *
pccard_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct pccard_ivar *dinfo;
	struct resource_list_entry *rle = 0;
	int passthrough = (device_get_parent(child) != dev);

	if (passthrough) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));
	}

	dinfo = device_get_ivars(child);
	rle = resource_list_find(&dinfo->resources, type, *rid);

	if (!rle)
		return NULL;		/* no resource of that type/rid */

	if (!rle->res) {
		device_printf(dev, "WARNING: Resource not reserved by pccard bus\n");
		return NULL;
	} else {
		if (rle->res->r_dev != dev)
			return (NULL);
		bus_release_resource(dev, type, *rid, rle->res);
		rle->res = NULL;
		switch(type) {
		case SYS_RES_IOPORT:
		case SYS_RES_MEMORY:
			if (!(flags & RF_ALIGNMENT_MASK))
				flags |= rman_make_alignment_flags(rle->count);
			break;
		case SYS_RES_IRQ:
			flags |= RF_SHAREABLE;
			break;
		}
		rle->res = resource_list_alloc(&dinfo->resources, dev, child,
		    type, rid, rle->start, rle->end, rle->count, flags);
		return (rle->res);
	}
}

static int
pccard_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_ivar *dinfo;
	int passthrough = (device_get_parent(child) != dev);
	struct resource_list_entry *rle = 0;
	int ret;
	int flags;

	if (passthrough)
		return BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
		    type, rid, r);

	dinfo = device_get_ivars(child);

	rle = resource_list_find(&dinfo->resources, type, rid);

	if (!rle) {
		device_printf(dev, "Allocated resource not found, "
		    "%d %x %lx %lx\n",
		    type, rid, rman_get_start(r), rman_get_size(r));
		return ENOENT;
	}
	if (!rle->res) {
		device_printf(dev, "Allocated resource not recorded\n");
		return ENOENT;
	}

	ret = BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
	    type, rid, r);
	switch(type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		flags = rman_make_alignment_flags(rle->count);
		break;
	case SYS_RES_IRQ:
		flags = RF_SHAREABLE;
		break;
	default:
		flags = 0;
	}
	rle->res = bus_alloc_resource(dev, type, &rid,
	    rle->start, rle->end, rle->count, flags);
	if (rle->res == NULL)
		device_printf(dev, "release_resource: "
		    "unable to reaquire resource\n");
	return ret;
}

static void
pccard_child_detached(device_t parent, device_t dev)
{
	struct pccard_ivar *ivar = PCCARD_IVAR(dev);
	struct pccard_function *pf = ivar->fcn;

	pccard_function_disable(pf);
}

static void
pccard_intr(void *arg)
{
	struct pccard_function *pf = (struct pccard_function*) arg;
	int reg;

	if (pf->intr_handler == NULL)
		return;

	/*
	 * XXX The CCR_STATUS register bits used here are 
	 * only valid for multi function cards.
	 */
	reg = pccard_ccr_read(pf, PCCARD_CCR_STATUS);
	if (reg & PCCARD_CCR_STATUS_INTR) {
		pccard_ccr_write(pf, PCCARD_CCR_STATUS,
				 reg & ~PCCARD_CCR_STATUS_INTR);
		pf->intr_handler(pf->intr_handler_arg);
	}
}

static int
pccard_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct pccard_ivar *ivar = PCCARD_IVAR(child);
	struct pccard_function *func = ivar->fcn;
	int err;

	if (func->intr_handler != NULL)
		panic("Only one interrupt handler per function allowed");
	err = bus_generic_setup_intr(dev, child, irq, flags, pccard_intr,
	    func, cookiep);
	if (err != 0)
		return (err);
	func->intr_handler = intr;
	func->intr_handler_arg = arg;
	func->intr_handler_cookie = *cookiep;
#ifdef COOKIE_FOR_WARNER
	/* XXX Not sure this is right to write to ccr */
	pccard_ccr_write(func, PCCARD_CCR_OPTION,
	    pccard_ccr_read(func, PCCARD_CCR_OPTION) |
	    PCCARD_CCR_OPTION_IREQ_ENABLE);
#endif
	return (0);
}

static int
pccard_teardown_intr(device_t dev, device_t child, struct resource *r,
    void *cookie)
{
	struct pccard_ivar *ivar = PCCARD_IVAR(child);
	struct pccard_function *func = ivar->fcn;
	int ret;

#ifdef COOKIE_FOR_WARNER
	/* XXX Not sure this is right to write to ccr */
	pccard_ccr_write(func, PCCARD_CCR_OPTION,
	    pccard_ccr_read(func, PCCARD_CCR_OPTION) &
	    ~PCCARD_CCR_OPTION_IREQ_ENABLE);
#endif
	ret = bus_generic_teardown_intr(dev, child, r, cookie);
	if (ret == 0) {
		func->intr_handler = NULL;
		func->intr_handler_arg = NULL;
		func->intr_handler_cookie = NULL;
	}

	return (ret);
}

static device_method_t pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_probe),
	DEVMETHOD(device_attach,	pccard_attach),
	DEVMETHOD(device_detach,	pccard_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	pccard_suspend),
	DEVMETHOD(device_resume,	pccard_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pccard_print_child),
	DEVMETHOD(bus_driver_added,	pccard_driver_added),
	DEVMETHOD(bus_child_detached,	pccard_child_detached),
	DEVMETHOD(bus_alloc_resource,	pccard_alloc_resource),
	DEVMETHOD(bus_release_resource,	pccard_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	pccard_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pccard_teardown_intr),
	DEVMETHOD(bus_set_resource,	pccard_set_resource),
	DEVMETHOD(bus_get_resource,	pccard_get_resource),
	DEVMETHOD(bus_delete_resource,	pccard_delete_resource),
	DEVMETHOD(bus_read_ivar,	pccard_read_ivar),

	/* Card Interface */
	DEVMETHOD(card_set_res_flags,	pccard_set_res_flags),
	DEVMETHOD(card_set_memory_offset, pccard_set_memory_offset),
	DEVMETHOD(card_get_type,	pccard_card_gettype),
	DEVMETHOD(card_attach_card,	pccard_attach_card),
	DEVMETHOD(card_detach_card,	pccard_detach_card),
	DEVMETHOD(card_compat_do_probe, pccard_compat_do_probe),
	DEVMETHOD(card_compat_do_attach, pccard_compat_do_attach),
	DEVMETHOD(card_do_product_lookup, pccard_do_product_lookup),

	{ 0, 0 }
};

static driver_t pccard_driver = {
	"pccard",
	pccard_methods,
	sizeof(struct pccard_softc)
};

devclass_t	pccard_devclass;

/* Maybe we need to have a slot device? */
DRIVER_MODULE(pccard, pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, pc98pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, cbb, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, tcic, pccard_driver, pccard_devclass, 0, 0);
MODULE_VERSION(pccard, 1);
/*MODULE_DEPEND(pccard, pcic, 1, 1, 1);*/
