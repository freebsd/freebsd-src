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
 *
 * $FreeBSD$
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
}

static void
linux_handle_power_resume_event(void *arg __unused)
{
	linux_acpi_target_sleep_state = ACPI_STATE_S0;
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

#endif	/* !DEV_ACPI */
