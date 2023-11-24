/*
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>.  All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fstyp.h"

/*
 * https://developer.apple.com/library/archive/technotes/tn/tn1150.html
 */

#define	VOL_HDR_OFF	1024

typedef uint32_t hfsp_cat_nodeid;

typedef struct hfsp_ext_desc {
	uint32_t	ex_startBlock;
	uint32_t	ex_blockCount;
} hfsp_ext_desc;

typedef struct hfsp_fork_data {
	uint64_t	fd_logicalSz;
	uint32_t	fd_clumpSz;
	uint32_t	fd_totalBlocks;
	hfsp_ext_desc	fd_extents[8];
} hfsp_fork_data;

struct hfsp_vol_hdr {
	char		hp_signature[2];
	uint16_t	hp_version;
	uint32_t	hp_attributes;
	uint32_t	hp_lastMounted;
	uint32_t	hp_journalInfoBlock;

	/* Creation / etc dates. */
	uint32_t	hp_create;
	uint32_t	hp_modify;
	uint32_t	hp_backup;
	uint32_t	hp_checked;

	/* Stats */
	uint32_t	hp_files;
	uint32_t	hp_folders;

	/* Parameters */
	uint32_t	hp_blockSize;
	uint32_t	hp_totalBlocks;
	uint32_t	hp_freeBlocks;

	uint32_t	hp_nextAlloc;
	uint32_t	hp_rsrcClumpSz;
	uint32_t	hp_dataClumpSz;

	hfsp_cat_nodeid	hp_nextCatID;

	uint32_t	hp_writeCount;
	uint64_t	hp_encodingsBM;

	uint32_t	hp_finderInfo[8];

	hfsp_fork_data	hp_allocationFile;
	hfsp_fork_data	hp_extentsFile;
	hfsp_fork_data	hp_catalogFile;
	hfsp_fork_data	hp_attributesFile;
	hfsp_fork_data	hp_startupFile;
};
_Static_assert(sizeof(struct hfsp_vol_hdr) == 512, "");

int
fstyp_hfsp(FILE *fp, char *label, size_t size)
{
	struct hfsp_vol_hdr *hdr;
	int retval;

	retval = 1;
	hdr = read_buf(fp, VOL_HDR_OFF, sizeof(*hdr));
	if (hdr == NULL)
		goto fail;

	if ((strncmp(hdr->hp_signature, "H+", 2) != 0 || hdr->hp_version != 4)
	    &&
	    (strncmp(hdr->hp_signature, "HX", 2) != 0 || hdr->hp_version != 5))
		goto fail;

	/* This is an HFS+ volume. */
	retval = 0;

	/* No label support yet. */
	(void)size;
	(void)label;

fail:
	free(hdr);
	return (retval);
}
