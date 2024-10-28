/*-
 * Copyright (c) 2017-2019 The DragonFly Project
 * Copyright (c) 2017-2019 Tomohiro Kusumi <tkusumi@netbsd.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include <sys/types.h>

#include "hammer2_disk.h"

#include "fstyp.h"

static hammer2_volume_data_t*
read_voldata(FILE *fp)
{
	hammer2_volume_data_t *voldata;

	voldata = read_buf(fp, 0, sizeof(*voldata));
	if (voldata == NULL)
		err(1, "failed to read volume data");

	return (voldata);
}

static int
test_voldata(const hammer2_volume_data_t *voldata)
{
	if (voldata->magic != HAMMER2_VOLUME_ID_HBO &&
	    voldata->magic != HAMMER2_VOLUME_ID_ABO)
		return (1);

	return (0);
}

static hammer2_media_data_t*
read_media(FILE *fp, const hammer2_blockref_t *bref, size_t *media_bytes)
{
	hammer2_media_data_t *media;
	hammer2_off_t io_off, io_base;
	size_t bytes, io_bytes, boff;

	bytes = (bref->data_off & HAMMER2_OFF_MASK_RADIX);
	if (bytes)
		bytes = (size_t)1 << bytes;
	*media_bytes = bytes;

	if (!bytes) {
		warnx("blockref has no data");
		return (NULL);
	}

	io_off = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
	io_base = io_off & ~(hammer2_off_t)(HAMMER2_MINIOSIZE - 1);
	boff = io_off - io_base;

	io_bytes = HAMMER2_MINIOSIZE;
	while (io_bytes + boff < bytes)
		io_bytes <<= 1;

	if (io_bytes > sizeof(hammer2_media_data_t)) {
		warnx("invalid I/O bytes");
		return (NULL);
	}

	if (fseek(fp, io_base, SEEK_SET) == -1) {
		warnx("failed to seek media");
		return (NULL);
	}
	media = read_buf(fp, io_base, io_bytes);
	if (media == NULL) {
		warnx("failed to read media");
		return (NULL);
	}
	if (boff)
		memcpy(media, (char *)media + boff, bytes);

	return (media);
}

static int
find_pfs(FILE *fp, const hammer2_blockref_t *bref, const char *pfs, bool *res)
{
	hammer2_media_data_t *media;
	hammer2_inode_data_t ipdata;
	hammer2_blockref_t *bscan;
	size_t bytes;
	int i, bcount;

	media = read_media(fp, bref, &bytes);
	if (media == NULL)
		return (-1);

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
		ipdata = media->ipdata;
		if (ipdata.meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT) {
			bscan = &ipdata.u.blockset.blockref[0];
			bcount = HAMMER2_SET_COUNT;
		} else {
			bscan = NULL;
			bcount = 0;
			if (ipdata.meta.op_flags & HAMMER2_OPFLAG_PFSROOT) {
				if (memchr(ipdata.filename, 0,
				    sizeof(ipdata.filename))) {
					if (!strcmp(
					    (const char*)ipdata.filename, pfs))
						*res = true;
				} else {
					if (strlen(pfs) > 0 &&
					    !memcmp(ipdata.filename, pfs,
					    strlen(pfs)))
						*res = true;
				}
			} else
				assert(0);
		}
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		bscan = &media->npdata[0];
		bcount = bytes / sizeof(hammer2_blockref_t);
		break;
	default:
		bscan = NULL;
		bcount = 0;
		break;
	}

	for (i = 0; i < bcount; ++i) {
		if (bscan[i].type != HAMMER2_BREF_TYPE_EMPTY) {
			if (find_pfs(fp, &bscan[i], pfs, res) == -1) {
				free(media);
				return (-1);
			}
		}
	}
	free(media);

	return (0);
}

static char*
extract_device_name(const char *devpath)
{
	char *p, *head;

	if (!devpath)
		return NULL;

	p = strdup(devpath);
	head = p;

	p = strchr(p, '@');
	if (p)
		*p = 0;

	p = strrchr(head, '/');
	if (p) {
		p++;
		if (*p == 0) {
			free(head);
			return NULL;
		}
		p = strdup(p);
		free(head);
		return p;
	}

	return head;
}

static int
read_label(FILE *fp, char *label, size_t size)
{
	hammer2_blockref_t broot, best, *bref;
	hammer2_media_data_t *vols[HAMMER2_NUM_VOLHDRS], *media;
	size_t bytes;
	bool res = false;
	int i, best_i, error = 1;
	const char *pfs;
	char *devname;

	best_i = -1;
	memset(&best, 0, sizeof(best));

	for (i = 0; i < HAMMER2_NUM_VOLHDRS; i++) {
		memset(&broot, 0, sizeof(broot));
		broot.type = HAMMER2_BREF_TYPE_VOLUME;
		broot.data_off = (i * HAMMER2_ZONE_BYTES64) | HAMMER2_PBUFRADIX;
		vols[i] = read_buf(fp, broot.data_off & ~HAMMER2_OFF_MASK_RADIX,
		    sizeof(*vols[i]));
		if (vols[i] == NULL)
			errx(1, "failed to read volume header");
		broot.mirror_tid = vols[i]->voldata.mirror_tid;
		if (best_i < 0 || best.mirror_tid < broot.mirror_tid) {
			best_i = i;
			best = broot;
		}
	}

	bref = &vols[best_i]->voldata.sroot_blockset.blockref[0];
	if (bref->type != HAMMER2_BREF_TYPE_INODE) {
		warnx("blockref type is not inode");
		goto fail;
	}

	media = read_media(fp, bref, &bytes);
	if (media == NULL) {
		goto fail;
	}

	pfs = "";
	devname = extract_device_name(NULL);
	assert(!devname); /* Currently always NULL in FreeBSD. */

	/* Add device name to help support multiple autofs -media mounts. */
	if (find_pfs(fp, bref, pfs, &res) == 0 && res) {
		if (devname)
			snprintf(label, size, "%s_%s", pfs, devname);
		else
			strlcpy(label, pfs, size);
	} else {
		memset(label, 0, size);
		memcpy(label, media->ipdata.filename,
		    sizeof(media->ipdata.filename));
		if (devname) {
			strlcat(label, "_", size);
			strlcat(label, devname, size);
		}
	}
	if (devname)
		free(devname);
	free(media);
	error = 0;
fail:
	for (i = 0; i < HAMMER2_NUM_VOLHDRS; i++)
		free(vols[i]);

	return (error);
}

int
fstyp_hammer2(FILE *fp, char *label, size_t size)
{
	hammer2_volume_data_t *voldata;
	int error = 1;

	voldata = read_voldata(fp);
	if (test_voldata(voldata))
		goto fail;

	error = read_label(fp, label, size);
fail:
	free(voldata);
	return (error);
}
