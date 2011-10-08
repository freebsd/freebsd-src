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

#include <stdio.h>
#include <stdlib.h>

#define	libusb_device_handle libusb20_device

#include "libusb20.h"
#include "libusb20_desc.h"
#include "libusb20_int.h"
#include "libusb.h"
#include "libusb10.h"

#define	N_ALIGN(n) (-((-(n)) & (-8UL)))

/* USB descriptors */

int
libusb_get_device_descriptor(libusb_device *dev,
    struct libusb_device_descriptor *desc)
{
	struct LIBUSB20_DEVICE_DESC_DECODED *pdesc;
	struct libusb20_device *pdev;

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

	return (0);
}

int
libusb_get_active_config_descriptor(libusb_device *dev,
    struct libusb_config_descriptor **config)
{
	struct libusb20_device *pdev;
	uint8_t config_index;

	pdev = dev->os_priv;
	config_index = libusb20_dev_get_config_index(pdev);

	return (libusb_get_config_descriptor(dev, config_index, config));
}

int
libusb_get_config_descriptor(libusb_device *dev, uint8_t config_index,
    struct libusb_config_descriptor **config)
{
	struct libusb20_device *pdev;
	struct libusb20_config *pconf;
	struct libusb20_interface *pinf;
	struct libusb20_endpoint *pend;
	struct libusb_config_descriptor *pconfd;
	struct libusb_interface_descriptor *ifd;
	struct libusb_endpoint_descriptor *endd;
	uint8_t *pextra;
	uint16_t nextra;
	uint8_t nif;
	uint8_t nep;
	uint8_t nalt;
	uint8_t i;
	uint8_t j;
	uint8_t k;

	if (dev == NULL || config == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	*config = NULL;

	pdev = dev->os_priv;
	pconf = libusb20_dev_alloc_config(pdev, config_index);

	if (pconf == NULL)
		return (LIBUSB_ERROR_NOT_FOUND);

	nalt = nif = pconf->num_interface;
	nep = 0;
	nextra = N_ALIGN(pconf->extra.len);

	for (i = 0; i < nif; i++) {

		pinf = pconf->interface + i;
		nextra += N_ALIGN(pinf->extra.len);
		nep += pinf->num_endpoints;
		k = pinf->num_endpoints;
		pend = pinf->endpoints;
		while (k--) {
			nextra += N_ALIGN(pend->extra.len);
			pend++;
		}

		j = pinf->num_altsetting;
		nalt += pinf->num_altsetting;
		pinf = pinf->altsetting;
		while (j--) {
			nextra += N_ALIGN(pinf->extra.len);
			nep += pinf->num_endpoints;
			k = pinf->num_endpoints;
			pend = pinf->endpoints;
			while (k--) {
				nextra += N_ALIGN(pend->extra.len);
				pend++;
			}
			pinf++;
		}
	}

	nextra = nextra +
	    (1 * sizeof(libusb_config_descriptor)) +
	    (nif * sizeof(libusb_interface)) +
	    (nalt * sizeof(libusb_interface_descriptor)) +
	    (nep * sizeof(libusb_endpoint_descriptor));

	nextra = N_ALIGN(nextra);

	pconfd = malloc(nextra);

	if (pconfd == NULL) {
		free(pconf);
		return (LIBUSB_ERROR_NO_MEM);
	}
	/* make sure memory is initialised */
	memset(pconfd, 0, nextra);

	pconfd->interface = (libusb_interface *) (pconfd + 1);

	ifd = (libusb_interface_descriptor *) (pconfd->interface + nif);
	endd = (libusb_endpoint_descriptor *) (ifd + nalt);
	pextra = (uint8_t *)(endd + nep);

	/* fill in config descriptor */

	pconfd->bLength = pconf->desc.bLength;
	pconfd->bDescriptorType = pconf->desc.bDescriptorType;
	pconfd->wTotalLength = pconf->desc.wTotalLength;
	pconfd->bNumInterfaces = pconf->desc.bNumInterfaces;
	pconfd->bConfigurationValue = pconf->desc.bConfigurationValue;
	pconfd->iConfiguration = pconf->desc.iConfiguration;
	pconfd->bmAttributes = pconf->desc.bmAttributes;
	pconfd->MaxPower = pconf->desc.bMaxPower;

	if (pconf->extra.len != 0) {
		pconfd->extra_length = pconf->extra.len;
		pconfd->extra = pextra;
		memcpy(pextra, pconf->extra.ptr, pconfd->extra_length);
		pextra += N_ALIGN(pconfd->extra_length);
	}
	/* setup all interface and endpoint pointers */

	for (i = 0; i < nif; i++) {

		pconfd->interface[i].altsetting = ifd;
		ifd->endpoint = endd;
		endd += pconf->interface[i].num_endpoints;
		ifd++;

		for (j = 0; j < pconf->interface[i].num_altsetting; j++) {
			ifd->endpoint = endd;
			endd += pconf->interface[i].altsetting[j].num_endpoints;
			ifd++;
		}
	}

	/* fill in all interface and endpoint data */

	for (i = 0; i < nif; i++) {
		pinf = &pconf->interface[i];
		pconfd->interface[i].num_altsetting = pinf->num_altsetting + 1;
		for (j = 0; j < pconfd->interface[i].num_altsetting; j++) {
			if (j != 0)
				pinf = &pconf->interface[i].altsetting[j - 1];
			ifd = &pconfd->interface[i].altsetting[j];
			ifd->bLength = pinf->desc.bLength;
			ifd->bDescriptorType = pinf->desc.bDescriptorType;
			ifd->bInterfaceNumber = pinf->desc.bInterfaceNumber;
			ifd->bAlternateSetting = pinf->desc.bAlternateSetting;
			ifd->bNumEndpoints = pinf->desc.bNumEndpoints;
			ifd->bInterfaceClass = pinf->desc.bInterfaceClass;
			ifd->bInterfaceSubClass = pinf->desc.bInterfaceSubClass;
			ifd->bInterfaceProtocol = pinf->desc.bInterfaceProtocol;
			ifd->iInterface = pinf->desc.iInterface;
			if (pinf->extra.len != 0) {
				ifd->extra_length = pinf->extra.len;
				ifd->extra = pextra;
				memcpy(pextra, pinf->extra.ptr, pinf->extra.len);
				pextra += N_ALIGN(pinf->extra.len);
			}
			for (k = 0; k < pinf->num_endpoints; k++) {
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
				if (pend->extra.len != 0) {
					endd->extra_length = pend->extra.len;
					endd->extra = pextra;
					memcpy(pextra, pend->extra.ptr, pend->extra.len);
					pextra += N_ALIGN(pend->extra.len);
				}
			}
		}
	}

	free(pconf);

	*config = pconfd;

	return (0);			/* success */
}

int
libusb_get_config_descriptor_by_value(libusb_device *dev,
    uint8_t bConfigurationValue, struct libusb_config_descriptor **config)
{
	struct LIBUSB20_DEVICE_DESC_DECODED *pdesc;
	struct libusb20_device *pdev;
	int i;
	int err;

	if (dev == NULL || config == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pdev = dev->os_priv;
	pdesc = libusb20_dev_get_device_desc(pdev);

	for (i = 0; i < pdesc->bNumConfigurations; i++) {
		err = libusb_get_config_descriptor(dev, i, config);
		if (err)
			return (err);

		if ((*config)->bConfigurationValue == bConfigurationValue)
			return (0);	/* success */

		libusb_free_config_descriptor(*config);
	}

	*config = NULL;

	return (LIBUSB_ERROR_NOT_FOUND);
}

void
libusb_free_config_descriptor(struct libusb_config_descriptor *config)
{
	free(config);
}

int
libusb_get_string_descriptor_ascii(libusb_device_handle *pdev,
    uint8_t desc_index, unsigned char *data, int length)
{
	if (pdev == NULL || data == NULL || length < 1)
		return (LIBUSB20_ERROR_INVALID_PARAM);

	if (length > 65535)
		length = 65535;

	/* put some default data into the destination buffer */
	data[0] = 0;

	if (libusb20_dev_req_string_simple_sync(pdev, desc_index,
	    data, length) == 0)
		return (strlen(data));

	return (LIBUSB_ERROR_OTHER);
}

int
libusb_get_descriptor(libusb_device_handle * devh, uint8_t desc_type, 
    uint8_t desc_index, uint8_t *data, int length)
{
	if (devh == NULL || data == NULL || length < 1)
		return (LIBUSB20_ERROR_INVALID_PARAM);

	if (length > 65535)
		length = 65535;

	return (libusb_control_transfer(devh, LIBUSB_ENDPOINT_IN,
	    LIBUSB_REQUEST_GET_DESCRIPTOR, (desc_type << 8) | desc_index, 0, data,
	    length, 1000));
}
