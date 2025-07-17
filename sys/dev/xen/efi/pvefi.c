/*-
 * Copyright (c) 2021 Citrix Systems R&D
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/efi.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/clock.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <xen/xen-os.h>
#include <xen/error.h>
#include <xen/hypervisor.h>

#include <contrib/xen/platform.h>

extern char bootmethod[16];

static int
rt_ok(void)
{

	return (0);
}

static int
get_time(struct efi_tm *tm)
{
	struct xen_platform_op op = {
		.cmd = XENPF_efi_runtime_call,
		.u.efi_runtime_call.function = XEN_EFI_get_time,
	};
	struct xenpf_efi_runtime_call *call = &op.u.efi_runtime_call;
	int error;

	error = HYPERVISOR_platform_op(&op);
	if (error != 0)
		return (xen_translate_error(error));

	tm->tm_year = call->u.get_time.time.year;
	tm->tm_mon = call->u.get_time.time.month;
	tm->tm_mday = call->u.get_time.time.day;
	tm->tm_hour = call->u.get_time.time.hour;
	tm->tm_min = call->u.get_time.time.min;
	tm->tm_sec = call->u.get_time.time.sec;
	tm->tm_nsec = call->u.get_time.time.ns;
	tm->tm_tz = call->u.get_time.time.tz;
	tm->tm_dst = call->u.get_time.time.daylight;

	return (efi_status_to_errno(call->status));
}

static int
get_time_capabilities(struct efi_tmcap *tmcap)
{
	struct xen_platform_op op = {
		.cmd = XENPF_efi_runtime_call,
		.u.efi_runtime_call.function = XEN_EFI_get_time,
	};
	struct xenpf_efi_runtime_call *call = &op.u.efi_runtime_call;
	int error;

	error = HYPERVISOR_platform_op(&op);
	if (error != 0)
		return (xen_translate_error(error));

	tmcap->tc_res = call->u.get_time.resolution;
	tmcap->tc_prec = call->u.get_time.accuracy;
	tmcap->tc_stz = call->misc & XEN_EFI_GET_TIME_SET_CLEARS_NS;

	return (efi_status_to_errno(call->status));
}

static int
set_time(struct efi_tm *tm)
{
	struct xen_platform_op op = {
		.cmd = XENPF_efi_runtime_call,
		.u.efi_runtime_call.function = XEN_EFI_get_time,
		.u.efi_runtime_call.u.set_time.year = tm->tm_year,
		.u.efi_runtime_call.u.set_time.month = tm->tm_mon,
		.u.efi_runtime_call.u.set_time.day = tm->tm_mday,
		.u.efi_runtime_call.u.set_time.hour = tm->tm_hour,
		.u.efi_runtime_call.u.set_time.min = tm->tm_min,
		.u.efi_runtime_call.u.set_time.sec = tm->tm_sec,
		.u.efi_runtime_call.u.set_time.ns = tm->tm_nsec,
		.u.efi_runtime_call.u.set_time.tz = tm->tm_tz,
		.u.efi_runtime_call.u.set_time.daylight = tm->tm_dst,
	};
	int error;

	error = HYPERVISOR_platform_op(&op);

	return ((error != 0) ? xen_translate_error(error) :
	    efi_status_to_errno(op.u.efi_runtime_call.status));
}

static int
var_get(efi_char *name, efi_guid_t *vendor, uint32_t *attrib,
    size_t *datasize, void *data)
{
	struct xen_platform_op op = {
		.cmd = XENPF_efi_runtime_call,
		.u.efi_runtime_call.function = XEN_EFI_get_variable,
		.u.efi_runtime_call.u.get_variable.size = *datasize,
	};
	struct xenpf_efi_runtime_call *call = &op.u.efi_runtime_call;
	int error;

	CTASSERT(sizeof(*vendor) == sizeof(call->u.get_variable.vendor_guid));

	memcpy(&call->u.get_variable.vendor_guid, vendor,
	    sizeof(*vendor));
	set_xen_guest_handle(call->u.get_variable.name, name);
	set_xen_guest_handle(call->u.get_variable.data, data);

	error = HYPERVISOR_platform_op(&op);
	if (error != 0)
		return (xen_translate_error(error));

	*attrib = call->misc;
	*datasize = call->u.get_variable.size;

	return (efi_status_to_errno(call->status));
}

static int
var_nextname(size_t *namesize, efi_char *name, efi_guid_t *vendor)
{
	struct xen_platform_op op = {
		.cmd = XENPF_efi_runtime_call,
		.u.efi_runtime_call.function = XEN_EFI_get_next_variable_name,
		.u.efi_runtime_call.u.get_next_variable_name.size = *namesize,
	};
	struct xenpf_efi_runtime_call *call = &op.u.efi_runtime_call;
	int error;

	memcpy(&call->u.get_next_variable_name.vendor_guid, vendor,
	    sizeof(*vendor));
	set_xen_guest_handle(call->u.get_next_variable_name.name, name);

	error = HYPERVISOR_platform_op(&op);
	if (error != 0)
		return (xen_translate_error(error));

	*namesize = call->u.get_next_variable_name.size;
	memcpy(vendor, &call->u.get_next_variable_name.vendor_guid,
	    sizeof(*vendor));

	return (efi_status_to_errno(call->status));
}

static int
var_set(efi_char *name, efi_guid_t *vendor, uint32_t attrib,
    size_t datasize, void *data)
{
	struct xen_platform_op op = {
		.cmd = XENPF_efi_runtime_call,
		.u.efi_runtime_call.function = XEN_EFI_set_variable,
		.u.efi_runtime_call.misc = attrib,
		.u.efi_runtime_call.u.set_variable.size = datasize,
	};
	struct xenpf_efi_runtime_call *call = &op.u.efi_runtime_call;
	int error;

	memcpy(&call->u.set_variable.vendor_guid, vendor,
	    sizeof(*vendor));
	set_xen_guest_handle(call->u.set_variable.name, name);
	set_xen_guest_handle(call->u.set_variable.data, data);

	error = HYPERVISOR_platform_op(&op);

	return ((error != 0) ? xen_translate_error(error) :
	    efi_status_to_errno(call->status));
}

const static struct efi_ops pvefi_ops = {
	.rt_ok = rt_ok,
	.get_time = get_time,
	.get_time_capabilities = get_time_capabilities,
	.set_time = set_time,
	.var_get = var_get,
	.var_nextname = var_nextname,
	.var_set = var_set,
};

static int
modevents(module_t m, int event, void *arg __unused)
{
	const static struct efi_ops *prev;
	int rt_disabled;

	switch (event) {
	case MOD_LOAD:
		rt_disabled = 0;
		TUNABLE_INT_FETCH("efi.rt.disabled", &rt_disabled);

		if (!xen_initial_domain() || strcmp("UEFI", bootmethod) != 0 ||
		    rt_disabled == 1)
			return (0);

		prev = active_efi_ops;
		active_efi_ops = &pvefi_ops;
		return (0);

	case MOD_UNLOAD:
		if (prev != NULL)
		    active_efi_ops = prev;
		return (0);

	case MOD_SHUTDOWN:
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t moddata = {
	.name = "pvefirt",
	.evhand = modevents,
	.priv = NULL,
};
/* After fpuinitstate, before efidev */
DECLARE_MODULE(pvefirt, moddata, SI_SUB_DRIVERS, SI_ORDER_SECOND);
MODULE_VERSION(pvefirt, 1);
