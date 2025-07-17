/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Johannes Lundberg <johalun@FreeBSD.org>
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

#include "opt_acpi.h"

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/uuid.h>

#include <acpi/acpi_bus.h>
#include <acpi/video.h>

#define	ACPI_AC_CLASS	"ac_adapter"

ACPI_MODULE_NAME("linux_acpi")

enum {
	LINUX_ACPI_ACAD,
	LINUX_ACPI_VIDEO,
	LINUX_ACPI_TAGS			/* must be last */
};
_Static_assert(LINUX_ACPI_TAGS <= LINUX_NOTIFY_TAGS,
    "Not enough space for tags in notifier_block structure");

#ifdef DEV_ACPI

suspend_state_t pm_suspend_target_state = PM_SUSPEND_ON;

static uint32_t linux_acpi_target_sleep_state = ACPI_STATE_S0;

static eventhandler_tag resume_tag;
static eventhandler_tag suspend_tag;

ACPI_HANDLE
bsd_acpi_get_handle(device_t bsddev)
{
	return (acpi_get_handle(bsddev));
}

bool
acpi_check_dsm(ACPI_HANDLE handle, const char *uuid, int rev, uint64_t funcs)
{

	if (funcs == 0)
		return (false);

	/*
	 * From ACPI 6.3 spec 9.1.1:
	 * Bit 0 indicates whether there is support for any functions other
	 * than function 0 for the specified UUID and Revision ID. If set to
	 * zero, no functions are supported (other than function zero) for the
	 * specified UUID and Revision ID.
	 */
	funcs |= 1 << 0;

	return ((acpi_DSMQuery(handle, uuid, rev) & funcs) == funcs);
}

ACPI_OBJECT *
acpi_evaluate_dsm_typed(ACPI_HANDLE handle, const char *uuid, int rev,
    int func, ACPI_OBJECT *argv4, ACPI_OBJECT_TYPE type)
{
	ACPI_BUFFER buf;

	return (ACPI_SUCCESS(acpi_EvaluateDSMTyped(handle, uuid, rev, func,
	    argv4, &buf, type)) ? (ACPI_OBJECT *)buf.Pointer : NULL);
}

union linuxkpi_acpi_object *
acpi_evaluate_dsm(ACPI_HANDLE ObjHandle, const guid_t *guid,
    UINT64 rev, UINT64 func, union linuxkpi_acpi_object *pkg)
{
	ACPI_BUFFER buf;

	return (ACPI_SUCCESS(acpi_EvaluateDSM(ObjHandle, (const uint8_t *)guid,
	    rev, func, (ACPI_OBJECT *)pkg, &buf)) ?
	    (union linuxkpi_acpi_object *)buf.Pointer : NULL);
}

static void
linux_handle_power_suspend_event(void *arg __unused)
{
	/*
	 * Only support S3 for now.
	 * acpi_sleep_event isn't always called so we use power_suspend_early
	 * instead which means we don't know what state we're switching to.
	 * TODO: Make acpi_sleep_event consistent
	 */
	linux_acpi_target_sleep_state = ACPI_STATE_S3;
	pm_suspend_target_state = PM_SUSPEND_MEM;
}

static void
linux_handle_power_resume_event(void *arg __unused)
{
	linux_acpi_target_sleep_state = ACPI_STATE_S0;
	pm_suspend_target_state = PM_SUSPEND_ON;
}

static void
linux_handle_acpi_acad_event(void *arg, int data)
{
	struct notifier_block *nb = arg;
	/*
	 * Event type information is lost ATM in FreeBSD ACPI event handler.
	 * Fortunately, drm-kmod do not distinct AC event types too, so we can
	 * use any type e.g. ACPI_NOTIFY_BUS_CHECK that suits notifier handler.
	 */
	struct acpi_bus_event abe = {
	    .device_class = ACPI_AC_CLASS,
	    .type = ACPI_NOTIFY_BUS_CHECK,
	    .data = data,
	};

	nb->notifier_call(nb, 0, &abe);
}

static void
linux_handle_acpi_video_event(void *arg, int type)
{
	struct notifier_block *nb = arg;
	struct acpi_bus_event abe = {
	    .device_class = ACPI_VIDEO_CLASS,
	    .type = type,
	    .data = 0,
	};

	nb->notifier_call(nb, 0, &abe);
}

int
register_acpi_notifier(struct notifier_block *nb)
{
	nb->tags[LINUX_ACPI_ACAD] = EVENTHANDLER_REGISTER(acpi_acad_event,
	    linux_handle_acpi_acad_event, nb, EVENTHANDLER_PRI_FIRST);
	nb->tags[LINUX_ACPI_VIDEO] = EVENTHANDLER_REGISTER(acpi_video_event,
	    linux_handle_acpi_video_event, nb, EVENTHANDLER_PRI_FIRST);

	return (0);
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	EVENTHANDLER_DEREGISTER(acpi_acad_event, nb->tags[LINUX_ACPI_ACAD]);
	EVENTHANDLER_DEREGISTER(acpi_video_event, nb->tags[LINUX_ACPI_VIDEO]);

	return (0);
}

uint32_t
acpi_target_system_state(void)
{
	return (linux_acpi_target_sleep_state);
}

struct acpi_dev_present_ctx {
	const char *hid;
	const char *uid;
	int64_t hrv;
	struct acpi_device *dev;
};

static ACPI_STATUS
acpi_dev_present_cb(ACPI_HANDLE handle, UINT32 level, void *context,
    void **result)
{
	ACPI_DEVICE_INFO *devinfo;
	struct acpi_device *dev;
	struct acpi_dev_present_ctx *match = context;
	bool present = false;
	UINT32 sta, hrv;
	int i;

	if (handle == NULL)
		return (AE_OK);

	if (!ACPI_FAILURE(acpi_GetInteger(handle, "_STA", &sta)) &&
	    !ACPI_DEVICE_PRESENT(sta))
		return (AE_OK);

	if (ACPI_FAILURE(AcpiGetObjectInfo(handle, &devinfo)))
		return (AE_OK);

	if ((devinfo->Valid & ACPI_VALID_HID) != 0 &&
	    strcmp(match->hid, devinfo->HardwareId.String) == 0) {
		present = true;
	} else if ((devinfo->Valid & ACPI_VALID_CID) != 0) {
		for (i = 0; i < devinfo->CompatibleIdList.Count; i++) {
			if (strcmp(match->hid,
			    devinfo->CompatibleIdList.Ids[i].String) == 0) {
				present = true;
				break;
			}
		}
	}
	if (present && match->uid != NULL &&
	    ((devinfo->Valid & ACPI_VALID_UID) == 0 ||
	      strcmp(match->uid, devinfo->UniqueId.String) != 0))
		present = false;

	AcpiOsFree(devinfo);
	if (!present)
		return (AE_OK);

	if (match->hrv != -1) {
		if (ACPI_FAILURE(acpi_GetInteger(handle, "_HRV", &hrv)))
			return (AE_OK);
		if (hrv != match->hrv)
			return (AE_OK);
	}

	dev = acpi_get_device(handle);
	if (dev == NULL)
		return (AE_OK);
	match->dev = dev;

	return (AE_ERROR);
}

bool
lkpi_acpi_dev_present(const char *hid, const char *uid, int64_t hrv)
{
	struct acpi_dev_present_ctx match;
	int rv;

	match.hid = hid;
	match.uid = uid;
	match.hrv = hrv;

	rv = AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    ACPI_UINT32_MAX, acpi_dev_present_cb, NULL, &match, NULL);

	return (rv == AE_ERROR);
}

struct acpi_device *
lkpi_acpi_dev_get_first_match_dev(const char *hid, const char *uid,
    int64_t hrv)
{
	struct acpi_dev_present_ctx match;
	int rv;

	match.hid = hid;
	match.uid = uid;
	match.hrv = hrv;
	match.dev = NULL;

	rv = AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    ACPI_UINT32_MAX, acpi_dev_present_cb, NULL, &match, NULL);

	return (rv == AE_ERROR ? match.dev : NULL);
}

static void
linux_register_acpi_event_handlers(void *arg __unused)
{
	/*
	 * XXX johalun: acpi_{sleep,wakeup}_event can't be trusted, use
	 * power_{suspend_early,resume} 'acpiconf -s 3' or 'zzz' will not
	 * generate acpi_sleep_event... Lid open or wake on button generates
	 * acpi_wakeup_event on one of my Dell laptops but not the other
	 * (but it does power on)... is this a general thing?
	 */
	resume_tag = EVENTHANDLER_REGISTER(power_resume,
	    linux_handle_power_resume_event, NULL, EVENTHANDLER_PRI_FIRST);
	suspend_tag = EVENTHANDLER_REGISTER(power_suspend_early,
	    linux_handle_power_suspend_event, NULL, EVENTHANDLER_PRI_FIRST);
}

static void
linux_deregister_acpi_event_handlers(void *arg __unused)
{
	EVENTHANDLER_DEREGISTER(power_resume, resume_tag);
	EVENTHANDLER_DEREGISTER(power_suspend_early, suspend_tag);
}

SYSINIT(linux_acpi_events, SI_SUB_DRIVERS, SI_ORDER_ANY,
    linux_register_acpi_event_handlers, NULL);
SYSUNINIT(linux_acpi_events, SI_SUB_DRIVERS, SI_ORDER_ANY,
    linux_deregister_acpi_event_handlers, NULL);

#else	/* !DEV_ACPI */

ACPI_HANDLE
bsd_acpi_get_handle(device_t bsddev)
{
	return (NULL);
}

bool
acpi_check_dsm(ACPI_HANDLE handle, const char *uuid, int rev, uint64_t funcs)
{
	return (false);
}

ACPI_OBJECT *
acpi_evaluate_dsm_typed(ACPI_HANDLE handle, const char *uuid, int rev,
     int func, ACPI_OBJECT *argv4, ACPI_OBJECT_TYPE type)
{
	return (NULL);
}

union linuxkpi_acpi_object *
acpi_evaluate_dsm(ACPI_HANDLE ObjHandle, const guid_t *guid,
    UINT64 rev, UINT64 func, union linuxkpi_acpi_object *pkg)
{
	return (NULL);
}

int
register_acpi_notifier(struct notifier_block *nb)
{
	return (0);
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	return (0);
}

uint32_t
acpi_target_system_state(void)
{
	return (ACPI_STATE_S0);
}

bool
lkpi_acpi_dev_present(const char *hid, const char *uid, int64_t hrv)
{
	return (false);
}

struct acpi_device *
lkpi_acpi_dev_get_first_match_dev(const char *hid, const char *uid,
    int64_t hrv)
{
	return (NULL);
}

#endif	/* !DEV_ACPI */
