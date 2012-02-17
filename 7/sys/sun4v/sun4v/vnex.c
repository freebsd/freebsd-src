/*-
 * Copyright (c) 2006 by Marius Strobl <marius@FreeBSD.org>.
 * Copyright (c) 2006 Kip Macy <kmacy@FreeBSD.org>.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/pcpu.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>
#include <machine/intr_machdep.h>
#include <machine/nexusvar.h>
#include <machine/resource.h>

#include <machine/mdesc_bus.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

#include <sys/rman.h>


#define	SUN4V_REG_SPEC2CFG_HDL(x)	((x >> 32) & ~(0xfull << 28))

static device_probe_t vnex_probe;
static device_attach_t vnex_attach;
static bus_print_child_t vnex_print_child;
static bus_add_child_t vnex_add_child;
static bus_probe_nomatch_t vnex_probe_nomatch;
static bus_setup_intr_t vnex_setup_intr;
static bus_teardown_intr_t vnex_teardown_intr;
static bus_get_resource_list_t vnex_get_resource_list;
static mdesc_bus_get_devinfo_t vnex_get_devinfo;

static struct vnex_devinfo * vnex_setup_dinfo(device_t, mde_cookie_t node);
static void vnex_destroy_dinfo(struct vnex_devinfo *);
static int vnex_print_res(struct vnex_devinfo *);

struct vnex_devinfo {
	struct mdesc_bus_devinfo	vndi_mbdinfo;
	struct resource_list	vndi_rl;

	/* Some common properties. */
	struct		nexus_regs *vndi_reg;
	int		vndi_nreg;
};

struct vnex_softc {
	struct rman	sc_intr_rman;
	struct rman	sc_mem_rman;
};

static device_method_t vnex_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vnex_probe),
	DEVMETHOD(device_attach,	vnex_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	vnex_print_child),
	DEVMETHOD(bus_probe_nomatch,	vnex_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_add_child,	vnex_add_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_setup_intr,	vnex_setup_intr),
	DEVMETHOD(bus_teardown_intr,	vnex_teardown_intr),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_get_resource_list, vnex_get_resource_list),

	/* ofw_bus interface */
	/* mdesc_bus interface */
	DEVMETHOD(mdesc_bus_get_devinfo, vnex_get_devinfo),
	DEVMETHOD(mdesc_bus_get_compat,	mdesc_bus_gen_get_compat),
	DEVMETHOD(mdesc_bus_get_name,	mdesc_bus_gen_get_name),
	DEVMETHOD(mdesc_bus_get_type,	mdesc_bus_gen_get_type),

	{ 0, 0 }
};


static driver_t vnex_driver = {
	"vnex",
	vnex_methods,
	sizeof(struct vnex_softc),
};


static devclass_t vnex_devclass;
DRIVER_MODULE(vnex, nexus, vnex_driver, vnex_devclass, 0, 0);


static int
vnex_probe(device_t dev)
{
	if (strcmp(ofw_bus_get_name(dev), "virtual-devices"))
		return (ENXIO);

	device_set_desc(dev, "virtual nexus device");
	return (0);
}


static int
vnex_attach(device_t dev)
{
	struct vnex_devinfo  *vndi;
	struct vnex_softc    *sc;
	device_t              cdev;
	phandle_t             node;
	mde_cookie_t          rootnode, *listp = NULL;
	int                   i, listsz, num_nodes, num_devices;
	md_t                 *mdp;


	node = ofw_bus_get_node(dev);
	if (node == -1)
		panic("%s: ofw_bus_get_node failed.", __func__);

	sc = device_get_softc(dev);
	sc->sc_intr_rman.rm_type = RMAN_ARRAY;
	sc->sc_intr_rman.rm_descr = "Interrupts";
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "Device Memory";
	if (rman_init(&sc->sc_intr_rman) != 0 ||
	    rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_intr_rman, 0, IV_MAX - 1) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 0ULL, ~0ULL) != 0)
		panic("%s: failed to set up rmans.", __func__);

	if ((mdp = md_get()) == NULL) 
		return (ENXIO);

	num_nodes = md_node_count(mdp);
	listsz = num_nodes * sizeof(mde_cookie_t);
	listp = (mde_cookie_t *)malloc(listsz, M_DEVBUF, M_WAITOK);
	rootnode = md_root_node(mdp);
	
	/*
	 * scan the machine description for virtual devices
	 */
	num_devices = md_scan_dag(mdp, rootnode, 
				  md_find_name(mdp, "virtual-device"),
				  md_find_name(mdp, "fwd"), listp);

	for (i = 0; i < num_devices; i++) {
		if ((vndi = vnex_setup_dinfo(dev, listp[i])) == NULL)
			continue;

		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    vndi->vndi_mbdinfo.mbd_name);
			vnex_destroy_dinfo(vndi);
			continue;
		}
		device_set_ivars(cdev, vndi);
	}
	bus_generic_attach(dev);
	free(listp, M_DEVBUF);

	return (0);
}

static device_t
vnex_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t cdev;
	struct vnex_devinfo *vndi;

	cdev = device_add_child_ordered(dev, order, name, unit);
	if (cdev == NULL)
		return (NULL);

	vndi = malloc(sizeof(*vndi), M_DEVBUF, M_WAITOK | M_ZERO);
	vndi->vndi_mbdinfo.mbd_name = strdup(name, M_OFWPROP);
	resource_list_init(&vndi->vndi_rl);
	device_set_ivars(cdev, vndi);

	return (cdev);
}

static int
vnex_print_child(device_t dev, device_t child)
{
	int rv;

	rv = bus_print_child_header(dev, child);
	rv += vnex_print_res(device_get_ivars(child));
	rv += bus_print_child_footer(dev, child);
	return (rv);
}

static void
vnex_probe_nomatch(device_t dev, device_t child)
{
	const char *type;

	device_printf(dev, "<%s>", mdesc_bus_get_name(child));
	vnex_print_res(device_get_ivars(child));
	type = mdesc_bus_get_type(child);
	printf(" type %s (no driver attached)\n",
	    type != NULL ? type : "unknown");
}


static int
vnex_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_filter_t *filt,driver_intr_t *intr, void *arg, void **cookiep)
{

	uint64_t reg, nreg;
	uint64_t ihdl, cfg;
	uint64_t ino, nino;
	int error, cpuid;
	
	if (res == NULL)
		panic("%s: NULL interrupt resource!", __func__);

	if ((error = bus_get_resource(dev, SYS_RES_MEMORY, 0, &reg, &nreg)))
		goto fail;

	if ((error = bus_get_resource(child, SYS_RES_IRQ, 0, &ino, &nino)))
		goto fail;
       
	cfg = SUN4V_REG_SPEC2CFG_HDL(reg);

	if (hv_intr_devino_to_sysino(cfg, (uint32_t)ino, &ihdl) != H_EOK) {
		error = ENXIO;
		goto fail;
	}

	cpuid = 0;

	if (hv_intr_settarget(ihdl, cpuid) != H_EOK) {
		error = ENXIO;
		goto fail;
	}

	if (hv_intr_setstate(ihdl, HV_INTR_IDLE_STATE) != H_EOK) {
		error = ENXIO;
		goto fail;
	}

	if (hv_intr_setenabled(ihdl, HV_INTR_ENABLED) != H_EOK) {
		error = ENXIO;
		goto fail;
	}
	
	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/* We depend here on rman_activate_resource() being idempotent. */
	if ((error = rman_activate_resource(res)))
		goto fail;

	error = inthand_add(device_get_nameunit(child), ihdl,
			    filt, intr, arg, flags, cookiep);

	printf("inthandler added\n");
fail:

	return (error);
}

static int
vnex_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{

	inthand_remove(rman_get_start(r), ih);
	return (0);
}

static struct resource_list *
vnex_get_resource_list(device_t dev, device_t child)
{
	struct vnex_devinfo *vndi;

	vndi = device_get_ivars(child);
	return (&vndi->vndi_rl);
}

static const struct mdesc_bus_devinfo *
vnex_get_devinfo(device_t dev, device_t child)
{
	struct vnex_devinfo *vndi;

	vndi = device_get_ivars(child);
	return (&vndi->vndi_mbdinfo);
}

static struct vnex_devinfo *
vnex_setup_dinfo(device_t dev, mde_cookie_t node)
{
	struct vnex_devinfo *vndi;

	vndi = malloc(sizeof(*vndi), M_DEVBUF, M_WAITOK | M_ZERO);
	if (mdesc_bus_gen_setup_devinfo(&vndi->vndi_mbdinfo, node) != 0) {
		free(vndi, M_DEVBUF);
		return (NULL);
	}

	return (vndi);
}

static void
vnex_destroy_dinfo(struct vnex_devinfo *vndi)
{

	resource_list_free(&vndi->vndi_rl);
	mdesc_bus_gen_destroy_devinfo(&vndi->vndi_mbdinfo);
	free(vndi, M_DEVBUF);
}


static int
vnex_print_res(struct vnex_devinfo *vndi)
{
	int rv;

	rv = 0;
	rv += resource_list_print_type(&vndi->vndi_rl, "mem", SYS_RES_MEMORY,
	    "%#lx");
	rv += resource_list_print_type(&vndi->vndi_rl, "irq", SYS_RES_IRQ,
	    "%ld");
	return (rv);
}
