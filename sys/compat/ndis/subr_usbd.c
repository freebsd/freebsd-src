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
#include <sys/socket.h>
#include <machine/bus.h>
#include <sys/bus.h>

#include <sys/queue.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <legacy/dev/usb/usb.h>
#include <legacy/dev/usb/usbdi.h>
#include <legacy/dev/usb/usbdi_util.h>
#include <legacy/dev/usb/usbdivar.h>
#include <legacy/dev/usb/usb_quirks.h>
#include "usbdevs.h"

#include <compat/ndis/pe_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/usbd_var.h>
#include <dev/if_ndis/if_ndisvar.h>

static driver_object usbd_driver;

static int32_t		 usbd_func_bulkintr(irp *);
static int32_t		 usbd_func_vendorclass(irp *);
static int32_t		 usbd_func_selconf(irp *);
static int32_t		 usbd_func_getdesc(irp *);
static usbd_status	 usbd_get_desc_ndis(usbd_device_handle, int, int, int,
			    void *, int *);
static union usbd_urb	*usbd_geturb(irp *);
static usbd_status	 usbd_init_ndispipe(irp *, usb_endpoint_descriptor_t *);
static usbd_xfer_handle	 usbd_init_ndisxfer(irp *, usb_endpoint_descriptor_t *,
			    void *, uint32_t);
static int32_t		 usbd_iodispatch(device_object *, irp *);
static int32_t		 usbd_ioinvalid(device_object *, irp *);
static int32_t		 usbd_pnp(device_object *, irp *);
static int32_t		 usbd_power(device_object *, irp *);
static void		 usbd_irpcancel(device_object *, irp *);
static void		 usbd_irpcancel_cb(void *);
static int32_t		 usbd_submit_urb(irp *);
static int32_t		 usbd_urb2nt(int32_t);
static void		 usbd_xfereof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		 usbd_xferadd(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		 usbd_xfertask(device_object *, void *);
static void		 dummy(void);

static union usbd_urb	*USBD_CreateConfigurationRequestEx(
			    usb_config_descriptor_t *,
			    struct usbd_interface_list_entry *);
static union usbd_urb	*USBD_CreateConfigurationRequest(
			    usb_config_descriptor_t *,
			    uint16_t *);
static void		 USBD_GetUSBDIVersion(usbd_version_info *);
static usb_interface_descriptor_t *USBD_ParseConfigurationDescriptorEx(
			    usb_config_descriptor_t *, void *, int32_t, int32_t,
			    int32_t, int32_t, int32_t);
static usb_interface_descriptor_t *USBD_ParseConfigurationDescriptor(
		    usb_config_descriptor_t *, uint8_t, uint8_t);

/*
 * We need to wrap these functions because these need `context switch' from
 * Windows to UNIX before it's called.
 */
static funcptr usbd_iodispatch_wrap;
static funcptr usbd_ioinvalid_wrap;
static funcptr usbd_pnp_wrap;
static funcptr usbd_power_wrap;
static funcptr usbd_irpcancel_wrap;
static funcptr usbd_xfertask_wrap;

int
usbd_libinit(void)
{
	image_patch_table	*patch;
	int i;

	patch = usbd_functbl;
	while (patch->ipt_func != NULL) {
		windrv_wrap((funcptr)patch->ipt_func,
		    (funcptr *)&patch->ipt_wrap,
		    patch->ipt_argcnt, patch->ipt_ftype);
		patch++;
	}

	windrv_wrap((funcptr)usbd_ioinvalid,
	    (funcptr *)&usbd_ioinvalid_wrap, 2, WINDRV_WRAP_STDCALL);
	windrv_wrap((funcptr)usbd_iodispatch,
	    (funcptr *)&usbd_iodispatch_wrap, 2, WINDRV_WRAP_STDCALL);
	windrv_wrap((funcptr)usbd_pnp,
	    (funcptr *)&usbd_pnp_wrap, 2, WINDRV_WRAP_STDCALL);
	windrv_wrap((funcptr)usbd_power,
	    (funcptr *)&usbd_power_wrap, 2, WINDRV_WRAP_STDCALL);
	windrv_wrap((funcptr)usbd_irpcancel,
	    (funcptr *)&usbd_irpcancel_wrap, 2, WINDRV_WRAP_STDCALL);
	windrv_wrap((funcptr)usbd_xfertask,
	    (funcptr *)&usbd_xfertask_wrap, 2, WINDRV_WRAP_STDCALL);

	/* Create a fake USB driver instance. */

	windrv_bus_attach(&usbd_driver, "USB Bus");

	/* Set up our dipatch routine. */
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
                usbd_driver.dro_dispatch[i] =
			(driver_dispatch)usbd_ioinvalid_wrap;

	usbd_driver.dro_dispatch[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
	    (driver_dispatch)usbd_iodispatch_wrap;
	usbd_driver.dro_dispatch[IRP_MJ_DEVICE_CONTROL] =
	    (driver_dispatch)usbd_iodispatch_wrap;
	usbd_driver.dro_dispatch[IRP_MJ_POWER] =
	    (driver_dispatch)usbd_power_wrap;
	usbd_driver.dro_dispatch[IRP_MJ_PNP] =
	    (driver_dispatch)usbd_pnp_wrap;

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

	windrv_unwrap(usbd_ioinvalid_wrap);
	windrv_unwrap(usbd_iodispatch_wrap);
	windrv_unwrap(usbd_pnp_wrap);
	windrv_unwrap(usbd_power_wrap);
	windrv_unwrap(usbd_irpcancel_wrap);
	windrv_unwrap(usbd_xfertask_wrap);

	free(usbd_driver.dro_drivername.us_buf, M_DEVBUF);

	return(0);
}

static int32_t
usbd_iodispatch(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	device_t dev = dobj->do_devext;
	int32_t status;
	struct io_stack_location *irp_sl;

	irp_sl = IoGetCurrentIrpStackLocation(ip);
	switch (irp_sl->isl_parameters.isl_ioctl.isl_iocode) {
	case IOCTL_INTERNAL_USB_SUBMIT_URB:
		IRP_NDIS_DEV(ip) = dev;

		status = usbd_submit_urb(ip);
		break;
	default:
		device_printf(dev, "ioctl 0x%x isn't supported\n",
		    irp_sl->isl_parameters.isl_ioctl.isl_iocode);
		status = USBD_STATUS_NOT_SUPPORTED;
		break;
	}

	if (status == USBD_STATUS_PENDING)
		return (STATUS_PENDING);

	ip->irp_iostat.isb_status = usbd_urb2nt(status);
	if (status != USBD_STATUS_SUCCESS)
		ip->irp_iostat.isb_info = 0;
	return (ip->irp_iostat.isb_status);
}

static int32_t
usbd_ioinvalid(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	device_t dev = dobj->do_devext;
	struct io_stack_location *irp_sl;

	irp_sl = IoGetCurrentIrpStackLocation(ip);
	device_printf(dev, "invalid I/O dispatch %d:%d\n", irp_sl->isl_major,
	    irp_sl->isl_minor);

	ip->irp_iostat.isb_status = STATUS_FAILURE;
	ip->irp_iostat.isb_info = 0;

	IoCompleteRequest(ip, IO_NO_INCREMENT);

	return (STATUS_FAILURE);
}

static int32_t
usbd_pnp(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	device_t dev = dobj->do_devext;
	struct io_stack_location *irp_sl;

	irp_sl = IoGetCurrentIrpStackLocation(ip);
	device_printf(dev, "%s: unsupported I/O dispatch %d:%d\n",
	    __func__, irp_sl->isl_major, irp_sl->isl_minor);

	ip->irp_iostat.isb_status = STATUS_FAILURE;
	ip->irp_iostat.isb_info = 0;

	IoCompleteRequest(ip, IO_NO_INCREMENT);

	return (STATUS_FAILURE);
}

static int32_t
usbd_power(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	device_t dev = dobj->do_devext;
	struct io_stack_location *irp_sl;

	irp_sl = IoGetCurrentIrpStackLocation(ip);
	device_printf(dev, "%s: unsupported I/O dispatch %d:%d\n",
	    __func__, irp_sl->isl_major, irp_sl->isl_minor);

	ip->irp_iostat.isb_status = STATUS_FAILURE;
	ip->irp_iostat.isb_info = 0;

	IoCompleteRequest(ip, IO_NO_INCREMENT);

	return (STATUS_FAILURE);
}

/* Convert USBD_STATUS to NTSTATUS  */
static int32_t
usbd_urb2nt(status)
	int32_t			status;
{

	switch (status) {
	case USBD_STATUS_SUCCESS:
		return (STATUS_SUCCESS);
	case USBD_STATUS_DEVICE_GONE:
		return (STATUS_DEVICE_NOT_CONNECTED);
	case USBD_STATUS_PENDING:
		return (STATUS_PENDING);
	case USBD_STATUS_NOT_SUPPORTED:
		return (STATUS_NOT_IMPLEMENTED);
	case USBD_STATUS_NO_MEMORY:
		return (STATUS_NO_MEMORY);
	case USBD_STATUS_REQUEST_FAILED:
		return (STATUS_NOT_SUPPORTED);
	case USBD_STATUS_CANCELED:
		return (STATUS_CANCELLED);
	default:
		break;
	}

	return (STATUS_FAILURE);
}

/* Convert FreeBSD's usbd_status to USBD_STATUS  */
static int32_t
usbd_usb2urb(int status)
{

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		return (USBD_STATUS_SUCCESS);
	case USBD_IN_PROGRESS:
		return (USBD_STATUS_PENDING);
	case USBD_TIMEOUT:
		return (USBD_STATUS_TIMEOUT);
	case USBD_SHORT_XFER:
		return (USBD_STATUS_ERROR_SHORT_TRANSFER);
	case USBD_IOERROR:
		return (USBD_STATUS_XACT_ERROR);
	case USBD_NOMEM:
		return (USBD_STATUS_NO_MEMORY);
	case USBD_INVAL:
		return (USBD_STATUS_REQUEST_FAILED);
	case USBD_NOT_STARTED:
	case USBD_TOO_DEEP:
	case USBD_NO_POWER:
		return (USBD_STATUS_DEVICE_GONE);
	case USBD_CANCELLED:
		return (USBD_STATUS_CANCELED);
	default:
		break;
	}
	
	return (USBD_STATUS_NOT_SUPPORTED);
}

static union usbd_urb *
usbd_geturb(ip)
	irp			*ip;
{
	struct io_stack_location *irp_sl;

	irp_sl = IoGetCurrentIrpStackLocation(ip);

	return (irp_sl->isl_parameters.isl_others.isl_arg1);
}

static int32_t
usbd_submit_urb(ip)
	irp			*ip;
{
	device_t dev = IRP_NDIS_DEV(ip);
	int32_t status;
	union usbd_urb *urb;

	urb = usbd_geturb(ip);
	/*
	 * In a case of URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER,
	 * USBD_URB_STATUS(urb) would be set at callback functions like
	 * usbd_intr() or usbd_xfereof().
	 */
	switch (urb->uu_hdr.uuh_func) {
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		status = usbd_func_bulkintr(ip);
		if (status != USBD_STATUS_SUCCESS &&
		    status != USBD_STATUS_PENDING)
			USBD_URB_STATUS(urb) = status;
		break;
	case URB_FUNCTION_VENDOR_DEVICE:
	case URB_FUNCTION_VENDOR_INTERFACE:
	case URB_FUNCTION_VENDOR_ENDPOINT:
	case URB_FUNCTION_VENDOR_OTHER:
	case URB_FUNCTION_CLASS_DEVICE:
	case URB_FUNCTION_CLASS_INTERFACE:
	case URB_FUNCTION_CLASS_ENDPOINT:
	case URB_FUNCTION_CLASS_OTHER:
		status = usbd_func_vendorclass(ip);
		USBD_URB_STATUS(urb) = status;
		break;
	case URB_FUNCTION_SELECT_CONFIGURATION:
		status = usbd_func_selconf(ip);
		USBD_URB_STATUS(urb) = status;
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		status = usbd_func_getdesc(ip);
		USBD_URB_STATUS(urb) = status;
		break;
	default:
		device_printf(dev, "func 0x%x isn't supported\n",
		    urb->uu_hdr.uuh_func);
		USBD_URB_STATUS(urb) = status = USBD_STATUS_NOT_SUPPORTED;
		break;
	}

	return (status);
}

static int32_t
usbd_func_getdesc(ip)
	irp			*ip;
{
	device_t dev = IRP_NDIS_DEV(ip);
	int actlen, i;
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usbd_urb_control_descriptor_request *ctldesc;
	uint32_t len;
	union usbd_urb *urb;
	usb_config_descriptor_t cd, *cdp;
	usbd_status status;

	mtx_lock(&Giant);

	urb = usbd_geturb(ip);
	ctldesc = &urb->uu_ctldesc;
	if (ctldesc->ucd_desctype == UDESC_CONFIG) {
		/* Get the short config descriptor. */
		status = usbd_get_config_desc(uaa->device, ctldesc->ucd_idx,
		    &cd);
		if (status != USBD_NORMAL_COMPLETION) {
			ctldesc->ucd_trans_buflen = 0;
			mtx_unlock(&Giant);
			return usbd_usb2urb(status);
		}
		/* Get the full descriptor.  Try a few times for slow devices. */
		len = MIN(ctldesc->ucd_trans_buflen, UGETW(cd.wTotalLength));
		for (i = 0; i < 3; i++) {
			status = usbd_get_desc_ndis(uaa->device,
			    ctldesc->ucd_desctype, ctldesc->ucd_idx,
			    len, ctldesc->ucd_trans_buf, &actlen);
			if (status == USBD_NORMAL_COMPLETION)
				break;
			usbd_delay_ms(uaa->device, 200);
		}
		if (status != USBD_NORMAL_COMPLETION) {
			ctldesc->ucd_trans_buflen = 0;
			mtx_unlock(&Giant);
			return usbd_usb2urb(status);
		}

		cdp = (usb_config_descriptor_t *)ctldesc->ucd_trans_buf;
		if (cdp->bDescriptorType != UDESC_CONFIG) {
			device_printf(dev, "bad desc %d\n",
			    cdp->bDescriptorType);
			status = USBD_INVAL;
		}
	} else if (ctldesc->ucd_desctype == UDESC_STRING) {
		/* Try a few times for slow devices.  */
		for (i = 0; i < 3; i++) {
			status = usbd_get_string_desc(uaa->device,
			    (UDESC_STRING << 8) + ctldesc->ucd_idx,
			    ctldesc->ucd_langid, ctldesc->ucd_trans_buf,
			    &actlen);
			if (actlen > ctldesc->ucd_trans_buflen)
				panic("small string buffer for UDESC_STRING");
			if (status == USBD_NORMAL_COMPLETION)
				break;
			usbd_delay_ms(uaa->device, 200);
		}
	} else
		status = usbd_get_desc_ndis(uaa->device, ctldesc->ucd_desctype,
		    ctldesc->ucd_idx, ctldesc->ucd_trans_buflen,
		    ctldesc->ucd_trans_buf, &actlen);

	if (status != USBD_NORMAL_COMPLETION) {
		ctldesc->ucd_trans_buflen = 0;
		mtx_unlock(&Giant);
		return usbd_usb2urb(status);
	}

	ctldesc->ucd_trans_buflen = actlen;
	ip->irp_iostat.isb_info = actlen;

	mtx_unlock(&Giant);

	return (USBD_STATUS_SUCCESS);
}

/*
 * FIXME: at USB1, not USB2, framework, there's no a interface to get `actlen'.
 * However, we need it!!!
 */
static usbd_status
usbd_get_desc_ndis(usbd_device_handle dev, int type, int index, int len,
    void *desc, int *actlen)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, type, index);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return usbd_do_request_flags_pipe(dev, dev->default_pipe, &req, desc,
	    0, actlen, USBD_DEFAULT_TIMEOUT);
}

static int32_t
usbd_func_selconf(ip)
	irp			*ip;
{
	device_t dev = IRP_NDIS_DEV(ip);
	int i, j;
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usbd_interface_information *intf;
	struct usbd_pipe_information *pipe;
	struct usbd_urb_select_configuration *selconf;
	union usbd_urb *urb;
	usb_config_descriptor_t *conf;
	usb_endpoint_descriptor_t *edesc;
	usbd_device_handle udev = uaa->device;
	usbd_interface_handle iface;
	usbd_status ret;

	urb = usbd_geturb(ip);

	selconf = &urb->uu_selconf;
	conf = selconf->usc_conf;
	if (conf == NULL) {
		device_printf(dev, "select configuration is NULL\n");
		return usbd_usb2urb(USBD_NORMAL_COMPLETION);
	}

	if (conf->bConfigurationValue > NDISUSB_CONFIG_NO)
		device_printf(dev, "warning: config_no is larger than default");

	intf = &selconf->usc_intf;
	for (i = 0; i < conf->bNumInterface && intf->uii_len > 0; i++) {
		ret = usbd_device2interface_handle(uaa->device,
		    intf->uii_intfnum, &iface);
		if (ret != USBD_NORMAL_COMPLETION) {
			device_printf(dev,
			    "getting interface handle failed: %s\n",
			    usbd_errstr(ret));
			return usbd_usb2urb(ret);
		}
		
		ret = usbd_set_interface(iface, intf->uii_altset);
		if (ret != USBD_NORMAL_COMPLETION && ret != USBD_IN_USE) {
			device_printf(dev,
			    "setting alternate interface failed: %s\n",
			    usbd_errstr(ret));
			return usbd_usb2urb(ret);
		}
		
		for (j = 0; j < iface->idesc->bNumEndpoints; j++) {
			if (j >= intf->uii_numeps) {
				device_printf(dev,
				    "endpoint %d and above are ignored",
				    intf->uii_numeps);
				break;
			}
			edesc = iface->endpoints[j].edesc;
			pipe = &intf->uii_pipes[j];
			pipe->upi_handle = edesc;
			pipe->upi_epaddr = edesc->bEndpointAddress;
			pipe->upi_maxpktsize = UGETW(edesc->wMaxPacketSize);
			pipe->upi_type = UE_GET_XFERTYPE(edesc->bmAttributes);
			if (pipe->upi_type != UE_INTERRUPT)
				continue;

			/* XXX we're following linux USB's interval policy.  */
			if (udev->speed == USB_SPEED_LOW)
				pipe->upi_interval = edesc->bInterval + 5;
			else if (udev->speed == USB_SPEED_FULL)
				pipe->upi_interval = edesc->bInterval;
			else {
				int k0 = 0, k1 = 1;
				do {
					k1 = k1 * 2;
					k0 = k0 + 1;
				} while (k1 < edesc->bInterval);
				pipe->upi_interval = k0;
			}
		}

		intf = (struct usbd_interface_information *)(((char *)intf) +
		    intf->uii_len);
	}

	return USBD_STATUS_SUCCESS;
}

static int32_t
usbd_func_vendorclass(ip)
	irp			*ip;
{
	device_t dev = IRP_NDIS_DEV(ip);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usbd_urb_vendor_or_class_request *vcreq;
	uint8_t type = 0;
	union usbd_urb *urb;
	usb_device_request_t req;
	usbd_status status;

	urb = usbd_geturb(ip);
	vcreq = &urb->uu_vcreq;

	switch (urb->uu_hdr.uuh_func) {
	case URB_FUNCTION_CLASS_DEVICE:
		type = UT_CLASS | UT_DEVICE;
		break;
	case URB_FUNCTION_CLASS_INTERFACE:
		type = UT_CLASS | UT_INTERFACE;
		break;
	case URB_FUNCTION_CLASS_OTHER:
		type = UT_CLASS | UT_OTHER;
		break;
	case URB_FUNCTION_CLASS_ENDPOINT:
		type = UT_CLASS | UT_ENDPOINT;
		break;
	case URB_FUNCTION_VENDOR_DEVICE:
		type = UT_VENDOR | UT_DEVICE;
		break;
	case URB_FUNCTION_VENDOR_INTERFACE:
		type = UT_VENDOR | UT_INTERFACE;
		break;
	case URB_FUNCTION_VENDOR_OTHER:
		type = UT_VENDOR | UT_OTHER;
		break;
	case URB_FUNCTION_VENDOR_ENDPOINT:
		type = UT_VENDOR | UT_ENDPOINT;
		break;
	default:
		/* never reach.  */
		break;
	}

	type |= (vcreq->uvc_trans_flags & USBD_TRANSFER_DIRECTION_IN) ?
	    UT_READ : UT_WRITE;
	type |= vcreq->uvc_reserved1;

	req.bmRequestType = type;
	req.bRequest = vcreq->uvc_req;
	USETW(req.wIndex, vcreq->uvc_idx);
	USETW(req.wValue, vcreq->uvc_value);
	USETW(req.wLength, vcreq->uvc_trans_buflen);

	if (vcreq->uvc_trans_flags & USBD_TRANSFER_DIRECTION_IN) {
		mtx_lock(&Giant);
		status = usbd_do_request(uaa->device, &req,
		    vcreq->uvc_trans_buf);
		mtx_unlock(&Giant);
	} else
		status = usbd_do_request_async(uaa->device, &req,
		    vcreq->uvc_trans_buf);

	return usbd_usb2urb(status);
}

static usbd_status
usbd_init_ndispipe(ip, ep)
	irp			*ip;
	usb_endpoint_descriptor_t *ep;
{
	device_t dev = IRP_NDIS_DEV(ip);
	struct ndis_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usbd_interface_handle iface;
	usbd_status status;

	status = usbd_device2interface_handle(uaa->device, NDISUSB_IFACE_INDEX,
	    &iface);
	if (status != USBD_NORMAL_COMPLETION) {
		device_printf(dev, "could not get interface handle\n");
		return (status);
	}

	switch (UE_GET_XFERTYPE(ep->bmAttributes)) {
	case UE_BULK:
		if (UE_GET_DIR(ep->bEndpointAddress) == UE_DIR_IN) {
			/* RX (bulk IN)  */
			if (sc->ndisusb_ep[NDISUSB_ENDPT_BIN] != NULL)
				return (USBD_NORMAL_COMPLETION);

			status = usbd_open_pipe(iface, ep->bEndpointAddress,
			    USBD_EXCLUSIVE_USE,
			    &sc->ndisusb_ep[NDISUSB_ENDPT_BIN]);
			break;
		}

		/* TX (bulk OUT)  */
		if (sc->ndisusb_ep[NDISUSB_ENDPT_BOUT] != NULL)
			return (USBD_NORMAL_COMPLETION);

		status = usbd_open_pipe(iface, ep->bEndpointAddress,
		    USBD_EXCLUSIVE_USE, &sc->ndisusb_ep[NDISUSB_ENDPT_BOUT]);
		break;
	case UE_INTERRUPT:
		if (UE_GET_DIR(ep->bEndpointAddress) == UE_DIR_IN) {
			/* Interrupt IN.  */
			if (sc->ndisusb_ep[NDISUSB_ENDPT_IIN] != NULL)
				return (USBD_NORMAL_COMPLETION);

			status = usbd_open_pipe(iface, ep->bEndpointAddress,
			    USBD_EXCLUSIVE_USE,
			    &sc->ndisusb_ep[NDISUSB_ENDPT_IIN]);
			break;
		}

		/* Interrupt OUT.  */
		if (sc->ndisusb_ep[NDISUSB_ENDPT_IOUT] != NULL)
			return (USBD_NORMAL_COMPLETION);

		status = usbd_open_pipe(iface, ep->bEndpointAddress,
		    USBD_EXCLUSIVE_USE, &sc->ndisusb_ep[NDISUSB_ENDPT_IOUT]);
		break;
	default:
		device_printf(dev, "can't handle xfertype 0x%x\n",
		    UE_GET_XFERTYPE(ep->bmAttributes));
		return (USBD_INVAL);
	}

	if (status != USBD_NORMAL_COMPLETION)
		device_printf(dev,  "open pipe failed: (0x%x) %s\n",
		    ep->bEndpointAddress, usbd_errstr(status));

	return (status);
}

static void
usbd_irpcancel_cb(priv)
	void			*priv;
{
	struct ndisusb_cancel *nc = priv;
	struct ndis_softc *sc = device_get_softc(nc->dev);
	usbd_status status;
	usbd_xfer_handle xfer = nc->xfer;

	if (sc->ndisusb_status & NDISUSB_STATUS_DETACH)
		goto exit;

	status = usbd_abort_pipe(xfer->pipe);
	if (status != USBD_NORMAL_COMPLETION)
		device_printf(nc->dev, "can't be canceld");
exit:
	free(nc, M_USBDEV);
}

static void
usbd_irpcancel(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	device_t dev = IRP_NDIS_DEV(ip);
	struct ndisusb_cancel *nc;
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (IRP_NDISUSB_XFER(ip) == NULL) {
		ip->irp_cancel = TRUE;
		IoReleaseCancelSpinLock(ip->irp_cancelirql);
		return;
	}

	/*
	 * XXX Since we're under DISPATCH_LEVEL during calling usbd_irpcancel(),
	 * we can't sleep at all.  However, currently FreeBSD's USB stack
	 * requires a sleep to abort a transfer.  It's inevitable! so it causes
	 * serveral fatal problems (e.g. kernel hangups or crashes).  I think
	 * that there are no ways to make this reliable.  In this implementation,
	 * I used usb_add_task() but it's not a perfect method to solve this
	 * because of as follows: NDIS drivers would expect that IRP's
	 * completely canceld when usbd_irpcancel() is returned but we need
	 * a sleep to do it.  During canceling XFERs, usbd_intr() would be
	 * called with a status, USBD_CANCELLED.
	 */
	nc = malloc(sizeof(struct ndisusb_cancel), M_USBDEV, M_NOWAIT | M_ZERO);
	if (nc == NULL) {
		ip->irp_cancel = FALSE;
		IoReleaseCancelSpinLock(ip->irp_cancelirql);
		return;
	}

	nc->dev = dev;
	nc->xfer = IRP_NDISUSB_XFER(ip);
	usb_init_task(&nc->task, usbd_irpcancel_cb, nc);

	IRP_NDISUSB_XFER(ip) = NULL;
	usb_add_task(uaa->device, &nc->task, USB_TASKQ_DRIVER);

	ip->irp_cancel = TRUE;
	IoReleaseCancelSpinLock(ip->irp_cancelirql);
}

static usbd_xfer_handle
usbd_init_ndisxfer(ip, ep, buf, buflen)
	irp			*ip;
	usb_endpoint_descriptor_t *ep;
	void			*buf;
	uint32_t		buflen;
{
	device_t dev = IRP_NDIS_DEV(ip);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usbd_xfer_handle xfer;
	
	xfer = usbd_alloc_xfer(uaa->device);
	if (xfer == NULL)
		return (NULL);

	if (buf != NULL && MmIsAddressValid(buf) == FALSE && buflen > 0) {
		xfer->buffer = usbd_alloc_buffer(xfer, buflen);
		if (xfer->buffer == NULL)
			return (NULL);

		if (UE_GET_DIR(ep->bEndpointAddress) == UE_DIR_OUT)
			memcpy(xfer->buffer, buf, buflen);
	} else
		xfer->buffer = buf;

	xfer->length = buflen;

	IoAcquireCancelSpinLock(&ip->irp_cancelirql);
	IRP_NDISUSB_XFER(ip) = xfer;
	ip->irp_cancelfunc = (cancel_func)usbd_irpcancel_wrap;
	IoReleaseCancelSpinLock(ip->irp_cancelirql);

	return (xfer);
}

static void
usbd_xferadd(xfer, priv, status)
	usbd_xfer_handle xfer;
	usbd_private_handle priv;
	usbd_status status;
{
	irp *ip = priv;
	device_t dev = IRP_NDIS_DEV(ip);
	struct ndis_softc *sc = device_get_softc(dev);
	struct ndisusb_xfer *nx;
	uint8_t irql;

	nx = malloc(sizeof(struct ndisusb_xfer), M_USBDEV, M_NOWAIT | M_ZERO);
	if (nx == NULL) {
		device_printf(dev, "out of memory");
		return;
	}
	nx->nx_xfer = xfer;
	nx->nx_priv = priv;
	nx->nx_status = status;

	KeAcquireSpinLock(&sc->ndisusb_xferlock, &irql);
	InsertTailList((&sc->ndisusb_xferlist), (&nx->nx_xferlist));
	KeReleaseSpinLock(&sc->ndisusb_xferlock, irql);

	IoQueueWorkItem(sc->ndisusb_xferitem,
	    (io_workitem_func)usbd_xfertask_wrap, WORKQUEUE_CRITICAL, sc);
}

static void
usbd_xfereof(xfer, priv, status)
	usbd_xfer_handle xfer;
	usbd_private_handle priv;
	usbd_status status;
{

	usbd_xferadd(xfer, priv, status);
}

static void
usbd_xfertask(dobj, arg)
	device_object		*dobj;
	void			*arg;
{
	int error;
	irp *ip;
	device_t dev;
	list_entry *l;
	struct ndis_softc *sc = arg;
	struct ndisusb_xfer *nx;
	struct usbd_urb_bulk_or_intr_transfer *ubi;
	uint8_t irql;
	union usbd_urb *urb;
	usbd_private_handle priv;
	usbd_status status;
	usbd_xfer_handle xfer;

	dev = sc->ndis_dev;

	if (IsListEmpty(&sc->ndisusb_xferlist))
		return;

	KeAcquireSpinLock(&sc->ndisusb_xferlock, &irql);
	l = sc->ndisusb_xferlist.nle_flink;
	while (l != &sc->ndisusb_xferlist) {
		nx = CONTAINING_RECORD(l, struct ndisusb_xfer, nx_xferlist);
		xfer = nx->nx_xfer;
		priv = nx->nx_priv;
		status = nx->nx_status;
		error = 0;
		ip = priv;

		if (status != USBD_NORMAL_COMPLETION) {
			if (status == USBD_NOT_STARTED) {
				error = 1;
				goto next;
			}
			if (status == USBD_STALLED)
				usbd_clear_endpoint_stall_async(xfer->pipe);
			/*
			 * NB: just for notice.  We must handle error cases also
			 * because if we just return without notifying to the
			 * NDIS driver the driver never knows about that there
			 * was a error.  This can cause a lot of problems like
			 * system hangs.
			 */
			device_printf(dev, "usb xfer warning (%s)\n",
			    usbd_errstr(status));
		}
		
		urb = usbd_geturb(ip);
		
		KASSERT(urb->uu_hdr.uuh_func ==
		    URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER,
		    ("function(%d) isn't for bulk or interrupt",
			urb->uu_hdr.uuh_func));
		
		IoAcquireCancelSpinLock(&ip->irp_cancelirql);
		
		ip->irp_cancelfunc = NULL;
		IRP_NDISUSB_XFER(ip) = NULL;
		
		switch (status) {
		case USBD_NORMAL_COMPLETION:
			ubi = &urb->uu_bulkintr;
			ubi->ubi_trans_buflen = xfer->actlen;
			if (ubi->ubi_trans_flags & USBD_TRANSFER_DIRECTION_IN)
				memcpy(ubi->ubi_trans_buf, xfer->buffer,
				    xfer->actlen);
			
			ip->irp_iostat.isb_info = xfer->actlen;
			ip->irp_iostat.isb_status = STATUS_SUCCESS;
			USBD_URB_STATUS(urb) = USBD_STATUS_SUCCESS;
			break;
		case USBD_CANCELLED:
			ip->irp_iostat.isb_info = 0;
			ip->irp_iostat.isb_status = STATUS_CANCELLED;
			USBD_URB_STATUS(urb) = USBD_STATUS_CANCELED;
			break;
		default:
			ip->irp_iostat.isb_info = 0;
			USBD_URB_STATUS(urb) = usbd_usb2urb(status);
			ip->irp_iostat.isb_status =
			    usbd_urb2nt(USBD_URB_STATUS(urb));
			break;
		}
		
		IoReleaseCancelSpinLock(ip->irp_cancelirql);
next:
		l = l->nle_flink;
		RemoveEntryList(&nx->nx_xferlist);
		usbd_free_xfer(nx->nx_xfer);
		free(nx, M_USBDEV);
		if (error)
			continue;
		/* NB: call after cleaning  */
		IoCompleteRequest(ip, IO_NO_INCREMENT);
	}
	KeReleaseSpinLock(&sc->ndisusb_xferlock, irql);
}

static int32_t
usbd_func_bulkintr(ip)
	irp			*ip;
{
	device_t dev = IRP_NDIS_DEV(ip);
	struct ndis_softc *sc = device_get_softc(dev);
	struct usbd_urb_bulk_or_intr_transfer *ubi;
	union usbd_urb *urb;
	usb_endpoint_descriptor_t *ep;
	usbd_status status;
	usbd_xfer_handle xfer;

	urb = usbd_geturb(ip);
	ubi = &urb->uu_bulkintr;
	ep = ubi->ubi_epdesc;
	if (ep == NULL)
		return (USBD_STATUS_INVALID_PIPE_HANDLE);

	status = usbd_init_ndispipe(ip, ep);
	if (status != USBD_NORMAL_COMPLETION)
		return usbd_usb2urb(status);

	xfer = usbd_init_ndisxfer(ip, ep, ubi->ubi_trans_buf,
	    ubi->ubi_trans_buflen);
	if (xfer == NULL) {
		device_printf(IRP_NDIS_DEV(ip), "can't allocate xfer\n");
		return (USBD_STATUS_NO_MEMORY);
	}

	if (UE_GET_DIR(ep->bEndpointAddress) == UE_DIR_IN) {
		xfer->flags |= USBD_SHORT_XFER_OK;
		if (!(ubi->ubi_trans_flags & USBD_SHORT_TRANSFER_OK))
			xfer->flags &= ~USBD_SHORT_XFER_OK;
	}

	if (UE_GET_XFERTYPE(ep->bmAttributes) == UE_BULK) {
		if (UE_GET_DIR(ep->bEndpointAddress) == UE_DIR_IN)
			/* RX (bulk IN)  */
			usbd_setup_xfer(xfer, sc->ndisusb_ep[NDISUSB_ENDPT_BIN],
			    ip, xfer->buffer, xfer->length, xfer->flags,
			    USBD_NO_TIMEOUT, usbd_xfereof);
		else {
			/* TX (bulk OUT)  */
			xfer->flags |= USBD_NO_COPY;
			
			usbd_setup_xfer(xfer, sc->ndisusb_ep[NDISUSB_ENDPT_BOUT],
			    ip, xfer->buffer, xfer->length, xfer->flags,
			    NDISUSB_TX_TIMEOUT, usbd_xfereof);
		}
	} else {
		if (UE_GET_DIR(ep->bEndpointAddress) == UE_DIR_IN)
			/* Interrupt IN  */
			usbd_setup_xfer(xfer, sc->ndisusb_ep[NDISUSB_ENDPT_IIN],
			    ip, xfer->buffer, xfer->length, xfer->flags,
			    USBD_NO_TIMEOUT, usbd_xfereof);
		else
			/* Interrupt OUT  */
			usbd_setup_xfer(xfer, sc->ndisusb_ep[NDISUSB_ENDPT_IOUT],
			    ip, xfer->buffer, xfer->length, xfer->flags,
			    NDISUSB_INTR_TIMEOUT, usbd_xfereof);
	}

	/* we've done to setup xfer.  Let's transfer it.  */
	ip->irp_iostat.isb_status = STATUS_PENDING;
	ip->irp_iostat.isb_info = 0;
	USBD_URB_STATUS(urb) = USBD_STATUS_PENDING;
	IoMarkIrpPending(ip);

	status = usbd_transfer(xfer);
	if (status == USBD_IN_PROGRESS)
		return (USBD_STATUS_PENDING);

	usbd_free_xfer(xfer);
	IRP_NDISUSB_XFER(ip) = NULL;
	IoUnmarkIrpPending(ip);
	USBD_URB_STATUS(urb) = usbd_usb2urb(status);

	return USBD_URB_STATUS(urb);
}

static union usbd_urb *
USBD_CreateConfigurationRequest(conf, len)
        usb_config_descriptor_t *conf;
	uint16_t *len;
{
        struct usbd_interface_list_entry list[2];
        union usbd_urb *urb;

	bzero(list, sizeof(struct usbd_interface_list_entry) * 2);
        list[0].uil_intfdesc = USBD_ParseConfigurationDescriptorEx(conf, conf,
	    -1, -1, -1, -1, -1);
        urb = USBD_CreateConfigurationRequestEx(conf, list);
        if (urb == NULL)
                return NULL;

        *len = urb->uu_selconf.usc_hdr.uuh_len;
        return urb;
}

static union usbd_urb *
USBD_CreateConfigurationRequestEx(conf, list)
	usb_config_descriptor_t *conf;
	struct usbd_interface_list_entry *list;
{
        int i, j, size;
        struct usbd_interface_information *intf;
        struct usbd_pipe_information *pipe;
        struct usbd_urb_select_configuration *selconf;
        usb_interface_descriptor_t *desc;

	for (i = 0, size = 0; i < conf->bNumInterface; i++) {
		j = list[i].uil_intfdesc->bNumEndpoints;
		size = size + sizeof(struct usbd_interface_information) +
		    sizeof(struct usbd_pipe_information) * (j - 1);
	}
	size += sizeof(struct usbd_urb_select_configuration) -
	    sizeof(struct usbd_interface_information);

        selconf = ExAllocatePoolWithTag(NonPagedPool, size, 0);
        if (selconf == NULL)
                return NULL;
        selconf->usc_hdr.uuh_func = URB_FUNCTION_SELECT_CONFIGURATION;
        selconf->usc_hdr.uuh_len = size;
        selconf->usc_handle = conf;
        selconf->usc_conf = conf;

        intf = &selconf->usc_intf;
        for (i = 0; i < conf->bNumInterface; i++) {
		if (list[i].uil_intfdesc == NULL)
			break;

                list[i].uil_intf = intf;
                desc = list[i].uil_intfdesc;

                intf->uii_len = sizeof(struct usbd_interface_information) +
		    (desc->bNumEndpoints - 1) *
		    sizeof(struct usbd_pipe_information);
                intf->uii_intfnum = desc->bInterfaceNumber;
                intf->uii_altset = desc->bAlternateSetting;
                intf->uii_intfclass = desc->bInterfaceClass;
                intf->uii_intfsubclass = desc->bInterfaceSubClass;
                intf->uii_intfproto = desc->bInterfaceProtocol;
                intf->uii_handle = desc;
                intf->uii_numeps = desc->bNumEndpoints;

                pipe = &intf->uii_pipes[0];
                for (j = 0; j < intf->uii_numeps; j++)
                        pipe[j].upi_maxtxsize =
			    USBD_DEFAULT_MAXIMUM_TRANSFER_SIZE;

                intf = (struct usbd_interface_information *)((char *)intf +
		    intf->uii_len);
        }

        return ((union usbd_urb *)selconf);
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

static usb_interface_descriptor_t *
USBD_ParseConfigurationDescriptor(usb_config_descriptor_t *conf,
	uint8_t intfnum, uint8_t altset)
{
        return USBD_ParseConfigurationDescriptorEx(conf, conf, intfnum, altset,
	    -1, -1, -1);
}

static usb_interface_descriptor_t *
USBD_ParseConfigurationDescriptorEx(conf, start, intfnum,
    altset, intfclass, intfsubclass, intfproto)
	usb_config_descriptor_t *conf;
	void *start;
	int32_t intfnum;
	int32_t altset;
	int32_t intfclass;
	int32_t intfsubclass;
	int32_t intfproto;
{
	char *pos;
	usb_interface_descriptor_t *desc;

	for (pos = start; pos < ((char *)conf + UGETW(conf->wTotalLength));
	     pos += desc->bLength) {
		desc = (usb_interface_descriptor_t *)pos;
		if (desc->bDescriptorType != UDESC_INTERFACE)
			continue;
		if (!(intfnum == -1 || desc->bInterfaceNumber == intfnum))
			continue;
		if (!(altset == -1 || desc->bAlternateSetting == altset))
			continue;
		if (!(intfclass == -1 || desc->bInterfaceClass == intfclass))
			continue;
		if (!(intfsubclass == -1 ||
		    desc->bInterfaceSubClass == intfsubclass))
			continue;
		if (!(intfproto == -1 || desc->bInterfaceProtocol == intfproto))
			continue;
		return (desc);
	}

	return (NULL);
}

static void
dummy(void)
{
	printf("USBD dummy called\n");
	return;
}

image_patch_table usbd_functbl[] = {
	IMPORT_SFUNC(USBD_CreateConfigurationRequest, 2),
	IMPORT_SFUNC(USBD_CreateConfigurationRequestEx, 2),
	IMPORT_SFUNC_MAP(_USBD_CreateConfigurationRequestEx@8,
	    USBD_CreateConfigurationRequestEx, 2),
	IMPORT_SFUNC(USBD_GetUSBDIVersion, 1),
	IMPORT_SFUNC(USBD_ParseConfigurationDescriptor, 3),
	IMPORT_SFUNC(USBD_ParseConfigurationDescriptorEx, 7),
	IMPORT_SFUNC_MAP(_USBD_ParseConfigurationDescriptorEx@28,
	    USBD_ParseConfigurationDescriptorEx, 7),

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy, NULL, 0, WINDRV_WRAP_STDCALL },

	/* End of list. */

	{ NULL, NULL, NULL }
};

MODULE_DEPEND(ndis, usb, 1, 1, 1);

