/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <sys/endian.h>
#include <sys/stat.h>

#include <netgraph/bluetooth/include/ng_hci.h>

#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libusb.h>

#include "rtlbt_fw.h"
#include "rtlbt_hw.h"
#include "rtlbt_dbg.h"

static int
rtlbt_hci_command(struct libusb_device_handle *hdl, struct rtlbt_hci_cmd *cmd,
    void *event, int size, int *transferred, int timeout)
{
	struct timespec to, now, remains;
	int ret;

	ret = libusb_control_transfer(hdl,
	    LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
	    0,
	    0,
	    0,
	    (uint8_t *)cmd,
	    RTLBT_HCI_CMD_SIZE(cmd),
	    timeout);

	if (ret < 0) {
		rtlbt_err("libusb_control_transfer() failed: err=%s",
		    libusb_strerror(ret));
		return (ret);
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	to = RTLBT_MSEC2TS(timeout);
	timespecadd(&to, &now, &to);

	do {
		timespecsub(&to, &now, &remains);
		ret = libusb_interrupt_transfer(hdl,
		    RTLBT_INTERRUPT_ENDPOINT_ADDR,
		    event,
		    size,
		    transferred,
		    RTLBT_TS2MSEC(remains) + 1);

		if (ret < 0) {
			rtlbt_err("libusb_interrupt_transfer() failed: err=%s",
			    libusb_strerror(ret));
			return (ret);
		}

		switch (((struct rtlbt_hci_event *)event)->header.event) {
		case NG_HCI_EVENT_COMMAND_COMPL:
			if (*transferred <
			    (int)offsetof(struct rtlbt_hci_event_cmd_compl, data))
				break;
			if (cmd->opcode !=
			    ((struct rtlbt_hci_event_cmd_compl *)event)->opcode)
				break;
			return (0);
		default:
			break;
		}
		rtlbt_debug("Stray HCI event: %x",
		    ((struct rtlbt_hci_event *)event)->header.event);
	} while (timespeccmp(&to, &now, >));

	rtlbt_err("libusb_interrupt_transfer() failed: err=%s",
	    libusb_strerror(LIBUSB_ERROR_TIMEOUT));

	return (LIBUSB_ERROR_TIMEOUT);
}

int
rtlbt_read_local_ver(struct libusb_device_handle *hdl,
    ng_hci_read_local_ver_rp *ver)
{
	int ret, transferred;
	struct rtlbt_hci_event_cmd_compl *event;
	struct rtlbt_hci_cmd cmd = {
		.opcode = htole16(NG_HCI_OPCODE(NG_HCI_OGF_INFO,
		    NG_HCI_OCF_READ_LOCAL_VER)),
		.length = 0,
	};
	uint8_t buf[RTLBT_HCI_EVT_COMPL_SIZE(ng_hci_read_local_ver_rp)];

	memset(buf, 0, sizeof(buf));

	ret = rtlbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    RTLBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred != sizeof(buf)) {
		 rtlbt_debug("Can't read local version: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	event = (struct rtlbt_hci_event_cmd_compl *)buf;
	memcpy(ver, event->data, sizeof(ng_hci_read_local_ver_rp));

	return (0);
}

int
rtlbt_read_rom_ver(struct libusb_device_handle *hdl, uint8_t *ver)
{
	int ret, transferred;
	struct rtlbt_hci_event_cmd_compl *event;
	struct rtlbt_hci_cmd cmd = {
		.opcode = htole16(0xfc6d),
		.length = 0,
	};
	uint8_t buf[RTLBT_HCI_EVT_COMPL_SIZE(struct rtlbt_rom_ver_rp)];

	memset(buf, 0, sizeof(buf));

	ret = rtlbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    RTLBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred != sizeof(buf)) {
		 rtlbt_debug("Can't read ROM version: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	event = (struct rtlbt_hci_event_cmd_compl *)buf;
	*ver = ((struct rtlbt_rom_ver_rp *)event->data)->version;

	return (0);
}

int
rtlbt_read_reg16(struct libusb_device_handle *hdl,
    struct rtlbt_vendor_cmd *vcmd, uint8_t *resp)
{
	int ret, transferred;
	struct rtlbt_hci_event_cmd_compl *event;
	uint8_t cmd_buf[offsetof(struct rtlbt_hci_cmd, data) + sizeof(*vcmd)];
	struct rtlbt_hci_cmd *cmd = (struct rtlbt_hci_cmd *)cmd_buf;
	cmd->opcode = htole16(0xfc61);
	cmd->length = sizeof(struct rtlbt_vendor_cmd);
	memcpy(cmd->data, vcmd, sizeof(struct rtlbt_vendor_cmd));
	uint8_t buf[RTLBT_HCI_EVT_COMPL_SIZE(struct rtlbt_vendor_rp)];

	memset(buf, 0, sizeof(buf));

	ret = rtlbt_hci_command(hdl,
	    cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    RTLBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred != sizeof(buf)) {
		 rtlbt_debug("Can't read reg16: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	event = (struct rtlbt_hci_event_cmd_compl *)buf;
	memcpy(resp, &(((struct rtlbt_vendor_rp *)event->data)->data), 2);

	return (0);
}

int
rtlbt_load_fwfile(struct libusb_device_handle *hdl,
    const struct rtlbt_firmware *fw)
{
	uint8_t cmd_buf[RTLBT_HCI_MAX_CMD_SIZE];
	struct rtlbt_hci_cmd *cmd = (struct rtlbt_hci_cmd *)cmd_buf;
	struct rtlbt_hci_dl_cmd *dl_cmd = (struct rtlbt_hci_dl_cmd *)cmd->data;
	uint8_t evt_buf[RTLBT_HCI_EVT_COMPL_SIZE(struct rtlbt_hci_dl_rp)];
	uint8_t *data = fw->buf;
	int frag_num = fw->len / RTLBT_MAX_CMD_DATA_LEN + 1;
	int frag_len = RTLBT_MAX_CMD_DATA_LEN;
	int i, j;
	int ret, transferred;

	for (i = 0, j = 0; i < frag_num; i++, j++) {

		rtlbt_debug("download fw (%d/%d)", i + 1, frag_num);

		memset(cmd_buf, 0, sizeof(cmd_buf));
		cmd->opcode = htole16(0xfc20);
		if (j > 0x7f)
			j = 1;
		dl_cmd->index = j;

		if (i == (frag_num - 1)) {
			dl_cmd->index |= 0x80; /* data end */
			frag_len = fw->len % RTLBT_MAX_CMD_DATA_LEN;
		}
		cmd->length = frag_len + 1;
		memcpy(dl_cmd->data, data, frag_len);

		/* Send download command */
		ret = rtlbt_hci_command(hdl,
		    cmd,
		    evt_buf,
		    sizeof(evt_buf),
		    &transferred,
		    RTLBT_HCI_CMD_TIMEOUT);
		if (ret < 0) {
			rtlbt_err("download fw command failed (%d)", errno);
			goto out;
		}
		if (transferred != sizeof(evt_buf)) {
			rtlbt_err("download fw event length mismatch");
			errno = EIO;
			ret = -1;
			goto out;
		}

		data += RTLBT_MAX_CMD_DATA_LEN;
	}

out:
	return (ret);
}
