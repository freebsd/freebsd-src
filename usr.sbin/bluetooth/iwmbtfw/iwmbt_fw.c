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

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iwmbt_fw.h"
#include "iwmbt_dbg.h"

int
iwmbt_fw_read(struct iwmbt_firmware *fw, const char *fwname)
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
		iwmbt_err("read len %d != file size %d",
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
iwmbt_fw_free(struct iwmbt_firmware *fw)
{
	if (fw->fwname)
		free(fw->fwname);
	if (fw->buf)
		free(fw->buf);
	memset(fw, 0, sizeof(*fw));
}

char *
iwmbt_get_fwname(struct iwmbt_version *ver, struct iwmbt_boot_params *params,
    const char *prefix, const char *suffix)
{
	struct stat sb;
	char *fwname;

	switch (ver->hw_variant) {
	case 0x07:	/* 7260 */
	case 0x08:	/* 7265 */
		// NB: don't use params, they are NULL for 7xxx
		asprintf(&fwname, "%s/ibt-hw-%x.%x.%x-fw-%x.%x.%x.%x.%x.%s",
		    prefix,
		    le16toh(ver->hw_platform),
		    le16toh(ver->hw_variant),
		    le16toh(ver->hw_revision),
		    le16toh(ver->fw_variant),
		    le16toh(ver->fw_revision),
		    le16toh(ver->fw_build_num),
		    le16toh(ver->fw_build_ww),
		    le16toh(ver->fw_build_yy),
		    suffix);
		/*
		 * Fallback to the default firmware patch if
		 * the correct firmware patch file is not found.
		 */
		if (stat(fwname, &sb) != 0 && errno == ENOENT) {
			free(fwname);
			asprintf(&fwname, "%s/ibt-hw-%x.%x.%s",
			    prefix,
			    le16toh(ver->hw_platform),
			    le16toh(ver->hw_variant),
			    suffix);
		}
		break;

	case 0x0b:	/* 8260 */
	case 0x0c:	/* 8265 */
		asprintf(&fwname, "%s/ibt-%u-%u.%s",
		    prefix,
		    le16toh(ver->hw_variant),
		    le16toh(params->dev_revid),
		    suffix);
		break;

	case 0x11:	/* 9560 */
	case 0x12:	/* 9260 */
	case 0x13:
	case 0x14:	/* 22161 */
		asprintf(&fwname, "%s/ibt-%u-%u-%u.%s",
		    prefix,
		    le16toh(ver->hw_variant),
		    le16toh(ver->hw_revision),
		    le16toh(ver->fw_revision),
		    suffix);
		break;

	default:
		fwname = NULL;
	}

	return (fwname);
}

char *
iwmbt_get_fwname_tlv(struct iwmbt_version_tlv *ver, const char *prefix,
    const char *suffix)
{
	char *fwname;

#define	IWMBT_PACK_CNVX_TOP(cnvx_top)	((uint16_t)(	\
	((cnvx_top) & 0x0f000000) >> 16 |		\
	((cnvx_top) & 0x0000000f) << 12 |		\
	((cnvx_top) & 0x00000ff0) >> 4))

	asprintf(&fwname, "%s/ibt-%04x-%04x.%s",
	    prefix,
	    IWMBT_PACK_CNVX_TOP(ver->cnvi_top),
	    IWMBT_PACK_CNVX_TOP(ver->cnvr_top),
	    suffix);

	return (fwname);
}

int
iwmbt_parse_tlv(uint8_t *data, uint8_t datalen,
    struct iwmbt_version_tlv *version)
{
	uint8_t status, type, len;

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
