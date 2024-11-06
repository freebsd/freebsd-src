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
#include <sys/endian.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtlbt_fw.h"
#include "rtlbt_dbg.h"

static const struct rtlbt_id_table rtlbt_ic_id_table[] = {
	{ /* 8723A */
	    .lmp_subversion = RTLBT_ROM_LMP_8723A,
	    .hci_revision = 0xb,
	    .hci_version = 0x6,
	    .flags = RTLBT_IC_FLAG_SIMPLE,
	    .fw_name = "rtl8723a",
	}, { /* 8723B */
	    .lmp_subversion = RTLBT_ROM_LMP_8723B,
	    .hci_revision = 0xb,
	    .hci_version = 0x6,
	    .fw_name = "rtl8723b",
	}, { /* 8723D */
	    .lmp_subversion = RTLBT_ROM_LMP_8723B,
	    .hci_revision = 0xd,
	    .hci_version = 0x8,
	    .flags = RTLBT_IC_FLAG_CONFIG,
	    .fw_name = "rtl8723d",
	}, { /* 8821A */
	    .lmp_subversion = RTLBT_ROM_LMP_8821A,
	    .hci_revision = 0xa,
	    .hci_version = 0x6,
	    .fw_name = "rtl8821a",
	}, { /* 8821C */
	    .lmp_subversion = RTLBT_ROM_LMP_8821A,
	    .hci_revision = 0xc,
	    .hci_version = 0x8,
	    .flags = RTLBT_IC_FLAG_MSFT,
	    .fw_name = "rtl8821c",
	}, { /* 8761A */
	    .lmp_subversion = RTLBT_ROM_LMP_8761A,
	    .hci_revision = 0xa,
	    .hci_version = 0x6,
	    .fw_name = "rtl8761a",
	}, { /* 8761BU */
	    .lmp_subversion = RTLBT_ROM_LMP_8761A,
	    .hci_revision = 0xb,
	    .hci_version = 0xa,
	    .fw_name = "rtl8761bu",
	}, { /* 8822C with USB interface */
	    .lmp_subversion = RTLBT_ROM_LMP_8822B,
	    .hci_revision = 0xc,
	    .hci_version = 0xa,
	    .flags = RTLBT_IC_FLAG_MSFT,
	    .fw_name = "rtl8822cu",
	}, { /* 8822B */
	    .lmp_subversion = RTLBT_ROM_LMP_8822B,
	    .hci_revision = 0xb,
	    .hci_version = 0x7,
	    .flags = RTLBT_IC_FLAG_CONFIG | RTLBT_IC_FLAG_MSFT,
	    .fw_name = "rtl8822b",
	}, { /* 8852A */
	    .lmp_subversion = RTLBT_ROM_LMP_8852A,
	    .hci_revision = 0xa,
	    .hci_version = 0xb,
	    .flags = RTLBT_IC_FLAG_MSFT,
	    .fw_name = "rtl8852au",
	}, { /* 8852B */
	    .lmp_subversion = RTLBT_ROM_LMP_8852A,
	    .hci_revision = 0xb,
	    .hci_version = 0xb,
	    .flags = RTLBT_IC_FLAG_MSFT,
	    .fw_name = "rtl8852bu",
#ifdef RTLBTFW_SUPPORTS_FW_V2
	}, { /* 8852C */
	    .lmp_subversion = RTLBT_ROM_LMP_8852A,
	    .hci_revision = 0xc,
	    .hci_version = 0xc,
	    .flags = RTLBT_IC_FLAG_MSFT,
	    .fw_name  = "rtl8852cu",
	}, { /* 8851B */
	    .lmp_subversion = RTLBT_ROM_LMP_8851B,
	    .hci_revision = 0xb,
	    .hci_version = 0xc,
	    .flags = RTLBT_IC_FLAG_MSFT,
	    .fw_name  = "rtl8851bu",
#endif
	},
};

static const uint16_t project_ids[] = {
	[  0 ] = RTLBT_ROM_LMP_8723A,
	[  1 ] = RTLBT_ROM_LMP_8723B,
	[  2 ] = RTLBT_ROM_LMP_8821A,
	[  3 ] = RTLBT_ROM_LMP_8761A,
	[  7 ] = RTLBT_ROM_LMP_8703B,
	[  8 ] = RTLBT_ROM_LMP_8822B,
	[  9 ] = RTLBT_ROM_LMP_8723B,	/* 8723DU */
	[ 10 ] = RTLBT_ROM_LMP_8821A,	/* 8821CU */
	[ 13 ] = RTLBT_ROM_LMP_8822B,	/* 8822CU */
	[ 14 ] = RTLBT_ROM_LMP_8761A,	/* 8761BU */
	[ 18 ] = RTLBT_ROM_LMP_8852A,	/* 8852AU */
	[ 19 ] = RTLBT_ROM_LMP_8723B,	/* 8723FU */
	[ 20 ] = RTLBT_ROM_LMP_8852A,	/* 8852BU */
	[ 25 ] = RTLBT_ROM_LMP_8852A,	/* 8852CU */
	[ 33 ] = RTLBT_ROM_LMP_8822B,	/* 8822EU */
	[ 36 ] = RTLBT_ROM_LMP_8851B,	/* 8851BU */
};

/* Signatures */
static const uint8_t fw_header_sig_v1[8] =
    {0x52, 0x65, 0x61, 0x6C, 0x74, 0x65, 0x63, 0x68};	/* Realtech */
#ifdef RTLBTFW_SUPPORTS_FW_V2
static const uint8_t fw_header_sig_v2[8] =
    {0x52, 0x54, 0x42, 0x54, 0x43, 0x6F, 0x72, 0x65};	/* RTBTCore */
#endif
static const uint8_t fw_ext_sig[4] = {0x51, 0x04, 0xFD, 0x77};

int
rtlbt_fw_read(struct rtlbt_firmware *fw, const char *fwname)
{
	int fd;
	struct stat sb;
	unsigned char *buf;
	ssize_t r;

	fd = open(fwname, O_RDONLY);
	if (fd < 0) {
		warn("%s: open: %s", __func__, fwname);
		return (0);
	}

	if (fstat(fd, &sb) != 0) {
		warn("%s: stat: %s", __func__, fwname);
		close(fd);
		return (0);
	}

	buf = calloc(1, sb.st_size);
	if (buf == NULL) {
		warn("%s: calloc", __func__);
		close(fd);
		return (0);
	}

	/* XXX handle partial reads */
	r = read(fd, buf, sb.st_size);
	if (r < 0) {
		warn("%s: read", __func__);
		free(buf);
		close(fd);
		return (0);
	}

	if (r != sb.st_size) {
		rtlbt_err("read len %d != file size %d",
		    (int) r,
		    (int) sb.st_size);
		free(buf);
		close(fd);
		return (0);
	}

	/* We have everything, so! */

	memset(fw, 0, sizeof(*fw));

	fw->fwname = strdup(fwname);
	fw->len = sb.st_size;
	fw->buf = buf;

	close(fd);
	return (1);
}

void
rtlbt_fw_free(struct rtlbt_firmware *fw)
{
	if (fw->fwname)
		free(fw->fwname);
	if (fw->buf)
		free(fw->buf);
	memset(fw, 0, sizeof(*fw));
}

char *
rtlbt_get_fwname(const char *fw_name, const char *prefix, const char *suffix)
{
	char *fwname;

	asprintf(&fwname, "%s/%s%s", prefix, fw_name, suffix);

	return (fwname);
}

const struct rtlbt_id_table *
rtlbt_get_ic(uint16_t lmp_subversion, uint16_t hci_revision,
    uint8_t hci_version)
{
	unsigned int i;

	for (i = 0; i < nitems(rtlbt_ic_id_table); i++) {
		if (rtlbt_ic_id_table[i].lmp_subversion == lmp_subversion &&
		    rtlbt_ic_id_table[i].hci_revision == hci_revision &&
		    rtlbt_ic_id_table[i].hci_version == hci_version)
			return (rtlbt_ic_id_table + i);
	}

	return (NULL);
}

enum rtlbt_fw_type
rtlbt_get_fw_type(struct rtlbt_firmware *fw, uint16_t *fw_lmp_subversion)
{
	enum rtlbt_fw_type fw_type;
	size_t fw_header_len;
	uint8_t *ptr;
	uint8_t opcode, oplen, project_id;

	if (fw->len < 8) {
		rtlbt_err("firmware file too small");
		return (RTLBT_FW_TYPE_UNKNOWN);
	}

	if (memcmp(fw->buf, fw_header_sig_v1, sizeof(fw_header_sig_v1)) == 0) {
		fw_type = RTLBT_FW_TYPE_V1;
		fw_header_len = sizeof(struct rtlbt_fw_header_v1);
	} else
#ifdef RTLBTFW_SUPPORTS_FW_V2
	if (memcmp(fw->buf, fw_header_sig_v2, sizeof(fw_header_sig_v2)) == 0) {
		fw_type = RTLBT_FW_TYPE_V2;
		fw_header_len = sizeof(struct rtlbt_fw_header_v2);
	} else
#endif
		return (RTLBT_FW_TYPE_UNKNOWN);

	if (fw->len < fw_header_len + sizeof(fw_ext_sig) + 4) {
		rtlbt_err("firmware file too small");
		return (RTLBT_FW_TYPE_UNKNOWN);
	}

	ptr = fw->buf + fw->len - sizeof(fw_ext_sig);
	if (memcmp(ptr, fw_ext_sig, sizeof(fw_ext_sig)) != 0) {
		rtlbt_err("invalid extension section signature");
		return (RTLBT_FW_TYPE_UNKNOWN);
	}

	do {
		opcode = *--ptr;
		oplen = *--ptr;
		ptr -= oplen;

		rtlbt_debug("code=%x len=%x", opcode, oplen);

		if (opcode == 0x00) {
			if (oplen != 1) {
				rtlbt_err("invalid instruction length");
				return (RTLBT_FW_TYPE_UNKNOWN);
			}
			project_id = *ptr;
			rtlbt_debug("project_id=%x", project_id);
			if (project_id >= nitems(project_ids) ||
			    project_ids[project_id] == 0) {
				rtlbt_err("unknown project id %x", project_id);
				return (RTLBT_FW_TYPE_UNKNOWN);
			}
			*fw_lmp_subversion = project_ids[project_id];
			return (fw_type);
		}
	} while (opcode != 0xff && ptr > fw->buf + fw_header_len);

	rtlbt_err("can not find project id instruction");
	return (RTLBT_FW_TYPE_UNKNOWN);
};

int
rtlbt_parse_fwfile_v1(struct rtlbt_firmware *fw, uint8_t rom_version)
{
	struct rtlbt_fw_header_v1 *fw_header;
	uint8_t *patch_buf;
	unsigned int i;
	const uint8_t *chip_id_base;
	uint32_t patch_offset;
	uint16_t patch_length, num_patches;

	fw_header = (struct rtlbt_fw_header_v1 *)fw->buf;
	num_patches = le16toh(fw_header->num_patches);
	rtlbt_debug("fw_version=%x, num_patches=%d",
	       le32toh(fw_header->fw_version), num_patches);

	/* Find a right patch for the chip. */
	if (fw->len < sizeof(struct rtlbt_fw_header_v1) +
		      sizeof(fw_ext_sig) + 4 + 8 * num_patches) {
		errno = EINVAL;
		return (-1);
	}

	chip_id_base = fw->buf + sizeof(struct rtlbt_fw_header_v1);
	for (i = 0; i < num_patches; i++) {
		if (le16dec(chip_id_base + i * 2) != rom_version + 1)
			continue;
		patch_length = le16dec(chip_id_base + 2 * (num_patches + i));
		patch_offset = le32dec(chip_id_base + 4 * (num_patches + i));
		break;
	}

	if (i >= num_patches) {
		rtlbt_err("can not find patch for chip id %d", rom_version);
		errno = EINVAL;
		return (-1);
	}

	rtlbt_debug(
	    "index=%d length=%x offset=%x", i, patch_length, patch_offset);
	if (fw->len < patch_offset + patch_length) {
		errno = EINVAL;
		return (-1);
	}

	patch_buf = malloc(patch_length);
	if (patch_buf == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	memcpy(patch_buf, fw->buf + patch_offset, patch_length - 4);
	memcpy(patch_buf + patch_length - 4, &fw_header->fw_version, 4);

	free(fw->buf);
	fw->buf = patch_buf;
	fw->len = patch_length;

	return (0);
}

int
rtlbt_append_fwfile(struct rtlbt_firmware *fw, struct rtlbt_firmware *opt)
{
	uint8_t *buf;
	int len;

	len = fw->len + opt->len;
	buf = realloc(fw->buf, len);
	if (buf == NULL)
		return (-1);
	memcpy(buf + fw->len, opt->buf, opt->len);
	fw->buf = buf;
	fw->len = len;

	return (0);
}
