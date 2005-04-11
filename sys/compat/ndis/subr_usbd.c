/*-
 * Copyright (c) 2005
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/types.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/bus.h>

#include <sys/queue.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/usbd_var.h>

static driver_object usbd_driver;

static uint32_t usbd_iodispatch(device_object *, irp *);

static void USBD_GetUSBDIVersion(usbd_version_info *);
static void dummy(void);

int
usbd_libinit(void)
{
	image_patch_table	*patch;

	patch = usbd_functbl;
	while (patch->ipt_func != NULL) {
		windrv_wrap((funcptr)patch->ipt_func,
		    (funcptr *)&patch->ipt_wrap,
		    patch->ipt_argcnt, patch->ipt_ftype);
		patch++;
	}

	/* Create a fake USB driver instance. */

	windrv_bus_attach(&usbd_driver, "USB Bus");

	/* Set up our dipatch routine. */

	usbd_driver.dro_dispatch[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
	    (driver_dispatch)usbd_iodispatch;

	return(0);
}

int
usbd_libfini(void)
{
	image_patch_table	*patch;

	patch = usbd_functbl;
	while (patch->ipt_func != NULL) {
		windrv_unwrap(patch->ipt_wrap);
		patch++;
	}

	free(usbd_driver.dro_drivername.us_buf, M_DEVBUF);

	return(0);
}

static uint32_t
usbd_iodispatch(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	return(0);
}

static void
USBD_GetUSBDIVersion(ui)
	usbd_version_info	*ui;
{
	/* Pretend to be Windows XP. */

	ui->uvi_usbdi_vers = USBDI_VERSION;
	ui->uvi_supported_vers = USB_VER_2_0;

	return;
}

static void
dummy(void)
{
	printf("USBD dummy called\n");
	return;
}

image_patch_table usbd_functbl[] = {
	IMPORT_SFUNC(USBD_GetUSBDIVersion, 0),
	IMPORT_SFUNC(usbd_iodispatch, 2),
#ifdef notyet
	IMPORT_FUNC_MAP(_USBD_ParseConfigurationDescriptorEx@28,
	    USBD_ParseConfigurationDescriptorEx),
	IMPORT_FUNC_MAP(_USBD_CreateConfigurationRequestEx@8,
	    USBD_CreateConfigurationRequestEx),
#endif

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy, NULL, 0, WINDRV_WRAP_CDECL },

	/* End of list. */

	{ NULL, NULL, NULL }
};

