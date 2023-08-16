/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>.  All rights reserved.
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

/*
 * VM Generation Counter driver
 *
 * See, e.g., the "Virtual Machine Generation ID" white paper:
 * https://go.microsoft.com/fwlink/p/?LinkID=260709 , and perhaps also:
 * https://docs.microsoft.com/en-us/windows/win32/hyperv_v2/virtual-machine-generation-identifier ,
 * https://azure.microsoft.com/en-us/blog/accessing-and-using-azure-vm-unique-id/
 *
 * Microsoft introduced the concept in 2013 or so and seems to have
 * successfully driven it to a consensus standard among hypervisors, not just
 * HyperV/Azure:
 * - QEMU: https://bugzilla.redhat.com/show_bug.cgi?id=1118834
 * - VMware/ESXi: https://kb.vmware.com/s/article/2032586
 * - Xen: https://github.com/xenserver/xen-4.5/blob/master/tools/firmware/hvmloader/acpi/dsdt.asl#L456
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/random/random_harvestq.h>
#include <dev/vmgenc/vmgenc_acpi.h>

#ifndef	ACPI_NOTIFY_STATUS_CHANGED
#define	ACPI_NOTIFY_STATUS_CHANGED	0x80
#endif

#define	GUID_BYTES	16

static const char *vmgenc_ids[] = {
	"VM_GEN_COUNTER",
	NULL
};
#if 0
MODULE_PNP_INFO("Z:_CID", acpi, vmgenc, vmgenc_ids, nitems(vmgenc_ids) - 1);
#endif

struct vmgenc_softc {
	volatile void	*vmg_pguid;
	uint8_t		vmg_cache_guid[GUID_BYTES];
};

static void
vmgenc_harvest_all(const void *p, size_t sz)
{
	size_t nbytes;

	while (sz > 0) {
		nbytes = MIN(sz,
		    sizeof(((struct harvest_event *)0)->he_entropy));
		random_harvest_direct(p, nbytes, RANDOM_PURE_VMGENID);
		p = (const char *)p + nbytes;
		sz -= nbytes;
	}
}

static void
vmgenc_status_changed(void *context)
{
	uint8_t guid[GUID_BYTES];
	struct vmgenc_softc *sc;
	device_t dev;

	dev = context;
	sc = device_get_softc(dev);

	/* Check for spurious notify events. */
	memcpy(guid, __DEVOLATILE(void *, sc->vmg_pguid), sizeof(guid));
	if (memcmp(guid, sc->vmg_cache_guid, GUID_BYTES) == 0)
		return; /* No change. */

	/* Update cache. */
	memcpy(sc->vmg_cache_guid, guid, GUID_BYTES);

	vmgenc_harvest_all(sc->vmg_cache_guid, sizeof(sc->vmg_cache_guid));

	EVENTHANDLER_INVOKE(acpi_vmgenc_event);
	acpi_UserNotify("VMGenerationCounter", acpi_get_handle(dev), 0);
}

static void
vmgenc_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
	device_t dev;

	dev = context;
	switch (notify) {
	case ACPI_NOTIFY_STATUS_CHANGED:
		/*
		 * We're possibly in GPE / interrupt context, kick the event up
		 * to a taskqueue.
		 */
		AcpiOsExecute(OSL_NOTIFY_HANDLER, vmgenc_status_changed, dev);
		break;
	default:
		device_printf(dev, "unknown notify %#x\n", notify);
		break;
	}
}

static int
vmgenc_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("vmgenc"))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev,
	    __DECONST(char **, vmgenc_ids), NULL);
	if (rv <= 0)
		device_set_desc(dev, "VM Generation Counter");
	return (rv);
}

static const char *
vmgenc_acpi_getname(ACPI_HANDLE handle, char data[static 256])
{
    ACPI_BUFFER buf;

    buf.Length = 256;
    buf.Pointer = data;

    if (ACPI_SUCCESS(AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf)))
	return (data);
    return ("(unknown)");
}

static int
acpi_GetPackedUINT64(device_t dev, ACPI_HANDLE handle, char *path,
    uint64_t *out)
{
	char hpath[256];
	ACPI_STATUS status;
	ACPI_BUFFER buf;
	ACPI_OBJECT param[3];

	buf.Pointer = param;
	buf.Length = sizeof(param);
	status = AcpiEvaluateObject(handle, path, NULL, &buf);
	if (!ACPI_SUCCESS(status)) {
		device_printf(dev, "%s(%s::%s()): %s\n", __func__,
		    vmgenc_acpi_getname(handle, hpath), path,
		    AcpiFormatException(status));
		return (ENXIO);
	}
	if (param[0].Type != ACPI_TYPE_PACKAGE) {
		device_printf(dev, "%s(%s::%s()): Wrong type %#x\n", __func__,
		    vmgenc_acpi_getname(handle, hpath), path,
		    param[0].Type);
		return (ENXIO);
	}
	if (param[0].Package.Count != 2) {
		device_printf(dev, "%s(%s::%s()): Wrong number of results %u\n",
		    __func__, vmgenc_acpi_getname(handle, hpath), path,
		    param[0].Package.Count);
		return (ENXIO);
	}
	if (param[0].Package.Elements[0].Type != ACPI_TYPE_INTEGER ||
	    param[0].Package.Elements[1].Type != ACPI_TYPE_INTEGER) {
		device_printf(dev, "%s(%s::%s()): Wrong type results %#x, %#x\n",
		    __func__, vmgenc_acpi_getname(handle, hpath), path,
		    param[0].Package.Elements[0].Type,
		    param[0].Package.Elements[1].Type);
		return (ENXIO);
	}

	*out = (param[0].Package.Elements[0].Integer.Value & UINT32_MAX) |
	    ((uint64_t)param[0].Package.Elements[1].Integer.Value << 32);
	if (*out == 0)
		return (ENXIO);
	return (0);

}

static int
vmgenc_attach(device_t dev)
{
	struct vmgenc_softc *sc;
	uint64_t guid_physaddr;
	ACPI_HANDLE h;
	int error;

	h = acpi_get_handle(dev);
	sc = device_get_softc(dev);

	error = acpi_GetPackedUINT64(dev, h, "ADDR", &guid_physaddr);
	if (error != 0)
		return (error);

	SYSCTL_ADD_OPAQUE(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "guid",
	    CTLFLAG_RD, sc->vmg_cache_guid, GUID_BYTES, "",
	    "latest cached VM generation counter (128-bit UUID)");

	sc->vmg_pguid = AcpiOsMapMemory(guid_physaddr, GUID_BYTES);
	memcpy(sc->vmg_cache_guid, __DEVOLATILE(void *, sc->vmg_pguid),
	    sizeof(sc->vmg_cache_guid));

	random_harvest_register_source(RANDOM_PURE_VMGENID);
	vmgenc_harvest_all(sc->vmg_cache_guid, sizeof(sc->vmg_cache_guid));

	AcpiInstallNotifyHandler(h, ACPI_DEVICE_NOTIFY, vmgenc_notify, dev);
	return (0);
}

static device_method_t vmgenc_methods[] = {
	DEVMETHOD(device_probe,		vmgenc_probe),
	DEVMETHOD(device_attach,	vmgenc_attach),
	DEVMETHOD_END
};

static driver_t vmgenc_driver = {
	"vmgenc",
	vmgenc_methods,
	sizeof(struct vmgenc_softc),
};

DRIVER_MODULE(vmgenc, acpi, vmgenc_driver, NULL, NULL);
MODULE_DEPEND(vmgenc, acpi, 1, 1, 1);
MODULE_DEPEND(vemgenc, random_harvestq, 1, 1, 1);
