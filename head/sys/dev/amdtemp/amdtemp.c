/*-
 * Copyright (c) 2008, 2009 Rui Paulo <rpaulo@FreeBSD.org>
 * Copyright (c) 2009 Norikatsu Shigemura <nork@FreeBSD.org>
 * Copyright (c) 2009 Jung-uk Kim <jkim@FreeBSD.org>
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
 * Driver for the AMD CPU on-die thermal sensors for Family 0Fh/10h/11h procs.
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
	int		sc_ncores;
	int		sc_ntemps;
	int		sc_flags;
#define	AMDTEMP_FLAG_DO_QUIRK	0x01	/* DiodeOffset may be incorrect. */
#define	AMDTEMP_FLAG_DO_ZERO	0x02	/* DiodeOffset starts from 0C. */
#define	AMDTEMP_FLAG_DO_SIGN	0x04	/* DiodeOffsetSignBit is present. */
#define	AMDTEMP_FLAG_CS_SWAP	0x08	/* ThermSenseCoreSel is inverted. */
#define	AMDTEMP_FLAG_CT_10BIT	0x10	/* CurTmp is 10-bit wide. */
	int32_t		(*sc_gettemp)(device_t, amdsensor_t);
	struct sysctl_oid *sc_sysctl_cpu[MAXCPU];
	struct intr_config_hook sc_ich;
};

#define	VENDORID_AMD		0x1022
#define	DEVICEID_AMD_MISC0F	0x1103
#define	DEVICEID_AMD_MISC10	0x1203
#define	DEVICEID_AMD_MISC11	0x1303

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
 * Reported Temperature Control Register (Family 10h/11h only)
 */
#define	AMDTEMP_REPTMP_CTRL	0xa4

/*
 * Thermaltrip Status Register
 */
#define	AMDTEMP_THERMTP_STAT	0xe4
#define	AMDTEMP_TTSR_SELCORE	0x04	/* Family 0Fh only */
#define	AMDTEMP_TTSR_SELSENSOR	0x40	/* Family 0Fh only */

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
	uint32_t family, model;

	if (resource_disabled("amdtemp", 0))
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
	struct amdtemp_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;
	uint32_t regs[4];
	uint32_t cpuid, family, model;

	/*
	 * Errata #154: Incorect Diode Offset
	 */
	if (cpu_id == 0x20f32) {
		do_cpuid(0x80000001, regs);
		if ((regs[1] & 0xfff) == 0x2c)
			sc->sc_flags |= AMDTEMP_FLAG_DO_QUIRK;
	}

	/*
	 * CPUID Register is available from Revision F.
	 */
	family = CPUID_TO_FAMILY(cpu_id);
	model = CPUID_TO_MODEL(cpu_id);
	if (family != 0x0f || model >= 0x40) {
		cpuid = pci_read_config(dev, AMDTEMP_CPUID, 4);
		family = CPUID_TO_FAMILY(cpuid);
		model = CPUID_TO_MODEL(cpuid);
	}

	switch (family) {
	case 0x0f:
		/*
		 * Thermaltrip Status Register
		 *
		 * - DiodeOffsetSignBit
		 *
		 * Revision D & E:	bit 24
		 * Other:		N/A
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
		 */
		if (model < 0x40) {
			sc->sc_flags |= AMDTEMP_FLAG_DO_ZERO;
			if (model >= 0x10)
				sc->sc_flags |= AMDTEMP_FLAG_DO_SIGN;
		} else {
			sc->sc_flags |= AMDTEMP_FLAG_CS_SWAP;
			if (model >= 0x60 && model != 0xc1)
				sc->sc_flags |= AMDTEMP_FLAG_CT_10BIT;
		}

		/*
		 * There are two sensors per core.
		 */
		sc->sc_ntemps = 2;

		sc->sc_gettemp = amdtemp_gettemp0f;
		break;
	case 0x10:
	case 0x11:
		/*
		 * There is only one sensor per package.
		 */
		sc->sc_ntemps = 1;

		sc->sc_gettemp = amdtemp_gettemp;
		break;
	}

	/* Find number of cores per package. */
	sc->sc_ncores = (amd_feature2 & AMDID2_CMP) != 0 ?
	    (cpu_procinfo2 & AMDID_CMP_CORES) + 1 : 1;
	if (sc->sc_ncores > MAXCPU)
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "Found %d cores and %d sensors.\n",
		    sc->sc_ncores,
		    sc->sc_ntemps > 1 ? sc->sc_ntemps * sc->sc_ncores : 1);

	/*
	 * dev.amdtemp.N tree.
	 */
	sysctlctx = device_get_sysctl_ctx(dev);
	sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "sensor0", CTLFLAG_RD, 0, "Sensor 0");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode),
	    OID_AUTO, "core0", CTLTYPE_INT | CTLFLAG_RD,
	    dev, SENSOR0_CORE0, amdtemp_sysctl, "IK",
	    "Sensor 0 / Core 0 temperature");

	if (sc->sc_ntemps > 1) {
		if (sc->sc_ncores > 1)
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sysctlnode),
			    OID_AUTO, "core1", CTLTYPE_INT | CTLFLAG_RD,
			    dev, SENSOR0_CORE1, amdtemp_sysctl, "IK",
			    "Sensor 0 / Core 1 temperature");

		sysctlnode = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "sensor1", CTLFLAG_RD, 0, "Sensor 1");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sysctlnode),
		    OID_AUTO, "core0", CTLTYPE_INT | CTLFLAG_RD,
		    dev, SENSOR1_CORE0, amdtemp_sysctl, "IK",
		    "Sensor 1 / Core 0 temperature");

		if (sc->sc_ncores > 1)
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(sysctlnode),
			    OID_AUTO, "core1", CTLTYPE_INT | CTLFLAG_RD,
			    dev, SENSOR1_CORE1, amdtemp_sysctl, "IK",
			    "Sensor 1 / Core 1 temperature");
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
			    (i == 0 ? CORE0 : CORE1) : SENSOR0_CORE0;
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
		temp = sc->sc_gettemp(dev, sensor);
		break;
	}
	error = sysctl_handle_int(oidp, &temp, 0, req);

	return (error);
}

#define	AMDTEMP_ZERO_C_TO_K	2732

static int32_t
amdtemp_gettemp0f(device_t dev, amdsensor_t sensor)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	uint32_t mask, temp;
	int32_t diode_offset, offset;
	uint8_t cfg, sel;

	/* Set Sensor/Core selector. */
	sel = 0;
	switch (sensor) {
	case SENSOR1_CORE0:
		sel |= AMDTEMP_TTSR_SELSENSOR;
		/* FALLTHROUGH */
	case SENSOR0_CORE0:
	case CORE0:
		if ((sc->sc_flags & AMDTEMP_FLAG_CS_SWAP) != 0)
			sel |= AMDTEMP_TTSR_SELCORE;
		break;
	case SENSOR1_CORE1:
		sel |= AMDTEMP_TTSR_SELSENSOR;
		/* FALLTHROUGH */
	case SENSOR0_CORE1:
	case CORE1:
		if ((sc->sc_flags & AMDTEMP_FLAG_CS_SWAP) == 0)
			sel |= AMDTEMP_TTSR_SELCORE;
		break;
	}
	cfg = pci_read_config(dev, AMDTEMP_THERMTP_STAT, 1);
	cfg &= ~(AMDTEMP_TTSR_SELSENSOR | AMDTEMP_TTSR_SELCORE);
	pci_write_config(dev, AMDTEMP_THERMTP_STAT, cfg | sel, 1);

	/* CurTmp starts from -49C. */
	offset = AMDTEMP_ZERO_C_TO_K - 490;

	/* Adjust offset if DiodeOffset is set and valid. */
	temp = pci_read_config(dev, AMDTEMP_THERMTP_STAT, 4);
	diode_offset = (temp >> 8) & 0x3f;
	if ((sc->sc_flags & AMDTEMP_FLAG_DO_ZERO) != 0) {
		if ((sc->sc_flags & AMDTEMP_FLAG_DO_SIGN) != 0 &&
		    ((temp >> 24) & 0x1) != 0)
			diode_offset *= -1;
		if ((sc->sc_flags & AMDTEMP_FLAG_DO_QUIRK) != 0 &&
		    ((temp >> 25) & 0xf) <= 2)
			diode_offset += 10;
		offset += diode_offset * 10;
	} else if (diode_offset != 0)
		offset += (diode_offset - 11) * 10;

	mask = (sc->sc_flags & AMDTEMP_FLAG_CT_10BIT) != 0 ? 0x3ff : 0x3fc;
	temp = ((temp >> 14) & mask) * 5 / 2 + offset;

	return (temp);
}

static int32_t
amdtemp_gettemp(device_t dev, amdsensor_t sensor)
{
	uint32_t temp;
	int32_t diode_offset, offset;

	/* CurTmp starts from 0C. */
	offset = AMDTEMP_ZERO_C_TO_K;

	/* Adjust offset if DiodeOffset is set and valid. */
	temp = pci_read_config(dev, AMDTEMP_THERMTP_STAT, 4);
	diode_offset = (temp >> 8) & 0x7f;
	if (diode_offset > 0 && diode_offset < 0x40)
		offset += (diode_offset - 11) * 10;

	temp = pci_read_config(dev, AMDTEMP_REPTMP_CTRL, 4);
	temp = ((temp >> 21) & 0x7ff) * 5 / 4 + offset;

	return (temp);
}
