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
