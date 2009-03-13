/*-
 * Copyright (c) 2008, 2009 Rui Paulo <rpaulo@FreeBSD.org>
 * Copyright (c) 2009 Norikatsu Shigemura <nork@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the AMD K8/K10/K11 thermal sensors. Initially based on the
 * amdtemp Linux driver.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/specialreg.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

typedef enum {
	SENSOR0_CORE0,
	SENSOR0_CORE1,
	SENSOR1_CORE0,
	SENSOR1_CORE1,
	CORE0,
	CORE1
} amdsensor_t;

struct amdtemp_softc {
	device_t	sc_dev;
	int		sc_temps[4];
	int		sc_ntemps;
	struct sysctl_oid *sc_oid;
	struct sysctl_oid *sc_sysctl_cpu[2];
	struct intr_config_hook sc_ich;
	int32_t (*sc_gettemp)(device_t, amdsensor_t);
};

#define VENDORID_AMD		0x1022
#define DEVICEID_AMD_MISC0F	0x1103
#define DEVICEID_AMD_MISC10	0x1203
#define DEVICEID_AMD_MISC11	0x1303

static struct amdtemp_product {
	uint16_t	amdtemp_vendorid;
	uint16_t	amdtemp_deviceid;
} amdtemp_products[] = {
	{ VENDORID_AMD,	DEVICEID_AMD_MISC0F },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC10 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC11 },
	{ 0, 0 }
};

/*
 * Register control (K8 family)
 */
#define	AMDTEMP_REG0F		0xe4
#define	AMDTEMP_REG_SELSENSOR	0x40
#define	AMDTEMP_REG_SELCORE	0x04

/*
 * Register control (K10 & K11) family
 */
#define	AMDTEMP_REG		0xa4

#define	TZ_ZEROC		2732

					/* -49 C is the mininum temperature */
#define	AMDTEMP_OFFSET0F	(TZ_ZEROC-490)
#define	AMDTEMP_OFFSET		(TZ_ZEROC)

/*
 * Device methods.
 */
static void 	amdtemp_identify(driver_t *driver, device_t parent);
static int	amdtemp_probe(device_t dev);
static int	amdtemp_attach(device_t dev);
static void	amdtemp_intrhook(void *arg);
static int	amdtemp_detach(device_t dev);
static int 	amdtemp_match(device_t dev);
static int32_t	amdtemp_gettemp0f(device_t dev, amdsensor_t sensor);
static int32_t	amdtemp_gettemp(device_t dev, amdsensor_t sensor);
static int	amdtemp_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t amdtemp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	amdtemp_identify),
	DEVMETHOD(device_probe,		amdtemp_probe),
	DEVMETHOD(device_attach,	amdtemp_attach),
	DEVMETHOD(device_detach,	amdtemp_detach),

	{0, 0}
};

static driver_t amdtemp_driver = {
	"amdtemp",
	amdtemp_methods,
	sizeof(struct amdtemp_softc),
};

static devclass_t amdtemp_devclass;
DRIVER_MODULE(amdtemp, hostb, amdtemp_driver, amdtemp_devclass, NULL, NULL);

static int
amdtemp_match(device_t dev)
{
	int i;
	uint16_t vendor, devid;
	
        vendor = pci_get_vendor(dev);
	devid = pci_get_device(dev);

	for (i = 0; amdtemp_products[i].amdtemp_vendorid != 0; i++) {
		if (vendor == amdtemp_products[i].amdtemp_vendorid &&
		    devid == amdtemp_products[i].amdtemp_deviceid)
			return (1);
	}

	return (0);
}

static void
amdtemp_identify(driver_t *driver, device_t parent)
{
	device_t child;

	/* Make sure we're not being doubly invoked. */
	if (device_find_child(parent, "amdtemp", -1) != NULL)
		return;
	
	if (amdtemp_match(parent)) {
		child = device_add_child(parent, "amdtemp", -1);
		if (child == NULL)
			device_printf(parent, "add amdtemp child failed\n");
	}
    
}

static int
amdtemp_probe(device_t dev)
{
	uint32_t regs[4];
	
	if (resource_disabled("amdtemp", 0))
		return (ENXIO);

	do_cpuid(1, regs);
	switch (regs[0]) {
	case 0xf40:
	case 0xf50:
	case 0xf51:
		return (ENXIO);
	}
	device_set_desc(dev, "AMD K8 Thermal Sensors");
	
	return (BUS_PROBE_GENERIC);
}

static int
amdtemp_attach(device_t dev)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;


	/*
	 * Setup intrhook function to create dev.cpu sysctl entries. This is
	 * needed because the cpu driver may be loaded late on boot, after
	 * us.
	 */
	sc->sc_ich.ich_func = amdtemp_intrhook;
	sc->sc_ich.ich_arg = dev;
	if (config_intrhook_establish(&sc->sc_ich) != 0) {
		device_printf(dev, "config_intrhook_establish "
		    "failed!\n");
		return (ENXIO);
	}
	
	if (pci_get_device(dev) == DEVICEID_AMD_MISC0F)
		sc->sc_gettemp = amdtemp_gettemp0f;
	else {
		sc->sc_gettemp = amdtemp_gettemp;
		return (0);
	}

	/*
	 * dev.amdtemp.N tree.
	 */
	sysctlctx = device_get_sysctl_ctx(dev);
	sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "sensor0",
	    CTLFLAG_RD, 0, "Sensor 0");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core0", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR0_CORE0, amdtemp_sysctl, "IK",
	    "Sensor 0 / Core 0 temperature");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core1", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR0_CORE1, amdtemp_sysctl, "IK",
	    "Sensor 0 / Core 1 temperature");
	
	sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "sensor1",
	    CTLFLAG_RD, 0, "Sensor 1");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core0", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR1_CORE0, amdtemp_sysctl, "IK",
	    "Sensor 1 / Core 0 temperature");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core1", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR1_CORE1, amdtemp_sysctl, "IK",
	    "Sensor 1 / Core 1 temperature");

	return (0);
}

void
amdtemp_intrhook(void *arg)
{
	int i;
	device_t nexus, acpi, cpu;
	device_t dev = (device_t) arg;
	struct amdtemp_softc *sc;
	struct sysctl_ctx_list *sysctlctx;

	sc = device_get_softc(dev);
	
	/*
	 * dev.cpu.N.temperature.
	 */
	nexus = device_find_child(root_bus, "nexus", 0);
	acpi = device_find_child(nexus, "acpi", 0);

	for (i = 0; i < 2; i++) {
		cpu = device_find_child(acpi, "cpu",
		    device_get_unit(dev) * 2 + i);
		if (cpu) {
			sysctlctx = device_get_sysctl_ctx(cpu);

			sc->sc_sysctl_cpu[i] = SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(device_get_sysctl_tree(cpu)),
			    OID_AUTO, "temperature", CTLTYPE_INT | CTLFLAG_RD,
			    dev, CORE0, amdtemp_sysctl, "IK",
			    "Max of sensor 0 / 1");
		}
	}
	config_intrhook_disestablish(&sc->sc_ich);
}

int
amdtemp_detach(device_t dev)
{
	int i;
	struct amdtemp_softc *sc = device_get_softc(dev);
	
	for (i = 0; i < 2; i++) {
		if (sc->sc_sysctl_cpu[i])
			sysctl_remove_oid(sc->sc_sysctl_cpu[i], 1, 0);
	}

	/* NewBus removes the dev.amdtemp.N tree by itself. */
	
	return (0);
}

static int
amdtemp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct amdtemp_softc *sc = device_get_softc(dev);
	int error;
	int32_t temp, auxtemp[2];

	switch (arg2) {
	case CORE0:
		auxtemp[0] = sc->sc_gettemp(dev, SENSOR0_CORE0);
		auxtemp[1] = sc->sc_gettemp(dev, SENSOR1_CORE0);
		temp = imax(auxtemp[0], auxtemp[1]);
		break;
	case CORE1:
		auxtemp[0] = sc->sc_gettemp(dev, SENSOR0_CORE1);
		auxtemp[1] = sc->sc_gettemp(dev, SENSOR1_CORE1);
		temp = imax(auxtemp[0], auxtemp[1]);
		break;
	default:
		temp = sc->sc_gettemp(dev, arg2);
		break;
	}
	error = sysctl_handle_int(oidp, &temp, 0, req);
	
	return (error);
}

static int32_t
amdtemp_gettemp0f(device_t dev, amdsensor_t sensor)
{
	uint8_t cfg;
	uint32_t temp;
	
	cfg = pci_read_config(dev, AMDTEMP_REG0F, 1);
	switch (sensor) {
	case SENSOR0_CORE0:
		cfg &= ~(AMDTEMP_REG_SELSENSOR | AMDTEMP_REG_SELCORE);
		break;
	case SENSOR0_CORE1:
		cfg &= ~AMDTEMP_REG_SELSENSOR;
		cfg |= AMDTEMP_REG_SELCORE;
		break;
	case SENSOR1_CORE0:
		cfg &= ~AMDTEMP_REG_SELCORE;
		cfg |= AMDTEMP_REG_SELSENSOR;
		break;
	case SENSOR1_CORE1:
		cfg |= (AMDTEMP_REG_SELSENSOR | AMDTEMP_REG_SELCORE);
		break;
	default:
		cfg = 0;
		break;
	}
	pci_write_config(dev, AMDTEMP_REG0F, cfg, 1);
	temp = pci_read_config(dev, AMDTEMP_REG0F, 4);
	temp = ((temp >> 16) & 0xff) * 10 + AMDTEMP_OFFSET0F;
	
	return (temp);
}

static int32_t
amdtemp_gettemp(device_t dev, amdsensor_t sensor)
{
	uint32_t temp;

	temp = pci_read_config(dev, AMDTEMP_REG, 4);
	temp = ((temp >> 21) & 0x3ff) * 10 / 8 + AMDTEMP_OFFSET;

	return (temp);
}
