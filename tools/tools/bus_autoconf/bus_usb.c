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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/queue.h>

#include "bus_autoconf.h"
#include "bus_sections.h"
#include "bus_usb.h"

struct usb_blob;
typedef TAILQ_HEAD(,usb_blob) usb_blob_head_t;
typedef TAILQ_ENTRY(usb_blob) usb_blob_entry_t;

static usb_blob_head_t usb_blob_head = TAILQ_HEAD_INITIALIZER(usb_blob_head);
static uint32_t usb_blob_count;

struct usb_blob {
	usb_blob_entry_t entry;
	struct usb_device_id temp;
};

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
	int retval;

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

	/* in the end sort by module name and mode */

	retval = strcmp(a->module_name, b->module_name);
	if (retval == 0)
		retval = strcmp(a->module_mode, b->module_mode);
	return (retval);
}

static void
usb_sort_entries(struct usb_device_id *id, uint32_t nid)
{
	qsort(id, nid, sizeof(*id), &usb_compare);
}

static void
usb_import_entry(struct usb_device_id *id, const char *type,
    const char *module, const uint8_t *ptr, uint16_t size)
{
	const char *mode;

	if (strstr(type, "_host_"))
		mode = "host";
	else if (strstr(type, "_device_"))
		mode = "device";
	else
		mode = "(host|device)";

	strlcpy(id->module_name, module, sizeof(id->module_name));
	strlcpy(id->module_mode, mode, sizeof(id->module_mode));

	/* import data from binary object */

	if (format_get_field(type, "mfl_vendor", ptr, size))
		id->match_flag_vendor = 1;
	if (format_get_field(type, "mfl_product", ptr, size))
		id->match_flag_product = 1;
	if (format_get_field(type, "mfl_dev_lo", ptr, size))
		id->match_flag_dev_lo = 1;
	if (format_get_field(type, "mfl_dev_hi", ptr, size))
		id->match_flag_dev_hi = 1;
	if (format_get_field(type, "mfl_dev_class", ptr, size))
		id->match_flag_dev_class = 1;
	if (format_get_field(type, "mfl_dev_subclass", ptr, size))
		id->match_flag_dev_subclass = 1;
	if (format_get_field(type, "mfl_dev_protocol", ptr, size))
		id->match_flag_dev_protocol = 1;
	if (format_get_field(type, "mfl_int_class", ptr, size))
		id->match_flag_int_class = 1;
	if (format_get_field(type, "mfl_int_subclass", ptr, size))
		id->match_flag_int_subclass = 1;
	if (format_get_field(type, "mfl_int_protocol", ptr, size))
		id->match_flag_int_protocol = 1;

	id->idVendor = format_get_field(type, "idVendor[0]", ptr, size) |
	    (format_get_field(type, "idVendor[1]", ptr, size) << 8);
	id->idProduct = format_get_field(type, "idProduct[0]", ptr, size) |
	    (format_get_field(type, "idProduct[1]", ptr, size) << 8);

	id->bcdDevice_lo = format_get_field(type, "bcdDevice_lo[0]", ptr, size) |
	    (format_get_field(type, "bcdDevice_lo[1]", ptr, size) << 8);

	id->bcdDevice_hi = format_get_field(type, "bcdDevice_hi[0]", ptr, size) |
	    (format_get_field(type, "bcdDevice_hi[1]", ptr, size) << 8);

	id->bDeviceClass = format_get_field(type, "bDeviceClass", ptr, size);
	id->bDeviceSubClass = format_get_field(type, "bDeviceSubClass", ptr, size);
	id->bDeviceProtocol = format_get_field(type, "bDeviceProtocol", ptr, size);

	id->bInterfaceClass = format_get_field(type, "bInterfaceClass", ptr, size);
	id->bInterfaceSubClass = format_get_field(type, "bInterfaceSubClass", ptr, size);
	id->bInterfaceProtocol = format_get_field(type, "bInterfaceProtocol", ptr, size);

	if (format_get_field(type, "mf_vendor", ptr, size))
		id->match_flag_vendor = 1;
	if (format_get_field(type, "mf_product", ptr, size))
		id->match_flag_product = 1;
	if (format_get_field(type, "mf_dev_lo", ptr, size))
		id->match_flag_dev_lo = 1;
	if (format_get_field(type, "mf_dev_hi", ptr, size))
		id->match_flag_dev_hi = 1;
	if (format_get_field(type, "mf_dev_class", ptr, size))
		id->match_flag_dev_class = 1;
	if (format_get_field(type, "mf_dev_subclass", ptr, size))
		id->match_flag_dev_subclass = 1;
	if (format_get_field(type, "mf_dev_protocol", ptr, size))
		id->match_flag_dev_protocol = 1;
	if (format_get_field(type, "mf_int_class", ptr, size))
		id->match_flag_int_class = 1;
	if (format_get_field(type, "mf_int_subclass", ptr, size))
		id->match_flag_int_subclass = 1;
	if (format_get_field(type, "mf_int_protocol", ptr, size))
		id->match_flag_int_protocol = 1;

	/* compute some internal fields */
	id->is_iface = id->match_flag_int_class |
	    id->match_flag_int_protocol |
	    id->match_flag_int_subclass;

	id->is_dev = id->match_flag_dev_class |
	    id->match_flag_dev_subclass;

	id->is_vp = id->match_flag_vendor |
	    id->match_flag_product;

	id->is_any = id->is_vp + id->is_dev + id->is_iface;
}

static uint32_t
usb_dump(struct usb_device_id *id, uint32_t nid)
{
	uint32_t n = 1;

	if (id->is_any) {
		printf("nomatch 32 {\n"
		    "	match \"bus\" \"uhub[0-9]+\";\n"
		    "	match \"mode\" \"%s\";\n", id->module_mode);
	} else {
		printf("# skipped entry on module %s\n",
		    id->module_name);
		return (n);
	}

	if (id->match_flag_vendor) {
		printf("	match \"vendor\" \"0x%04x\";\n",
		    id->idVendor);
	}
	if (id->match_flag_product) {
		uint32_t x;

		if (id->is_any == 1 && id->is_vp == 1) {
			/* try to join similar entries */
			while (n < nid) {
				if (id[n].is_any != 1 || id[n].is_vp != 1)
					break;
				if (id[n].idVendor != id[0].idVendor)
					break;
				n++;
			}
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
	    "};\n\n", id->module_name);

	return (n);
}

void
usb_import_entries(const char *section, const char *module,
    const uint8_t *ptr, uint32_t len)
{
	struct usb_blob *pub;
	uint32_t section_size;
	uint32_t off;

	section_size = format_get_section_size(section);
	if (section_size == 0) {
		errx(EX_DATAERR, "Invalid or non-existing "
		    "section format '%s'", section);
	}
	if (len % section_size) {
		errx(EX_DATAERR, "Length %d is not "
		    "divisible by %d. Section format '%s'",
		    len, section_size, section);
	}
	for (off = 0; off != len; off += section_size) {
		pub = malloc(sizeof(*pub));
		if (pub == NULL)
			errx(EX_SOFTWARE, "Out of memory");

		memset(pub, 0, sizeof(*pub));

		usb_import_entry(&pub->temp, section,
		    module, ptr + off, section_size);

		TAILQ_INSERT_TAIL(&usb_blob_head, pub, entry);

		usb_blob_count++;
		if (usb_blob_count == 0)
			errx(EX_SOFTWARE, "Too many entries");
	}
}

void
usb_dump_entries(void)
{
	struct usb_blob *pub;
	struct usb_device_id *id;
	uint32_t x;

	id = malloc(usb_blob_count * sizeof(*id));
	if (id == NULL)
		errx(EX_SOFTWARE, "Out of memory");

	/* make linear array of all USB blobs */
	x = 0;
	TAILQ_FOREACH(pub, &usb_blob_head, entry)
	    id[x++] = pub->temp;

	usb_sort_entries(id, usb_blob_count);

	for (x = 0; x != usb_blob_count;)
		x += usb_dump(id + x, usb_blob_count - x);

	free(id);

	printf("# %d USB entries processed\n\n", usb_blob_count);
}
