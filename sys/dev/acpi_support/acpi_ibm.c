/*-
 * Copyright (c) 2004 Takanori Watanabe
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

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/cpufunc.h>
#include "acpi.h"
#include "acpi_if.h"
#include <sys/module.h>
#include <dev/acpica/acpivar.h>
#include <sys/sysctl.h>
#include <machine/clock.h>

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("IBM")

#define IBM_RTC_MISCKEY 0x65
#define IBM_RTC_BRIGHTNESS 0x6c
#define   IBM_RTC_MASK_BRI 0x7
#define   IBM_RTC_MASK_BRKEY 0x40
#define IBM_RTC_KEYLIGHT 0x66
#define   IBM_RTC_MASK_KEYLIGHT 0x10
#define IBM_RTC_VOLUME 0x6e
#define   IBM_RTC_MASK_VOL 0xf
#define   IBM_RTC_MASK_MUTE 0x40
#define   IBM_RTC_MASK_VOLKEY 0x80

#define IBM_NAME_GET_WIRELESS "GBDC"
#define IBM_NAME_SET_WIRELESS "SBDC"
#define IBM_NAME_INTERFACE_VERSION "MHKV"
#define IBM_NAME_AVAIL_MASK "MHKA"
#define IBM_NAME_CURRENT_MASK "MHKN"
#define IBM_NAME_MODIFY_MASK "MHKM"
#define IBM_NAME_GET_EVENT "MHKP"
#define IBM_NAME_ENABLE "MHKC"
#if 0
/* TPX31 Specific? */
#define IBM_UCMS_VOLDN 0x0
#define IBM_UCMS_VOLUP 0x1
#define IBM_UCMS_MUTE 0x2
#define IBM_UCMS_BRIUP 0x4
#define IBM_UCMS_BRIDN 0x5
#define IBM_UCMS_KEYLIGHT 0xe
#endif

struct acpi_ibm_softc {
	unsigned int	ibm_version;
	unsigned int	ibm_availmask;
	unsigned int	ibm_initialmask;
	int		ibm_enable;
	int		device_flag;
#define IBM_MHKN_AVAIL 1
#define IBM_MHKM_AVAIL 2
	struct sysctl_oid *oid_bluetooth;
	struct sysctl_oid *oid_wlan;
};

static int	acpi_ibm_probe(device_t dev);
static int	acpi_ibm_attach(device_t dev);
static int	acpi_ibm_detach(device_t dev);
static void
acpi_ibm_notify_handler(ACPI_HANDLE h, UINT32 notify,
			void *context);
static int	sysctl_acpi_ibm_mask_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_enable_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_misckey_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_volume_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_mute_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_brightness_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_keylight_handler(SYSCTL_HANDLER_ARGS);
static int	sysctl_acpi_ibm_wireless_handler(SYSCTL_HANDLER_ARGS);
static int	acpi_ibm_enable_mask(device_t dev, int val);

static device_method_t acpi_ibm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, acpi_ibm_probe),
	DEVMETHOD(device_attach, acpi_ibm_attach),
	DEVMETHOD(device_detach, acpi_ibm_detach),

	{0, 0}
};

static driver_t	acpi_ibm_driver = {
	"acpi_ibm",
	acpi_ibm_methods,
	sizeof(struct acpi_ibm_softc),
};

static devclass_t acpi_ibm_devclass;

DRIVER_MODULE(acpi_ibm, acpi, acpi_ibm_driver, acpi_ibm_devclass,
	      0, 0);
MODULE_DEPEND(acpi_ibm, acpi, 1, 1, 1);
static char    *ibm_id[] = {"IBM0068", NULL};

static int
acpi_ibm_probe(device_t dev)
{
	struct acpi_ibm_softc *sc;
	int		ret = ENXIO;

	sc = device_get_softc(dev);

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, ibm_id)) {
		device_set_desc(dev, "IBM ThinkPad Button");
		ret = 0;
	}
	return (ret);
}

static int
acpi_ibm_call_two_method(device_t dev, char *name, int val1, int val2)
{
	ACPI_OBJECT	arg [2];
	ACPI_OBJECT_LIST args = {.Count = 2,.Pointer = arg};
	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = val1;
	arg[1].Type = ACPI_TYPE_INTEGER;
	arg[1].Integer.Value = val2;
	return AcpiEvaluateObject(acpi_get_handle(dev), name, &args, NULL);
}

static int
acpi_ibm_attach(device_t dev)
{
	struct acpi_ibm_softc *sc;
	ACPI_STATUS	status;
	ACPI_HANDLE	h;
	int		dummy;
	struct sysctl_oid *oid;
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->device_flag = 0;
	if (ACPI_FAILURE
	    (acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_INTERFACE_VERSION, &sc->ibm_version))) {
		sc->ibm_version = 0;
	}
	device_printf(dev, "Version %x\n", sc->ibm_version);
	if (ACPI_FAILURE
	    (acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_AVAIL_MASK, &sc->ibm_availmask)))
		sc->ibm_availmask = 0xffffffff;

	if (ACPI_FAILURE
	    (acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_CURRENT_MASK, &sc->ibm_initialmask)))
		sc->ibm_initialmask = 0xffffffff;
	else
		sc->device_flag |= IBM_MHKN_AVAIL;

	if (ACPI_SUCCESS(status = AcpiGetHandle(acpi_get_handle(dev), IBM_NAME_MODIFY_MASK, &h)))
		sc->device_flag |= IBM_MHKM_AVAIL;
	else
		printf("%s\n", AcpiFormatException(status));

	device_printf(dev, "Available Mask %x\n", sc->ibm_availmask);
	device_printf(dev, "Initial Mask %x\n", sc->ibm_initialmask);
	/* Install Specific Handler */
	status = AcpiInstallNotifyHandler(acpi_get_handle(dev), ACPI_DEVICE_NOTIFY, acpi_ibm_notify_handler, dev);
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "key_mask", CTLTYPE_INT | CTLFLAG_RW,
			dev, 0,
			sysctl_acpi_ibm_mask_handler, "I", "Hot key mask");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
		       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		       OID_AUTO, "version", CTLFLAG_RD,
		       &sc->ibm_version, 0, "Interface version");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
		       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		       OID_AUTO, "avail_mask", CTLFLAG_RD,
		       &sc->ibm_availmask, 0, "Available Key mask");
	if (ACPI_FAILURE(acpi_SetInteger(acpi_get_handle(dev), IBM_NAME_ENABLE, 1)))
		goto fail;
	sc->ibm_enable = 1;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "enable", CTLTYPE_INT | CTLFLAG_RW,
			dev, 0,
		     sysctl_acpi_ibm_enable_handler, "I", "Hot key enable");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "misckey", CTLTYPE_INT | CTLFLAG_RD,
			dev, 0,
	       sysctl_acpi_ibm_misckey_handler, "I", "Key Status: Poll me");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "brightness", CTLTYPE_INT | CTLFLAG_RD,
			dev, 0,
		     sysctl_acpi_ibm_brightness_handler, "I", "Brightness");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "volume", CTLTYPE_INT | CTLFLAG_RD,
			dev, 0,
			sysctl_acpi_ibm_volume_handler, "I", "Volume");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "mute", CTLTYPE_INT | CTLFLAG_RD,
			dev, 0,
			sysctl_acpi_ibm_mute_handler, "I", "Muting");


	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "keylight", CTLTYPE_INT | CTLFLAG_RD,
			dev, 0,
			sysctl_acpi_ibm_keylight_handler, "I", "Key Light");
	if (ACPI_SUCCESS(acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_GET_WIRELESS, &dummy))) {
		oid = SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "bluetooth", CTLTYPE_INT | CTLFLAG_RW, dev, 0,
		 sysctl_acpi_ibm_wireless_handler, "I", "Bluetooth Enable");
		sc->oid_bluetooth = oid;
		oid = SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			       SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			 OID_AUTO, "wlan", CTLTYPE_INT | CTLFLAG_RW, dev, 0,
		      sysctl_acpi_ibm_wireless_handler, "I", "WLAN Enable");
		sc->oid_wlan = oid;
	}
	return_VALUE(0);
fail:
	device_printf(dev, "FAILED\n");
	AcpiRemoveNotifyHandler(acpi_get_handle(dev), ACPI_DEVICE_NOTIFY, acpi_ibm_notify_handler);
	return_VALUE(EINVAL);
}

static int
acpi_ibm_detach(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	struct acpi_ibm_softc *sc = device_get_softc(dev);
	acpi_SetInteger(acpi_get_handle(dev), IBM_NAME_ENABLE, 0);
	acpi_ibm_enable_mask(dev, sc->ibm_initialmask);

	AcpiRemoveNotifyHandler(acpi_get_handle(dev), ACPI_DEVICE_NOTIFY, acpi_ibm_notify_handler);
	return_VALUE(0);
}
#if 0
static int
acpi_ibm_suspend(device_t dev)
{
	struct acpi_ibm_softc *sc = device_get_softc(dev);
	return_VALUE(0);
}

static int
acpi_ibm_resume(device_t dev)
{
	return (0);
}
#endif
static void
acpi_ibm_notify_handler(ACPI_HANDLE h, UINT32 notify,
			void *context)
{
	int		mhkp      , arg, type;
	device_t	dev = context;
	struct acpi_ibm_softc *sc = device_get_softc(dev);

	printf("IBM:NOTIFY:%x\n", notify);
	if (notify != 0x80) {
		printf("Unknown notify\n");
	}
	for (;;) {

		acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_GET_EVENT, &mhkp);

		if (mhkp == 0) {
			break;
		}
		printf("notify:%x\n", mhkp);

		type = (mhkp >> 12) & 0xf;
		arg = mhkp & 0xfff;
		switch (type) {
		case 1:
			if (!(sc->ibm_availmask & (1 << (arg - 1)))) {
				printf("Unknown key %d\n", arg);
				break;
			}
			acpi_UserNotify("IBM", h, (arg & 0xff));
			break;
		default:
			break;
		}
	}
}

static int
acpi_ibm_enable_mask(device_t dev, int val)
{
	int		i;
	struct acpi_ibm_softc *sc = device_get_softc(dev);

	if (!(sc->device_flag | IBM_MHKM_AVAIL)) {
		return -1;
	}
	for (i = 0; i < 32; i++) {
		acpi_ibm_call_two_method(dev, IBM_NAME_MODIFY_MASK, i + 1, 1);
		if (!((1 << i) & val))
			acpi_ibm_call_two_method(dev, IBM_NAME_MODIFY_MASK, i + 1, 0);
	}
	return 0;
}

static int
sysctl_acpi_ibm_mask_handler(SYSCTL_HANDLER_ARGS)
{
	device_t	dev = arg1;
	int		val = 0xffffffff;
	int		error = 0;

	struct acpi_ibm_softc *sc = device_get_softc(dev);

	if (sc->device_flag & IBM_MHKN_AVAIL)
		acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_CURRENT_MASK, &val);

	error = sysctl_handle_int(oidp, &val, 0, req);

	if (error || !req->newptr)
		return error;

	val &= sc->ibm_availmask;
	val |= sc->ibm_initialmask;

	acpi_ibm_enable_mask(dev, val);

	return 0;
}

static int 
sysctl_acpi_ibm_misckey_handler(SYSCTL_HANDLER_ARGS)
{
	int		val       , error;
	val = rtcin(IBM_RTC_MISCKEY);
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}


static int 
sysctl_acpi_ibm_brightness_handler(SYSCTL_HANDLER_ARGS)
{
	int		val       , error;
	val = rtcin(IBM_RTC_BRIGHTNESS);
	val &= IBM_RTC_MASK_BRI;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}

static int 
sysctl_acpi_ibm_mute_handler(SYSCTL_HANDLER_ARGS)
{
	int		val       , error;
	val = rtcin(IBM_RTC_VOLUME);
	val = ((val & IBM_RTC_MASK_MUTE) == IBM_RTC_MASK_MUTE);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}

static int 
sysctl_acpi_ibm_keylight_handler(SYSCTL_HANDLER_ARGS)
{
	int		val       , error;
	val = ((rtcin(IBM_RTC_KEYLIGHT) & IBM_RTC_MASK_KEYLIGHT)
	       == IBM_RTC_MASK_KEYLIGHT);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}

static int 
sysctl_acpi_ibm_volume_handler(SYSCTL_HANDLER_ARGS)
{
	int		val       , error;
	val = rtcin(IBM_RTC_VOLUME);
	val &= IBM_RTC_MASK_VOL;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}

static int
sysctl_acpi_ibm_enable_handler(SYSCTL_HANDLER_ARGS)
{
	device_t	dev = arg1;
	struct acpi_ibm_softc *sc = device_get_softc(dev);
	int		error = 0;

	error = sysctl_handle_int(oidp, &sc->ibm_enable, 0, req);

	if (error || !req->newptr)
		return error;

	if (sc->ibm_enable)
		sc->ibm_enable = 1;
	else
		sc->ibm_enable = 0;
	acpi_SetInteger(acpi_get_handle(dev), IBM_NAME_ENABLE, sc->ibm_enable);

	return 0;
}

static int 
sysctl_acpi_ibm_wireless_handler(SYSCTL_HANDLER_ARGS)
{
	device_t	dev = arg1;
	struct acpi_ibm_softc *sc = device_get_softc(dev);
	int		error = 0,	val, oldval, mask;
	if (sc->oid_bluetooth == oidp) {
		mask = 2;
	} else if (sc->oid_wlan == oidp) {
		mask = 4;
	} else {
		printf("WARNING: wrong handler invoked\n");
		return ENOENT;
	}

	acpi_GetInteger(acpi_get_handle(dev), IBM_NAME_GET_WIRELESS, &oldval);
	val = !((oldval & mask) == 0);
	error = sysctl_handle_int(oidp, &val, 0, req);

	if (error || !req->newptr)
		return error;
	oldval &= (~mask);
	if (val)
		oldval |= mask;
	acpi_SetInteger(acpi_get_handle(dev), IBM_NAME_SET_WIRELESS, oldval);
	return 0;


}
