/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Adrian Chadd <adrian@freebsd.org>
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2023 Future Crew LLC.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/endian.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libusb.h>

#include "rtlbt_fw.h"
#include "rtlbt_hw.h"
#include "rtlbt_dbg.h"

#define	_DEFAULT_RTLBT_FIRMWARE_PATH	"/usr/share/firmware/rtlbt"

int	rtlbt_do_debug = 0;
int	rtlbt_do_info = 0;

struct rtlbt_devid {
	uint16_t product_id;
	uint16_t vendor_id;
};

static struct rtlbt_devid rtlbt_list[] = {
	/* Realtek 8821CE Bluetooth devices */
	{ .vendor_id = 0x13d3, .product_id = 0x3529 },

	/* Realtek 8822CE Bluetooth devices */
	{ .vendor_id = 0x0bda, .product_id = 0xb00c },
	{ .vendor_id = 0x0bda, .product_id = 0xc822 },

	/* Realtek 8822CU Bluetooth devices */
	{ .vendor_id = 0x13d3, .product_id = 0x3549 },

	/* Realtek 8851BE Bluetooth devices */
	{ .vendor_id = 0x13d3, .product_id = 0x3600 },

	/* Realtek 8852AE Bluetooth devices */
	{ .vendor_id = 0x0bda, .product_id = 0x2852 },
	{ .vendor_id = 0x0bda, .product_id = 0xc852 },
	{ .vendor_id = 0x0bda, .product_id = 0x385a },
	{ .vendor_id = 0x0bda, .product_id = 0x4852 },
	{ .vendor_id = 0x04c5, .product_id = 0x165c },
	{ .vendor_id = 0x04ca, .product_id = 0x4006 },
	{ .vendor_id = 0x0cb8, .product_id = 0xc549 },

	/* Realtek 8852CE Bluetooth devices */
	{ .vendor_id = 0x04ca, .product_id = 0x4007 },
	{ .vendor_id = 0x04c5, .product_id = 0x1675 },
	{ .vendor_id = 0x0cb8, .product_id = 0xc558 },
	{ .vendor_id = 0x13d3, .product_id = 0x3587 },
	{ .vendor_id = 0x13d3, .product_id = 0x3586 },
	{ .vendor_id = 0x13d3, .product_id = 0x3592 },
	{ .vendor_id = 0x0489, .product_id = 0xe122 },

	/* Realtek 8852BE Bluetooth devices */
	{ .vendor_id = 0x0cb8, .product_id = 0xc559 },
	{ .vendor_id = 0x0bda, .product_id = 0x4853 },
	{ .vendor_id = 0x0bda, .product_id = 0x887b },
	{ .vendor_id = 0x0bda, .product_id = 0xb85b },
	{ .vendor_id = 0x13d3, .product_id = 0x3570 },
	{ .vendor_id = 0x13d3, .product_id = 0x3571 },
	{ .vendor_id = 0x13d3, .product_id = 0x3572 },
	{ .vendor_id = 0x13d3, .product_id = 0x3591 },
	{ .vendor_id = 0x0489, .product_id = 0xe123 },
	{ .vendor_id = 0x0489, .product_id = 0xe125 },

	/* Realtek 8852BT/8852BE-VT Bluetooth devices */
	{ .vendor_id = 0x0bda, .product_id = 0x8520 },

	/* Realtek 8922AE Bluetooth devices */
	{ .vendor_id = 0x0bda, .product_id = 0x8922 },
	{ .vendor_id = 0x13d3, .product_id = 0x3617 },
	{ .vendor_id = 0x13d3, .product_id = 0x3616 },
	{ .vendor_id = 0x0489, .product_id = 0xe130 },

	/* Realtek 8723AE Bluetooth devices */
	{ .vendor_id = 0x0930, .product_id = 0x021d },
	{ .vendor_id = 0x13d3, .product_id = 0x3394 },

	/* Realtek 8723BE Bluetooth devices */
	{ .vendor_id = 0x0489, .product_id = 0xe085 },
	{ .vendor_id = 0x0489, .product_id = 0xe08b },
	{ .vendor_id = 0x04f2, .product_id = 0xb49f },
	{ .vendor_id = 0x13d3, .product_id = 0x3410 },
	{ .vendor_id = 0x13d3, .product_id = 0x3416 },
	{ .vendor_id = 0x13d3, .product_id = 0x3459 },
	{ .vendor_id = 0x13d3, .product_id = 0x3494 },

	/* Realtek 8723BU Bluetooth devices */
	{ .vendor_id = 0x7392, .product_id = 0xa611 },

	/* Realtek 8723DE Bluetooth devices */
	{ .vendor_id = 0x0bda, .product_id = 0xb009 },
	{ .vendor_id = 0x2ff8, .product_id = 0xb011 },

	/* Realtek 8761BUV Bluetooth devices */
	{ .vendor_id = 0x2c4e, .product_id = 0x0115 },
	{ .vendor_id = 0x2357, .product_id = 0x0604 },
	{ .vendor_id = 0x0b05, .product_id = 0x190e },
	{ .vendor_id = 0x2550, .product_id = 0x8761 },
	{ .vendor_id = 0x0bda, .product_id = 0x8771 },
	{ .vendor_id = 0x6655, .product_id = 0x8771 },
	{ .vendor_id = 0x7392, .product_id = 0xc611 },
	{ .vendor_id = 0x2b89, .product_id = 0x8761 },

	/* Realtek 8821AE Bluetooth devices */
	{ .vendor_id = 0x0b05, .product_id = 0x17dc },
	{ .vendor_id = 0x13d3, .product_id = 0x3414 },
	{ .vendor_id = 0x13d3, .product_id = 0x3458 },
	{ .vendor_id = 0x13d3, .product_id = 0x3461 },
	{ .vendor_id = 0x13d3, .product_id = 0x3462 },

	/* Realtek 8822BE Bluetooth devices */
	{ .vendor_id = 0x13d3, .product_id = 0x3526 },
	{ .vendor_id = 0x0b05, .product_id = 0x185c },

	/* Realtek 8822CE Bluetooth devices */
	{ .vendor_id = 0x04ca, .product_id = 0x4005 },
	{ .vendor_id = 0x04c5, .product_id = 0x161f },
	{ .vendor_id = 0x0b05, .product_id = 0x18ef },
	{ .vendor_id = 0x13d3, .product_id = 0x3548 },
	{ .vendor_id = 0x13d3, .product_id = 0x3549 },
	{ .vendor_id = 0x13d3, .product_id = 0x3553 },
	{ .vendor_id = 0x13d3, .product_id = 0x3555 },
	{ .vendor_id = 0x2ff8, .product_id = 0x3051 },
	{ .vendor_id = 0x1358, .product_id = 0xc123 },
	{ .vendor_id = 0x0bda, .product_id = 0xc123 },
	{ .vendor_id = 0x0cb5, .product_id = 0xc547 },
};

static int
rtlbt_is_realtek(struct libusb_device_descriptor *d)
{
	int i;

	/* Search looking for whether it's a Realtek-based device */
	for (i = 0; i < (int) nitems(rtlbt_list); i++) {
		if ((rtlbt_list[i].product_id == d->idProduct) &&
		    (rtlbt_list[i].vendor_id == d->idVendor)) {
			rtlbt_info("found USB Realtek");
			return (1);
		}
	}

	/* Not found */
	return (0);
}

static int
rtlbt_is_bluetooth(struct libusb_device *dev)
{
	struct libusb_config_descriptor *cfg;
	const struct libusb_interface *ifc;
	const struct libusb_interface_descriptor *d;
	int r;

	r = libusb_get_active_config_descriptor(dev, &cfg);
	if (r < 0) {
		rtlbt_err("Cannot retrieve config descriptor: %s",
		    libusb_error_name(r));
		return (0);
	}

	if (cfg->bNumInterfaces != 0) {
		/* Only 0-th HCI/ACL interface is supported by downloader */
		ifc = &cfg->interface[0];
		if (ifc->num_altsetting != 0) {
			/* BT HCI/ACL interface has no altsettings */
			d = &ifc->altsetting[0];
			/* Check if interface is a bluetooth */
			if (d->bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
			    d->bInterfaceSubClass == 0x01 &&
			    d->bInterfaceProtocol == 0x01) {
				rtlbt_info("found USB Realtek");
				libusb_free_config_descriptor(cfg);
				return (1);
			}
		}
	}
	libusb_free_config_descriptor(cfg);

	/* Not found */
	return (0);
}

static libusb_device *
rtlbt_find_device(libusb_context *ctx, int bus_id, int dev_id)
{
	libusb_device **list, *dev = NULL, *found = NULL;
	struct libusb_device_descriptor d;
	ssize_t cnt, i;
	int r;

	cnt = libusb_get_device_list(ctx, &list);
	if (cnt < 0) {
		rtlbt_err("libusb_get_device_list() failed: code %lld",
		    (long long int) cnt);
		return (NULL);
	}

	/*
	 * Scan through USB device list.
	 */
	for (i = 0; i < cnt; i++) {
		dev = list[i];
		if (bus_id == libusb_get_bus_number(dev) &&
		    dev_id == libusb_get_device_address(dev)) {
			/* Get the device descriptor for this device entry */
			r = libusb_get_device_descriptor(dev, &d);
			if (r != 0) {
				rtlbt_err("libusb_get_device_descriptor: %s",
				    libusb_strerror(r));
				break;
			}

			/* For non-Realtek match on the vendor/product id */
			if (rtlbt_is_realtek(&d)) {
				/*
				 * Take a reference so it's not freed later on.
				 */
				found = libusb_ref_device(dev);
				break;
			}
			/* For Realtek vendor match on the interface class */
			if (d.idVendor == 0x0bda && rtlbt_is_bluetooth(dev)) {
				/*
				 * Take a reference so it's not freed later on.
				 */
				found = libusb_ref_device(dev);
				break;
			}
		}
	}

	libusb_free_device_list(list, 1);
	return (found);
}

static void
rtlbt_dump_version(ng_hci_read_local_ver_rp *ver)
{
	rtlbt_info("hci_version    0x%02x", ver->hci_version);
	rtlbt_info("hci_revision   0x%04x", le16toh(ver->hci_revision));
	rtlbt_info("lmp_version    0x%02x", ver->lmp_version);
	rtlbt_info("lmp_subversion 0x%04x", le16toh(ver->lmp_subversion));
}

/*
 * Parse ugen name and extract device's bus and address
 */

static int
parse_ugen_name(char const *ugen, uint8_t *bus, uint8_t *addr)
{
	char *ep;

	if (strncmp(ugen, "ugen", 4) != 0)
		return (-1);

	*bus = (uint8_t) strtoul(ugen + 4, &ep, 10);
	if (*ep != '.')
		return (-1);

	*addr = (uint8_t) strtoul(ep + 1, &ep, 10);
	if (*ep != '\0')
		return (-1);

	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: rtlbtfw (-D) -d ugenX.Y (-f firmware path) (-I)\n");
	fprintf(stderr, "    -D: enable debugging\n");
	fprintf(stderr, "    -d: device to operate upon\n");
	fprintf(stderr, "    -f: firmware path, if not default\n");
	fprintf(stderr, "    -I: enable informational output\n");
	exit(127);
}

int
main(int argc, char *argv[])
{
	libusb_context *ctx = NULL;
	libusb_device *dev = NULL;
	libusb_device_handle *hdl = NULL;
	ng_hci_read_local_ver_rp ver;
	int r;
	uint8_t bus_id = 0, dev_id = 0;
	int devid_set = 0;
	int n;
	char *firmware_dir = NULL;
	char *firmware_path = NULL;
	char *config_path = NULL;
	const char *fw_suffix;
	int retcode = 1;
	const struct rtlbt_id_table *ic;
	uint8_t rom_version;
	struct rtlbt_firmware fw, cfg;
	enum rtlbt_fw_type fw_type;
	uint16_t fw_lmp_subversion;

	/* Parse command line arguments */
	while ((n = getopt(argc, argv, "Dd:f:hIm:p:v:")) != -1) {
		switch (n) {
		case 'd': /* ugen device name */
			devid_set = 1;
			if (parse_ugen_name(optarg, &bus_id, &dev_id) < 0)
				usage();
			break;
		case 'D':
			rtlbt_do_debug = 1;
			break;
		case 'f': /* firmware dir */
			if (firmware_dir)
				free(firmware_dir);
			firmware_dir = strdup(optarg);
			break;
		case 'I':
			rtlbt_do_info = 1;
			break;
		case 'h':
		default:
			usage();
			break;
			/* NOT REACHED */
		}
	}

	/* Ensure the devid was given! */
	if (devid_set == 0) {
		usage();
		/* NOTREACHED */
	}

	/* libusb setup */
	r = libusb_init(&ctx);
	if (r != 0) {
		rtlbt_err("libusb_init failed: code %d", r);
		exit(127);
	}

	rtlbt_debug("opening dev %d.%d", (int) bus_id, (int) dev_id);

	/* Find a device based on the bus/dev id */
	dev = rtlbt_find_device(ctx, bus_id, dev_id);
	if (dev == NULL) {
		rtlbt_err("device not found");
		goto shutdown;
	}

	/* XXX enforce that bInterfaceNumber is 0 */

	/* XXX enforce the device/product id if they're non-zero */

	/* Grab device handle */
	r = libusb_open(dev, &hdl);
	if (r != 0) {
		rtlbt_err("libusb_open() failed: code %d", r);
		goto shutdown;
	}

	/* Check if ng_ubt is attached */
	r = libusb_kernel_driver_active(hdl, 0);
	if (r < 0) {
		rtlbt_err("libusb_kernel_driver_active() failed: code %d", r);
		goto shutdown;
	}
	if (r > 0) {
		rtlbt_info("Firmware has already been downloaded");
		retcode = 0;
		goto shutdown;
	}

	/* Get local version */
	r = rtlbt_read_local_ver(hdl, &ver);
	if (r < 0) {
		rtlbt_err("rtlbt_read_local_ver() failed code %d", r);
		goto shutdown;
	}
	rtlbt_dump_version(&ver);

	ic = rtlbt_get_ic(ver.lmp_subversion, ver.hci_revision,
	    ver.hci_version);
	if (ic == NULL) {
		rtlbt_err("rtlbt_get_ic() failed: Unknown IC");
		goto shutdown;
	}

	/* Default the firmware path */
	if (firmware_dir == NULL)
		firmware_dir = strdup(_DEFAULT_RTLBT_FIRMWARE_PATH);

	fw_suffix = ic->fw_suffix == NULL ? "_fw.bin" : ic->fw_suffix;
	firmware_path = rtlbt_get_fwname(ic->fw_name, firmware_dir, fw_suffix);
	if (firmware_path == NULL)
		goto shutdown;

	rtlbt_debug("firmware_path = %s", firmware_path);

	rtlbt_info("loading firmware %s", firmware_path);

	/* Read in the firmware */
	if (rtlbt_fw_read(&fw, firmware_path) <= 0) {
		rtlbt_debug("rtlbt_fw_read() failed");
		return (-1);
	}

	fw_type = rtlbt_get_fw_type(&fw, &fw_lmp_subversion);
	if (fw_type == RTLBT_FW_TYPE_UNKNOWN &&
	    (ic->flags & RTLBT_IC_FLAG_SIMPLE) == 0) {
		rtlbt_debug("Unknown firmware type");
		goto shutdown;
	}

	if (fw_type != RTLBT_FW_TYPE_UNKNOWN) {

		/* Match hardware and firmware lmp_subversion */
		if (fw_lmp_subversion != ver.lmp_subversion) {
			rtlbt_err("firmware is for %x but this is a %x",
			    fw_lmp_subversion, ver.lmp_subversion);
			goto shutdown;
		}

		/* Query a ROM version */
		r = rtlbt_read_rom_ver(hdl, &rom_version);
		if (r < 0) {
			rtlbt_err("rtlbt_read_rom_ver() failed code %d", r);
			goto shutdown;
		}
		rtlbt_debug("rom_version = %d", rom_version);

		/* Load in the firmware */
		if (fw_type == RTLBT_FW_TYPE_V2) {
			uint8_t key_id, reg_val[2];
			r = rtlbt_read_reg16(hdl, RTLBT_SEC_PROJ, reg_val);
			if (r < 0) {
				rtlbt_err("rtlbt_read_reg16() failed code %d", r);
				goto shutdown;
			}
			key_id = reg_val[0];
			rtlbt_debug("key_id = %d", key_id);
			r = rtlbt_parse_fwfile_v2(&fw, rom_version, key_id);
		} else
			r = rtlbt_parse_fwfile_v1(&fw, rom_version);
		if (r < 0) {
			rtlbt_err("Parseing firmware file failed");
			goto shutdown;
		}

		config_path = rtlbt_get_fwname(ic->fw_name, firmware_dir,
		    "_config.bin");
		if (config_path == NULL)
			goto shutdown;

		rtlbt_info("loading config %s", config_path);

		/* Read in the config file */
		if (rtlbt_fw_read(&cfg, config_path) <= 0) {
			rtlbt_err("rtlbt_fw_read() failed");
			if ((ic->flags & RTLBT_IC_FLAG_CONFIG) != 0)
				goto shutdown;
		} else {
			r = rtlbt_append_fwfile(&fw, &cfg);
			rtlbt_fw_free(&cfg);
			if (r < 0) {
				rtlbt_err("Appending config file failed");
				goto shutdown;
			}
		}
	}

	r = rtlbt_load_fwfile(hdl, &fw);
	if (r < 0) {
		rtlbt_debug("Loading firmware file failed");
		goto shutdown;
	}

	/* free it */
	rtlbt_fw_free(&fw);

	rtlbt_info("Firmware download complete");

	/* Execute Read Local Version one more time */
	r = rtlbt_read_local_ver(hdl, &ver);
	if (r < 0) {
		rtlbt_err("rtlbt_read_local_ver() failed code %d", r);
		goto shutdown;
	}
	rtlbt_dump_version(&ver);

	retcode = 0;

	/* Ask kernel driver to probe and attach device again */
	r = libusb_reset_device(hdl);
	if (r != 0)
		rtlbt_err("libusb_reset_device() failed: %s",
		    libusb_strerror(r));

shutdown:

	/* Shutdown */

	if (hdl != NULL)
		libusb_close(hdl);

	if (dev != NULL)
		libusb_unref_device(dev);

	if (ctx != NULL)
		libusb_exit(ctx);

	if (retcode == 0)
		rtlbt_info("Firmware download is successful!");
	else
		rtlbt_err("Firmware download failed!");

	return (retcode);
}
