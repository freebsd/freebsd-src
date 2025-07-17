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

#ifndef	__IWMBT_FW_H__
#define	__IWMBT_FW_H__

#include <sys/types.h>
#define	L2CAP_SOCKET_CHECKED
#include <bluetooth.h>

#define	RSA_HEADER_LEN		644
#define	ECDSA_HEADER_LEN	320
#define	ECDSA_OFFSET		RSA_HEADER_LEN
#define	CSS_HEADER_OFFSET	8

struct iwmbt_version {
	uint8_t status;
	uint8_t hw_platform;
	uint8_t hw_variant;
	uint8_t hw_revision;
	uint8_t fw_variant;
	uint8_t fw_revision;
	uint8_t fw_build_num;
	uint8_t fw_build_ww;
	uint8_t fw_build_yy;
	uint8_t fw_patch_num;
} __attribute__ ((packed));

/* Known values for fw_variant */
#define	FW_VARIANT_BOOTLOADER	0x06 /* Bootloader mode */
#define	FW_VARIANT_OPERATIONAL	0x23 /* Operational mode */

struct iwmbt_boot_params {
	uint8_t status;
	uint8_t otp_format;
	uint8_t otp_content;
	uint8_t otp_patch;
	uint16_t dev_revid;
	uint8_t secure_boot;
	uint8_t key_from_hdr;
	uint8_t key_type;
	uint8_t otp_lock;
	uint8_t api_lock;
	uint8_t debug_lock;
	uint8_t otp_bdaddr[6];
	uint8_t min_fw_build_nn;
	uint8_t min_fw_build_cw;
	uint8_t min_fw_build_yy;
	uint8_t limited_cce;
	uint8_t unlocked_state;
} __attribute__ ((packed));

enum {
	IWMBT_TLV_CNVI_TOP = 0x10,
	IWMBT_TLV_CNVR_TOP,
	IWMBT_TLV_CNVI_BT,
	IWMBT_TLV_CNVR_BT,
	IWMBT_TLV_CNVI_OTP,
	IWMBT_TLV_CNVR_OTP,
	IWMBT_TLV_DEV_REV_ID,
	IWMBT_TLV_USB_VENDOR_ID,
	IWMBT_TLV_USB_PRODUCT_ID,
	IWMBT_TLV_PCIE_VENDOR_ID,
	IWMBT_TLV_PCIE_DEVICE_ID,
	IWMBT_TLV_PCIE_SUBSYSTEM_ID,
	IWMBT_TLV_IMAGE_TYPE,
	IWMBT_TLV_TIME_STAMP,
	IWMBT_TLV_BUILD_TYPE,
	IWMBT_TLV_BUILD_NUM,
	IWMBT_TLV_FW_BUILD_PRODUCT,
	IWMBT_TLV_FW_BUILD_HW,
	IWMBT_TLV_FW_STEP,
	IWMBT_TLV_BT_SPEC,
	IWMBT_TLV_MFG_NAME,
	IWMBT_TLV_HCI_REV,
	IWMBT_TLV_LMP_SUBVER,
	IWMBT_TLV_OTP_PATCH_VER,
	IWMBT_TLV_SECURE_BOOT,
	IWMBT_TLV_KEY_FROM_HDR,
	IWMBT_TLV_OTP_LOCK,
	IWMBT_TLV_API_LOCK,
	IWMBT_TLV_DEBUG_LOCK,
	IWMBT_TLV_MIN_FW,
	IWMBT_TLV_LIMITED_CCE,
	IWMBT_TLV_SBE_TYPE,
	IWMBT_TLV_OTP_BDADDR,
	IWMBT_TLV_UNLOCKED_STATE
};

struct iwmbt_version_tlv {
	uint32_t cnvi_top;
	uint32_t cnvr_top;
	uint32_t cnvi_bt;
	uint32_t cnvr_bt;
	uint16_t dev_rev_id;
	uint8_t img_type;
	uint16_t timestamp;
	uint8_t build_type;
	uint32_t build_num;
	uint8_t secure_boot;
	uint8_t otp_lock;
	uint8_t api_lock;
	uint8_t debug_lock;
	uint8_t min_fw_build_nn;
	uint8_t min_fw_build_cw;
	uint8_t min_fw_build_yy;
	uint8_t limited_cce;
	uint8_t sbe_type;
	bdaddr_t otp_bd_addr;
};

/* Known TLV img_type values */
#define	TLV_IMG_TYPE_BOOTLOADER		0x01 /* Bootloader mode */
#define	TLV_IMG_TYPE_OPERATIONAL	0x03 /* Operational mode */

struct iwmbt_firmware {
	char *fwname;
	int len;
	unsigned char *buf;
};

extern	int iwmbt_fw_read(struct iwmbt_firmware *fw, const char *fwname);
extern	void iwmbt_fw_free(struct iwmbt_firmware *fw);
extern	char *iwmbt_get_fwname(struct iwmbt_version *ver,
	struct iwmbt_boot_params *params, const char *prefix,
	const char *suffix);
extern	char *iwmbt_get_fwname_tlv(struct iwmbt_version_tlv *ver,
	const char *prefix, const char *suffix);

#endif
