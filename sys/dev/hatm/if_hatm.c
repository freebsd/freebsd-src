/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * ForeHE driver.
 *
 * This file contains the module and driver infrastructure stuff as well
 * as a couple of utility functions and the entire initialisation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_atm.h>
#include <net/route.h>
#ifdef ENABLE_BPF
#include <net/bpf.h>
#endif
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/utopia/utopia.h>
#include <dev/hatm/if_hatmconf.h>
#include <dev/hatm/if_hatmreg.h>
#include <dev/hatm/if_hatmvar.h>

static const struct {
	uint16_t	vid;
	uint16_t	did;
	const char	*name;
} hatm_devs[] = {
	{ 0x1127, 0x400,
	  "FORE HE" },
	{ 0, 0, NULL }
};

SYSCTL_DECL(_hw_atm);

MODULE_DEPEND(hatm, utopia, 1, 1, 1);
MODULE_DEPEND(hatm, pci, 1, 1, 1);
MODULE_DEPEND(hatm, atm, 1, 1, 1);

#define EEPROM_DELAY	400 /* microseconds */

/* Read from EEPROM 0000 0011b */
static const uint32_t readtab[] = {
	HE_REGM_HOST_PROM_SEL | HE_REGM_HOST_PROM_CLOCK,
	0,
	HE_REGM_HOST_PROM_CLOCK,
	0,				/* 0 */
	HE_REGM_HOST_PROM_CLOCK,	
	0,				/* 0 */
	HE_REGM_HOST_PROM_CLOCK,
	0,				/* 0 */
	HE_REGM_HOST_PROM_CLOCK,
	0,				/* 0 */
	HE_REGM_HOST_PROM_CLOCK,
	0,				/* 0 */
	HE_REGM_HOST_PROM_CLOCK,
	HE_REGM_HOST_PROM_DATA_IN,	/* 0 */
	HE_REGM_HOST_PROM_CLOCK | HE_REGM_HOST_PROM_DATA_IN,
	HE_REGM_HOST_PROM_DATA_IN,	/* 1 */
	HE_REGM_HOST_PROM_CLOCK | HE_REGM_HOST_PROM_DATA_IN,
	HE_REGM_HOST_PROM_DATA_IN,	/* 1 */
};
static const uint32_t clocktab[] = {
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0, HE_REGM_HOST_PROM_CLOCK,
	0
};

/*
 * Convert cell rate to ATM Forum format
 */
u_int
hatm_cps2atmf(uint32_t pcr)
{
	u_int e;

	if (pcr == 0)
		return (0);
	pcr <<= 9;
	e = 0;
	while (pcr > (1024 - 1)) {
		e++;
		pcr >>= 1;
	}
	return ((1 << 14) | (e << 9) | (pcr & 0x1ff));
}
u_int
hatm_atmf2cps(uint32_t fcr)
{
	fcr &= 0x7fff;

	return ((1 << ((fcr >> 9) & 0x1f)) * (512 + (fcr & 0x1ff)) / 512
	  * (fcr >> 14));
}

/************************************************************
 *
 * Initialisation
 */
/*
 * Probe for a HE controller
 */
static int
hatm_probe(device_t dev)
{
	int i;

	for (i = 0; hatm_devs[i].name; i++)
		if (pci_get_vendor(dev) == hatm_devs[i].vid &&
		    pci_get_device(dev) == hatm_devs[i].did) {
			device_set_desc(dev, hatm_devs[i].name);
			return (0);
		}
	return (ENXIO);
}

/*
 * Allocate and map DMA-able memory. We support only contiguous mappings.
 */
static void
dmaload_helper(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error)
		return;
	KASSERT(nsegs == 1, ("too many segments for DMA: %d", nsegs));
	KASSERT(segs[0].ds_addr <= 0xffffffffUL,
	    ("phys addr too large %lx", (u_long)segs[0].ds_addr));

	*(bus_addr_t *)arg = segs[0].ds_addr;
}
static int
hatm_alloc_dmamem(struct hatm_softc *sc, const char *what, struct dmamem *mem)
{
	int error;

	mem->base = NULL;

	/*
	 * Alignement does not work in the bus_dmamem_alloc function below
	 * on FreeBSD. malloc seems to align objects at least to the object
	 * size so increase the size to the alignment if the size is lesser
	 * than the alignemnt.
	 * XXX on sparc64 this is (probably) not needed.
	 */
	if (mem->size < mem->align)
		mem->size = mem->align;

	error = bus_dma_tag_create(sc->parent_tag, mem->align, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, mem->size, 1,
	    BUS_SPACE_MAXSIZE_32BIT, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &mem->tag);
	if (error) {
		if_printf(&sc->ifatm.ifnet, "DMA tag create (%s)\n", what);
		return (error);
	}

	error = bus_dmamem_alloc(mem->tag, &mem->base, 0, &mem->map);
	if (error) {
		if_printf(&sc->ifatm.ifnet, "DMA mem alloc (%s): %d\n",
		    what, error);
		bus_dma_tag_destroy(mem->tag);
		mem->base = NULL;
		return (error);
	}

	error = bus_dmamap_load(mem->tag, mem->map, mem->base, mem->size,
	    dmaload_helper, &mem->paddr, BUS_DMA_NOWAIT);
	if (error) {
		if_printf(&sc->ifatm.ifnet, "DMA map load (%s): %d\n",
		    what, error);
		bus_dmamem_free(mem->tag, mem->base, mem->map);
		bus_dma_tag_destroy(mem->tag);
		mem->base = NULL;
		return (error);
	}

	DBG(sc, DMA, ("%s S/A/V/P 0x%x 0x%x %p 0x%lx", what, mem->size,
	    mem->align, mem->base, (u_long)mem->paddr));

	return (0);
}

/*
 * Destroy all the resources of an DMA-able memory region.
 */
static void
hatm_destroy_dmamem(struct dmamem *mem)
{
	if (mem->base != NULL) {
		bus_dmamap_unload(mem->tag, mem->map);
		bus_dmamem_free(mem->tag, mem->base, mem->map);
		(void)bus_dma_tag_destroy(mem->tag);
		mem->base = NULL;
	}
}

/*
 * Initialize/destroy DMA maps for the large pool 0
 */
static void
hatm_destroy_rmaps(struct hatm_softc *sc)
{
	u_int b;

	DBG(sc, ATTACH, ("destroying rmaps and lbuf pointers..."));
	if (sc->rmaps != NULL) {
		for (b = 0; b < sc->lbufs_size; b++)
			bus_dmamap_destroy(sc->mbuf_tag, sc->rmaps[b]);
		free(sc->rmaps, M_DEVBUF);
	}
	if (sc->lbufs != NULL)
		free(sc->lbufs, M_DEVBUF);
}

static void
hatm_init_rmaps(struct hatm_softc *sc)
{
	u_int b;
	int err;

	DBG(sc, ATTACH, ("allocating rmaps and lbuf pointers..."));
	sc->lbufs = malloc(sizeof(sc->lbufs[0]) * sc->lbufs_size,
	    M_DEVBUF, M_ZERO | M_WAITOK);

	/* allocate and create the DMA maps for the large pool */
	sc->rmaps = malloc(sizeof(sc->rmaps[0]) * sc->lbufs_size,
	    M_DEVBUF, M_WAITOK);
	for (b = 0; b < sc->lbufs_size; b++) {
		err = bus_dmamap_create(sc->mbuf_tag, 0, &sc->rmaps[b]);
		if (err != 0)
			panic("bus_dmamap_create: %d\n", err);
	}
}

/*
 * Initialize and destroy small mbuf page pointers and pages
 */
static void
hatm_destroy_smbufs(struct hatm_softc *sc)
{
	u_int i, b;
	struct mbuf_page *pg;

	if (sc->mbuf_pages != NULL) {
		for (i = 0; i < sc->mbuf_npages; i++) {
			pg = sc->mbuf_pages[i];
			for (b = 0; b < pg->hdr.nchunks; b++) {
				if (MBUF_TST_BIT(pg->hdr.card, b))
					if_printf(&sc->ifatm.ifnet,
					    "%s -- mbuf page=%u card buf %u\n",
					    __func__, i, b);
				if (MBUF_TST_BIT(pg->hdr.used, b))
					if_printf(&sc->ifatm.ifnet,
					    "%s -- mbuf page=%u used buf %u\n",
					    __func__, i, b);
			}
			bus_dmamap_unload(sc->mbuf_tag, pg->hdr.map);
			bus_dmamap_destroy(sc->mbuf_tag, pg->hdr.map);
			free(pg, M_DEVBUF);
		}
		free(sc->mbuf_pages, M_DEVBUF);
	}
}

static void
hatm_init_smbufs(struct hatm_softc *sc)
{
	sc->mbuf_pages = malloc(sizeof(sc->mbuf_pages[0]) *
	    HE_CONFIG_MAX_MBUF_PAGES, M_DEVBUF, M_WAITOK);
	sc->mbuf_npages = 0;
}

/*
 * Initialize/destroy TPDs. This is called from attach/detach.
 */
static void
hatm_destroy_tpds(struct hatm_softc *sc)
{
	struct tpd *t;

	if (sc->tpds.base == NULL)
		return;

	DBG(sc, ATTACH, ("releasing TPDs ..."));
	if (sc->tpd_nfree != sc->tpd_total)
		if_printf(&sc->ifatm.ifnet, "%u tpds still in use from %u\n",
		    sc->tpd_total - sc->tpd_nfree, sc->tpd_total);
	while ((t = SLIST_FIRST(&sc->tpd_free)) != NULL) {
		SLIST_REMOVE_HEAD(&sc->tpd_free, link);
		bus_dmamap_destroy(sc->tx_tag, t->map);
	}
	hatm_destroy_dmamem(&sc->tpds);
	free(sc->tpd_used, M_DEVBUF);
	DBG(sc, ATTACH, ("... done"));
}
static int
hatm_init_tpds(struct hatm_softc *sc)
{
	int error;
	u_int i;
	struct tpd *t;

	DBG(sc, ATTACH, ("allocating %u TPDs and maps ...", sc->tpd_total));
	error = hatm_alloc_dmamem(sc, "TPD memory", &sc->tpds);
	if (error != 0) {
		DBG(sc, ATTACH, ("... dmamem error=%d", error));
		return (error);
	}

	/* put all the TPDs on the free list and allocate DMA maps */
	for (i = 0; i < sc->tpd_total; i++) {
		t = TPD_ADDR(sc, i);
		t->no = i;
		t->mbuf = NULL;
		error = bus_dmamap_create(sc->tx_tag, 0, &t->map);
		if (error != 0) {
			DBG(sc, ATTACH, ("... dmamap error=%d", error));
			while ((t = SLIST_FIRST(&sc->tpd_free)) != NULL) {
				SLIST_REMOVE_HEAD(&sc->tpd_free, link);
				bus_dmamap_destroy(sc->tx_tag, t->map);
			}
			hatm_destroy_dmamem(&sc->tpds);
			return (error);
		}

		SLIST_INSERT_HEAD(&sc->tpd_free, t, link);
	}

	/* allocate and zero bitmap */
	sc->tpd_used = malloc(sizeof(uint8_t) * (sc->tpd_total + 7) / 8,
	    M_DEVBUF, M_ZERO | M_WAITOK);
	sc->tpd_nfree = sc->tpd_total;

	DBG(sc, ATTACH, ("... done"));

	return (0);
}

/*
 * Free all the TPDs that where given to the card. 
 * An mbuf chain may be attached to a TPD - free it also and
 * unload its associated DMA map.
 */
static void
hatm_stop_tpds(struct hatm_softc *sc)
{
	u_int i;
	struct tpd *t;

	DBG(sc, ATTACH, ("free TPDs ..."));
	for (i = 0; i < sc->tpd_total; i++) {
		if (TPD_TST_USED(sc, i)) {
			t = TPD_ADDR(sc, i);
			if (t->mbuf) {
				m_freem(t->mbuf);
				t->mbuf = NULL;
				bus_dmamap_unload(sc->tx_tag, t->map);
			}
			TPD_CLR_USED(sc, i);
			SLIST_INSERT_HEAD(&sc->tpd_free, t, link);
			sc->tpd_nfree++;
		}
	}
}

/*
 * This frees ALL resources of this interface and leaves the structure
 * in an indeterminate state. This is called just before detaching or
 * on a failed attach. No lock should be held.
 */
static void
hatm_destroy(struct hatm_softc *sc)
{
	bus_teardown_intr(sc->dev, sc->irqres, sc->ih);

	hatm_destroy_rmaps(sc);
	hatm_destroy_smbufs(sc);
	hatm_destroy_tpds(sc);

	if (sc->vcc_zone != NULL)
		uma_zdestroy(sc->vcc_zone);

	/*
	 * Release all memory allocated to the various queues and
	 * Status pages. These have there own flag which shows whether
	 * they are really allocated.
	 */
	hatm_destroy_dmamem(&sc->irq_0.mem);
	hatm_destroy_dmamem(&sc->rbp_s0.mem);
	hatm_destroy_dmamem(&sc->rbp_l0.mem);
	hatm_destroy_dmamem(&sc->rbp_s1.mem);
	hatm_destroy_dmamem(&sc->rbrq_0.mem);
	hatm_destroy_dmamem(&sc->rbrq_1.mem);
	hatm_destroy_dmamem(&sc->tbrq.mem);
	hatm_destroy_dmamem(&sc->tpdrq.mem);
	hatm_destroy_dmamem(&sc->hsp_mem);

	if (sc->irqres != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ,
		    sc->irqid, sc->irqres);

	if (sc->tx_tag != NULL)
		if (bus_dma_tag_destroy(sc->tx_tag))
			if_printf(&sc->ifatm.ifnet, "mbuf DMA tag busy\n");

	if (sc->mbuf_tag != NULL)
		if (bus_dma_tag_destroy(sc->mbuf_tag))
			if_printf(&sc->ifatm.ifnet, "mbuf DMA tag busy\n");

	if (sc->parent_tag != NULL)
		if (bus_dma_tag_destroy(sc->parent_tag))
			if_printf(&sc->ifatm.ifnet, "parent DMA tag busy\n");

	if (sc->memres != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    sc->memid, sc->memres);

	sysctl_ctx_free(&sc->sysctl_ctx);

	cv_destroy(&sc->cv_rcclose);
	cv_destroy(&sc->vcc_cv);
	mtx_destroy(&sc->mbuf0_mtx);
	mtx_destroy(&sc->mbuf1_mtx);
	mtx_destroy(&sc->mtx);
}

/*
 * 4.4 Card reset
 */
static int
hatm_reset(struct hatm_softc *sc)
{
	u_int v, count;

	WRITE4(sc, HE_REGO_RESET_CNTL, 0x00);
	BARRIER_W(sc);
	WRITE4(sc, HE_REGO_RESET_CNTL, 0xff);
	BARRIER_RW(sc);
	count = 0;
	while (((v = READ4(sc, HE_REGO_RESET_CNTL)) & HE_REGM_RESET_STATE) == 0) {
		BARRIER_R(sc);
		if (++count == 100) {
			if_printf(&sc->ifatm.ifnet, "reset failed\n");
			return (ENXIO);
		}
		DELAY(1000);
	}
	return (0);
}

/*
 * 4.5 Set Bus Width
 */
static void
hatm_init_bus_width(struct hatm_softc *sc)
{
	uint32_t v, v1;

	v = READ4(sc, HE_REGO_HOST_CNTL);
	BARRIER_R(sc);
	if (v & HE_REGM_HOST_BUS64) {
		sc->pci64 = 1;
		v1 = pci_read_config(sc->dev, HE_PCIR_GEN_CNTL_0, 4);
		v1 |= HE_PCIM_CTL0_64BIT;
		pci_write_config(sc->dev, HE_PCIR_GEN_CNTL_0, v1, 4);

		v |= HE_REGM_HOST_DESC_RD64
		    | HE_REGM_HOST_DATA_RD64
		    | HE_REGM_HOST_DATA_WR64;
		WRITE4(sc, HE_REGO_HOST_CNTL, v);
		BARRIER_W(sc);
	} else {
		sc->pci64 = 0;
		v = pci_read_config(sc->dev, HE_PCIR_GEN_CNTL_0, 4);
		v &= ~HE_PCIM_CTL0_64BIT;
		pci_write_config(sc->dev, HE_PCIR_GEN_CNTL_0, v, 4);
	}
}

/*
 * 4.6 Set Host Endianess
 */
static void
hatm_init_endianess(struct hatm_softc *sc)
{
	uint32_t v;

	v = READ4(sc, HE_REGO_LB_SWAP);
	BARRIER_R(sc);
#if BYTE_ORDER == BIG_ENDIAN
	v |= HE_REGM_LBSWAP_INTR_SWAP |
	    HE_REGM_LBSWAP_DESC_WR_SWAP |
	    HE_REGM_LBSWAP_BIG_ENDIAN;
	v &= ~(HE_REGM_LBSWAP_DATA_WR_SWAP |
	    HE_REGM_LBSWAP_DESC_RD_SWAP |
	    HE_REGM_LBSWAP_DATA_RD_SWAP);
#else
	v &= ~(HE_REGM_LBSWAP_DATA_WR_SWAP |
	    HE_REGM_LBSWAP_DESC_RD_SWAP |
	    HE_REGM_LBSWAP_DATA_RD_SWAP |
	    HE_REGM_LBSWAP_INTR_SWAP |
	    HE_REGM_LBSWAP_DESC_WR_SWAP |
	    HE_REGM_LBSWAP_BIG_ENDIAN);
#endif

	if (sc->he622)
		v |= HE_REGM_LBSWAP_XFER_SIZE;

	WRITE4(sc, HE_REGO_LB_SWAP, v);
	BARRIER_W(sc);
}

/*
 * 4.7 Read EEPROM
 */
static uint8_t
hatm_read_prom_byte(struct hatm_softc *sc, u_int addr)
{
	uint32_t val, tmp_read, byte_read;
	u_int i, j;
	int n;

	val = READ4(sc, HE_REGO_HOST_CNTL);
	val &= HE_REGM_HOST_PROM_BITS;
	BARRIER_R(sc);

	val |= HE_REGM_HOST_PROM_WREN;
	WRITE4(sc, HE_REGO_HOST_CNTL, val);
	BARRIER_W(sc);

	/* send READ */
	for (i = 0; i < sizeof(readtab) / sizeof(readtab[0]); i++) {
		WRITE4(sc, HE_REGO_HOST_CNTL, val | readtab[i]);
		BARRIER_W(sc);
		DELAY(EEPROM_DELAY);
	}

	/* send ADDRESS */
	for (n = 7, j = 0; n >= 0; n--) {
		WRITE4(sc, HE_REGO_HOST_CNTL, val | clocktab[j++] |
		    (((addr >> n) & 1 ) << HE_REGS_HOST_PROM_DATA_IN));
		BARRIER_W(sc);
		DELAY(EEPROM_DELAY);
		WRITE4(sc, HE_REGO_HOST_CNTL, val | clocktab[j++] |
		    (((addr >> n) & 1 ) << HE_REGS_HOST_PROM_DATA_IN));
		BARRIER_W(sc);
		DELAY(EEPROM_DELAY);
	}

	val &= ~HE_REGM_HOST_PROM_WREN;
	WRITE4(sc, HE_REGO_HOST_CNTL, val);
	BARRIER_W(sc);

	/* read DATA */
	byte_read = 0;
	for (n = 7, j = 0; n >= 0; n--) {
		WRITE4(sc, HE_REGO_HOST_CNTL, val | clocktab[j++]);
		BARRIER_W(sc);
		DELAY(EEPROM_DELAY);
		tmp_read = READ4(sc, HE_REGO_HOST_CNTL);
		byte_read |= (uint8_t)(((tmp_read & HE_REGM_HOST_PROM_DATA_OUT)
				>> HE_REGS_HOST_PROM_DATA_OUT) << n);
		WRITE4(sc, HE_REGO_HOST_CNTL, val | clocktab[j++]);
		BARRIER_W(sc);
		DELAY(EEPROM_DELAY);
	}
	WRITE4(sc, HE_REGO_HOST_CNTL, val | clocktab[j++]);
	BARRIER_W(sc);
	DELAY(EEPROM_DELAY);

	return (byte_read);
}

static void
hatm_init_read_eeprom(struct hatm_softc *sc)
{
	u_int n, count;
	u_char byte;
	uint32_t v;

	for (n = count = 0; count < HE_EEPROM_PROD_ID_LEN; count++) {
		byte = hatm_read_prom_byte(sc, HE_EEPROM_PROD_ID + count);
		if (n > 0 || byte != ' ')
			sc->prod_id[n++] = byte;
	}
	while (n > 0 && sc->prod_id[n-1] == ' ')
		n--;
	sc->prod_id[n] = '\0';

	for (n = count = 0; count < HE_EEPROM_REV_LEN; count++) {
		byte = hatm_read_prom_byte(sc, HE_EEPROM_REV + count);
		if (n > 0 || byte != ' ')
			sc->rev[n++] = byte;
	}
	while (n > 0 && sc->rev[n-1] == ' ')
		n--;
	sc->rev[n] = '\0';
	sc->ifatm.mib.hw_version = sc->rev[0];

	sc->ifatm.mib.serial =  hatm_read_prom_byte(sc, HE_EEPROM_M_SN + 0) << 0;
	sc->ifatm.mib.serial |= hatm_read_prom_byte(sc, HE_EEPROM_M_SN + 1) << 8;
	sc->ifatm.mib.serial |= hatm_read_prom_byte(sc, HE_EEPROM_M_SN + 2) << 16;
	sc->ifatm.mib.serial |= hatm_read_prom_byte(sc, HE_EEPROM_M_SN + 3) << 24;

	v =  hatm_read_prom_byte(sc, HE_EEPROM_MEDIA + 0) << 0;
	v |= hatm_read_prom_byte(sc, HE_EEPROM_MEDIA + 1) << 8;
	v |= hatm_read_prom_byte(sc, HE_EEPROM_MEDIA + 2) << 16;
	v |= hatm_read_prom_byte(sc, HE_EEPROM_MEDIA + 3) << 24;

	switch (v) {
	  case HE_MEDIA_UTP155:
		sc->ifatm.mib.media = IFM_ATM_UTP_155;
		sc->ifatm.mib.pcr = ATM_RATE_155M;
		break;

	  case HE_MEDIA_MMF155:
		sc->ifatm.mib.media = IFM_ATM_MM_155;
		sc->ifatm.mib.pcr = ATM_RATE_155M;
		break;

	  case HE_MEDIA_MMF622:
		sc->ifatm.mib.media = IFM_ATM_MM_622;
		sc->ifatm.mib.device = ATM_DEVICE_HE622;
		sc->ifatm.mib.pcr = ATM_RATE_622M;
		sc->he622 = 1;
		break;

	  case HE_MEDIA_SMF155:
		sc->ifatm.mib.media = IFM_ATM_SM_155;
		sc->ifatm.mib.pcr = ATM_RATE_155M;
		break;

	  case HE_MEDIA_SMF622:
		sc->ifatm.mib.media = IFM_ATM_SM_622;
		sc->ifatm.mib.device = ATM_DEVICE_HE622;
		sc->ifatm.mib.pcr = ATM_RATE_622M;
		sc->he622 = 1;
		break;
	}

	sc->ifatm.mib.esi[0] = hatm_read_prom_byte(sc, HE_EEPROM_MAC + 0);
	sc->ifatm.mib.esi[1] = hatm_read_prom_byte(sc, HE_EEPROM_MAC + 1);
	sc->ifatm.mib.esi[2] = hatm_read_prom_byte(sc, HE_EEPROM_MAC + 2);
	sc->ifatm.mib.esi[3] = hatm_read_prom_byte(sc, HE_EEPROM_MAC + 3);
	sc->ifatm.mib.esi[4] = hatm_read_prom_byte(sc, HE_EEPROM_MAC + 4);
	sc->ifatm.mib.esi[5] = hatm_read_prom_byte(sc, HE_EEPROM_MAC + 5);
}

/*
 * Clear unused interrupt queue
 */
static void
hatm_clear_irq(struct hatm_softc *sc, u_int group)
{
	WRITE4(sc, HE_REGO_IRQ_BASE(group), 0);
	WRITE4(sc, HE_REGO_IRQ_HEAD(group), 0);
	WRITE4(sc, HE_REGO_IRQ_CNTL(group), 0);
	WRITE4(sc, HE_REGO_IRQ_DATA(group), 0);
}

/*
 * 4.10 Initialize interrupt queues
 */
static void
hatm_init_irq(struct hatm_softc *sc, struct heirq *q, u_int group)
{
	u_int i;

	if (q->size == 0) {
		hatm_clear_irq(sc, group);
		return;
	}

	q->group = group;
	q->sc = sc;
	q->irq = q->mem.base;
	q->head = 0;
	q->tailp = q->irq + (q->size - 1);
	*q->tailp = 0;

	for (i = 0; i < q->size; i++)
		q->irq[i] = HE_REGM_ITYPE_INVALID;

	WRITE4(sc, HE_REGO_IRQ_BASE(group), q->mem.paddr);
	WRITE4(sc, HE_REGO_IRQ_HEAD(group),
	    ((q->size - 1) << HE_REGS_IRQ_HEAD_SIZE) |
	    (q->thresh << HE_REGS_IRQ_HEAD_THRESH));
	WRITE4(sc, HE_REGO_IRQ_CNTL(group), q->line);
	WRITE4(sc, HE_REGO_IRQ_DATA(group), 0);
}

/*
 * 5.1.3 Initialize connection memory
 */
static void
hatm_init_cm(struct hatm_softc *sc)
{
	u_int rsra, mlbm, rabr, numbuffs;
	u_int tsra, tabr, mtpd;
	u_int n;

	for (n = 0; n < HE_CONFIG_TXMEM; n++)
		WRITE_TCM4(sc, n, 0);
	for (n = 0; n < HE_CONFIG_RXMEM; n++)
		WRITE_RCM4(sc, n, 0);

	numbuffs = sc->r0_numbuffs + sc->r1_numbuffs + sc->tx_numbuffs;

	rsra = 0;
	mlbm = ((rsra + sc->ifatm.mib.max_vccs * 8) + 0x7ff) & ~0x7ff;
	rabr = ((mlbm + numbuffs * 2) + 0x7ff) & ~0x7ff;
	sc->rsrb = ((rabr + 2048) + (2 * sc->ifatm.mib.max_vccs - 1)) &
	    ~(2 * sc->ifatm.mib.max_vccs - 1);

	tsra = 0;
	sc->tsrb = tsra + sc->ifatm.mib.max_vccs * 8;
	sc->tsrc = sc->tsrb + sc->ifatm.mib.max_vccs * 4;
	sc->tsrd = sc->tsrc + sc->ifatm.mib.max_vccs * 2;
	tabr = sc->tsrd + sc->ifatm.mib.max_vccs * 1;
	mtpd = ((tabr + 1024) + (16 * sc->ifatm.mib.max_vccs - 1)) &
	    ~(16 * sc->ifatm.mib.max_vccs - 1);

	DBG(sc, ATTACH, ("rsra=%x mlbm=%x rabr=%x rsrb=%x",
	    rsra, mlbm, rabr, sc->rsrb));
	DBG(sc, ATTACH, ("tsra=%x tsrb=%x tsrc=%x tsrd=%x tabr=%x mtpd=%x",
	    tsra, sc->tsrb, sc->tsrc, sc->tsrd, tabr, mtpd));

	WRITE4(sc, HE_REGO_TSRB_BA, sc->tsrb);
	WRITE4(sc, HE_REGO_TSRC_BA, sc->tsrc);
	WRITE4(sc, HE_REGO_TSRD_BA, sc->tsrd);
	WRITE4(sc, HE_REGO_TMABR_BA, tabr);
	WRITE4(sc, HE_REGO_TPD_BA, mtpd);

	WRITE4(sc, HE_REGO_RCMRSRB_BA, sc->rsrb);
	WRITE4(sc, HE_REGO_RCMLBM_BA, mlbm);
	WRITE4(sc, HE_REGO_RCMABR_BA, rabr);

	BARRIER_W(sc);
}

/*
 * 5.1.4 Initialize Local buffer Pools
 */
static void
hatm_init_rx_buffer_pool(struct hatm_softc *sc,
	u_int num,		/* bank */
	u_int start,		/* start row */
	u_int numbuffs		/* number of entries */
)
{
	u_int row_size;		/* bytes per row */
	uint32_t row_addr;	/* start address of this row */
	u_int lbuf_size;	/* bytes per lbuf */
	u_int lbufs_per_row;	/* number of lbufs per memory row */
	uint32_t lbufd_index;	/* index of lbuf descriptor */
	uint32_t lbufd_addr;	/* address of lbuf descriptor */
	u_int lbuf_row_cnt;	/* current lbuf in current row */
	uint32_t lbuf_addr;	/* address of current buffer */
	u_int i;

	row_size = sc->bytes_per_row;;
	row_addr = start * row_size;
	lbuf_size = sc->cells_per_lbuf * 48;
	lbufs_per_row = sc->cells_per_row / sc->cells_per_lbuf;

	/* descriptor index */
	lbufd_index = num;

	/* 2 words per entry */
	lbufd_addr = READ4(sc, HE_REGO_RCMLBM_BA) + lbufd_index * 2;

	/* write head of queue */
	WRITE4(sc, HE_REGO_RLBF_H(num), lbufd_index);

	lbuf_row_cnt = 0;
	for (i = 0; i < numbuffs; i++) {
		lbuf_addr = (row_addr + lbuf_row_cnt * lbuf_size) / 32;

		WRITE_RCM4(sc, lbufd_addr, lbuf_addr);

		lbufd_index += 2;
		WRITE_RCM4(sc, lbufd_addr + 1, lbufd_index);

		if (++lbuf_row_cnt == lbufs_per_row) {
			lbuf_row_cnt = 0;
			row_addr += row_size;
		}

		lbufd_addr += 2 * 2;
	}

	WRITE4(sc, HE_REGO_RLBF_T(num), lbufd_index - 2);
	WRITE4(sc, HE_REGO_RLBF_C(num), numbuffs);

	BARRIER_W(sc);
}

static void
hatm_init_tx_buffer_pool(struct hatm_softc *sc,
	u_int start,		/* start row */
	u_int numbuffs		/* number of entries */
)
{
	u_int row_size;		/* bytes per row */
	uint32_t row_addr;	/* start address of this row */
	u_int lbuf_size;	/* bytes per lbuf */
	u_int lbufs_per_row;	/* number of lbufs per memory row */
	uint32_t lbufd_index;	/* index of lbuf descriptor */
	uint32_t lbufd_addr;	/* address of lbuf descriptor */
	u_int lbuf_row_cnt;	/* current lbuf in current row */
	uint32_t lbuf_addr;	/* address of current buffer */
	u_int i;

	row_size = sc->bytes_per_row;;
	row_addr = start * row_size;
	lbuf_size = sc->cells_per_lbuf * 48;
	lbufs_per_row = sc->cells_per_row / sc->cells_per_lbuf;

	/* descriptor index */
	lbufd_index = sc->r0_numbuffs + sc->r1_numbuffs;

	/* 2 words per entry */
	lbufd_addr = READ4(sc, HE_REGO_RCMLBM_BA) + lbufd_index * 2;

	/* write head of queue */
	WRITE4(sc, HE_REGO_TLBF_H, lbufd_index);

	lbuf_row_cnt = 0;
	for (i = 0; i < numbuffs; i++) {
		lbuf_addr = (row_addr + lbuf_row_cnt * lbuf_size) / 32;

		WRITE_RCM4(sc, lbufd_addr, lbuf_addr);
		lbufd_index++;
		WRITE_RCM4(sc, lbufd_addr + 1, lbufd_index);

		if (++lbuf_row_cnt == lbufs_per_row) {
			lbuf_row_cnt = 0;
			row_addr += row_size;
		}

		lbufd_addr += 2;
	}

	WRITE4(sc, HE_REGO_TLBF_T, lbufd_index - 1);
	BARRIER_W(sc);
}

/*
 * 5.1.5 Initialize Intermediate Receive Queues
 */
static void
hatm_init_imed_queues(struct hatm_softc *sc)
{
	u_int n;

	if (sc->he622) {
		for (n = 0; n < 8; n++) {
			WRITE4(sc, HE_REGO_INMQ_S(n), 0x10*n+0x000f);
			WRITE4(sc, HE_REGO_INMQ_L(n), 0x10*n+0x200f);
		}
	} else {
		for (n = 0; n < 8; n++) {
			WRITE4(sc, HE_REGO_INMQ_S(n), n);
			WRITE4(sc, HE_REGO_INMQ_L(n), n+0x8);
		}
	}
}

/*
 * 5.1.7 Init CS block
 */
static void
hatm_init_cs_block(struct hatm_softc *sc)
{
	u_int n, i;
	u_int clkfreg, cellrate, decr, tmp;
	static const uint32_t erthr[2][5][3] = HE_REGT_CS_ERTHR;
	static const uint32_t erctl[2][3] = HE_REGT_CS_ERCTL;
	static const uint32_t erstat[2][2] = HE_REGT_CS_ERSTAT;
	static const uint32_t rtfwr[2] = HE_REGT_CS_RTFWR;
	static const uint32_t rtatr[2] = HE_REGT_CS_RTATR;
	static const uint32_t bwalloc[2][6] = HE_REGT_CS_BWALLOC;
	static const uint32_t orcf[2][2] = HE_REGT_CS_ORCF;

	/* Clear Rate Controller Start Times and Occupied Flags */
	for (n = 0; n < 32; n++)
		WRITE_MBOX4(sc, HE_REGO_CS_STTIM(n), 0);

	clkfreg = sc->he622 ? HE_622_CLOCK : HE_155_CLOCK;
	cellrate = sc->he622 ? ATM_RATE_622M : ATM_RATE_155M;
	decr = cellrate / 32;

	for (n = 0; n < 16; n++) {
		tmp = clkfreg / cellrate;
		WRITE_MBOX4(sc, HE_REGO_CS_TGRLD(n), tmp - 1);
		cellrate -= decr;
	}

	i = (sc->cells_per_lbuf == 2) ? 0
	   :(sc->cells_per_lbuf == 4) ? 1
	   :                            2;

	/* table 5.2 */
	WRITE_MBOX4(sc, HE_REGO_CS_ERTHR0, erthr[sc->he622][0][i]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERTHR1, erthr[sc->he622][1][i]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERTHR2, erthr[sc->he622][2][i]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERTHR3, erthr[sc->he622][3][i]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERTHR4, erthr[sc->he622][4][i]);

	WRITE_MBOX4(sc, HE_REGO_CS_ERCTL0, erctl[sc->he622][0]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERCTL1, erctl[sc->he622][1]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERCTL2, erctl[sc->he622][2]);

	WRITE_MBOX4(sc, HE_REGO_CS_ERSTAT0, erstat[sc->he622][0]);
	WRITE_MBOX4(sc, HE_REGO_CS_ERSTAT1, erstat[sc->he622][1]);

	WRITE_MBOX4(sc, HE_REGO_CS_RTFWR, rtfwr[sc->he622]);
	WRITE_MBOX4(sc, HE_REGO_CS_RTATR, rtatr[sc->he622]);

	WRITE_MBOX4(sc, HE_REGO_CS_TFBSET, bwalloc[sc->he622][0]);
	WRITE_MBOX4(sc, HE_REGO_CS_WCRMAX, bwalloc[sc->he622][1]);
	WRITE_MBOX4(sc, HE_REGO_CS_WCRMIN, bwalloc[sc->he622][2]);
	WRITE_MBOX4(sc, HE_REGO_CS_WCRINC, bwalloc[sc->he622][3]);
	WRITE_MBOX4(sc, HE_REGO_CS_WCRDEC, bwalloc[sc->he622][4]);
	WRITE_MBOX4(sc, HE_REGO_CS_WCRCEIL, bwalloc[sc->he622][5]);

	WRITE_MBOX4(sc, HE_REGO_CS_OTPPER, orcf[sc->he622][0]);
	WRITE_MBOX4(sc, HE_REGO_CS_OTWPER, orcf[sc->he622][1]);

	WRITE_MBOX4(sc, HE_REGO_CS_OTTLIM, 8);

	for (n = 0; n < 8; n++)
		WRITE_MBOX4(sc, HE_REGO_CS_HGRRT(n), 0);
}

/*
 * 5.1.8 CS Block Connection Memory Initialisation
 */
static void
hatm_init_cs_block_cm(struct hatm_softc *sc)
{
	u_int n, i;
	u_int expt, mant, etrm, wcr, ttnrm, tnrm;
	uint32_t rate;
	uint32_t clkfreq, cellrate, decr;
	uint32_t *rg, rtg, val = 0;
	uint64_t drate;
	u_int buf, buf_limit;
	uint32_t base = READ4(sc, HE_REGO_RCMABR_BA);

	for (n = 0; n < HE_REGL_CM_GQTBL; n++)
		WRITE_RCM4(sc, base + HE_REGO_CM_GQTBL + n, 0);
	for (n = 0; n < HE_REGL_CM_RGTBL; n++)
		WRITE_RCM4(sc, base + HE_REGO_CM_RGTBL + n, 0);

	tnrm = 0;
	for (n = 0; n < HE_REGL_CM_TNRMTBL * 4; n++) {
		expt = (n >> 5) & 0x1f;
		mant = ((n & 0x18) << 4) | 0x7f;
		wcr = (1 << expt) * (mant + 512) / 512;
		etrm = n & 0x7;
		ttnrm = wcr / 10 / (1 << etrm);
		if (ttnrm > 255)
			ttnrm = 255;
		else if(ttnrm < 2)
			ttnrm = 2;
		tnrm = (tnrm << 8) | (ttnrm & 0xff);
		if (n % 4 == 0)
			WRITE_RCM4(sc, base + HE_REGO_CM_TNRMTBL + (n/4), tnrm);
	}

	clkfreq = sc->he622 ? HE_622_CLOCK : HE_155_CLOCK;
	buf_limit = 4;

	cellrate = sc->he622 ? ATM_RATE_622M : ATM_RATE_155M;
	decr = cellrate / 32;

	/* compute GRID top row in 1000 * cps */
	for (n = 0; n < 16; n++) {
		u_int interval = clkfreq / cellrate;
		sc->rate_grid[0][n] = (u_int64_t)clkfreq * 1000 / interval;
		cellrate -= decr;
	}

	/* compute the other rows according to 2.4 */
	for (i = 1; i < 16; i++)
		for (n = 0; n < 16; n++)
			sc->rate_grid[i][n] = sc->rate_grid[i-1][n] /
			    ((i < 14) ? 2 : 4);

	/* first entry is line rate */
	n = hatm_cps2atmf(sc->he622 ? ATM_RATE_622M : ATM_RATE_155M);
	expt = (n >> 9) & 0x1f;
	mant = n & 0x1f0;
	sc->rate_grid[0][0] = (u_int64_t)(1<<expt) * 1000 * (mant+512) / 512;

	/* now build the conversion table - each 32 bit word contains
	 * two entries - this gives a total of 0x400 16 bit entries.
	 * This table maps the truncated ATMF rate version into a grid index */
	cellrate = sc->he622 ? ATM_RATE_622M : ATM_RATE_155M;
	rg = &sc->rate_grid[15][15];

	for (rate = 0; rate < 2 * HE_REGL_CM_RTGTBL; rate++) {
		/* unpack the ATMF rate */
		expt = rate >> 5;
		mant = (rate & 0x1f) << 4;

		/* get the cell rate - minimum is 10 per second */
		drate = (uint64_t)(1 << expt) * 1000 * (mant + 512) / 512;
		if (drate < 10 * 1000)
			drate = 10 * 1000;

		/* now look up the grid index */
		while (drate >= *rg && rg-- > &sc->rate_grid[0][0])
			;
		rg++;
		rtg = rg - &sc->rate_grid[0][0];

		/* now compute the buffer limit */
		buf = drate * sc->tx_numbuffs / (cellrate * 2) / 1000;
		if (buf == 0)
			buf = 1;
		else if (buf > buf_limit)
			buf = buf_limit;

		/* make value */
		val = (val << 16) | (rtg << 8) | buf;

		/* write */
		if (rate % 2 == 1)
			WRITE_RCM4(sc, base + HE_REGO_CM_RTGTBL + rate/2, val);
	}
}

/*
 * Clear an unused receive group buffer pool
 */
static void
hatm_clear_rpool(struct hatm_softc *sc, u_int group, u_int large)
{
	WRITE4(sc, HE_REGO_RBP_S(large, group), 0);
	WRITE4(sc, HE_REGO_RBP_T(large, group), 0);
	WRITE4(sc, HE_REGO_RBP_QI(large, group), 1);
	WRITE4(sc, HE_REGO_RBP_BL(large, group), 0);
}

/*
 * Initialize a receive group buffer pool
 */
static void
hatm_init_rpool(struct hatm_softc *sc, struct herbp *q, u_int group,
    u_int large)
{
	if (q->size == 0) {
		hatm_clear_rpool(sc, group, large);
		return;
	}

	bzero(q->mem.base, q->mem.size);
	q->rbp = q->mem.base;
	q->head = q->tail = 0;

	DBG(sc, ATTACH, ("RBP%u%c=0x%lx", group, "SL"[large],
	    (u_long)q->mem.paddr));

	WRITE4(sc, HE_REGO_RBP_S(large, group), q->mem.paddr);
	WRITE4(sc, HE_REGO_RBP_T(large, group), 0);
	WRITE4(sc, HE_REGO_RBP_QI(large, group),
	    ((q->size - 1) << HE_REGS_RBP_SIZE) |
	    HE_REGM_RBP_INTR_ENB |
	    (q->thresh << HE_REGS_RBP_THRESH));
	WRITE4(sc, HE_REGO_RBP_BL(large, group), (q->bsize >> 2) & ~1);
}

/*
 * Clear an unused receive buffer return queue
 */
static void
hatm_clear_rbrq(struct hatm_softc *sc, u_int group)
{
	WRITE4(sc, HE_REGO_RBRQ_ST(group), 0);
	WRITE4(sc, HE_REGO_RBRQ_H(group), 0);
	WRITE4(sc, HE_REGO_RBRQ_Q(group), (1 << HE_REGS_RBRQ_THRESH));
	WRITE4(sc, HE_REGO_RBRQ_I(group), 0);
}

/*
 * Initialize receive buffer return queue
 */
static void
hatm_init_rbrq(struct hatm_softc *sc, struct herbrq *rq, u_int group)
{
	if (rq->size == 0) {
		hatm_clear_rbrq(sc, group);
		return;
	}

	rq->rbrq = rq->mem.base;
	rq->head = 0;

	DBG(sc, ATTACH, ("RBRQ%u=0x%lx", group, (u_long)rq->mem.paddr));

	WRITE4(sc, HE_REGO_RBRQ_ST(group), rq->mem.paddr);
	WRITE4(sc, HE_REGO_RBRQ_H(group), 0);
	WRITE4(sc, HE_REGO_RBRQ_Q(group),
	    (rq->thresh << HE_REGS_RBRQ_THRESH) |
	    ((rq->size - 1) << HE_REGS_RBRQ_SIZE));
	WRITE4(sc, HE_REGO_RBRQ_I(group),
	    (rq->tout << HE_REGS_RBRQ_TIME) |
	    (rq->pcnt << HE_REGS_RBRQ_COUNT));
}

/*
 * Clear an unused transmit buffer return queue N
 */
static void
hatm_clear_tbrq(struct hatm_softc *sc, u_int group)
{
	WRITE4(sc, HE_REGO_TBRQ_B_T(group), 0);
	WRITE4(sc, HE_REGO_TBRQ_H(group), 0);
	WRITE4(sc, HE_REGO_TBRQ_S(group), 0);
	WRITE4(sc, HE_REGO_TBRQ_THRESH(group), 1);
}

/*
 * Initialize transmit buffer return queue N
 */
static void
hatm_init_tbrq(struct hatm_softc *sc, struct hetbrq *tq, u_int group)
{
	if (tq->size == 0) {
		hatm_clear_tbrq(sc, group);
		return;
	}

	tq->tbrq = tq->mem.base;
	tq->head = 0;

	DBG(sc, ATTACH, ("TBRQ%u=0x%lx", group, (u_long)tq->mem.paddr));

	WRITE4(sc, HE_REGO_TBRQ_B_T(group), tq->mem.paddr);
	WRITE4(sc, HE_REGO_TBRQ_H(group), 0);
	WRITE4(sc, HE_REGO_TBRQ_S(group), tq->size - 1);
	WRITE4(sc, HE_REGO_TBRQ_THRESH(group), tq->thresh);
}

/*
 * Initialize TPDRQ
 */
static void
hatm_init_tpdrq(struct hatm_softc *sc)
{
	struct hetpdrq *tq;

	tq = &sc->tpdrq;
	tq->tpdrq = tq->mem.base;
	tq->tail = tq->head = 0;

	DBG(sc, ATTACH, ("TPDRQ=0x%lx", (u_long)tq->mem.paddr));

	WRITE4(sc, HE_REGO_TPDRQ_H, tq->mem.paddr);
	WRITE4(sc, HE_REGO_TPDRQ_T, 0);
	WRITE4(sc, HE_REGO_TPDRQ_S, tq->size - 1);
}

/*
 * Function can be called by the infrastructure to start the card.
 */
static void
hatm_init(void *p)
{
	struct hatm_softc *sc = p;

	mtx_lock(&sc->mtx);
	hatm_stop(sc);
	hatm_initialize(sc);
	mtx_unlock(&sc->mtx);
}

enum {
	CTL_ISTATS,
};

/*
 * Sysctl handler
 */
static int
hatm_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	uint32_t *ret;
	int error;
	size_t len;

	switch (arg2) {

	  case CTL_ISTATS:
		len = sizeof(sc->istats);
		break;

	  default:
		panic("bad control code");
	}

	ret = malloc(len, M_TEMP, M_WAITOK);
	mtx_lock(&sc->mtx);

	switch (arg2) {

	  case CTL_ISTATS:
		sc->istats.mcc += READ4(sc, HE_REGO_MCC);
		sc->istats.oec += READ4(sc, HE_REGO_OEC);
		sc->istats.dcc += READ4(sc, HE_REGO_DCC);
		sc->istats.cec += READ4(sc, HE_REGO_CEC);
		bcopy(&sc->istats, ret, sizeof(sc->istats));
		break;
	}
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, ret, len);
	free(ret, M_TEMP);

	return (error);
}

static int
kenv_getuint(struct hatm_softc *sc, const char *var,
    u_int *ptr, u_int def, int rw)
{
	char full[IFNAMSIZ + 3 + 20];
	char *val, *end;
	u_int u;

	*ptr = def;

	if (SYSCTL_ADD_UINT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, var, rw ? CTLFLAG_RW : CTLFLAG_RD, ptr, 0, "") == NULL)
		return (ENOMEM);

	snprintf(full, sizeof(full), "hw.%s.%s",
	    device_get_nameunit(sc->dev), var);

	if ((val = getenv(full)) == NULL)
		return (0);
	u = strtoul(val, &end, 0);
	if (end == val || *end != '\0') {
		freeenv(val);
		return (EINVAL);
	}
	if (bootverbose)
		if_printf(&sc->ifatm.ifnet, "%s=%u\n", full, u);
	*ptr = u;
	return (0);
}

/*
 * Set configurable parameters. Many of these are configurable via
 * kenv.
 */
static int
hatm_configure(struct hatm_softc *sc)
{
	/* Receive buffer pool 0 small */
	kenv_getuint(sc, "rbps0.size", &sc->rbp_s0.size,
	    HE_CONFIG_RBPS0_SIZE, 0);
	kenv_getuint(sc, "rbps0.thresh", &sc->rbp_s0.thresh,
	    HE_CONFIG_RBPS0_THRESH, 0);
	sc->rbp_s0.bsize = MBUF0_SIZE;

	/* Receive buffer pool 0 large */
	kenv_getuint(sc, "rbpl0.size", &sc->rbp_l0.size,
	    HE_CONFIG_RBPL0_SIZE, 0);
	kenv_getuint(sc, "rbpl0.thresh", &sc->rbp_l0.thresh,
	    HE_CONFIG_RBPL0_THRESH, 0);
	sc->rbp_l0.bsize = MCLBYTES - MBUFL_OFFSET;

	/* Receive buffer return queue 0 */
	kenv_getuint(sc, "rbrq0.size", &sc->rbrq_0.size,
	    HE_CONFIG_RBRQ0_SIZE, 0);
	kenv_getuint(sc, "rbrq0.thresh", &sc->rbrq_0.thresh,
	    HE_CONFIG_RBRQ0_THRESH, 0);
	kenv_getuint(sc, "rbrq0.tout", &sc->rbrq_0.tout,
	    HE_CONFIG_RBRQ0_TOUT, 0);
	kenv_getuint(sc, "rbrq0.pcnt", &sc->rbrq_0.pcnt,
	    HE_CONFIG_RBRQ0_PCNT, 0);

	/* Receive buffer pool 1 small */
	kenv_getuint(sc, "rbps1.size", &sc->rbp_s1.size,
	    HE_CONFIG_RBPS1_SIZE, 0);
	kenv_getuint(sc, "rbps1.thresh", &sc->rbp_s1.thresh,
	    HE_CONFIG_RBPS1_THRESH, 0);
	sc->rbp_s1.bsize = MBUF1_SIZE;

	/* Receive buffer return queue 1 */
	kenv_getuint(sc, "rbrq1.size", &sc->rbrq_1.size,
	    HE_CONFIG_RBRQ1_SIZE, 0);
	kenv_getuint(sc, "rbrq1.thresh", &sc->rbrq_1.thresh,
	    HE_CONFIG_RBRQ1_THRESH, 0);
	kenv_getuint(sc, "rbrq1.tout", &sc->rbrq_1.tout,
	    HE_CONFIG_RBRQ1_TOUT, 0);
	kenv_getuint(sc, "rbrq1.pcnt", &sc->rbrq_1.pcnt,
	    HE_CONFIG_RBRQ1_PCNT, 0);

	/* Interrupt queue 0 */
	kenv_getuint(sc, "irq0.size", &sc->irq_0.size,
	    HE_CONFIG_IRQ0_SIZE, 0);
	kenv_getuint(sc, "irq0.thresh", &sc->irq_0.thresh,
	    HE_CONFIG_IRQ0_THRESH, 0);
	sc->irq_0.line = HE_CONFIG_IRQ0_LINE;

	/* Transmit buffer return queue 0 */
	kenv_getuint(sc, "tbrq0.size", &sc->tbrq.size,
	    HE_CONFIG_TBRQ_SIZE, 0);
	kenv_getuint(sc, "tbrq0.thresh", &sc->tbrq.thresh,
	    HE_CONFIG_TBRQ_THRESH, 0);

	/* Transmit buffer ready queue */
	kenv_getuint(sc, "tpdrq.size", &sc->tpdrq.size,
	    HE_CONFIG_TPDRQ_SIZE, 0);
	/* Max TPDs per VCC */
	kenv_getuint(sc, "tpdmax", &sc->max_tpd,
	    HE_CONFIG_TPD_MAXCC, 0);

	return (0);
}

#ifdef HATM_DEBUG

/*
 * Get TSRs from connection memory
 */
static int
hatm_sysctl_tsr(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	int error, i, j;
	uint32_t *val;

	val = malloc(sizeof(uint32_t) * HE_MAX_VCCS * 15, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	for (i = 0; i < HE_MAX_VCCS; i++)
		for (j = 0; j <= 14; j++)
			val[15 * i + j] = READ_TSR(sc, i, j);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, val, sizeof(uint32_t) * HE_MAX_VCCS * 15);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	return (EPERM);
}

/*
 * Get TPDs from connection memory
 */
static int
hatm_sysctl_tpd(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	int error, i, j;
	uint32_t *val;

	val = malloc(sizeof(uint32_t) * HE_MAX_VCCS * 16, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	for (i = 0; i < HE_MAX_VCCS; i++)
		for (j = 0; j < 16; j++)
			val[16 * i + j] = READ_TCM4(sc, 16 * i + j);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, val, sizeof(uint32_t) * HE_MAX_VCCS * 16);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	return (EPERM);
}

/*
 * Get mbox registers
 */
static int
hatm_sysctl_mbox(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	int error, i;
	uint32_t *val;

	val = malloc(sizeof(uint32_t) * HE_REGO_CS_END, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	for (i = 0; i < HE_REGO_CS_END; i++)
		val[i] = READ_MBOX4(sc, i);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, val, sizeof(uint32_t) * HE_REGO_CS_END);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	return (EPERM);
}

/*
 * Get connection memory
 */
static int
hatm_sysctl_cm(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	int error, i;
	uint32_t *val;

	val = malloc(sizeof(uint32_t) * (HE_CONFIG_RXMEM + 1), M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	val[0] = READ4(sc, HE_REGO_RCMABR_BA);
	for (i = 0; i < HE_CONFIG_RXMEM; i++)
		val[i + 1] = READ_RCM4(sc, i);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, val, sizeof(uint32_t) * (HE_CONFIG_RXMEM + 1));
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	return (EPERM);
}

/*
 * Get local buffer memory
 */
static int
hatm_sysctl_lbmem(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	int error, i;
	uint32_t *val;
	u_int bytes = (1 << 21);

	val = malloc(bytes, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	for (i = 0; i < bytes / 4; i++)
		val[i] = READ_LB4(sc, i);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, val, bytes);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	return (EPERM);
}

/*
 * Get all card registers
 */
static int
hatm_sysctl_heregs(SYSCTL_HANDLER_ARGS)
{
	struct hatm_softc *sc = arg1;
	int error, i;
	uint32_t *val;

	val = malloc(HE_REGO_END, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	for (i = 0; i < HE_REGO_END; i += 4)
		val[i / 4] = READ4(sc, i);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, val, HE_REGO_END);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	return (EPERM);
}
#endif

/*
 * Suni register access
 */
/*
 * read at most n SUNI registers starting at reg into val
 */
static int
hatm_utopia_readregs(struct ifatm *ifatm, u_int reg, uint8_t *val, u_int *n)
{
	u_int i;
	struct hatm_softc *sc = (struct hatm_softc *)ifatm;

	if (reg >= (HE_REGO_SUNI_END - HE_REGO_SUNI) / 4)
		return (EINVAL);
	if (reg + *n > (HE_REGO_SUNI_END - HE_REGO_SUNI) / 4)
		*n = reg - (HE_REGO_SUNI_END - HE_REGO_SUNI) / 4;

	mtx_assert(&sc->mtx, MA_OWNED);
	for (i = 0; i < *n; i++)
		val[i] = READ4(sc, HE_REGO_SUNI + 4 * (reg + i));

	return (0);
}

/*
 * change the bits given by mask to them in val in register reg
 */
static int
hatm_utopia_writereg(struct ifatm *ifatm, u_int reg, u_int mask, u_int val)
{
	uint32_t regval;
	struct hatm_softc *sc = (struct hatm_softc *)ifatm;

	if (reg >= (HE_REGO_SUNI_END - HE_REGO_SUNI) / 4)
		return (EINVAL);

	mtx_assert(&sc->mtx, MA_OWNED);
	regval = READ4(sc, HE_REGO_SUNI + 4 * reg);
	regval = (regval & ~mask) | (val & mask);
	WRITE4(sc, HE_REGO_SUNI + 4 * reg, regval);

	return (0);
}

static struct utopia_methods hatm_utopia_methods = {
	hatm_utopia_readregs,
	hatm_utopia_writereg,
};

/*
 * Detach - if it is running, stop. Destroy.
 */
static int
hatm_detach(device_t dev)
{
	struct hatm_softc *sc = (struct hatm_softc *)device_get_softc(dev);

	mtx_lock(&sc->mtx);
	hatm_stop(sc);
	if (sc->utopia.state & UTP_ST_ATTACHED) {
		utopia_stop(&sc->utopia);
		utopia_detach(&sc->utopia);
	}
	mtx_unlock(&sc->mtx);

	atm_ifdetach(&sc->ifatm.ifnet);

	hatm_destroy(sc);

	return (0);
}

/*
 * Attach to the device. Assume that no locking is needed here.
 * All resource we allocate here are freed by calling hatm_destroy.
 */
static int
hatm_attach(device_t dev)
{
	struct hatm_softc *sc;
	int unit;
	int error;
	uint32_t v;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	sc->dev = dev;
	sc->ifatm.mib.device = ATM_DEVICE_HE155;
	sc->ifatm.mib.serial = 0;
	sc->ifatm.mib.hw_version = 0;
	sc->ifatm.mib.sw_version = 0;
	sc->ifatm.mib.vpi_bits = HE_CONFIG_VPI_BITS;
	sc->ifatm.mib.vci_bits = HE_CONFIG_VCI_BITS;
	sc->ifatm.mib.max_vpcs = 0;
	sc->ifatm.mib.max_vccs = HE_MAX_VCCS;
	sc->ifatm.mib.media = IFM_ATM_UNKNOWN;
	sc->he622 = 0;
	sc->ifatm.phy = &sc->utopia;

	SLIST_INIT(&sc->mbuf0_list);
	SLIST_INIT(&sc->mbuf1_list);
	SLIST_INIT(&sc->tpd_free);

	mtx_init(&sc->mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK, MTX_DEF);
	mtx_init(&sc->mbuf0_mtx, device_get_nameunit(dev), "HEb0", MTX_DEF);
	mtx_init(&sc->mbuf1_mtx, device_get_nameunit(dev), "HEb1", MTX_DEF);
	cv_init(&sc->vcc_cv, "HEVCCcv");
	cv_init(&sc->cv_rcclose, "RCClose");

	sysctl_ctx_init(&sc->sysctl_ctx);

	/*
	 * 4.2 BIOS Configuration
	 */
	v = pci_read_config(dev, PCIR_COMMAND, 2);
	v |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_MWRICEN;
	pci_write_config(dev, PCIR_COMMAND, v, 2);

	/*
	 * 4.3 PCI Bus Controller-Specific Initialisation
	 */
	v = pci_read_config(dev, HE_PCIR_GEN_CNTL_0, 4);
	v |= HE_PCIM_CTL0_MRL | HE_PCIM_CTL0_MRM | HE_PCIM_CTL0_IGNORE_TIMEOUT;
#if BYTE_ORDER == BIG_ENDIAN && 0
	v |= HE_PCIM_CTL0_BIGENDIAN;
#endif
	pci_write_config(dev, HE_PCIR_GEN_CNTL_0, v, 4);

	/*
	 * Map memory
	 */
	v = pci_read_config(dev, PCIR_COMMAND, 2);
	if (!(v & PCIM_CMD_MEMEN)) {
		device_printf(dev, "failed to enable memory\n");
		error = ENXIO;
		goto failed;
	}
	sc->memid = PCIR_MAPS;
	sc->memres = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->memid,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->memres == NULL) {
		device_printf(dev, "could not map memory\n");
		error = ENXIO;
		goto failed;
	}
	sc->memh = rman_get_bushandle(sc->memres);
	sc->memt = rman_get_bustag(sc->memres);

	/*
	 * ALlocate a DMA tag for subsequent allocations
	 */
	if (bus_dma_tag_create(NULL, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, 1,
	    BUS_SPACE_MAXSIZE_32BIT, 0,
	    NULL, NULL, &sc->parent_tag)) {
		device_printf(dev, "could not allocate DMA tag\n");
		error = ENOMEM;
		goto failed;
	}

	if (bus_dma_tag_create(sc->parent_tag, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    MBUF_ALLOC_SIZE, 1,
	    MBUF_ALLOC_SIZE, 0,
	    NULL, NULL, &sc->mbuf_tag)) {
		device_printf(dev, "could not allocate mbuf DMA tag\n");
		error = ENOMEM;
		goto failed;
	}

	/*
	 * Allocate a DMA tag for packets to send. Here we have a problem with
	 * the specification of the maximum number of segments. Theoretically
	 * this would be the size of the transmit ring - 1 multiplied by 3,
	 * but this would not work. So make the maximum number of TPDs
	 * occupied by one packet a configuration parameter.
	 */
	if (bus_dma_tag_create(NULL, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    HE_MAX_PDU, 3 * HE_CONFIG_MAX_TPD_PER_PACKET, HE_MAX_PDU, 0,
	    NULL, NULL, &sc->tx_tag)) {
		device_printf(dev, "could not allocate TX tag\n");
		error = ENOMEM;
		goto failed;
	}

	/*
	 * Setup the interrupt
	 */
	sc->irqid = 0;
	sc->irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid,
	    0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irqres == 0) {
		device_printf(dev, "could not allocate irq\n");
		error = ENXIO;
		goto failed;
	}

	ifp = &sc->ifatm.ifnet;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "hatm";

	/*
	 * Make the sysctl tree
	 */
	error = ENOMEM;
	if ((sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw_atm), OID_AUTO,
	    device_get_nameunit(dev), CTLFLAG_RD, 0, "")) == NULL)
		goto failed;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "istats", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, CTL_ISTATS,
	    hatm_sysctl, "LU", "internal statistics") == NULL)
		goto failed;

#ifdef HATM_DEBUG
	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "tsr", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, 0,
	    hatm_sysctl_tsr, "S", "transmission status registers") == NULL)
		goto failed;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "tpd", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, 0,
	    hatm_sysctl_tpd, "S", "transmission packet descriptors") == NULL)
		goto failed;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "mbox", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, 0,
	    hatm_sysctl_mbox, "S", "mbox registers") == NULL)
		goto failed;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "cm", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, 0,
	    hatm_sysctl_cm, "S", "connection memory") == NULL)
		goto failed;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "heregs", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, 0,
	    hatm_sysctl_heregs, "S", "card registers") == NULL)
		goto failed;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "lbmem", CTLFLAG_RD | CTLTYPE_OPAQUE, sc, 0,
	    hatm_sysctl_lbmem, "S", "local memory") == NULL)
		goto failed;

	kenv_getuint(sc, "debug", &sc->debug, 0, 1);
#endif

	/*
	 * Configure
	 */
	if ((error = hatm_configure(sc)) != 0)
		goto failed;

	/*
	 * Compute memory parameters
	 */
	if (sc->rbp_s0.size != 0) {
		sc->rbp_s0.mask = (sc->rbp_s0.size - 1) << 3;
		sc->rbp_s0.mem.size = sc->rbp_s0.size * 8;
		sc->rbp_s0.mem.align = sc->rbp_s0.mem.size;
	}
	if (sc->rbp_l0.size != 0) {
		sc->rbp_l0.mask = (sc->rbp_l0.size - 1) << 3;
		sc->rbp_l0.mem.size = sc->rbp_l0.size * 8;
		sc->rbp_l0.mem.align = sc->rbp_l0.mem.size;
	}
	if (sc->rbp_s1.size != 0) {
		sc->rbp_s1.mask = (sc->rbp_s1.size - 1) << 3;
		sc->rbp_s1.mem.size = sc->rbp_s1.size * 8;
		sc->rbp_s1.mem.align = sc->rbp_s1.mem.size;
	}
	if (sc->rbrq_0.size != 0) {
		sc->rbrq_0.mem.size = sc->rbrq_0.size * 8;
		sc->rbrq_0.mem.align = sc->rbrq_0.mem.size;
	}
	if (sc->rbrq_1.size != 0) {
		sc->rbrq_1.mem.size = sc->rbrq_1.size * 8;
		sc->rbrq_1.mem.align = sc->rbrq_1.mem.size;
	}

	sc->irq_0.mem.size = sc->irq_0.size * sizeof(uint32_t);
	sc->irq_0.mem.align = 4 * 1024;

	sc->tbrq.mem.size = sc->tbrq.size * 4;
	sc->tbrq.mem.align = 2 * sc->tbrq.mem.size; /* ZZZ */

	sc->tpdrq.mem.size = sc->tpdrq.size * 8;
	sc->tpdrq.mem.align = sc->tpdrq.mem.size;

	sc->hsp_mem.size = sizeof(struct he_hsp);
	sc->hsp_mem.align = 1024;

	sc->lbufs_size = sc->rbp_l0.size + sc->rbrq_0.size;
	sc->tpd_total = sc->tbrq.size + sc->tpdrq.size;
	sc->tpds.align = 64;
	sc->tpds.size = sc->tpd_total * HE_TPD_SIZE;

	hatm_init_rmaps(sc);
	hatm_init_smbufs(sc);
	if ((error = hatm_init_tpds(sc)) != 0)
		goto failed;

	/*
	 * Allocate memory
	 */
	if ((error = hatm_alloc_dmamem(sc, "IRQ", &sc->irq_0.mem)) != 0 ||
	    (error = hatm_alloc_dmamem(sc, "TBRQ0", &sc->tbrq.mem)) != 0 ||
	    (error = hatm_alloc_dmamem(sc, "TPDRQ", &sc->tpdrq.mem)) != 0 ||
	    (error = hatm_alloc_dmamem(sc, "HSP", &sc->hsp_mem)) != 0)
		goto failed;

	if (sc->rbp_s0.mem.size != 0 &&
	    (error = hatm_alloc_dmamem(sc, "RBPS0", &sc->rbp_s0.mem)))
		goto failed;
	if (sc->rbp_l0.mem.size != 0 &&
	    (error = hatm_alloc_dmamem(sc, "RBPL0", &sc->rbp_l0.mem)))
		goto failed;
	if (sc->rbp_s1.mem.size != 0 &&
	    (error = hatm_alloc_dmamem(sc, "RBPS1", &sc->rbp_s1.mem)))
		goto failed;

	if (sc->rbrq_0.mem.size != 0 &&
	    (error = hatm_alloc_dmamem(sc, "RBRQ0", &sc->rbrq_0.mem)))
		goto failed;
	if (sc->rbrq_1.mem.size != 0 &&
	    (error = hatm_alloc_dmamem(sc, "RBRQ1", &sc->rbrq_1.mem)))
		goto failed;

	if ((sc->vcc_zone = uma_zcreate("HE vccs", sizeof(struct hevcc),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0)) == NULL) {
		device_printf(dev, "cannot allocate zone for vccs\n");
		goto failed;
	}

	/*
	 * 4.4 Reset the card.
	 */
	if ((error = hatm_reset(sc)) != 0)
		goto failed;

	/*
	 * Read the prom.
	 */
	hatm_init_bus_width(sc);
	hatm_init_read_eeprom(sc);
	hatm_init_endianess(sc);

	/*
	 * Initialize interface
	 */
	ifp->if_flags = IFF_SIMPLEX;
	ifp->if_ioctl = hatm_ioctl;
	ifp->if_start = hatm_start;
	ifp->if_watchdog = NULL;
	ifp->if_init = hatm_init;

	utopia_attach(&sc->utopia, &sc->ifatm, &sc->media, &sc->mtx,
	    &sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    &hatm_utopia_methods);
	utopia_init_media(&sc->utopia);

	/* these two SUNI routines need the lock */
	mtx_lock(&sc->mtx);
	/* poll while we are not running */
	sc->utopia.flags |= UTP_FL_POLL_CARRIER;
	utopia_start(&sc->utopia);
	utopia_reset(&sc->utopia);
	mtx_unlock(&sc->mtx);

	atm_ifattach(ifp);

#ifdef ENABLE_BPF
	bpfattach(ifp, DLT_ATM_RFC1483, sizeof(struct atmllc));
#endif

	error = bus_setup_intr(dev, sc->irqres, INTR_TYPE_NET, hatm_intr,
	    &sc->irq_0, &sc->ih);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt\n");
		hatm_detach(dev);
		return (error);
	}

	return (0);

  failed:
	hatm_destroy(sc);
	return (error);
}

/*
 * Start the interface. Assume a state as from attach().
 */
void
hatm_initialize(struct hatm_softc *sc)
{
	uint32_t v;
	static const u_int layout[2][7] = HE_CONFIG_MEM_LAYOUT;

	if (sc->ifatm.ifnet.if_flags & IFF_RUNNING)
		return;

	hatm_init_bus_width(sc);
	hatm_init_endianess(sc);

	if_printf(&sc->ifatm.ifnet, "%s, Rev. %s, S/N %u, "
	    "MAC=%02x:%02x:%02x:%02x:%02x:%02x (%ubit PCI)\n",
	    sc->prod_id, sc->rev, sc->ifatm.mib.serial,
	    sc->ifatm.mib.esi[0], sc->ifatm.mib.esi[1], sc->ifatm.mib.esi[2],
	    sc->ifatm.mib.esi[3], sc->ifatm.mib.esi[4], sc->ifatm.mib.esi[5],
	    sc->pci64 ? 64 : 32);

	/*
	 * 4.8 SDRAM Controller Initialisation
	 * 4.9 Initialize RNUM value
	 */
	if (sc->he622)
		WRITE4(sc, HE_REGO_SDRAM_CNTL, HE_REGM_SDRAM_64BIT);
	else
		WRITE4(sc, HE_REGO_SDRAM_CNTL, 0);
	BARRIER_W(sc);

	v = READ4(sc, HE_REGO_LB_SWAP);
	BARRIER_R(sc);
	v |= 0xf << HE_REGS_LBSWAP_RNUM;
	WRITE4(sc, HE_REGO_LB_SWAP, v);
	BARRIER_W(sc);

	hatm_init_irq(sc, &sc->irq_0, 0);
	hatm_clear_irq(sc, 1);
	hatm_clear_irq(sc, 2);
	hatm_clear_irq(sc, 3);

	WRITE4(sc, HE_REGO_GRP_1_0_MAP, 0);
	WRITE4(sc, HE_REGO_GRP_3_2_MAP, 0);
	WRITE4(sc, HE_REGO_GRP_5_4_MAP, 0);
	WRITE4(sc, HE_REGO_GRP_7_6_MAP, 0);
	BARRIER_W(sc);

	/*
	 * 4.11 Enable PCI Bus Controller State Machine
	 */
	v = READ4(sc, HE_REGO_HOST_CNTL);
	BARRIER_R(sc);
	v |= HE_REGM_HOST_OUTFF_ENB | HE_REGM_HOST_CMDFF_ENB |
	    HE_REGM_HOST_QUICK_RD | HE_REGM_HOST_QUICK_WR;
	WRITE4(sc, HE_REGO_HOST_CNTL, v);
	BARRIER_W(sc);

	/*
	 * 5.1.1 Generic configuration state
	 */
	sc->cells_per_row = layout[sc->he622][0];
	sc->bytes_per_row = layout[sc->he622][1];
	sc->r0_numrows = layout[sc->he622][2];
	sc->tx_numrows = layout[sc->he622][3];
	sc->r1_numrows = layout[sc->he622][4];
	sc->r0_startrow = layout[sc->he622][5];
	sc->tx_startrow = sc->r0_startrow + sc->r0_numrows;
	sc->r1_startrow = sc->tx_startrow + sc->tx_numrows;
	sc->cells_per_lbuf = layout[sc->he622][6];

	sc->r0_numbuffs = sc->r0_numrows * (sc->cells_per_row /
	    sc->cells_per_lbuf);
	sc->r1_numbuffs = sc->r1_numrows * (sc->cells_per_row /
	    sc->cells_per_lbuf);
	sc->tx_numbuffs = sc->tx_numrows * (sc->cells_per_row /
	    sc->cells_per_lbuf);

	if (sc->r0_numbuffs > 2560)
		sc->r0_numbuffs = 2560;
	if (sc->r1_numbuffs > 2560)
		sc->r1_numbuffs = 2560;
	if (sc->tx_numbuffs > 5120)
		sc->tx_numbuffs = 5120;

	DBG(sc, ATTACH, ("cells_per_row=%u bytes_per_row=%u r0_numrows=%u "
	    "tx_numrows=%u r1_numrows=%u r0_startrow=%u tx_startrow=%u "
	    "r1_startrow=%u cells_per_lbuf=%u\nr0_numbuffs=%u r1_numbuffs=%u "
	    "tx_numbuffs=%u\n", sc->cells_per_row, sc->bytes_per_row,
	    sc->r0_numrows, sc->tx_numrows, sc->r1_numrows, sc->r0_startrow,
	    sc->tx_startrow, sc->r1_startrow, sc->cells_per_lbuf,
	    sc->r0_numbuffs, sc->r1_numbuffs, sc->tx_numbuffs));

	/*
	 * 5.1.2 Configure Hardware dependend registers
	 */
	if (sc->he622) {
		WRITE4(sc, HE_REGO_LBARB,
		    (0x2 << HE_REGS_LBARB_SLICE) |
		    (0xf << HE_REGS_LBARB_RNUM) |
		    (0x3 << HE_REGS_LBARB_THPRI) |
		    (0x3 << HE_REGS_LBARB_RHPRI) |
		    (0x2 << HE_REGS_LBARB_TLPRI) |
		    (0x1 << HE_REGS_LBARB_RLPRI) |
		    (0x28 << HE_REGS_LBARB_BUS_MULT) |
		    (0x50 << HE_REGS_LBARB_NET_PREF));
		BARRIER_W(sc);
		WRITE4(sc, HE_REGO_SDRAMCON,
		    /* HW bug: don't use banking */
		    /* HE_REGM_SDRAMCON_BANK | */
		    HE_REGM_SDRAMCON_WIDE |
		    (0x384 << HE_REGS_SDRAMCON_REF));
		BARRIER_W(sc);
		WRITE4(sc, HE_REGO_RCMCONFIG,
		    (0x1 << HE_REGS_RCMCONFIG_BANK_WAIT) |
		    (0x1 << HE_REGS_RCMCONFIG_RW_WAIT) |
		    (0x0 << HE_REGS_RCMCONFIG_TYPE));
		WRITE4(sc, HE_REGO_TCMCONFIG,
		    (0x2 << HE_REGS_TCMCONFIG_BANK_WAIT) |
		    (0x1 << HE_REGS_TCMCONFIG_RW_WAIT) |
		    (0x0 << HE_REGS_TCMCONFIG_TYPE));
	} else {
		WRITE4(sc, HE_REGO_LBARB,
		    (0x2 << HE_REGS_LBARB_SLICE) |
		    (0xf << HE_REGS_LBARB_RNUM) |
		    (0x3 << HE_REGS_LBARB_THPRI) |
		    (0x3 << HE_REGS_LBARB_RHPRI) |
		    (0x2 << HE_REGS_LBARB_TLPRI) |
		    (0x1 << HE_REGS_LBARB_RLPRI) |
		    (0x46 << HE_REGS_LBARB_BUS_MULT) |
		    (0x8C << HE_REGS_LBARB_NET_PREF));
		BARRIER_W(sc);
		WRITE4(sc, HE_REGO_SDRAMCON,
		    /* HW bug: don't use banking */
		    /* HE_REGM_SDRAMCON_BANK | */
		    (0x150 << HE_REGS_SDRAMCON_REF));
		BARRIER_W(sc);
		WRITE4(sc, HE_REGO_RCMCONFIG,
		    (0x0 << HE_REGS_RCMCONFIG_BANK_WAIT) |
		    (0x1 << HE_REGS_RCMCONFIG_RW_WAIT) |
		    (0x0 << HE_REGS_RCMCONFIG_TYPE));
		WRITE4(sc, HE_REGO_TCMCONFIG,
		    (0x1 << HE_REGS_TCMCONFIG_BANK_WAIT) |
		    (0x1 << HE_REGS_TCMCONFIG_RW_WAIT) |
		    (0x0 << HE_REGS_TCMCONFIG_TYPE));
	}
	WRITE4(sc, HE_REGO_LBCONFIG, (sc->cells_per_lbuf * 48));

	WRITE4(sc, HE_REGO_RLBC_H, 0);
	WRITE4(sc, HE_REGO_RLBC_T, 0);
	WRITE4(sc, HE_REGO_RLBC_H2, 0);

	WRITE4(sc, HE_REGO_RXTHRSH, 512);
	WRITE4(sc, HE_REGO_LITHRSH, 256);

	WRITE4(sc, HE_REGO_RLBF0_C, sc->r0_numbuffs);
	WRITE4(sc, HE_REGO_RLBF1_C, sc->r1_numbuffs);

	if (sc->he622) {
		WRITE4(sc, HE_REGO_RCCONFIG,
		    (8 << HE_REGS_RCCONFIG_UTDELAY) |
		    (sc->ifatm.mib.vpi_bits << HE_REGS_RCCONFIG_VP) |
		    (sc->ifatm.mib.vci_bits << HE_REGS_RCCONFIG_VC));
		WRITE4(sc, HE_REGO_TXCONFIG,
		    (32 << HE_REGS_TXCONFIG_THRESH) |
		    (sc->ifatm.mib.vci_bits << HE_REGS_TXCONFIG_VCI_MASK) |
		    (sc->tx_numbuffs << HE_REGS_TXCONFIG_LBFREE));
	} else {
		WRITE4(sc, HE_REGO_RCCONFIG,
		    (0 << HE_REGS_RCCONFIG_UTDELAY) |
		    HE_REGM_RCCONFIG_UT_MODE |
		    (sc->ifatm.mib.vpi_bits << HE_REGS_RCCONFIG_VP) |
		    (sc->ifatm.mib.vci_bits << HE_REGS_RCCONFIG_VC));
		WRITE4(sc, HE_REGO_TXCONFIG,
		    (32 << HE_REGS_TXCONFIG_THRESH) |
		    HE_REGM_TXCONFIG_UTMODE |
		    (sc->ifatm.mib.vci_bits << HE_REGS_TXCONFIG_VCI_MASK) |
		    (sc->tx_numbuffs << HE_REGS_TXCONFIG_LBFREE));
	}

	WRITE4(sc, HE_REGO_TXAAL5_PROTO, 0);

	if (sc->rbp_s1.size != 0) {
		WRITE4(sc, HE_REGO_RHCONFIG,
		    HE_REGM_RHCONFIG_PHYENB |
		    ((sc->he622 ? 0x41 : 0x31) << HE_REGS_RHCONFIG_PTMR_PRE) |
		    (1 << HE_REGS_RHCONFIG_OAM_GID));
	} else {
		WRITE4(sc, HE_REGO_RHCONFIG,
		    HE_REGM_RHCONFIG_PHYENB |
		    ((sc->he622 ? 0x41 : 0x31) << HE_REGS_RHCONFIG_PTMR_PRE) |
		    (0 << HE_REGS_RHCONFIG_OAM_GID));
	}
	BARRIER_W(sc);

	hatm_init_cm(sc);

	hatm_init_rx_buffer_pool(sc, 0, sc->r0_startrow, sc->r0_numbuffs);
	hatm_init_rx_buffer_pool(sc, 1, sc->r1_startrow, sc->r1_numbuffs);
	hatm_init_tx_buffer_pool(sc, sc->tx_startrow, sc->tx_numbuffs);

	hatm_init_imed_queues(sc);

	/*
	 * 5.1.6 Application tunable Parameters
	 */
	WRITE4(sc, HE_REGO_MCC, 0);
	WRITE4(sc, HE_REGO_OEC, 0);
	WRITE4(sc, HE_REGO_DCC, 0);
	WRITE4(sc, HE_REGO_CEC, 0);

	hatm_init_cs_block(sc);
	hatm_init_cs_block_cm(sc);

	hatm_init_rpool(sc, &sc->rbp_s0, 0, 0);
	hatm_init_rpool(sc, &sc->rbp_l0, 0, 1);
	hatm_init_rpool(sc, &sc->rbp_s1, 1, 0);
	hatm_clear_rpool(sc, 1, 1);
	hatm_clear_rpool(sc, 2, 0);
	hatm_clear_rpool(sc, 2, 1);
	hatm_clear_rpool(sc, 3, 0);
	hatm_clear_rpool(sc, 3, 1);
	hatm_clear_rpool(sc, 4, 0);
	hatm_clear_rpool(sc, 4, 1);
	hatm_clear_rpool(sc, 5, 0);
	hatm_clear_rpool(sc, 5, 1);
	hatm_clear_rpool(sc, 6, 0);
	hatm_clear_rpool(sc, 6, 1);
	hatm_clear_rpool(sc, 7, 0);
	hatm_clear_rpool(sc, 7, 1);
	hatm_init_rbrq(sc, &sc->rbrq_0, 0);
	hatm_init_rbrq(sc, &sc->rbrq_1, 1);
	hatm_clear_rbrq(sc, 2);
	hatm_clear_rbrq(sc, 3);
	hatm_clear_rbrq(sc, 4);
	hatm_clear_rbrq(sc, 5);
	hatm_clear_rbrq(sc, 6);
	hatm_clear_rbrq(sc, 7);

	sc->lbufs_next = 0;
	bzero(sc->lbufs, sizeof(sc->lbufs[0]) * sc->lbufs_size);

	hatm_init_tbrq(sc, &sc->tbrq, 0);
	hatm_clear_tbrq(sc, 1);
	hatm_clear_tbrq(sc, 2);
	hatm_clear_tbrq(sc, 3);
	hatm_clear_tbrq(sc, 4);
	hatm_clear_tbrq(sc, 5);
	hatm_clear_tbrq(sc, 6);
	hatm_clear_tbrq(sc, 7);

	hatm_init_tpdrq(sc);

	WRITE4(sc, HE_REGO_UBUFF_BA, (sc->he622 ? 0x104780 : 0x800));

	/*
	 * Initialize HSP
	 */
	bzero(sc->hsp_mem.base, sc->hsp_mem.size);
	sc->hsp = sc->hsp_mem.base;
	WRITE4(sc, HE_REGO_HSP_BA, sc->hsp_mem.paddr);

	/*
	 * 5.1.12 Enable transmit and receive
	 * Enable bus master and interrupts
	 */
	v = READ_MBOX4(sc, HE_REGO_CS_ERCTL0);
	v |= 0x18000000;
	WRITE_MBOX4(sc, HE_REGO_CS_ERCTL0, v);

	v = READ4(sc, HE_REGO_RCCONFIG);
	v |= HE_REGM_RCCONFIG_RXENB;
	WRITE4(sc, HE_REGO_RCCONFIG, v);

	v = pci_read_config(sc->dev, HE_PCIR_GEN_CNTL_0, 4);
	v |= HE_PCIM_CTL0_INIT_ENB | HE_PCIM_CTL0_INT_PROC_ENB;
	pci_write_config(sc->dev, HE_PCIR_GEN_CNTL_0, v, 4);

	sc->ifatm.ifnet.if_flags |= IFF_RUNNING;
	sc->ifatm.ifnet.if_baudrate = 53 * 8 * sc->ifatm.mib.pcr;

	sc->utopia.flags &= ~UTP_FL_POLL_CARRIER;

	ATMEV_SEND_IFSTATE_CHANGED(&sc->ifatm,
	    sc->utopia.carrier == UTP_CARR_OK);
}

/*
 * This functions stops the card and frees all resources allocated after
 * the attach. Must have the global lock.
 */
void
hatm_stop(struct hatm_softc *sc)
{
	uint32_t v;
	u_int i, p, cid;
	struct mbuf_chunk_hdr *ch;
	struct mbuf_page *pg;

	mtx_assert(&sc->mtx, MA_OWNED);

	if (!(sc->ifatm.ifnet.if_flags & IFF_RUNNING))
		return;
	sc->ifatm.ifnet.if_flags &= ~IFF_RUNNING;

	ATMEV_SEND_IFSTATE_CHANGED(&sc->ifatm,
	    sc->utopia.carrier == UTP_CARR_OK);

	sc->utopia.flags |= UTP_FL_POLL_CARRIER;

	/*
	 * Stop and reset the hardware so that everything remains
	 * stable.
	 */
	v = READ_MBOX4(sc, HE_REGO_CS_ERCTL0);
	v &= ~0x18000000;
	WRITE_MBOX4(sc, HE_REGO_CS_ERCTL0, v);

	v = READ4(sc, HE_REGO_RCCONFIG);
	v &= ~HE_REGM_RCCONFIG_RXENB;
	WRITE4(sc, HE_REGO_RCCONFIG, v);

	WRITE4(sc, HE_REGO_RHCONFIG, (0x2 << HE_REGS_RHCONFIG_PTMR_PRE));
	BARRIER_W(sc);

	v = READ4(sc, HE_REGO_HOST_CNTL);
	BARRIER_R(sc);
	v &= ~(HE_REGM_HOST_OUTFF_ENB | HE_REGM_HOST_CMDFF_ENB);
	WRITE4(sc, HE_REGO_HOST_CNTL, v);
	BARRIER_W(sc);

	/*
	 * Disable bust master and interrupts
	 */
	v = pci_read_config(sc->dev, HE_PCIR_GEN_CNTL_0, 4);
	v &= ~(HE_PCIM_CTL0_INIT_ENB | HE_PCIM_CTL0_INT_PROC_ENB);
	pci_write_config(sc->dev, HE_PCIR_GEN_CNTL_0, v, 4);

	(void)hatm_reset(sc);

	/*
	 * Card resets the SUNI when resetted, so re-initialize it
	 */
	utopia_reset(&sc->utopia);

	/*
	 * Give any waiters on closing a VCC a chance. They will stop
	 * to wait if they see that IFF_RUNNING disappeared.
	 */
	while (!(cv_waitq_empty(&sc->vcc_cv))) {
		cv_broadcast(&sc->vcc_cv);
		DELAY(100);
	}
	while (!(cv_waitq_empty(&sc->cv_rcclose))) {
		cv_broadcast(&sc->cv_rcclose);
	}

	/*
	 * Now free all resources.
	 */

	/*
	 * Free the large mbufs that are given to the card.
	 */
	for (i = 0 ; i < sc->lbufs_size; i++) {
		if (sc->lbufs[i] != NULL) {
			bus_dmamap_unload(sc->mbuf_tag, sc->rmaps[i]);
			m_freem(sc->lbufs[i]);
			sc->lbufs[i] = NULL;
		}
	}

	/*
	 * Free small buffers
	 */
	for (p = 0; p < sc->mbuf_npages; p++) {
		pg = sc->mbuf_pages[p];
		for (i = 0; i < pg->hdr.nchunks; i++) {
			if (MBUF_TST_BIT(pg->hdr.card, i)) {
				MBUF_CLR_BIT(pg->hdr.card, i);
				MBUF_CLR_BIT(pg->hdr.used, i);
				ch = (struct mbuf_chunk_hdr *) ((char *)pg +
				    i * pg->hdr.chunksize + pg->hdr.hdroff);
				m_freem(ch->mbuf);
			}
		}
	}

	hatm_stop_tpds(sc);

	/*
	 * Free all partial reassembled PDUs on any VCC.
	 */
	for (cid = 0; cid < HE_MAX_VCCS; cid++) {
		if (sc->vccs[cid] != NULL) {
			if (sc->vccs[cid]->chain != NULL)
				m_freem(sc->vccs[cid]->chain);
			uma_zfree(sc->vcc_zone, sc->vccs[cid]);
		}
	}
	bzero(sc->vccs, sizeof(sc->vccs));
	sc->cbr_bw = 0;
	sc->open_vccs = 0;

	/*
	 * Reset CBR rate groups
	 */
	bzero(sc->rate_ctrl, sizeof(sc->rate_ctrl));

	if (sc->rbp_s0.size != 0)
		bzero(sc->rbp_s0.mem.base, sc->rbp_s0.mem.size);
	if (sc->rbp_l0.size != 0)
		bzero(sc->rbp_l0.mem.base, sc->rbp_l0.mem.size);
	if (sc->rbp_s1.size != 0)
		bzero(sc->rbp_s1.mem.base, sc->rbp_s1.mem.size);
	if (sc->rbrq_0.size != 0)
		bzero(sc->rbrq_0.mem.base, sc->rbrq_0.mem.size);
	if (sc->rbrq_1.size != 0)
		bzero(sc->rbrq_1.mem.base, sc->rbrq_1.mem.size);

	bzero(sc->tbrq.mem.base, sc->tbrq.mem.size);
	bzero(sc->tpdrq.mem.base, sc->tpdrq.mem.size);
	bzero(sc->hsp_mem.base, sc->hsp_mem.size);
}

/************************************************************
 *
 * Driver infrastructure
 */
devclass_t hatm_devclass;

static device_method_t hatm_methods[] = {
	DEVMETHOD(device_probe,		hatm_probe),
	DEVMETHOD(device_attach,	hatm_attach),
	DEVMETHOD(device_detach,	hatm_detach),
	{0,0}
};
static driver_t hatm_driver = {
	"hatm",
	hatm_methods,
	sizeof(struct hatm_softc),
};
DRIVER_MODULE(hatm, pci, hatm_driver, hatm_devclass, NULL, 0);
