/*-
 * Copyright (c) 2013 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_common.h>

#include "fdt_ic_if.h"

struct beripic_softc;

static uint64_t	bp_read_cfg(struct beripic_softc *, int);
static void	bp_write_cfg(struct beripic_softc *, int, uint64_t);
static void	bp_detach_resources(device_t);
static char	*bp_strconfig(uint64_t, char *, size_t);
static void	bp_config_source(device_t, int, int, u_long, u_long);
#ifdef __mips__
static void	bp_set_counter_name(device_t, device_t, int);
#endif

static int	beripic_fdt_probe(device_t);
static int	beripic_fdt_attach(device_t);

static int	beripic_activate_intr(device_t, struct resource *);
static struct resource *
		beripic_alloc_intr(device_t, device_t, int *, u_long, u_int);
static int	beripic_config_intr(device_t, int,  enum intr_trigger,
		    enum intr_polarity);
static int	beripic_release_intr(device_t, struct resource *);
static int	beripic_setup_intr(device_t, device_t, struct resource *,
		    int, driver_filter_t *, driver_intr_t *, void *, void **);
static int	beripic_teardown_intr(device_t, device_t, struct resource *,
		    void *);

static int	beripic_filter(void *);
static void	beripic_intr(void *);

#define	BP_MAX_HARD_IRQS	6
#define	BP_FIRST_SOFT		64

struct beripic_softc {
	device_t		bp_dev;
	struct resource		*bp_cfg_res;
	struct resource		*bp_read_res;
	struct resource		*bp_set_res;
	struct resource		*bp_clear_res;
	int			bp_cfg_rid;
	int			bp_read_rid;
	int			bp_set_rid;
	int			bp_clear_rid;
	bus_space_tag_t		bp_cfg_bst;
	bus_space_tag_t		bp_read_bst;
	bus_space_tag_t		bp_set_bst;
	bus_space_tag_t		bp_clear_bst;
	bus_space_handle_t	bp_cfg_bsh;
	bus_space_handle_t	bp_read_bsh;
	bus_space_handle_t	bp_set_bsh;
	bus_space_handle_t	bp_clear_bsh;

	struct resource		*bp_irqs[BP_MAX_HARD_IRQS];
	int			bp_irq_rids[BP_MAX_HARD_IRQS];
	int			bp_nirqs;
	int			bp_next_irq;
	int			bp_next_tid;

	int			bp_nthreads;

	int			bp_nhard;
	int			bp_nsoft;
	int			bp_nsrcs;
	struct rman		bp_src_rman;

#ifdef __mips__
	mips_intrcnt_t		*bp_counters;
#endif

	struct mtx		bp_cfgmtx;
};

struct beripic_intr_arg {
	driver_filter_t		*filter;
	driver_intr_t		*intr;
	void			*arg;
	struct resource		*irq;
#ifdef __mips__
	mips_intrcnt_t		counter;
#endif
};

struct beripic_cookie {
	struct beripic_intr_arg	*bpia;
	struct resource		*hirq;
	void			*cookie;
};

#define	BP_CFG_MASK_E		0x80000000ull
#define	BP_CFG_SHIFT_E		31
#define	BP_CFG_MASK_TID		0x7FFFFF00ull	/* Depends on CPU */
#define	BP_CFG_SHIFT_TID	8
#define	BP_CFG_MASK_IRQ		0x0000000Full
#define BP_CFG_SHIFT_IRQ	0
#define	BP_CFG_VALID		(BP_CFG_MASK_E|BP_CFG_MASK_TID|BP_CFG_MASK_IRQ)
#define	BP_CFG_RESERVED		~BP_CFG_VALID

#define	BP_CFG_ENABLED(cfg)	(((cfg) & BP_CFG_MASK_E) >> BP_CFG_SHIFT_E)
#define	BP_CFG_TID(cfg)		(((cfg) & BP_CFG_MASK_TID) >> BP_CFG_SHIFT_TID)
#define	BP_CFG_IRQ(cfg)		(((cfg) & BP_CFG_MASK_IRQ) >> BP_CFG_SHIFT_IRQ)

MALLOC_DEFINE(M_BERIPIC, "beripic", "beripic memory");

static uint64_t
bp_read_cfg(struct beripic_softc *sc, int irq)
{
	
	KASSERT((irq >= 0 && irq < sc->bp_nsrcs),
	    ("IRQ of of range %d (0-%d)", irq, sc->bp_nsrcs - 1));
	return (bus_space_read_8(sc->bp_cfg_bst, sc->bp_cfg_bsh, irq * 8));
}

static void
bp_write_cfg(struct beripic_softc *sc, int irq, uint64_t config)
{
	
	KASSERT((irq >= 0 && irq < sc->bp_nsrcs),
	    ("IRQ of of range %d (0-%d)", irq, sc->bp_nsrcs - 1));
	bus_space_write_8(sc->bp_cfg_bst, sc->bp_cfg_bsh, irq * 8, config);
}

static void
bp_detach_resources(device_t dev)
{
	struct beripic_softc *sc;
	int i;

	sc = device_get_softc(dev);

	if (sc->bp_cfg_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->bp_cfg_rid,
		    sc->bp_cfg_res);
		sc->bp_cfg_res = NULL;
	}
	if (sc->bp_read_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->bp_read_rid,
		    sc->bp_read_res);
		sc->bp_read_res = NULL;
	}
	if (sc->bp_set_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->bp_set_rid,
		    sc->bp_set_res);
		sc->bp_set_res = NULL;
	}
	if (sc->bp_clear_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->bp_clear_rid,
		    sc->bp_clear_res);
		sc->bp_clear_res = NULL;
	}
	for (i = sc->bp_nirqs - 1; i >= 0; i--) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->bp_irq_rids[i],
		    sc->bp_irqs[i]);
	}
	sc->bp_nirqs = 0;
}

static char *
bp_strconfig(uint64_t config, char *configstr, size_t len)
{
	
	if (snprintf(configstr, len, "%s tid: %llu hardintr %llu",
	    BP_CFG_ENABLED(config) ? "enabled" : "disabled",
	    BP_CFG_TID(config), BP_CFG_IRQ(config)) > len - 1)
		return (NULL);
	return (configstr);
}

static void
bp_config_source(device_t ic, int src, int enable, u_long tid, u_long irq)
{
	struct beripic_softc *sc;
	uint64_t config;

	sc = device_get_softc(ic);

	config = 0;
	config |= enable << BP_CFG_SHIFT_E;
	config |= tid << BP_CFG_SHIFT_TID;
	config |= irq << BP_CFG_SHIFT_IRQ;

	bp_write_cfg(sc, src, config);
}

#ifdef __mips__
static void
bp_set_counter_name(device_t ic, device_t child, int src)
{
	struct beripic_softc *sc;
	char name[MAXCOMLEN + 1];

	sc = device_get_softc(ic);

	if (snprintf(name, sizeof(name), "bp%dsrc%d%s%s%s",
	    device_get_unit(ic), src, src < sc->bp_nhard ? "" : "s",
	    child == NULL ? "" : " ",
	    child == NULL ? " " : device_get_nameunit(child)) >= sizeof(name))
		name[sizeof(name) - 2] = '+';
	
	mips_intrcnt_setname(sc->bp_counters[src], name);
}
#endif

static int
beripic_fdt_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "sri-cambridge,beri-pic"))
		return (ENXIO);
		
	device_set_desc(dev, "BERI Programmable Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
beripic_fdt_attach(device_t dev)
{
	char configstr[64];
	struct beripic_softc *sc;
	struct fdt_ic *fic;
	pcell_t nhard, nsoft;
	phandle_t ph;
	int error, i, src;
	uint64_t config;

	sc = device_get_softc(dev);
	sc->bp_dev = dev;

	mtx_init(&sc->bp_cfgmtx, "beripic config lock", NULL, MTX_DEF);

	/*
	 * FDT lists CONFIG, IP_READ, IP_SET, and IP_CLEAR registers as
	 * seperate memory regions in that order.
	 */
	sc->bp_cfg_rid = 0;
	sc->bp_cfg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->bp_cfg_rid, RF_ACTIVE);
	if (sc->bp_cfg_res == NULL) {
		device_printf(dev, "failed to map config memory");
		error = ENXIO;
		goto err;
	}
	if (bootverbose)
		device_printf(sc->bp_dev, "config region at mem %p-%p\n",
		    (void *)rman_get_start(sc->bp_cfg_res),
		    (void *)(rman_get_start(sc->bp_cfg_res) +
		    rman_get_size(sc->bp_cfg_res)));

	sc->bp_read_rid = 1;
	sc->bp_read_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->bp_read_rid, RF_ACTIVE);
	if (sc->bp_read_res == NULL) {
		device_printf(dev, "failed to map IP read memory");
		error = ENXIO;
		goto err;
	}
	if (bootverbose)
		device_printf(sc->bp_dev, "IP read region at mem %p-%p\n",
		    (void *)rman_get_start(sc->bp_read_res),
		    (void *)(rman_get_start(sc->bp_read_res) +
		    rman_get_size(sc->bp_read_res)));

	sc->bp_set_rid = 2;
	sc->bp_set_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->bp_set_rid, RF_ACTIVE);
	if (sc->bp_set_res == NULL) {
		device_printf(dev, "failed to map IP read memory");
		error = ENXIO;
		goto err;
	}
	if (bootverbose)
		device_printf(sc->bp_dev, "IP set region at mem %p-%p\n",
		    (void *)rman_get_start(sc->bp_set_res),
		    (void *)(rman_get_start(sc->bp_set_res) +
		    rman_get_size(sc->bp_set_res)));

	sc->bp_clear_rid = 3;
	sc->bp_clear_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->bp_clear_rid, RF_ACTIVE);
	if (sc->bp_clear_res == NULL) {
		device_printf(dev, "failed to map IP read memory");
		error = ENXIO;
		goto err;
	}
	if (bootverbose)
		device_printf(sc->bp_dev, "IP clear region at mem %p-%p\n",
		    (void *)rman_get_start(sc->bp_clear_res),
		    (void *)(rman_get_start(sc->bp_clear_res) +
		    rman_get_size(sc->bp_clear_res)));

	i = 0;
	for (i = 0; i < BP_MAX_HARD_IRQS; i++) {
		sc->bp_irq_rids[i] = i;
		sc->bp_irqs[i] = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->bp_irq_rids[i], RF_ACTIVE | RF_SHAREABLE);
		if (sc->bp_irqs[i] == NULL)
			break;
	}
	if (i == 0) {
		device_printf(dev, "failed to allocate any parent IRQs!");
		error = ENXIO;
		goto err;
	}
	sc->bp_nirqs = i;

	ph = ofw_bus_gen_get_node(device_get_parent(dev), dev);

#ifndef SMP
	sc->bp_nthreads = 1;
#else
	sc->bp_nthreads = 1;
	/* XXX: get nthreads from cpu(s) somehow */
#endif

	if (OF_getprop(ph, "hard-interrupt-sources", &nhard, sizeof(nhard))
	    <= 0) {
		device_printf(dev, "failed to get number of hard sources");
		error = ENXIO;
		goto err;
	}
	if (OF_getprop(ph, "soft-interrupt-sources", &nsoft, sizeof(nsoft))
	    <= 0) {
		device_printf(dev, "failed to get number of soft sources");
		error = ENXIO;
		goto err;
	}

	sc->bp_nhard = nhard;
	sc->bp_nsoft = nsoft;
	sc->bp_nsrcs = sc->bp_nhard + sc->bp_nsoft;
	/* XXX: should deal with gap between hard and soft */
	KASSERT(sc->bp_nhard <= BP_FIRST_SOFT,
	    ("too many hard sources"));
	KASSERT(rman_get_size(sc->bp_cfg_res) / 8 == sc->bp_nsrcs,
	    ("config space size does not match sources"));
	KASSERT(sc->bp_nhard % 64 == 0,
	    ("Non-multiple of 64 intr counts not supported"));
	KASSERT(sc->bp_nsoft % 64 == 0,
	    ("Non-multiple of 64 intr counts not supported"));
	if (bootverbose)
		device_printf(dev, "%d hard and %d soft sources\n",
		    sc->bp_nhard, sc->bp_nsoft);

#ifdef __mips__
	sc->bp_counters = malloc(sizeof(*sc->bp_counters) * sc->bp_nsrcs,
	    M_BERIPIC, M_WAITOK|M_ZERO);
	for (i = 0; i < sc->bp_nsrcs; i++) {
		sc->bp_counters[i] = mips_intrcnt_create("");
		bp_set_counter_name(dev, NULL, i);
	}
#endif
	
	sc->bp_src_rman.rm_start = 0;
	sc->bp_src_rman.rm_end = sc->bp_nsrcs - 1;
	sc->bp_src_rman.rm_type = RMAN_ARRAY;
	sc->bp_src_rman.rm_descr = "Interrupt source";
	if (rman_init(&(sc->bp_src_rman)) != 0 ||
	    rman_manage_region(&(sc->bp_src_rman), 0, sc->bp_nsrcs - 1) != 0) {
		device_printf(dev, "Failed to set up sources rman");
		error = ENXIO;
		goto err;
	}

	sc->bp_cfg_bst = rman_get_bustag(sc->bp_cfg_res);
	sc->bp_cfg_bsh = rman_get_bushandle(sc->bp_cfg_res);
	sc->bp_read_bst = rman_get_bustag(sc->bp_read_res);
	sc->bp_read_bsh = rman_get_bushandle(sc->bp_read_res);
	sc->bp_set_bst = rman_get_bustag(sc->bp_set_res);
	sc->bp_set_bsh = rman_get_bushandle(sc->bp_set_res);
	sc->bp_clear_bst = rman_get_bustag(sc->bp_clear_res);
	sc->bp_clear_bsh = rman_get_bushandle(sc->bp_clear_res);

	for (src = 0; src < sc->bp_nsrcs; src++) {
		config = bp_read_cfg(sc, src);
		if (config == 0)
			continue;

		if (bootverbose) {
			device_printf(dev, "initial config: src %d: %s\n", src,
			    bp_strconfig(config, configstr, sizeof(configstr)));
			if (config & BP_CFG_RESERVED)
				device_printf(dev,
				    "reserved bits not 0: 0x%016jx\n",
				    (uintmax_t) config);
		}

		bp_config_source(dev, src, 0, 0, 0);
	}

	fic = malloc(sizeof(*fic), M_BERIPIC, M_WAITOK|M_ZERO);
	fic->iph = ph;
	fic->dev = dev;
	SLIST_INSERT_HEAD(&fdt_ic_list_head, fic, fdt_ics);

	return (0);
err:
	bp_detach_resources(dev);

	return (error);
}

static struct resource *
beripic_alloc_intr(device_t ic, device_t child, int *rid, u_long irq,
    u_int flags)
{
	struct beripic_softc *sc;
	struct resource *rv;

	sc = device_get_softc(ic);

	rv = rman_reserve_resource(&(sc->bp_src_rman), irq, irq, 1, flags,
	    child);
	if (rv == NULL)
		 printf("%s: could not reserve source interrupt for %s\n",
		     __func__, device_get_nameunit(child));
	rman_set_rid(rv, *rid);

	if ((flags & RF_ACTIVE) &&
	    beripic_activate_intr(ic, rv) != 0) {
		printf("%s: could not activate interrupt\n", __func__);
		rman_release_resource(rv);
		return (NULL);
	}

	return (rv);
}

static int
beripic_release_intr(device_t ic, struct resource *r)
{
	
	return (rman_release_resource(r));
}

static int
beripic_activate_intr(device_t ic, struct resource *r)
{
	
	return (rman_activate_resource(r));
}

static int
beripic_deactivate_intr(device_t ic, struct resource *r)
{
	
	return (rman_deactivate_resource(r));
}

static int
beripic_config_intr(device_t dev, int irq, enum intr_trigger trig,
   enum intr_polarity pol)
{

	if (trig != INTR_TRIGGER_CONFORM || pol != INTR_POLARITY_CONFORM)
		return (EINVAL);

	return (0);
}

static int
beripic_setup_intr(device_t ic, device_t child, struct resource *irq,
    int flags, driver_filter_t *filter, driver_intr_t *intr, void *arg, 
    void **cookiep)
{
	struct beripic_softc *sc;
	struct beripic_intr_arg *bpia;
	struct beripic_cookie *bpc;
	int error;
	u_long hirq, src, tid;

	sc = device_get_softc(ic);

	src = rman_get_start(irq);

	KASSERT(src < sc->bp_nsrcs, ("source (%lu) out of range 0-%d",
	     src, sc->bp_nsrcs - 1));

	bpia = malloc(sizeof(*bpia), M_BERIPIC, M_WAITOK|M_ZERO);
	bpia->filter = filter;
	bpia->intr = intr;
	bpia->arg = arg;
	bpia->irq = irq;
#ifdef __mips__
	bpia->counter = sc->bp_counters[src];
	bp_set_counter_name(ic, child, src);
#endif

	bpc = malloc(sizeof(*bpc), M_BERIPIC, M_WAITOK|M_ZERO);
	bpc->bpia = bpia;

	mtx_lock(&(sc->bp_cfgmtx));
	bpc->hirq = sc->bp_irqs[sc->bp_next_irq];
	hirq = rman_get_start(bpc->hirq);
	tid = sc->bp_next_tid;

	error = BUS_SETUP_INTR(device_get_parent(ic), ic, bpc->hirq, flags,
	    beripic_filter, intr == NULL ? NULL : beripic_intr, bpia,
	    &(bpc->cookie));
	if (error != 0)
		goto err;

#ifdef NOTYET
#ifdef SMP
	/* XXX: bind ithread to cpu */
	sc->bp_next_tid++;
	if (sc->bp_next_tid >= sc->bp_nthreads)
		sc->bp_next_tid = 0;
#endif
#endif
	if (sc->bp_next_tid == 0) {
		sc->bp_next_irq++;
		if (sc->bp_next_irq >= sc->bp_nirqs)
			sc->bp_next_irq = 0;
	}
	mtx_unlock(&(sc->bp_cfgmtx));

	*cookiep = bpc;

	bp_config_source(ic, rman_get_start(irq), 1, tid, hirq);

	return (0);
err:
	free(bpc, M_BERIPIC);
	free(bpia, M_BERIPIC);

	return (error);
}

static int
beripic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct beripic_cookie *bpc;
	int error;

	bpc = cookie;

	bp_config_source(dev, rman_get_start(irq), 0, 0, 0);

	free(bpc->bpia, M_BERIPIC);

	error = BUS_TEARDOWN_INTR(device_get_parent(dev), dev, bpc->hirq,
	    bpc->cookie);

	free(bpc, M_BERIPIC);

	return (error);
}

static int
beripic_filter(void *arg)
{
	struct beripic_intr_arg *bpic;

	bpic = arg;

#ifdef __mips__
	mips_intrcnt_inc(bpic->counter);
#endif

	/* XXX: Add a check that our source is high */

	if (bpic->filter == NULL)
		return (FILTER_SCHEDULE_THREAD);

	return (bpic->filter(bpic->arg));
}

static void
beripic_intr(void *arg)
{
	struct beripic_intr_arg *bpic;

	bpic = arg;

	KASSERT(bpic->intr != NULL,
	    ("%s installed, but no child intr", __func__));

	bpic->intr(bpic->arg);
}

#ifdef SMP
static void
beripic_setup_ipi(device_t ic, u_int tid, u_int ipi_irq)
{
	
	bp_config_source(ic, BP_FIRST_SOFT + tid, 1, tid, ipi_irq);
}

static void
beripic_send_ipi(device_t ic, u_int tid)
{
	struct beripic_softc *sc;
	uint64_t bit;

	sc = device_get_softc(ic);

	KASSERT(tid < sc->bp_nsoft, ("tid (%d) too large\n", tid));

	bit = 1ULL << (tid % 64);
	bus_space_write_8(sc->bp_set_bst, sc->bp_set_bsh, 
	    (BP_FIRST_SOFT / 8) + (tid / 64), bit);
}

static void
beripic_clear_ipi(device_t ic, u_int tid)
{
	struct beripic_softc *sc;
	uint64_t bit;

	sc = device_get_softc(ic);

	KASSERT(tid < sc->bp_nsoft, ("tid (%d) to large\n", tid));

	bit = 1ULL << (tid % 64);
	bus_space_write_8(sc->bp_clear_bst, sc->bp_clear_bsh, 
	    (BP_FIRST_SOFT / 8) + (tid / 64), bit);
}
#endif

devclass_t	beripic_devclass;

static device_method_t beripic_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		beripic_fdt_probe),
	DEVMETHOD(device_attach,	beripic_fdt_attach),

	DEVMETHOD(fdt_ic_activate_intr,	beripic_activate_intr),
	DEVMETHOD(fdt_ic_alloc_intr,	beripic_alloc_intr),
	DEVMETHOD(fdt_ic_config_intr,	beripic_config_intr),
	DEVMETHOD(fdt_ic_deactivate_intr, beripic_deactivate_intr),
	DEVMETHOD(fdt_ic_release_intr,	beripic_release_intr),
	DEVMETHOD(fdt_ic_setup_intr,	beripic_setup_intr),
	DEVMETHOD(fdt_ic_teardown_intr,	beripic_teardown_intr),

#ifdef SMP
	DEVMETHOD(fdt_ic_setup_ipi,	beripic_setup_ipi),
	DEVMETHOD(fdt_ic_clear_ipi,	beripic_clear_ipi),
	DEVMETHOD(fdt_ic_send_ipi,	beripic_send_ipi),
#endif

	{ 0, 0 },
};

static driver_t beripic_fdt_driver = {
	"beripic",
	beripic_fdt_methods,
	sizeof(struct beripic_softc)
};

DRIVER_MODULE(beripic, simplebus, beripic_fdt_driver, beripic_devclass, 0, 0);
