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
static const char *mode;

struct usb_info;
static void usb_dump_sub(struct usb_device_id *, struct usb_info *);

/*
 * To ensure that the correct USB driver is loaded, the driver having
 * the most information about the device must be probed first. Then
 * more generic drivers shall be probed.
 */
static int
usb_compare(const void *_a, const void *_b)
{
	const struct usb_device_id *a = _a;
	const struct usb_device_id *b = _b;

	/* vendor matches first */

	if (a->match_flag_vendor > b->match_flag_vendor)
		return (-1);
	if (a->match_flag_vendor < b->match_flag_vendor)
		return (1);

	/* product matches first */

	if (a->match_flag_product > b->match_flag_product)
		return (-1);
	if (a->match_flag_product < b->match_flag_product)
		return (1);

	/* device class matches first */

	if (a->match_flag_dev_class > b->match_flag_dev_class)
		return (-1);
	if (a->match_flag_dev_class < b->match_flag_dev_class)
		return (1);

	if (a->match_flag_dev_subclass > b->match_flag_dev_subclass)
		return (-1);
	if (a->match_flag_dev_subclass < b->match_flag_dev_subclass)
		return (1);

	/* interface class matches first */

	if (a->match_flag_int_class > b->match_flag_int_class)
		return (-1);
	if (a->match_flag_int_class < b->match_flag_int_class)
		return (1);

	if (a->match_flag_int_subclass > b->match_flag_int_subclass)
		return (-1);
	if (a->match_flag_int_subclass < b->match_flag_int_subclass)
		return (1);

	if (a->match_flag_int_protocol > b->match_flag_int_protocol)
		return (-1);
	if (a->match_flag_int_protocol < b->match_flag_int_protocol)
		return (1);

	/* then sort according to value */

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

	/* in the end sort by module name */

	return (strcmp(a->module_name, b->module_name));
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
	if (id->match_flags != 0) {
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
		id->match_flags = 0;
	}
#endif
	if (pinfo != NULL) {

		pinfo->is_iface = id->match_flag_int_class |
		    id->match_flag_int_protocol |
		    id->match_flag_int_subclass;

		pinfo->is_dev = id->match_flag_dev_class |
		    id->match_flag_dev_subclass;

		pinfo->is_vp = id->match_flag_vendor |
		    id->match_flag_product;

		pinfo->is_any = pinfo->is_vp + pinfo->is_dev + pinfo->is_iface;
	}
}

static char *
usb_trim(char *ptr)
{
	char *end;

	end = strchr(ptr, ' ');
	if (end)
		*end = 0;
	return (ptr);
}

static uint32_t
usb_dump(struct usb_device_id *id, uint32_t nid)
{
	uint32_t n = 1;
	struct usb_info info;

	usb_dump_sub(id, &info);

	if (info.is_any) {
		printf("nomatch 32 {\n"
		    "	match \"bus\" \"uhub[0-9]+\";\n"
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
	    "};\n\n", usb_trim(id->module_name));

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

	for (x = 0; x != nid; x++) {
		/* make sure flag bits are correct */
		usb_dump_sub(id + x, NULL);
		/* zero terminate string */
		id[x].module_name[sizeof(id[0].module_name) - 1] = 0;
	}

	usb_sort(id, nid);

	for (x = 0; x != nid;)
		x += usb_dump(id + x, nid - x);

	free(id);

	printf("# %d %s entries processed\n\n", (int)nid, type);
}

static void
usage(void)
{
	fprintf(stderr,
	    "bus_autoconf - devd config file generator\n"
	    "	-i <input_binary>\n"
	    "	-t <structure_type>\n"
	    "	-h show usage\n"
	);
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	const char *params = "i:ht:";
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
		default:
			usage();
			break;
		}
	}

	if (type == NULL || file_name == NULL)
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
