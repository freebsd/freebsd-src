/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 * Copyright (c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <fcntl.h>

#include <libusb20.h>
#include <libusb20_desc.h>

#include <dev/usb/usb_ioctl.h>

#include "dump.h"
#include "pathnames.h"

#ifndef IOUSB
#define IOUSB(a) a
#endif

#define	DUMP0(n,type,field,...) dump_field(pdev, "  ", #field, n->field);
#define	DUMP0L(n,type,field,...) dump_fieldl(pdev, "  ", #field, n->field);
#define	DUMP1(n,type,field,...) dump_field(pdev, "    ", #field, n->field);
#define	DUMP2(n,type,field,...) dump_field(pdev, "      ", #field, n->field);
#define	DUMP3(n,type,field,...) dump_field(pdev, "        ", #field, n->field);

struct usb_product_info {
	STAILQ_ENTRY(usb_product_info)	link;
	int				id;
	char				*desc;
};

struct usb_vendor_info {
	STAILQ_ENTRY(usb_vendor_info)	link;
	STAILQ_HEAD(,usb_product_info)	devs;
	int				id;
	char				*desc;
};

STAILQ_HEAD(usb_vendors, usb_vendor_info);

const char *
dump_mode(uint8_t value)
{
	if (value == LIBUSB20_MODE_HOST)
		return ("HOST");
	return ("DEVICE");
}

const char *
dump_speed(uint8_t value)
{
	;				/* style fix */
	switch (value) {
	case LIBUSB20_SPEED_LOW:
		return ("LOW (1.5Mbps)");
	case LIBUSB20_SPEED_FULL:
		return ("FULL (12Mbps)");
	case LIBUSB20_SPEED_HIGH:
		return ("HIGH (480Mbps)");
	case LIBUSB20_SPEED_VARIABLE:
		return ("VARIABLE (52-480Mbps)");
	case LIBUSB20_SPEED_SUPER:
		return ("SUPER (5.0Gbps)");
	default:
		break;
	}
	return ("UNKNOWN ()");
}

const char *
dump_power_mode(uint8_t value)
{
	;				/* style fix */
	switch (value) {
	case LIBUSB20_POWER_OFF:
		return ("OFF");
	case LIBUSB20_POWER_ON:
		return ("ON");
	case LIBUSB20_POWER_SAVE:
		return ("SAVE");
	case LIBUSB20_POWER_SUSPEND:
		return ("SUSPEND");
	case LIBUSB20_POWER_RESUME:
		return ("RESUME");
	default:
		return ("UNKNOWN");
	}
}

static void
_dump_field(struct libusb20_device *pdev, const char *plevel,
    const char *field, uint32_t value, bool list_mode)
{
	uint8_t temp_string[256];

	if (list_mode) {
		/* Skip fields we are not interested in. */
		if (strcmp(field, "bLength") == 0 ||
		    strcmp(field, "bDescriptorType") == 0 ||
		    strcmp(field, "bMaxPacketSize0") == 0)
			return;

		printf("%s=%#06x ", field, value);
		return;
	}
	printf("%s%s = 0x%04x ", plevel, field, value);

	if (strlen(plevel) == 8) {
		/* Endpoint Descriptor */

		if (strcmp(field, "bEndpointAddress") == 0) {
			if (value & 0x80)
				printf(" <IN>\n");
			else
				printf(" <OUT>\n");
			return;
		}
		if (strcmp(field, "bmAttributes") == 0) {
			switch (value & 0x03) {
			case 0:
				printf(" <CONTROL>\n");
				break;
			case 1:
				switch (value & 0x0C) {
				case 0x00:
					printf(" <ISOCHRONOUS>\n");
					break;
				case 0x04:
					printf(" <ASYNC-ISOCHRONOUS>\n");
					break;
				case 0x08:
					printf(" <ADAPT-ISOCHRONOUS>\n");
					break;
				default:
					printf(" <SYNC-ISOCHRONOUS>\n");
					break;
				}
				break;
			case 2:
				printf(" <BULK>\n");
				break;
			default:
				printf(" <INTERRUPT>\n");
				break;
			}
			return;
		}
	}
	if ((field[0] == 'i') && (field[1] != 'd')) {
		/* Indirect String Descriptor */
		if (value == 0) {
			printf(" <no string>\n");
			return;
		}
		if (libusb20_dev_req_string_simple_sync(pdev, value,
		    temp_string, sizeof(temp_string))) {
			printf(" <retrieving string failed>\n");
			return;
		}
		printf(" <%s>\n", temp_string);
		return;
	}
	if (strlen(plevel) == 2 || strlen(plevel) == 6) {

		/* Device and Interface Descriptor class codes */

		if (strcmp(field, "bInterfaceClass") == 0 ||
		    strcmp(field, "bDeviceClass") == 0) {

			switch (value) {
			case 0x00:
				printf(" <Probed by interface class>\n");
				break;
			case 0x01:
				printf(" <Audio device>\n");
				break;
			case 0x02:
				printf(" <Communication device>\n");
				break;
			case 0x03:
				printf(" <HID device>\n");
				break;
			case 0x05:
				printf(" <Physical device>\n");
				break;
			case 0x06:
				printf(" <Still imaging>\n");
				break;
			case 0x07:
				printf(" <Printer device>\n");
				break;
			case 0x08:
				printf(" <Mass storage>\n");
				break;
			case 0x09:
				printf(" <HUB>\n");
				break;
			case 0x0A:
				printf(" <CDC-data>\n");
				break;
			case 0x0B:
				printf(" <Smart card>\n");
				break;
			case 0x0D:
				printf(" <Content security>\n");
				break;
			case 0x0E:
				printf(" <Video device>\n");
				break;
			case 0x0F:
				printf(" <Personal healthcare>\n");
				break;
			case 0x10:
				printf(" <Audio and video device>\n");
				break;
			case 0x11:
				printf(" <Billboard device>\n");
				break;
			case 0xDC:
				printf(" <Diagnostic device>\n");
				break;
			case 0xE0:
				printf(" <Wireless controller>\n");
				break;
			case 0xEF:
				printf(" <Miscellaneous device>\n");
				break;
			case 0xFE:
				printf(" <Application specific>\n");
				break;
			case 0xFF:
				printf(" <Vendor specific>\n");
				break;
			default:
				printf(" <Unknown>\n");
				break;
			}
			return;
		}
	}
	/* No additional information */
	printf("\n");
}

static void
dump_field(struct libusb20_device *pdev, const char *plevel,
    const char *field, uint32_t value)
{
	_dump_field(pdev, plevel, field, value, false);
}

static void
dump_fieldl(struct libusb20_device *pdev, const char *plevel,
    const char *field, uint32_t value)
{
	_dump_field(pdev, plevel, field, value, true);
}

static void
dump_extra(struct libusb20_me_struct *str, const char *plevel)
{
	const uint8_t *ptr;
	uint8_t x;

	ptr = NULL;

	while ((ptr = libusb20_desc_foreach(str, ptr))) {
		printf("\n" "%sAdditional Descriptor\n\n", plevel);
		printf("%sbLength = 0x%02x\n", plevel, ptr[0]);
		printf("%sbDescriptorType = 0x%02x\n", plevel, ptr[1]);
		if (ptr[0] > 1)
			printf("%sbDescriptorSubType = 0x%02x\n",
			    plevel, ptr[2]);
		printf("%s RAW dump: ", plevel);
		for (x = 0; x != ptr[0]; x++) {
			if ((x % 8) == 0) {
				printf("\n%s 0x%02x | ", plevel, x);
			}
			printf("0x%02x%s", ptr[x],
			    (x != (ptr[0] - 1)) ? ", " : (x % 8) ? "\n" : "");
		}
		printf("\n");
	}
}

static void
dump_endpoint(struct libusb20_device *pdev,
    struct libusb20_endpoint *ep)
{
	struct LIBUSB20_ENDPOINT_DESC_DECODED *edesc;

	edesc = &ep->desc;
	LIBUSB20_ENDPOINT_DESC(DUMP3, edesc);
	dump_extra(&ep->extra, "  " "  " "  ");
}

static void
dump_iface(struct libusb20_device *pdev,
    struct libusb20_interface *iface)
{
	struct LIBUSB20_INTERFACE_DESC_DECODED *idesc;
	uint8_t z;

	idesc = &iface->desc;
	LIBUSB20_INTERFACE_DESC(DUMP2, idesc);
	dump_extra(&iface->extra, "  " "  " "  ");

	for (z = 0; z != iface->num_endpoints; z++) {
		printf("\n     Endpoint %u\n", z);
		dump_endpoint(pdev, iface->endpoints + z);
	}
}

static struct usb_vendors *
load_vendors(void)
{
	const char *dbf;
	FILE *db = NULL;
	struct usb_vendor_info *cv;
	struct usb_product_info *cd;
	struct usb_vendors *usb_vendors;
	char buf[1024], str[1024];
	char *ch;
	int id;

	usb_vendors = malloc(sizeof(*usb_vendors));
	if (usb_vendors == NULL)
		err(1, "out of memory");
	STAILQ_INIT(usb_vendors);
	if ((dbf = getenv("USB_VENDOR_DATABASE")) != NULL)
		db = fopen(dbf, "r");
	if (db == NULL) {
		dbf = _PATH_LUSBVDB;
		if ((db = fopen(dbf, "r")) == NULL) {
			dbf = _PATH_USBVDB;
			if ((db = fopen(dbf, "r")) == NULL)
				return (usb_vendors);
		}
	}
	cv = NULL;
	cd = NULL;

	for (;;) {
		if (fgets(buf, sizeof(buf), db) == NULL)
			break;

		if ((ch = strchr(buf, '#')) != NULL)
			*ch = '\0';
		if (ch == buf)
			continue;
		ch = strchr(buf, '\0') - 1;
		while (ch > buf && isspace(*ch))
			*ch-- = '\0';
		if (ch <= buf)
			continue;

		/* Can't handle subvendor / subdevice entries yet */
		if (buf[0] == '\t' && buf[1] == '\t')
			continue;

		/* Check for vendor entry */
		if (buf[0] != '\t' && sscanf(buf, "%04x %[^\n]", &id, str) == 2) {
			if ((id == 0) || (strlen(str) < 1))
				continue;
			if ((cv = malloc(sizeof(struct usb_vendor_info))) == NULL)
				err(1, "out of memory");
			if ((cv->desc = strdup(str)) == NULL)
				err(1, "out of memory");
			cv->id = id;
			STAILQ_INIT(&cv->devs);
			STAILQ_INSERT_TAIL(usb_vendors, cv, link);
			continue;
		}

		/* Check for device entry */
		if (buf[0] == '\t' && sscanf(buf + 1, "%04x %[^\n]", &id, str) == 2) {
			if ((id == 0) || (strlen(str) < 1))
				continue;
			if (cv == NULL)
				continue;
			if ((cd = malloc(sizeof(struct usb_product_info))) == NULL)
				err(1, "out of memory");
			if ((cd->desc = strdup(str)) == NULL)
				err(1, "out of memory");
			cd->id = id;
			STAILQ_INSERT_TAIL(&cv->devs, cd, link);
			continue;
		}
	}
	if (ferror(db))
		err(1, "error reading the usb id db");

	fclose(db);
	/* cleanup */
	return (usb_vendors);
}

enum _device_descr_list_type {
	_DEVICE_DESCR_LIST_TYPE_DEFAULT		= 0,
	_DEVICE_DESCR_LIST_TYPE_UGEN		= 1,
	_DEVICE_DESCR_LIST_TYPE_PRODUCT_VENDOR	= 2,
};

static char *
_device_desc(struct libusb20_device *pdev,
    enum _device_descr_list_type list_type)
{
	static struct usb_vendors *usb_vendors = NULL;
	char *desc = NULL;
	const char *vendor = NULL, *product = NULL;
	uint16_t vid;
	uint16_t pid;
	struct usb_vendor_info *vi;
	struct usb_product_info *pi;
	struct usb_device_info devinfo;

	if (list_type == _DEVICE_DESCR_LIST_TYPE_UGEN) {
		asprintf(&desc, "ugen%u.%u",
				libusb20_dev_get_bus_number(pdev),
				libusb20_dev_get_address(pdev));
		return (desc);
	}

	vid = libusb20_dev_get_device_desc(pdev)->idVendor;
	pid = libusb20_dev_get_device_desc(pdev)->idProduct;

	if (usb_vendors == NULL)
		usb_vendors = load_vendors();

	STAILQ_FOREACH(vi, usb_vendors, link) {
		if (vi->id == vid) {
			vendor = vi->desc;
			break;
		}
	}
	if (vi != NULL) {
		STAILQ_FOREACH(pi, &vi->devs, link) {
			if (pi->id == pid) {
				product = pi->desc;
				break;
			}
		}
	}

	/*
	 * Try to gather the information; libusb2 unfortunately seems to
	 * only build an entire string but not save vendor/product individually.
	 */
	if (vendor == NULL || product == NULL) {
		char buf[64];
		int f;

		snprintf(buf, sizeof(buf), "/dev/" USB_GENERIC_NAME "%u.%u",
		    libusb20_dev_get_bus_number(pdev),
		    libusb20_dev_get_address(pdev));

		f = open(buf, O_RDWR);
		if (f < 0)
			goto skip_vp_recovery;

		if (ioctl(f, IOUSB(USB_GET_DEVICEINFO), &devinfo))
			goto skip_vp_recovery;


		if (vendor == NULL)
			vendor = devinfo.udi_vendor;
		if (product == NULL)
			product = devinfo.udi_product;

skip_vp_recovery:
		if (f >= 0)
			close(f);
	}

	if (list_type == _DEVICE_DESCR_LIST_TYPE_PRODUCT_VENDOR) {
		asprintf(&desc, "vendor='%s' product='%s'",
		    (vendor != NULL) ? vendor : "",
		    (product != NULL) ? product : "");
		return (desc);
	}

	if (vendor == NULL || product == NULL)
		return (NULL);

	asprintf(&desc, "ugen%u.%u: <%s %s> at usbus%u",
			libusb20_dev_get_bus_number(pdev),
			libusb20_dev_get_address(pdev),
			product, vendor,
			libusb20_dev_get_bus_number(pdev));

	return (desc);
}

void
dump_device_info(struct libusb20_device *pdev, uint8_t show_ifdrv,
    bool list_mode)
{
	char buf[128];
	uint8_t n;
	unsigned int usage;
	char *desc;

	usage = libusb20_dev_get_power_usage(pdev);

	desc = _device_desc(pdev, (list_mode) ? _DEVICE_DESCR_LIST_TYPE_UGEN :
	    _DEVICE_DESCR_LIST_TYPE_DEFAULT);

	if (list_mode)
		printf("%s: ", desc);
	else
		printf("%s, cfg=%u md=%s spd=%s pwr=%s (%umA)\n",
		    desc ? desc : libusb20_dev_get_desc(pdev),
		    libusb20_dev_get_config_index(pdev),
		    dump_mode(libusb20_dev_get_mode(pdev)),
		    dump_speed(libusb20_dev_get_speed(pdev)),
		    dump_power_mode(libusb20_dev_get_power_mode(pdev)),
		    usage);
	free(desc);

	if (list_mode || !show_ifdrv)
		return;

	for (n = 0; n != 255; n++) {
		if (libusb20_dev_get_iface_desc(pdev, n, buf, sizeof(buf)))
			break;
		if (buf[0] == 0)
			continue;
		printf("ugen%u.%u.%u: %s\n",
		    libusb20_dev_get_bus_number(pdev),
		    libusb20_dev_get_address(pdev), n, buf);
	}
}

void
dump_be_quirk_names(struct libusb20_backend *pbe)
{
	struct libusb20_quirk q;
	uint16_t x;
	int error;

	memset(&q, 0, sizeof(q));

	printf("\nDumping list of supported quirks:\n\n");

	for (x = 0; x != 0xFFFF; x++) {

		error = libusb20_be_get_quirk_name(pbe, x, &q);
		if (error) {
			if (x == 0) {
				printf("No quirk names - maybe the USB quirk "
				    "module has not been loaded.\n");
			}
			break;
		}
		if (strcmp(q.quirkname, "UQ_NONE"))
			printf("%s\n", q.quirkname);
	}
	printf("\n");
}

void
dump_be_dev_quirks(struct libusb20_backend *pbe)
{
	struct libusb20_quirk q;
	uint16_t x;
	int error;

	memset(&q, 0, sizeof(q));

	printf("\nDumping current device quirks:\n\n");

	for (x = 0; x != 0xFFFF; x++) {

		error = libusb20_be_get_dev_quirk(pbe, x, &q);
		if (error) {
			if (x == 0) {
				printf("No device quirks - maybe the USB quirk "
				    "module has not been loaded.\n");
			}
			break;
		}
		if (strcmp(q.quirkname, "UQ_NONE")) {
			printf("VID=0x%04x PID=0x%04x REVLO=0x%04x "
			    "REVHI=0x%04x QUIRK=%s\n",
			    q.vid, q.pid, q.bcdDeviceLow,
			    q.bcdDeviceHigh, q.quirkname);
		}
	}
	printf("\n");
}

void
dump_device_desc(struct libusb20_device *pdev, bool list_mode)
{
	struct LIBUSB20_DEVICE_DESC_DECODED *ddesc;

	ddesc = libusb20_dev_get_device_desc(pdev);
	if (list_mode) {
		char *desc;

		LIBUSB20_DEVICE_DESC(DUMP0L, ddesc);
		desc = _device_desc(pdev, _DEVICE_DESCR_LIST_TYPE_PRODUCT_VENDOR);
		printf("%s\n", (desc != NULL) ? desc : "");
		free(desc);
	} else {
		LIBUSB20_DEVICE_DESC(DUMP0, ddesc);
	}
}

void
dump_config(struct libusb20_device *pdev, uint8_t all_cfg)
{
	struct LIBUSB20_CONFIG_DESC_DECODED *cdesc;
	struct LIBUSB20_DEVICE_DESC_DECODED *ddesc;
	struct libusb20_config *pcfg = NULL;
	uint8_t cfg_index;
	uint8_t cfg_index_end;
	uint8_t x;
	uint8_t y;

	ddesc = libusb20_dev_get_device_desc(pdev);

	if (all_cfg) {
		cfg_index = 0;
		cfg_index_end = ddesc->bNumConfigurations;
	} else {
		cfg_index = libusb20_dev_get_config_index(pdev);
		cfg_index_end = cfg_index + 1;
	}

	for (; cfg_index != cfg_index_end; cfg_index++) {

		pcfg = libusb20_dev_alloc_config(pdev, cfg_index);
		if (!pcfg) {
			continue;
		}
		printf("\n Configuration index %u\n\n", cfg_index);
		cdesc = &(pcfg->desc);
		LIBUSB20_CONFIG_DESC(DUMP1, cdesc);
		dump_extra(&(pcfg->extra), "  " "  ");

		for (x = 0; x != pcfg->num_interface; x++) {
			printf("\n    Interface %u\n", x);
			dump_iface(pdev, pcfg->interface + x);
			printf("\n");
			for (y = 0; y != (pcfg->interface + x)->num_altsetting; y++) {
				printf("\n    Interface %u Alt %u\n", x, y + 1);
				dump_iface(pdev,
				    (pcfg->interface + x)->altsetting + y);
				printf("\n");
			}
		}
		printf("\n");
		free(pcfg);
	}
}

void
dump_string_by_index(struct libusb20_device *pdev, uint8_t str_index)
{
	char *pbuf;
	uint8_t n;
	uint8_t len;

	pbuf = malloc(256);
	if (pbuf == NULL)
		err(1, "out of memory");

	if (str_index == 0) {
		/* language table */
		if (libusb20_dev_req_string_sync(pdev,
		    str_index, 0, pbuf, 256)) {
			printf("STRING_0x%02x = <read error>\n", str_index);
		} else {
			printf("STRING_0x%02x = ", str_index);
			len = (uint8_t)pbuf[0];
			for (n = 0; n != len; n++) {
				printf("0x%02x%s", (uint8_t)pbuf[n],
				    (n != (len - 1)) ? ", " : "");
			}
			printf("\n");
		}
	} else {
		/* ordinary string */
		if (libusb20_dev_req_string_simple_sync(pdev,
		    str_index, pbuf, 256)) {
			printf("STRING_0x%02x = <read error>\n", str_index);
		} else {
			printf("STRING_0x%02x = <%s>\n", str_index, pbuf);
		}
	}
	free(pbuf);
}

void
dump_device_stats(struct libusb20_device *pdev)
{
	struct libusb20_device_stats st;

	if (libusb20_dev_get_stats(pdev, &st)) {
		printf("{}\n");
	} else {
		printf("{\n"
		    "    UE_CONTROL_OK       : %llu\n"
		    "    UE_ISOCHRONOUS_OK   : %llu\n"
		    "    UE_BULK_OK          : %llu\n"
		    "    UE_INTERRUPT_OK     : %llu\n"
		    "    UE_CONTROL_FAIL     : %llu\n"
		    "    UE_ISOCHRONOUS_FAIL : %llu\n"
		    "    UE_BULK_FAIL        : %llu\n"
		    "    UE_INTERRUPT_FAIL   : %llu\n"
		    "}\n",
		    (unsigned long long)st.xfer_ok[0],
		    (unsigned long long)st.xfer_ok[1],
		    (unsigned long long)st.xfer_ok[2],
	            (unsigned long long)st.xfer_ok[3],
		    (unsigned long long)st.xfer_fail[0],
		    (unsigned long long)st.xfer_fail[1],
		    (unsigned long long)st.xfer_fail[2],
		    (unsigned long long)st.xfer_fail[3]);
	}
}
