/*-
 * Copyright 2006 by Juniper Networks. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/spr.h>
#include <machine/ocpbus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <machine/bootinfo.h>

#include <powerpc/mpc85xx/ocpbus.h>
#include <powerpc/mpc85xx/mpc85xx.h>

#include "pic_if.h"

extern struct bus_space bs_be_tag;

struct ocpbus_softc {
	struct rman	sc_mem;
	struct rman	sc_irq;
};

struct ocp_devinfo {
	int		ocp_devtype;
	int		ocp_unit;
};

static int ocpbus_probe(device_t);
static int ocpbus_attach(device_t);
static int ocpbus_shutdown(device_t);
static int ocpbus_get_resource(device_t, device_t, int, int, u_long *,
    u_long *);
static struct resource *ocpbus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int ocpbus_print_child(device_t, device_t);
static int ocpbus_release_resource(device_t, device_t, int, int,
    struct resource *);
static int ocpbus_read_ivar(device_t, device_t, int, uintptr_t *);
static int ocpbus_setup_intr(device_t, device_t, struct resource *, int,
    driver_filter_t *, driver_intr_t *, void *, void **);
static int ocpbus_teardown_intr(device_t, device_t, struct resource *, void *);
static int ocpbus_config_intr(device_t, int, enum intr_trigger,
    enum intr_polarity);

/*
 * Bus interface definition
 */
static device_method_t ocpbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ocpbus_probe),
	DEVMETHOD(device_attach,	ocpbus_attach),
	DEVMETHOD(device_shutdown,	ocpbus_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	ocpbus_print_child),
	DEVMETHOD(bus_read_ivar,	ocpbus_read_ivar),
	DEVMETHOD(bus_setup_intr,	ocpbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	ocpbus_teardown_intr),
	DEVMETHOD(bus_config_intr,	ocpbus_config_intr),

	DEVMETHOD(bus_get_resource,	ocpbus_get_resource),
	DEVMETHOD(bus_alloc_resource,	ocpbus_alloc_resource),
	DEVMETHOD(bus_release_resource,	ocpbus_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	{ 0, 0 }
};

static driver_t ocpbus_driver = {
	"ocpbus",
	ocpbus_methods,
	sizeof(struct ocpbus_softc)
};

devclass_t ocpbus_devclass;

DRIVER_MODULE(ocpbus, nexus, ocpbus_driver, ocpbus_devclass, 0, 0);

static device_t
ocpbus_mk_child(device_t dev, int type, int unit)
{
	struct ocp_devinfo *dinfo;
	device_t child;

	child = device_add_child(dev, NULL, -1);
	if (child == NULL) {
		device_printf(dev, "could not add child device\n");
		return (NULL);
	}
	dinfo = malloc(sizeof(struct ocp_devinfo), M_DEVBUF, M_WAITOK|M_ZERO);
	dinfo->ocp_devtype = type;
	dinfo->ocp_unit = unit;
	device_set_ivars(child, dinfo);
	return (child);
}

static int
ocpbus_write_law(int trgt, int type, u_long *startp, u_long *countp)
{
	u_long addr, size;

	switch (type) {
	case SYS_RES_MEMORY:
		switch (trgt) {
		case OCP85XX_TGTIF_PCI0:
			addr = 0x80000000;
			size = 0x10000000;
			break;
		case OCP85XX_TGTIF_PCI1:
			addr = 0x90000000;
			size = 0x10000000;
			break;
		case OCP85XX_TGTIF_PCI2:
			addr = 0xA0000000;
			size = 0x10000000;
			break;
		case OCP85XX_TGTIF_PCI3:
			addr = 0xB0000000;
			size = 0x10000000;
			break;
		default:
			return (EINVAL);
		}
		break;
	case SYS_RES_IOPORT:
		switch (trgt) {
		case OCP85XX_TGTIF_PCI0:
			addr = 0xfee00000;
			size = 0x00010000;
			break;
		case OCP85XX_TGTIF_PCI1:
			addr = 0xfee10000;
			size = 0x00010000;
			break;
		case OCP85XX_TGTIF_PCI2:
			addr = 0xfee20000;
			size = 0x00010000;
			break;
		case OCP85XX_TGTIF_PCI3:
			addr = 0xfee30000;
			size = 0x00010000;
			break;
		default:
			return (EINVAL);
		}
		break;
	default:
		return (EINVAL);
	}

	*startp = addr;
	*countp = size;

	return (law_enable(trgt, *startp, *countp));
}

static int
ocpbus_probe(device_t dev)
{

	device_set_desc(dev, "Freescale on-chip peripherals bus");
	return (BUS_PROBE_DEFAULT);
}

static int
ocpbus_attach(device_t dev)
{
	struct ocpbus_softc *sc;
	int error, i, tgt, law_max;
	uint32_t sr;
	u_long start, end;

	sc = device_get_softc(dev);

	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_I2C, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_I2C, 1);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_UART, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_UART, 1);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_LBC, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_PCIB, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_PCIB, 1);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_PCIB, 2);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_PCIB, 3);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_TSEC, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_TSEC, 1);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_TSEC, 2);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_TSEC, 3);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_PIC, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_QUICC, 0);
	ocpbus_mk_child(dev, OCPBUS_DEVTYPE_SEC, 0);

	/* Set up IRQ rman */
	start = 0;
	end = INTR_VECTORS - 1;
	sc->sc_irq.rm_start = start;
	sc->sc_irq.rm_end = end;
	sc->sc_irq.rm_type = RMAN_ARRAY;
	sc->sc_irq.rm_descr = "Interrupt request lines";
	if (rman_init(&sc->sc_irq) ||
	    rman_manage_region(&sc->sc_irq, start, end))
		panic("ocpbus_attach IRQ rman");

	/* Set up I/O mem rman */
	sc->sc_mem.rm_type = RMAN_ARRAY;
	sc->sc_mem.rm_descr = "OCPBus Device Memory";
	error = rman_init(&sc->sc_mem);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	error = rman_manage_region(&sc->sc_mem, CCSRBAR_VA,
	    CCSRBAR_VA + CCSRBAR_SIZE - 1);
	if (error) {
		device_printf(dev, "rman_manage_region() failed. error = %d\n",
		    error);
		return (error);
	}

	/*
	 * Clear local access windows. Skip DRAM entries, so we don't shoot
	 * ourselves in the foot.
	 */
	law_max = law_getmax();
	for (i = 0; i < law_max; i++) {
		sr = ccsr_read4(OCP85XX_LAWSR(i));
		if ((sr & 0x80000000) == 0)
			continue;
		tgt = (sr & 0x01f00000) >> 20;
		if (tgt == OCP85XX_TGTIF_RAM1 || tgt == OCP85XX_TGTIF_RAM2 ||
		    tgt == OCP85XX_TGTIF_RAM_INTL)
			continue;

		ccsr_write4(OCP85XX_LAWSR(i), sr & 0x7fffffff);
	}

	if (bootverbose)
		device_printf(dev, "PORDEVSR=%08x, PORDEVSR2=%08x\n",
		    ccsr_read4(OCP85XX_PORDEVSR),
		    ccsr_read4(OCP85XX_PORDEVSR2));

	/*
	 * Internal interrupt are always active-high. Since the sense cannot
	 * be specified, we program as edge-triggered to make sure we write
	 * a 0 value to the reserved bit in the OpenPIC compliant PIC. This
	 * is not to say anything about the sense of any of the internal
	 * interrupt sources.
	 */
	for (i = PIC_IRQ_INT(0); i < PIC_IRQ_INT(32); i++)
		powerpc_config_intr(i, INTR_TRIGGER_EDGE, INTR_POLARITY_HIGH);

	return (bus_generic_attach(dev));
}

static int
ocpbus_shutdown(device_t dev)
{

	return(0);
}

struct ocp_resource {
	int	sr_devtype;
	int	sr_unit;
	int	sr_resource;
	int	sr_rid;
	int	sr_offset;
	int	sr_size;
};

const struct ocp_resource mpc8555_resources[] = {
	{OCPBUS_DEVTYPE_PIC, 0, SYS_RES_MEMORY, 0, OCP85XX_OPENPIC_OFF,
	    OCP85XX_OPENPIC_SIZE},

	{OCPBUS_DEVTYPE_QUICC, 0, SYS_RES_MEMORY, 0, OCP85XX_QUICC_OFF,
	    OCP85XX_QUICC_SIZE},
	{OCPBUS_DEVTYPE_QUICC, 0, SYS_RES_IRQ, 0, 30, 1},

	{OCPBUS_DEVTYPE_TSEC, 0, SYS_RES_MEMORY, 0, OCP85XX_TSEC0_OFF,
	    OCP85XX_TSEC_SIZE},
	{OCPBUS_DEVTYPE_TSEC, 0, SYS_RES_IRQ, 0, 13, 1},
	{OCPBUS_DEVTYPE_TSEC, 0, SYS_RES_IRQ, 1, 14, 1},
	{OCPBUS_DEVTYPE_TSEC, 0, SYS_RES_IRQ, 2, 18, 1},
	{OCPBUS_DEVTYPE_TSEC, 1, SYS_RES_MEMORY, 0, OCP85XX_TSEC1_OFF,
	    OCP85XX_TSEC_SIZE},
	{OCPBUS_DEVTYPE_TSEC, 1, SYS_RES_IRQ, 0, 19, 1},
	{OCPBUS_DEVTYPE_TSEC, 1, SYS_RES_IRQ, 1, 20, 1},
	{OCPBUS_DEVTYPE_TSEC, 1, SYS_RES_IRQ, 2, 24, 1},
	{OCPBUS_DEVTYPE_TSEC, 2, SYS_RES_MEMORY, 0, OCP85XX_TSEC2_OFF,
	    OCP85XX_TSEC_SIZE},
	{OCPBUS_DEVTYPE_TSEC, 2, SYS_RES_IRQ, 0, 15, 1},
	{OCPBUS_DEVTYPE_TSEC, 2, SYS_RES_IRQ, 1, 16, 1},
	{OCPBUS_DEVTYPE_TSEC, 2, SYS_RES_IRQ, 2, 17, 1},
	{OCPBUS_DEVTYPE_TSEC, 3, SYS_RES_MEMORY, 0, OCP85XX_TSEC3_OFF,
	    OCP85XX_TSEC_SIZE},
	{OCPBUS_DEVTYPE_TSEC, 3, SYS_RES_IRQ, 0, 21, 1},
	{OCPBUS_DEVTYPE_TSEC, 3, SYS_RES_IRQ, 1, 22, 1},
	{OCPBUS_DEVTYPE_TSEC, 3, SYS_RES_IRQ, 2, 23, 1},

	{OCPBUS_DEVTYPE_UART, 0, SYS_RES_MEMORY, 0, OCP85XX_UART0_OFF,
	    OCP85XX_UART_SIZE},
	{OCPBUS_DEVTYPE_UART, 0, SYS_RES_IRQ, 0, 26, 1},
	{OCPBUS_DEVTYPE_UART, 1, SYS_RES_MEMORY, 0, OCP85XX_UART1_OFF,
	    OCP85XX_UART_SIZE},
	{OCPBUS_DEVTYPE_UART, 1, SYS_RES_IRQ, 0, 26, 1},

	{OCPBUS_DEVTYPE_PCIB, 0, SYS_RES_MEMORY, 0, OCP85XX_PCI0_OFF,
	    OCP85XX_PCI_SIZE},
	{OCPBUS_DEVTYPE_PCIB, 0, SYS_RES_MEMORY, 1, 0, OCP85XX_TGTIF_PCI0},
	{OCPBUS_DEVTYPE_PCIB, 0, SYS_RES_IOPORT, 1, 0, OCP85XX_TGTIF_PCI0},
	{OCPBUS_DEVTYPE_PCIB, 1, SYS_RES_MEMORY, 0, OCP85XX_PCI1_OFF,
	    OCP85XX_PCI_SIZE},
	{OCPBUS_DEVTYPE_PCIB, 1, SYS_RES_MEMORY, 1, 0, OCP85XX_TGTIF_PCI1},
	{OCPBUS_DEVTYPE_PCIB, 1, SYS_RES_IOPORT, 1, 0, OCP85XX_TGTIF_PCI1},
	{OCPBUS_DEVTYPE_PCIB, 2, SYS_RES_MEMORY, 0, OCP85XX_PCI2_OFF,
	    OCP85XX_PCI_SIZE},
	{OCPBUS_DEVTYPE_PCIB, 2, SYS_RES_MEMORY, 1, 0, OCP85XX_TGTIF_PCI2},
	{OCPBUS_DEVTYPE_PCIB, 2, SYS_RES_IOPORT, 1, 0, OCP85XX_TGTIF_PCI2},
	{OCPBUS_DEVTYPE_PCIB, 3, SYS_RES_MEMORY, 0, OCP85XX_PCI3_OFF,
	    OCP85XX_PCI_SIZE},
	{OCPBUS_DEVTYPE_PCIB, 3, SYS_RES_MEMORY, 1, 0, OCP85XX_TGTIF_PCI3},
	{OCPBUS_DEVTYPE_PCIB, 3, SYS_RES_IOPORT, 1, 0, OCP85XX_TGTIF_PCI3},

	{OCPBUS_DEVTYPE_LBC, 0, SYS_RES_MEMORY, 0, OCP85XX_LBC_OFF,
	    OCP85XX_LBC_SIZE},

	{OCPBUS_DEVTYPE_I2C, 0, SYS_RES_MEMORY, 0, OCP85XX_I2C0_OFF,
	    OCP85XX_I2C_SIZE},
	{OCPBUS_DEVTYPE_I2C, 0, SYS_RES_IRQ, 0, 27, 1},
	{OCPBUS_DEVTYPE_I2C, 1, SYS_RES_MEMORY, 0, OCP85XX_I2C1_OFF,
	    OCP85XX_I2C_SIZE},
	{OCPBUS_DEVTYPE_I2C, 1, SYS_RES_IRQ, 0, 27, 1},

	{OCPBUS_DEVTYPE_SEC, 0, SYS_RES_MEMORY, 0, OCP85XX_SEC_OFF,
	    OCP85XX_SEC_SIZE},
	{OCPBUS_DEVTYPE_SEC, 0, SYS_RES_IRQ, 0, 29, 1},
	{OCPBUS_DEVTYPE_SEC, 0, SYS_RES_IRQ, 1, 42, 1},

	{0}
};

static int
ocpbus_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	const struct ocp_resource *res;
	struct ocp_devinfo *dinfo;
	u_long start = 0, count = 0;
	int error;

	dinfo = device_get_ivars(child);

	/*
	 * Lookup the correct values.
	 */
	res = mpc8555_resources;
	for (; res->sr_devtype; res++) {
		if (res->sr_devtype != dinfo->ocp_devtype)
			continue;
		if (res->sr_unit != dinfo->ocp_unit)
			continue;
		if (res->sr_rid != rid)
			continue;
		if (res->sr_resource != type)
			continue;

		if (res->sr_offset != 0) {
			error = 0;
			switch (type) {
			case SYS_RES_MEMORY:
				start = res->sr_offset + CCSRBAR_VA;
				break;
			case SYS_RES_IRQ:
				start = PIC_IRQ_INT(res->sr_offset);
				break;
			default:
				error = EINVAL;
				break;
			}
			count = res->sr_size;
		} else
			error = ocpbus_write_law(res->sr_size, type, &start,
			    &count);

		if (!error) {
			if (startp != NULL)
				*startp = start;
			if (countp != NULL)
				*countp = count;
		}
		return (error);
	}

	return (ENOENT);
}

static struct resource *
ocpbus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ocpbus_softc *sc;
	struct resource *rv;
	int error;

	sc = device_get_softc(dev);

	switch (type) {
	case SYS_RES_IRQ:
		if (start == 0ul && end == ~0ul) {
			error = ocpbus_get_resource(dev, child, type, *rid,
			    &start, &count);
			if (error)
				return (NULL);
		}

		rv = rman_reserve_resource(&sc->sc_irq, start,
		    start + count - 1, count, flags, child);
		if (rv == NULL)
			return (NULL);
		break;

	case SYS_RES_MEMORY:
		if (start != 0ul || end != ~0ul)
			return (NULL);

		error = ocpbus_get_resource(dev, child, type, *rid, &start,
		    &count);
		if (error)
			return (NULL);

		rv = rman_reserve_resource(&sc->sc_mem, start,
		    start + count - 1, count, flags, child);
		if (rv == NULL)
			return (NULL);

		rman_set_bustag(rv, &bs_be_tag);
		rman_set_bushandle(rv, rman_get_start(rv));
		break;

	default:
		return (NULL);
	}

	rman_set_rid(rv, *rid);
	return (rv);
}

static int
ocpbus_print_child(device_t dev, device_t child)
{
	u_long size, start;
	int error, retval, rid;

	retval = bus_print_child_header(dev, child);

	rid = 0;
	while (1) {
		error = ocpbus_get_resource(dev, child, SYS_RES_MEMORY, rid,
		    &start, &size);
		if (error)
			break;
		retval += (rid == 0) ? printf(" iomem ") : printf(",");
		retval += printf("%#lx", start);
		if (size > 1)
			retval += printf("-%#lx", start + size - 1);
		rid++;
	}

	/*
	 * The SYS_RES_IOPORT resource of the PCIB has rid 1 because the
	 * the SYS_RES_MEMORY resource related to the decoding window also
	 * has rid 1. This is friendlier for the PCIB child...
	 */
	rid = 1;
	while (1) {
		error = ocpbus_get_resource(dev, child, SYS_RES_IOPORT, rid,
		    &start, &size);
		if (error)
			break;
		retval += (rid == 1) ? printf(" ioport ") : printf(",");
		retval += printf("%#lx", start);
		if (size > 1)
			retval += printf("-%#lx", start + size - 1);
		rid++;
	}

	rid = 0;
	while (1) {
		error = ocpbus_get_resource(dev, child, SYS_RES_IRQ, rid,
		    &start, &size);
		if (error)
			break;
		retval += (rid == 0) ? printf(" irq ") : printf(",");
		retval += printf("%ld", start);
		rid++;
	}

	retval += bus_print_child_footer(dev, child);
	return (retval);
}

static int
ocpbus_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct ocp_devinfo *dinfo;
	struct bi_eth_addr *eth;
	int unit;

	if (device_get_parent(child) != dev)
		return (EINVAL);

	dinfo = device_get_ivars(child);

	switch (index) {
	case OCPBUS_IVAR_CLOCK:
		*result = bootinfo->bi_bus_clk;
		return (0);
	case OCPBUS_IVAR_DEVTYPE:
		*result = dinfo->ocp_devtype;
		return (0);
	case OCPBUS_IVAR_HWUNIT:
		*result = dinfo->ocp_unit;
		return (0);
	case OCPBUS_IVAR_MACADDR:
		unit = device_get_unit(child);
		if (unit > bootinfo->bi_eth_addr_no - 1)
			return (EINVAL);
		eth = bootinfo_eth() + unit;
		*result = (uintptr_t)eth;
		return (0);
	}

	return (EINVAL);
}

static int
ocpbus_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	return (rman_release_resource(res));
}

static int
ocpbus_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_filter_t *filter, driver_intr_t *ihand, void *arg, void **cookiep)
{
	int error;

	if (res == NULL)
		panic("ocpbus_setup_intr: NULL irq resource!");

	*cookiep = 0;
	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = powerpc_setup_intr(device_get_nameunit(child),
	    rman_get_start(res), filter, ihand, arg, flags, cookiep);

	return (error);
}

static int
ocpbus_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{

	return (powerpc_teardown_intr(cookie));
}

static int
ocpbus_config_intr(device_t dev, int irq, enum intr_trigger trig, 
    enum intr_polarity pol)
{

	return (powerpc_config_intr(irq, trig, pol));
}
