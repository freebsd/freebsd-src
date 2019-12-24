/*-
 * Copyright (c) 2017-2019 The DragonFly Project
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <sys/types.h>

#include "hammer2_disk.h"

#include "fstyp.h"

static hammer2_volume_data_t*
__read_voldata(FILE *fp)
{
	hammer2_volume_data_t *voldata;

	voldata = read_buf(fp, 0, sizeof(*voldata));
	if (voldata == NULL)
		err(1, "failed to read volume data");

	return (voldata);
}

static int
__test_voldata(const hammer2_volume_data_t *voldata)
{
	if (voldata->magic != HAMMER2_VOLUME_ID_HBO &&
	    voldata->magic != HAMMER2_VOLUME_ID_ABO)
		return (1);

	return (0);
}

static int
__read_label(FILE *fp, char *label, size_t size)
{
	hammer2_blockref_t broot, best, *bref;
	hammer2_media_data_t *vols[HAMMER2_NUM_VOLHDRS], *media;
	hammer2_off_t io_off, io_base;
	size_t bytes, io_bytes, boff;
	int i, best_i, error = 0;

	best_i = -1;
	memset(&best, 0, sizeof(best));

	for (i = 0; i < HAMMER2_NUM_VOLHDRS; i++) {
		memset(&broot, 0, sizeof(broot));
		broot.type = HAMMER2_BREF_TYPE_VOLUME;
		broot.data_off = (i * HAMMER2_ZONE_BYTES64) | HAMMER2_PBUFRADIX;
		vols[i] = read_buf(fp, broot.data_off & ~HAMMER2_OFF_MASK_RADIX,
		    sizeof(*vols[i]));
		broot.mirror_tid = vols[i]->voldata.mirror_tid;
		if (best_i < 0 || best.mirror_tid < broot.mirror_tid) {
			best_i = i;
			best = broot;
		}
	}
	if (best_i == -1) {
		warnx("Failed to find volume header from zones");
		error = 1;
		goto done;
	}

	bref = &vols[best_i]->voldata.sroot_blockset.blockref[0];
	if (bref->type != HAMMER2_BREF_TYPE_INODE) {
		warnx("Superroot blockref type is not inode");
		error = 2;
		goto done;
	}

	bytes = bref->data_off & HAMMER2_OFF_MASK_RADIX;
	if (bytes)
		bytes = (size_t)1 << bytes;
	if (bytes != sizeof(hammer2_inode_data_t)) {
		warnx("Superroot blockref size does not match inode size");
		error = 3;
		goto done;
	}

	io_off = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
	io_base = io_off & ~(hammer2_off_t)(HAMMER2_MINIOSIZE - 1);
	boff = io_off - io_base;

	io_bytes = HAMMER2_MINIOSIZE;
	while (io_bytes + boff < bytes)
		io_bytes <<= 1;
	if (io_bytes > sizeof(*media)) {
		warnx("Invalid I/O bytes");
		error = 4;
		goto done;
	}

	media = read_buf(fp, io_base, io_bytes);
	if (boff)
		memcpy(media, (char*)media + boff, bytes);

	strlcpy(label, (char*)media->ipdata.filename, size);
	free(media);
done:
	for (i = 0; i < HAMMER2_NUM_VOLHDRS; i++)
		free(vols[i]);

	return (error);
}

int
fstyp_hammer2(FILE *fp, char *label, size_t size)
{
	hammer2_volume_data_t *voldata;
	int error = 1;

	voldata = __read_voldata(fp);
	if (__test_voldata(voldata))
		goto done;

	error = __read_label(fp, label, size);
done:
	free(voldata);
	return (error);
}
