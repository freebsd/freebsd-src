/*-
 * Copyright (c) 2006 IronPort Systems Inc. <ambrisko@ironport.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/watchdog.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/pc/bios.h>

#ifdef LOCAL_MODULE
#include <ipmi.h>
#include <ipmivars.h>
#else
#include <sys/ipmi.h>
#include <dev/ipmi/ipmivars.h>
#endif

struct smbios_table_entry {
	uint8_t		anchor_string[4];
	uint8_t		checksum;
	uint8_t		length;
	uint8_t		major_version;
	uint8_t		minor_version;
	uint16_t	maximum_structure_size;
	uint8_t		entry_point_revision;
	uint8_t		formatted_area[5];
	uint8_t		DMI_anchor_string[5];
	uint8_t		intermediate_checksum;
	uint16_t	structure_table_length;
	uint32_t	structure_table_address;
	uint16_t	number_structures;
	uint8_t		BCD_revision;
};

struct structure_header {
	uint8_t		type;
	uint8_t		length;
	uint16_t	handle;
};

struct ipmi_device {
	uint8_t		type;
	uint8_t		length;
	uint16_t	handle;
	uint8_t		interface_type;
	uint8_t		spec_revision;
	uint8_t		i2c_slave_address;
	uint8_t		NV_storage_device_address;
	uint64_t	base_address;
	uint8_t		base_address_modifier;
	uint8_t		interrupt_number;
};

#define	SMBIOS_START		0xf0000
#define	SMBIOS_STEP		0x10
#define	SMBIOS_OFF		0
#define	SMBIOS_LEN		4
#define	SMBIOS_SIG		"_SM_"

devclass_t ipmi_devclass;
typedef void (*dispatchproc_t)(uint8_t *p, char **table,
    struct ipmi_get_info *info);

static void smbios_run_table(uint8_t *, int, dispatchproc_t *,
    struct ipmi_get_info *);
static char *get_strings(char *, char **);
static int smbios_cksum	(struct smbios_table_entry *);
static void smbios_t38_proc_info(uint8_t *, char **, struct ipmi_get_info *);
static int ipmi_smbios_attach	(device_t);
static int ipmi_smbios_modevent(module_t, int, void *);

static void
smbios_t38_proc_info(uint8_t *p, char **table, struct ipmi_get_info *info)
{
	struct ipmi_device *s = (struct ipmi_device *) p;

	bzero(info, sizeof(struct ipmi_get_info));
	if (s->interface_type == 0x01)
		info->kcs_mode = 1;
	if (s->interface_type == 0x02)
		info->smic_mode = 1;
	info->address = s->base_address & ~1;
	switch (s->base_address_modifier >> 6) {
	case 0x00:
		info->offset = 1;
		break;
	case 0x01:
		info->offset = 4;
		break;
	case 0x10:
		info->offset = 2;
		break;
	case 0x11:
		info->offset = 0;
		break;
	}
	info->io_mode = s->base_address & 1;
}

static char *
get_strings(char *p, char **table)
{
	/* Scan for strings, stoping at a single null byte */
	while (*p != 0) {
		*table++ = p;
		p += strlen(p) + 1;
	}
	*table = 0;

	/* Skip past terminating null byte */
	return p + 1;
}


static void
smbios_run_table(uint8_t *p, int entries, dispatchproc_t *dispatchstatus,
    struct ipmi_get_info *info)
{
	struct structure_header *s;
	char *table[20];
	uint8_t *nextp;

	while(entries--) {
		s = (struct structure_header *) p;
		nextp = get_strings(p + s->length, table);

		/*
		 * No strings still has a double-null at the end,
		 * skip over it
		 */
		if (table[0] == 0)
			nextp++;

		if (dispatchstatus[*p]) {
			(dispatchstatus[*p])(p, table, info);
		}
		p = nextp;
	}
}

device_t
ipmi_smbios_identify (driver_t *driver, device_t parent)
{
	device_t child = NULL;
	u_int32_t addr;
	int length;
	int rid1, rid2;

	addr = bios_sigsearch(SMBIOS_START, SMBIOS_SIG, SMBIOS_LEN,
			      SMBIOS_STEP, SMBIOS_OFF);
	if (addr != 0) {
		rid1 = 0;
		length = ((struct smbios_table_entry *)BIOS_PADDRTOVADDR(addr))
		    ->length;

		child = BUS_ADD_CHILD(parent, 0, "ipmi", -1);
		if (driver != NULL)
			device_set_driver(child, driver);
		bus_set_resource(child, SYS_RES_MEMORY, rid1, addr, length);
		rid2 = 1;
		length = ((struct smbios_table_entry *)BIOS_PADDRTOVADDR(addr))
		    ->structure_table_length;
		addr = ((struct smbios_table_entry *)BIOS_PADDRTOVADDR(addr))
		    ->structure_table_address;
		bus_set_resource(child, SYS_RES_MEMORY, rid2, addr, length);
		device_set_desc(child, "System Management BIOS");
	} else {
		device_printf(parent, "Failed to find SMBIOS signature\n");
	}

	return child;
}

int
ipmi_smbios_probe(device_t dev)
{
	struct resource *res1 = NULL, *res2 = NULL;
	int rid1, rid2;
	int error;

	if (ipmi_attached)
		return(EBUSY);

	error = 0;
	rid1 = 0;
	rid2 = 1;
	res1 = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid1,
		0ul, ~0ul, 1, RF_ACTIVE | RF_SHAREABLE);

	if (res1 == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}
	res2 = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid2,
		0ul, ~0ul, 1, RF_ACTIVE | RF_SHAREABLE);
	if (res2 == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}

	if (smbios_cksum(
	    (struct smbios_table_entry *)rman_get_virtual(res1))) {
		device_printf(dev, "SMBIOS checksum failed.\n");
		error = ENXIO;
		goto bad;
	}

bad:
	if (res1)
		bus_release_resource(dev, SYS_RES_MEMORY, rid1, res1);
	if (res2)
		bus_release_resource(dev, SYS_RES_MEMORY, rid2, res2);
	return error;
}


int
ipmi_smbios_query(device_t dev)
{
	struct ipmi_softc *sc = device_get_softc(dev);
	dispatchproc_t dispatch_smbios_ipmi[256];
	struct resource *res = NULL , *res2 = NULL;
	int rid, rid2;
	int error;

	error = 0;

	rid = 0;
	res = bus_alloc_resource(sc->ipmi_smbios_dev, SYS_RES_MEMORY, &rid,
	    0ul, ~0ul, 1, RF_ACTIVE | RF_SHAREABLE );
	if (res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}
	rid2 = 1;
	res2 = bus_alloc_resource(sc->ipmi_smbios_dev, SYS_RES_MEMORY, &rid2,
	    0ul, ~0ul, 1, RF_ACTIVE | RF_SHAREABLE);
	if (res2 == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}

	sc->ipmi_smbios =
	    (struct smbios_table_entry *)rman_get_virtual(res);

	sc->ipmi_busy = 0;

	device_printf(sc->ipmi_smbios_dev, "SMBIOS Version: %d.%02d",
		sc->ipmi_smbios->major_version,
		sc->ipmi_smbios->minor_version);
	if (bcd2bin(sc->ipmi_smbios->BCD_revision))
		printf(", revision: %d.%02d",
			bcd2bin(sc->ipmi_smbios->BCD_revision >> 4),
			bcd2bin(sc->ipmi_smbios->BCD_revision & 0x0f));
	printf("\n");

	bzero(&sc->ipmi_bios_info, sizeof(sc->ipmi_bios_info));

	bzero((void *)dispatch_smbios_ipmi, sizeof(dispatch_smbios_ipmi));
	dispatch_smbios_ipmi[38] = (void *)smbios_t38_proc_info;
	smbios_run_table(
	    (void *)rman_get_virtual(res2),
	    sc->ipmi_smbios->number_structures,
	    dispatch_smbios_ipmi,
	    (void*)&sc->ipmi_bios_info);

bad:
	if (res)
		bus_release_resource(sc->ipmi_smbios_dev, SYS_RES_MEMORY,
		    rid, res);
	res = NULL;
	if (res2)
		bus_release_resource(sc->ipmi_smbios_dev, SYS_RES_MEMORY,
		    rid2, res2);
	res2 = NULL;
	return 0;
}

static int
ipmi_smbios_attach(device_t dev)
{
	struct ipmi_softc *sc = device_get_softc(dev);
	int error;
	int status, flags;

	error = 0;
	sc->ipmi_smbios_dev = dev;
	sc->ipmi_dev = dev;
	ipmi_smbios_query(dev);

	if (sc->ipmi_bios_info.kcs_mode) {
		if (sc->ipmi_bios_info.io_mode)
			device_printf(dev, "KCS mode found at io 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));
		else
			device_printf(dev, "KCS mode found at mem 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));

		sc->ipmi_kcs_status_reg   = sc->ipmi_bios_info.offset;
		sc->ipmi_kcs_command_reg  = sc->ipmi_bios_info.offset;
		sc->ipmi_kcs_data_out_reg = 0;
		sc->ipmi_kcs_data_in_reg  = 0;

		if (sc->ipmi_bios_info.io_mode) {
			sc->ipmi_io_rid = 2;
			sc->ipmi_io_res = bus_alloc_resource(dev,
			    SYS_RES_IOPORT, &sc->ipmi_io_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
				(sc->ipmi_bios_info.offset * 2),
			    sc->ipmi_bios_info.offset * 2,
			    RF_ACTIVE);
		} else {
			sc->ipmi_mem_rid = 2;
			sc->ipmi_mem_res = bus_alloc_resource(dev,
			    SYS_RES_MEMORY, &sc->ipmi_mem_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
			        (sc->ipmi_bios_info.offset * 2),
			    sc->ipmi_bios_info.offset * 2,
			    RF_ACTIVE);
		}

		if (!sc->ipmi_io_res){
			device_printf(dev,
			    "couldn't configure smbios io res\n");
			goto bad;
		}

		status = INB(sc, sc->ipmi_kcs_status_reg);
		if (status == 0xff) {
			device_printf(dev, "couldn't find it\n");
			error = ENXIO;
			goto bad;
		}
		if(status & KCS_STATUS_OBF){
			ipmi_read(dev, NULL, 0);
		}
	} else if (sc->ipmi_bios_info.smic_mode) {
		if (sc->ipmi_bios_info.io_mode)
			device_printf(dev, "SMIC mode found at io 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));
		else
			device_printf(dev, "SMIC mode found at mem 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));

		sc->ipmi_smic_data    = 0;
		sc->ipmi_smic_ctl_sts = sc->ipmi_bios_info.offset;
		sc->ipmi_smic_flags   = sc->ipmi_bios_info.offset * 2;

		if (sc->ipmi_bios_info.io_mode) {
			sc->ipmi_io_rid = 2;
			sc->ipmi_io_res = bus_alloc_resource(dev,
			    SYS_RES_IOPORT, &sc->ipmi_io_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
				(sc->ipmi_bios_info.offset * 3),
			    sc->ipmi_bios_info.offset * 3,
			    RF_ACTIVE);
		} else {
			sc->ipmi_mem_res = bus_alloc_resource(dev,
			    SYS_RES_MEMORY, &sc->ipmi_mem_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
			        (sc->ipmi_bios_info.offset * 2),
			    sc->ipmi_bios_info.offset * 2,
			    RF_ACTIVE);
		}

		if (!sc->ipmi_io_res && !sc->ipmi_mem_res){
			device_printf(dev, "couldn't configure smbios res\n");
			error = ENXIO;
			goto bad;
		}

		flags = INB(sc, sc->ipmi_smic_flags);
		if (flags == 0xff) {
			device_printf(dev, "couldn't find it\n");
			error = ENXIO;
			goto bad;
		}
		if ((flags & SMIC_STATUS_SMS_ATN)
		    && (flags & SMIC_STATUS_RX_RDY)){
			/* skip in smbio mode
			ipmi_read(dev, NULL, 0);
			*/
		}
	} else {
		device_printf(dev, "No IPMI interface found\n");
		error = ENXIO;
		goto bad;
	}
	ipmi_attach(dev);

	return 0;
bad:
	/*
	device_delete_child(device_get_parent(dev), dev);
	*/
	return error;
}

static int
smbios_cksum (struct smbios_table_entry *e)
{
	u_int8_t *ptr;
	u_int8_t cksum;
	int i;

	ptr = (u_int8_t *)e;
	cksum = 0;
	for (i = 0; i < e->length; i++) {
		cksum += ptr[i];
	}

	return cksum;
}


static int
ipmi_smbios_detach (device_t dev)
{
	struct ipmi_softc *sc;

	sc = device_get_softc(dev);
	ipmi_detach(dev);
	if (sc->ipmi_ev_tag)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ipmi_ev_tag);

	if (sc->ipmi_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->ipmi_mem_rid,
		    sc->ipmi_mem_res);
	if (sc->ipmi_io_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->ipmi_io_rid,
		    sc->ipmi_io_res);
	return 0;
}


static device_method_t ipmi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,      ipmi_smbios_identify),
	DEVMETHOD(device_probe,         ipmi_smbios_probe),
	DEVMETHOD(device_attach,        ipmi_smbios_attach),
	DEVMETHOD(device_detach,        ipmi_smbios_detach),
	{ 0, 0 }
};

static driver_t ipmi_smbios_driver = {
	"ipmi",
	ipmi_methods,
	sizeof(struct ipmi_softc),
};

static int
ipmi_smbios_modevent (mod, what, arg)
        module_t        mod;
        int             what;
        void *          arg;
{
	device_t *	devs;
	int		count;
	int		i;

	switch (what) {
	case MOD_LOAD:
		return 0;
	case MOD_UNLOAD:
		devclass_get_devices(ipmi_devclass, &devs, &count);
		for (i = 0; i < count; i++) {
			device_delete_child(device_get_parent(devs[i]),
					    devs[i]);
		}
		break;
	default:
		break;
	}

	return 0;
}

DRIVER_MODULE(ipmi, isa, ipmi_smbios_driver, ipmi_devclass,
    ipmi_smbios_modevent, 0);
