/*-
 * Copyright (c) 2008 Rui Paulo <rpaulo@FreeBSD.org>
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
 * Driver for the AMD K8 thermal sensors. Based on a Linux driver by the
 * same name.
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

struct k8temp_softc {
	device_t	sc_dev;
	int		sc_temps[4];
	int		sc_ntemps;
	struct sysctl_oid *sc_oid;
	struct sysctl_oid *sc_sysctl_cpu[2];
	struct intr_config_hook sc_ich;
};

#define VENDORID_AMD		0x1022
#define DEVICEID_AMD_MISC	0x1103

static struct k8temp_product {
	uint16_t	k8temp_vendorid;
	uint16_t	k8temp_deviceid;
} k8temp_products[] = {
	{ VENDORID_AMD,	DEVICEID_AMD_MISC },
	{ 0, 0 }
};

/*
 * Register control
 */
#define	K8TEMP_REG		0xe4
#define	K8TEMP_REG_SELSENSOR	0x40
#define	K8TEMP_REG_SELCORE	0x04

#define K8TEMP_MINTEMP		49	/* -49 C is the mininum temperature */

typedef enum {
	SENSOR0_CORE0,
	SENSOR0_CORE1,
	SENSOR1_CORE0,
	SENSOR1_CORE1,
	CORE0,
	CORE1
} k8sensor_t;

/*
 * Device methods.
 */
static void 	k8temp_identify(driver_t *driver, device_t parent);
static int	k8temp_probe(device_t dev);
static int	k8temp_attach(device_t dev);
static void	k8temp_intrhook(void *arg);
static int	k8temp_detach(device_t dev);
static int 	k8temp_match(device_t dev);
static int32_t	k8temp_gettemp(device_t dev, k8sensor_t sensor);
static int	k8temp_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t k8temp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	k8temp_identify),
	DEVMETHOD(device_probe,		k8temp_probe),
	DEVMETHOD(device_attach,	k8temp_attach),
	DEVMETHOD(device_detach,	k8temp_detach),

	{0, 0}
};

static driver_t k8temp_driver = {
	"k8temp",
	k8temp_methods,
	sizeof(struct k8temp_softc),
};

static devclass_t k8temp_devclass;
DRIVER_MODULE(k8temp, hostb, k8temp_driver, k8temp_devclass, NULL, NULL);

static int
k8temp_match(device_t dev)
{
	int i;
	uint16_t vendor, devid;
	
        vendor = pci_get_vendor(dev);
	devid = pci_get_device(dev);

	for (i = 0; k8temp_products[i].k8temp_vendorid != 0; i++) {
		if (vendor == k8temp_products[i].k8temp_vendorid &&
		    devid == k8temp_products[i].k8temp_deviceid)
			return (1);
	}

	return (0);
}

static void
k8temp_identify(driver_t *driver, device_t parent)
{
	device_t child;

	/* Make sure we're not being doubly invoked. */
	if (device_find_child(parent, "k8temp", -1) != NULL)
		return;
	
	if (k8temp_match(parent)) {
		child = device_add_child(parent, "k8temp", -1);
		if (child == NULL)
			device_printf(parent, "add k8temp child failed\n");
	}
    
}

static int
k8temp_probe(device_t dev)
{
	uint32_t regs[4];
	
	if (resource_disabled("k8temp", 0))
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
k8temp_attach(device_t dev)
{
	struct k8temp_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;


	/*
	 * Setup intrhook function to create dev.cpu sysctl entries. This is
	 * needed because the cpu driver may be loaded late on boot, after
	 * us.
	 */
	sc->sc_ich.ich_func = k8temp_intrhook;
	sc->sc_ich.ich_arg = dev;
	if (config_intrhook_establish(&sc->sc_ich) != 0) {
		device_printf(dev, "config_intrhook_establish "
		    "failed!\n");
		return (ENXIO);
	}
	
	/*
	 * dev.k8temp.N tree.
	 */
	sysctlctx = device_get_sysctl_ctx(dev);
	sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "sensor0",
	    CTLFLAG_RD, 0, "Sensor 0");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core0", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR0_CORE0, k8temp_sysctl, "I",
	    "Sensor 0 / Core 0 temperature");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core1", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR0_CORE1, k8temp_sysctl, "I",
	    "Sensor 0 / Core 1 temperature");
	
	sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "sensor1",
	    CTLFLAG_RD, 0, "Sensor 1");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core0", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR1_CORE0, k8temp_sysctl, "I",
	    "Sensor 1 / Core 0 temperature");
	
	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core1", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR1_CORE1, k8temp_sysctl, "I",
	    "Sensor 1 / Core 1 temperature");

	return (0);
}

void
k8temp_intrhook(void *arg)
{
	int i;
	device_t nexus, acpi, cpu;
	device_t dev = (device_t) arg;
	struct k8temp_softc *sc;
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
			    dev, CORE0, k8temp_sysctl, "I",
			    "Max of sensor 0 / 1");
		}
	}
	config_intrhook_disestablish(&sc->sc_ich);
}

int
k8temp_detach(device_t dev)
{
	int i;
	struct k8temp_softc *sc = device_get_softc(dev);
	
	for (i = 0; i < 2; i++) {
		if (sc->sc_sysctl_cpu[i])
			sysctl_remove_oid(sc->sc_sysctl_cpu[i], 1, 0);
	}

	/* NewBus removes the dev.k8temp.N tree by itself. */
	
	return (0);
}

static int
k8temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int error;
	int32_t temp, auxtemp[2];

	switch (arg2) {
	case CORE0:
		auxtemp[0] = k8temp_gettemp(dev, SENSOR0_CORE0);
		auxtemp[1] = k8temp_gettemp(dev, SENSOR1_CORE0);
		temp = imax(auxtemp[0], auxtemp[1]);
		break;
	case CORE1:
		auxtemp[0] = k8temp_gettemp(dev, SENSOR0_CORE1);
		auxtemp[1] = k8temp_gettemp(dev, SENSOR1_CORE1);
		temp = imax(auxtemp[0], auxtemp[1]);
		break;
	default:
		temp = k8temp_gettemp(dev, arg2);
		break;
	}
	error = sysctl_handle_int(oidp, &temp, 0, req);
	
	return (error);
}

static int32_t
k8temp_gettemp(device_t dev, k8sensor_t sensor)
{
	uint8_t cfg;
	uint32_t temp;
	
	cfg = pci_read_config(dev, K8TEMP_REG, 1);
	switch (sensor) {
	case SENSOR0_CORE0:
		cfg &= ~(K8TEMP_REG_SELSENSOR | K8TEMP_REG_SELCORE);
		break;
	case SENSOR0_CORE1:
		cfg &= ~K8TEMP_REG_SELSENSOR;
		cfg |= K8TEMP_REG_SELCORE;
		break;
	case SENSOR1_CORE0:
		cfg &= ~K8TEMP_REG_SELCORE;
		cfg |= K8TEMP_REG_SELSENSOR;
		break;
	case SENSOR1_CORE1:
		cfg |= (K8TEMP_REG_SELSENSOR | K8TEMP_REG_SELCORE);
		break;
	default:
		cfg = 0;
		break;
	}
	pci_write_config(dev, K8TEMP_REG, cfg, 1);
	temp = pci_read_config(dev, K8TEMP_REG, 4);
	temp = ((temp >> 16) & 0xff) - K8TEMP_MINTEMP;
	
	return (temp);
}
