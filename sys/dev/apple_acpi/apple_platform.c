/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Apple platform ACPI: Wake-on-LAN and lid state.
 *
 * Walks the ACPI namespace on shutdown calling _PSW(1) on any device
 * with ACPI_FLAG_WAKE_ENABLED set, arming EC.EWOL for wake on magic
 * packet.  Also follows acpi_lid(4) to blank and restore the panel
 * backlight.
 */

#include <sys/cdefs.h>
#include "apple_acpi.h"

/*
 * APP9876 and APP000B attach concurrently as ACPI_APPLE_PLATFORM on T2
 * Macs with a discrete GPU.  Refcount the shutdown and lid hooks here so
 * the second instance's attach/detach doesn't reinstall or rip out the
 * first instance's hooks.
 */
static struct mtx	acpi_apple_platform_mtx;
MTX_SYSINIT(acpi_apple_platform_mtx_init, &acpi_apple_platform_mtx,
    "apple_platform", MTX_DEF);
static int		acpi_apple_platform_refs;
static eventhandler_tag	acpi_apple_shutdown_tag;
static eventhandler_tag	acpi_apple_lid_tag;

static ACPI_STATUS
acpi_apple_psw_cb(ACPI_HANDLE handle, UINT32 level __unused, void *context,
    void **retval __unused)
{
	ACPI_HANDLE psw_handle;
	device_t dev;
	int enable;

	enable = (int)(uintptr_t)context;

	if (ACPI_FAILURE(AcpiGetHandle(handle, "_PSW", &psw_handle)))
		return (AE_OK);

	dev = acpi_get_device(handle);
	if (dev == NULL)
		return (AE_OK);

	if (enable && (acpi_get_flags(dev) & ACPI_FLAG_WAKE_ENABLED) == 0)
		return (AE_OK);

	(void)acpi_SetInteger(handle, "_PSW", enable);
	if (bootverbose)
		device_printf(dev, "acpi_apple: _PSW(%d) on %s\n",
		    enable, acpi_name(handle));

	return (AE_OK);
}

static void
acpi_apple_arm_wol(void *arg __unused, int howto __unused)
{
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    ACPI_UINT32_MAX, acpi_apple_psw_cb, NULL,
	    (void *)(uintptr_t)1, NULL);
}

/*
 * The GMUX handshake polls for milliseconds, so keep it out of the
 * acpi_lid(4) notify path, which runs with ACPI_SERIAL(lid) held.
 */
static void
acpi_apple_backlight_task(void *context)
{
	if (context != NULL)
		acpi_apple_backlight_restore();
	else
		acpi_apple_backlight_off();
}

static void
acpi_apple_lid_event(void *arg __unused, int lid_open)
{
	AcpiOsExecute(OSL_NOTIFY_HANDLER, acpi_apple_backlight_task,
	    (void *)(uintptr_t)lid_open);
}

int
acpi_apple_platform_attach(struct acpi_apple_softc *sc __unused)
{
	mtx_lock(&acpi_apple_platform_mtx);
	if (acpi_apple_platform_refs++ == 0) {
		acpi_apple_shutdown_tag = EVENTHANDLER_REGISTER(
		    shutdown_final, acpi_apple_arm_wol, NULL,
		    SHUTDOWN_PRI_LAST);
		acpi_apple_lid_tag = EVENTHANDLER_REGISTER(acpi_lid_event,
		    acpi_apple_lid_event, NULL, EVENTHANDLER_PRI_ANY);
	}
	mtx_unlock(&acpi_apple_platform_mtx);

	return (0);
}

void
acpi_apple_platform_detach(struct acpi_apple_softc *sc __unused)
{
	mtx_lock(&acpi_apple_platform_mtx);
	KASSERT(acpi_apple_platform_refs > 0,
	    ("acpi_apple: platform refcount underflow"));
	if (acpi_apple_platform_refs > 0 && --acpi_apple_platform_refs == 0) {
		if (acpi_apple_lid_tag != NULL) {
			EVENTHANDLER_DEREGISTER(acpi_lid_event,
			    acpi_apple_lid_tag);
			acpi_apple_lid_tag = NULL;
		}
		if (acpi_apple_shutdown_tag != NULL) {
			EVENTHANDLER_DEREGISTER(shutdown_final,
			    acpi_apple_shutdown_tag);
			acpi_apple_shutdown_tag = NULL;
		}
	}
	mtx_unlock(&acpi_apple_platform_mtx);
}
