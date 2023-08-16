/*
 * Copyright (c) 2020 Takanori Watanabe
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define PCHTHERM_REG_TEMP 0
#define PCHTHERM_REG_TSC 4
#define PCHTHERM_REG_TSS 6
#define PCHTHERM_REG_TSEL 8
#define PCHTHERM_REG_TSREL 0xa
#define PCHTHERM_REG_TSMIC 0xc
#define PCHTHERM_REG_CTT 0x10
#define PCHTHERM_REG_TAHV 0x14
#define PCHTHERM_REG_TALV 0x18
#define PCHTHERM_REG_TSPM 0x1c
#define PCHTHERM_REG_TL 0x40
#define PCHTHERM_REG_TL2 0x50
#define PCHTHERM_REG_PHL 0x60
#define PCHTHERM_REG_PHLC 0x62
#define PCHTHERM_REG_TAS 0x80
#define PCHTHERM_REG_TSPIEN 0x82
#define PCHTHERM_REG_TSGPEN 0x84
#define PCHTHERM_REG_TCFD 0xf0
#define PCHTHERM_GEN_LOCKDOWN 0x80
#define PCHTHERM_GEN_ENABLE 1
#define PCHTHERM_TEMP_FACTOR 5
#define PCHTHERM_TEMP_ZERO 2231
#define PCHTHERM_TEMP_MASK 0x1ff
#define PCHTHERM_TL_T0 0
#define PCHTHERM_TL_T1 10
#define PCHTHERM_TL_T2 20
#define PCHTHERM_TEMP_TO_IK(val) (((val) & PCHTHERM_TEMP_MASK) * \
				  PCHTHERM_TEMP_FACTOR +       \
				  PCHTHERM_TEMP_ZERO)

struct pchtherm_softc
{
	int tbarrid;
	struct resource *tbar;
	int enable;
	int ctten;
	int pmtemp;
	unsigned int pmtime;
};

static const struct pci_device_table pchtherm_devices[] =
{
	{ PCI_DEV(0x8086, 0x9c24),
	  PCI_DESCR("Haswell Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0x8c24),
	  PCI_DESCR("Haswell Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0x9ca4),
	  PCI_DESCR("Wildcat Point Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0x9d31),
	  PCI_DESCR("Skylake PCH Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0xa131),
	  PCI_DESCR("Skylake PCH 100 Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0x9df9),
	  PCI_DESCR("CannonLake-LP Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0xa379),
	  PCI_DESCR("CannonLake-H Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0x02f9),
	  PCI_DESCR("CometLake-LP Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0x06f9),
	  PCI_DESCR("CometLake-H Thermal Subsystem")},
	{ PCI_DEV(0x8086, 0xa1b1),
	  PCI_DESCR("Lewisburg Thermal Subsystem")},
};

static int pchtherm_probe(device_t dev)
{
	const struct pci_device_table *tbl;

	tbl = PCI_MATCH(dev, pchtherm_devices);
	if (tbl == NULL)
		return (ENXIO);
	device_set_desc(dev, tbl->descr);

	return (BUS_PROBE_DEFAULT);

}
static int pchtherm_tltemp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct pchtherm_softc *sc = oidp->oid_arg1;
	int regshift = oidp->oid_arg2;
	int temp;
	
	temp = bus_read_4(sc->tbar, PCHTHERM_REG_TL);
	temp >>= regshift;
	temp = PCHTHERM_TEMP_TO_IK(temp);
	
	return sysctl_handle_int(oidp, &temp, 0, req);
}	
static int pchtherm_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct pchtherm_softc *sc = oidp->oid_arg1;
	int regoff = oidp->oid_arg2;
	int temp;

	temp = bus_read_2(sc->tbar, regoff);
	temp = PCHTHERM_TEMP_TO_IK(temp);
	
	return sysctl_handle_int(oidp, &temp, 0, req);
}

#define FLAG_PRINT(dev, str, val) device_printf				\
	(dev, str " %s %sable\n",				       	\
	 ((val) & 0x80)? "Locked" : "",					\
	 ((val) & 0x1)? "En" : "Dis")

static int pchtherm_attach(device_t dev)
{
	struct pchtherm_softc *sc =  device_get_softc(dev);
	unsigned int val;
	int flag;
	int temp;

	sc->tbarrid = PCIR_BAR(0);
	sc->tbar = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
					  &sc->tbarrid, RF_ACTIVE|RF_SHAREABLE);
	if (sc->tbar == NULL) {
		return (ENOMEM);
	}
	sc->enable = bus_read_1(sc->tbar, PCHTHERM_REG_TSEL);
	if (!(sc->enable & PCHTHERM_GEN_ENABLE )) {
		if (sc->enable & PCHTHERM_GEN_LOCKDOWN) {
			device_printf(dev, "Sensor not available\n");
			return 0;
		} else {
			device_printf(dev, "Enabling Sensor\n");
			bus_write_1(sc->tbar, PCHTHERM_REG_TSEL,
				    PCHTHERM_GEN_ENABLE);
			sc->enable = bus_read_1(sc->tbar, PCHTHERM_REG_TSEL);
			if (!(sc->enable & PCHTHERM_GEN_ENABLE)) {
				device_printf(dev, "Sensor enable failed\n");
				return 0;
			}
		}
	}
	
	sc->ctten = bus_read_1(sc->tbar, PCHTHERM_REG_TSC);
	if (bootverbose) {
		FLAG_PRINT(dev, "Catastrophic Power Down", sc->ctten);
	}
	val = bus_read_1(sc->tbar, PCHTHERM_REG_TSREL);
	if (bootverbose) {
		FLAG_PRINT(dev, "SMBus report", val);
	}
	val = bus_read_1(sc->tbar, PCHTHERM_REG_TSMIC);
	if (bootverbose) {
		FLAG_PRINT(dev, "SMI on alert", val);
	}
	val = bus_read_2(sc->tbar, PCHTHERM_REG_TSPM);
	flag = val >> 13;
	if (bootverbose) {
		device_printf(dev, "TSPM: %b\n",
			      flag, "\20\3Lock\2DTSS0EN\1DSSOC0");
		device_printf(dev, "MAX Thermal Sensor Shutdown Time %ds\n",
			      1<<((val>>9)&7));
	}

	temp = val & PCHTHERM_TEMP_MASK;
	sc->pmtemp = PCHTHERM_TEMP_TO_IK(temp);
	sc->pmtime = 1<<((val>>9)&7);
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "pmtemp", CTLTYPE_INT|CTLFLAG_RD,
			sc, PCHTHERM_REG_TSPM, pchtherm_temp_sysctl,
			"IK", "Thermal sensor idle temperature");
	SYSCTL_ADD_U32(device_get_sysctl_ctx(dev),
		       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		       OID_AUTO, "pmtime", CTLFLAG_RD,
		       &sc->pmtime, 0,"Thermal sensor idle duration");

	val = bus_read_4(sc->tbar, PCHTHERM_REG_TL);
	flag = val>>29;

	if (bootverbose) {
		device_printf(dev, "Throttling %b\n",
			      flag, "\20\3Lock\2TT13EN\1TTEN");
	}
	
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "t0temp", CTLTYPE_INT |CTLFLAG_RD,
			sc, PCHTHERM_TL_T0, pchtherm_tltemp_sysctl,
		        "IK", "T0 temperature");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "t1temp", CTLTYPE_INT |CTLFLAG_RD,
			sc, PCHTHERM_TL_T1, pchtherm_tltemp_sysctl,
		        "IK", "T1 temperature");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "t2temp", CTLTYPE_INT |CTLFLAG_RD,
			sc, PCHTHERM_TL_T2, pchtherm_tltemp_sysctl,
			"IK", "T2 temperature");

	val = bus_read_2(sc->tbar, PCHTHERM_REG_TL2);
	if (bootverbose) {
		flag = val >>14;
		device_printf(dev, "TL2 flag %b\n",
			      flag, "\20\1PMCTEN\2Lock");
	}

	/* If PHL is disable and lockdown, don't export it.*/
	val = bus_read_2(sc->tbar, PCHTHERM_REG_PHLC);
	val <<= 16;
	val |= bus_read_2(sc->tbar, PCHTHERM_REG_PHL);
	if ((val & 0x10000) != 0x10000) {
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
				SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
				OID_AUTO, "pch_hot_level",
				CTLTYPE_INT |CTLFLAG_RD,
				sc, PCHTHERM_REG_PHL,
				pchtherm_temp_sysctl, "IK",
				"PCH Hot level Temperature");
	}

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "temperature", CTLTYPE_INT |CTLFLAG_RD,
			sc, PCHTHERM_REG_TEMP,
			pchtherm_temp_sysctl, "IK", "Current temperature");
	/*
	 * If sensor enable bit is locked down, there is no way to change
	 * alart values effectively. 
	 */
	if (!(sc->enable & PCHTHERM_GEN_LOCKDOWN) ||
	    bus_read_2(sc->tbar, PCHTHERM_REG_TAHV) != 0) {
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
				SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
				OID_AUTO, "tahv", CTLTYPE_INT |CTLFLAG_RD,
				sc, PCHTHERM_REG_TAHV,
				pchtherm_temp_sysctl, "IK",
				"Alart High temperature");
	}
	   
	if (!(sc->enable & PCHTHERM_GEN_LOCKDOWN) ||
	    bus_read_2(sc->tbar, PCHTHERM_REG_TALV) != 0) {
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
				SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
				OID_AUTO, "talv", CTLTYPE_INT |CTLFLAG_RD,
				sc, PCHTHERM_REG_TALV,
				pchtherm_temp_sysctl, "IK",
				"Alart Low temperature");
	}
	if (!(sc->ctten& PCHTHERM_GEN_LOCKDOWN) ||
	    sc->ctten& PCHTHERM_GEN_ENABLE) {
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
				SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
				OID_AUTO, "ctt",
				CTLTYPE_INT |CTLFLAG_RD,
				sc, PCHTHERM_REG_CTT,
				pchtherm_temp_sysctl, "IK",
				"Catastrophic Trip Point");
	}
		
	return 0;
}
static int pchtherm_detach(device_t dev)
{
	struct pchtherm_softc *sc =  device_get_softc(dev);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->tbarrid, sc->tbar);

	return 0;
}

static device_method_t pchtherm_methods[] =
{
	DEVMETHOD(device_probe, pchtherm_probe),
	DEVMETHOD(device_attach, pchtherm_attach),
	DEVMETHOD(device_detach, pchtherm_detach),

	DEVMETHOD_END
};

static driver_t pchtherm_driver = {
	"pchtherm",
	pchtherm_methods,
	sizeof(struct pchtherm_softc)
};

DRIVER_MODULE(pchtherm, pci, pchtherm_driver, 0, 0);
PCI_PNP_INFO(pchtherm_devices);
