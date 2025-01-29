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

#include "iwmbt_fw.h"
#include "iwmbt_hw.h"
#include "iwmbt_dbg.h"

#define	_DEFAULT_IWMBT_FIRMWARE_PATH	"/usr/share/firmware/intel"

int	iwmbt_do_debug = 0;
int	iwmbt_do_info = 0;

enum iwmbt_device {
	IWMBT_DEVICE_UNKNOWN,
	IWMBT_DEVICE_7260,
	IWMBT_DEVICE_8260,
	IWMBT_DEVICE_9260,
};

struct iwmbt_devid {
	uint16_t product_id;
	uint16_t vendor_id;
	enum iwmbt_device device;
};

static struct iwmbt_devid iwmbt_list[] = {

    /* Intel Wireless 7260/7265 and successors */
    { .vendor_id = 0x8087, .product_id = 0x07dc, .device = IWMBT_DEVICE_7260 },
    { .vendor_id = 0x8087, .product_id = 0x0a2a, .device = IWMBT_DEVICE_7260 },
    { .vendor_id = 0x8087, .product_id = 0x0aa7, .device = IWMBT_DEVICE_7260 },

    /* Intel Wireless 8260/8265 and successors */
    { .vendor_id = 0x8087, .product_id = 0x0a2b, .device = IWMBT_DEVICE_8260 },
    { .vendor_id = 0x8087, .product_id = 0x0aaa, .device = IWMBT_DEVICE_8260 },
    { .vendor_id = 0x8087, .product_id = 0x0025, .device = IWMBT_DEVICE_8260 },
    { .vendor_id = 0x8087, .product_id = 0x0026, .device = IWMBT_DEVICE_8260 },
    { .vendor_id = 0x8087, .product_id = 0x0029, .device = IWMBT_DEVICE_8260 },

    /* Intel Wireless 9260/9560 and successors */
    { .vendor_id = 0x8087, .product_id = 0x0032, .device = IWMBT_DEVICE_9260 },
    { .vendor_id = 0x8087, .product_id = 0x0033, .device = IWMBT_DEVICE_9260 },
};

static enum iwmbt_device
iwmbt_is_supported(struct libusb_device_descriptor *d)
{
	int i;

	/* Search looking for whether it's an 7260/7265 */
	for (i = 0; i < (int) nitems(iwmbt_list); i++) {
		if ((iwmbt_list[i].product_id == d->idProduct) &&
		    (iwmbt_list[i].vendor_id == d->idVendor)) {
			iwmbt_info("found iwmbtfw compatible");
			return (iwmbt_list[i].device);
		}
	}

	/* Not found */
	return (IWMBT_DEVICE_UNKNOWN);
}

static libusb_device *
iwmbt_find_device(libusb_context *ctx, int bus_id, int dev_id,
    enum iwmbt_device *iwmbt_device)
{
	libusb_device **list, *dev = NULL, *found = NULL;
	struct libusb_device_descriptor d;
	enum iwmbt_device device;
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
			device = iwmbt_is_supported(&d);
			if (device != IWMBT_DEVICE_UNKNOWN) {
				/*
				 * Take a reference so it's not freed later on.
				 */
				found = libusb_ref_device(dev);
				*iwmbt_device = device;
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

static void
iwmbt_dump_version_tlv(struct iwmbt_version_tlv *ver)
{
	iwmbt_info("cnvi_top     0x%08x", ver->cnvi_top);
	iwmbt_info("cnvr_top     0x%08x", ver->cnvr_top);
	iwmbt_info("cnvi_bt      0x%08x", ver->cnvi_bt);
	iwmbt_info("cnvr_bt      0x%08x", ver->cnvr_bt);
	iwmbt_info("dev_rev_id   0x%04x", ver->dev_rev_id);
	iwmbt_info("img_type     0x%02x", ver->img_type);
	iwmbt_info("timestamp    0x%04x", ver->timestamp);
	iwmbt_info("build_type   0x%02x", ver->build_type);
	iwmbt_info("build_num    0x%08x", ver->build_num);
	iwmbt_info("Secure Boot:  %s", ver->secure_boot ? "on" : "off");
	iwmbt_info("OTP lock:     %s", ver->otp_lock    ? "on" : "off");
	iwmbt_info("API lock:     %s", ver->api_lock    ? "on" : "off");
	iwmbt_info("Debug lock:   %s", ver->debug_lock  ? "on" : "off");
	iwmbt_info("Minimum firmware build %u week %u year %u",
	    ver->min_fw_build_nn,
	    ver->min_fw_build_cw,
	    2000 + ver->min_fw_build_yy);
	iwmbt_info("limited_cce  0x%02x", ver->limited_cce);
	iwmbt_info("sbe_type     0x%02x", ver->sbe_type);
	iwmbt_info("OTC BD_ADDR:  %02x:%02x:%02x:%02x:%02x:%02x",
	    ver->otp_bd_addr.b[5],
	    ver->otp_bd_addr.b[4],
	    ver->otp_bd_addr.b[3],
	    ver->otp_bd_addr.b[2],
	    ver->otp_bd_addr.b[1],
	    ver->otp_bd_addr.b[0]);
	if (ver->img_type == TLV_IMG_TYPE_BOOTLOADER ||
	    ver->img_type == TLV_IMG_TYPE_OPERATIONAL)
		iwmbt_info("%s timestamp %u.%u buildtype %u build %u",
		    (ver->img_type == TLV_IMG_TYPE_BOOTLOADER ?
		    "Bootloader" : "Firmware"),
		    2000 + (ver->timestamp >> 8),
		    ver->timestamp & 0xff,
		    ver->build_type,
		    ver->build_num);
}


static int
iwmbt_init_firmware(libusb_device_handle *hdl, const char *firmware_path,
    uint32_t *boot_param, uint8_t hw_variant, uint8_t sbe_type)
{
	struct iwmbt_firmware fw;
	int header_len, ret = -1;

	iwmbt_debug("loading %s", firmware_path);

	/* Read in the firmware */
	if (iwmbt_fw_read(&fw, firmware_path) <= 0) {
		iwmbt_debug("iwmbt_fw_read() failed");
		return (-1);
	}

	iwmbt_debug("Firmware file size=%d", fw.len);

	if (hw_variant <= 0x14) {
		/*
		 * Hardware variants 0x0b, 0x0c, 0x11 - 0x14 .sfi file have
		 * a RSA header of 644 bytes followed by Command Buffer.
		 */
		header_len = RSA_HEADER_LEN;
		if (fw.len < header_len) {
			iwmbt_err("Invalid size of firmware file (%d)", fw.len);
			ret = -1;
			goto exit;
		}

		/* Check if the CSS Header version is RSA(0x00010000) */
		if (le32dec(fw.buf + CSS_HEADER_OFFSET) != 0x00010000) {
			iwmbt_err("Invalid CSS Header version");
			ret = -1;
			goto exit;
		}

		/* Only RSA secure boot engine supported */
		if (sbe_type != 0x00) {
			iwmbt_err("Invalid SBE type for hardware variant (%d)",
			    hw_variant);
			ret = -1;
			goto exit;
		}

	} else if (hw_variant >= 0x17) {
		/*
		 * Hardware variants 0x17, 0x18 onwards support both RSA and
		 * ECDSA secure boot engine. As a result, the corresponding sfi
		 * file will have RSA header of 644, ECDSA header of 320 bytes
		 * followed by Command Buffer.
		 */
		header_len = ECDSA_OFFSET + ECDSA_HEADER_LEN;
		if (fw.len < header_len) {
			iwmbt_err("Invalid size of firmware file (%d)", fw.len);
			ret = -1;
			goto exit;
		}

		/* Check if CSS header for ECDSA follows the RSA header */
		if (fw.buf[ECDSA_OFFSET] != 0x06) {
			ret = -1;
			goto exit;
		}

		/* Check if the CSS Header version is ECDSA(0x00020000) */
		if (le32dec(fw.buf + ECDSA_OFFSET + CSS_HEADER_OFFSET) != 0x00020000) {
			iwmbt_err("Invalid CSS Header version");
			ret = -1;
			goto exit;
		}
	}

	/* Load in the CSS header */
	if (sbe_type == 0x00)
		ret = iwmbt_load_rsa_header(hdl, &fw);
	else if (sbe_type == 0x01)
		ret = iwmbt_load_ecdsa_header(hdl, &fw);
	if (ret < 0)
		goto exit;

	/* Load in the Command Buffer */
	ret = iwmbt_load_fwfile(hdl, &fw, boot_param, header_len);

exit:
	/* free firmware */
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
	    "Usage: iwmbtfw [-DI] -d ugenX.Y [-f firmware path]\n");
	fprintf(stderr, "    -D: enable debugging\n");
	fprintf(stderr, "    -d: device to operate upon\n");
	fprintf(stderr, "    -f: firmware path (defaults to %s)\n",
	    _DEFAULT_IWMBT_FIRMWARE_PATH);
	fprintf(stderr, "    -I: enable informational output\n");
	exit(127);
}



/*
 * Returns 0 on success.
 */
static int
handle_7260(libusb_device_handle *hdl, char *firmware_dir)
{
	int r;
	char *firmware_path;
	struct iwmbt_version ver;
	struct iwmbt_firmware fw;

	r = iwmbt_get_version(hdl, &ver);
	if (r < 0) {
		iwmbt_debug("iwmbt_get_version() failed code %d", r);
		return 1;
	}
	iwmbt_dump_version(&ver);
	iwmbt_debug("fw_patch_num=0x%02x", (int) ver.fw_patch_num);

	/* fw_patch_num = >0 operational mode */
	if (ver.fw_patch_num > 0x00) {
		iwmbt_info("Firmware has already been downloaded");
		return 0;
	}

	firmware_path = iwmbt_get_fwname(&ver, NULL, firmware_dir, "bseq");
	if (firmware_path == NULL)
		return 1;
	iwmbt_debug("firmware_path = %s", firmware_path);

	r = iwmbt_fw_read(&fw, firmware_path);
	free(firmware_path);
	if (r <= 0) {
		iwmbt_debug("iwmbt_fw_read() failed");
		return 1;
	}

	r = iwmbt_enter_manufacturer(hdl);
	if (r < 0) {
		iwmbt_debug("iwmbt_enter_manufacturer() failed code %d", r);
		iwmbt_fw_free(&fw);
		return 1;
	}

	/* Download firmware */
	r = iwmbt_patch_fwfile(hdl, &fw);
	iwmbt_fw_free(&fw);
	if (r < 0) {
		iwmbt_debug("Loading firmware file failed");
		(void)iwmbt_exit_manufacturer(hdl, IWMBT_MM_EXIT_COLD_RESET);
		return 1;
	}

	iwmbt_info("Firmware download complete");

	r = iwmbt_exit_manufacturer(hdl,
	    (r == 0 ? IWMBT_MM_EXIT_ONLY : IWMBT_MM_EXIT_WARM_RESET));
	if (r < 0) {
		iwmbt_debug("iwmbt_exit_manufacturer() failed code %d", r);
		return 1;
	}

	/* Once device is running in operational mode we can ignore failures */

	/* Dump actual controller version */
	r = iwmbt_get_version(hdl, &ver);
	if (r == 0)
		iwmbt_dump_version(&ver);

	if (iwmbt_enter_manufacturer(hdl) < 0)
		return 0;
	r = iwmbt_set_event_mask(hdl);
	if (r == 0)
		iwmbt_info("Intel Event Mask is set");
	(void)iwmbt_exit_manufacturer(hdl, IWMBT_MM_EXIT_ONLY);

	return 0;
}


/*
 * Returns 0 on success.
 */
static int
handle_8260(libusb_device_handle *hdl, char *firmware_dir)
{
	int r;
	uint32_t boot_param;
	struct iwmbt_version ver;
	struct iwmbt_boot_params params;
	char *firmware_path = NULL;

	r = iwmbt_get_version(hdl, &ver);
	if (r < 0) {
		iwmbt_debug("iwmbt_get_version() failed code %d", r);
		return 1;
	}
	iwmbt_dump_version(&ver);
	iwmbt_debug("fw_variant=0x%02x", (int) ver.fw_variant);

	if (ver.fw_variant == FW_VARIANT_OPERATIONAL) {
		iwmbt_info("Firmware has already been downloaded");
		return 0;
	}

	if (ver.fw_variant != FW_VARIANT_BOOTLOADER){
		iwmbt_err("unknown fw_variant 0x%02x", (int) ver.fw_variant);
		return 1;
	}

	/* Read Intel Secure Boot Params */
	r = iwmbt_get_boot_params(hdl, &params);
	if (r < 0) {
		iwmbt_debug("iwmbt_get_boot_params() failed!");
		return 1;
	}
	iwmbt_dump_boot_params(&params);

	/* Check if firmware fragments are ACKed with a cmd complete event */
	if (params.limited_cce != 0x00) {
		iwmbt_err("Unsupported Intel firmware loading method (%u)",
		   params.limited_cce);
		return 1;
	}

	firmware_path = iwmbt_get_fwname(&ver, &params, firmware_dir, "sfi");
	if (firmware_path == NULL)
		return 1;
	iwmbt_debug("firmware_path = %s", firmware_path);

	/* Download firmware and parse it for magic Intel Reset parameter */
	r = iwmbt_init_firmware(hdl, firmware_path, &boot_param, 0, 0);
	free(firmware_path);
	if (r < 0)
		return 1;

	iwmbt_info("Firmware download complete");

	r = iwmbt_intel_reset(hdl, boot_param);
	if (r < 0) {
		iwmbt_debug("iwmbt_intel_reset() failed!");
		return 1;
	}

	iwmbt_info("Firmware operational");

	/* Once device is running in operational mode we can ignore failures */

	/* Dump actual controller version */
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

	r = iwmbt_set_event_mask(hdl);
	if (r == 0)
		iwmbt_info("Intel Event Mask is set");

	return 0;
}


static int
handle_9260(libusb_device_handle *hdl, char *firmware_dir)
{
	int r;
	uint32_t boot_param;
	struct iwmbt_version vl;
	struct iwmbt_version_tlv vt;
	char *firmware_path = NULL;

	r = iwmbt_get_version_tlv(hdl, &vt);
	if (r < 0) {
		iwmbt_debug("iwmbt_get_version_tlv() failed code %d", r);
		return 1;
	}
	iwmbt_dump_version_tlv(&vt);
	iwmbt_debug("img_type=0x%02x", (int) vt.img_type);

	if (vt.img_type == TLV_IMG_TYPE_OPERATIONAL) {
		iwmbt_info("Firmware has already been downloaded");
		return 0;
	}

	if (vt.img_type != TLV_IMG_TYPE_BOOTLOADER) {
		iwmbt_err("unknown img_type 0x%02x", (int) vt.img_type);
		return 1;
	}

	/* Check if firmware fragments are ACKed with a cmd complete event */
	if (vt.limited_cce != 0x00) {
		iwmbt_err("Unsupported Intel firmware loading method (%u)",
		   vt.limited_cce);
		return 1;
	}

	/* Check if secure boot engine is supported: 1 (ECDSA) or 0 (RSA) */
	if (vt.sbe_type > 0x01) {
		iwmbt_err("Unsupported secure boot engine (%u)",
		   vt.sbe_type);
		return 1;
	}

	firmware_path = iwmbt_get_fwname_tlv(&vt, firmware_dir, "sfi");
	if (firmware_path == NULL)
		return 1;
	iwmbt_debug("firmware_path = %s", firmware_path);

	/* Download firmware and parse it for magic Intel Reset parameter */
	r = iwmbt_init_firmware(hdl, firmware_path, &boot_param,
	    vt.cnvi_bt >> 16 & 0x3f, vt.sbe_type);
	free(firmware_path);
	if (r < 0)
		return 1;

	iwmbt_info("Firmware download complete");

	r = iwmbt_intel_reset(hdl, boot_param);
	if (r < 0) {
		iwmbt_debug("iwmbt_intel_reset() failed!");
		return 1;
	}

	iwmbt_info("Firmware operational");

	/* Once device is running in operational mode we can ignore failures */

	r = iwmbt_get_version(hdl, &vl);
	if (r == 0)
		iwmbt_dump_version(&vl);

	/* Apply the device configuration (DDC) parameters */
	firmware_path = iwmbt_get_fwname_tlv(&vt, firmware_dir, "ddc");
	iwmbt_debug("ddc_path = %s", firmware_path);
	if (firmware_path != NULL) {
		r = iwmbt_init_ddc(hdl, firmware_path);
		if (r == 0)
			iwmbt_info("DDC download complete");
		free(firmware_path);
	}

	r = iwmbt_set_event_mask(hdl);
	if (r == 0)
		iwmbt_info("Intel Event Mask is set");

	return 0;
}


int
main(int argc, char *argv[])
{
	libusb_context *ctx = NULL;
	libusb_device *dev = NULL;
	libusb_device_handle *hdl = NULL;
	int r;
	uint8_t bus_id = 0, dev_id = 0;
	int devid_set = 0;
	int n;
	char *firmware_dir = NULL;
	int retcode = 1;
	enum iwmbt_device iwmbt_device;

	/* Parse command line arguments */
	while ((n = getopt(argc, argv, "Dd:f:hI")) != -1) {
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

	/* Default the firmware path */
	if (firmware_dir == NULL)
		firmware_dir = strdup(_DEFAULT_IWMBT_FIRMWARE_PATH);

	/* libusb setup */
	r = libusb_init(&ctx);
	if (r != 0) {
		iwmbt_err("libusb_init failed: code %d", r);
		exit(127);
	}

	iwmbt_debug("opening dev %d.%d", (int) bus_id, (int) dev_id);

	/* Find a device based on the bus/dev id */
	dev = iwmbt_find_device(ctx, bus_id, dev_id, &iwmbt_device);
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

	switch(iwmbt_device) {
	case IWMBT_DEVICE_7260:
		retcode = handle_7260(hdl, firmware_dir);
		break;
	case IWMBT_DEVICE_8260:
		retcode = handle_8260(hdl, firmware_dir);
		break;
	case IWMBT_DEVICE_9260:
		retcode = handle_9260(hdl, firmware_dir);
		break;
	default:
		iwmbt_err("FIXME: unknown iwmbt type %d", (int)iwmbt_device);
		retcode = 1;
	}

	if (retcode == 0) {
		/* Ask kernel driver to probe and attach device again */
		r = libusb_reset_device(hdl);
		if (r != 0)
			iwmbt_err("libusb_reset_device() failed: %s",
			    libusb_strerror(r));
	}

shutdown:
	if (hdl != NULL)
		libusb_close(hdl);

	if (dev != NULL)
		libusb_unref_device(dev);

	if (ctx != NULL)
		libusb_exit(ctx);

	if (retcode == 0)
		iwmbt_info("Firmware download is successful!");
	else
		iwmbt_err("Firmware download failed!");

	return (retcode);
}
