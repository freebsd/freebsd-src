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
#ifndef	__IWMBT_HW_H__
#define	__IWMBT_HW_H__

/* USB control request (HCI command) structure */
struct iwmbt_hci_cmd {
	uint16_t	opcode;
	uint8_t		length;
	uint8_t		data[];
} __attribute__ ((packed));

#define IWMBT_HCI_CMD_SIZE(cmd) \
	((cmd)->length + offsetof(struct iwmbt_hci_cmd, data))

/* USB interrupt transfer HCI event header structure */
struct iwmbt_hci_evhdr {
	uint8_t		event;
	uint8_t		length;
} __attribute__ ((packed));

/* USB interrupt transfer (generic HCI event) structure */
struct iwmbt_hci_event {
	struct iwmbt_hci_evhdr	header;
	uint8_t			data[];
} __attribute__ ((packed));

/* USB interrupt transfer (HCI command completion event) structure */
struct iwmbt_hci_event_cmd_compl {
	struct iwmbt_hci_evhdr	header;
	uint8_t			numpkt;
	uint16_t		opcode;
	uint8_t			data[];
} __attribute__ ((packed));

/*
 * Manufacturer mode exit type: selects reset type,
 * 0x00: simply exit manufacturer mode without a reset.
 * 0x01: exit manufacturer mode with a reset and patches disabled
 * 0x02: exit manufacturer mode with a reset and patches enabled
 */
enum iwmbt_mm_exit {
	IWMBT_MM_EXIT_ONLY = 0x00,
	IWMBT_MM_EXIT_COLD_RESET = 0x01,
	IWMBT_MM_EXIT_WARM_RESET = 0x02,
};

#define IWMBT_HCI_EVT_COMPL_SIZE(payload) \
	(offsetof(struct iwmbt_hci_event_cmd_compl, data) + sizeof(payload))
#define	IWMBT_HCI_EVENT_COMPL_HEAD_SIZE \
	(offsetof(struct iwmbt_hci_event_cmd_compl, data) - \
	 offsetof(struct iwmbt_hci_event_cmd_compl, numpkt))

#define	IWMBT_CONTROL_ENDPOINT_ADDR	0x00
#define	IWMBT_INTERRUPT_ENDPOINT_ADDR	0x81
#define	IWMBT_BULK_IN_ENDPOINT_ADDR	0x82
#define	IWMBT_BULK_OUT_ENDPOINT_ADDR	0x02

#define	IWMBT_HCI_MAX_CMD_SIZE		256
#define	IWMBT_HCI_MAX_EVENT_SIZE	16

#define	IWMBT_MSEC2TS(msec)				\
	(struct timespec) {				\
	    .tv_sec = (msec) / 1000,			\
	    .tv_nsec = ((msec) % 1000) * 1000000	\
	};
#define	IWMBT_TS2MSEC(ts)	((ts).tv_sec * 1000 + (ts).tv_nsec / 1000000)
#define	IWMBT_HCI_CMD_TIMEOUT		2000	/* ms */
#define	IWMBT_LOADCMPL_TIMEOUT		5000	/* ms */

extern	int iwmbt_patch_fwfile(struct libusb_device_handle *hdl,
	    const struct iwmbt_firmware *fw);
extern	int iwmbt_load_rsa_header(struct libusb_device_handle *hdl,
	    const struct iwmbt_firmware *fw);
extern	int iwmbt_load_ecdsa_header(struct libusb_device_handle *hdl,
	    const struct iwmbt_firmware *fw);
extern	int iwmbt_load_fwfile(struct libusb_device_handle *hdl,
	    const struct iwmbt_firmware *fw, uint32_t *boot_param, int offset);
extern	int iwmbt_enter_manufacturer(struct libusb_device_handle *hdl);
extern	int iwmbt_exit_manufacturer(struct libusb_device_handle *hdl,
	    enum iwmbt_mm_exit mode);
extern	int iwmbt_get_version(struct libusb_device_handle *hdl,
	    struct iwmbt_version *version);
extern	int iwmbt_get_version_tlv(struct libusb_device_handle *hdl,
	    struct iwmbt_version_tlv *version);
extern	int iwmbt_get_boot_params(struct libusb_device_handle *hdl,
	    struct iwmbt_boot_params *params);
extern	int iwmbt_intel_reset(struct libusb_device_handle *hdl,
	    uint32_t boot_param);
extern	int iwmbt_load_ddc(struct libusb_device_handle *hdl,
	    const struct iwmbt_firmware *ddc);
extern	int iwmbt_set_event_mask(struct libusb_device_handle *hdl);

#endif
