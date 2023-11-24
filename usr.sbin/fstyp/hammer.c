/*-
 * Copyright (c) 2016-2019 The DragonFly Project
 * Copyright (c) 2016-2019 Tomohiro Kusumi <tkusumi@netbsd.org>
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
#include <string.h>
#include <err.h>
#include <assert.h>

#include <sys/types.h>

#include "hammer_disk.h"

#include "fstyp.h"

extern int fsvtyp_hammer(const char *blkdevs, char *label, size_t size);
extern int fsvtyp_hammer_partial(const char *blkdevs, char *label, size_t size);

static hammer_volume_ondisk_t
read_ondisk(FILE *fp)
{
	hammer_volume_ondisk_t ondisk;

	ondisk = read_buf(fp, 0, sizeof(*ondisk));
	if (ondisk == NULL)
		err(1, "failed to read ondisk");

	return (ondisk);
}

static int
test_ondisk(const hammer_volume_ondisk_t ondisk)
{
	static int count = 0;
	static hammer_uuid_t fsid, fstype;
	static char label[64];

	if (ondisk->vol_signature != HAMMER_FSBUF_VOLUME &&
	    ondisk->vol_signature != HAMMER_FSBUF_VOLUME_REV)
		return (1);
	if (ondisk->vol_rootvol != HAMMER_ROOT_VOLNO)
		return (2);
	if (ondisk->vol_no < 0 || ondisk->vol_no > HAMMER_MAX_VOLUMES - 1)
		return (3);
	if (ondisk->vol_count < 1 || ondisk->vol_count > HAMMER_MAX_VOLUMES)
		return (4);

	if (count == 0) {
		count = ondisk->vol_count;
		assert(count != 0);
		memcpy(&fsid, &ondisk->vol_fsid, sizeof(fsid));
		memcpy(&fstype, &ondisk->vol_fstype, sizeof(fstype));
		strlcpy(label, ondisk->vol_label, sizeof(label));
	} else {
		if (ondisk->vol_count != count)
			return (5);
		if (memcmp(&ondisk->vol_fsid, &fsid, sizeof(fsid)))
			return (6);
		if (memcmp(&ondisk->vol_fstype, &fstype, sizeof(fstype)))
			return (7);
		if (strcmp(ondisk->vol_label, label))
			return (8);
	}

	return (0);
}

int
fstyp_hammer(FILE *fp, char *label, size_t size)
{
	hammer_volume_ondisk_t ondisk;
	int error = 1;

	ondisk = read_ondisk(fp);
	if (ondisk->vol_no != HAMMER_ROOT_VOLNO)
		goto fail;
	if (ondisk->vol_count != 1)
		goto fail;
	if (test_ondisk(ondisk))
		goto fail;

	strlcpy(label, ondisk->vol_label, size);
	error = 0;
fail:
	free(ondisk);
	return (error);
}

static int
test_volume(const char *volpath)
{
	hammer_volume_ondisk_t ondisk;
	FILE *fp;
	int volno = -1;

	if ((fp = fopen(volpath, "r")) == NULL)
		err(1, "failed to open %s", volpath);

	ondisk = read_ondisk(fp);
	fclose(fp);
	if (test_ondisk(ondisk))
		goto fail;

	volno = ondisk->vol_no;
fail:
	free(ondisk);
	return (volno);
}

static int
__fsvtyp_hammer(const char *blkdevs, char *label, size_t size, int partial)
{
	hammer_volume_ondisk_t ondisk = NULL;
	FILE *fp;
	char *dup, *p, *volpath, x[HAMMER_MAX_VOLUMES];
	int i, volno, error = 1;

	if (!blkdevs)
		goto fail;

	memset(x, 0, sizeof(x));
	dup = strdup(blkdevs);
	p = dup;

	volpath = NULL;
	volno = -1;
	while (p) {
		volpath = p;
		if ((p = strchr(p, ':')) != NULL)
			*p++ = '\0';
		if ((volno = test_volume(volpath)) == -1)
			break;
		assert(volno >= 0);
		assert(volno < HAMMER_MAX_VOLUMES);
		x[volno]++;
	}

	if (!volpath)
		err(1, "invalid path %s", blkdevs);
	if ((fp = fopen(volpath, "r")) == NULL)
		err(1, "failed to open %s", volpath);
	ondisk = read_ondisk(fp);
	fclose(fp);

	free(dup);

	if (volno == -1)
		goto fail;
	if (partial)
		goto success;

	for (i = 0; i < HAMMER_MAX_VOLUMES; i++)
		if (x[i] > 1)
			goto fail;
	for (i = 0; i < HAMMER_MAX_VOLUMES; i++)
		if (x[i] == 0)
			break;
	if (ondisk->vol_count != i)
		goto fail;
	for (; i < HAMMER_MAX_VOLUMES; i++)
		if (x[i] != 0)
			goto fail;
success:
	strlcpy(label, ondisk->vol_label, size);
	error = 0;
fail:
	free(ondisk);
	return (error);
}

int
fsvtyp_hammer(const char *blkdevs, char *label, size_t size)
{
	return (__fsvtyp_hammer(blkdevs, label, size, 0));
}

int
fsvtyp_hammer_partial(const char *blkdevs, char *label, size_t size)
{
	return (__fsvtyp_hammer(blkdevs, label, size, 1));
}
