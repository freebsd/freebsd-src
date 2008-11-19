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

#include <sys/queue.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

#include "libusb20.h"
#include "libusb20_desc.h"
#include "libusb20_int.h"

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_revision.h>

static libusb20_init_backend_t ugen20_init_backend;
static libusb20_open_device_t ugen20_open_device;
static libusb20_close_device_t ugen20_close_device;
static libusb20_get_backend_name_t ugen20_get_backend_name;
static libusb20_exit_backend_t ugen20_exit_backend;
static libusb20_bus_set_owner_t ugen20_bus_set_owner;
static libusb20_bus_get_owner_t ugen20_bus_get_owner;
static libusb20_bus_set_perm_t ugen20_bus_set_perm;
static libusb20_bus_get_perm_t ugen20_bus_get_perm;
static libusb20_dev_get_iface_owner_t ugen20_dev_get_iface_owner;
static libusb20_dev_get_iface_perm_t ugen20_dev_get_iface_perm;
static libusb20_dev_get_owner_t ugen20_dev_get_owner;
static libusb20_dev_get_perm_t ugen20_dev_get_perm;
static libusb20_dev_set_iface_owner_t ugen20_dev_set_iface_owner;
static libusb20_dev_set_iface_perm_t ugen20_dev_set_iface_perm;
static libusb20_dev_set_owner_t ugen20_dev_set_owner;
static libusb20_dev_set_perm_t ugen20_dev_set_perm;
static libusb20_root_get_dev_quirk_t ugen20_root_get_dev_quirk;
static libusb20_root_get_quirk_name_t ugen20_root_get_quirk_name;
static libusb20_root_add_dev_quirk_t ugen20_root_add_dev_quirk;
static libusb20_root_remove_dev_quirk_t ugen20_root_remove_dev_quirk;
static libusb20_root_set_owner_t ugen20_root_set_owner;
static libusb20_root_get_owner_t ugen20_root_get_owner;
static libusb20_root_set_perm_t ugen20_root_set_perm;
static libusb20_root_get_perm_t ugen20_root_get_perm;

const struct libusb20_backend_methods libusb20_ugen20_backend = {
	LIBUSB20_BACKEND(LIBUSB20_DECLARE, ugen20)
};

/* USB device specific */
static libusb20_get_config_desc_full_t ugen20_get_config_desc_full;
static libusb20_get_config_index_t ugen20_get_config_index;
static libusb20_set_config_index_t ugen20_set_config_index;
static libusb20_claim_interface_t ugen20_claim_interface;
static libusb20_release_interface_t ugen20_release_interface;
static libusb20_set_alt_index_t ugen20_set_alt_index;
static libusb20_reset_device_t ugen20_reset_device;
static libusb20_set_power_mode_t ugen20_set_power_mode;
static libusb20_get_power_mode_t ugen20_get_power_mode;
static libusb20_kernel_driver_active_t ugen20_kernel_driver_active;
static libusb20_detach_kernel_driver_t ugen20_detach_kernel_driver;
static libusb20_do_request_sync_t ugen20_do_request_sync;
static libusb20_process_t ugen20_process;

/* USB transfer specific */
static libusb20_tr_open_t ugen20_tr_open;
static libusb20_tr_close_t ugen20_tr_close;
static libusb20_tr_clear_stall_sync_t ugen20_tr_clear_stall_sync;
static libusb20_tr_submit_t ugen20_tr_submit;
static libusb20_tr_cancel_async_t ugen20_tr_cancel_async;

static const struct libusb20_device_methods libusb20_ugen20_device_methods = {
	LIBUSB20_DEVICE(LIBUSB20_DECLARE, ugen20)
};

static const char *
ugen20_get_backend_name(void)
{
	return ("FreeBSD UGEN 2.0");
}

static uint32_t
ugen20_path_convert_one(const char **pp)
{
	const char *ptr;
	uint32_t temp = 0;

	ptr = *pp;

	while ((*ptr >= '0') && (*ptr <= '9')) {
		temp *= 10;
		temp += (*ptr - '0');
		if (temp >= 1000000) {
			/* catch overflow early */
			return (0 - 1);
		}
		ptr++;
	}

	if (*ptr == '.') {
		/* skip dot */
		ptr++;
	}
	*pp = ptr;

	return (temp);
}

static int
ugen20_enumerate(struct libusb20_device *pdev, const char *id)
{
	const char *tmp = id;
	struct usb2_device_descriptor ddesc;
	struct usb2_device_info devinfo;
	uint32_t plugtime;
	char buf[64];
	int f;
	int error;

	pdev->bus_number = ugen20_path_convert_one(&tmp);
	pdev->device_address = ugen20_path_convert_one(&tmp);

	snprintf(buf, sizeof(buf), "/dev/ugen%u.%u",
	    pdev->bus_number, pdev->device_address);

	f = open(buf, O_RDWR);
	if (f < 0) {
		return (LIBUSB20_ERROR_OTHER);
	}
	if (ioctl(f, USB_GET_PLUGTIME, &plugtime)) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	}
	/* store when the device was plugged */
	pdev->session_data.plugtime = plugtime;

	if (ioctl(f, USB_GET_DEVICE_DESC, &ddesc)) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	}
	LIBUSB20_INIT(LIBUSB20_DEVICE_DESC, &(pdev->ddesc));

	libusb20_me_decode(&ddesc, sizeof(ddesc), &(pdev->ddesc));

	if (pdev->ddesc.bNumConfigurations == 0) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	} else if (pdev->ddesc.bNumConfigurations >= 8) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	}
	if (ioctl(f, USB_GET_DEVICEINFO, &devinfo)) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	}
	switch (devinfo.udi_mode) {
	case USB_MODE_DEVICE:
		pdev->usb_mode = LIBUSB20_MODE_DEVICE;
		break;
	default:
		pdev->usb_mode = LIBUSB20_MODE_HOST;
		break;
	}

	switch (devinfo.udi_speed) {
	case USB_SPEED_LOW:
		pdev->usb_speed = LIBUSB20_SPEED_LOW;
		break;
	case USB_SPEED_FULL:
		pdev->usb_speed = LIBUSB20_SPEED_FULL;
		break;
	case USB_SPEED_HIGH:
		pdev->usb_speed = LIBUSB20_SPEED_HIGH;
		break;
	case USB_SPEED_VARIABLE:
		pdev->usb_speed = LIBUSB20_SPEED_VARIABLE;
		break;
	case USB_SPEED_SUPER:
		pdev->usb_speed = LIBUSB20_SPEED_SUPER;
		break;
	default:
		pdev->usb_speed = LIBUSB20_SPEED_UNKNOWN;
		break;
	}

	/* generate a nice description for printout */

	snprintf(pdev->usb_desc, sizeof(pdev->usb_desc),
	    "ugen%u.%u: <%s %s> at usbus%u", pdev->bus_number,
	    pdev->device_address, devinfo.udi_product,
	    devinfo.udi_vendor, pdev->bus_number);

	error = 0;
done:
	close(f);
	return (error);
}

struct ugen20_urd_state {
	struct usb2_read_dir urd;
	uint32_t nparsed;
	int	f;
	uint8_t *ptr;
	const char *src;
	const char *dst;
	uint8_t	buf[256];
	uint8_t	dummy_zero[1];
};

static int
ugen20_readdir(struct ugen20_urd_state *st)
{
	;				/* style fix */
repeat:
	if (st->ptr == NULL) {
		st->urd.urd_startentry += st->nparsed;
		st->urd.urd_data = st->buf;
		st->urd.urd_maxlen = sizeof(st->buf);
		st->nparsed = 0;

		if (ioctl(st->f, USB_READ_DIR, &st->urd)) {
			return (EINVAL);
		}
		st->ptr = st->buf;
	}
	if (st->ptr[0] == 0) {
		if (st->nparsed) {
			st->ptr = NULL;
			goto repeat;
		} else {
			return (ENXIO);
		}
	}
	st->src = (void *)(st->ptr + 1);
	st->dst = st->src + strlen(st->src) + 1;
	st->ptr = st->ptr + st->ptr[0];
	st->nparsed++;

	if ((st->ptr < st->buf) ||
	    (st->ptr > st->dummy_zero)) {
		/* invalid entry */
		return (EINVAL);
	}
	return (0);
}

static int
ugen20_init_backend(struct libusb20_backend *pbe)
{
	struct ugen20_urd_state state;
	struct libusb20_device *pdev;

	memset(&state, 0, sizeof(state));

	state.f = open("/dev/usb", O_RDONLY);
	if (state.f < 0)
		return (LIBUSB20_ERROR_OTHER);

	while (ugen20_readdir(&state) == 0) {

		if ((state.src[0] != 'u') ||
		    (state.src[1] != 'g') ||
		    (state.src[2] != 'e') ||
		    (state.src[3] != 'n')) {
			continue;
		}
		pdev = libusb20_dev_alloc();
		if (pdev == NULL) {
			continue;
		}
		if (ugen20_enumerate(pdev, state.src + 4)) {
			libusb20_dev_free(pdev);
			continue;
		}
		/* put the device on the backend list */
		libusb20_be_enqueue_device(pbe, pdev);
	}
	close(state.f);
	return (0);			/* success */
}

static int
ugen20_tr_renew(struct libusb20_device *pdev)
{
	struct usb2_fs_uninit fs_uninit;
	struct usb2_fs_init fs_init;
	struct usb2_fs_endpoint *pfse;
	int error;
	uint32_t size;
	uint16_t nMaxTransfer;

	nMaxTransfer = pdev->nTransfer;
	error = 0;

	if (nMaxTransfer == 0) {
		goto done;
	}
	size = nMaxTransfer * sizeof(*pfse);

	if (pdev->privBeData != NULL) {
		memset(&fs_uninit, 0, sizeof(fs_uninit));
		if (ioctl(pdev->file, USB_FS_UNINIT, &fs_uninit)) {
			/* ignore any errors of this kind */
		}
	} else {
		pfse = malloc(size);
		if (pfse == NULL) {
			error = LIBUSB20_ERROR_NO_MEM;
			goto done;
		}
		pdev->privBeData = pfse;
	}

	/* reset endpoint data */
	memset(pdev->privBeData, 0, size);

	memset(&fs_init, 0, sizeof(fs_init));

	fs_init.pEndpoints = pdev->privBeData;
	fs_init.ep_index_max = nMaxTransfer;

	if (ioctl(pdev->file, USB_FS_INIT, &fs_init)) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	}
done:
	return (error);
}

static int
ugen20_open_device(struct libusb20_device *pdev, uint16_t nMaxTransfer)
{
	uint32_t plugtime;
	char buf[64];
	int f;
	int g;
	int error;

	snprintf(buf, sizeof(buf), "/dev/ugen%u.%u",
	    pdev->bus_number, pdev->device_address);

	/*
	 * We need two file handles, one for the control endpoint and one
	 * for BULK, INTERRUPT and ISOCHRONOUS transactions due to optimised
	 * kernel locking.
	 */
	g = open(buf, O_RDWR);
	if (g < 0) {
		return (LIBUSB20_ERROR_NO_DEVICE);
	}
	f = open(buf, O_RDWR);
	if (f < 0) {
		close(g);
		return (LIBUSB20_ERROR_NO_DEVICE);
	}
	if (ioctl(f, USB_GET_PLUGTIME, &plugtime)) {
		error = LIBUSB20_ERROR_OTHER;
		goto done;
	}
	/* check that the correct device is still plugged */
	if (pdev->session_data.plugtime != plugtime) {
		error = LIBUSB20_ERROR_NO_DEVICE;
		goto done;
	}
	/* need to set this before "tr_renew()" */
	pdev->file = f;
	pdev->file_ctrl = g;

	/* renew all USB transfers */
	error = ugen20_tr_renew(pdev);
	if (error) {
		goto done;
	}
	/* set methods */
	pdev->methods = &libusb20_ugen20_device_methods;

done:
	if (error) {
		if (pdev->privBeData) {
			/* cleanup after "tr_renew()" */
			free(pdev->privBeData);
			pdev->privBeData = NULL;
		}
		pdev->file = -1;
		pdev->file_ctrl = -1;
		close(f);
		close(g);
	}
	return (error);
}

static int
ugen20_close_device(struct libusb20_device *pdev)
{
	struct usb2_fs_uninit fs_uninit;
	int error = 0;

	if (pdev->privBeData) {
		memset(&fs_uninit, 0, sizeof(fs_uninit));
		if (ioctl(pdev->file, USB_FS_UNINIT, &fs_uninit)) {
			error = LIBUSB20_ERROR_OTHER;
		}
		free(pdev->privBeData);
	}
	pdev->nTransfer = 0;
	pdev->privBeData = NULL;
	close(pdev->file);
	close(pdev->file_ctrl);
	pdev->file = -1;
	pdev->file_ctrl = -1;
	return (error);
}

static void
ugen20_exit_backend(struct libusb20_backend *pbe)
{
	return;				/* nothing to do */
}

static int
ugen20_get_config_desc_full(struct libusb20_device *pdev,
    uint8_t **ppbuf, uint16_t *plen, uint8_t cfg_index)
{
	struct usb2_gen_descriptor gen_desc;
	struct usb2_config_descriptor cdesc;
	uint8_t *ptr;
	uint16_t len;
	int error;

	memset(&gen_desc, 0, sizeof(gen_desc));

	gen_desc.ugd_data = &cdesc;
	gen_desc.ugd_maxlen = sizeof(cdesc);
	gen_desc.ugd_config_index = cfg_index;

	error = ioctl(pdev->file_ctrl, USB_GET_FULL_DESC, &gen_desc);
	if (error) {
		return (LIBUSB20_ERROR_OTHER);
	}
	len = UGETW(cdesc.wTotalLength);
	if (len < sizeof(cdesc)) {
		/* corrupt descriptor */
		return (LIBUSB20_ERROR_OTHER);
	}
	ptr = malloc(len);
	if (!ptr) {
		return (LIBUSB20_ERROR_NO_MEM);
	}
	gen_desc.ugd_data = ptr;
	gen_desc.ugd_maxlen = len;

	error = ioctl(pdev->file_ctrl, USB_GET_FULL_DESC, &gen_desc);
	if (error) {
		free(ptr);
		return (LIBUSB20_ERROR_OTHER);
	}
	/* make sure that the device doesn't fool us */
	memcpy(ptr, &cdesc, sizeof(cdesc));

	*ppbuf = ptr;
	*plen = len;

	return (0);			/* success */
}

static int
ugen20_get_config_index(struct libusb20_device *pdev, uint8_t *pindex)
{
	int temp;

	if (ioctl(pdev->file_ctrl, USB_GET_CONFIG, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	*pindex = temp;

	return (0);
}

static int
ugen20_set_config_index(struct libusb20_device *pdev, uint8_t cfg_index)
{
	int temp = cfg_index;

	if (ioctl(pdev->file_ctrl, USB_SET_CONFIG, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (ugen20_tr_renew(pdev));
}

static int
ugen20_claim_interface(struct libusb20_device *pdev, uint8_t iface_index)
{
	int temp = iface_index;

	if (ioctl(pdev->file_ctrl, USB_CLAIM_INTERFACE, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (0);
}

static int
ugen20_release_interface(struct libusb20_device *pdev, uint8_t iface_index)
{
	int temp = iface_index;

	if (ioctl(pdev->file_ctrl, USB_RELEASE_INTERFACE, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (0);
}

static int
ugen20_set_alt_index(struct libusb20_device *pdev,
    uint8_t iface_index, uint8_t alt_index)
{
	struct usb2_alt_interface alt_iface;

	memset(&alt_iface, 0, sizeof(alt_iface));

	alt_iface.uai_interface_index = iface_index;
	alt_iface.uai_alt_index = alt_index;

	if (ioctl(pdev->file_ctrl, USB_SET_ALTINTERFACE, &alt_iface)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (ugen20_tr_renew(pdev));
}

static int
ugen20_reset_device(struct libusb20_device *pdev)
{
	int temp = 0;

	if (ioctl(pdev->file_ctrl, USB_DEVICEENUMERATE, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (ugen20_tr_renew(pdev));
}

static int
ugen20_set_power_mode(struct libusb20_device *pdev, uint8_t power_mode)
{
	int temp;

	switch (power_mode) {
	case LIBUSB20_POWER_OFF:
		temp = USB_POWER_MODE_OFF;
		break;
	case LIBUSB20_POWER_ON:
		temp = USB_POWER_MODE_ON;
		break;
	case LIBUSB20_POWER_SAVE:
		temp = USB_POWER_MODE_SAVE;
		break;
	case LIBUSB20_POWER_SUSPEND:
		temp = USB_POWER_MODE_SUSPEND;
		break;
	case LIBUSB20_POWER_RESUME:
		temp = USB_POWER_MODE_RESUME;
		break;
	default:
		return (LIBUSB20_ERROR_INVALID_PARAM);
	}
	if (ioctl(pdev->file_ctrl, USB_SET_POWER_MODE, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (0);
}

static int
ugen20_get_power_mode(struct libusb20_device *pdev, uint8_t *power_mode)
{
	int temp;

	if (ioctl(pdev->file_ctrl, USB_GET_POWER_MODE, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	switch (temp) {
	case USB_POWER_MODE_OFF:
		temp = LIBUSB20_POWER_OFF;
		break;
	case USB_POWER_MODE_ON:
		temp = LIBUSB20_POWER_ON;
		break;
	case USB_POWER_MODE_SAVE:
		temp = LIBUSB20_POWER_SAVE;
		break;
	case USB_POWER_MODE_SUSPEND:
		temp = LIBUSB20_POWER_SUSPEND;
		break;
	case USB_POWER_MODE_RESUME:
		temp = LIBUSB20_POWER_RESUME;
		break;
	default:
		temp = LIBUSB20_POWER_ON;
		break;
	}
	*power_mode = temp;
	return (0);			/* success */
}

static int
ugen20_kernel_driver_active(struct libusb20_device *pdev,
    uint8_t iface_index)
{
	int temp = iface_index;

	if (ioctl(pdev->file_ctrl, USB_IFACE_DRIVER_ACTIVE, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (0);			/* kernel driver is active */
}

static int
ugen20_detach_kernel_driver(struct libusb20_device *pdev,
    uint8_t iface_index)
{
	int temp = iface_index;

	if (ioctl(pdev->file_ctrl, USB_IFACE_DRIVER_DETACH, &temp)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	return (0);			/* kernel driver is active */
}

static int
ugen20_do_request_sync(struct libusb20_device *pdev,
    struct LIBUSB20_CONTROL_SETUP_DECODED *setup,
    void *data, uint16_t *pactlen, uint32_t timeout, uint8_t flags)
{
	struct usb2_ctl_request req;

	memset(&req, 0, sizeof(req));

	req.ucr_data = data;
	if (!(flags & LIBUSB20_TRANSFER_SINGLE_SHORT_NOT_OK)) {
		req.ucr_flags |= USB_SHORT_XFER_OK;
	}
	if (libusb20_me_encode(&req.ucr_request,
	    sizeof(req.ucr_request), setup)) {
		/* ignore */
	}
	if (ioctl(pdev->file_ctrl, USB_DO_REQUEST, &req)) {
		return (LIBUSB20_ERROR_OTHER);
	}
	if (pactlen) {
		/* get actual length */
		*pactlen = req.ucr_actlen;
	}
	return (0);			/* kernel driver is active */
}

static int
ugen20_process(struct libusb20_device *pdev)
{
	struct usb2_fs_complete temp;
	struct usb2_fs_endpoint *fsep;
	struct libusb20_transfer *xfer;

	while (1) {

		if (ioctl(pdev->file, USB_FS_COMPLETE, &temp)) {
			if (errno == EBUSY) {
				break;
			} else {
				/* device detached */
				return (LIBUSB20_ERROR_OTHER);
			}
		}
		fsep = pdev->privBeData;
		xfer = pdev->pTransfer;
		fsep += temp.ep_index;
		xfer += temp.ep_index;

		/* update transfer status */

		if (fsep->status == 0) {
			xfer->aFrames = fsep->aFrames;
			xfer->timeComplete = fsep->isoc_time_complete;
			xfer->status = LIBUSB20_TRANSFER_COMPLETED;
		} else if (fsep->status == USB_ERR_CANCELLED) {
			xfer->aFrames = 0;
			xfer->timeComplete = 0;
			xfer->status = LIBUSB20_TRANSFER_CANCELLED;
		} else if (fsep->status == USB_ERR_STALLED) {
			xfer->aFrames = 0;
			xfer->timeComplete = 0;
			xfer->status = LIBUSB20_TRANSFER_STALL;
		} else if (fsep->status == USB_ERR_TIMEOUT) {
			xfer->aFrames = 0;
			xfer->timeComplete = 0;
			xfer->status = LIBUSB20_TRANSFER_TIMED_OUT;
		} else {
			xfer->aFrames = 0;
			xfer->timeComplete = 0;
			xfer->status = LIBUSB20_TRANSFER_ERROR;
		}
		libusb20_tr_callback_wrapper(xfer);
	}
	return (0);			/* done */
}

static int
ugen20_tr_open(struct libusb20_transfer *xfer, uint32_t MaxBufSize,
    uint32_t MaxFrameCount, uint8_t ep_no)
{
	struct usb2_fs_open temp;
	struct usb2_fs_endpoint *fsep;

	memset(&temp, 0, sizeof(temp));

	fsep = xfer->pdev->privBeData;
	fsep += xfer->trIndex;

	temp.max_bufsize = MaxBufSize;
	temp.max_frames = MaxFrameCount;
	temp.ep_index = xfer->trIndex;
	temp.ep_no = ep_no;

	if (ioctl(xfer->pdev->file, USB_FS_OPEN, &temp)) {
		return (LIBUSB20_ERROR_INVALID_PARAM);
	}
	/* maximums might have changed - update */
	xfer->maxFrames = temp.max_frames;

	/* "max_bufsize" should be multiple of "max_packet_length" */
	xfer->maxTotalLength = temp.max_bufsize;
	xfer->maxPacketLen = temp.max_packet_length;

	/* setup buffer and length lists */
	fsep->ppBuffer = xfer->ppBuffer;/* zero copy */
	fsep->pLength = xfer->pLength;	/* zero copy */

	return (0);			/* success */
}

static int
ugen20_tr_close(struct libusb20_transfer *xfer)
{
	struct usb2_fs_close temp;

	memset(&temp, 0, sizeof(temp));

	temp.ep_index = xfer->trIndex;

	if (ioctl(xfer->pdev->file, USB_FS_CLOSE, &temp)) {
		return (LIBUSB20_ERROR_INVALID_PARAM);
	}
	return (0);			/* success */
}

static int
ugen20_tr_clear_stall_sync(struct libusb20_transfer *xfer)
{
	struct usb2_fs_clear_stall_sync temp;

	memset(&temp, 0, sizeof(temp));

	/* if the transfer is active, an error will be returned */

	temp.ep_index = xfer->trIndex;

	if (ioctl(xfer->pdev->file, USB_FS_CLEAR_STALL_SYNC, &temp)) {
		return (LIBUSB20_ERROR_INVALID_PARAM);
	}
	return (0);			/* success */
}

static void
ugen20_tr_submit(struct libusb20_transfer *xfer)
{
	struct usb2_fs_start temp;
	struct usb2_fs_endpoint *fsep;

	memset(&temp, 0, sizeof(temp));

	fsep = xfer->pdev->privBeData;
	fsep += xfer->trIndex;

	fsep->nFrames = xfer->nFrames;
	fsep->flags = 0;
	if (!(xfer->flags & LIBUSB20_TRANSFER_SINGLE_SHORT_NOT_OK)) {
		fsep->flags |= USB_FS_FLAG_SINGLE_SHORT_OK;
	}
	if (!(xfer->flags & LIBUSB20_TRANSFER_MULTI_SHORT_NOT_OK)) {
		fsep->flags |= USB_FS_FLAG_MULTI_SHORT_OK;
	}
	if (xfer->flags & LIBUSB20_TRANSFER_FORCE_SHORT) {
		fsep->flags |= USB_FS_FLAG_FORCE_SHORT;
	}
	if (xfer->flags & LIBUSB20_TRANSFER_DO_CLEAR_STALL) {
		fsep->flags |= USB_FS_FLAG_CLEAR_STALL;
	}
	fsep->timeout = xfer->timeout;

	temp.ep_index = xfer->trIndex;

	if (ioctl(xfer->pdev->file, USB_FS_START, &temp)) {
		/* ignore any errors - should never happen */
	}
	return;				/* success */
}

static void
ugen20_tr_cancel_async(struct libusb20_transfer *xfer)
{
	struct usb2_fs_stop temp;

	memset(&temp, 0, sizeof(temp));

	temp.ep_index = xfer->trIndex;

	if (ioctl(xfer->pdev->file, USB_FS_STOP, &temp)) {
		/* ignore any errors - should never happen */
	}
	return;
}

static int
ugen20_be_ioctl(uint32_t cmd, void *data)
{
	int f;
	int error;

	f = open("/dev/usb", O_RDONLY);
	if (f < 0)
		return (LIBUSB20_ERROR_OTHER);
	error = ioctl(f, cmd, data);
	if (error == -1) {
		if (errno == EPERM) {
			error = LIBUSB20_ERROR_ACCESS;
		} else {
			error = LIBUSB20_ERROR_OTHER;
		}
	}
	close(f);
	return (error);
}

static int
ugen20_be_do_perm(uint32_t get_cmd, uint32_t set_cmd, uint8_t bus,
    uint8_t dev, uint8_t iface, uid_t *uid,
    gid_t *gid, mode_t *mode)
{
	struct usb2_dev_perm perm;
	int error;

	memset(&perm, 0, sizeof(perm));

	perm.bus_index = bus;
	perm.dev_index = dev;
	perm.iface_index = iface;

	error = ugen20_be_ioctl(get_cmd, &perm);
	if (error)
		return (error);

	if (set_cmd == 0) {
		if (uid)
			*uid = perm.user_id;
		if (gid)
			*gid = perm.group_id;
		if (mode)
			*mode = perm.mode;
		return (0);
	}
	if (uid)
		perm.user_id = *uid;
	if (gid)
		perm.group_id = *gid;
	if (mode)
		perm.mode = *mode;

	return (ugen20_be_ioctl(set_cmd, &perm));
}

static int
ugen20_bus_set_owner(struct libusb20_backend *pbe,
    uint8_t bus, uid_t user, gid_t group)
{
	return (ugen20_be_do_perm(USB_GET_BUS_PERM, USB_SET_BUS_PERM,
	    bus, 0, 0, &user, &group, NULL));
}

static int
ugen20_bus_get_owner(struct libusb20_backend *pbe, uint8_t bus,
    uid_t *user, gid_t *group)
{
	return (ugen20_be_do_perm(USB_GET_BUS_PERM, 0,
	    bus, 0, 0, user, group, NULL));
}

static int
ugen20_bus_set_perm(struct libusb20_backend *pbe,
    uint8_t bus, mode_t mode)
{
	return (ugen20_be_do_perm(USB_GET_BUS_PERM, USB_SET_BUS_PERM,
	    bus, 0, 0, NULL, NULL, &mode));
}

static int
ugen20_bus_get_perm(struct libusb20_backend *pbe,
    uint8_t bus, mode_t *mode)
{
	return (ugen20_be_do_perm(USB_GET_BUS_PERM, 0,
	    bus, 0, 0, NULL, NULL, mode));
}

static int
ugen20_dev_get_iface_owner(struct libusb20_device *pdev,
    uint8_t iface_index, uid_t *user, gid_t *group)
{
	return (ugen20_be_do_perm(USB_GET_IFACE_PERM, 0,
	    pdev->bus_number, pdev->device_address, iface_index,
	    user, group, NULL));
}

static int
ugen20_dev_get_iface_perm(struct libusb20_device *pdev,
    uint8_t iface_index, mode_t *mode)
{
	return (ugen20_be_do_perm(USB_GET_IFACE_PERM, 0,
	    pdev->bus_number, pdev->device_address, iface_index,
	    NULL, NULL, mode));
}

static int
ugen20_dev_get_owner(struct libusb20_device *pdev,
    uid_t *user, gid_t *group)
{
	return (ugen20_be_do_perm(USB_GET_DEVICE_PERM, 0,
	    pdev->bus_number, pdev->device_address, 0,
	    user, group, NULL));
}

static int
ugen20_dev_get_perm(struct libusb20_device *pdev, mode_t *mode)
{
	return (ugen20_be_do_perm(USB_GET_DEVICE_PERM, 0,
	    pdev->bus_number, pdev->device_address, 0,
	    NULL, NULL, mode));
}

static int
ugen20_dev_set_iface_owner(struct libusb20_device *pdev,
    uint8_t iface_index, uid_t user, gid_t group)
{
	return (ugen20_be_do_perm(USB_GET_IFACE_PERM, USB_SET_IFACE_PERM,
	    pdev->bus_number, pdev->device_address, iface_index,
	    &user, &group, NULL));
}

static int
ugen20_dev_set_iface_perm(struct libusb20_device *pdev,
    uint8_t iface_index, mode_t mode)
{
	return (ugen20_be_do_perm(USB_GET_IFACE_PERM, USB_SET_IFACE_PERM,
	    pdev->bus_number, pdev->device_address, iface_index,
	    NULL, NULL, &mode));
}

static int
ugen20_root_get_dev_quirk(struct libusb20_backend *pbe,
    uint16_t quirk_index, struct libusb20_quirk *pq)
{
	struct usb2_gen_quirk q;
	int error;

	memset(&q, 0, sizeof(q));

	q.index = quirk_index;

	error = ugen20_be_ioctl(USB_DEV_QUIRK_GET, &q);

	if (error) {
		if (errno == EINVAL) {
			return (LIBUSB20_ERROR_NOT_FOUND);
		}
	} else {
		pq->vid = q.vid;
		pq->pid = q.pid;
		pq->bcdDeviceLow = q.bcdDeviceLow;
		pq->bcdDeviceHigh = q.bcdDeviceHigh;
		strlcpy(pq->quirkname, q.quirkname, sizeof(pq->quirkname));
	}
	return (error);
}

static int
ugen20_root_get_quirk_name(struct libusb20_backend *pbe, uint16_t quirk_index,
    struct libusb20_quirk *pq)
{
	struct usb2_gen_quirk q;
	int error;

	memset(&q, 0, sizeof(q));

	q.index = quirk_index;

	error = ugen20_be_ioctl(USB_QUIRK_NAME_GET, &q);

	if (error) {
		if (errno == EINVAL) {
			return (LIBUSB20_ERROR_NOT_FOUND);
		}
	} else {
		strlcpy(pq->quirkname, q.quirkname, sizeof(pq->quirkname));
	}
	return (error);
}

static int
ugen20_root_add_dev_quirk(struct libusb20_backend *pbe,
    struct libusb20_quirk *pq)
{
	struct usb2_gen_quirk q;
	int error;

	memset(&q, 0, sizeof(q));

	q.vid = pq->vid;
	q.pid = pq->pid;
	q.bcdDeviceLow = pq->bcdDeviceLow;
	q.bcdDeviceHigh = pq->bcdDeviceHigh;
	strlcpy(q.quirkname, pq->quirkname, sizeof(q.quirkname));

	error = ugen20_be_ioctl(USB_DEV_QUIRK_ADD, &q);
	if (error) {
		if (errno == ENOMEM) {
			return (LIBUSB20_ERROR_NO_MEM);
		}
	}
	return (error);
}

static int
ugen20_root_remove_dev_quirk(struct libusb20_backend *pbe,
    struct libusb20_quirk *pq)
{
	struct usb2_gen_quirk q;
	int error;

	memset(&q, 0, sizeof(q));

	q.vid = pq->vid;
	q.pid = pq->pid;
	q.bcdDeviceLow = pq->bcdDeviceLow;
	q.bcdDeviceHigh = pq->bcdDeviceHigh;
	strlcpy(q.quirkname, pq->quirkname, sizeof(q.quirkname));

	error = ugen20_be_ioctl(USB_DEV_QUIRK_REMOVE, &q);
	if (error) {
		if (errno == EINVAL) {
			return (LIBUSB20_ERROR_NOT_FOUND);
		}
	}
	return (error);
}

static int
ugen20_dev_set_owner(struct libusb20_device *pdev,
    uid_t user, gid_t group)
{
	return (ugen20_be_do_perm(USB_GET_DEVICE_PERM, USB_SET_DEVICE_PERM,
	    pdev->bus_number, pdev->device_address, 0,
	    &user, &group, NULL));
}

static int
ugen20_dev_set_perm(struct libusb20_device *pdev, mode_t mode)
{
	return (ugen20_be_do_perm(USB_GET_DEVICE_PERM, USB_SET_DEVICE_PERM,
	    pdev->bus_number, pdev->device_address, 0,
	    NULL, NULL, &mode));
}

static int
ugen20_root_set_owner(struct libusb20_backend *pbe,
    uid_t user, gid_t group)
{
	return (ugen20_be_do_perm(USB_GET_ROOT_PERM, USB_SET_ROOT_PERM, 0, 0, 0,
	    &user, &group, NULL));
}

static int
ugen20_root_get_owner(struct libusb20_backend *pbe, uid_t *user, gid_t *group)
{
	return (ugen20_be_do_perm(USB_GET_ROOT_PERM, 0, 0, 0, 0,
	    user, group, NULL));
}

static int
ugen20_root_set_perm(struct libusb20_backend *pbe, mode_t mode)
{
	return (ugen20_be_do_perm(USB_GET_ROOT_PERM, USB_SET_ROOT_PERM, 0, 0, 0,
	    NULL, NULL, &mode));
}

static int
ugen20_root_get_perm(struct libusb20_backend *pbe, mode_t *mode)
{
	return (ugen20_be_do_perm(USB_GET_ROOT_PERM, 0, 0, 0, 0,
	    NULL, NULL, mode));
}
