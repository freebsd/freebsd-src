/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2020 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2020 The DragonFly Project
 * All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstab.h>
#include <assert.h>
#include <errno.h>
#include <err.h>

#include <fs/hammer2/hammer2_disk.h>

#include "hammer2_subs.h"

static hammer2_ondisk_t fso;
static int hammer2_volumes_initialized;

static void
hammer2_init_volume(hammer2_volume_t *vol)
{
	vol->fd = -1;
	vol->id = -1;
	vol->offset = (hammer2_off_t)-1;
	vol->size = (hammer2_off_t)-1;
}

void
hammer2_init_ondisk(hammer2_ondisk_t *fsp)
{
	int i;

	bzero(fsp, sizeof(*fsp));
	fsp->version = -1;
	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i)
		hammer2_init_volume(&fsp->volumes[i]);
}

void
hammer2_install_volume(hammer2_volume_t *vol, int fd, int id, const char *path,
		       hammer2_off_t offset, hammer2_off_t size)
{
	bzero(vol, sizeof(*vol));
	vol->fd = fd;
	vol->id = id;
	vol->path = strdup(path);
	vol->offset = offset;
	vol->size = size;
}

void
hammer2_uninstall_volume(hammer2_volume_t *vol)
{
	fsync(vol->fd);
	close(vol->fd);
	free(vol->path);
	hammer2_init_volume(vol);
}

/*
 * Locate a valid volume header.  If any of the four volume headers is good,
 * we have a valid volume header and choose the best one based on mirror_tid.
 */
static int
hammer2_read_volume_header(int fd, const char *path,
			   hammer2_volume_data_t *voldata)
{
	hammer2_volume_data_t vd;
	hammer2_tid_t mirror_tid = -1;
	hammer2_off_t size = check_volume(fd);
	hammer2_crc32_t crc0, crc1;
	const char *p;
	int i, zone = -1;
	ssize_t ret;

	for (i = 0; i < HAMMER2_NUM_VOLHDRS; ++i) {
		if (i * HAMMER2_ZONE_BYTES64 >= size)
			break;
		if (lseek(fd, i * HAMMER2_ZONE_BYTES64, SEEK_SET) == -1)
			break;
		ret = read(fd, &vd, HAMMER2_PBUFSIZE);
		if (ret == -1) {
			fprintf(stderr, "%s #%d: read %s\n",
				path, i, strerror(errno));
			continue;
		}
		if (ret != HAMMER2_PBUFSIZE) {
			fprintf(stderr, "%s #%d: read %s\n",
				path, i, strerror(errno));
			continue;
		}

		p = (const char*)&vd;
		/* verify volume header magic */
		if ((vd.magic != HAMMER2_VOLUME_ID_HBO) &&
		    (vd.magic != HAMMER2_VOLUME_ID_ABO)) {
			fprintf(stderr, "%s #%d: bad magic\n", path, i);
			continue;
		}

		if (vd.magic == HAMMER2_VOLUME_ID_ABO) {
			/* XXX: Reversed-endianness filesystem */
			fprintf(stderr,
				"%s #%d: reverse-endian filesystem detected",
				path, i);
			continue;
		}

		/* verify volume header CRC's */
		crc0 = vd.icrc_sects[HAMMER2_VOL_ICRC_SECT0];
		crc1 = hammer2_icrc32(p + HAMMER2_VOLUME_ICRC0_OFF,
				      HAMMER2_VOLUME_ICRC0_SIZE);
		if (crc0 != crc1) {
			fprintf(stderr,
				"%s #%d: volume header crc mismatch "
				"sect0 %08x/%08x\n",
				path, i, crc0, crc1);
			continue;
		}

		crc0 = vd.icrc_sects[HAMMER2_VOL_ICRC_SECT1];
		crc1 = hammer2_icrc32(p + HAMMER2_VOLUME_ICRC1_OFF,
				      HAMMER2_VOLUME_ICRC1_SIZE);
		if (crc0 != crc1) {
			fprintf(stderr,
				"%s #%d: volume header crc mismatch "
				"sect1 %08x/%08x",
				path, i, crc0, crc1);
			continue;
		}

		crc0 = vd.icrc_volheader;
		crc1 = hammer2_icrc32(p + HAMMER2_VOLUME_ICRCVH_OFF,
				      HAMMER2_VOLUME_ICRCVH_SIZE);
		if (crc0 != crc1) {
			fprintf(stderr,
				"%s #%d: volume header crc mismatch "
				"vh %08x/%08x",
				path, i, crc0, crc1);
			continue;
		}
		if (zone == -1 || mirror_tid < vd.mirror_tid) {
			bcopy(&vd, voldata, sizeof(vd));
			mirror_tid = vd.mirror_tid;
			zone = i;
		}
	}
	return(zone);
}

static void
hammer2_err_uuid_mismatch(uuid_t *uuid1, uuid_t *uuid2, const char *id)
{
	char *p1 = NULL, *p2 = NULL;

	hammer2_uuid_to_str(uuid1, &p1);
	hammer2_uuid_to_str(uuid2, &p2);

	errx(1, "%s uuid mismatch %s vs %s", id, p1, p2);

	free(p1);
	free(p2);
}

static void
hammer2_add_volume(const char *path, int rdonly)
{
	hammer2_volume_data_t voldata;
	hammer2_volume_t *vol;
	struct stat st;
	int fd, i;
	uuid_t uuid;

	fd = open(path, rdonly ? O_RDONLY : O_RDWR);
	if (fd == -1)
		err(1, "open");

	if (fstat(fd, &st) == -1)
		err(1, "fstat");
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "Unsupported file type");

	if (hammer2_read_volume_header(fd, path, &voldata) >= 0) {
		i = voldata.volu_id;
		if (i < 0 || i >= HAMMER2_MAX_VOLUMES)
			errx(1, "%s has bad volume id %d", path, i);
		vol = &fso.volumes[i];
		if (vol->id != -1)
			errx(1, "volume id %d already initialized", i);
		/* all headers must have the same version, nvolumes and uuid */
		if (!fso.nvolumes) {
			fso.version = voldata.version;
			fso.nvolumes = voldata.nvolumes;
			fso.fsid = voldata.fsid;
			fso.fstype = voldata.fstype;
		} else {
			if (fso.version != (int)voldata.version)
				errx(1, "Volume version mismatch %d vs %d",
				     fso.version, (int)voldata.version);
			if (fso.nvolumes != voldata.nvolumes)
				errx(1, "Volume count mismatch %d vs %d",
				     fso.nvolumes, voldata.nvolumes);
			uuid = voldata.fsid;
			if (!uuid_equal(&fso.fsid, &uuid, NULL))
				hammer2_err_uuid_mismatch(&fso.fsid,
							  &uuid,
							  "fsid");
			uuid = voldata.fstype;
			if (!uuid_equal(&fso.fstype, &uuid, NULL))
				hammer2_err_uuid_mismatch(&fso.fstype,
							  &uuid,
							  "fstype");
		}
		/* all per-volume tests passed */
		hammer2_install_volume(vol, fd, i, path,
				       voldata.volu_loff[i], voldata.volu_size);
		fso.total_size += vol->size;
	} else {
		errx(1, "No valid volume headers found!");
	}
}

static void
hammer2_verify_volumes_common(const hammer2_ondisk_t *fsp)
{
	const hammer2_volume_t *vol;
	hammer2_off_t size;
	struct stat *st;
	const char *path;
	int i, j, nvolumes = 0;

	if (fsp->version == -1)
		errx(1, "Bad volume version %d", fsp->version);

	/* check initialized volume count */
	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		vol = &fsp->volumes[i];
		if (vol->id != -1)
			nvolumes++;
	}

	/* fsp->nvolumes hasn't been verified yet, use nvolumes */
	st = calloc(nvolumes, sizeof(*st));

	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		vol = &fsp->volumes[i];
		if (vol->id == -1)
			continue;
		path = vol->path;
		/* check volumes are unique */
		if (stat(path, &st[i]) != 0)
			errx(1, "Failed to stat %s", path);
		if (fstat(vol->fd, &st[i]) != 0)
			errx(1, "Failed to fstat %d", vol->fd);
		for (j = 0; j < i; ++j) {
			if ((st[i].st_ino == st[j].st_ino) &&
			    (st[i].st_dev == st[j].st_dev))
				errx(1, "%s specified more than once", path);
		}
		/* check volume fields are initialized */
		if (vol->fd == -1)
			errx(1, "%s has bad fd %d", path, vol->fd);
		if (vol->offset == (hammer2_off_t)-1)
			errx(1, "%s has bad offset 0x%016jx", path,
			     (intmax_t)vol->offset);
		if (vol->size == (hammer2_off_t)-1)
			errx(1, "%s has bad size 0x%016jx", path,
			     (intmax_t)vol->size);
		/* check volume size vs block device size */
		size = check_volume(vol->fd);
		printf("checkvolu header %d %016jx/%016jx\n", i, vol->size, size);
		if (vol->size > size)
			errx(1, "%s's size 0x%016jx exceeds device size 0x%016jx",
			     path, (intmax_t)vol->size, size);
	}
	free(st);
}

static void
hammer2_verify_volumes_1(hammer2_ondisk_t *fsp,
			 const hammer2_volume_data_t *rootvoldata)
{
	const hammer2_volume_t *vol;
	hammer2_off_t off;
	const char *path;
	int i, nvolumes = 0;

	/* check initialized volume count */
	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		vol = &fsp->volumes[i];
		if (vol->id != -1)
			nvolumes++;
	}
	if (nvolumes != 1)
		errx(1, "Only 1 volume supported");
	fsp->nvolumes = nvolumes; /* adjust with actual count */

	/* check volume header */
	if (rootvoldata) {
		if (rootvoldata->volu_id)
			errx(1, "Volume id %d must be 0", rootvoldata->volu_id);
		if (rootvoldata->nvolumes)
			errx(1, "Volume count %d must be 0",
			     rootvoldata->nvolumes);
		if (rootvoldata->total_size)
			errx(1, "Total size 0x%016jx must be 0",
			     (intmax_t)rootvoldata->total_size);
		for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
			off = rootvoldata->volu_loff[i];
			if (off)
				errx(1, "Volume offset[%d] 0x%016jx must be 0",
				     i, (intmax_t)off);
		}
	}

	/* check volume */
	vol = &fsp->volumes[0];
	path = vol->path;
	if (vol->id)
		errx(1, "%s has non zero id %d", path, vol->id);
	if (vol->offset)
		errx(1, "%s has non zero offset 0x%016jx", path,
		     (intmax_t)vol->offset);
	if (vol->size & HAMMER2_VOLUME_ALIGNMASK64)
		errx(1, "%s's size is not 0x%016jx aligned", path,
		     (intmax_t)HAMMER2_VOLUME_ALIGN);
}

static void
hammer2_verify_volumes_2(const hammer2_ondisk_t *fsp,
			 const hammer2_volume_data_t *rootvoldata)
{
	const hammer2_volume_t *vol;
	hammer2_off_t off;
	const char *path;
	int i, nvolumes = 0;

	/* check initialized volume count */
	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		vol = &fsp->volumes[i];
		if (vol->id != -1)
			nvolumes++;
	}
	if (fsp->nvolumes != nvolumes)
		errx(1, "Volume count mismatch %d vs %d",
		     fsp->nvolumes, nvolumes);

	/* check volume header */
	if (rootvoldata) {
		if (rootvoldata->volu_id != HAMMER2_ROOT_VOLUME)
			errx(1, "Volume id %d must be %d",
			     rootvoldata->volu_id, HAMMER2_ROOT_VOLUME);
		if (rootvoldata->nvolumes != fso.nvolumes)
			errx(1, "Volume header requires %d devices, %d specified",
			     rootvoldata->nvolumes, fso.nvolumes);
		if (rootvoldata->total_size != fso.total_size)
			errx(1, "Total size 0x%016jx does not equal sum of "
			     "volumes 0x%016jx",
			     rootvoldata->total_size, fso.total_size);
		for (i = 0; i < nvolumes; ++i) {
			off = rootvoldata->volu_loff[i];
			if (off == (hammer2_off_t)-1)
				errx(1, "Volume offset[%d] 0x%016jx must not be -1",
				     i, (intmax_t)off);
		}
		for (i = nvolumes; i < HAMMER2_MAX_VOLUMES; ++i) {
			off = rootvoldata->volu_loff[i];
			if (off != (hammer2_off_t)-1)
				errx(1, "Volume offset[%d] 0x%016jx must be -1",
				     i, (intmax_t)off);
		}
	}

	/* check volumes */
	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		vol = &fsp->volumes[i];
		if (vol->id == -1)
			continue;
		path = vol->path;
		/* check offset */
		if (vol->offset & HAMMER2_FREEMAP_LEVEL1_MASK)
			errx(1, "%s's offset 0x%016jx not 0x%016jx aligned",
			     path, (intmax_t)vol->offset,
			     HAMMER2_FREEMAP_LEVEL1_SIZE);
		/* check vs previous volume */
		if (i) {
			if (vol->id != (vol-1)->id + 1)
				errx(1, "%s has inconsistent id %d", path,
				     vol->id);
			if (vol->offset != (vol-1)->offset + (vol-1)->size)
				errx(1, "%s has inconsistent offset 0x%016jx",
				     path, (intmax_t)vol->offset);
		} else { /* first */
			if (vol->offset)
				errx(1, "%s has non zero offset 0x%016jx", path,
				     (intmax_t)vol->offset);
		}
		/* check size for non-last and last volumes */
		if (i != fsp->nvolumes - 1) {
			if (vol->size < HAMMER2_FREEMAP_LEVEL1_SIZE)
				errx(1, "%s's size must be >= 0x%016jx", path,
				     (intmax_t)HAMMER2_FREEMAP_LEVEL1_SIZE);
			if (vol->size & HAMMER2_FREEMAP_LEVEL1_MASK)
				errx(1, "%s's size is not 0x%016jx aligned",
				     path,
				     (intmax_t)HAMMER2_FREEMAP_LEVEL1_SIZE);
		} else { /* last */
			if (vol->size & HAMMER2_VOLUME_ALIGNMASK64)
				errx(1, "%s's size is not 0x%016jx aligned",
				     path, (intmax_t)HAMMER2_VOLUME_ALIGN);
		}
	}
}

void
hammer2_verify_volumes(hammer2_ondisk_t *fsp,
		       const hammer2_volume_data_t *rootvoldata)
{
	hammer2_verify_volumes_common(fsp);
	if (fsp->version >= HAMMER2_VOL_VERSION_MULTI_VOLUMES)
		hammer2_verify_volumes_2(fsp, rootvoldata);
	else
		hammer2_verify_volumes_1(fsp, rootvoldata);
	assert(fsp->nvolumes > 0);
}

void
hammer2_print_volumes(const hammer2_ondisk_t *fsp)
{
	const hammer2_volume_t *vol;
	int i, n, w = 0;

	for (i = 0; i < fsp->nvolumes; ++i) {
		vol = &fsp->volumes[i];
		n = (int)strlen(vol->path);
		if (n > w)
			w = n;
	}

	printf("total    %-*.*s 0x%016jx 0x%016jx\n",
		w, w, "", (intmax_t)0, (intmax_t)fsp->total_size);

	for (i = 0; i < fsp->nvolumes; ++i) {
		vol = &fsp->volumes[i];
		printf("volume%-2d %-*.*s 0x%016jx 0x%016jx%s\n",
		       vol->id, w, w, vol->path, (intmax_t)vol->offset,
		       (intmax_t)vol->size,
		       (vol->id == HAMMER2_ROOT_VOLUME ?
		       " (root volume)" : ""));
	}
}

void
hammer2_init_volumes(const char *blkdevs, int rdonly)
{
	hammer2_volume_data_t *rootvoldata;
	char *p, *devpath;

	if (hammer2_volumes_initialized)
		errx(1, "Already initialized");
	if (!blkdevs)
		errx(1, "NULL blkdevs");

	hammer2_init_ondisk(&fso);
	p = strdup(blkdevs);
	while ((devpath = p) != NULL) {
		if ((p = strchr(p, ':')) != NULL)
			*p++ = 0;
		/* DragonFly uses getdevpath(3) here */
		if (strchr(devpath, ':'))
			hammer2_init_volumes(devpath, rdonly);
		else
			hammer2_add_volume(devpath, rdonly);
	}
	free(p);
	hammer2_volumes_initialized = 1;

	rootvoldata = hammer2_read_root_volume_header();
	hammer2_verify_volumes(&fso, rootvoldata);
	free(rootvoldata);
}

void
hammer2_cleanup_volumes(void)
{
	hammer2_volume_t *vol;
	int i;

	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		vol = &fso.volumes[i];
		if (vol->id == -1)
			continue;
		hammer2_uninstall_volume(vol);
	}
	hammer2_volumes_initialized = 0;
}

typedef void (*callback)(const hammer2_volume_t*, void *data);

static int
hammer2_get_volume_attr(hammer2_off_t offset, callback fn, void *data)
{
	hammer2_volume_t *vol;
	int i;

	assert(hammer2_volumes_initialized == 1);
	offset &= ~HAMMER2_OFF_MASK_RADIX;

	/* do binary search if users really use this many supported volumes */
	for (i = 0; i < fso.nvolumes; ++i) {
		vol = &fso.volumes[i];
		if ((offset >= vol->offset) &&
		    (offset < vol->offset + vol->size)) {
			fn(vol, data);
			return(0);
		}
	}

	return(-1);
}

/* fd */
static void
hammer2_volume_fd_cb(const hammer2_volume_t *vol, void *data)
{
	*(int*)data = vol->fd;
}

int
hammer2_get_volume_fd(hammer2_off_t offset)
{
	int ret = 0;

	if (hammer2_get_volume_attr(offset, hammer2_volume_fd_cb, &ret) < 0)
		return(-1);
	return(ret);
}

int
hammer2_get_root_volume_fd(void)
{
	return(hammer2_get_volume_fd(0));
}

/* id */
static void
hammer2_volume_id_cb(const hammer2_volume_t *vol, void *data)
{
	*(int*)data = vol->id;
}

int
hammer2_get_volume_id(hammer2_off_t offset)
{
	int ret = 0;

	if (hammer2_get_volume_attr(offset, hammer2_volume_id_cb, &ret) < 0)
		return(-1);
	return(ret);
}

int
hammer2_get_root_volume_id(void)
{
	return(hammer2_get_volume_id(0));
}

/* path */
static void
hammer2_volume_path_cb(const hammer2_volume_t *vol, void *data)
{
	*(const char**)data = vol->path;
}

const char *
hammer2_get_volume_path(hammer2_off_t offset)
{
	const char *ret = NULL;

	if (hammer2_get_volume_attr(offset, hammer2_volume_path_cb, &ret) < 0)
		return(NULL);
	return(ret);
}

const char *
hammer2_get_root_volume_path(void)
{
	return(hammer2_get_volume_path(0));
}

/* offset */
static void
hammer2_volume_offset_cb(const hammer2_volume_t *vol, void *data)
{
	*(hammer2_off_t*)data = vol->offset;
}

hammer2_off_t
hammer2_get_volume_offset(hammer2_off_t offset)
{
	hammer2_off_t ret = 0;

	if (hammer2_get_volume_attr(offset, hammer2_volume_offset_cb, &ret) < 0)
		return(-1);
	return(ret);
}

hammer2_off_t
hammer2_get_root_volume_offset(void)
{
	return(hammer2_get_volume_offset(0));
}

/* size */
static void
hammer2_volume_size_cb(const hammer2_volume_t *vol, void *data)
{
	*(hammer2_off_t*)data = vol->size;
}

hammer2_off_t
hammer2_get_volume_size(hammer2_off_t offset)
{
	hammer2_off_t ret = 0;

	if (hammer2_get_volume_attr(offset, hammer2_volume_size_cb, &ret) < 0)
		return(-1);
	return(ret);
}

hammer2_off_t
hammer2_get_root_volume_size(void)
{
	return(hammer2_get_volume_size(0));
}

/* total size */
hammer2_off_t
hammer2_get_total_size(void)
{
	return(fso.total_size);
}

hammer2_volume_data_t*
hammer2_read_root_volume_header(void)
{
	hammer2_volume_data_t *voldata;
	int fd = hammer2_get_root_volume_fd();
	const char *path = hammer2_get_root_volume_path();

	if (fd == -1)
		return(NULL);

	voldata = calloc(1, sizeof(*voldata));
	if (hammer2_read_volume_header(fd, path, voldata) >= 0)
		return(voldata);
	else
		errx(1, "Failed to read volume header");
}
