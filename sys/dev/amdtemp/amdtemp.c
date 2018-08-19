/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2008, 2009 Rui Paulo <rpaulo@FreeBSD.org>
 * Copyright (c) 2009 Norikatsu Shigemura <nork@FreeBSD.org>
 * Copyright (c) 2009-2012 Jung-uk Kim <jkim@FreeBSD.org>
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
 * Driver for the AMD CPU on-die thermal sensors.
 * Initially based on the k8temp Linux driver.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/pci/pcivar.h>
#include <x86/pci_cfgreg.h>

#include <dev/amdsmn/amdsmn.h>

typedef enum {
	CORE0_SENSOR0,
	CORE0_SENSOR1,
	CORE1_SENSOR0,
	CORE1_SENSOR1,
	CORE0,
	CORE1
} amdsensor_t;

struct amdtemp_softc {
	int		sc_ncores;
	int		sc_ntemps;
	int		sc_flags;
#define	AMDTEMP_FLAG_CS_SWAP	0x01	/* ThermSenseCoreSel is inverted. */
#define	AMDTEMP_FLAG_CT_10BIT	0x02	/* CurTmp is 10-bit wide. */
#define	AMDTEMP_FLAG_ALT_OFFSET	0x04	/* CurTmp starts at -28C. */
	int32_t		sc_offset;
	int32_t		(*sc_gettemp)(device_t, amdsensor_t);
	struct sysctl_oid *sc_sysctl_cpu[MAXCPU];
	struct intr_config_hook sc_ich;
	device_t	sc_smn;
};

#define	VENDORID_AMD		0x1022
#define	DEVICEID_AMD_MISC0F	0x1103
#define	DEVICEID_AMD_MISC10	0x1203
#define	DEVICEID_AMD_MISC11	0x1303
#define	DEVICEID_AMD_MISC12	0x1403
#define	DEVICEID_AMD_MISC14	0x1703
#define	DEVICEID_AMD_MISC15	0x1603
#define	DEVICEID_AMD_MISC16	0x1533
#define	DEVICEID_AMD_MISC16_M30H	0x1583
#define	DEVICEID_AMD_MISC17	0x141d
#define	DEVICEID_AMD_HOSTB17H	0x1450

static struct amdtemp_product {
	uint16_t	amdtemp_vendorid;
	uint16_t	amdtemp_deviceid;
} amdtemp_products[] = {
	{ VENDORID_AMD,	DEVICEID_AMD_MISC0F },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC10 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC11 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC12 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC14 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC15 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC16 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC16_M30H },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC17 },
	{ VENDORID_AMD,	DEVICEID_AMD_HOSTB17H },
};

/*
 * Reported Temperature Control Register
 */
#define	AMDTEMP_REPTMP_CTRL	0xa4

/*
 * Reported Temperature, Family 17h
 */
#define	AMDTEMP_17H_CUR_TMP	0x59800

/*
 * Thermaltrip Status Register (Family 0Fh only)
 */
#define	AMDTEMP_THERMTP_STAT	0xe4
#define	AMDTEMP_TTSR_SELCORE	0x04
#define	AMDTEMP_TTSR_SELSENSOR	0x40

/*
 * DRAM Configuration High Register
 */
#define	AMDTEMP_DRAM_CONF_HIGH	0x94	/* Function 2 */
#define	AMDTEMP_DRAM_MODE_DDR3	0x0100

/*
 * CPU Family/Model Register
 */
#define	AMDTEMP_CPUID		0xfc

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
static int32_t	amdtemp_gettemp17h(device_t dev, amdsensor_t sensor);
static int	amdtemp_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t amdtemp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	amdtemp_identify),
	DEVMETHOD(device_probe,		amdtemp_probe),
	DEVMETHOD(device_attach,	amdtemp_attach),
	DEVMETHOD(device_detach,	amdtemp_detach),

	DEVMETHOD_END
};

static driver_t amdtemp_driver = {
	"amdtemp",
	amdtemp_methods,
	sizeof(struct amdtemp_softc),
};

static devclass_t amdtemp_devclass;
DRIVER_MODULE(amdtemp, hostb, amdtemp_driver, amdtemp_devclass, NULL, NULL);
MODULE_VERSION(amdtemp, 1);
MODULE_DEPEND(amdtemp, amdsmn, 1, 1, 1);
MODULE_PNP_INFO("U16:vendor;U16:device", pci, amdtemp, amdtemp_products,
    sizeof(amdtemp_products[0]), nitems(amdtemp_products));

static int
amdtemp_match(device_t dev)
{
	int i;
	uint16_t vendor, devid;

	vendor = pci_get_vendor(dev);
	devid = pci_get_device(dev);

	for (i = 0; i < nitems(amdtemp_products); i++) {
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
	uint32_t family, model;

	if (resource_disabled("amdtemp", 0))
		return (ENXIO);
	if (!amdtemp_match(device_get_parent(dev)))
		return (ENXIO);

	family = CPUID_TO_FAMILY(cpu_id);
	model = CPUID_TO_MODEL(cpu_id);

	switch (family) {
	case 0x0f:
		if ((model == 0x04 && (cpu_id & CPUID_STEPPING) == 0) ||
		    (model == 0x05 && (cpu_id & CPUID_STEPPING) <= 1))
			return (ENXIO);
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
		break;
	default:
		return (ENXIO);
	}
	device_set_desc(dev, "AMD CPU On-Die Thermal Sensors");

	return (BUS_PROBE_GENERIC);
}

static int
amdtemp_attach(device_t dev)
{
	char tn[32];
	u_int regs[4];
	struct amdtemp_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;
	uint32_t cpuid, family, model;
	u_int bid;
	int erratum319, unit;

	erratum319 = 0;

	/*
	 * CPUID Register is available from Revision F.
	 */
	cpuid = cpu_id;
	family = CPUID_TO_FAMILY(cpuid);
	model = CPUID_TO_MODEL(cpuid);
	if ((family != 0x0f || model >= 0x40) && family != 0x17) {
		cpuid = pci_read_config(dev, AMDTEMP_CPUID, 4);
		family = CPUID_TO_FAMILY(cpuid);
		model = CPUID_TO_MODEL(cpuid);
	}

	switch (family) {
	case 0x0f:
		/*
		 * Thermaltrip Status Register
		 *
		 * - ThermSenseCoreSel
		 *
		 * Revision F & G:	0 - Core1, 1 - Core0
		 * Other:		0 - Core0, 1 - Core1
		 *
		 * - CurTmp
		 *
		 * Revision G:		bits 23-14
		 * Other:		bits 23-16
		 *
		 * XXX According to the BKDG, CurTmp, ThermSenseSel and
		 * ThermSenseCoreSel bits were introduced in Revision F
		 * but CurTmp seems working fine as early as Revision C.
		 * However, it is not clear whether ThermSenseSel and/or
		 * ThermSenseCoreSel work in undocumented cases as well.
		 * In fact, the Linux driver suggests it may not work but
		 * we just assume it does until we find otherwise.
		 *
		 * XXX According to Linux, CurTmp starts at -28C on
		 * Socket AM2 Revision G processors, which is not
		 * documented anywhere.
		 */
		if (model >= 0x40)
			sc->sc_flags |= AMDTEMP_FLAG_CS_SWAP;
		if (model >= 0x60 && model != 0xc1) {
			do_cpuid(0x80000001, regs);
			bid = (regs[1] >> 9) & 0x1f;
			switch (model) {
			case 0x68: /* Socket S1g1 */
			case 0x6c:
			case 0x7c:
				break;
			case 0x6b: /* Socket AM2 and ASB1 (2 cores) */
				if (bid != 0x0b && bid != 0x0c)
					sc->sc_flags |=
					    AMDTEMP_FLAG_ALT_OFFSET;
				break;
			case 0x6f: /* Socket AM2 and ASB1 (1 core) */
			case 0x7f:
				if (bid != 0x07 && bid != 0x09 &&
				    bid != 0x0c)
					sc->sc_flags |=
					    AMDTEMP_FLAG_ALT_OFFSET;
				break;
			default:
				sc->sc_flags |= AMDTEMP_FLAG_ALT_OFFSET;
			}
			sc->sc_flags |= AMDTEMP_FLAG_CT_10BIT;
		}

		/*
		 * There are two sensors per core.
		 */
		sc->sc_ntemps = 2;

		sc->sc_gettemp = amdtemp_gettemp0f;
		break;
	case 0x10:
		/*
		 * Erratum 319 Inaccurate Temperature Measurement
		 *
		 * http://support.amd.com/us/Processor_TechDocs/41322.pdf
		 */
		do_cpuid(0x80000001, regs);
		switch ((regs[1] >> 28) & 0xf) {
		case 0:	/* Socket F */
			erratum319 = 1;
			break;
		case 1:	/* Socket AM2+ or AM3 */
			if ((pci_cfgregread(pci_get_bus(dev),
			    pci_get_slot(dev), 2, AMDTEMP_DRAM_CONF_HIGH, 2) &
			    AMDTEMP_DRAM_MODE_DDR3) != 0 || model > 0x04 ||
			    (model == 0x04 && (cpuid & CPUID_STEPPING) >= 3))
				break;
			/* XXX 00100F42h (RB-C2) exists in both formats. */
			erratum319 = 1;
			break;
		}
		/* FALLTHROUGH */
	case 0x11:
	case 0x12:
	case 0x14:
	case 0x15:
	case 0x16:
		/*
		 * There is only one sensor per package.
		 */
		sc->sc_ntemps = 1;

		sc->sc_gettemp = amdtemp_gettemp;
		break;
	case 0x17:
		sc->sc_ntemps = 1;
		sc->sc_gettemp = amdtemp_gettemp17h;
		sc->sc_smn = device_find_child(
		    device_get_parent(dev), "amdsmn", -1);
		if (sc->sc_smn == NULL) {
			if (bootverbose)
				device_printf(dev, "No SMN device found\n");
			return (ENXIO);
		}
		break;
	}

	/* Find number of cores per package. */
	sc->sc_ncores = (amd_feature2 & AMDID2_CMP) != 0 ?
	    (cpu_procinfo2 & AMDID_CMP_CORES) + 1 : 1;
	if (sc->sc_ncores > MAXCPU)
		return (ENXIO);

	if (erratum319)
		device_printf(dev,
		    "Erratum 319: temperature measurement may be inaccurate\n");
	if (bootverbose)
		device_printf(dev, "Found %d cores and %d sensors.\n",
		    sc->sc_ncores,
		    sc->sc_ntemps > 1 ? sc->sc_ntemps * sc->sc_ncores : 1);

	/*
	 * dev.amdtemp.N tree.
	 */
	unit = device_get_unit(dev);
	snprintf(tn, sizeof(tn), "dev.amdtemp.%d.sensor_offset", unit);
	TUNABLE_INT_FETCH(tn, &sc->sc_offset);

	sysctlctx = device_get_sysctl_ctx(dev);
	SYSCTL_ADD_INT(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "sensor_offset", CTLFLAG_RW, &sc->sc_offset, 0,
	    "Temperature sensor offset");
	sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "core0", CTLFLAG_RD, 0, "Core 0");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "sensor0", CTLTYPE_INT | CTLFLAG_RD,
	    dev, CORE0_SENSOR0, amdtemp_sysctl, "IK",
	    "Core 0 / Sensor 0 temperature");

	if (sc->sc_ntemps > 1) {
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sysctlnode),
		    OID_AUTO, "sensor1", CTLTYPE_INT | CTLFLAG_RD,
		    dev, CORE0_SENSOR1, amdtemp_sysctl, "IK",
		    "Core 0 / Sensor 1 temperature");

		if (sc->sc_ncores > 1) {
			sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
			    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			    OID_AUTO, "core1", CTLFLAG_RD, 0, "Core 1");

			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sysctlnode),
			    OID_AUTO, "sensor0", CTLTYPE_INT | CTLFLAG_RD,
			    dev, CORE1_SENSOR0, amdtemp_sysctl, "IK",
			    "Core 1 / Sensor 0 temperature");

			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sysctlnode),
			    OID_AUTO, "sensor1", CTLTYPE_INT | CTLFLAG_RD,
			    dev, CORE1_SENSOR1, amdtemp_sysctl, "IK",
			    "Core 1 / Sensor 1 temperature");
		}
	}

	/*
	 * Try to create dev.cpu sysctl entries and setup intrhook function.
	 * This is needed because the cpu driver may be loaded late on boot,
	 * after us.
	 */
	amdtemp_intrhook(dev);
	sc->sc_ich.ich_func = amdtemp_intrhook;
	sc->sc_ich.ich_arg = dev;
	if (config_intrhook_establish(&sc->sc_ich) != 0) {
		device_printf(dev, "config_intrhook_establish failed!\n");
		return (ENXIO);
	}

	return (0);
}

void
amdtemp_intrhook(void *arg)
{
	struct amdtemp_softc *sc;
	struct sysctl_ctx_list *sysctlctx;
	device_t dev = (device_t)arg;
	device_t acpi, cpu, nexus;
	amdsensor_t sensor;
	int i;

	sc = device_get_softc(dev);

	/*
	 * dev.cpu.N.temperature.
	 */
	nexus = device_find_child(root_bus, "nexus", 0);
	acpi = device_find_child(nexus, "acpi", 0);

	for (i = 0; i < sc->sc_ncores; i++) {
		if (sc->sc_sysctl_cpu[i] != NULL)
			continue;
		cpu = device_find_child(acpi, "cpu",
		    device_get_unit(dev) * sc->sc_ncores + i);
		if (cpu != NULL) {
			sysctlctx = device_get_sysctl_ctx(cpu);

			sensor = sc->sc_ntemps > 1 ?
			    (i == 0 ? CORE0 : CORE1) : CORE0_SENSOR0;
			sc->sc_sysctl_cpu[i] = SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(device_get_sysctl_tree(cpu)),
			    OID_AUTO, "temperature", CTLTYPE_INT | CTLFLAG_RD,
			    dev, sensor, amdtemp_sysctl, "IK",
			    "Current temparature");
		}
	}
	if (sc->sc_ich.ich_arg != NULL)
		config_intrhook_disestablish(&sc->sc_ich);
}

int
amdtemp_detach(device_t dev)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_ncores; i++)
		if (sc->sc_sysctl_cpu[i] != NULL)
			sysctl_remove_oid(sc->sc_sysctl_cpu[i], 1, 0);

	/* NewBus removes the dev.amdtemp.N tree by itself. */

	return (0);
}

static int
amdtemp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	struct amdtemp_softc *sc = device_get_softc(dev);
	amdsensor_t sensor = (amdsensor_t)arg2;
	int32_t auxtemp[2], temp;
	int error;

	switch (sensor) {
	case CORE0:
		auxtemp[0] = sc->sc_gettemp(dev, CORE0_SENSOR0);
		auxtemp[1] = sc->sc_gettemp(dev, CORE0_SENSOR1);
		temp = imax(auxtemp[0], auxtemp[1]);
		break;
	case CORE1:
		auxtemp[0] = sc->sc_gettemp(dev, CORE1_SENSOR0);
		auxtemp[1] = sc->sc_gettemp(dev, CORE1_SENSOR1);
		temp = imax(auxtemp[0], auxtemp[1]);
		break;
	default:
		temp = sc->sc_gettemp(dev, sensor);
		break;
	}
	error = sysctl_handle_int(oidp, &temp, 0, req);

	return (error);
}

#define	AMDTEMP_ZERO_C_TO_K	2731

static int32_t
amdtemp_gettemp0f(device_t dev, amdsensor_t sensor)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	uint32_t mask, offset, temp;

	/* Set Sensor/Core selector. */
	temp = pci_read_config(dev, AMDTEMP_THERMTP_STAT, 1);
	temp &= ~(AMDTEMP_TTSR_SELCORE | AMDTEMP_TTSR_SELSENSOR);
	switch (sensor) {
	case CORE0_SENSOR1:
		temp |= AMDTEMP_TTSR_SELSENSOR;
		/* FALLTHROUGH */
	case CORE0_SENSOR0:
	case CORE0:
		if ((sc->sc_flags & AMDTEMP_FLAG_CS_SWAP) != 0)
			temp |= AMDTEMP_TTSR_SELCORE;
		break;
	case CORE1_SENSOR1:
		temp |= AMDTEMP_TTSR_SELSENSOR;
		/* FALLTHROUGH */
	case CORE1_SENSOR0:
	case CORE1:
		if ((sc->sc_flags & AMDTEMP_FLAG_CS_SWAP) == 0)
			temp |= AMDTEMP_TTSR_SELCORE;
		break;
	}
	pci_write_config(dev, AMDTEMP_THERMTP_STAT, temp, 1);

	mask = (sc->sc_flags & AMDTEMP_FLAG_CT_10BIT) != 0 ? 0x3ff : 0x3fc;
	offset = (sc->sc_flags & AMDTEMP_FLAG_ALT_OFFSET) != 0 ? 28 : 49;
	temp = pci_read_config(dev, AMDTEMP_THERMTP_STAT, 4);
	temp = ((temp >> 14) & mask) * 5 / 2;
	temp += AMDTEMP_ZERO_C_TO_K + (sc->sc_offset - offset) * 10;

	return (temp);
}

static int32_t
amdtemp_gettemp(device_t dev, amdsensor_t sensor)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	uint32_t temp;

	temp = pci_read_config(dev, AMDTEMP_REPTMP_CTRL, 4);
	temp = ((temp >> 21) & 0x7ff) * 5 / 4;
	temp += AMDTEMP_ZERO_C_TO_K + sc->sc_offset * 10;

	return (temp);
}

static int32_t
amdtemp_gettemp17h(device_t dev, amdsensor_t sensor)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	uint32_t temp;
	int error;

	error = amdsmn_read(sc->sc_smn, AMDTEMP_17H_CUR_TMP, &temp);
	KASSERT(error == 0, ("amdsmn_read"));

	temp = ((temp >> 21) & 0x7ff) * 5 / 4;
	temp += AMDTEMP_ZERO_C_TO_K + sc->sc_offset * 10;

	return (temp);
}
