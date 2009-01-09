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

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_parse.h>

/*------------------------------------------------------------------------*
 *	usb2_desc_foreach
 *
 * This function is the safe way to iterate across the USB config
 * descriptor. It contains several checks against invalid
 * descriptors. If the "desc" argument passed to this function is
 * "NULL" the first descriptor, if any, will be returned.
 *
 * Return values:
 *   NULL: End of descriptors
 *   Else: Next descriptor after "desc"
 *------------------------------------------------------------------------*/
struct usb2_descriptor *
usb2_desc_foreach(struct usb2_config_descriptor *cd, struct usb2_descriptor *desc)
{
	void *end;

	if (cd == NULL) {
		return (NULL);
	}
	end = USB_ADD_BYTES(cd, UGETW(cd->wTotalLength));

	if (desc == NULL) {
		desc = USB_ADD_BYTES(cd, 0);
	} else {
		desc = USB_ADD_BYTES(desc, desc->bLength);
	}
	return (((((void *)desc) >= ((void *)cd)) &&
	    (((void *)desc) < end) &&
	    (USB_ADD_BYTES(desc, desc->bLength) >= ((void *)cd)) &&
	    (USB_ADD_BYTES(desc, desc->bLength) <= end) &&
	    (desc->bLength >= sizeof(*desc))) ? desc : NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_find_idesc
 *
 * This function will return the interface descriptor, if any, that
 * has index "iface_index" and alternate index "alt_index".
 *
 * Return values:
 *   NULL: End of descriptors
 *   Else: A valid interface descriptor
 *------------------------------------------------------------------------*/
struct usb2_interface_descriptor *
usb2_find_idesc(struct usb2_config_descriptor *cd,
    uint8_t iface_index, uint8_t alt_index)
{
	struct usb2_descriptor *desc = NULL;
	struct usb2_interface_descriptor *id;
	uint8_t curidx = 0;
	uint8_t lastidx = 0;
	uint8_t curaidx = 0;
	uint8_t first = 1;

	while ((desc = usb2_desc_foreach(cd, desc))) {
		if ((desc->bDescriptorType == UDESC_INTERFACE) &&
		    (desc->bLength >= sizeof(*id))) {
			id = (void *)desc;

			if (first) {
				first = 0;
				lastidx = id->bInterfaceNumber;

			} else if (id->bInterfaceNumber != lastidx) {

				lastidx = id->bInterfaceNumber;
				curidx++;
				curaidx = 0;

			} else {
				curaidx++;
			}

			if ((iface_index == curidx) && (alt_index == curaidx)) {
				return (id);
			}
		}
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_find_edesc
 *
 * This function will return the endpoint descriptor for the passed
 * interface index, alternate index and endpoint index.
 *
 * Return values:
 *   NULL: End of descriptors
 *   Else: A valid endpoint descriptor
 *------------------------------------------------------------------------*/
struct usb2_endpoint_descriptor *
usb2_find_edesc(struct usb2_config_descriptor *cd,
    uint8_t iface_index, uint8_t alt_index, uint8_t ep_index)
{
	struct usb2_descriptor *desc = NULL;
	struct usb2_interface_descriptor *d;
	uint8_t curidx = 0;

	d = usb2_find_idesc(cd, iface_index, alt_index);
	if (d == NULL)
		return (NULL);

	if (ep_index >= d->bNumEndpoints)	/* quick exit */
		return (NULL);

	desc = ((void *)d);

	while ((desc = usb2_desc_foreach(cd, desc))) {

		if (desc->bDescriptorType == UDESC_INTERFACE) {
			break;
		}
		if (desc->bDescriptorType == UDESC_ENDPOINT) {
			if (curidx == ep_index) {
				if (desc->bLength <
				    sizeof(struct usb2_endpoint_descriptor)) {
					/* endpoint index is invalid */
					break;
				}
				return ((void *)desc);
			}
			curidx++;
		}
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_get_no_endpoints
 *
 * This function will count the total number of endpoints available.
 *------------------------------------------------------------------------*/
uint16_t
usb2_get_no_endpoints(struct usb2_config_descriptor *cd)
{
	struct usb2_descriptor *desc = NULL;
	uint16_t count = 0;

	while ((desc = usb2_desc_foreach(cd, desc))) {
		if (desc->bDescriptorType == UDESC_ENDPOINT) {
			count++;
		}
	}
	return (count);
}

/*------------------------------------------------------------------------*
 *	usb2_get_no_alts
 *
 * Return value:
 *   Number of alternate settings for the given "ifaceno".
 *
 * NOTE: The returned can be larger than the actual number of
 * alternate settings.
 *------------------------------------------------------------------------*/
uint16_t
usb2_get_no_alts(struct usb2_config_descriptor *cd, uint8_t ifaceno)
{
	struct usb2_descriptor *desc = NULL;
	struct usb2_interface_descriptor *id;
	uint16_t n = 0;

	while ((desc = usb2_desc_foreach(cd, desc))) {

		if ((desc->bDescriptorType == UDESC_INTERFACE) &&
		    (desc->bLength >= sizeof(*id))) {
			id = (void *)desc;
			if (id->bInterfaceNumber == ifaceno) {
				n++;
			}
		}
	}
	return (n);
}
