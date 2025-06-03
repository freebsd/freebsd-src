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
#ifndef	__RTLBT_HW_H__
#define	__RTLBT_HW_H__

#include <netgraph/bluetooth/include/ng_hci.h>

/* USB control request (HCI command) structure */
struct rtlbt_hci_cmd {
	uint16_t	opcode;
	uint8_t		length;
	uint8_t		data[];
} __attribute__ ((packed));

#define RTLBT_HCI_CMD_SIZE(cmd) \
	((cmd)->length + offsetof(struct rtlbt_hci_cmd, data))

/* USB interrupt transfer HCI event header structure */
struct rtlbt_hci_evhdr {
	uint8_t		event;
	uint8_t		length;
} __attribute__ ((packed));

/* USB interrupt transfer (generic HCI event) structure */
struct rtlbt_hci_event {
	struct rtlbt_hci_evhdr	header;
	uint8_t			data[];
} __attribute__ ((packed));

/* USB interrupt transfer (HCI command completion event) structure */
struct rtlbt_hci_event_cmd_compl {
	struct rtlbt_hci_evhdr	header;
	uint8_t			numpkt;
	uint16_t		opcode;
	uint8_t			data[];
} __attribute__ ((packed));

#define RTLBT_HCI_EVT_COMPL_SIZE(payload) \
	(offsetof(struct rtlbt_hci_event_cmd_compl, data) + sizeof(payload))

#define	RTLBT_CONTROL_ENDPOINT_ADDR	0x00
#define	RTLBT_INTERRUPT_ENDPOINT_ADDR	0x81

#define	RTLBT_HCI_MAX_CMD_SIZE		256
#define	RTLBT_HCI_MAX_EVENT_SIZE	16

#define	RTLBT_MSEC2TS(msec)				\
	(struct timespec) {				\
	    .tv_sec = (msec) / 1000,			\
	    .tv_nsec = ((msec) % 1000) * 1000000	\
	};
#define	RTLBT_TS2MSEC(ts)	((ts).tv_sec * 1000 + (ts).tv_nsec / 1000000)
#define	RTLBT_HCI_CMD_TIMEOUT		2000	/* ms */
#define	RTLBT_LOADCMPL_TIMEOUT		5000	/* ms */

#define RTLBT_MAX_CMD_DATA_LEN		252

struct rtlbt_rom_ver_rp {
	uint8_t status;
	uint8_t version;
} __attribute__ ((packed));

struct rtlbt_hci_dl_cmd {
        uint8_t index;
        uint8_t data[RTLBT_MAX_CMD_DATA_LEN];
} __attribute__ ((packed));

struct rtlbt_hci_dl_rp {
        uint8_t status;
        uint8_t index;
} __attribute__ ((packed));

/* Vendor USB request payload */
struct rtlbt_vendor_cmd {
	uint8_t data[5];
}  __attribute__ ((packed));
#define	RTLBT_SEC_PROJ	(&(struct rtlbt_vendor_cmd) {{0x10, 0xA4, 0x0D, 0x00, 0xb0}})

struct rtlbt_vendor_rp {
	uint8_t status;
	uint8_t data[2];
};

int	rtlbt_read_local_ver(struct libusb_device_handle *hdl,
	    ng_hci_read_local_ver_rp *ver);
int	rtlbt_read_rom_ver(struct libusb_device_handle *hdl, uint8_t *ver);
int	rtlbt_read_reg16(struct libusb_device_handle *hdl,
	    struct rtlbt_vendor_cmd *cmd, uint8_t *resp);
int	rtlbt_load_fwfile(struct libusb_device_handle *hdl,
	    const struct rtlbt_firmware *fw);

#endif
