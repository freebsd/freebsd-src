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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libusb.h>

#include <netgraph/bluetooth/include/ng_hci.h>

#include "iwmbt_fw.h"
#include "iwmbt_hw.h"
#include "iwmbt_dbg.h"

#define	XMIN(x, y)	((x) < (y) ? (x) : (y))

static int
iwmbt_send_fragment(struct libusb_device_handle *hdl,
    uint8_t fragment_type, const void *data, uint8_t len, int timeout)
{
	int ret, transferred;
	uint8_t buf[IWMBT_HCI_MAX_CMD_SIZE];
	struct iwmbt_hci_cmd *cmd = (struct iwmbt_hci_cmd *) buf;

	memset(buf, 0, sizeof(buf));
	cmd->opcode = htole16(0xfc09),
	cmd->length = len + 1,
	cmd->data[0] = fragment_type;
	memcpy(cmd->data + 1, data, len);

	ret = libusb_bulk_transfer(hdl,
	    IWMBT_BULK_OUT_ENDPOINT_ADDR,
	    (uint8_t *)cmd,
	    IWMBT_HCI_CMD_SIZE(cmd),
	    &transferred,
	    timeout);

	if (ret < 0 || transferred != (int)IWMBT_HCI_CMD_SIZE(cmd)) {
		iwmbt_err("libusb_bulk_transfer() failed: err=%s, size=%zu",
		    libusb_strerror(ret),
		    IWMBT_HCI_CMD_SIZE(cmd));
		return (-1);
	}

	ret = libusb_bulk_transfer(hdl,
	    IWMBT_BULK_IN_ENDPOINT_ADDR,
	    buf,
	    sizeof(buf),
	    &transferred,
	    timeout);

	if (ret < 0) {
		iwmbt_err("libusb_bulk_transfer() failed: err=%s",
		    libusb_strerror(ret));
		return (-1);
	}

	return (0);
}

static int
iwmbt_hci_command(struct libusb_device_handle *hdl, struct iwmbt_hci_cmd *cmd,
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
	    IWMBT_HCI_CMD_SIZE(cmd),
	    timeout);

	if (ret < 0) {
		iwmbt_err("libusb_control_transfer() failed: err=%s",
		    libusb_strerror(ret));
		return (ret);
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	to = IWMBT_MSEC2TS(timeout);
	timespecadd(&to, &now, &to);

	do {
		timespecsub(&to, &now, &remains);
		ret = libusb_interrupt_transfer(hdl,
		    IWMBT_INTERRUPT_ENDPOINT_ADDR,
		    event,
		    size,
		    transferred,
		    IWMBT_TS2MSEC(remains) + 1);

		if (ret < 0) {
			iwmbt_err("libusb_interrupt_transfer() failed: err=%s",
			    libusb_strerror(ret));
			return (ret);
		}

		switch (((struct iwmbt_hci_event *)event)->header.event) {
		case NG_HCI_EVENT_COMMAND_COMPL:
			if (*transferred <
			    (int)offsetof(struct iwmbt_hci_event_cmd_compl, data))
				break;
			if (cmd->opcode !=
			    ((struct iwmbt_hci_event_cmd_compl *)event)->opcode)
				break;
			/* FALLTHROUGH */
		case 0xFF:
			return (0);
		default:
			break;
		}
		iwmbt_debug("Stray HCI event: %x",
		    ((struct iwmbt_hci_event *)event)->header.event);
	} while (timespeccmp(&to, &now, >));

	iwmbt_err("libusb_interrupt_transfer() failed: err=%s",
	    libusb_strerror(LIBUSB_ERROR_TIMEOUT));

	return (LIBUSB_ERROR_TIMEOUT);
}

int
iwmbt_patch_fwfile(struct libusb_device_handle *hdl,
    const struct iwmbt_firmware *fw)
{
	int ret, transferred;
	struct iwmbt_firmware fw_job = *fw;
	uint16_t cmd_opcode;
	uint8_t cmd_length;
	struct iwmbt_hci_cmd *cmd_buf;
	uint8_t evt_code;
	uint8_t evt_length;
	uint8_t evt_buf[IWMBT_HCI_MAX_EVENT_SIZE];
	int activate_patch = 0;

	while (fw_job.len > 0) {
		if (fw_job.len < 4) {
			iwmbt_err("Invalid firmware, unexpected EOF in HCI "
			    "command header. Remains=%d", fw_job.len);
			return (-1);
		}

		if (fw_job.buf[0] != 0x01) {
			iwmbt_err("Invalid firmware, expected HCI command (%d)",
					fw_job.buf[0]);
			return (-1);
		}

		/* Advance by one. */
		fw_job.buf++;
		fw_job.len--;

		/* Load in the HCI command to perform. */
		cmd_opcode = le16dec(fw_job.buf);
		cmd_length = fw_job.buf[2];
		cmd_buf = (struct iwmbt_hci_cmd *)fw_job.buf;

		iwmbt_debug("opcode=%04x, len=%02x", cmd_opcode, cmd_length);

		/*
		 * If there is a command that loads a patch in the
		 * firmware file, then activate the patch upon success,
		 * otherwise just disable the manufacturer mode.
		 */
		if (cmd_opcode == 0xfc8e)
			activate_patch = 1;

		/* Advance by three. */
		fw_job.buf += 3;
		fw_job.len -= 3;

		if (fw_job.len < cmd_length) {
			iwmbt_err("Invalid firmware, unexpected EOF in HCI "
			    "command data. len=%d, remains=%d",
			    cmd_length, fw_job.len);
			return (-1);
		}

		/* Advance by data length. */
		fw_job.buf += cmd_length;
		fw_job.len -= cmd_length;

		ret = libusb_control_transfer(hdl,
		    LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
		    0,
		    0,
		    0,
		    (uint8_t *)cmd_buf,
		    IWMBT_HCI_CMD_SIZE(cmd_buf),
		    IWMBT_HCI_CMD_TIMEOUT);

		if (ret < 0) {
			iwmbt_err("libusb_control_transfer() failed: err=%s",
			    libusb_strerror(ret));
			return (-1);
		}

		/*
		 * Every command has its associated event: data must match
		 * what is recorded in the firmware file. Perform that check
		 * now.
		 */

		while (fw_job.len > 0 && fw_job.buf[0] == 0x02) {
			/* Is this the end of the file? */
			if (fw_job.len < 3) {
				iwmbt_err("Invalid firmware, unexpected EOF in"
				    "event header. remains=%d", fw_job.len);
				return (-1);
			}

			/* Advance by one. */
			fw_job.buf++;
			fw_job.len--;

			/* Load in the HCI event. */
			evt_code = fw_job.buf[0];
			evt_length = fw_job.buf[1];

			/* Advance by two. */
			fw_job.buf += 2;
			fw_job.len -= 2;

			/* Prepare HCI event buffer. */
			memset(evt_buf, 0, IWMBT_HCI_MAX_EVENT_SIZE);

			iwmbt_debug("event=%04x, len=%02x",
					evt_code, evt_length);

			if (fw_job.len < evt_length) {
				iwmbt_err("Invalid firmware, unexpected EOF in"
				    " event data. len=%d, remains=%d",
				    evt_length, fw_job.len);
				return (-1);
			}

			ret = libusb_interrupt_transfer(hdl,
			    IWMBT_INTERRUPT_ENDPOINT_ADDR,
			    evt_buf,
			    IWMBT_HCI_MAX_EVENT_SIZE,
			    &transferred,
			    IWMBT_HCI_CMD_TIMEOUT);

			if (ret < 0) {
				iwmbt_err("libusb_interrupt_transfer() failed:"
				    " err=%s", libusb_strerror(ret));
				return (-1);
			}

			if ((int)evt_length + 2 != transferred ||
			    memcmp(evt_buf + 2, fw_job.buf, evt_length) != 0) {
				iwmbt_err("event does not match firmware");
				return (-1);
			}

			/* Advance by data length. */
			fw_job.buf += evt_length;
			fw_job.len -= evt_length;
		}
	}

	return (activate_patch);
}

#define	IWMBT_SEND_FRAGMENT(fragment_type, size, msg)	do {		\
	iwmbt_debug("transferring %d bytes, offset %d", size, sent);	\
									\
	ret = iwmbt_send_fragment(hdl,					\
	    fragment_type,						\
	    fw->buf + sent,						\
	    XMIN(size, fw->len - sent),					\
	    IWMBT_HCI_CMD_TIMEOUT);					\
									\
	if (ret < 0) {							\
		iwmbt_debug("Failed to send "msg": code=%d", ret);	\
		return (-1);						\
	}								\
	sent += size;							\
} while (0)

int
iwmbt_load_rsa_header(struct libusb_device_handle *hdl,
    const struct iwmbt_firmware *fw)
{
	int ret, sent = 0;

	IWMBT_SEND_FRAGMENT(0x00, 0x80, "CCS segment");
	IWMBT_SEND_FRAGMENT(0x03, 0x80, "public key / part 1");
	IWMBT_SEND_FRAGMENT(0x03, 0x80, "public key / part 2");

	/* skip 4 bytes */
	sent += 4;

	IWMBT_SEND_FRAGMENT(0x02, 0x80, "signature / part 1");
	IWMBT_SEND_FRAGMENT(0x02, 0x80, "signature / part 2");

	return (0);
}

int
iwmbt_load_ecdsa_header(struct libusb_device_handle *hdl,
    const struct iwmbt_firmware *fw)
{
	int ret, sent = ECDSA_OFFSET;

	IWMBT_SEND_FRAGMENT(0x00, 0x80, "CCS segment");
	IWMBT_SEND_FRAGMENT(0x03, 0x60, "public key");
	IWMBT_SEND_FRAGMENT(0x02, 0x60, "signature");

	return (0);
}

int
iwmbt_load_fwfile(struct libusb_device_handle *hdl,
    const struct iwmbt_firmware *fw, uint32_t *boot_param, int offset)
{
	int ready = 0, sent = offset;
	int ret, transferred;
	struct iwmbt_hci_cmd *cmd;
	struct iwmbt_hci_event *event;
	uint8_t buf[IWMBT_HCI_MAX_EVENT_SIZE];

	/*
	 * Send firmware chunks. Chunk len must be 4 byte aligned.
	 * multiple commands can be combined
	 */
	while (fw->len - sent - ready >= (int) sizeof(struct iwmbt_hci_cmd)) {
		cmd = (struct iwmbt_hci_cmd *)(fw->buf + sent + ready);
		/* Parse firmware for Intel Reset HCI command parameter */
		if (cmd->opcode == htole16(0xfc0e)) {
			*boot_param = le32dec(cmd->data);
			iwmbt_debug("boot_param=0x%08x", *boot_param);
		}
		ready += IWMBT_HCI_CMD_SIZE(cmd);
		while (ready >= 0xFC) {
			IWMBT_SEND_FRAGMENT(0x01, 0xFC, "firmware chunk");
			ready -= 0xFC;
		}
		if (ready > 0 && ready % 4 == 0) {
			IWMBT_SEND_FRAGMENT(0x01, ready, "firmware chunk");
			ready = 0;
		}
	}

	/* Wait for firmware download completion event */
	ret = libusb_interrupt_transfer(hdl,
	    IWMBT_INTERRUPT_ENDPOINT_ADDR,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_LOADCMPL_TIMEOUT);

	if (ret < 0 || transferred < (int)sizeof(struct iwmbt_hci_event) + 1) {
		iwmbt_err("libusb_interrupt_transfer() failed: "
		    "err=%s, size=%d",
		    libusb_strerror(ret),
		    transferred);
		return (-1);
	}

	/* Expect Vendor Specific Event 0x06 */
	event = (struct iwmbt_hci_event *)buf;
	if (event->header.event != 0xFF || event->data[0] != 0x06) {
		iwmbt_err("firmware download completion event missed");
		return (-1);
	}

	return (0);
}

int
iwmbt_enter_manufacturer(struct libusb_device_handle *hdl)
{
	int ret, transferred;
	static struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc11),
		.length = 2,
		.data = { 0x01, 0x00 },
	};
	uint8_t buf[IWMBT_HCI_MAX_EVENT_SIZE];

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0) {
		 iwmbt_debug("Can't enter manufacturer mode: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	return (0);
}

int
iwmbt_exit_manufacturer(struct libusb_device_handle *hdl,
    enum iwmbt_mm_exit mode)
{
	int ret, transferred;
	static struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc11),
		.length = 2,
		.data = { 0x00, 0x00 },
	};
	uint8_t buf[IWMBT_HCI_MAX_EVENT_SIZE];

	cmd.data[1] = (uint8_t)mode;

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0) {
		 iwmbt_debug("Can't exit manufacturer mode: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	return (0);
}

int
iwmbt_get_version(struct libusb_device_handle *hdl,
    struct iwmbt_version *version)
{
	int ret, transferred;
	struct iwmbt_hci_event_cmd_compl*event;
	struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc05),
		.length = 0,
	};
	uint8_t buf[IWMBT_HCI_EVT_COMPL_SIZE(struct iwmbt_version)];

	memset(buf, 0, sizeof(buf));

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred != sizeof(buf)) {
		 iwmbt_debug("Can't get version: : code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	event = (struct iwmbt_hci_event_cmd_compl *)buf;
	memcpy(version, event->data, sizeof(struct iwmbt_version));

	return (0);
}

int
iwmbt_get_version_tlv(struct libusb_device_handle *hdl,
    struct iwmbt_version_tlv *version)
{
	int ret, transferred;
	struct iwmbt_hci_event_cmd_compl *event;
	static struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc05),
		.length = 1,
		.data = { 0xff },
	};
	uint8_t status, datalen, type, len;
	uint8_t *data;
	uint8_t buf[255];

	memset(buf, 0, sizeof(buf));

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred < (int)IWMBT_HCI_EVT_COMPL_SIZE(uint16_t)) {
		 iwmbt_debug("Can't get version: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	event = (struct iwmbt_hci_event_cmd_compl *)buf;
	memcpy(version, event->data, sizeof(struct iwmbt_version));

	datalen = event->header.length - IWMBT_HCI_EVENT_COMPL_HEAD_SIZE;
	data = event->data;
	status = *data++;
	if (status != 0)
		return (-1);
	datalen--;

	while (datalen >= 2) {
		type = *data++;
		len = *data++;
		datalen -= 2;

		if (datalen < len)
			return (-1);

		switch (type) {
		case IWMBT_TLV_CNVI_TOP:
			assert(len == 4);
			version->cnvi_top = le32dec(data);
			break;
		case IWMBT_TLV_CNVR_TOP:
			assert(len == 4);
			version->cnvr_top = le32dec(data);
			break;
		case IWMBT_TLV_CNVI_BT:
			assert(len == 4);
			version->cnvi_bt = le32dec(data);
			break;
		case IWMBT_TLV_CNVR_BT:
			assert(len == 4);
			version->cnvr_bt = le32dec(data);
			break;
		case IWMBT_TLV_DEV_REV_ID:
			assert(len == 2);
			version->dev_rev_id = le16dec(data);
			break;
		case IWMBT_TLV_IMAGE_TYPE:
			assert(len == 1);
			version->img_type = *data;
			break;
		case IWMBT_TLV_TIME_STAMP:
			assert(len == 2);
			version->min_fw_build_cw = data[0];
			version->min_fw_build_yy = data[1];
			version->timestamp = le16dec(data);
			break;
		case IWMBT_TLV_BUILD_TYPE:
			assert(len == 1);
			version->build_type = *data;
			break;
		case IWMBT_TLV_BUILD_NUM:
			assert(len == 4);
			version->min_fw_build_nn = *data;
			version->build_num = le32dec(data);
			break;
		case IWMBT_TLV_SECURE_BOOT:
			assert(len == 1);
			version->secure_boot = *data;
			break;
		case IWMBT_TLV_OTP_LOCK:
			assert(len == 1);
			version->otp_lock = *data;
			break;
		case IWMBT_TLV_API_LOCK:
			assert(len == 1);
			version->api_lock = *data;
			break;
		case IWMBT_TLV_DEBUG_LOCK:
			assert(len == 1);
			version->debug_lock = *data;
			break;
		case IWMBT_TLV_MIN_FW:
			assert(len == 3);
			version->min_fw_build_nn = data[0];
			version->min_fw_build_cw = data[1];
			version->min_fw_build_yy = data[2];
			break;
		case IWMBT_TLV_LIMITED_CCE:
			assert(len == 1);
			version->limited_cce = *data;
			break;
		case IWMBT_TLV_SBE_TYPE:
			assert(len == 1);
			version->sbe_type = *data;
			break;
		case IWMBT_TLV_OTP_BDADDR:
			memcpy(&version->otp_bd_addr, data, sizeof(bdaddr_t));
			break;
		default:
			/* Ignore other types */
			break;
		}

		datalen -= len;
		data += len;
	}

	return (0);
}

int
iwmbt_get_boot_params(struct libusb_device_handle *hdl,
    struct iwmbt_boot_params *params)
{
	int ret, transferred = 0;
	struct iwmbt_hci_event_cmd_compl *event;
	struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc0d),
		.length = 0,
	};
	uint8_t buf[IWMBT_HCI_EVT_COMPL_SIZE(struct iwmbt_boot_params)];

	memset(buf, 0, sizeof(buf));

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred != sizeof(buf)) {
		 iwmbt_debug("Can't get boot params: code=%d, size=%d",
		     ret,
		     transferred);
		 return (-1);
	}

	event = (struct iwmbt_hci_event_cmd_compl *)buf;
	memcpy(params, event->data, sizeof(struct iwmbt_boot_params));

	return (0);
}

int
iwmbt_intel_reset(struct libusb_device_handle *hdl, uint32_t boot_param)
{
	int ret, transferred = 0;
	struct iwmbt_hci_event *event;
	static struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc01),
		.length = 8,
		.data = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 },
	};
	uint8_t buf[IWMBT_HCI_MAX_EVENT_SIZE];

	le32enc(cmd.data + 4, boot_param);
	memset(buf, 0, sizeof(buf));

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0 || transferred < (int)sizeof(struct iwmbt_hci_event) + 1) {
		 iwmbt_debug("Intel Reset command failed: code=%d, size=%d",
		    ret,
		    transferred);
		 return (ret);
	}

	/* expect Vendor Specific Event 0x02 */
	event = (struct iwmbt_hci_event *)buf;
	if (event->header.event != 0xFF || event->data[0] != 0x02) {
		iwmbt_err("Intel Reset completion event missed");
		return (-1);
	}

	return (0);
}

int
iwmbt_load_ddc(struct libusb_device_handle *hdl,
    const struct iwmbt_firmware *ddc)
{
	int size, sent = 0;
	int ret, transferred;
	uint8_t buf[IWMBT_HCI_MAX_CMD_SIZE];
	uint8_t evt[IWMBT_HCI_MAX_CMD_SIZE];
	struct iwmbt_hci_cmd *cmd = (struct iwmbt_hci_cmd *)buf;

	size = ddc->len;

	iwmbt_debug("file=%s, size=%d", ddc->fwname, size);

	while (size > 0) {

		memset(buf, 0, sizeof(buf));
		cmd->opcode = htole16(0xfc8b);
		cmd->length = ddc->buf[sent] + 1;
		memcpy(cmd->data, ddc->buf + sent, XMIN(ddc->buf[sent], size));

		iwmbt_debug("transferring %d bytes, offset %d",
		    cmd->length,
		    sent);

		size -= cmd->length;
		sent += cmd->length;

		ret = iwmbt_hci_command(hdl,
		    cmd,
		    evt,
		    sizeof(evt),
		    &transferred,
		    IWMBT_HCI_CMD_TIMEOUT);

		if (ret < 0) {
			 iwmbt_debug("Intel Write DDC failed: code=%d", ret);
			 return (-1);
		}
	}

	return (0);
}

int
iwmbt_set_event_mask(struct libusb_device_handle *hdl)
{
	int ret, transferred = 0;
	static struct iwmbt_hci_cmd cmd = {
		.opcode = htole16(0xfc52),
		.length = 8,
		.data = { 0x87, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};
	uint8_t buf[IWMBT_HCI_MAX_EVENT_SIZE];

	ret = iwmbt_hci_command(hdl,
	    &cmd,
	    buf,
	    sizeof(buf),
	    &transferred,
	    IWMBT_HCI_CMD_TIMEOUT);

	if (ret < 0)
		 iwmbt_debug("Intel Set Event Mask failed: code=%d", ret);

	return (ret);
}
