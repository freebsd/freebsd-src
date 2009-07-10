/* $FreeBSD$ */
/*-
 * Copyright (c) 2009 Sylvestre Gallon. All rights reserved.
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

#include <sys/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "libusb20.h"
#include "libusb20_desc.h"
#include "libusb20_int.h"
#include "libusb.h"
#include "libusb10.h"

/* USB descriptors */

int
libusb_get_device_descriptor(libusb_device * dev,
    struct libusb_device_descriptor *desc)
{
	struct LIBUSB20_DEVICE_DESC_DECODED *pdesc;
	struct libusb20_device *pdev;
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device_descriptor enter");

	if ((dev == NULL) || (desc == NULL))
		return (LIBUSB_ERROR_INVALID_PARAM);

	pdev = dev->os_priv;
	pdesc = libusb20_dev_get_device_desc(pdev);

	desc->bLength = pdesc->bLength;
	desc->bDescriptorType = pdesc->bDescriptorType;
	desc->bcdUSB = pdesc->bcdUSB;
	desc->bDeviceClass = pdesc->bDeviceClass;
	desc->bDeviceSubClass = pdesc->bDeviceSubClass;
	desc->bDeviceProtocol = pdesc->bDeviceProtocol;
	desc->bMaxPacketSize0 = pdesc->bMaxPacketSize0;
	desc->idVendor = pdesc->idVendor;
	desc->idProduct = pdesc->idProduct;
	desc->bcdDevice = pdesc->bcdDevice;
	desc->iManufacturer = pdesc->iManufacturer;
	desc->iProduct = pdesc->iProduct;
	desc->iSerialNumber = pdesc->iSerialNumber;
	desc->bNumConfigurations = pdesc->bNumConfigurations;

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device_descriptor leave");
	return (0);
}

int
libusb_get_active_config_descriptor(libusb_device * dev,
    struct libusb_config_descriptor **config)
{
	struct libusb20_device *pdev;
	libusb_context *ctx;
	uint8_t idx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_active_config_descriptor enter");

	pdev = dev->os_priv;
	idx = libusb20_dev_get_config_index(pdev);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_active_config_descriptor leave");
	return (libusb_get_config_descriptor(dev, idx, config));
}

/*
 * XXX Need to check if extra need a dup because
 * XXX free pconf could free this char *
 */
int
libusb_get_config_descriptor(libusb_device * dev, uint8_t config_index,
    struct libusb_config_descriptor **config)
{
	struct libusb20_device *pdev;
	struct libusb20_config *pconf;
	struct libusb20_interface *pinf;
	struct libusb20_endpoint *pend;
	libusb_interface_descriptor *ifd;
	libusb_endpoint_descriptor *endd;
	libusb_context *ctx;
	uint8_t nif, nend, nalt, i, j, k;
	uint32_t if_idx, endp_idx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_config_descriptor enter");

	if (dev == NULL || config == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pdev = dev->os_priv;
	pconf = libusb20_dev_alloc_config(pdev, config_index);

	if (pconf == NULL)
		return (LIBUSB_ERROR_NOT_FOUND);

	nalt = nif = pconf->num_interface;
	nend = 0;
	for (i = 0 ; i < nif ; i++) {
		if (pconf->interface[i].num_altsetting > 0)
		{
			nalt += pconf->interface[i].num_altsetting;
			for (j = 0 ; j < nalt ; j++) {
				nend += pconf->interface[i].altsetting[j].num_endpoints;
			}
		}
		nend += pconf->interface[i].num_endpoints;
	}

	*config = malloc(sizeof(libusb_config_descriptor) + 
	    (nif * sizeof(libusb_interface)) +
	    (nalt * sizeof(libusb_interface_descriptor)) +
	    (nend * sizeof(libusb_endpoint_descriptor)));
	if (*config == NULL) {
		free(pconf);
		return (LIBUSB_ERROR_NO_MEM);
	}

	(*config)->interface = (libusb_interface *)(*config + 
	    sizeof(libusb_config_descriptor));
	for (i = if_idx = endp_idx = 0 ; i < nif ; if_idx, i++) {
		(*config)->interface[i].altsetting = (libusb_interface_descriptor *)
		    (*config + sizeof(libusb_config_descriptor) +
		    (nif * sizeof(libusb_interface)) +
		    (if_idx * sizeof(libusb_interface_descriptor)));
		(*config)->interface[i].altsetting[0].endpoint = 
		    (libusb_endpoint_descriptor *) (*config +
	   	    sizeof(libusb_config_descriptor) +
		    (nif * sizeof(libusb_interface)) +
		    (nalt * sizeof(libusb_interface_descriptor)) +
		    (endp_idx * sizeof(libusb_endpoint_descriptor)));
		    endp_idx += pconf->interface[i].num_endpoints;

		if (pconf->interface[i].num_altsetting > 0)
		{
			for (j = 0 ; j < pconf->interface[i].num_altsetting ; j++, if_idx++) {
				(*config)->interface[i].altsetting[j + 1].endpoint = 
		    		(libusb_endpoint_descriptor *) (*config +
	   	    		sizeof(libusb_config_descriptor) +
		    		(nif * sizeof(libusb_interface)) +
		    		(nalt * sizeof(libusb_interface_descriptor)) +
		    		(endp_idx * sizeof(libusb_endpoint_descriptor)));
				endp_idx += pconf->interface[i].altsetting[j].num_endpoints;
			}
		}
	}

	(*config)->bLength = pconf->desc.bLength;
	(*config)->bDescriptorType = pconf->desc.bDescriptorType;
	(*config)->wTotalLength = pconf->desc.wTotalLength;
	(*config)->bNumInterfaces = pconf->desc.bNumInterfaces;
	(*config)->bConfigurationValue = pconf->desc.bConfigurationValue;
	(*config)->iConfiguration = pconf->desc.iConfiguration;
	(*config)->bmAttributes = pconf->desc.bmAttributes;
	(*config)->MaxPower = pconf->desc.bMaxPower;
	(*config)->extra_length = pconf->extra.len;
	if ((*config)->extra_length != 0)
		(*config)->extra = pconf->extra.ptr;

	for (i = 0 ; i < nif ; i++) {
		pinf = &pconf->interface[i];
		(*config)->interface[i].num_altsetting = pinf->num_altsetting + 1;
		for (j = 0 ; j < (*config)->interface[i].num_altsetting ; j++) {
			if (j != 0)
				pinf = &pconf->interface[i].altsetting[j - 1];
			ifd = &(*config)->interface[i].altsetting[j];
			ifd->bLength = pinf->desc.bLength;
			ifd->bDescriptorType = pinf->desc.bDescriptorType;
			ifd->bInterfaceNumber = pinf->desc.bInterfaceNumber;
			ifd->bAlternateSetting = pinf->desc.bAlternateSetting;
			ifd->bNumEndpoints = pinf->desc.bNumEndpoints;
			ifd->bInterfaceClass = pinf->desc.bInterfaceClass;
			ifd->bInterfaceSubClass = pinf->desc.bInterfaceSubClass;
			ifd->bInterfaceProtocol = pinf->desc.bInterfaceProtocol;
			ifd->iInterface = pinf->desc.iInterface;
			ifd->extra_length = pinf->extra.len;
			if (ifd->extra_length != 0)
				ifd->extra = pinf->extra.ptr;
			for (k = 0 ; k < pinf->num_endpoints ; k++) {
				pend = &pinf->endpoints[k];
				endd = &ifd->endpoint[k];
				endd->bLength = pend->desc.bLength;
				endd->bDescriptorType = pend->desc.bDescriptorType;
				endd->bEndpointAddress = pend->desc.bEndpointAddress;
				endd->bmAttributes = pend->desc.bmAttributes;
				endd->wMaxPacketSize = pend->desc.wMaxPacketSize;
				endd->bInterval = pend->desc.bInterval;
				endd->bRefresh = pend->desc.bRefresh;
				endd->bSynchAddress = pend->desc.bSynchAddress;
				endd->extra_length = pend->extra.len;
				if (endd->extra_length != 0)
					endd->extra = pend->extra.ptr;
			}
		}	
	}

	free(pconf);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_config_descriptor leave");
	return (0);
}

int
libusb_get_config_descriptor_by_value(libusb_device * dev,
    uint8_t bConfigurationValue, struct libusb_config_descriptor **config)
{
	struct LIBUSB20_DEVICE_DESC_DECODED *pdesc;
	struct libusb20_device *pdev;
	struct libusb20_config *pconf;
	libusb_context *ctx;
	int i;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_config_descriptor_by_value enter");

	if (dev == NULL || config == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);
	
	pdev = dev->os_priv;
	pdesc = libusb20_dev_get_device_desc(pdev);

	for (i = 0 ; i < pdesc->bNumConfigurations ; i++) {
		pconf = libusb20_dev_alloc_config(pdev, i);
	       	if (pconf->desc.bConfigurationValue == bConfigurationValue) {
			free(pconf);
			return libusb_get_config_descriptor(dev, i, config);	

		}
		free(pconf);
	}

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_config_descriptor_by_value leave");
	return (LIBUSB_ERROR_NOT_FOUND);
}

void
libusb_free_config_descriptor(struct libusb_config_descriptor *config)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_free_config_descriptor enter");

	free(config);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_free_config_descriptor leave");
}

int
libusb_get_string_descriptor_ascii(libusb_device_handle * dev,
    uint8_t desc_index, unsigned char *data, int length)
{
	struct libusb20_device *pdev;
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_string_descriptor_ascii enter");

	if (dev == NULL || data == NULL)
		return (LIBUSB20_ERROR_INVALID_PARAM);

	pdev = dev->os_priv;
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_string_descriptor_ascii leave");
	if (libusb20_dev_req_string_simple_sync(pdev, desc_index, 
	    data, length) == 0)
		return (strlen(data));

	return (LIBUSB_ERROR_OTHER);
}
