/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 Adrian Chadd <adrian@freebsd.org>
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 *
 * $FreeBSD$
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

#include "iwmbt_fw.h"
#include "iwmbt_hw.h"
#include "iwmbt_dbg.h"

#define	_DEFAULT_IWMBT_FIRMWARE_PATH	"/usr/share/firmware/intel"

int	iwmbt_do_debug = 0;
int	iwmbt_do_info = 0;

struct iwmbt_devid {
	uint16_t product_id;
	uint16_t vendor_id;
};

static struct iwmbt_devid iwmbt_list[] = {

	/* Intel Wireless 8260/8265 and successors */
	{ .vendor_id = 0x8087, .product_id = 0x0a2b },
	{ .vendor_id = 0x8087, .product_id = 0x0aaa },
	{ .vendor_id = 0x8087, .product_id = 0x0025 },
	{ .vendor_id = 0x8087, .product_id = 0x0026 },
	{ .vendor_id = 0x8087, .product_id = 0x0029 },
};

static int
iwmbt_is_8260(struct libusb_device_descriptor *d)
{
	int i;

	/* Search looking for whether it's an 8260/8265 */
	for (i = 0; i < (int) nitems(iwmbt_list); i++) {
		if ((iwmbt_list[i].product_id == d->idProduct) &&
		    (iwmbt_list[i].vendor_id == d->idVendor)) {
			iwmbt_info("found 8260/8265");
			return (1);
		}
	}

	/* Not found */
	return (0);
}

static libusb_device *
iwmbt_find_device(libusb_context *ctx, int bus_id, int dev_id)
{
	libusb_device **list, *dev = NULL, *found = NULL;
	struct libusb_device_descriptor d;
	ssize_t cnt, i;
	int r;

	cnt = libusb_get_device_list(ctx, &list);
	if (cnt < 0) {
		iwmbt_err("libusb_get_device_list() failed: code %lld",
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
				iwmbt_err("libusb_get_device_descriptor: %s",
				    libusb_strerror(r));
				break;
			}

			/* Match on the vendor/product id */
			if (iwmbt_is_8260(&d)) {
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
iwmbt_dump_version(struct iwmbt_version *ver)
{
	iwmbt_info("status       0x%02x", ver->status);
	iwmbt_info("hw_platform  0x%02x", ver->hw_platform);
	iwmbt_info("hw_variant   0x%02x", ver->hw_variant);
	iwmbt_info("hw_revision  0x%02x", ver->hw_revision);
	iwmbt_info("fw_variant   0x%02x", ver->fw_variant);
	iwmbt_info("fw_revision  0x%02x", ver->fw_revision);
	iwmbt_info("fw_build_num 0x%02x", ver->fw_build_num);
	iwmbt_info("fw_build_ww  0x%02x", ver->fw_build_ww);
	iwmbt_info("fw_build_yy  0x%02x", ver->fw_build_yy);
	iwmbt_info("fw_patch_num 0x%02x", ver->fw_patch_num);
}

static void
iwmbt_dump_boot_params(struct iwmbt_boot_params *params)
{
	iwmbt_info("Device revision: %u", le16toh(params->dev_revid));
	iwmbt_info("Secure Boot:  %s", params->secure_boot ? "on" : "off");
	iwmbt_info("OTP lock:     %s", params->otp_lock    ? "on" : "off");
	iwmbt_info("API lock:     %s", params->api_lock    ? "on" : "off");
	iwmbt_info("Debug lock:   %s", params->debug_lock  ? "on" : "off");
	iwmbt_info("Minimum firmware build %u week %u year %u",
	    params->min_fw_build_nn,
	    params->min_fw_build_cw,
	    2000 + params->min_fw_build_yy);
	iwmbt_info("OTC BD_ADDR:  %02x:%02x:%02x:%02x:%02x:%02x",
	    params->otp_bdaddr[5],
	    params->otp_bdaddr[4],
	    params->otp_bdaddr[3],
	    params->otp_bdaddr[2],
	    params->otp_bdaddr[1],
	    params->otp_bdaddr[0]);
}

static int
iwmbt_init_firmware(libusb_device_handle *hdl, const char *firmware_path,
    uint32_t *boot_param)
{
	struct iwmbt_firmware fw;
	int ret;

	iwmbt_debug("loading %s", firmware_path);

	/* Read in the firmware */
	if (iwmbt_fw_read(&fw, firmware_path) <= 0) {
		iwmbt_debug("iwmbt_fw_read() failed");
		return (-1);
	}

	/* Load in the firmware */
	ret = iwmbt_load_fwfile(hdl, &fw, boot_param);
	if (ret < 0)
		iwmbt_debug("Loading firmware file failed");

	/* free it */
	iwmbt_fw_free(&fw);

	return (ret);
}

static int
iwmbt_init_ddc(libusb_device_handle *hdl, const char *ddc_path)
{
	struct iwmbt_firmware ddc;
	int ret;

	iwmbt_debug("loading %s", ddc_path);

	/* Read in the DDC file */
	if (iwmbt_fw_read(&ddc, ddc_path) <= 0) {
		iwmbt_debug("iwmbt_fw_read() failed");
		return (-1);
	}

	/* Load in the DDC file */
	ret = iwmbt_load_ddc(hdl, &ddc);
	if (ret < 0)
		iwmbt_debug("Loading DDC file failed");

	/* free it */
	iwmbt_fw_free(&ddc);

	return (ret);
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
	    "Usage: iwmbtfw (-D) -d ugenX.Y (-f firmware path) (-I)\n");
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
	static struct iwmbt_version ver;
	static struct iwmbt_boot_params params;
	uint32_t boot_param;
	int r;
	uint8_t bus_id = 0, dev_id = 0;
	int devid_set = 0;
	int n;
	char *firmware_dir = NULL;
	char *firmware_path = NULL;
	int retcode = 1;

	/* Parse command line arguments */
	while ((n = getopt(argc, argv, "Dd:f:hIm:p:v:")) != -1) {
		switch (n) {
		case 'd': /* ugen device name */
			devid_set = 1;
			if (parse_ugen_name(optarg, &bus_id, &dev_id) < 0)
				usage();
			break;
		case 'D':
			iwmbt_do_debug = 1;
			break;
		case 'f': /* firmware dir */
			if (firmware_dir)
				free(firmware_dir);
			firmware_dir = strdup(optarg);
			break;
		case 'I':
			iwmbt_do_info = 1;
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
		iwmbt_err("libusb_init failed: code %d", r);
		exit(127);
	}

	iwmbt_debug("opening dev %d.%d", (int) bus_id, (int) dev_id);

	/* Find a device based on the bus/dev id */
	dev = iwmbt_find_device(ctx, bus_id, dev_id);
	if (dev == NULL) {
		iwmbt_err("device not found");
		goto shutdown;
	}

	/* XXX enforce that bInterfaceNumber is 0 */

	/* XXX enforce the device/product id if they're non-zero */

	/* Grab device handle */
	r = libusb_open(dev, &hdl);
	if (r != 0) {
		iwmbt_err("libusb_open() failed: code %d", r);
		goto shutdown;
	}

	/* Check if ng_ubt is attached */
	r = libusb_kernel_driver_active(hdl, 0);
	if (r < 0) {
		iwmbt_err("libusb_kernel_driver_active() failed: code %d", r);
		goto shutdown;
	}
	if (r > 0) {
		iwmbt_info("Firmware has already been downloaded");
		retcode = 0;
		goto shutdown;
	}

	/* Get Intel version */
	r = iwmbt_get_version(hdl, &ver);
	if (r < 0) {
		iwmbt_debug("iwmbt_get_version() failedL code %d", r);
		goto shutdown;
	}
	iwmbt_dump_version(&ver);
	iwmbt_debug("fw_variant=0x%02x", (int) ver.fw_variant);

	/* fw_variant = 0x06 bootloader mode / 0x23 operational mode */
	if (ver.fw_variant == 0x23) {
		iwmbt_info("Firmware has already been downloaded");
		retcode = 0;
		goto reset;
	}

	if (ver.fw_variant != 0x06){
		iwmbt_err("unknown fw_variant 0x%02x", (int) ver.fw_variant);
		goto shutdown;
	}

	/* Read Intel Secure Boot Params */
	r = iwmbt_get_boot_params(hdl, &params);
	if (r < 0) {
		iwmbt_debug("iwmbt_get_boot_params() failed!");
		goto shutdown;
	}
	iwmbt_dump_boot_params(&params);

	/* Check if firmware fragments are ACKed with a cmd complete event */
	if (params.limited_cce != 0x00) {
		iwmbt_err("Unsupported Intel firmware loading method (%u)",
		   params.limited_cce);
		goto shutdown;
	}

	/* Default the firmware path */
	if (firmware_dir == NULL)
		firmware_dir = strdup(_DEFAULT_IWMBT_FIRMWARE_PATH);

	firmware_path = iwmbt_get_fwname(&ver, &params, firmware_dir, "sfi");
	if (firmware_path == NULL)
		goto shutdown;

	iwmbt_debug("firmware_path = %s", firmware_path);

	/* Download firmware and parse it for magic Intel Reset parameter */
	r = iwmbt_init_firmware(hdl, firmware_path, &boot_param);
	free(firmware_path);
	if (r < 0)
		goto shutdown;

	iwmbt_info("Firmware download complete");

	r = iwmbt_intel_reset(hdl, boot_param);
	if (r < 0) {
		iwmbt_debug("iwmbt_intel_reset() failed!");
		goto shutdown;
	}

	iwmbt_info("Firmware operational");

	/* Once device is running in operational mode we can ignore failures */
	retcode = 0;

	/* Execute Read Intel Version one more time */
	r = iwmbt_get_version(hdl, &ver);
	if (r == 0)
		iwmbt_dump_version(&ver);

	/* Apply the device configuration (DDC) parameters */
	firmware_path = iwmbt_get_fwname(&ver, &params, firmware_dir, "ddc");
	iwmbt_debug("ddc_path = %s", firmware_path);
	if (firmware_path != NULL) {
		r = iwmbt_init_ddc(hdl, firmware_path);
		if (r == 0)
			iwmbt_info("DDC download complete");
		free(firmware_path);
	}

	/* Set Intel Event mask */
	r = iwmbt_set_event_mask(hdl);
	if (r == 0)
		iwmbt_info("Intel Event Mask is set");

reset:

	/* Ask kernel driver to probe and attach device again */
	r = libusb_reset_device(hdl);
	if (r != 0)
		iwmbt_err("libusb_reset_device() failed: %s",
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
		iwmbt_info("Firmware download is succesful!");
	else
		iwmbt_err("Firmware download failed!");

	return (retcode);
}
