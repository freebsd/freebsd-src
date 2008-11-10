/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_devid.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_parse.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_hub.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>
#include <dev/usb2/core/usb2_msctest.h>

#include <dev/usb2/quirk/usb2_quirk.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

/* function prototypes */

static void usb2_fill_pipe_data(struct usb2_device *udev, uint8_t iface_index, struct usb2_endpoint_descriptor *edesc, struct usb2_pipe *pipe);
static void usb2_free_pipe_data(struct usb2_device *udev, uint8_t iface_index, uint8_t iface_mask);
static void usb2_free_iface_data(struct usb2_device *udev);
static void usb2_detach_device_sub(struct usb2_device *udev, device_t *ppdev, uint8_t free_subdev);
static uint8_t usb2_probe_and_attach_sub(struct usb2_device *udev, struct usb2_attach_arg *uaa);
static void usb2_init_attach_arg(struct usb2_device *udev, struct usb2_attach_arg *uaa);
static void usb2_suspend_resume_sub(struct usb2_device *udev, device_t dev, uint8_t do_suspend);
static void usb2_clear_stall_proc(struct usb2_proc_msg *_pm);
static void usb2_check_strings(struct usb2_device *udev);
static usb2_error_t usb2_fill_iface_data(struct usb2_device *udev, uint8_t iface_index, uint8_t alt_index);
static void usb2_notify_addq(const char *type, struct usb2_device *udev);
static void usb2_fifo_free_wrap(struct usb2_device *udev, uint8_t iface_index, uint8_t free_all);

/* static structures */

static const uint8_t usb2_hub_speed_combs[USB_SPEED_MAX][USB_SPEED_MAX] = {
	/* HUB *//* subdevice */
	[USB_SPEED_HIGH][USB_SPEED_HIGH] = 1,
	[USB_SPEED_HIGH][USB_SPEED_FULL] = 1,
	[USB_SPEED_HIGH][USB_SPEED_LOW] = 1,
	[USB_SPEED_FULL][USB_SPEED_FULL] = 1,
	[USB_SPEED_FULL][USB_SPEED_LOW] = 1,
	[USB_SPEED_LOW][USB_SPEED_LOW] = 1,
};

/* This variable is global to allow easy access to it: */

int	usb2_template = 0;

SYSCTL_INT(_hw_usb2, OID_AUTO, template, CTLFLAG_RW,
    &usb2_template, 0, "Selected USB device side template");


/*------------------------------------------------------------------------*
 *	usb2_get_pipe_by_addr
 *
 * This function searches for an USB pipe by endpoint address and
 * direction.
 *
 * Returns:
 * NULL: Failure
 * Else: Success
 *------------------------------------------------------------------------*/
struct usb2_pipe *
usb2_get_pipe_by_addr(struct usb2_device *udev, uint8_t ea_val)
{
	struct usb2_pipe *pipe = udev->pipes;
	struct usb2_pipe *pipe_end = udev->pipes + USB_EP_MAX;
	enum {
		EA_MASK = (UE_DIR_IN | UE_DIR_OUT | UE_ADDR),
	};

	/*
	 * According to the USB specification not all bits are used
	 * for the endpoint address. Keep defined bits only:
	 */
	ea_val &= EA_MASK;

	/*
	 * Iterate accross all the USB pipes searching for a match
	 * based on the endpoint address:
	 */
	for (; pipe != pipe_end; pipe++) {

		if (pipe->edesc == NULL) {
			continue;
		}
		/* do the mask and check the value */
		if ((pipe->edesc->bEndpointAddress & EA_MASK) == ea_val) {
			goto found;
		}
	}

	/*
	 * The default pipe is always present and is checked separately:
	 */
	if ((udev->default_pipe.edesc) &&
	    ((udev->default_pipe.edesc->bEndpointAddress & EA_MASK) == ea_val)) {
		pipe = &udev->default_pipe;
		goto found;
	}
	return (NULL);

found:
	return (pipe);
}

/*------------------------------------------------------------------------*
 *	usb2_get_pipe
 *
 * This function searches for an USB pipe based on the information
 * given by the passed "struct usb2_config" pointer.
 *
 * Return values:
 * NULL: No match.
 * Else: Pointer to "struct usb2_pipe".
 *------------------------------------------------------------------------*/
struct usb2_pipe *
usb2_get_pipe(struct usb2_device *udev, uint8_t iface_index,
    const struct usb2_config *setup)
{
	struct usb2_pipe *pipe = udev->pipes;
	struct usb2_pipe *pipe_end = udev->pipes + USB_EP_MAX;
	uint8_t index = setup->ep_index;
	uint8_t ea_mask;
	uint8_t ea_val;
	uint8_t type_mask;
	uint8_t type_val;

	DPRINTFN(10, "udev=%p iface_index=%d address=0x%x "
	    "type=0x%x dir=0x%x index=%d\n",
	    udev, iface_index, setup->endpoint,
	    setup->type, setup->direction, setup->ep_index);

	/* setup expected endpoint direction mask and value */

	if (setup->direction == UE_DIR_ANY) {
		/* match any endpoint direction */
		ea_mask = 0;
		ea_val = 0;
	} else {
		/* match the given endpoint direction */
		ea_mask = (UE_DIR_IN | UE_DIR_OUT);
		ea_val = (setup->direction & (UE_DIR_IN | UE_DIR_OUT));
	}

	/* setup expected endpoint address */

	if (setup->endpoint == UE_ADDR_ANY) {
		/* match any endpoint address */
	} else {
		/* match the given endpoint address */
		ea_mask |= UE_ADDR;
		ea_val |= (setup->endpoint & UE_ADDR);
	}

	/* setup expected endpoint type */

	if (setup->type == UE_BULK_INTR) {
		/* this will match BULK and INTERRUPT endpoints */
		type_mask = 2;
		type_val = 2;
	} else if (setup->type == UE_TYPE_ANY) {
		/* match any endpoint type */
		type_mask = 0;
		type_val = 0;
	} else {
		/* match the given endpoint type */
		type_mask = UE_XFERTYPE;
		type_val = (setup->type & UE_XFERTYPE);
	}

	/*
	 * Iterate accross all the USB pipes searching for a match
	 * based on the endpoint address. Note that we are searching
	 * the pipes from the beginning of the "udev->pipes" array.
	 */
	for (; pipe != pipe_end; pipe++) {

		if ((pipe->edesc == NULL) ||
		    (pipe->iface_index != iface_index)) {
			continue;
		}
		/* do the masks and check the values */

		if (((pipe->edesc->bEndpointAddress & ea_mask) == ea_val) &&
		    ((pipe->edesc->bmAttributes & type_mask) == type_val)) {
			if (!index--) {
				goto found;
			}
		}
	}

	/*
	 * Match against default pipe last, so that "any pipe", "any
	 * address" and "any direction" returns the first pipe of the
	 * interface. "iface_index" and "direction" is ignored:
	 */
	if ((udev->default_pipe.edesc) &&
	    ((udev->default_pipe.edesc->bEndpointAddress & ea_mask) == ea_val) &&
	    ((udev->default_pipe.edesc->bmAttributes & type_mask) == type_val) &&
	    (!index)) {
		pipe = &udev->default_pipe;
		goto found;
	}
	return (NULL);

found:
	return (pipe);
}

/*------------------------------------------------------------------------*
 *	usb2_interface_count
 *
 * This function stores the number of USB interfaces excluding
 * alternate settings, which the USB config descriptor reports into
 * the unsigned 8-bit integer pointed to by "count".
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_interface_count(struct usb2_device *udev, uint8_t *count)
{
	if (udev->cdesc == NULL) {
		*count = 0;
		return (USB_ERR_NOT_CONFIGURED);
	}
	*count = udev->cdesc->bNumInterface;
	return (USB_ERR_NORMAL_COMPLETION);
}


/*------------------------------------------------------------------------*
 *	usb2_fill_pipe_data
 *
 * This function will initialise the USB pipe structure pointed to by
 * the "pipe" argument.
 *------------------------------------------------------------------------*/
static void
usb2_fill_pipe_data(struct usb2_device *udev, uint8_t iface_index,
    struct usb2_endpoint_descriptor *edesc, struct usb2_pipe *pipe)
{
	bzero(pipe, sizeof(*pipe));

	(udev->bus->methods->pipe_init) (udev, edesc, pipe);

	if (pipe->methods == NULL) {
		/* the pipe is invalid: just return */
		return;
	}
	/* initialise USB pipe structure */
	pipe->edesc = edesc;
	pipe->iface_index = iface_index;
	TAILQ_INIT(&pipe->pipe_q.head);
	pipe->pipe_q.command = &usb2_pipe_start;

	/* clear stall, if any */
	if (udev->bus->methods->clear_stall) {
		USB_BUS_LOCK(udev->bus);
		(udev->bus->methods->clear_stall) (udev, pipe);
		USB_BUS_UNLOCK(udev->bus);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_free_pipe_data
 *
 * This function will free USB pipe data for the given interface
 * index. Hence we do not have any dynamic allocations we simply clear
 * "pipe->edesc" to indicate that the USB pipe structure can be
 * reused. The pipes belonging to the given interface should not be in
 * use when this function is called and no check is performed to
 * prevent this.
 *------------------------------------------------------------------------*/
static void
usb2_free_pipe_data(struct usb2_device *udev,
    uint8_t iface_index, uint8_t iface_mask)
{
	struct usb2_pipe *pipe = udev->pipes;
	struct usb2_pipe *pipe_end = udev->pipes + USB_EP_MAX;

	while (pipe != pipe_end) {
		if ((pipe->iface_index & iface_mask) == iface_index) {
			/* free pipe */
			pipe->edesc = NULL;
		}
		pipe++;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_fill_iface_data
 *
 * This function will fill in interface data and allocate USB pipes
 * for all the endpoints that belong to the given interface. This
 * function is typically called when setting the configuration or when
 * setting an alternate interface.
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_fill_iface_data(struct usb2_device *udev,
    uint8_t iface_index, uint8_t alt_index)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_pipe *pipe;
	struct usb2_pipe *pipe_end;
	struct usb2_interface_descriptor *id;
	struct usb2_endpoint_descriptor *ed = NULL;
	struct usb2_descriptor *desc;
	uint8_t nendpt;

	if (iface == NULL) {
		return (USB_ERR_INVAL);
	}
	DPRINTFN(5, "iface_index=%d alt_index=%d\n",
	    iface_index, alt_index);

	sx_assert(udev->default_sx + 1, SA_LOCKED);

	pipe = udev->pipes;
	pipe_end = udev->pipes + USB_EP_MAX;

	/*
	 * Check if any USB pipes on the given USB interface are in
	 * use:
	 */
	while (pipe != pipe_end) {
		if ((pipe->edesc != NULL) &&
		    (pipe->iface_index == iface_index) &&
		    (pipe->refcount != 0)) {
			return (USB_ERR_IN_USE);
		}
		pipe++;
	}

	pipe = &udev->pipes[0];

	id = usb2_find_idesc(udev->cdesc, iface_index, alt_index);
	if (id == NULL) {
		return (USB_ERR_INVAL);
	}
	/*
	 * Free old pipes after we know that an interface descriptor exists,
	 * if any.
	 */
	usb2_free_pipe_data(udev, iface_index, 0 - 1);

	/* Setup USB interface structure */
	iface->idesc = id;
	iface->alt_index = alt_index;
	iface->parent_iface_index = USB_IFACE_INDEX_ANY;

	nendpt = id->bNumEndpoints;
	DPRINTFN(5, "found idesc nendpt=%d\n", nendpt);

	desc = (void *)id;

	while (nendpt--) {
		DPRINTFN(11, "endpt=%d\n", nendpt);

		while ((desc = usb2_desc_foreach(udev->cdesc, desc))) {
			if ((desc->bDescriptorType == UDESC_ENDPOINT) &&
			    (desc->bLength >= sizeof(*ed))) {
				goto found;
			}
			if (desc->bDescriptorType == UDESC_INTERFACE) {
				break;
			}
		}
		goto error;

found:
		ed = (void *)desc;

		/* find a free pipe */
		while (pipe != pipe_end) {
			if (pipe->edesc == NULL) {
				/* pipe is free */
				usb2_fill_pipe_data(udev, iface_index, ed, pipe);
				break;
			}
			pipe++;
		}
	}
	return (USB_ERR_NORMAL_COMPLETION);

error:
	/* passed end, or bad desc */
	DPRINTFN(0, "%s: bad descriptor(s), addr=%d!\n",
	    __FUNCTION__, udev->address);

	/* free old pipes if any */
	usb2_free_pipe_data(udev, iface_index, 0 - 1);
	return (USB_ERR_INVAL);
}

/*------------------------------------------------------------------------*
 *	usb2_free_iface_data
 *
 * This function will free all USB interfaces and USB pipes belonging
 * to an USB device.
 *------------------------------------------------------------------------*/
static void
usb2_free_iface_data(struct usb2_device *udev)
{
	struct usb2_interface *iface = udev->ifaces;
	struct usb2_interface *iface_end = udev->ifaces + USB_IFACE_MAX;

	/* mtx_assert() */

	/* free Linux compat device, if any */
	if (udev->linux_dev) {
		usb_linux_free_device(udev->linux_dev);
		udev->linux_dev = NULL;
	}
	/* free all pipes, if any */
	usb2_free_pipe_data(udev, 0, 0);

	/* free all interfaces, if any */
	while (iface != iface_end) {
		iface->idesc = NULL;
		iface->alt_index = 0;
		iface->parent_iface_index = USB_IFACE_INDEX_ANY;
		iface->perm.mode = 0;	/* disable permissions */
		iface++;
	}

	/* free "cdesc" after "ifaces", if any */
	if (udev->cdesc) {
		free(udev->cdesc, M_USB);
		udev->cdesc = NULL;
	}
	/* set unconfigured state */
	udev->curr_config_no = USB_UNCONFIG_NO;
	udev->curr_config_index = USB_UNCONFIG_INDEX;
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_set_config_index
 *
 * This function selects configuration by index, independent of the
 * actual configuration number. This function should not be used by
 * USB drivers.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_set_config_index(struct usb2_device *udev, uint8_t index)
{
	struct usb2_status ds;
	struct usb2_hub_descriptor hd;
	struct usb2_config_descriptor *cdp;
	uint16_t power;
	uint16_t max_power;
	uint8_t nifc;
	uint8_t selfpowered;
	uint8_t do_unlock;
	usb2_error_t err;

	DPRINTFN(6, "udev=%p index=%d\n", udev, index);

	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}

	/* detach all interface drivers */
	usb2_detach_device(udev, USB_IFACE_INDEX_ANY, 1);

	/* free all FIFOs except control endpoint FIFOs */
	usb2_fifo_free_wrap(udev, USB_IFACE_INDEX_ANY, 0);

	/* free all configuration data structures */
	usb2_free_iface_data(udev);

	if (index == USB_UNCONFIG_INDEX) {
		/*
		 * Leave unallocated when unconfiguring the
		 * device. "usb2_free_iface_data()" will also reset
		 * the current config number and index.
		 */
		err = usb2_req_set_config(udev, &Giant, USB_UNCONFIG_NO);
		goto done;
	}
	/* get the full config descriptor */
	err = usb2_req_get_config_desc_full(udev,
	    &Giant, &cdp, M_USB, index);
	if (err) {
		goto done;
	}
	/* set the new config descriptor */

	udev->cdesc = cdp;

	if (cdp->bNumInterface > USB_IFACE_MAX) {
		DPRINTFN(0, "too many interfaces: %d\n", cdp->bNumInterface);
		cdp->bNumInterface = USB_IFACE_MAX;
	}
	/* Figure out if the device is self or bus powered. */
	selfpowered = 0;
	if ((!udev->flags.uq_bus_powered) &&
	    (cdp->bmAttributes & UC_SELF_POWERED) &&
	    (udev->flags.usb2_mode == USB_MODE_HOST)) {
		/* May be self powered. */
		if (cdp->bmAttributes & UC_BUS_POWERED) {
			/* Must ask device. */
			if (udev->flags.uq_power_claim) {
				/*
				 * HUB claims to be self powered, but isn't.
				 * It seems that the power status can be
				 * determined by the HUB characteristics.
				 */
				err = usb2_req_get_hub_descriptor
				    (udev, &Giant, &hd, 1);
				if (err) {
					DPRINTFN(0, "could not read "
					    "HUB descriptor: %s\n",
					    usb2_errstr(err));

				} else if (UGETW(hd.wHubCharacteristics) &
				    UHD_PWR_INDIVIDUAL) {
					selfpowered = 1;
				}
				DPRINTF("characteristics=0x%04x\n",
				    UGETW(hd.wHubCharacteristics));
			} else {
				err = usb2_req_get_device_status
				    (udev, &Giant, &ds);
				if (err) {
					DPRINTFN(0, "could not read "
					    "device status: %s\n",
					    usb2_errstr(err));
				} else if (UGETW(ds.wStatus) & UDS_SELF_POWERED) {
					selfpowered = 1;
				}
				DPRINTF("status=0x%04x \n",
				    UGETW(ds.wStatus));
			}
		} else
			selfpowered = 1;
	}
	DPRINTF("udev=%p cdesc=%p (addr %d) cno=%d attr=0x%02x, "
	    "selfpowered=%d, power=%d\n",
	    udev, cdp,
	    cdp->bConfigurationValue, udev->address, cdp->bmAttributes,
	    selfpowered, cdp->bMaxPower * 2);

	/* Check if we have enough power. */
	power = cdp->bMaxPower * 2;

	if (udev->parent_hub) {
		max_power = udev->parent_hub->hub->portpower;
	} else {
		max_power = USB_MAX_POWER;
	}

	if (power > max_power) {
		DPRINTFN(0, "power exceeded %d > %d\n", power, max_power);
		err = USB_ERR_NO_POWER;
		goto done;
	}
	/* Only update "self_powered" in USB Host Mode */
	if (udev->flags.usb2_mode == USB_MODE_HOST) {
		udev->flags.self_powered = selfpowered;
	}
	udev->power = power;
	udev->curr_config_no = cdp->bConfigurationValue;
	udev->curr_config_index = index;

	/* Set the actual configuration value. */
	err = usb2_req_set_config(udev, &Giant, cdp->bConfigurationValue);
	if (err) {
		goto done;
	}
	/* Allocate and fill interface data. */
	nifc = cdp->bNumInterface;
	while (nifc--) {
		err = usb2_fill_iface_data(udev, nifc, 0);
		if (err) {
			goto done;
		}
	}

done:
	DPRINTF("error=%s\n", usb2_errstr(err));
	if (err) {
		usb2_free_iface_data(udev);
	}
	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_set_alt_interface_index
 *
 * This function will select an alternate interface index for the
 * given interface index. The interface should not be in use when this
 * function is called. That means there should be no open USB
 * transfers. Else an error is returned.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_set_alt_interface_index(struct usb2_device *udev,
    uint8_t iface_index, uint8_t alt_index)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	usb2_error_t err;
	uint8_t do_unlock;

	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}
	if (iface == NULL) {
		err = USB_ERR_INVAL;
		goto done;
	}
	if (udev->flags.usb2_mode == USB_MODE_DEVICE) {
		usb2_detach_device(udev, iface_index, 1);
	}
	/* free all FIFOs for this interface */
	usb2_fifo_free_wrap(udev, iface_index, 0);

	err = usb2_fill_iface_data(udev, iface_index, alt_index);
	if (err) {
		goto done;
	}
	err = usb2_req_set_alt_interface_no
	    (udev, &Giant, iface_index,
	    iface->idesc->bAlternateSetting);

done:
	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_set_endpoint_stall
 *
 * This function is used to make a BULK or INTERRUPT endpoint
 * send STALL tokens.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_set_endpoint_stall(struct usb2_device *udev, struct usb2_pipe *pipe,
    uint8_t do_stall)
{
	struct usb2_xfer *xfer;
	uint8_t et;
	uint8_t was_stalled;

	if (pipe == NULL) {
		/* nothing to do */
		DPRINTF("Cannot find endpoint\n");
		/*
		 * Pretend that the clear or set stall request is
		 * successful else some USB host stacks can do
		 * strange things, especially when a control endpoint
		 * stalls.
		 */
		return (0);
	}
	et = (pipe->edesc->bmAttributes & UE_XFERTYPE);

	if ((et != UE_BULK) &&
	    (et != UE_INTERRUPT)) {
		/*
	         * Should not stall control
	         * nor isochronous endpoints.
	         */
		DPRINTF("Invalid endpoint\n");
		return (0);
	}
	USB_BUS_LOCK(udev->bus);

	/* store current stall state */
	was_stalled = pipe->is_stalled;

	/* check for no change */
	if (was_stalled && do_stall) {
		/* if the pipe is already stalled do nothing */
		USB_BUS_UNLOCK(udev->bus);
		DPRINTF("No change\n");
		return (0);
	}
	/* set stalled state */
	pipe->is_stalled = 1;

	if (do_stall || (!was_stalled)) {
		if (!was_stalled) {
			/* lookup the current USB transfer, if any */
			xfer = pipe->pipe_q.curr;
		} else {
			xfer = NULL;
		}

		/*
		 * If "xfer" is non-NULL the "set_stall" method will
		 * complete the USB transfer like in case of a timeout
		 * setting the error code "USB_ERR_STALLED".
		 */
		(udev->bus->methods->set_stall) (udev, xfer, pipe);
	}
	if (!do_stall) {
		pipe->toggle_next = 0;	/* reset data toggle */
		pipe->is_stalled = 0;	/* clear stalled state */

		(udev->bus->methods->clear_stall) (udev, pipe);

		/* start up the current or next transfer, if any */
		usb2_command_wrapper(&pipe->pipe_q, pipe->pipe_q.curr);
	}
	USB_BUS_UNLOCK(udev->bus);
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_reset_iface_endpoints - used in USB device side mode
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_reset_iface_endpoints(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_pipe *pipe;
	struct usb2_pipe *pipe_end;
	usb2_error_t err;

	pipe = udev->pipes;
	pipe_end = udev->pipes + USB_EP_MAX;

	for (; pipe != pipe_end; pipe++) {

		if ((pipe->edesc == NULL) ||
		    (pipe->iface_index != iface_index)) {
			continue;
		}
		/* simulate a clear stall from the peer */
		err = usb2_set_endpoint_stall(udev, pipe, 0);
		if (err) {
			/* just ignore */
		}
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_detach_device_sub
 *
 * This function will try to detach an USB device. If it fails a panic
 * will result.
 *------------------------------------------------------------------------*/
static void
usb2_detach_device_sub(struct usb2_device *udev, device_t *ppdev,
    uint8_t free_subdev)
{
	device_t dev;
	int err;

	if (!free_subdev) {

		*ppdev = NULL;

	} else if (*ppdev) {

		/*
		 * NOTE: It is important to clear "*ppdev" before deleting
		 * the child due to some device methods being called late
		 * during the delete process !
		 */
		dev = *ppdev;
		*ppdev = NULL;

		device_printf(dev, "at %s, port %d, addr %d "
		    "(disconnected)\n",
		    device_get_nameunit(udev->parent_dev),
		    udev->port_no, udev->address);

		if (device_is_attached(dev)) {
			if (udev->flags.suspended) {
				err = DEVICE_RESUME(dev);
				if (err) {
					device_printf(dev, "Resume failed!\n");
				}
			}
			if (device_detach(dev)) {
				goto error;
			}
		}
		if (device_delete_child(udev->parent_dev, dev)) {
			goto error;
		}
	}
	return;

error:
	/* Detach is not allowed to fail in the USB world */
	panic("An USB driver would not detach!\n");
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_detach_device
 *
 * The following function will detach the matching interfaces.
 * This function is NULL safe.
 *------------------------------------------------------------------------*/
void
usb2_detach_device(struct usb2_device *udev, uint8_t iface_index,
    uint8_t free_subdev)
{
	struct usb2_interface *iface;
	uint8_t i;
	uint8_t do_unlock;

	if (udev == NULL) {
		/* nothing to do */
		return;
	}
	DPRINTFN(4, "udev=%p\n", udev);

	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}

	/*
	 * First detach the child to give the child's detach routine a
	 * chance to detach the sub-devices in the correct order.
	 * Then delete the child using "device_delete_child()" which
	 * will detach all sub-devices from the bottom and upwards!
	 */
	if (iface_index != USB_IFACE_INDEX_ANY) {
		i = iface_index;
		iface_index = i + 1;
	} else {
		i = 0;
		iface_index = USB_IFACE_MAX;
	}

	/* do the detach */

	for (; i != iface_index; i++) {

		iface = usb2_get_iface(udev, i);
		if (iface == NULL) {
			/* looks like the end of the USB interfaces */
			break;
		}
		usb2_detach_device_sub(udev, &iface->subdev, free_subdev);
	}

	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_probe_and_attach_sub
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
usb2_probe_and_attach_sub(struct usb2_device *udev,
    struct usb2_attach_arg *uaa)
{
	struct usb2_interface *iface;
	device_t dev;
	int err;

	iface = uaa->iface;
	if (iface->parent_iface_index != USB_IFACE_INDEX_ANY) {
		/* leave interface alone */
		return (0);
	}
	dev = iface->subdev;
	if (dev) {

		/* clean up after module unload */

		if (device_is_attached(dev)) {
			/* already a device there */
			return (0);
		}
		/* clear "iface->subdev" as early as possible */

		iface->subdev = NULL;

		if (device_delete_child(udev->parent_dev, dev)) {

			/*
			 * Panic here, else one can get a double call
			 * to device_detach().  USB devices should
			 * never fail on detach!
			 */
			panic("device_delete_child() failed!\n");
		}
	}
	if (uaa->temp_dev == NULL) {

		/* create a new child */
		uaa->temp_dev = device_add_child(udev->parent_dev, NULL, -1);
		if (uaa->temp_dev == NULL) {
			device_printf(udev->parent_dev,
			    "Device creation failed!\n");
			return (1);	/* failure */
		}
		device_set_ivars(uaa->temp_dev, uaa);
		device_quiet(uaa->temp_dev);
	}
	/*
	 * Set "subdev" before probe and attach so that "devd" gets
	 * the information it needs.
	 */
	iface->subdev = uaa->temp_dev;

	if (device_probe_and_attach(iface->subdev) == 0) {
		/*
		 * The USB attach arguments are only available during probe
		 * and attach !
		 */
		uaa->temp_dev = NULL;
		device_set_ivars(iface->subdev, NULL);

		if (udev->flags.suspended) {
			err = DEVICE_SUSPEND(iface->subdev);
			device_printf(iface->subdev, "Suspend failed\n");
		}
		return (0);		/* success */
	} else {
		/* No USB driver found */
		iface->subdev = NULL;
	}
	return (1);			/* failure */
}

/*------------------------------------------------------------------------*
 *	usb2_set_parent_iface
 *
 * Using this function will lock the alternate interface setting on an
 * interface. It is typically used for multi interface drivers. In USB
 * device side mode it is assumed that the alternate interfaces all
 * have the same endpoint descriptors. The default parent index value
 * is "USB_IFACE_INDEX_ANY". Then the alternate setting value is not
 * locked.
 *------------------------------------------------------------------------*/
void
usb2_set_parent_iface(struct usb2_device *udev, uint8_t iface_index,
    uint8_t parent_index)
{
	struct usb2_interface *iface;

	iface = usb2_get_iface(udev, iface_index);
	if (iface) {
		iface->parent_iface_index = parent_index;
	}
	return;
}

static void
usb2_init_attach_arg(struct usb2_device *udev,
    struct usb2_attach_arg *uaa)
{
	bzero(uaa, sizeof(*uaa));

	uaa->device = udev;
	uaa->usb2_mode = udev->flags.usb2_mode;
	uaa->port = udev->port_no;

	uaa->info.idVendor = UGETW(udev->ddesc.idVendor);
	uaa->info.idProduct = UGETW(udev->ddesc.idProduct);
	uaa->info.bcdDevice = UGETW(udev->ddesc.bcdDevice);
	uaa->info.bDeviceClass = udev->ddesc.bDeviceClass;
	uaa->info.bDeviceSubClass = udev->ddesc.bDeviceSubClass;
	uaa->info.bDeviceProtocol = udev->ddesc.bDeviceProtocol;
	uaa->info.bConfigIndex = udev->curr_config_index;
	uaa->info.bConfigNum = udev->curr_config_no;

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_probe_and_attach
 *
 * This function is called from "uhub_explore_sub()",
 * "usb2_handle_set_config()" and "usb2_handle_request()".
 *
 * Returns:
 *    0: Success
 * Else: A control transfer failed
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_probe_and_attach(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_attach_arg uaa;
	struct usb2_interface *iface;
	uint8_t i;
	uint8_t j;
	uint8_t do_unlock;

	if (udev == NULL) {
		DPRINTF("udev == NULL\n");
		return (USB_ERR_INVAL);
	}
	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}

	if (udev->curr_config_index == USB_UNCONFIG_INDEX) {
		/* do nothing - no configuration has been set */
		goto done;
	}
	/* setup USB attach arguments */

	usb2_init_attach_arg(udev, &uaa);

	/* Check if only one interface should be probed: */
	if (iface_index != USB_IFACE_INDEX_ANY) {
		i = iface_index;
		j = i + 1;
	} else {
		i = 0;
		j = USB_IFACE_MAX;
	}

	/* Do the probe and attach */
	for (; i != j; i++) {

		iface = usb2_get_iface(udev, i);
		if (iface == NULL) {
			/*
			 * Looks like the end of the USB
			 * interfaces !
			 */
			DPRINTFN(2, "end of interfaces "
			    "at %u\n", i);
			break;
		}
		if (iface->idesc == NULL) {
			/* no interface descriptor */
			continue;
		}
		uaa.iface = iface;

		uaa.info.bInterfaceClass =
		    iface->idesc->bInterfaceClass;
		uaa.info.bInterfaceSubClass =
		    iface->idesc->bInterfaceSubClass;
		uaa.info.bInterfaceProtocol =
		    iface->idesc->bInterfaceProtocol;
		uaa.info.bIfaceIndex = i;
		uaa.info.bIfaceNum =
		    iface->idesc->bInterfaceNumber;
		uaa.use_generic = 0;

		DPRINTFN(2, "iclass=%u/%u/%u iindex=%u/%u\n",
		    uaa.info.bInterfaceClass,
		    uaa.info.bInterfaceSubClass,
		    uaa.info.bInterfaceProtocol,
		    uaa.info.bIfaceIndex,
		    uaa.info.bIfaceNum);

		/* try specific interface drivers first */

		if (usb2_probe_and_attach_sub(udev, &uaa)) {
			/* ignore */
		}
		/* try generic interface drivers last */

		uaa.use_generic = 1;

		if (usb2_probe_and_attach_sub(udev, &uaa)) {
			/* ignore */
		}
	}

	if (uaa.temp_dev) {
		/* remove the last created child; it is unused */

		if (device_delete_child(udev->parent_dev, uaa.temp_dev)) {
			DPRINTFN(0, "device delete child failed!\n");
		}
	}
done:
	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_suspend_resume_sub
 *
 * This function is called when the suspend or resume methods should
 * be executed on an USB device.
 *------------------------------------------------------------------------*/
static void
usb2_suspend_resume_sub(struct usb2_device *udev, device_t dev, uint8_t do_suspend)
{
	int err;

	if (dev == NULL) {
		return;
	}
	if (!device_is_attached(dev)) {
		return;
	}
	if (do_suspend) {
		err = DEVICE_SUSPEND(dev);
	} else {
		err = DEVICE_RESUME(dev);
	}
	if (err) {
		device_printf(dev, "%s failed!\n",
		    do_suspend ? "Suspend" : "Resume");
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_suspend_resume_device
 *
 * The following function will suspend or resume the USB device.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_suspend_resume(struct usb2_device *udev, uint8_t do_suspend)
{
	struct usb2_interface *iface;
	uint8_t i;

	if (udev == NULL) {
		/* nothing to do */
		return (0);
	}
	DPRINTFN(4, "udev=%p do_suspend=%d\n", udev, do_suspend);

	sx_assert(udev->default_sx + 1, SA_LOCKED);

	USB_BUS_LOCK(udev->bus);
	/* filter the suspend events */
	if (udev->flags.suspended == do_suspend) {
		USB_BUS_UNLOCK(udev->bus);
		/* nothing to do */
		return (0);
	}
	udev->flags.suspended = do_suspend;
	USB_BUS_UNLOCK(udev->bus);

	/* do the suspend or resume */

	for (i = 0; i != USB_IFACE_MAX; i++) {

		iface = usb2_get_iface(udev, i);
		if (iface == NULL) {
			/* looks like the end of the USB interfaces */
			break;
		}
		usb2_suspend_resume_sub(udev, iface->subdev, do_suspend);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *      usb2_clear_stall_proc
 *
 * This function performs generic USB clear stall operations.
 *------------------------------------------------------------------------*/
static void
usb2_clear_stall_proc(struct usb2_proc_msg *_pm)
{
	struct usb2_clear_stall_msg *pm = (void *)_pm;
	struct usb2_device *udev = pm->udev;

	/* Change lock */
	USB_BUS_UNLOCK(udev->bus);
	mtx_lock(udev->default_mtx);

	/* Start clear stall callback */
	usb2_transfer_start(udev->default_xfer[1]);

	/* Change lock */
	mtx_unlock(udev->default_mtx);
	USB_BUS_LOCK(udev->bus);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_alloc_device
 *
 * This function allocates a new USB device. This function is called
 * when a new device has been put in the powered state, but not yet in
 * the addressed state. Get initial descriptor, set the address, get
 * full descriptor and get strings.
 *
 * Return values:
 *    0: Failure
 * Else: Success
 *------------------------------------------------------------------------*/
struct usb2_device *
usb2_alloc_device(device_t parent_dev, struct usb2_bus *bus,
    struct usb2_device *parent_hub, uint8_t depth,
    uint8_t port_index, uint8_t port_no, uint8_t speed, uint8_t usb2_mode)
{
	struct usb2_attach_arg uaa;
	struct usb2_device *udev;
	struct usb2_device *adev;
	struct usb2_device *hub;
	uint8_t *scratch_ptr;
	uint32_t scratch_size;
	usb2_error_t err;
	uint8_t device_index;

	DPRINTF("parent_dev=%p, bus=%p, parent_hub=%p, depth=%u, "
	    "port_index=%u, port_no=%u, speed=%u, usb2_mode=%u\n",
	    parent_dev, bus, parent_hub, depth, port_index, port_no,
	    speed, usb2_mode);

	/*
	 * Find an unused device index. In USB Host mode this is the
	 * same as the device address.
	 *
	 * NOTE: Index 1 is reserved for the Root HUB.
	 */
	for (device_index = USB_ROOT_HUB_ADDR; device_index !=
	    USB_MAX_DEVICES; device_index++) {
		if (bus->devices[device_index] == NULL)
			break;
	}

	if (device_index == USB_MAX_DEVICES) {
		device_printf(bus->bdev,
		    "No free USB device index for new device!\n");
		return (NULL);
	}
	if (depth > 0x10) {
		device_printf(bus->bdev,
		    "Invalid device depth!\n");
		return (NULL);
	}
	udev = malloc(sizeof(*udev), M_USB, M_WAITOK | M_ZERO);
	if (udev == NULL) {
		return (NULL);
	}
	/* initialise our SX-lock */
	sx_init(udev->default_sx, "0123456789ABCDEF - USB device SX lock" + depth);

	/* initialise our SX-lock */
	sx_init(udev->default_sx + 1, "0123456789ABCDEF - USB config SX lock" + depth);

	usb2_cv_init(udev->default_cv, "WCTRL");
	usb2_cv_init(udev->default_cv + 1, "UGONE");

	/* initialise our mutex */
	mtx_init(udev->default_mtx, "USB device mutex", NULL, MTX_DEF);

	/* initialise generic clear stall */
	udev->cs_msg[0].hdr.pm_callback = &usb2_clear_stall_proc;
	udev->cs_msg[0].udev = udev;
	udev->cs_msg[1].hdr.pm_callback = &usb2_clear_stall_proc;
	udev->cs_msg[1].udev = udev;

	/* initialise some USB device fields */
	udev->parent_hub = parent_hub;
	udev->parent_dev = parent_dev;
	udev->port_index = port_index;
	udev->port_no = port_no;
	udev->depth = depth;
	udev->bus = bus;
	udev->address = USB_START_ADDR;	/* default value */
	udev->plugtime = (uint32_t)ticks;
	udev->power_mode = USB_POWER_MODE_ON;

	/* we are not ready yet */
	udev->refcount = 1;

	/* set up default endpoint descriptor */
	udev->default_ep_desc.bLength = sizeof(udev->default_ep_desc);
	udev->default_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	udev->default_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	udev->default_ep_desc.bmAttributes = UE_CONTROL;
	udev->default_ep_desc.wMaxPacketSize[0] = USB_MAX_IPACKET;
	udev->default_ep_desc.wMaxPacketSize[1] = 0;
	udev->default_ep_desc.bInterval = 0;
	udev->ddesc.bMaxPacketSize = USB_MAX_IPACKET;

	udev->speed = speed;
	udev->flags.usb2_mode = usb2_mode;

	/* check speed combination */

	hub = udev->parent_hub;
	if (hub) {
		if (usb2_hub_speed_combs[hub->speed][speed] == 0) {
#if USB_DEBUG
			printf("%s: the selected subdevice and HUB speed "
			    "combination is not supported %d/%d.\n",
			    __FUNCTION__, speed, hub->speed);
#endif
			/* reject this combination */
			err = USB_ERR_INVAL;
			goto done;
		}
	}
	/* search for our High Speed USB HUB, if any */

	adev = udev;
	hub = udev->parent_hub;

	while (hub) {
		if (hub->speed == USB_SPEED_HIGH) {
			udev->hs_hub_addr = hub->address;
			udev->hs_port_no = adev->port_no;
			break;
		}
		adev = hub;
		hub = hub->parent_hub;
	}

	/* init the default pipe */
	usb2_fill_pipe_data(udev, 0,
	    &udev->default_ep_desc,
	    &udev->default_pipe);

	/* set device index */
	udev->device_index = device_index;

	if (udev->flags.usb2_mode == USB_MODE_HOST) {

		err = usb2_req_set_address(udev, &Giant, device_index);

		/* This is the new USB device address from now on */

		udev->address = device_index;

		/*
		 * We ignore any set-address errors, hence there are
		 * buggy USB devices out there that actually receive
		 * the SETUP PID, but manage to set the address before
		 * the STATUS stage is ACK'ed. If the device responds
		 * to the subsequent get-descriptor at the new
		 * address, then we know that the set-address command
		 * was successful.
		 */
		if (err) {
			DPRINTFN(0, "set address %d failed "
			    "(ignored)\n", udev->address);
		}
		/* allow device time to set new address */
		usb2_pause_mtx(&Giant, USB_SET_ADDRESS_SETTLE);
	} else {
		/* We are not self powered */
		udev->flags.self_powered = 0;

		/* Set unconfigured state */
		udev->curr_config_no = USB_UNCONFIG_NO;
		udev->curr_config_index = USB_UNCONFIG_INDEX;

		/* Setup USB descriptors */
		err = (usb2_temp_setup_by_index_p) (udev, usb2_template);
		if (err) {
			DPRINTFN(0, "setting up USB template failed maybe the USB "
			    "template module has not been loaded\n");
			goto done;
		}
	}

	/*
	 * Get the first 8 bytes of the device descriptor !
	 *
	 * NOTE: "usb2_do_request" will check the device descriptor
	 * next time we do a request to see if the maximum packet size
	 * changed! The 8 first bytes of the device descriptor
	 * contains the maximum packet size to use on control endpoint
	 * 0. If this value is different from "USB_MAX_IPACKET" a new
	 * USB control request will be setup!
	 */
	err = usb2_req_get_desc(udev, &Giant, &udev->ddesc,
	    USB_MAX_IPACKET, USB_MAX_IPACKET, 0, UDESC_DEVICE, 0, 0);
	if (err) {
		DPRINTFN(0, "getting device descriptor "
		    "at addr %d failed!\n", udev->address);
		goto done;
	}
	DPRINTF("adding unit addr=%d, rev=%02x, class=%d, "
	    "subclass=%d, protocol=%d, maxpacket=%d, len=%d, speed=%d\n",
	    udev->address, UGETW(udev->ddesc.bcdUSB),
	    udev->ddesc.bDeviceClass,
	    udev->ddesc.bDeviceSubClass,
	    udev->ddesc.bDeviceProtocol,
	    udev->ddesc.bMaxPacketSize,
	    udev->ddesc.bLength,
	    udev->speed);

	/* get the full device descriptor */
	err = usb2_req_get_device_desc(udev, &Giant, &udev->ddesc);
	if (err) {
		DPRINTF("addr=%d, getting full desc failed\n",
		    udev->address);
		goto done;
	}
	/*
	 * Setup temporary USB attach args so that we can figure out some
	 * basic quirks for this device.
	 */
	usb2_init_attach_arg(udev, &uaa);

	if (usb2_test_quirk(&uaa, UQ_BUS_POWERED)) {
		udev->flags.uq_bus_powered = 1;
	}
	if (usb2_test_quirk(&uaa, UQ_POWER_CLAIM)) {
		udev->flags.uq_power_claim = 1;
	}
	if (usb2_test_quirk(&uaa, UQ_NO_STRINGS)) {
		udev->flags.no_strings = 1;
	}
	/*
	 * Workaround for buggy USB devices.
	 *
	 * It appears that some string-less USB chips will crash and
	 * disappear if any attempts are made to read any string
	 * descriptors.
	 *
	 * Try to detect such chips by checking the strings in the USB
	 * device descriptor. If no strings are present there we
	 * simply disable all USB strings.
	 */
	scratch_ptr = udev->bus->scratch[0].data;
	scratch_size = sizeof(udev->bus->scratch[0].data);

	if (udev->ddesc.iManufacturer ||
	    udev->ddesc.iProduct ||
	    udev->ddesc.iSerialNumber) {
		/* read out the language ID string */
		err = usb2_req_get_string_desc(udev, &Giant,
		    (char *)scratch_ptr, 4, scratch_size,
		    USB_LANGUAGE_TABLE);
	} else {
		err = USB_ERR_INVAL;
	}

	if (err || (scratch_ptr[0] < 4)) {
		udev->flags.no_strings = 1;
	} else {
		/* pick the first language as the default */
		udev->langid = UGETW(scratch_ptr + 2);
	}

	/* assume 100mA bus powered for now. Changed when configured. */
	udev->power = USB_MIN_POWER;

	/* get serial number string */
	err = usb2_req_get_string_any
	    (udev, &Giant, (char *)scratch_ptr,
	    scratch_size, udev->ddesc.iSerialNumber);

	strlcpy(udev->serial, (char *)scratch_ptr, sizeof(udev->serial));

	/* get manufacturer string */
	err = usb2_req_get_string_any
	    (udev, &Giant, (char *)scratch_ptr,
	    scratch_size, udev->ddesc.iManufacturer);

	strlcpy(udev->manufacturer, (char *)scratch_ptr, sizeof(udev->manufacturer));

	/* get product string */
	err = usb2_req_get_string_any
	    (udev, &Giant, (char *)scratch_ptr,
	    scratch_size, udev->ddesc.iProduct);

	strlcpy(udev->product, (char *)scratch_ptr, sizeof(udev->product));

	/* finish up all the strings */
	usb2_check_strings(udev);

	if (udev->flags.usb2_mode == USB_MODE_HOST) {
		uint8_t config_index;
		uint8_t config_quirk;

		/*
		 * Most USB devices should attach to config index 0 by
		 * default
		 */
		if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_0)) {
			config_index = 0;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_1)) {
			config_index = 1;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_2)) {
			config_index = 2;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_3)) {
			config_index = 3;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_4)) {
			config_index = 4;
			config_quirk = 1;
		} else {
			config_index = 0;
			config_quirk = 0;
		}

repeat_set_config:

		DPRINTF("setting config %u\n", config_index);

		/* get the USB device configured */
		sx_xlock(udev->default_sx + 1);
		err = usb2_set_config_index(udev, config_index);
		sx_unlock(udev->default_sx + 1);
		if (err) {
			DPRINTFN(0, "Failure selecting "
			    "configuration index %u: %s, port %u, addr %u\n",
			    config_index, usb2_errstr(err), udev->port_no,
			    udev->address);

		} else if (config_quirk) {
			/* user quirk selects configuration index */
		} else if ((config_index + 1) < udev->ddesc.bNumConfigurations) {

			if ((udev->cdesc->bNumInterface < 2) &&
			    (usb2_get_no_endpoints(udev->cdesc) == 0)) {
				DPRINTFN(0, "Found no endpoints "
				    "(trying next config)!\n");
				config_index++;
				goto repeat_set_config;
			}
			if (config_index == 0) {
				/*
				 * Try to figure out if we have an
				 * auto-install disk there:
				 */
				if (usb2_test_autoinstall(udev, 0) == 0) {
					DPRINTFN(0, "Found possible auto-install "
					    "disk (trying next config)\n");
					config_index++;
					goto repeat_set_config;
				}
			}
		} else if (UGETW(udev->ddesc.idVendor) == USB_VENDOR_HUAWEI) {
			if (usb2_test_huawei(udev, 0) == 0) {
				DPRINTFN(0, "Found Huawei auto-install disk!\n");
				err = USB_ERR_STALLED;	/* fake an error */
			}
		}
	} else {
		err = 0;		/* set success */
	}

	DPRINTF("new dev (addr %d), udev=%p, parent_hub=%p\n",
	    udev->address, udev, udev->parent_hub);

	/* register our device - we are ready */
	usb2_bus_port_set_device(bus, parent_hub ?
	    parent_hub->hub->ports + port_index : NULL, udev, device_index);

	/* make a symlink for UGEN */
	if (snprintf((char *)scratch_ptr, scratch_size,
	    USB_DEVICE_NAME "%u.%u.0.0",
	    device_get_unit(udev->bus->bdev),
	    udev->device_index)) {
		/* ignore */
	}
	udev->ugen_symlink =
	    usb2_alloc_symlink((char *)scratch_ptr, "ugen%u.%u",
	    device_get_unit(udev->bus->bdev),
	    udev->device_index);

	printf("ugen%u.%u: <%s> at %s\n",
	    device_get_unit(udev->bus->bdev),
	    udev->device_index, udev->manufacturer,
	    device_get_nameunit(udev->bus->bdev));

	usb2_notify_addq("+", udev);
done:
	if (err) {
		/* free device  */
		usb2_free_device(udev);
		udev = NULL;
	}
	return (udev);
}

/*------------------------------------------------------------------------*
 *	usb2_free_device
 *
 * This function is NULL safe and will free an USB device.
 *------------------------------------------------------------------------*/
void
usb2_free_device(struct usb2_device *udev)
{
	struct usb2_bus *bus;

	if (udev == NULL) {
		/* already freed */
		return;
	}
	DPRINTFN(4, "udev=%p port=%d\n", udev, udev->port_no);

	usb2_notify_addq("-", udev);

	bus = udev->bus;

	/*
	 * Destroy UGEN symlink, if any
	 */
	if (udev->ugen_symlink) {
		usb2_free_symlink(udev->ugen_symlink);
		udev->ugen_symlink = NULL;
	}
	/*
	 * Unregister our device first which will prevent any further
	 * references:
	 */
	usb2_bus_port_set_device(bus, udev->parent_hub ?
	    udev->parent_hub->hub->ports + udev->port_index : NULL,
	    NULL, USB_ROOT_HUB_ADDR);

	/* wait for all pending references to go away: */

	mtx_lock(&usb2_ref_lock);
	udev->refcount--;
	while (udev->refcount != 0) {
		usb2_cv_wait(udev->default_cv + 1, &usb2_ref_lock);
	}
	mtx_unlock(&usb2_ref_lock);

	if (udev->flags.usb2_mode == USB_MODE_DEVICE) {
		/* stop receiving any control transfers (Device Side Mode) */
		usb2_transfer_unsetup(udev->default_xfer, USB_DEFAULT_XFER_MAX);
	}
	/* free all FIFOs */
	usb2_fifo_free_wrap(udev, USB_IFACE_INDEX_ANY, 1);

	/*
	 * Free all interface related data and FIFOs, if any.
	 */
	usb2_free_iface_data(udev);

	/* unsetup any leftover default USB transfers */
	usb2_transfer_unsetup(udev->default_xfer, USB_DEFAULT_XFER_MAX);

	(usb2_temp_unsetup_p) (udev);

	sx_destroy(udev->default_sx);
	sx_destroy(udev->default_sx + 1);

	usb2_cv_destroy(udev->default_cv);
	usb2_cv_destroy(udev->default_cv + 1);

	mtx_destroy(udev->default_mtx);

	/* free device */
	free(udev, M_USB);

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_get_iface
 *
 * This function is the safe way to get the USB interface structure
 * pointer by interface index.
 *
 * Return values:
 *   NULL: Interface not present.
 *   Else: Pointer to USB interface structure.
 *------------------------------------------------------------------------*/
struct usb2_interface *
usb2_get_iface(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_interface *iface = udev->ifaces + iface_index;

	if ((iface < udev->ifaces) ||
	    (iface_index >= USB_IFACE_MAX) ||
	    (udev->cdesc == NULL) ||
	    (iface_index >= udev->cdesc->bNumInterface)) {
		return (NULL);
	}
	return (iface);
}

/*------------------------------------------------------------------------*
 *	usb2_find_descriptor
 *
 * This function will lookup the first descriptor that matches the
 * criteria given by the arguments "type" and "subtype". Descriptors
 * will only be searched within the interface having the index
 * "iface_index".  If the "id" argument points to an USB descriptor,
 * it will be skipped before the search is started. This allows
 * searching for multiple descriptors using the same criteria. Else
 * the search is started after the interface descriptor.
 *
 * Return values:
 *   NULL: End of descriptors
 *   Else: A descriptor matching the criteria
 *------------------------------------------------------------------------*/
void   *
usb2_find_descriptor(struct usb2_device *udev, void *id, uint8_t iface_index,
    uint8_t type, uint8_t type_mask,
    uint8_t subtype, uint8_t subtype_mask)
{
	struct usb2_descriptor *desc;
	struct usb2_config_descriptor *cd;
	struct usb2_interface *iface;

	cd = usb2_get_config_descriptor(udev);
	if (cd == NULL) {
		return (NULL);
	}
	if (id == NULL) {
		iface = usb2_get_iface(udev, iface_index);
		if (iface == NULL) {
			return (NULL);
		}
		id = usb2_get_interface_descriptor(iface);
		if (id == NULL) {
			return (NULL);
		}
	}
	desc = (void *)id;

	while ((desc = usb2_desc_foreach(cd, desc))) {

		if (desc->bDescriptorType == UDESC_INTERFACE) {
			break;
		}
		if (((desc->bDescriptorType & type_mask) == type) &&
		    ((desc->bDescriptorSubtype & subtype_mask) == subtype)) {
			return (desc);
		}
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_devinfo
 *
 * This function will dump information from the device descriptor
 * belonging to the USB device pointed to by "udev", to the string
 * pointed to by "dst_ptr" having a maximum length of "dst_len" bytes
 * including the terminating zero.
 *------------------------------------------------------------------------*/
void
usb2_devinfo(struct usb2_device *udev, char *dst_ptr, uint16_t dst_len)
{
	struct usb2_device_descriptor *udd = &udev->ddesc;
	uint16_t bcdDevice;
	uint16_t bcdUSB;

	bcdUSB = UGETW(udd->bcdUSB);
	bcdDevice = UGETW(udd->bcdDevice);

	if (udd->bDeviceClass != 0xFF) {
		snprintf(dst_ptr, dst_len, "%s %s, class %d/%d, rev %x.%02x/"
		    "%x.%02x, addr %d", udev->manufacturer, udev->product,
		    udd->bDeviceClass, udd->bDeviceSubClass,
		    (bcdUSB >> 8), bcdUSB & 0xFF,
		    (bcdDevice >> 8), bcdDevice & 0xFF,
		    udev->address);
	} else {
		snprintf(dst_ptr, dst_len, "%s %s, rev %x.%02x/"
		    "%x.%02x, addr %d", udev->manufacturer, udev->product,
		    (bcdUSB >> 8), bcdUSB & 0xFF,
		    (bcdDevice >> 8), bcdDevice & 0xFF,
		    udev->address);
	}
	return;
}

#if USB_VERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
struct usb_knowndev {
	uint16_t vendor;
	uint16_t product;
	uint32_t flags;
	const char *vendorname;
	const char *productname;
};

#define	USB_KNOWNDEV_NOPROD	0x01	/* match on vendor only */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_devtable.h>
#endif					/* USB_VERBOSE */

/*------------------------------------------------------------------------*
 *	usb2_check_strings
 *
 * This function checks the manufacturer and product strings and will
 * fill in defaults for missing strings.
 *------------------------------------------------------------------------*/
static void
usb2_check_strings(struct usb2_device *udev)
{
	struct usb2_device_descriptor *udd = &udev->ddesc;
	const char *vendor;
	const char *product;

#if USB_VERBOSE
	const struct usb_knowndev *kdp;

#endif
	uint16_t vendor_id;
	uint16_t product_id;

	usb2_trim_spaces(udev->manufacturer);
	usb2_trim_spaces(udev->product);

	if (udev->manufacturer[0]) {
		vendor = udev->manufacturer;
	} else {
		vendor = NULL;
	}

	if (udev->product[0]) {
		product = udev->product;
	} else {
		product = NULL;
	}

	vendor_id = UGETW(udd->idVendor);
	product_id = UGETW(udd->idProduct);

#if USB_VERBOSE
	if (vendor == NULL || product == NULL) {

		for (kdp = usb_knowndevs;
		    kdp->vendorname != NULL;
		    kdp++) {
			if (kdp->vendor == vendor_id &&
			    (kdp->product == product_id ||
			    (kdp->flags & USB_KNOWNDEV_NOPROD) != 0))
				break;
		}
		if (kdp->vendorname != NULL) {
			if (vendor == NULL)
				vendor = kdp->vendorname;
			if (product == NULL)
				product = (kdp->flags & USB_KNOWNDEV_NOPROD) == 0 ?
				    kdp->productname : NULL;
		}
	}
#endif
	if (vendor && *vendor) {
		if (udev->manufacturer != vendor) {
			strlcpy(udev->manufacturer, vendor,
			    sizeof(udev->manufacturer));
		}
	} else {
		snprintf(udev->manufacturer,
		    sizeof(udev->manufacturer), "vendor 0x%04x", vendor_id);
	}

	if (product && *product) {
		if (udev->product != product) {
			strlcpy(udev->product, product,
			    sizeof(udev->product));
		}
	} else {
		snprintf(udev->product,
		    sizeof(udev->product), "product 0x%04x", product_id);
	}
	return;
}

uint8_t
usb2_get_speed(struct usb2_device *udev)
{
	return (udev->speed);
}

struct usb2_device_descriptor *
usb2_get_device_descriptor(struct usb2_device *udev)
{
	if (udev == NULL)
		return (NULL);		/* be NULL safe */
	return (&udev->ddesc);
}

struct usb2_config_descriptor *
usb2_get_config_descriptor(struct usb2_device *udev)
{
	if (udev == NULL)
		return (NULL);		/* be NULL safe */
	return (udev->cdesc);
}

/*------------------------------------------------------------------------*
 *	usb2_test_quirk - test a device for a given quirk
 *
 * Return values:
 * 0: The USB device does not have the given quirk.
 * Else: The USB device has the given quirk.
 *------------------------------------------------------------------------*/
uint8_t
usb2_test_quirk(const struct usb2_attach_arg *uaa, uint16_t quirk)
{
	uint8_t found;

	found = (usb2_test_quirk_p) (&uaa->info, quirk);
	return (found);
}

struct usb2_interface_descriptor *
usb2_get_interface_descriptor(struct usb2_interface *iface)
{
	if (iface == NULL)
		return (NULL);		/* be NULL safe */
	return (iface->idesc);
}

uint8_t
usb2_get_interface_altindex(struct usb2_interface *iface)
{
	return (iface->alt_index);
}

uint8_t
usb2_get_bus_index(struct usb2_device *udev)
{
	return ((uint8_t)device_get_unit(udev->bus->bdev));
}

uint8_t
usb2_get_device_index(struct usb2_device *udev)
{
	return (udev->device_index);
}

/*------------------------------------------------------------------------*
 *	usb2_notify_addq
 *
 * This function will generate events for dev.
 *------------------------------------------------------------------------*/
static void
usb2_notify_addq(const char *type, struct usb2_device *udev)
{
	char *data = NULL;
	struct malloc_type *mt;

	mtx_lock(&malloc_mtx);
	mt = malloc_desc2type("bus");	/* XXX M_BUS */
	mtx_unlock(&malloc_mtx);
	if (mt == NULL)
		return;

	data = malloc(512, mt, M_NOWAIT);
	if (data == NULL)
		return;

	/* String it all together. */
	if (udev->parent_hub) {
		snprintf(data, 1024,
		    "%s"
		    "ugen%u.%u "
		    "vendor=0x%04x "
		    "product=0x%04x "
		    "devclass=0x%02x "
		    "devsubclass=0x%02x "
		    "sernum=\"%s\" "
		    "at "
		    "port=%u "
		    "on "
		    "ugen%u.%u\n",
		    type,
		    device_get_unit(udev->bus->bdev),
		    udev->device_index,
		    UGETW(udev->ddesc.idVendor),
		    UGETW(udev->ddesc.idProduct),
		    udev->ddesc.bDeviceClass,
		    udev->ddesc.bDeviceSubClass,
		    udev->serial,
		    udev->port_no,
		    device_get_unit(udev->bus->bdev),
		    udev->parent_hub->device_index);
	} else {
		snprintf(data, 1024,
		    "%s"
		    "ugen%u.%u "
		    "vendor=0x%04x "
		    "product=0x%04x "
		    "devclass=0x%02x "
		    "devsubclass=0x%02x "
		    "sernum=\"%s\" "
		    "at port=%u "
		    "on "
		    "%s\n",
		    type,
		    device_get_unit(udev->bus->bdev),
		    udev->device_index,
		    UGETW(udev->ddesc.idVendor),
		    UGETW(udev->ddesc.idProduct),
		    udev->ddesc.bDeviceClass,
		    udev->ddesc.bDeviceSubClass,
		    udev->serial,
		    udev->port_no,
		    device_get_nameunit(device_get_parent(udev->bus->bdev)));
	}
	devctl_queue_data(data);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_free_wrap
 *
 * The function will free the FIFOs.
 *------------------------------------------------------------------------*/
static void
usb2_fifo_free_wrap(struct usb2_device *udev,
    uint8_t iface_index, uint8_t free_all)
{
	struct usb2_fifo *f;
	struct usb2_pipe *pipe;
	uint16_t i;

	/*
	 * Free any USB FIFOs on the given interface:
	 */
	for (i = 0; i != USB_FIFO_MAX; i++) {
		f = udev->fifo[i];
		if (f == NULL) {
			continue;
		}
		pipe = f->priv_sc0;
		if ((pipe == &udev->default_pipe) && (free_all == 0)) {
			/* don't free UGEN control endpoint yet */
			continue;
		}
		/* Check if the interface index matches */
		if ((iface_index == f->iface_index) ||
		    (iface_index == USB_IFACE_INDEX_ANY)) {
			usb2_fifo_free(f);
		}
	}
	return;
}
