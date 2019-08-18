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

#ifndef	__IWMBT_FW_H__
#define	__IWMBT_FW_H__

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

#endif
