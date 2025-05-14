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

#ifndef	__RTLBT_FW_H__
#define	__RTLBT_FW_H__

#include <sys/queue.h>
#include <sys/queue_mergesort.h>

#define	RTLBT_ROM_LMP_8703B	0x8703
#define	RTLBT_ROM_LMP_8723A	0x1200
#define	RTLBT_ROM_LMP_8723B	0x8723
#define	RTLBT_ROM_LMP_8821A	0x8821
#define	RTLBT_ROM_LMP_8761A	0x8761
#define	RTLBT_ROM_LMP_8822B	0x8822
#define	RTLBT_ROM_LMP_8852A	0x8852
#define	RTLBT_ROM_LMP_8851B	0x8851
#define	RTLBT_ROM_LMP_8922A	0x8922

#define RTLBT_PATCH_SNIPPETS		0x01
#define RTLBT_PATCH_DUMMY_HEADER	0x02
#define RTLBT_PATCH_SECURITY_HEADER	0x03

enum rtlbt_fw_type {
	RTLBT_FW_TYPE_UNKNOWN,
	RTLBT_FW_TYPE_V1,
	RTLBT_FW_TYPE_V2,
};

struct rtlbt_id_table {
	uint16_t lmp_subversion;
	uint16_t hci_revision;
	uint8_t hci_version;
	uint8_t flags;
#define	RTLBT_IC_FLAG_SIMPLE	(0 << 1)
#define	RTLBT_IC_FLAG_CONFIG	(1 << 1)
#define	RTLBT_IC_FLAG_MSFT	(2 << 1)
	const char *fw_name;
	const char *fw_suffix;
};

struct rtlbt_firmware {
	char *fwname;
	size_t len;
	unsigned char *buf;
};

SLIST_HEAD(rtlbt_patch_list, rtlbt_patch_entry);

struct rtlbt_patch_entry {
	SLIST_ENTRY(rtlbt_patch_entry) next;
	uint32_t opcode;
	uint32_t len;
	uint8_t prio;
	uint8_t *data;
};

struct rtlbt_iov {
	uint8_t *data;
	uint32_t len;
};

struct rtlbt_fw_header_v1 {
	uint8_t signature[8];
	uint32_t fw_version;
	uint16_t num_patches;
} __attribute__ ((packed));

struct rtlbt_fw_header_v2 {
	uint8_t signature[8];
	uint8_t fw_version[8];
	uint32_t num_sections;
} __attribute__ ((packed));

struct rtlbt_section {
	uint32_t opcode;
	uint32_t len;
	uint8_t data[];
} __attribute__ ((packed));

struct rtlbt_sec_hdr {
	uint16_t num;
	uint16_t reserved;
} __attribute__ ((packed));

struct rtlbt_subsec_hdr {
	uint8_t eco;
	uint8_t prio;
	uint8_t cb[2];
	uint32_t len;
} __attribute__ ((packed));

struct rtlbt_subsec_security_hdr {
	uint8_t eco;
	uint8_t prio;
	uint8_t key_id;
	uint8_t reserved;
	uint32_t len;
} __attribute__ ((packed));

int rtlbt_fw_read(struct rtlbt_firmware *fw, const char *fwname);
void rtlbt_fw_free(struct rtlbt_firmware *fw);
char *rtlbt_get_fwname(const char *fw_name, const char *prefix,
    const char *suffix);
const struct rtlbt_id_table *rtlbt_get_ic(uint16_t lmp_subversion,
    uint16_t hci_revision, uint8_t hci_version);
enum rtlbt_fw_type rtlbt_get_fw_type(struct rtlbt_firmware *fw,
    uint16_t *fw_lmp_subversion);
int rtlbt_parse_fwfile_v1(struct rtlbt_firmware *fw, uint8_t rom_version);
int rtlbt_parse_fwfile_v2(struct rtlbt_firmware *fw, uint8_t rom_version,
    uint8_t reg_id);
int rtlbt_append_fwfile(struct rtlbt_firmware *fw, struct rtlbt_firmware *opt);

#endif
