/*-
 * Copyright (c) 2009 Michael Gmelin <freebsd@grem.de>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for extra ACPI-controlled features found on HP laptops
 * that use a WMI enabled BIOS (e.g. HP Compaq 8510p and 6510p).
 * Allows to control and read status of integrated hardware and read
 * BIOS settings through CMI.
 * Inspired by the hp-wmi driver, which implements a subset of these
 * features (hotkeys) on Linux.
 *
 * HP CMI whitepaper:
 *     http://h20331.www2.hp.com/Hpsub/downloads/cmi_whitepaper.pdf
 * wmi-hp for Linux:
 *     http://www.kernel.org
 * WMI and ACPI:
 *     http://www.microsoft.com/whdc/system/pnppwr/wmi/wmi-acpi.mspx
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/sbuf.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>                                
#include <dev/acpica/acpivar.h>
#include "acpi_wmi_if.h"

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("HP")

#define ACPI_HP_WMI_EVENT_GUID		"95F24279-4D7B-4334-9387-ACCDC67EF61C"
#define ACPI_HP_WMI_BIOS_GUID		"5FB7F034-2C63-45E9-BE91-3D44E2C707E4"
#define ACPI_HP_WMI_CMI_GUID		"2D114B49-2DFB-4130-B8FE-4A3C09E75133"

#define ACPI_HP_WMI_DISPLAY_COMMAND	0x1
#define ACPI_HP_WMI_HDDTEMP_COMMAND	0x2
#define ACPI_HP_WMI_ALS_COMMAND		0x3
#define ACPI_HP_WMI_DOCK_COMMAND	0x4
#define ACPI_HP_WMI_WIRELESS_COMMAND	0x5

#define ACPI_HP_METHOD_WLAN_ENABLED			1
#define ACPI_HP_METHOD_WLAN_RADIO			2
#define ACPI_HP_METHOD_WLAN_ON_AIR			3
#define ACPI_HP_METHOD_WLAN_ENABLE_IF_RADIO_ON		4
#define ACPI_HP_METHOD_WLAN_DISABLE_IF_RADIO_OFF	5
#define ACPI_HP_METHOD_BLUETOOTH_ENABLED		6
#define ACPI_HP_METHOD_BLUETOOTH_RADIO			7
#define ACPI_HP_METHOD_BLUETOOTH_ON_AIR			8
#define ACPI_HP_METHOD_BLUETOOTH_ENABLE_IF_RADIO_ON	9
#define ACPI_HP_METHOD_BLUETOOTH_DISABLE_IF_RADIO_OFF	10
#define ACPI_HP_METHOD_WWAN_ENABLED			11
#define ACPI_HP_METHOD_WWAN_RADIO			12
#define ACPI_HP_METHOD_WWAN_ON_AIR			13
#define ACPI_HP_METHOD_WWAN_ENABLE_IF_RADIO_ON		14
#define ACPI_HP_METHOD_WWAN_DISABLE_IF_RADIO_OFF	15
#define ACPI_HP_METHOD_ALS				16
#define ACPI_HP_METHOD_DISPLAY				17
#define ACPI_HP_METHOD_HDDTEMP				18
#define ACPI_HP_METHOD_DOCK				19
#define ACPI_HP_METHOD_CMI_DETAIL			20
#define ACPI_HP_METHOD_VERBOSE				21

#define HP_MASK_WWAN_ON_AIR			0x1000000
#define HP_MASK_BLUETOOTH_ON_AIR		0x10000
#define HP_MASK_WLAN_ON_AIR			0x100
#define HP_MASK_WWAN_RADIO			0x8000000
#define HP_MASK_BLUETOOTH_RADIO			0x80000
#define HP_MASK_WLAN_RADIO			0x800
#define HP_MASK_WWAN_ENABLED			0x2000000
#define HP_MASK_BLUETOOTH_ENABLED		0x20000
#define HP_MASK_WLAN_ENABLED			0x200

#define ACPI_HP_CMI_DETAIL_PATHS		0x01
#define ACPI_HP_CMI_DETAIL_ENUMS		0x02
#define ACPI_HP_CMI_DETAIL_FLAGS		0x04
#define ACPI_HP_CMI_DETAIL_SHOW_MAX_INSTANCE	0x08

struct acpi_hp_inst_seq_pair {
	UINT32	sequence;	/* sequence number as suggested by cmi bios */
	UINT8	instance;	/* object instance on guid */
};

struct acpi_hp_softc {
	device_t	dev;
	ACPI_HANDLE	handle;
	device_t	wmi_dev;
	int		has_notify;		/* notification GUID found */
	int		has_cmi;		/* CMI GUID found */
	int		cmi_detail;		/* CMI detail level
						   (set by sysctl) */
	int		verbose;		/* add debug output */
	int		wlan_enable_if_radio_on;	/* set by sysctl */
	int		wlan_disable_if_radio_off;	/* set by sysctl */
	int		bluetooth_enable_if_radio_on;	/* set by sysctl */
	int		bluetooth_disable_if_radio_off;	/* set by sysctl */
	int		wwan_enable_if_radio_on;	/* set by sysctl */
	int		wwan_disable_if_radio_off;	/* set by sysctl */
	int		was_wlan_on_air;		/* last known WLAN
							   on air status */
	int		was_bluetooth_on_air;		/* last known BT
							   on air status */
	int		was_wwan_on_air;		/* last known WWAN
							   on air status */
	struct sysctl_ctx_list	*sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
	struct cdev	*hpcmi_dev_t;		/* hpcmi device handle */
	struct sbuf	hpcmi_sbuf;		/* /dev/hpcmi output sbuf */
	pid_t		hpcmi_open_pid;		/* pid operating on
						   /dev/hpcmi */
	int		hpcmi_bufptr;		/* current pointer position
						   in /dev/hpcmi output buffer
						 */
	int		cmi_order_size;		/* size of cmi_order list */
	struct acpi_hp_inst_seq_pair cmi_order[128];	/* list of CMI
			     instances ordered by BIOS suggested sequence */
};

static struct {
	char	*name;
	int	method;
	char	*description;
	int	access;
} acpi_hp_sysctls[] = {
	{
		.name		= "wlan_enabled",
		.method		= ACPI_HP_METHOD_WLAN_ENABLED,
		.description	= "Enable/Disable WLAN (WiFi)",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "wlan_radio",
		.method		= ACPI_HP_METHOD_WLAN_RADIO,
		.description	= "WLAN radio status",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "wlan_on_air",
		.method		= ACPI_HP_METHOD_WLAN_ON_AIR,
		.description	= "WLAN radio ready to use (enabled and radio)",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "wlan_enable_if_radio_on",
		.method		= ACPI_HP_METHOD_WLAN_ENABLE_IF_RADIO_ON,
		.description	= "Enable WLAN if radio is turned on",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "wlan_disable_if_radio_off",
		.method		= ACPI_HP_METHOD_WLAN_DISABLE_IF_RADIO_OFF,
		.description	= "Disable WLAN if radio is turned off",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "bt_enabled",
		.method		= ACPI_HP_METHOD_BLUETOOTH_ENABLED,
		.description	= "Enable/Disable Bluetooth",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "bt_radio",
		.method		= ACPI_HP_METHOD_BLUETOOTH_RADIO,
		.description	= "Bluetooth radio status",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "bt_on_air",
		.method		= ACPI_HP_METHOD_BLUETOOTH_ON_AIR,
		.description	= "Bluetooth radio ready to use"
				    " (enabled and radio)",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "bt_enable_if_radio_on",
		.method		= ACPI_HP_METHOD_BLUETOOTH_ENABLE_IF_RADIO_ON,
		.description	= "Enable bluetooth if radio is turned on",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "bt_disable_if_radio_off",
		.method		= ACPI_HP_METHOD_BLUETOOTH_DISABLE_IF_RADIO_OFF,
		.description	= "Disable bluetooth if radio is turned off",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "wwan_enabled",
		.method		= ACPI_HP_METHOD_WWAN_ENABLED,
		.description	= "Enable/Disable WWAN (UMTS)",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "wwan_radio",
		.method		= ACPI_HP_METHOD_WWAN_RADIO,
		.description	= "WWAN radio status",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "wwan_on_air",
		.method		= ACPI_HP_METHOD_WWAN_ON_AIR,
		.description	= "WWAN radio ready to use (enabled and radio)",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "wwan_enable_if_radio_on",
		.method		= ACPI_HP_METHOD_WWAN_ENABLE_IF_RADIO_ON,
		.description	= "Enable WWAN if radio is turned on",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "wwan_disable_if_radio_off",
		.method		= ACPI_HP_METHOD_WWAN_DISABLE_IF_RADIO_OFF,
		.description	= "Disable WWAN if radio is turned off",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "als_enabled",
		.method		= ACPI_HP_METHOD_ALS,
		.description	= "Enable/Disable ALS (Ambient light sensor)",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "display",
		.method		= ACPI_HP_METHOD_DISPLAY,
		.description	= "Display status",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "hdd_temperature",
		.method		= ACPI_HP_METHOD_HDDTEMP,
		.description	= "HDD temperature",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "is_docked",
		.method		= ACPI_HP_METHOD_DOCK,
		.description	= "Docking station status",
		.access		= CTLTYPE_INT | CTLFLAG_RD
	},
	{
		.name		= "cmi_detail",
		.method		= ACPI_HP_METHOD_CMI_DETAIL,
		.description	= "Details shown in CMI output "
				    "(cat /dev/hpcmi)",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},
	{
		.name		= "verbose",
		.method		= ACPI_HP_METHOD_VERBOSE,
		.description	= "Verbosity level",
		.access		= CTLTYPE_INT | CTLFLAG_RW
	},

	{ NULL, 0, NULL, 0 }
};

ACPI_SERIAL_DECL(hp, "HP ACPI-WMI Mapping");

static int	acpi_hp_probe(device_t dev);
static int	acpi_hp_attach(device_t dev);
static int	acpi_hp_detach(device_t dev);

static void	acpi_hp_evaluate_auto_on_off(struct acpi_hp_softc* sc);
static int	acpi_hp_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_hp_sysctl_set(struct acpi_hp_softc *sc, int method,
		    int arg, int oldarg);
static int	acpi_hp_sysctl_get(struct acpi_hp_softc *sc, int method);
static int	acpi_hp_exec_wmi_command(device_t wmi_dev, int command,
		    int is_write, int val);
static void	acpi_hp_notify(ACPI_HANDLE h, UINT32 notify, void *context);
static int	acpi_hp_get_cmi_block(device_t wmi_dev, const char* guid,
		    UINT8 instance, char* outbuf, size_t outsize,
		    UINT32* sequence, int detail);
static void	acpi_hp_hex_decode(char* buffer);

static d_open_t	acpi_hp_hpcmi_open;
static d_close_t acpi_hp_hpcmi_close;
static d_read_t	acpi_hp_hpcmi_read;

/* handler /dev/hpcmi device */
static struct cdevsw hpcmi_cdevsw = {
	.d_version = D_VERSION,
	.d_open = acpi_hp_hpcmi_open,
	.d_close = acpi_hp_hpcmi_close,
	.d_read = acpi_hp_hpcmi_read,
	.d_name = "hpcmi",
};

static device_method_t acpi_hp_methods[] = {
	DEVMETHOD(device_probe, acpi_hp_probe),
	DEVMETHOD(device_attach, acpi_hp_attach),
	DEVMETHOD(device_detach, acpi_hp_detach),
	{0, 0}
};

static driver_t	acpi_hp_driver = {
	"acpi_hp",
	acpi_hp_methods,
	sizeof(struct acpi_hp_softc),
};

static devclass_t acpi_hp_devclass;

DRIVER_MODULE(acpi_hp, acpi, acpi_hp_driver, acpi_hp_devclass,
		0, 0);
MODULE_DEPEND(acpi_hp, acpi_wmi, 1, 1, 1);
MODULE_DEPEND(acpi_hp, acpi, 1, 1, 1);

static void	
acpi_hp_evaluate_auto_on_off(struct acpi_hp_softc *sc)
{
	int	wireless;
	int	new_wlan_status;
	int	new_bluetooth_status;
	int	new_wwan_status;

	wireless = acpi_hp_exec_wmi_command(sc->wmi_dev,
		    ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
	new_wlan_status = -1;
	new_bluetooth_status = -1;
	new_wwan_status = -1;

	if (sc->verbose)
		device_printf(sc->wmi_dev, "Wireless status is %x\n", wireless);
	if (sc->wlan_disable_if_radio_off && !(wireless & HP_MASK_WLAN_RADIO)
	    &&  (wireless & HP_MASK_WLAN_ENABLED)) {
		acpi_hp_exec_wmi_command(sc->wmi_dev,
		    ACPI_HP_WMI_WIRELESS_COMMAND, 1, 0x100);
		new_wlan_status = 0;
	}
	else if (sc->wlan_enable_if_radio_on && (wireless & HP_MASK_WLAN_RADIO)
		&&  !(wireless & HP_MASK_WLAN_ENABLED)) {
		acpi_hp_exec_wmi_command(sc->wmi_dev,
		    ACPI_HP_WMI_WIRELESS_COMMAND, 1, 0x101);
		new_wlan_status = 1;
	}
	if (sc->bluetooth_disable_if_radio_off &&
	    !(wireless & HP_MASK_BLUETOOTH_RADIO) &&
	    (wireless & HP_MASK_BLUETOOTH_ENABLED)) {
		acpi_hp_exec_wmi_command(sc->wmi_dev,
		    ACPI_HP_WMI_WIRELESS_COMMAND, 1, 0x200);
		new_bluetooth_status = 0;
	}
	else if (sc->bluetooth_enable_if_radio_on &&
		(wireless & HP_MASK_BLUETOOTH_RADIO) &&
		!(wireless & HP_MASK_BLUETOOTH_ENABLED)) {
		acpi_hp_exec_wmi_command(sc->wmi_dev,
		    ACPI_HP_WMI_WIRELESS_COMMAND, 1, 0x202);
		new_bluetooth_status = 1;
	}
	if (sc->wwan_disable_if_radio_off &&
	    !(wireless & HP_MASK_WWAN_RADIO) &&
	    (wireless & HP_MASK_WWAN_ENABLED)) {
		acpi_hp_exec_wmi_command(sc->wmi_dev,
		ACPI_HP_WMI_WIRELESS_COMMAND, 1, 0x400);
		new_wwan_status = 0;
	}
	else if (sc->wwan_enable_if_radio_on &&
		(wireless & HP_MASK_WWAN_RADIO) &&
		!(wireless & HP_MASK_WWAN_ENABLED)) {
		acpi_hp_exec_wmi_command(sc->wmi_dev,
		    ACPI_HP_WMI_WIRELESS_COMMAND, 1, 0x404);
		new_wwan_status = 1;
	}

	if (new_wlan_status == -1) {
		new_wlan_status = (wireless & HP_MASK_WLAN_ON_AIR);
		if ((new_wlan_status?1:0) != sc->was_wlan_on_air) {
			sc->was_wlan_on_air = sc->was_wlan_on_air?0:1;
			if (sc->verbose)
				device_printf(sc->wmi_dev,
			    	    "WLAN on air changed to %i "
			    	    "(new_wlan_status is %i)\n",
			    	    sc->was_wlan_on_air, new_wlan_status);
			acpi_UserNotify("HP", sc->handle,
			    0xc0+sc->was_wlan_on_air);
		}
	}
	if (new_bluetooth_status == -1) {
		new_bluetooth_status = (wireless & HP_MASK_BLUETOOTH_ON_AIR);
		if ((new_bluetooth_status?1:0) != sc->was_bluetooth_on_air) {
			sc->was_bluetooth_on_air = sc->was_bluetooth_on_air?
			    0:1;
			if (sc->verbose)
				device_printf(sc->wmi_dev,
				    "BLUETOOTH on air changed"
				    " to %i (new_bluetooth_status is %i)\n",
				    sc->was_bluetooth_on_air,
				    new_bluetooth_status);
			acpi_UserNotify("HP", sc->handle,
			    0xd0+sc->was_bluetooth_on_air);
		}
	}
	if (new_wwan_status == -1) {
		new_wwan_status = (wireless & HP_MASK_WWAN_ON_AIR);
		if ((new_wwan_status?1:0) != sc->was_wwan_on_air) {
			sc->was_wwan_on_air = sc->was_wwan_on_air?0:1;
			if (sc->verbose)
				device_printf(sc->wmi_dev,
				    "WWAN on air changed to %i"
			    	    " (new_wwan_status is %i)\n",
				    sc->was_wwan_on_air, new_wwan_status);
			acpi_UserNotify("HP", sc->handle,
			    0xe0+sc->was_wwan_on_air);
		}
	}
}

static int
acpi_hp_probe(device_t dev)
{
	if (acpi_disabled("hp") || device_get_unit(dev) != 0)
		return (ENXIO);
	device_set_desc(dev, "HP ACPI-WMI Mapping");

	return (0);
}

static int
acpi_hp_attach(device_t dev)
{
	struct acpi_hp_softc	*sc;
	devclass_t		wmi_devclass;
	int			arg;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);
	sc->has_notify = 0;
	sc->has_cmi = 0;
	sc->bluetooth_enable_if_radio_on = 0;
	sc->bluetooth_disable_if_radio_off = 0;
	sc->wlan_enable_if_radio_on = 0;
	sc->wlan_disable_if_radio_off = 0;
	sc->wlan_enable_if_radio_on = 0;
	sc->wlan_disable_if_radio_off = 0;
	sc->was_wlan_on_air = 0;
	sc->was_bluetooth_on_air = 0;
	sc->was_wwan_on_air = 0;
	sc->cmi_detail = 0;
	sc->cmi_order_size = -1;
	sc->verbose = 0;
	memset(sc->cmi_order, 0, sizeof(sc->cmi_order));

	if (!(wmi_devclass = devclass_find ("acpi_wmi"))) {
		device_printf(dev, "Couldn't find acpi_wmi devclass\n");
		return (EINVAL);
	}
	if (!(sc->wmi_dev = devclass_get_device(wmi_devclass, 0))) {
		device_printf(dev, "Couldn't find acpi_wmi device\n");
		return (EINVAL);
	}
	if (!ACPI_WMI_PROVIDES_GUID_STRING(sc->wmi_dev,
	    ACPI_HP_WMI_BIOS_GUID)) {
		device_printf(dev,
		    "WMI device does not provide the HP BIOS GUID\n");
		return (EINVAL);
	}
	if (ACPI_WMI_PROVIDES_GUID_STRING(sc->wmi_dev,
	    ACPI_HP_WMI_EVENT_GUID)) {
		device_printf(dev,
		    "HP event GUID detected, installing event handler\n");
		if (ACPI_WMI_INSTALL_EVENT_HANDLER(sc->wmi_dev,
		    ACPI_HP_WMI_EVENT_GUID, acpi_hp_notify, dev)) {
			device_printf(dev,
			    "Could not install notification handler!\n");
		}
		else {
			sc->has_notify = 1;
		}
	}
	if ((sc->has_cmi = 
	    ACPI_WMI_PROVIDES_GUID_STRING(sc->wmi_dev, ACPI_HP_WMI_CMI_GUID)
	    )) {
		device_printf(dev, "HP CMI GUID detected\n");
	}

	if (sc->has_cmi) {
		sc->hpcmi_dev_t = make_dev(&hpcmi_cdevsw, 0, UID_ROOT,
			    GID_WHEEL, 0644, "hpcmi");
		sc->hpcmi_dev_t->si_drv1 = sc;
		sc->hpcmi_open_pid = 0;
		sc->hpcmi_bufptr = -1;
	}

	ACPI_SERIAL_BEGIN(hp);

	sc->sysctl_ctx = device_get_sysctl_ctx(dev);
	sc->sysctl_tree = device_get_sysctl_tree(dev);
	for (int i = 0; acpi_hp_sysctls[i].name != NULL; ++i) {
		arg = 0;
		if ((!sc->has_notify &&
		    (acpi_hp_sysctls[i].method ==
			ACPI_HP_METHOD_WLAN_ENABLE_IF_RADIO_ON ||
		    acpi_hp_sysctls[i].method ==
			ACPI_HP_METHOD_WLAN_DISABLE_IF_RADIO_OFF ||
		    acpi_hp_sysctls[i].method ==
			ACPI_HP_METHOD_BLUETOOTH_ENABLE_IF_RADIO_ON ||
		    acpi_hp_sysctls[i].method ==
			ACPI_HP_METHOD_BLUETOOTH_DISABLE_IF_RADIO_OFF ||
		    acpi_hp_sysctls[i].method ==
			ACPI_HP_METHOD_WWAN_ENABLE_IF_RADIO_ON ||
		    acpi_hp_sysctls[i].method ==
			ACPI_HP_METHOD_WWAN_DISABLE_IF_RADIO_OFF)) ||
		    (arg = acpi_hp_sysctl_get(sc,
		    acpi_hp_sysctls[i].method)) < 0) {
			continue;
		}
		if (acpi_hp_sysctls[i].method == ACPI_HP_METHOD_WLAN_ON_AIR) {
			sc->was_wlan_on_air = arg;
		}
		else if (acpi_hp_sysctls[i].method ==
			    ACPI_HP_METHOD_BLUETOOTH_ON_AIR) {
			sc->was_bluetooth_on_air = arg;
		}
		else if (acpi_hp_sysctls[i].method ==
			    ACPI_HP_METHOD_WWAN_ON_AIR) {
			sc->was_wwan_on_air = arg;
		}

		SYSCTL_ADD_PROC(sc->sysctl_ctx,
		SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
			acpi_hp_sysctls[i].name, acpi_hp_sysctls[i].access,
			sc, i, acpi_hp_sysctl, "I",
			acpi_hp_sysctls[i].description);
	}
	ACPI_SERIAL_END(hp);

	return (0);
}

static int
acpi_hp_detach(device_t dev)
{
	int	ret;
	
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);
	struct acpi_hp_softc *sc = device_get_softc(dev);
	if (sc->has_cmi && sc->hpcmi_open_pid != 0) {
		ret = EBUSY;
	}
	else {
		if (sc->has_notify) {
			ACPI_WMI_REMOVE_EVENT_HANDLER(dev,
			    ACPI_HP_WMI_EVENT_GUID);
		}
		if (sc->hpcmi_bufptr != -1) {
			sbuf_delete(&sc->hpcmi_sbuf);
			sc->hpcmi_bufptr = -1;
		}
		sc->hpcmi_open_pid = 0;
		destroy_dev(sc->hpcmi_dev_t);
		ret = 0;
	}

	return (ret);
}

static int
acpi_hp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct acpi_hp_softc	*sc;
	int			arg;
	int			oldarg;
	int			error = 0;
	int			function;
	int			method;
	
	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_hp_softc *)oidp->oid_arg1;
	function = oidp->oid_arg2;
	method = acpi_hp_sysctls[function].method;

	ACPI_SERIAL_BEGIN(hp);
	arg = acpi_hp_sysctl_get(sc, method);
	oldarg = arg;
	error = sysctl_handle_int(oidp, &arg, 0, req);
	if (!error && req->newptr != NULL) {
		error = acpi_hp_sysctl_set(sc, method, arg, oldarg);
	}
	ACPI_SERIAL_END(hp);

	return (error);
}

static int
acpi_hp_sysctl_get(struct acpi_hp_softc *sc, int method)
{
	int	val = 0;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
	ACPI_SERIAL_ASSERT(hp);

	switch (method) {
	case ACPI_HP_METHOD_WLAN_ENABLED:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_WLAN_ENABLED) != 0);
		break;
	case ACPI_HP_METHOD_WLAN_RADIO:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_WLAN_RADIO) != 0);
		break;
	case ACPI_HP_METHOD_WLAN_ON_AIR:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_WLAN_ON_AIR) != 0);
		break;
	case ACPI_HP_METHOD_WLAN_ENABLE_IF_RADIO_ON:
		val = sc->wlan_enable_if_radio_on;
		break;
	case ACPI_HP_METHOD_WLAN_DISABLE_IF_RADIO_OFF:
		val = sc->wlan_disable_if_radio_off;
		break;
	case ACPI_HP_METHOD_BLUETOOTH_ENABLED:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_BLUETOOTH_ENABLED) != 0);
		break;
	case ACPI_HP_METHOD_BLUETOOTH_RADIO:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_BLUETOOTH_RADIO) != 0);
		break;
	case ACPI_HP_METHOD_BLUETOOTH_ON_AIR:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_BLUETOOTH_ON_AIR) != 0);
		break;
	case ACPI_HP_METHOD_BLUETOOTH_ENABLE_IF_RADIO_ON:
		val = sc->bluetooth_enable_if_radio_on;
		break;
	case ACPI_HP_METHOD_BLUETOOTH_DISABLE_IF_RADIO_OFF:
		val = sc->bluetooth_disable_if_radio_off;
		break;
	case ACPI_HP_METHOD_WWAN_ENABLED:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_WWAN_ENABLED) != 0);
		break;
	case ACPI_HP_METHOD_WWAN_RADIO:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_WWAN_RADIO) != 0);
		break;
	case ACPI_HP_METHOD_WWAN_ON_AIR:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_WIRELESS_COMMAND, 0, 0);
		val = ((val & HP_MASK_WWAN_ON_AIR) != 0);
		break;
	case ACPI_HP_METHOD_WWAN_ENABLE_IF_RADIO_ON:
		val = sc->wwan_enable_if_radio_on;
		break;
	case ACPI_HP_METHOD_WWAN_DISABLE_IF_RADIO_OFF:
		val = sc->wwan_disable_if_radio_off;
		break;
	case ACPI_HP_METHOD_ALS:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_ALS_COMMAND, 0, 0);
		break;
	case ACPI_HP_METHOD_DISPLAY:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_DISPLAY_COMMAND, 0, 0);
		break;
	case ACPI_HP_METHOD_HDDTEMP:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_HDDTEMP_COMMAND, 0, 0);
		break;
	case ACPI_HP_METHOD_DOCK:
		val = acpi_hp_exec_wmi_command(sc->wmi_dev,
			ACPI_HP_WMI_DOCK_COMMAND, 0, 0);
		break;
	case ACPI_HP_METHOD_CMI_DETAIL:
		val = sc->cmi_detail;
		break;
	case ACPI_HP_METHOD_VERBOSE:
		val = sc->verbose;
		break;
	}

	return (val);
}

static int
acpi_hp_sysctl_set(struct acpi_hp_softc *sc, int method, int arg, int oldarg)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
	ACPI_SERIAL_ASSERT(hp);

	if (method != ACPI_HP_METHOD_CMI_DETAIL &&
	    method != ACPI_HP_METHOD_VERBOSE)
		arg = arg?1:0;

	if (arg != oldarg) {
		switch (method) {
		case ACPI_HP_METHOD_WLAN_ENABLED:
			return (acpi_hp_exec_wmi_command(sc->wmi_dev,
				    ACPI_HP_WMI_WIRELESS_COMMAND, 1,
				    arg?0x101:0x100));
		case ACPI_HP_METHOD_WLAN_ENABLE_IF_RADIO_ON:
			sc->wlan_enable_if_radio_on = arg;
			acpi_hp_evaluate_auto_on_off(sc);
			break;
		case ACPI_HP_METHOD_WLAN_DISABLE_IF_RADIO_OFF:
			sc->wlan_disable_if_radio_off = arg;
			acpi_hp_evaluate_auto_on_off(sc);
			break;
		case ACPI_HP_METHOD_BLUETOOTH_ENABLED:
			return (acpi_hp_exec_wmi_command(sc->wmi_dev,
				    ACPI_HP_WMI_WIRELESS_COMMAND, 1,
				    arg?0x202:0x200));
		case ACPI_HP_METHOD_BLUETOOTH_ENABLE_IF_RADIO_ON:
			sc->bluetooth_enable_if_radio_on = arg;
			acpi_hp_evaluate_auto_on_off(sc);
			break;
		case ACPI_HP_METHOD_BLUETOOTH_DISABLE_IF_RADIO_OFF:
			sc->bluetooth_disable_if_radio_off = arg?1:0;
			acpi_hp_evaluate_auto_on_off(sc);
			break;
		case ACPI_HP_METHOD_WWAN_ENABLED:
			return (acpi_hp_exec_wmi_command(sc->wmi_dev,
				    ACPI_HP_WMI_WIRELESS_COMMAND, 1,
				    arg?0x404:0x400));
		case ACPI_HP_METHOD_WWAN_ENABLE_IF_RADIO_ON:
			sc->wwan_enable_if_radio_on = arg?1:0;
			acpi_hp_evaluate_auto_on_off(sc);
			break;
		case ACPI_HP_METHOD_WWAN_DISABLE_IF_RADIO_OFF:
			sc->wwan_disable_if_radio_off = arg?1:0;
			acpi_hp_evaluate_auto_on_off(sc);
			break;
		case ACPI_HP_METHOD_ALS:
			return (acpi_hp_exec_wmi_command(sc->wmi_dev,
				    ACPI_HP_WMI_ALS_COMMAND, 1,
				    arg?1:0));
		case ACPI_HP_METHOD_CMI_DETAIL:
			sc->cmi_detail = arg;
			if ((arg & ACPI_HP_CMI_DETAIL_SHOW_MAX_INSTANCE) != 
			    (oldarg & ACPI_HP_CMI_DETAIL_SHOW_MAX_INSTANCE)) {
			    sc->cmi_order_size = -1;
			}
			break;
		case ACPI_HP_METHOD_VERBOSE:
			sc->verbose = arg;
			break;
		}
	}

	return (0);
}

static __inline void
acpi_hp_free_buffer(ACPI_BUFFER* buf) {
	if (buf && buf->Pointer) {
		AcpiOsFree(buf->Pointer);
	}
}

static void
acpi_hp_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
	device_t dev = context;
	ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);

	struct acpi_hp_softc *sc = device_get_softc(dev);
	ACPI_BUFFER response = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *obj;
	ACPI_WMI_GET_EVENT_DATA(sc->wmi_dev, notify, &response);
	obj = (ACPI_OBJECT*) response.Pointer;
	if (obj && obj->Type == ACPI_TYPE_BUFFER && obj->Buffer.Length == 8) {
		if (*((UINT8 *) obj->Buffer.Pointer) == 0x5) {
			acpi_hp_evaluate_auto_on_off(sc);
		}
	}
	acpi_hp_free_buffer(&response);
}

static int
acpi_hp_exec_wmi_command(device_t wmi_dev, int command, int is_write, int val)
{
	UINT32		params[5] = { 0x55434553,
			    is_write?2:1,
			    command,
			    is_write?4:0,
			    val};
	UINT32*		result;
	ACPI_OBJECT	*obj;
	ACPI_BUFFER	in = { sizeof(params), &params };
	ACPI_BUFFER	out = { ACPI_ALLOCATE_BUFFER, NULL };
	int retval;
	
	if (ACPI_FAILURE(ACPI_WMI_EVALUATE_CALL(wmi_dev, ACPI_HP_WMI_BIOS_GUID,
		    0, 0x3, &in, &out))) {
		acpi_hp_free_buffer(&out);
		return (-EINVAL);
	}
	obj = out.Pointer;
	if (!obj || obj->Type != ACPI_TYPE_BUFFER) {
		acpi_hp_free_buffer(&out);
		return (-EINVAL);
	}
	result = (UINT32*) obj->Buffer.Pointer;
	retval = result[2];
	if (result[1] > 0) {
		retval = result[1];
	}
	acpi_hp_free_buffer(&out);

	return (retval);
}

static __inline char*
acpi_hp_get_string_from_object(ACPI_OBJECT* obj, char* dst, size_t size) {
	int	length;

	dst[0] = 0;
	if (obj->Type == ACPI_TYPE_STRING) {
		length = obj->String.Length+1;
		if (length > size) {
			length = size - 1;
		}
		strlcpy(dst, obj->String.Pointer, length);
		acpi_hp_hex_decode(dst);
	}

	return (dst);
}


/*
 * Read BIOS Setting block in instance "instance".
 * The block returned is ACPI_TYPE_PACKAGE which should contain the following
 * elements:
 * Index Meaning
 * 0        Setting Name [string]
 * 1        Value (comma separated, asterisk marks the current value) [string]
 * 2        Path within the bios hierarchy [string]
 * 3        IsReadOnly [int]
 * 4        DisplayInUI [int]
 * 5        RequiresPhysicalPresence [int]
 * 6        Sequence for ordering within the bios settings (absolute) [int]
 * 7        Length of prerequisites array [int]
 * 8..8+[7] PrerequisiteN [string]
 * 9+[7]    Current value (in case of enum) [string] / Array length [int]
 * 10+[7]   Enum length [int] / Array values
 * 11+[7]ff Enum value at index x [string]
 */
static int
acpi_hp_get_cmi_block(device_t wmi_dev, const char* guid, UINT8 instance,
    char* outbuf, size_t outsize, UINT32* sequence, int detail)
{
	ACPI_OBJECT	*obj;
	ACPI_BUFFER	out = { ACPI_ALLOCATE_BUFFER, NULL };
	int		i;
	int		outlen;
	int		size = 255;
	int		has_enums = 0;
	int		valuebase = 0;
	char		string_buffer[size];
	int		enumbase;

	outlen = 0;
	outbuf[0] = 0;	
	if (ACPI_FAILURE(ACPI_WMI_GET_BLOCK(wmi_dev, guid, instance, &out))) {
		acpi_hp_free_buffer(&out);
		return (-EINVAL);
	}
	obj = out.Pointer;
	if (!obj || obj->Type != ACPI_TYPE_PACKAGE) {
		acpi_hp_free_buffer(&out);
		return (-EINVAL);
	}

	if (obj->Package.Count >= 8 &&
	    obj->Package.Elements[7].Type == ACPI_TYPE_INTEGER) {
	    valuebase = 8 + obj->Package.Elements[7].Integer.Value;
	}

	/* check if this matches our expectations based on limited knowledge */
	if (valuebase > 7 && obj->Package.Count > valuebase + 1 &&
	    obj->Package.Elements[0].Type == ACPI_TYPE_STRING &&
	    obj->Package.Elements[1].Type == ACPI_TYPE_STRING &&
	    obj->Package.Elements[2].Type == ACPI_TYPE_STRING &&
	    obj->Package.Elements[3].Type == ACPI_TYPE_INTEGER &&
	    obj->Package.Elements[4].Type == ACPI_TYPE_INTEGER &&
	    obj->Package.Elements[5].Type == ACPI_TYPE_INTEGER &&
	    obj->Package.Elements[6].Type == ACPI_TYPE_INTEGER &&
	    obj->Package.Elements[valuebase].Type == ACPI_TYPE_STRING &&
	    obj->Package.Elements[valuebase+1].Type == ACPI_TYPE_INTEGER &&
	    obj->Package.Count > valuebase + 
	        obj->Package.Elements[valuebase+1].Integer.Value
	   ) {
		enumbase = valuebase + 1;
		if (detail & ACPI_HP_CMI_DETAIL_PATHS) {
			strlcat(outbuf, acpi_hp_get_string_from_object(
				&obj->Package.Elements[2], string_buffer, size),
				outsize);
			outlen += 48;
			while (strlen(outbuf) < outlen)
				strlcat(outbuf, " ", outsize);
		}
		strlcat(outbuf, acpi_hp_get_string_from_object(
				&obj->Package.Elements[0], string_buffer, size),
				outsize);
		outlen += 43;
		while (strlen(outbuf) < outlen)
			strlcat(outbuf, " ", outsize);
		strlcat(outbuf, acpi_hp_get_string_from_object(
				&obj->Package.Elements[valuebase], string_buffer, 
				size),
				outsize);
		outlen += 21;
		while (strlen(outbuf) < outlen)
			strlcat(outbuf, " ", outsize);
		for (i = 0; i < strlen(outbuf); ++i)
			if (outbuf[i] == '\\')
				outbuf[i] = '/';
		if (detail & ACPI_HP_CMI_DETAIL_ENUMS) {
			for (i = enumbase + 1; i < enumbase + 1 +
			    obj->Package.Elements[enumbase].Integer.Value;
			    ++i) {
				acpi_hp_get_string_from_object(
				    &obj->Package.Elements[i], string_buffer,
				    size);
				if (strlen(string_buffer) > 1 ||
				    (strlen(string_buffer) == 1 &&
				    string_buffer[0] != ' ')) {
					if (has_enums)
						strlcat(outbuf, "/", outsize);
					else
						strlcat(outbuf, " (", outsize);
					strlcat(outbuf, string_buffer, outsize);
					has_enums = 1;
				}
			}
		}
		if (has_enums)
			strlcat(outbuf, ")", outsize);
		if (detail & ACPI_HP_CMI_DETAIL_FLAGS) {
			strlcat(outbuf, obj->Package.Elements[3].Integer.Value?
			    " [ReadOnly]":"", outsize);
			strlcat(outbuf, obj->Package.Elements[4].Integer.Value?
			    "":" [NOUI]", outsize);
			strlcat(outbuf, obj->Package.Elements[5].Integer.Value?
			    " [RPP]":"", outsize);
		}
		*sequence = (UINT32) obj->Package.Elements[6].Integer.Value;
	}
	acpi_hp_free_buffer(&out);

	return (0);
}



/*
 * Convert given two digit hex string (hexin) to an UINT8 referenced
 * by byteout.
 * Return != 0 if the was a problem (invalid input)
 */
static __inline int acpi_hp_hex_to_int(const UINT8 *hexin, UINT8 *byteout)
{
	unsigned int	hi;
	unsigned int	lo;

	hi = hexin[0];
	lo = hexin[1];
	if ('0' <= hi && hi <= '9')
		hi -= '0';
	else if ('A' <= hi && hi <= 'F')
		hi -= ('A' - 10);
	else if ('a' <= hi && hi <= 'f')
		hi -= ('a' - 10);
	else
		return (1);
	if ('0' <= lo && lo <= '9')
		lo -= '0';
	else if ('A' <= lo && lo <= 'F')
		lo -= ('A' - 10);
	else if ('a' <= lo && lo <= 'f')
		lo -= ('a' - 10);
	else
		return (1);
	*byteout = (hi << 4) + lo;

	return (0);
}


static void
acpi_hp_hex_decode(char* buffer)
{
	int	i;
	int	length = strlen(buffer);
	UINT8	*uin;
	UINT8	uout;

	if (((int)length/2)*2 == length || length < 10) return;

	for (i = 0; i<length; ++i) {
		if (!((i+1)%3)) {
			if (buffer[i] != ' ')
				return;
		}
		else
			if (!((buffer[i] >= '0' && buffer[i] <= '9') ||
		    	    (buffer[i] >= 'A' && buffer[i] <= 'F')))
				return;			
	}

	for (i = 0; i<length; i += 3) {
		uin = &buffer[i];
		uout = 0;
		acpi_hp_hex_to_int(uin, &uout);
		buffer[i/3] = (char) uout;
	}
	buffer[(length+1)/3] = 0;
}


/*
 * open hpcmi device
 */
static int
acpi_hp_hpcmi_open(struct cdev* dev, int flags, int mode, struct thread *td)
{
	struct acpi_hp_softc	*sc;
	int			ret;

	if (dev == NULL || dev->si_drv1 == NULL)
		return (EBADF);
	sc = dev->si_drv1;

	ACPI_SERIAL_BEGIN(hp);
	if (sc->hpcmi_open_pid != 0) {
		ret = EBUSY;
	}
	else {
		if (sbuf_new(&sc->hpcmi_sbuf, NULL, 4096, SBUF_AUTOEXTEND)
		    == NULL) {
			ret = ENXIO;
		} else {
			sc->hpcmi_open_pid = td->td_proc->p_pid;
			sc->hpcmi_bufptr = 0;
			ret = 0;
		}
	}
	ACPI_SERIAL_END(hp);

	return (ret);
}

/*
 * close hpcmi device
 */
static int
acpi_hp_hpcmi_close(struct cdev* dev, int flags, int mode, struct thread *td)
{
	struct acpi_hp_softc	*sc;
	int			ret;

	if (dev == NULL || dev->si_drv1 == NULL)
		return (EBADF);
	sc = dev->si_drv1;

	ACPI_SERIAL_BEGIN(hp);
	if (sc->hpcmi_open_pid == 0) {
		ret = EBADF;
	}
	else {
		if (sc->hpcmi_bufptr != -1) {
			sbuf_delete(&sc->hpcmi_sbuf);
			sc->hpcmi_bufptr = -1;
		}
		sc->hpcmi_open_pid = 0;
		ret = 0;
	}
	ACPI_SERIAL_END(hp);

	return (ret);
}

/*
 * Read from hpcmi bios information
 */
static int
acpi_hp_hpcmi_read(struct cdev *dev, struct uio *buf, int flag)
{
	struct acpi_hp_softc	*sc;
	int			pos, i, l, ret;
	UINT8			instance;
	UINT8			maxInstance;
	UINT32			sequence;
	int			linesize = 1025;
	char			line[linesize];

	if (dev == NULL || dev->si_drv1 == NULL)
		return (EBADF);
	sc = dev->si_drv1;
	
	ACPI_SERIAL_BEGIN(hp);
	if (sc->hpcmi_open_pid != buf->uio_td->td_proc->p_pid
	    || sc->hpcmi_bufptr == -1) {
		ret = EBADF;
	}
	else {
		if (!sbuf_done(&sc->hpcmi_sbuf)) {
			if (sc->cmi_order_size < 0) {
				maxInstance = sc->has_cmi;
				if (!(sc->cmi_detail & 
				    ACPI_HP_CMI_DETAIL_SHOW_MAX_INSTANCE) &&
				    maxInstance > 0) {
					maxInstance--;
				}
				sc->cmi_order_size = 0;
				for (instance = 0; instance < maxInstance;
				    ++instance) {
					if (acpi_hp_get_cmi_block(sc->wmi_dev,
						ACPI_HP_WMI_CMI_GUID, instance,
						line, linesize, &sequence,
						sc->cmi_detail)) {
						instance = maxInstance;
					}
					else {
						pos = sc->cmi_order_size;
						for (i=0;
						  i<sc->cmi_order_size && i<127;
						     ++i) {
				if (sc->cmi_order[i].sequence > sequence) {
								pos = i;
								break; 							
							}
						}
						for (i=sc->cmi_order_size;
						    i>pos;
						    --i) {
						sc->cmi_order[i].sequence =
						    sc->cmi_order[i-1].sequence;
						sc->cmi_order[i].instance =
						    sc->cmi_order[i-1].instance;
						}
						sc->cmi_order[pos].sequence =
						    sequence;
						sc->cmi_order[pos].instance =
						    instance;
						sc->cmi_order_size++;
					}
				}
			}
			for (i=0; i<sc->cmi_order_size; ++i) {
				if (!acpi_hp_get_cmi_block(sc->wmi_dev,
				    ACPI_HP_WMI_CMI_GUID,
				    sc->cmi_order[i].instance, line, linesize,
				    &sequence, sc->cmi_detail)) {
					sbuf_printf(&sc->hpcmi_sbuf, "%s\n", line);
				}
			}
			sbuf_finish(&sc->hpcmi_sbuf);
		}
		if (sbuf_len(&sc->hpcmi_sbuf) <= 0) {
			sbuf_delete(&sc->hpcmi_sbuf);
			sc->hpcmi_bufptr = -1;
			sc->hpcmi_open_pid = 0;
			ret = ENOMEM;
		} else {
			l = min(buf->uio_resid, sbuf_len(&sc->hpcmi_sbuf) -
			    sc->hpcmi_bufptr);
			ret = (l > 0)?uiomove(sbuf_data(&sc->hpcmi_sbuf) +
			    sc->hpcmi_bufptr, l, buf) : 0;
			sc->hpcmi_bufptr += l;
		}
	}
	ACPI_SERIAL_END(hp);

	return (ret);
}
