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
#include <errno.h>
#include <ctype.h>

#include <libusb20_desc.h>
#include <libusb20.h>

#include "dump.h"

struct options {
	const char *quirkname;
	void   *buffer;
	gid_t	gid;
	uid_t	uid;
	mode_t	mode;
	uint32_t got_any;
	struct LIBUSB20_CONTROL_SETUP_DECODED setup;
	uint16_t bus;
	uint16_t addr;
	uint16_t iface;
	uint16_t vid;
	uint16_t pid;
	uint16_t lo_rev;		/* inclusive */
	uint16_t hi_rev;		/* inclusive */
	uint8_t	string_index;
	uint8_t	config_index;
	uint8_t	alt_index;
	uint8_t	got_list:1;
	uint8_t	got_bus:1;
	uint8_t	got_addr:1;
	uint8_t	got_iface:1;
	uint8_t	got_set_config:1;
	uint8_t	got_set_alt:1;
	uint8_t	got_set_owner:1;
	uint8_t	got_set_perm:1;
	uint8_t	got_suspend:1;
	uint8_t	got_resume:1;
	uint8_t	got_reset:1;
	uint8_t	got_power_off:1;
	uint8_t	got_power_save:1;
	uint8_t	got_power_on:1;
	uint8_t	got_dump_device_quirks:1;
	uint8_t	got_dump_quirk_names:1;
	uint8_t	got_dump_device_desc:1;
	uint8_t	got_dump_curr_config:1;
	uint8_t	got_dump_all_config:1;
	uint8_t	got_dump_info:1;
	uint8_t	got_dump_access:1;
	uint8_t	got_remove_device_quirk:1;
	uint8_t	got_add_device_quirk:1;
	uint8_t	got_dump_string:1;
	uint8_t	got_do_request:1;
};

struct token {
	const char *name;
	uint8_t	value;
	uint8_t	narg;
};

enum {
	T_UNIT,
	T_ADDR,
	T_IFACE,
	T_SET_CONFIG,
	T_SET_ALT,
	T_SET_OWNER,
	T_SET_PERM,
	T_ADD_DEVICE_QUIRK,
	T_REMOVE_DEVICE_QUIRK,
	T_DUMP_QUIRK_NAMES,
	T_DUMP_DEVICE_QUIRKS,
	T_DUMP_DEVICE_DESC,
	T_DUMP_CURR_CONFIG_DESC,
	T_DUMP_ALL_CONFIG_DESC,
	T_DUMP_STRING,
	T_DUMP_ACCESS,
	T_DUMP_INFO,
	T_SUSPEND,
	T_RESUME,
	T_POWER_OFF,
	T_POWER_SAVE,
	T_POWER_ON,
	T_RESET,
	T_LIST,
	T_DO_REQUEST,
};

static struct options options;

static const struct token token[] = {
	{"-u", T_UNIT, 1},
	{"-a", T_ADDR, 1},
	{"-i", T_IFACE, 1},
	{"set_config", T_SET_CONFIG, 1},
	{"set_alt", T_SET_ALT, 1},
	{"set_owner", T_SET_OWNER, 1},
	{"set_perm", T_SET_PERM, 1},
	{"add_dev_quirk_vplh", T_ADD_DEVICE_QUIRK, 5},
	{"remove_dev_quirk_vplh", T_REMOVE_DEVICE_QUIRK, 5},
	{"dump_quirk_names", T_DUMP_QUIRK_NAMES, 0},
	{"dump_device_quirks", T_DUMP_DEVICE_QUIRKS, 0},
	{"dump_device_desc", T_DUMP_DEVICE_DESC, 0},
	{"dump_curr_config_desc", T_DUMP_CURR_CONFIG_DESC, 0},
	{"dump_all_config_desc", T_DUMP_ALL_CONFIG_DESC, 0},
	{"dump_string", T_DUMP_STRING, 1},
	{"dump_access", T_DUMP_ACCESS, 0},
	{"dump_info", T_DUMP_INFO, 0},
	{"suspend", T_SUSPEND, 0},
	{"resume", T_RESUME, 0},
	{"power_off", T_POWER_OFF, 0},
	{"power_save", T_POWER_SAVE, 0},
	{"power_on", T_POWER_ON, 0},
	{"reset", T_RESET, 0},
	{"list", T_LIST, 0},
	{"do_request", T_DO_REQUEST, 5},
};

static void
be_dev_remove_quirk(struct libusb20_backend *pbe,
    uint16_t vid, uint16_t pid, uint16_t lorev, uint16_t hirev,
    const char *str)
{
	struct libusb20_quirk q;
	int error;

	memset(&q, 0, sizeof(q));

	q.vid = vid;
	q.pid = pid;
	q.bcdDeviceLow = lorev;
	q.bcdDeviceHigh = hirev;
	strlcpy(q.quirkname, str, sizeof(q.quirkname));

	error = libusb20_be_remove_dev_quirk(pbe, &q);
	if (error) {
		printf("Removing quirk '%s' failed, continuing.\n", str);
	}
	return;
}

static void
be_dev_add_quirk(struct libusb20_backend *pbe,
    uint16_t vid, uint16_t pid, uint16_t lorev, uint16_t hirev,
    const char *str)
{
	struct libusb20_quirk q;
	int error;

	memset(&q, 0, sizeof(q));

	q.vid = vid;
	q.pid = pid;
	q.bcdDeviceLow = lorev;
	q.bcdDeviceHigh = hirev;
	strlcpy(q.quirkname, str, sizeof(q.quirkname));

	error = libusb20_be_add_dev_quirk(pbe, &q);
	if (error) {
		printf("Adding quirk '%s' failed, continuing.\n", str);
	}
	return;
}

static uint8_t
get_token(const char *str, uint8_t narg)
{
	uint8_t n;

	for (n = 0; n != (sizeof(token) / sizeof(token[0])); n++) {
		if (strcasecmp(str, token[n].name) == 0) {
			if (token[n].narg > narg) {
				/* too few arguments */
				break;
			}
			return (token[n].value);
		}
	}
	return (0 - 1);
}

static uid_t
num_id(const char *name, const char *type)
{
	uid_t val;
	char *ep;

	errno = 0;
	val = strtoul(name, &ep, 0);
	if (errno) {
		err(1, "%s", name);
	}
	if (*ep != '\0') {
		errx(1, "%s: illegal %s name", name, type);
	}
	return (val);
}

static gid_t
a_gid(const char *s)
{
	struct group *gr;

	if (*s == '\0') {
		/* empty group ID */
		return ((gid_t)-1);
	}
	return ((gr = getgrnam(s)) ? gr->gr_gid : num_id(s, "group"));
}

static uid_t
a_uid(const char *s)
{
	struct passwd *pw;

	if (*s == '\0') {
		/* empty user ID */
		return ((uid_t)-1);
	}
	return ((pw = getpwnam(s)) ? pw->pw_uid : num_id(s, "user"));
}

static mode_t
a_mode(const char *s)
{
	uint16_t val;
	char *ep;

	errno = 0;
	val = strtoul(s, &ep, 8);
	if (errno) {
		err(1, "%s", s);
	}
	if (*ep != '\0') {
		errx(1, "illegal permissions: %s", s);
	}
	return val;
}

static void
usage(void)
{
	printf(""
	    "usbconfig - configure the USB subsystem" "\n"
	    "usage: usbconfig -u <busnum> -a <devaddr> -i <ifaceindex> [cmds...]" "\n"
	    "commands:" "\n"
	    "  set_config <cfg_index>" "\n"
	    "  set_alt <alt_index>" "\n"
	    "  set_owner <user:group>" "\n"
	    "  set_perm <mode>" "\n"
	    "  add_dev_quirk_vplh <vid> <pid> <lo_rev> <hi_rev> <quirk>" "\n"
	    "  remove_dev_quirk_vplh <vid> <pid> <lo_rev> <hi_rev> <quirk>" "\n"
	    "  dump_quirk_names" "\n"
	    "  dump_device_quirks" "\n"
	    "  dump_device_desc" "\n"
	    "  dump_curr_config_desc" "\n"
	    "  dump_all_config_desc" "\n"
	    "  dump_string <index>" "\n"
	    "  dump_access" "\n"
	    "  dump_info" "\n"
	    "  suspend" "\n"
	    "  resume" "\n"
	    "  power_off" "\n"
	    "  power_save" "\n"
	    "  power_on" "\n"
	    "  reset" "\n"
	    "  list" "\n"
	    "  do_request <bmReqTyp> <bReq> <wVal> <wIdx> <wLen> <data...>" "\n"
	);
	exit(1);
}

static void
reset_options(struct options *opt)
{
	if (opt->buffer)
		free(opt->buffer);
	memset(opt, 0, sizeof(*opt));
	return;
}

static void
flush_command(struct libusb20_backend *pbe, struct options *opt)
{
	struct libusb20_device *pdev = NULL;
	uint32_t matches = 0;
	uint8_t dump_any;

	/* check for invalid option combinations */
	if ((opt->got_suspend +
	    opt->got_resume +
	    opt->got_reset +
	    opt->got_set_config +
	    opt->got_set_alt +
	    opt->got_power_save +
	    opt->got_power_on +
	    opt->got_power_off) > 1) {
		err(1, "cannot only specify one of 'set_config', "
		    "'set_alt', 'reset', 'suspend', 'resume', "
		    "'power_save', 'power_on' and 'power_off' "
		    "at the same time!");
	}
	if (opt->got_dump_access) {
		opt->got_any--;
		dump_be_access(pbe);
	}
	if (opt->got_dump_quirk_names) {
		opt->got_any--;
		dump_be_quirk_names(pbe);
	}
	if (opt->got_dump_device_quirks) {
		opt->got_any--;
		dump_be_dev_quirks(pbe);
	}
	if (opt->got_remove_device_quirk) {
		opt->got_any--;
		be_dev_remove_quirk(pbe,
		    opt->vid, opt->pid, opt->lo_rev, opt->hi_rev, opt->quirkname);
	}
	if (opt->got_add_device_quirk) {
		opt->got_any--;
		be_dev_add_quirk(pbe,
		    opt->vid, opt->pid, opt->lo_rev, opt->hi_rev, opt->quirkname);
	}
	if (opt->got_any == 0) {
		/*
		 * do not scan through all the devices if there are no valid
		 * options
		 */
		goto done;
	}
	while ((pdev = libusb20_be_device_foreach(pbe, pdev))) {

		if (opt->got_bus &&
		    (libusb20_dev_get_bus_number(pdev) != opt->bus)) {
			continue;
		}
		if (opt->got_addr &&
		    (libusb20_dev_get_address(pdev) != opt->addr)) {
			continue;
		}
		matches++;

		/* do owner and permissions first */

		if (opt->got_bus && opt->got_addr && opt->got_iface) {
			if (opt->got_set_owner) {
				if (libusb20_dev_set_iface_owner(pdev,
				    opt->iface, opt->uid, opt->gid)) {
					err(1, "setting owner and group failed\n");
				}
			}
			if (opt->got_set_perm) {
				if (libusb20_dev_set_iface_perm(pdev,
				    opt->iface, opt->mode)) {
					err(1, "setting mode failed\n");
				}
			}
		} else if (opt->got_bus && opt->got_addr) {
			if (opt->got_set_owner) {
				if (libusb20_dev_set_owner(pdev,
				    opt->uid, opt->gid)) {
					err(1, "setting owner and group failed\n");
				}
			}
			if (opt->got_set_perm) {
				if (libusb20_dev_set_perm(pdev,
				    opt->mode)) {
					err(1, "setting mode failed\n");
				}
			}
		} else if (opt->got_bus) {
			if (opt->got_set_owner) {
				if (libusb20_bus_set_owner(pbe, opt->bus,
				    opt->uid, opt->gid)) {
					err(1, "setting owner and group failed\n");
				}
			}
			if (opt->got_set_perm) {
				if (libusb20_bus_set_perm(pbe, opt->bus,
				    opt->mode)) {
					err(1, "setting mode failed\n");
				}
			}
		} else {
			if (opt->got_set_owner) {
				if (libusb20_be_set_owner(pbe,
				    opt->uid, opt->gid)) {
					err(1, "setting owner and group failed\n");
				}
			}
			if (opt->got_set_perm) {
				if (libusb20_be_set_perm(pbe,
				    opt->mode)) {
					err(1, "setting mode failed\n");
				}
			}
		}

		if (libusb20_dev_open(pdev, 0)) {
			err(1, "could not open device");
		}
		if (opt->got_dump_string) {
			char *pbuf;

			pbuf = malloc(256);
			if (pbuf == NULL) {
				err(1, "out of memory");
			}
			if (libusb20_dev_req_string_simple_sync(pdev,
			    opt->string_index, pbuf, 256)) {
				printf("STRING_0x%02x = <read error>\n",
				    opt->string_index);
			} else {
				printf("STRING_0x%02x = <%s>\n",
				    opt->string_index, pbuf);
			}
			free(pbuf);
		}
		if (opt->got_do_request) {
			uint16_t actlen;
			uint16_t t;

			if (libusb20_dev_request_sync(pdev, &opt->setup,
			    opt->buffer, &actlen, 5000 /* 5 seconds */ , 0)) {
				printf("REQUEST = <ERROR>\n");
			} else if (!(opt->setup.bmRequestType &
			    LIBUSB20_ENDPOINT_IN)) {
				printf("REQUEST = <OK>\n");
			} else {
				t = actlen;
				printf("REQUEST = <");
				for (t = 0; t != actlen; t++) {
					printf("0x%02x%s",
					    ((uint8_t *)opt->buffer)[t],
					    (t == (actlen - 1)) ? "" : " ");
				}
				printf("><");
				for (t = 0; t != actlen; t++) {
					char c;

					c = ((uint8_t *)opt->buffer)[t];
					if ((c != '<') &&
					    (c != '>') && isprint(c)) {
						putchar(c);
					}
				}
				printf(">\n");
			}
		}
		if (opt->got_set_config) {
			if (libusb20_dev_set_config_index(pdev,
			    opt->config_index)) {
				err(1, "could not set config index");
			}
		}
		if (opt->got_set_alt) {
			if (libusb20_dev_set_alt_index(pdev, opt->iface,
			    opt->alt_index)) {
				err(1, "could not set alternate setting");
			}
		}
		if (opt->got_reset) {
			if (libusb20_dev_reset(pdev)) {
				err(1, "could not reset device");
			}
		}
		if (opt->got_suspend) {
			if (libusb20_dev_set_power_mode(pdev,
			    LIBUSB20_POWER_SUSPEND)) {
				err(1, "could not set suspend");
			}
		}
		if (opt->got_resume) {
			if (libusb20_dev_set_power_mode(pdev,
			    LIBUSB20_POWER_RESUME)) {
				err(1, "could not set resume");
			}
		}
		if (opt->got_power_off) {
			if (libusb20_dev_set_power_mode(pdev,
			    LIBUSB20_POWER_OFF)) {
				err(1, "could not set power OFF");
			}
		}
		if (opt->got_power_save) {
			if (libusb20_dev_set_power_mode(pdev,
			    LIBUSB20_POWER_SAVE)) {
				err(1, "could not set power SAVE");
			}
		}
		if (opt->got_power_on) {
			if (libusb20_dev_set_power_mode(pdev,
			    LIBUSB20_POWER_ON)) {
				err(1, "could not set power ON");
			}
		}
		dump_any =
		    (opt->got_dump_device_desc ||
		    opt->got_dump_curr_config ||
		    opt->got_dump_all_config ||
		    opt->got_dump_info ||
		    opt->got_dump_access);

		if (opt->got_list || dump_any) {
			dump_device_info(pdev);
		}
		if (opt->got_dump_access) {
			printf("\n");
			dump_device_access(pdev, opt->got_iface ?
			    opt->iface : 0xFF);
		}
		if (opt->got_dump_device_desc) {
			printf("\n");
			dump_device_desc(pdev);
		}
		if (opt->got_dump_all_config) {
			printf("\n");
			dump_config(pdev, 1);
		} else if (opt->got_dump_curr_config) {
			printf("\n");
			dump_config(pdev, 0);
		}
		if (dump_any) {
			printf("\n");
		}
		if (libusb20_dev_close(pdev)) {
			err(1, "could not close device");
		}
	}

	if (matches == 0) {
		printf("No device match\n");
	}
done:
	reset_options(opt);

	return;
}

int
main(int argc, char **argv)
{
	struct libusb20_backend *pbe;
	struct options *opt = &options;
	char *cp;
	int n;
	int t;

	if (argc < 1) {
		usage();
	}
	pbe = libusb20_be_alloc_default();
	if (pbe == NULL)
		err(1, "could not access USB backend\n");

	for (n = 1; n != argc; n++) {

		/* get number of additional options */
		t = (argc - n - 1);
		if (t > 255)
			t = 255;
		switch (get_token(argv[n], t)) {
		case T_ADD_DEVICE_QUIRK:
			if (opt->got_add_device_quirk) {
				flush_command(pbe, opt);
			}
			opt->vid = num_id(argv[n + 1], "Vendor ID");
			opt->pid = num_id(argv[n + 2], "Product ID");
			opt->lo_rev = num_id(argv[n + 3], "Low Revision");
			opt->hi_rev = num_id(argv[n + 4], "High Revision");
			opt->quirkname = argv[n + 5];
			n += 5;

			opt->got_add_device_quirk = 1;
			opt->got_any++;
			break;

		case T_REMOVE_DEVICE_QUIRK:
			if (opt->got_remove_device_quirk) {
				flush_command(pbe, opt);
			}
			opt->vid = num_id(argv[n + 1], "Vendor ID");
			opt->pid = num_id(argv[n + 2], "Product ID");
			opt->lo_rev = num_id(argv[n + 3], "Low Revision");
			opt->hi_rev = num_id(argv[n + 4], "High Revision");
			opt->quirkname = argv[n + 5];
			n += 5;
			opt->got_remove_device_quirk = 1;
			opt->got_any++;
			break;

		case T_DUMP_QUIRK_NAMES:
			opt->got_dump_quirk_names = 1;
			opt->got_any++;
			break;

		case T_DUMP_DEVICE_QUIRKS:
			opt->got_dump_device_quirks = 1;
			opt->got_any++;
			break;

		case T_UNIT:
			if (opt->got_any) {
				/* allow multiple commands on the same line */
				flush_command(pbe, opt);
			}
			opt->bus = num_id(argv[n + 1], "busnum");
			opt->got_bus = 1;
			n++;
			break;
		case T_ADDR:
			opt->addr = num_id(argv[n + 1], "addr");
			opt->got_addr = 1;
			n++;
			break;
		case T_IFACE:
			opt->iface = num_id(argv[n + 1], "iface");
			opt->got_iface = 1;
			n++;
			break;
		case T_SET_CONFIG:
			opt->config_index = num_id(argv[n + 1], "cfg_index");
			opt->got_set_config = 1;
			opt->got_any++;
			n++;
			break;
		case T_SET_ALT:
			opt->alt_index = num_id(argv[n + 1], "cfg_index");
			opt->got_set_alt = 1;
			opt->got_any++;
			n++;
			break;
		case T_SET_OWNER:
			cp = argv[n + 1];
			cp = strchr(cp, ':');
			if (cp == NULL) {
				err(1, "missing colon in '%s'!", argv[n + 1]);
			}
			*(cp++) = '\0';
			opt->gid = a_gid(cp);
			opt->uid = a_uid(argv[n + 1]);
			opt->got_set_owner = 1;
			opt->got_any++;
			n++;
			break;
		case T_SET_PERM:
			opt->mode = a_mode(argv[n + 1]);
			opt->got_set_perm = 1;
			opt->got_any++;
			n++;
			break;
		case T_DUMP_DEVICE_DESC:
			opt->got_dump_device_desc = 1;
			opt->got_any++;
			break;
		case T_DUMP_CURR_CONFIG_DESC:
			opt->got_dump_curr_config = 1;
			opt->got_any++;
			break;
		case T_DUMP_ALL_CONFIG_DESC:
			opt->got_dump_all_config = 1;
			opt->got_any++;
			break;
		case T_DUMP_INFO:
			opt->got_dump_info = 1;
			opt->got_any++;
			break;
		case T_DUMP_STRING:
			if (opt->got_dump_string) {
				flush_command(pbe, opt);
			}
			opt->string_index = num_id(argv[n + 1], "str_index");
			opt->got_dump_string = 1;
			opt->got_any++;
			n++;
			break;
		case T_DUMP_ACCESS:
			opt->got_dump_access = 1;
			opt->got_any += 2;
			break;
		case T_SUSPEND:
			opt->got_suspend = 1;
			opt->got_any++;
			break;
		case T_RESUME:
			opt->got_resume = 1;
			opt->got_any++;
			break;
		case T_POWER_OFF:
			opt->got_power_off = 1;
			opt->got_any++;
			break;
		case T_POWER_SAVE:
			opt->got_power_save = 1;
			opt->got_any++;
			break;
		case T_POWER_ON:
			opt->got_power_on = 1;
			opt->got_any++;
			break;
		case T_RESET:
			opt->got_reset = 1;
			opt->got_any++;
			break;
		case T_LIST:
			opt->got_list = 1;
			opt->got_any++;
			break;
		case T_DO_REQUEST:
			if (opt->got_do_request) {
				flush_command(pbe, opt);
			}
			LIBUSB20_INIT(LIBUSB20_CONTROL_SETUP, &opt->setup);
			opt->setup.bmRequestType = num_id(argv[n + 1], "bmReqTyp");
			opt->setup.bRequest = num_id(argv[n + 2], "bReq");
			opt->setup.wValue = num_id(argv[n + 3], "wVal");
			opt->setup.wIndex = num_id(argv[n + 4], "wIndex");
			opt->setup.wLength = num_id(argv[n + 5], "wLen");
			if (opt->setup.wLength != 0) {
				opt->buffer = malloc(opt->setup.wLength);
			} else {
				opt->buffer = NULL;
			}

			n += 5;

			if (!(opt->setup.bmRequestType &
			    LIBUSB20_ENDPOINT_IN)) {
				/* copy in data */
				t = (argc - n - 1);
				if (t < opt->setup.wLength) {
					err(1, "request data missing");
				}
				t = opt->setup.wLength;
				while (t--) {
					((uint8_t *)opt->buffer)[t] =
					    num_id(argv[n + t + 1], "req_data");
				}
				n += opt->setup.wLength;
			}
			opt->got_do_request = 1;
			opt->got_any++;
			break;
		default:
			usage();
			break;
		}
	}
	if (opt->got_any) {
		/* flush out last command */
		flush_command(pbe, opt);
	} else {
		/* list all the devices */
		opt->got_list = 1;
		opt->got_any++;
		flush_command(pbe, opt);
	}
	/* release data */
	libusb20_be_free(pbe);

	return (0);
}
