/*
 * ath3kfw.c
 */

/*-
 * Copyright (c) 2010 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
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

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <libusb20_desc.h>
#include <libusb20.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define ATH3KFW			"ath3kfw"
#define ATH3KFW_VENDOR_ID	0x0cf3
#define ATH3KFW_PRODUCT_ID	0x3000
#define ATH3KFW_FW		"/usr/local/etc/ath3k-1.fw"
#define ATH3KFW_BULK_EP		0x02
#define	ATH3KFW_REQ_DFU_DNLOAD	1
#define	ATH3KFW_MAX_BSIZE	4096

static int	parse_ugen_name		(char const *ugen, uint8_t *bus,
					 uint8_t *addr);
static int	find_device		(struct libusb20_backend *be,
					 uint8_t bus, uint8_t addr,
					 struct libusb20_device **dev);
static int	download_firmware	(struct libusb20_device *dev,
					 char const *firmware);
static void	usage			(void);

static int			vendor_id = ATH3KFW_VENDOR_ID;
static int			product_id = ATH3KFW_PRODUCT_ID;

/*
 * Firmware downloader for Atheros AR3011 based USB Bluetooth devices
 */

int
main(int argc, char **argv)
{
	uint8_t			bus, addr;
	char const		*firmware;
	struct libusb20_backend	*be;
	struct libusb20_device	*dev;
	int			n;

	openlog(ATH3KFW, LOG_NDELAY|LOG_PERROR|LOG_PID, LOG_USER);

	bus = 0;
	addr = 0;
	firmware = ATH3KFW_FW;

	while ((n = getopt(argc, argv, "d:f:hp:v:")) != -1) {
		switch (n) {
		case 'd': /* ugen device name */
			if (parse_ugen_name(optarg, &bus, &addr) < 0)
				usage();
			break;

		case 'f': /* firmware file */
			firmware = optarg;
			break;
		case 'p': /* product id */
			product_id = strtol(optarg, NULL, 0);
			break;
		case 'v': /* vendor id */
			vendor_id = strtol(optarg, NULL, 0);
			break;
		case 'h':
		default:
			usage();
			break;
			/* NOT REACHED */
		}
	}

	be = libusb20_be_alloc_default();
	if (be == NULL) {
		syslog(LOG_ERR, "libusb20_be_alloc_default() failed");
		return (-1);
	}

	if (find_device(be, bus, addr, &dev) < 0) {
		syslog(LOG_ERR, "ugen%d.%d is not recognized as " \
			"Atheros AR3011 based device " \
			"(possibly caused by lack of permissions)", bus, addr);
		return (-1);
	}

	if (download_firmware(dev, firmware) < 0) {
		syslog(LOG_ERR, "could not download %s firmare to ugen%d.%d",
			firmware, bus, addr);
		return (-1);
	}

	libusb20_be_free(be);
	closelog();
	
	return (0);
}

/*
 * Parse ugen name and extract device's bus and address
 */

static int
parse_ugen_name(char const *ugen, uint8_t *bus, uint8_t *addr)
{
	char	*ep;

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

/*
 * Find USB device
 */

static int
find_device(struct libusb20_backend *be, uint8_t bus, uint8_t addr,
		struct libusb20_device **dev)
{
	struct LIBUSB20_DEVICE_DESC_DECODED	*desc;

	*dev = NULL;

	while ((*dev = libusb20_be_device_foreach(be, *dev)) != NULL) {
		if (libusb20_dev_get_bus_number(*dev) != bus ||
		    libusb20_dev_get_address(*dev) != addr)
			continue;

		desc = libusb20_dev_get_device_desc(*dev);
		if (desc == NULL)
			continue;

		if (desc->idVendor != vendor_id ||
		    desc->idProduct != product_id)
			continue;

		break;
	}

	return ((*dev == NULL)? -1 : 0);
}

/*
 * Download firmware
 */

static int
download_firmware(struct libusb20_device *dev, char const *firmware)
{
	struct libusb20_transfer		*bulk;
	struct LIBUSB20_CONTROL_SETUP_DECODED	req;
	int					fd, n, error;
	uint8_t					buf[ATH3KFW_MAX_BSIZE];

	error = -1;

	if (libusb20_dev_open(dev, 1) != 0) {
		syslog(LOG_ERR, "libusb20_dev_open() failed");
		return (error);
	}

	if ((bulk = libusb20_tr_get_pointer(dev, 0)) == NULL) {
		syslog(LOG_ERR, "libusb20_tr_get_pointer() failed");
		goto out;
	}

	if (libusb20_tr_open(bulk, ATH3KFW_MAX_BSIZE, 1, ATH3KFW_BULK_EP) != 0) {
		syslog(LOG_ERR, "libusb20_tr_open(%d, 1, %d) failed",
			ATH3KFW_MAX_BSIZE, ATH3KFW_BULK_EP);
		goto out;
	}

	if ((fd = open(firmware, O_RDONLY)) < 0) {
		syslog(LOG_ERR, "open(%s) failed. %s",
			firmware, strerror(errno));
		goto out1;
	}

	n = read(fd, buf, 20);
	if (n != 20) {
		syslog(LOG_ERR, "read(%s, 20) failed. %s",
			firmware, strerror(errno));
		goto out2;
	}

	LIBUSB20_INIT(LIBUSB20_CONTROL_SETUP, &req);
	req.bmRequestType = LIBUSB20_REQUEST_TYPE_VENDOR;
	req.bRequest = ATH3KFW_REQ_DFU_DNLOAD;
	req.wLength = 20;

	if (libusb20_dev_request_sync(dev, &req, buf, NULL, 5000, 0) != 0) {
		syslog(LOG_ERR, "libusb20_dev_request_sync() failed");
		goto out2;
	}

	for (;;) {
		n = read(fd, buf, sizeof(buf));
		if (n < 0) {
			syslog(LOG_ERR, "read(%s, %d) failed. %s",
				firmware, (int) sizeof(buf), strerror(errno));
			goto out2;
		}
		if (n == 0)
			break;

		libusb20_tr_setup_bulk(bulk, buf, n, 3000);
		libusb20_tr_start(bulk);

		while (libusb20_dev_process(dev) == 0) {
			if (libusb20_tr_pending(bulk) == 0)
				break;

			libusb20_dev_wait_process(dev, -1);
		}

		if (libusb20_tr_get_status(bulk) != 0) {
			syslog(LOG_ERR, "bulk transfer failed with status %d",
				libusb20_tr_get_status(bulk));
			goto out2;
		}
	}

	error = 0;
out2:
	close(fd);
out1:
	libusb20_tr_close(bulk);
out:
	libusb20_dev_close(dev);

	return (error);
}

/*
 * Display usage and exit
 */

static void
usage(void)
{
	printf(
"Usage: %s -d ugenX.Y -f firmware_file\n"
"Usage: %s -h\n" \
"Where:\n" \
"\t-d ugenX.Y           ugen device name\n" \
"\t-f firmware image    firmware image file name for download\n" \
"\t-v vendor_id         vendor id\n" \
"\t-p vendor_id         product id\n" \
"\t-h                   display this message\n", ATH3KFW, ATH3KFW);

        exit(255);
}

