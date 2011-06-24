/* $FreeBSD$ */

/*-
 * Copyright (c) 2011 Hans Petter Selasky. All rights reserved.
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

/*
 * Disclaimer: This utility and format is subject to change and not a
 * comitted interface.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>

#include "bus_autoconf.h"

static char *type;
static char *file_name;
static char *module;
static const char *mode;

static int
usb_compare(const void *_a, const void *_b)
{
	const struct usb_device_id *a = _a;
	const struct usb_device_id *b = _b;

	if (a->idVendor > b->idVendor)
		return (1);
	if (a->idVendor < b->idVendor)
		return (-1);
	if (a->idProduct > b->idProduct)
		return (1);
	if (a->idProduct < b->idProduct)
		return (-1);
	if (a->bDeviceClass > b->bDeviceClass)
		return (1);
	if (a->bDeviceClass < b->bDeviceClass)
		return (-1);
	if (a->bDeviceSubClass > b->bDeviceSubClass)
		return (1);
	if (a->bDeviceSubClass < b->bDeviceSubClass)
		return (-1);
	if (a->bDeviceProtocol > b->bDeviceProtocol)
		return (1);
	if (a->bDeviceProtocol < b->bDeviceProtocol)
		return (-1);
	if (a->bInterfaceClass > b->bInterfaceClass)
		return (1);
	if (a->bInterfaceClass < b->bInterfaceClass)
		return (-1);
	if (a->bInterfaceSubClass > b->bInterfaceSubClass)
		return (1);
	if (a->bInterfaceSubClass < b->bInterfaceSubClass)
		return (-1);
	if (a->bInterfaceProtocol > b->bInterfaceProtocol)
		return (1);
	if (a->bInterfaceProtocol < b->bInterfaceProtocol)
		return (-1);

	return (0);
}

static void
usb_sort(struct usb_device_id *id, uint32_t nid)
{
	qsort(id, nid, sizeof(*id), &usb_compare);
}

struct usb_info {
	uint8_t	is_iface;
	uint8_t	is_any;
	uint8_t	is_vp;
	uint8_t	is_dev;
};

static void
usb_dump_sub(struct usb_device_id *id, struct usb_info *pinfo)
{
#if USB_HAVE_COMPAT_LINUX
	if (id->match_flags & USB_DEVICE_ID_MATCH_VENDOR)
		id->match_flag_vendor = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT)
		id->match_flag_product = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO)
		id->match_flag_dev_lo = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI)
		id->match_flag_dev_hi = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS)
		id->match_flag_dev_class = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS)
		id->match_flag_dev_subclass = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL)
		id->match_flag_dev_protocol = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS)
		id->match_flag_int_class = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS)
		id->match_flag_int_subclass = 1;
	if (id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL)
		id->match_flag_int_protocol = 1;
#endif

	pinfo->is_iface = id->match_flag_int_class |
	    id->match_flag_int_protocol |
	    id->match_flag_int_subclass;

	pinfo->is_dev = id->match_flag_dev_class |
	    id->match_flag_dev_subclass;

	pinfo->is_vp = id->match_flag_vendor |
	    id->match_flag_product;

	pinfo->is_any = pinfo->is_vp + pinfo->is_dev + pinfo->is_iface;
}

static uint32_t
usb_dump(struct usb_device_id *id, uint32_t nid)
{
	uint32_t n = 1;
	struct usb_info info;

	usb_dump_sub(id, &info);

	if (info.is_iface) {
		printf("nomatch 10 {\n"
		    "	match \"system\" \"USB\";\n"
		    "	match \"subsystem\" \"INTERFACE\";\n"
		    "	match \"mode\" \"%s\";\n", mode);
	} else if (info.is_any) {
		printf("nomatch 10 {\n"
		    "	match \"system\" \"USB\";\n"
		    "	match \"subsystem\" \"DEVICE\";\n"
		    "	match \"mode\" \"%s\";\n", mode);
	} else {
		return (n);
	}

	if (id->match_flag_vendor) {
		printf("	match \"vendor\" \"0x%04x\";\n",
		    id->idVendor);
	}
	if (id->match_flag_product) {
		uint32_t x;

		if (info.is_any == 1 && info.is_vp == 1) {
			/* try to join similar entries */
			while (n < nid) {
				usb_dump_sub(id + n, &info);

				if (info.is_any != 1 || info.is_vp != 1)
					break;
				if (id[n].idVendor != id[0].idVendor)
					break;
				n++;
			}
			/* restore infos */
			usb_dump_sub(id, &info);
		}
		if (n == 1) {
			printf("	match \"product\" \"0x%04x\";\n",
			    id->idProduct);
		} else {
			printf("	match \"product\" \"(");

			for (x = 0; x != n; x++) {
				printf("0x%04x%s", id[x].idProduct,
				    (x == (n - 1)) ? "" : "|");
			}

			printf(")\";\n");
		}
	}
	if (id->match_flag_dev_class) {
		printf("	match \"devclass\" \"0x%02x\";\n",
		    id->bDeviceClass);
	}
	if (id->match_flag_dev_subclass) {
		printf("	match \"devsubclass\" \"0x%02x\";\n",
		    id->bDeviceSubClass);
	}
	if (id->match_flag_int_class) {
		printf("	match \"intclass\" \"0x%02x\";\n",
		    id->bInterfaceClass);
	}
	if (id->match_flag_int_subclass) {
		printf("	match \"intsubclass\" \"0x%02x\";\n",
		    id->bInterfaceSubClass);
	}
	if (id->match_flag_int_protocol) {
		printf("	match \"intprotocol\" \"0x%02x\";\n",
		    id->bInterfaceProtocol);
	}
	printf("	action \"kldload %s\";\n"
	    "};\n\n", module);

	return (n);
}

static void
usb_parse_and_dump(int f, off_t size)
{
	struct usb_device_id *id;
	uint32_t nid;
	uint32_t x;

	if (size % sizeof(struct usb_device_id)) {
		errx(EX_NOINPUT, "Size is not divisible by %d",
		    (int)sizeof(struct usb_device_id));
	}
	lseek(f, 0, SEEK_SET);

	id = malloc(size);
	if (id == NULL) {
		errx(EX_SOFTWARE, "Out of memory");
	}
	if (read(f, id, size) != size) {
		err(EX_NOINPUT, "Cannot read all data");
	}
	nid = size / sizeof(*id);

	usb_sort(id, nid);

	for (x = 0; x != nid;)
		x += usb_dump(id + x, nid - x);

	free(id);
}

static void
usage(void)
{
	fprintf(stderr,
	    "bus_autoconf - devd config file generator\n"
	    "	-i <input_binary>\n"
	    "	-m <module_name>\n"
	    "	-t <structure_type>\n"
	    "	-h show usage\n"
	);
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	const char *params = "i:m:ht:";
	int c;
	int f;
	off_t off;

	while ((c = getopt(argc, argv, params)) != -1) {
		switch (c) {
		case 'i':
			file_name = optarg;
			break;
		case 't':
			type = optarg;
			break;
		case 'm':
			module = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	if (type == NULL || module == NULL || file_name == NULL)
		usage();

	f = open(file_name, O_RDONLY);
	if (f < 0)
		err(EX_NOINPUT, "Cannot open file '%s'", file_name);

	off = lseek(f, 0, SEEK_END);
	if (off <= 0)
		err(EX_NOINPUT, "Cannot seek to end of file");

	if (strcmp(type, "usb_host") == 0) {
		mode = "host";
		usb_parse_and_dump(f, off);
	} else if (strcmp(type, "usb_device") == 0) {
		mode = "device";
		usb_parse_and_dump(f, off);
	} else if (strcmp(type, "usb_dual") == 0) {
		mode = "(host|device)";
		usb_parse_and_dump(f, off);
	} else {
		err(EX_USAGE, "Unsupported structure type: %s", type);
	}

	close(f);

	return (0);
}
