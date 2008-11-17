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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>

#include <libusb20.h>
#include <libusb20_desc.h>

#include "dump.h"

#define	DUMP0(n,type,field,...) dump_field(pdev, "  ", #field, n->field);
#define	DUMP1(n,type,field,...) dump_field(pdev, "    ", #field, n->field);
#define	DUMP2(n,type,field,...) dump_field(pdev, "      ", #field, n->field);
#define	DUMP3(n,type,field,...) dump_field(pdev, "        ", #field, n->field);

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
		return ("SUPER (4.8Gbps)");
	default:
		break;
	}
	return ("unknown");
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
dump_field(struct libusb20_device *pdev, const char *plevel,
    const char *field, uint32_t value)
{
	struct LIBUSB20_CONTROL_SETUP_DECODED req;
	uint16_t lang_id;
	uint8_t index;
	uint8_t temp_string[256];

	printf("%s%s = 0x%04x ", plevel, field, value);

	if ((field[0] != 'i') || (field[1] == 'd')) {
		printf("\n");
		return;
	}
	if (value == 0) {
		printf(" <no string> \n");
		return;
	}
	LIBUSB20_INIT(LIBUSB20_CONTROL_SETUP, &req);

	lang_id = 0;
	index = 0;

	req.bmRequestType =
	    LIBUSB20_REQUEST_TYPE_STANDARD |
	    LIBUSB20_RECIPIENT_DEVICE |
	    LIBUSB20_ENDPOINT_IN;
	req.bRequest = LIBUSB20_REQUEST_GET_DESCRIPTOR;
	req.wValue = (256 * LIBUSB20_DT_STRING) | index;
	req.wIndex = lang_id;
	req.wLength = 4;		/* bytes */

	if (libusb20_dev_request_sync(pdev, &req,
	    temp_string, NULL, 1000, 0)) {
		goto done;
	}
	lang_id = temp_string[2] | (temp_string[3] << 8);

	printf(" LangId:0x%04x <", lang_id);

	index = value;

	req.wValue = (256 * LIBUSB20_DT_STRING) | index;
	req.wIndex = lang_id;
	req.wLength = 4;		/* bytes */

	if (libusb20_dev_request_sync(pdev, &req,
	    temp_string, NULL, 1000, 0)) {
		printf("ERROR>\n");
		goto done;
	}
	req.wValue = (256 * LIBUSB20_DT_STRING) | index;
	req.wIndex = lang_id;
	req.wLength = temp_string[0];	/* bytes */

	if (libusb20_dev_request_sync(pdev, &req,
	    temp_string, NULL, 1000, 0)) {
		printf("ERROR>\n");
		goto done;
	}
	req.wLength /= 2;

	for (index = 1; index != req.wLength; index++) {
		if (isprint(temp_string[(2 * index) + 0])) {
			printf("%c", temp_string[(2 * index) + 0]);
		} else if (isprint(temp_string[(2 * index) + 1])) {
			printf("%c", temp_string[(2 * index) + 1]);
		} else {
			printf("?");
		}
	}
	printf(">\n");
done:
	return;
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
	return;
}

static void
dump_endpoint(struct libusb20_device *pdev,
    struct libusb20_endpoint *ep)
{
	struct LIBUSB20_ENDPOINT_DESC_DECODED *edesc;

	edesc = &ep->desc;
	LIBUSB20_ENDPOINT_DESC(DUMP3, edesc);
	dump_extra(&ep->extra, "  " "  " "  ");
	return;
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
	return;
}

void
dump_device_info(struct libusb20_device *pdev)
{
	printf("%s, cfg=%u md=%s spd=%s pwr=%s\n",
	    libusb20_dev_get_desc(pdev),
	    libusb20_dev_get_config_index(pdev),
	    dump_mode(libusb20_dev_get_mode(pdev)),
	    dump_speed(libusb20_dev_get_speed(pdev)),
	    dump_power_mode(libusb20_dev_get_power_mode(pdev)));
	return;
}

void
dump_be_quirk_names(struct libusb20_backend *pbe)
{
	struct libusb20_quirk q;
	uint16_t x;
	int err;

	memset(&q, 0, sizeof(q));

	printf("\nDumping list of supported quirks:\n\n");

	for (x = 0; x != 0xFFFF; x++) {

		err = libusb20_be_get_quirk_name(pbe, x, &q);
		if (err) {
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
	return;
}

void
dump_be_dev_quirks(struct libusb20_backend *pbe)
{
	struct libusb20_quirk q;
	uint16_t x;
	int err;

	memset(&q, 0, sizeof(q));

	printf("\nDumping current device quirks:\n\n");

	for (x = 0; x != 0xFFFF; x++) {

		err = libusb20_be_get_dev_quirk(pbe, x, &q);
		if (err) {
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
	return;
}

void
dump_be_access(struct libusb20_backend *pbe)
{
	struct group *gr;
	struct passwd *pw;
	const char *owner;
	const char *group;
	uid_t uid;
	gid_t gid;
	mode_t mode;

	if (libusb20_be_get_owner(pbe, &uid, &gid)) {
		err(1, "could not get owner");
	}
	if (libusb20_be_get_perm(pbe, &mode)) {
		err(1, "could not get permission");
	}
	owner = (pw = getpwuid(uid)) ? pw->pw_name : "UNKNOWN";
	group = (gr = getgrgid(gid)) ? gr->gr_name : "UNKNOWN";

	if (mode || 1) {
		printf("Global Access: %s:%s 0%o\n", owner, group, mode);
	} else {
		printf("Global Access: <not set>\n");
	}
	return;
}

void
dump_device_access(struct libusb20_device *pdev, uint8_t iface)
{
	struct group *gr;
	struct passwd *pw;
	const char *owner;
	const char *group;
	uid_t uid;
	gid_t gid;
	mode_t mode;

	if (libusb20_dev_get_owner(pdev, &uid, &gid)) {
		err(1, "could not get owner");
	}
	if (libusb20_dev_get_perm(pdev, &mode)) {
		err(1, "could not get permission");
	}
	if (mode) {
		owner = (pw = getpwuid(uid)) ? pw->pw_name : "UNKNOWN";
		group = (gr = getgrgid(gid)) ? gr->gr_name : "UNKNOWN";

		printf("  " "Device Access: %s:%s 0%o\n", owner, group, mode);

	} else {
		printf("  " "Device Access: <not set>\n");
	}

	if (iface == 0xFF) {
		for (iface = 0; iface != 0xFF; iface++) {
			if (dump_device_iface_access(pdev, iface)) {
				break;
			}
		}
	} else {
		if (dump_device_iface_access(pdev, iface)) {
			err(1, "could not get interface access info");
		}
	}
	return;
}

int
dump_device_iface_access(struct libusb20_device *pdev, uint8_t iface)
{
	struct group *gr;
	struct passwd *pw;
	const char *owner;
	const char *group;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	int error;

	if ((error = libusb20_dev_get_iface_owner(pdev, iface, &uid, &gid))) {
		return (error);
	}
	if ((error = libusb20_dev_get_iface_perm(pdev, iface, &mode))) {
		return (error);
	}
	if (mode) {

		owner = (pw = getpwuid(uid)) ? pw->pw_name : "UNKNOWN";
		group = (gr = getgrgid(gid)) ? gr->gr_name : "UNKNOWN";

		printf("    " "Interface %u Access: %s:%s 0%o\n", iface, owner, group, mode);
	} else {
		printf("    " "Interface %u Access: <not set>\n", iface);
	}

	return (0);
}

void
dump_device_desc(struct libusb20_device *pdev)
{
	struct LIBUSB20_DEVICE_DESC_DECODED *ddesc;

	ddesc = libusb20_dev_get_device_desc(pdev);
	LIBUSB20_DEVICE_DESC(DUMP0, ddesc);
	return;
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
	return;
}
