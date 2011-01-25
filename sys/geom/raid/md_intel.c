/*-
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <geom/geom.h>
#include "geom/raid/g_raid.h"
#include "g_raid_md_if.h"

static MALLOC_DEFINE(M_MD_INTEL, "md_intel_data", "GEOM_RAID Intel metadata");

struct intel_raid_map {
	uint32_t	offset;
	uint32_t	disk_sectors;
	uint32_t	stripe_count;
	uint16_t	strip_sectors;
	uint8_t		status;
#define INTEL_S_READY           0x00
#define INTEL_S_DISABLED        0x01
#define INTEL_S_DEGRADED        0x02
#define INTEL_S_FAILURE         0x03

	uint8_t		type;
#define INTEL_T_RAID0           0x00
#define INTEL_T_RAID1           0x01
#define INTEL_T_RAID5           0x05

	uint8_t		total_disks;
	uint8_t		total_domains;
	uint8_t		failed_disk_num;
	uint8_t		ddf;
	uint32_t	filler_2[7];
	uint32_t	disk_idx[1];	/* total_disks entries. */
#define INTEL_DI_IDX	0x00ffffff
#define INTEL_DI_RBLD	0x01000000
} __packed;

struct intel_raid_vol {
	uint8_t		name[16];
	u_int64_t	total_sectors __packed;
	uint32_t	state;
	uint32_t	reserved;
	uint8_t		migr_priority;
	uint8_t		num_sub_vols;
	uint8_t		tid;
	uint8_t		cng_master_disk;
	uint16_t	cache_policy;
	uint8_t		cng_state;
	uint8_t		cng_sub_state;
	uint32_t	filler_0[10];

	uint32_t	curr_migr_unit;
	uint32_t	checkpoint_id;
	uint8_t		migr_state;
	uint8_t		migr_type;
#define INTEL_MT_INIT		0
#define INTEL_MT_REBUILD	1
#define INTEL_MT_VERIFY		2
#define INTEL_MT_GEN_MIGR	3
#define INTEL_MT_STATE_CHANGE	4
#define INTEL_MT_REPAIR		5
	uint8_t		dirty;
	uint8_t		fs_state;
	uint16_t	verify_errors;
	uint16_t	bad_blocks;
	uint32_t	filler_1[4];
	struct intel_raid_map map[1];	/* 2 entries if migr_state != 0. */
} __packed;

struct intel_raid_disk {
#define INTEL_SERIAL_LEN	16
	uint8_t		serial[INTEL_SERIAL_LEN];
	uint32_t	sectors;
	uint32_t	id;
	uint32_t	flags;
#define INTEL_F_SPARE		0x01
#define INTEL_F_ASSIGNED	0x02
#define INTEL_F_FAILED		0x04
#define INTEL_F_ONLINE		0x08

	uint32_t	filler[5];
} __packed;

struct intel_raid_conf {
	uint8_t		intel_id[24];
#define INTEL_MAGIC             "Intel Raid ISM Cfg Sig. "

	uint8_t		version[6];
#define INTEL_VERSION_1000	"1.0.00"	/* RAID0 */
#define INTEL_VERSION_1100	"1.1.00"	/* RAID1 */
#define INTEL_VERSION_1200	"1.2.00"	/* Many volumes */
#define INTEL_VERSION_1201	"1.2.01"	/* 3 or 4 disks */
#define INTEL_VERSION_1202	"1.2.02"	/* RAID5 */
#define INTEL_VERSION_1204	"1.2.04"	/* 5 or 6 disks */
#define INTEL_VERSION_1206	"1.2.06"	/* CNG */
#define INTEL_VERSION_1300	"1.3.00"	/* Attributes */

	uint8_t		dummy_0[2];
	uint32_t	checksum;
	uint32_t	config_size;
	uint32_t	config_id;
	uint32_t	generation;
	uint32_t	error_log_size;
	uint32_t	attributes;
#define INTEL_ATTR_RAID0	0x00000001
#define INTEL_ATTR_RAID1	0x00000002
#define INTEL_ATTR_RAID10	0x00000004
#define INTEL_ATTR_RAID1E	0x00000008
#define INTEL_ATTR_RAID5	0x00000010
#define INTEL_ATTR_RAIDCNG	0x00000020
#define INTEL_ATTR_2TB		0x20000000
#define INTEL_ATTR_PM		0x40000000
#define INTEL_ATTR_CHECKSUM	0x80000000

	uint8_t		total_disks;
	uint8_t		total_volumes;
	uint8_t		dummy_2[2];
	uint32_t	filler_0[39];
	struct intel_raid_disk	disk[1];	/* total_disks entries. */
	/* Here goes total_volumes of struct intel_raid_vol. */
} __packed;

#define INTEL_MAX_MD_SIZE(ndisks)				\
    (sizeof(struct intel_raid_conf) +				\
     sizeof(struct intel_raid_disk) * (ndisks - 1) +		\
     sizeof(struct intel_raid_vol) * 2 +			\
     sizeof(struct intel_raid_map) * 2 +			\
     sizeof(uint32_t) * (ndisks - 1) * 4)

struct g_raid_md_intel_perdisk {
	struct intel_raid_conf	*pd_meta;
	int			 pd_disk_pos;
	struct intel_raid_disk	 pd_disk_meta;
};

struct g_raid_md_intel_object {
	struct g_raid_md_object	 mdio_base;
	uint32_t		 mdio_config_id;
	uint32_t		 mdio_generation;
	struct intel_raid_conf	*mdio_meta;
	struct callout		 mdio_start_co;	/* STARTING state timer. */
	int			 mdio_disks_present;
	int			 mdio_started;
	int			 mdio_incomplete;
	struct root_hold_token	*mdio_rootmount; /* Root mount delay token. */
};

static g_raid_md_create_t g_raid_md_create_intel;
static g_raid_md_taste_t g_raid_md_taste_intel;
static g_raid_md_event_t g_raid_md_event_intel;
static g_raid_md_ctl_t g_raid_md_ctl_intel;
static g_raid_md_write_t g_raid_md_write_intel;
static g_raid_md_fail_disk_t g_raid_md_fail_disk_intel;
static g_raid_md_free_disk_t g_raid_md_free_disk_intel;
static g_raid_md_free_t g_raid_md_free_intel;

static kobj_method_t g_raid_md_intel_methods[] = {
	KOBJMETHOD(g_raid_md_create,	g_raid_md_create_intel),
	KOBJMETHOD(g_raid_md_taste,	g_raid_md_taste_intel),
	KOBJMETHOD(g_raid_md_event,	g_raid_md_event_intel),
	KOBJMETHOD(g_raid_md_ctl,	g_raid_md_ctl_intel),
	KOBJMETHOD(g_raid_md_write,	g_raid_md_write_intel),
	KOBJMETHOD(g_raid_md_fail_disk,	g_raid_md_fail_disk_intel),
	KOBJMETHOD(g_raid_md_free_disk,	g_raid_md_free_disk_intel),
	KOBJMETHOD(g_raid_md_free,	g_raid_md_free_intel),
	{ 0, 0 }
};

static struct g_raid_md_class g_raid_md_intel_class = {
	"Intel",
	g_raid_md_intel_methods,
	sizeof(struct g_raid_md_intel_object),
	.mdc_priority = 100
};


static struct intel_raid_map *
intel_get_map(struct intel_raid_vol *mvol, int i)
{
	struct intel_raid_map *mmap;

	if (i > (mvol->migr_state ? 1 : 0))
		return (NULL);
	mmap = &mvol->map[0];
	for (; i > 0; i--) {
		mmap = (struct intel_raid_map *)
		    &mmap->disk_idx[mmap->total_disks];
	}
	return ((struct intel_raid_map *)mmap);
}

static struct intel_raid_vol *
intel_get_volume(struct intel_raid_conf *meta, int i)
{
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap;

	if (i > 1)
		return (NULL);
	mvol = (struct intel_raid_vol *)&meta->disk[meta->total_disks];
	for (; i > 0; i--) {
		mmap = intel_get_map(mvol, mvol->migr_state ? 1 : 0);
		mvol = (struct intel_raid_vol *)
		    &mmap->disk_idx[mmap->total_disks];
	}
	return (mvol);
}

static void
g_raid_md_intel_print(struct intel_raid_conf *meta)
{
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap;
	int i, j, k;

	if (g_raid_debug < 1)
		return;

	printf("********* ATA Intel MatrixRAID Metadata *********\n");
	printf("intel_id            <%.24s>\n", meta->intel_id);
	printf("version             <%.6s>\n", meta->version);
	printf("checksum            0x%08x\n", meta->checksum);
	printf("config_size         0x%08x\n", meta->config_size);
	printf("config_id           0x%08x\n", meta->config_id);
	printf("generation          0x%08x\n", meta->generation);
	printf("attributes          0x%08x\n", meta->attributes);
	printf("total_disks         %u\n", meta->total_disks);
	printf("total_volumes       %u\n", meta->total_volumes);
	printf("DISK#   serial disk_sectors disk_id flags\n");
	for (i = 0; i < meta->total_disks; i++ ) {
		printf("    %d   <%.16s> %u 0x%08x 0x%08x\n", i,
		    meta->disk[i].serial, meta->disk[i].sectors,
		    meta->disk[i].id, meta->disk[i].flags);
	}
	for (i = 0; i < meta->total_volumes; i++) {
		mvol = intel_get_volume(meta, i);
		printf(" ****** Volume %d ******\n", i);
		printf(" name               %.16s\n", mvol->name);
		printf(" total_sectors      %ju\n", mvol->total_sectors);
		printf(" state              %u\n", mvol->state);
		printf(" reserved           %u\n", mvol->reserved);
		printf(" curr_migr_unit     %u\n", mvol->curr_migr_unit);
		printf(" checkpoint_id      %u\n", mvol->checkpoint_id);
		printf(" migr_state         %u\n", mvol->migr_state);
		printf(" migr_type          %u\n", mvol->migr_type);
		printf(" dirty              %u\n", mvol->dirty);

		for (j = 0; j < (mvol->migr_state ? 2 : 1); j++) {
			printf("  *** Map %d ***\n", j);
			mmap = intel_get_map(mvol, j);
			printf("  offset            %u\n", mmap->offset);
			printf("  disk_sectors      %u\n", mmap->disk_sectors);
			printf("  stripe_count      %u\n", mmap->stripe_count);
			printf("  strip_sectors     %u\n", mmap->strip_sectors);
			printf("  status            %u\n", mmap->status);
			printf("  type              %u\n", mmap->type);
			printf("  total_disks       %u\n", mmap->total_disks);
			printf("  total_domains     %u\n", mmap->total_domains);
			printf("  failed_disk_num   %u\n", mmap->failed_disk_num);
			printf("  ddf               %u\n", mmap->ddf);
			printf("  disk_idx         ");
			for (k = 0; k < mmap->total_disks; k++)
				printf(" 0x%08x", mmap->disk_idx[k]);
			printf("\n");
		}
	}
	printf("=================================================\n");
}

static struct intel_raid_conf *
intel_meta_copy(struct intel_raid_conf *meta)
{
	struct intel_raid_conf *nmeta;

	nmeta = malloc(meta->config_size, M_MD_INTEL, M_WAITOK);
	memcpy(nmeta, meta, meta->config_size);
	return (nmeta);
}

static int
intel_meta_find_disk(struct intel_raid_conf *meta, char *serial)
{
	int pos;

	for (pos = 0; pos < meta->total_disks; pos++) {
		if (strncmp(meta->disk[pos].serial,
		    serial, INTEL_SERIAL_LEN) == 0)
			return (pos);
	}
	return (-1);
}

static struct intel_raid_conf *
intel_meta_read(struct g_consumer *cp)
{
	struct g_provider *pp;
	struct intel_raid_conf *meta;
	char *buf;
	int error, i, left;
	uint32_t checksum, *ptr;

	pp = cp->provider;

	/* Read the anchor sector. */
	buf = g_read_data(cp,
	    pp->mediasize - pp->sectorsize * 2, pp->sectorsize, &error);
	if (buf == NULL) {
		G_RAID_DEBUG(1, "Cannot read metadata from %s (error=%d).",
		    pp->name, error);
		return (NULL);
	}
	meta = (struct intel_raid_conf *)buf;

	/* Check if this is an Intel RAID struct */
	if (strncmp(meta->intel_id, INTEL_MAGIC, strlen(INTEL_MAGIC))) {
		G_RAID_DEBUG(1, "Intel signature check failed on %s", pp->name);
		g_free(buf);
		return (NULL);
	}
	if (meta->config_size > 65536) {
		G_RAID_DEBUG(1, "Intel metadata size looks too big: %d",
		    meta->config_size);
		g_free(buf);
		return (NULL);
	}
	meta = malloc(meta->config_size, M_MD_INTEL, M_WAITOK);
	memcpy(meta, buf, pp->sectorsize);
	g_free(buf);

	/* Read all the rest, if needed. */
	if (meta->config_size > pp->sectorsize) {
		left = (meta->config_size - 1) / pp->sectorsize;
		buf = g_read_data(cp,
		    pp->mediasize - pp->sectorsize * (2 + left),
		    pp->sectorsize * left, &error);
		if (buf == NULL) {
			G_RAID_DEBUG(1, "Cannot read remaining metadata"
			    " part from %s (error=%d).",
			    pp->name, error);
			free(meta, M_MD_INTEL);
			return (NULL);
		}
		memcpy(((char *)meta) + pp->sectorsize, buf,
		    pp->sectorsize * left);
		g_free(buf);
	}

	/* Check metadata checksum. */
	for (checksum = 0, ptr = (uint32_t *)meta, i = 0;
	    i < (meta->config_size / sizeof(uint32_t)); i++) {
		checksum += *ptr++;
	}
	checksum -= meta->checksum;
	if (checksum != meta->checksum) {
		G_RAID_DEBUG(1, "Intel checksum check failed on %s", pp->name);
		free(meta, M_MD_INTEL);
		return (NULL);
	}

	return (meta);
}

static int
intel_meta_write(struct g_consumer *cp, struct intel_raid_conf *meta)
{
	struct g_provider *pp;
	char *buf;
	int error, i, sectors;
	uint32_t checksum, *ptr;

	pp = cp->provider;

	/* Recalculate checksum for case if metadata were changed. */
	meta->checksum = 0;
	for (checksum = 0, ptr = (uint32_t *)meta, i = 0;
	    i < (meta->config_size / sizeof(uint32_t)); i++) {
		checksum += *ptr++;
	}
	meta->checksum = checksum;

	/* Create and fill buffer. */
	sectors = (meta->config_size + pp->sectorsize - 1) / pp->sectorsize;
	buf = malloc(sectors * pp->sectorsize, M_MD_INTEL, M_WAITOK | M_ZERO);
	if (sectors > 1) {
		memcpy(buf, ((char *)meta) + pp->sectorsize,
		    (sectors - 1) * pp->sectorsize);
	}
	memcpy(buf + (sectors - 1) * pp->sectorsize, meta, pp->sectorsize);

	error = g_write_data(cp,
	    pp->mediasize - pp->sectorsize * (1 + sectors),
	    buf, pp->sectorsize * sectors);
	if (error != 0) {
		G_RAID_DEBUG(1, "Cannot write metadata to %s (error=%d).",
		    pp->name, error);
	}

	free(buf, M_MD_INTEL);
	return (error);
}

static int
intel_meta_erase(struct g_consumer *cp)
{
	struct g_provider *pp;
	char *buf;
	int error;

	pp = cp->provider;
	buf = malloc(pp->sectorsize, M_MD_INTEL, M_WAITOK | M_ZERO);
	error = g_write_data(cp,
	    pp->mediasize - 2 * pp->sectorsize,
	    buf, pp->sectorsize);
	if (error != 0) {
		G_RAID_DEBUG(1, "Cannot erase metadata on %s (error=%d).",
		    pp->name, error);
	}
	free(buf, M_MD_INTEL);
	return (error);
}

static int
intel_meta_write_spare(struct g_consumer *cp, struct intel_raid_disk *d)
{
	struct intel_raid_conf *meta;
	int error;

	/* Fill anchor and single disk. */
	meta = malloc(INTEL_MAX_MD_SIZE(1), M_MD_INTEL, M_WAITOK | M_ZERO);
	memcpy(&meta->intel_id[0], INTEL_MAGIC, sizeof(INTEL_MAGIC));
	memcpy(&meta->version[0], INTEL_VERSION_1000,
	    sizeof(INTEL_VERSION_1000));
	meta->config_size = INTEL_MAX_MD_SIZE(1);
	meta->config_id = arc4random();
	meta->generation = 1;
	meta->total_disks = 1;
	meta->disk[0] = *d;
	error = intel_meta_write(cp, meta);
	free(meta, M_MD_INTEL);
	return (error);
}

static struct g_raid_disk *
g_raid_md_intel_get_disk(struct g_raid_softc *sc, int id)
{
	struct g_raid_disk	*disk;
	struct g_raid_md_intel_perdisk *pd;

	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
		if (pd->pd_disk_pos == id)
			break;
	}
	return (disk);
}

static struct g_raid_volume *
g_raid_md_intel_get_volume(struct g_raid_softc *sc, int id)
{
	struct g_raid_volume	*mvol;

	TAILQ_FOREACH(mvol, &sc->sc_volumes, v_next) {
		if ((intptr_t)(mvol->v_md_data) == id)
			break;
	}
	return (mvol);
}

static int
g_raid_md_intel_start_disk(struct g_raid_disk *disk)
{
	struct g_raid_softc *sc;
	struct g_raid_subdisk *sd, *tmpsd;
	struct g_raid_disk *olddisk;
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd, *oldpd;
	struct intel_raid_conf *meta;
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap0, *mmap1;
	int disk_pos, resurrection = 0;

	sc = disk->d_softc;
	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	meta = mdi->mdio_meta;
	pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
	olddisk = NULL;

	/* Find disk position in metadata by it's serial. */
	disk_pos = intel_meta_find_disk(meta, pd->pd_disk_meta.serial);
	if (disk_pos < 0) {
		G_RAID_DEBUG(1, "Unknown, probably new or stale disk");
		/* Failed stale disk is useless for us. */
		if (pd->pd_disk_meta.flags & INTEL_F_FAILED) {
			g_raid_change_disk_state(disk, G_RAID_DISK_S_STALE_FAILED);
			return (0);
		}
		/* If we are in the start process, that's all for now. */
		if (!mdi->mdio_started)
			goto nofit;
		/* If we have already started - try to get use of the disk. */
		TAILQ_FOREACH(olddisk, &sc->sc_disks, d_next) {
			if (olddisk->d_state != G_RAID_DISK_S_OFFLINE &&
			    olddisk->d_state != G_RAID_DISK_S_FAILED)
				continue;
			/* Make sure this disk is big enough. */
			TAILQ_FOREACH(sd, &olddisk->d_subdisks, sd_next) {
				if (sd->sd_offset + sd->sd_size + 4096 >
				    (uint64_t)pd->pd_disk_meta.sectors * 512) {
					G_RAID_DEBUG(1,
					    "Disk too small (%llu < %llu)",
					    ((unsigned long long)
					    pd->pd_disk_meta.sectors) * 512,
					    (unsigned long long)
					    sd->sd_offset + sd->sd_size + 4096);
					break;
				}
			}
			if (sd != NULL)
				continue;
			break;
		}
		if (olddisk == NULL) {
nofit:
			if (pd->pd_disk_meta.flags & INTEL_F_SPARE) {
				g_raid_change_disk_state(disk,
				    G_RAID_DISK_S_SPARE);
				return (1);
			} else {
				g_raid_change_disk_state(disk,
				    G_RAID_DISK_S_STALE);
				return (0);
			}
		}
		oldpd = (struct g_raid_md_intel_perdisk *)olddisk->d_md_data;
		disk_pos = oldpd->pd_disk_pos;
		resurrection = 1;
	}

	if (olddisk == NULL) {
		/* Find placeholder by position. */
		olddisk = g_raid_md_intel_get_disk(sc, disk_pos);
		if (olddisk == NULL)
			panic("No disk at position %d!", disk_pos);
		if (olddisk->d_state != G_RAID_DISK_S_OFFLINE) {
			G_RAID_DEBUG(1, "More then one disk for pos %d",
			    disk_pos);
			g_raid_change_disk_state(disk, G_RAID_DISK_S_STALE);
			return (0);
		}
		oldpd = (struct g_raid_md_intel_perdisk *)olddisk->d_md_data;
	}

	/* Replace failed disk or placeholder with new disk. */
	TAILQ_FOREACH_SAFE(sd, &olddisk->d_subdisks, sd_next, tmpsd) {
		TAILQ_REMOVE(&olddisk->d_subdisks, sd, sd_next);
		TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
		sd->sd_disk = disk;
		oldpd->pd_disk_pos = -2;
		pd->pd_disk_pos = disk_pos;
	}

	/* If it was placeholder -- destroy it. */
	if (olddisk->d_state == G_RAID_DISK_S_OFFLINE) {
		g_raid_destroy_disk(olddisk);
	} else {
		/* Otherwise, make it STALE_FAILED. */
		g_raid_change_disk_state(olddisk, G_RAID_DISK_S_STALE_FAILED);
		/* Update global metadata just in case. */
		memcpy(&meta->disk[disk_pos], &pd->pd_disk_meta,
		    sizeof(struct intel_raid_disk));
	}

	/* Welcome the new disk. */
	if (resurrection)
		g_raid_change_disk_state(disk, G_RAID_DISK_S_ACTIVE);
	else if (meta->disk[disk_pos].flags & INTEL_F_FAILED)
		g_raid_change_disk_state(disk, G_RAID_DISK_S_FAILED);
	else if (meta->disk[disk_pos].flags & INTEL_F_SPARE)
		g_raid_change_disk_state(disk, G_RAID_DISK_S_SPARE);
	else
		g_raid_change_disk_state(disk, G_RAID_DISK_S_ACTIVE);
	TAILQ_FOREACH(sd, &disk->d_subdisks, sd_next) {
		mvol = intel_get_volume(meta,
		    (uintptr_t)(sd->sd_volume->v_md_data));
		mmap0 = intel_get_map(mvol, 0);
		if (mvol->migr_state)
			mmap1 = intel_get_map(mvol, 1);
		else
			mmap1 = mmap0;

		if (resurrection) {
			g_raid_change_subdisk_state(sd,
			    G_RAID_SUBDISK_S_NEW);
		} else if (meta->disk[disk_pos].flags & INTEL_F_FAILED) {
			g_raid_change_subdisk_state(sd,
			    G_RAID_SUBDISK_S_FAILED);
		} else if (mvol->migr_state == 0) {
			if (mmap0->disk_idx[sd->sd_pos] & INTEL_DI_RBLD) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_NEW);
			} else {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
			}
		} else if (mvol->migr_type == INTEL_MT_INIT ||
			   mvol->migr_type == INTEL_MT_REBUILD) {
			if (!(mmap1->disk_idx[sd->sd_pos] & INTEL_DI_RBLD)) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
			} else if (mmap0->disk_idx[sd->sd_pos] & INTEL_DI_RBLD) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_NEW);
			} else {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_REBUILD);
			}
		} else if (mvol->migr_type == INTEL_MT_VERIFY ||
			   mvol->migr_type == INTEL_MT_REPAIR) {
			if (!(mmap1->disk_idx[sd->sd_pos] & INTEL_DI_RBLD)) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
			} else if (mmap0->disk_idx[sd->sd_pos] & INTEL_DI_RBLD) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_STALE);
			} else {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_RESYNC);
			}
		}
		g_raid_event_send(sd, G_RAID_SUBDISK_E_NEW,
		    G_RAID_EVENT_SUBDISK);
	}

	/* Update status of our need for spare. */
	if (mdi->mdio_started) {
		mdi->mdio_incomplete =
		    (g_raid_ndisks(sc, G_RAID_DISK_S_ACTIVE) <
		     meta->total_disks);
	}

	return (resurrection);
}

static void
g_disk_md_intel_retaste(void *arg, int pending)
{

	G_RAID_DEBUG(1, "Array is not complete, trying to retaste.");
	g_retaste(&g_raid_class);
	free(arg, M_MD_INTEL);
}

static void
g_raid_md_intel_refill(struct g_raid_softc *sc)
{
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;
	struct intel_raid_conf *meta;
	struct g_raid_disk *disk;
	struct task *task;
	int update;

	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	meta = mdi->mdio_meta;
	update = 0;
	do {
		/* Make sure we miss anything. */
		if (g_raid_ndisks(sc, G_RAID_DISK_S_ACTIVE) ==
		    meta->total_disks)
			break;

		G_RAID_DEBUG(1, "Array is not complete, trying to refill.");

		/* Try to get use some of STALE disks. */
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_state == G_RAID_DISK_S_STALE) {
				update += g_raid_md_intel_start_disk(disk);
				if (disk->d_state == G_RAID_DISK_S_ACTIVE)
					break;
			}
		}
		if (disk != NULL)
			continue;

		/* Try to get use some of SPARE disks. */
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_state == G_RAID_DISK_S_SPARE) {
				update += g_raid_md_intel_start_disk(disk);
				if (disk->d_state == G_RAID_DISK_S_ACTIVE)
					break;
			}
		}
	} while (disk != NULL);

	/* Write new metadata if we changed something. */
	if (update)
		g_raid_md_write_intel(md, NULL, NULL, NULL);

	/* Update status of our need for spare. */
	mdi->mdio_incomplete = (g_raid_ndisks(sc, G_RAID_DISK_S_ACTIVE) <
	    meta->total_disks);

	/* Request retaste hoping to find spare. */
	if (mdi->mdio_incomplete) {
		task = malloc(sizeof(struct task),
		    M_MD_INTEL, M_WAITOK | M_ZERO);
		TASK_INIT(task, 0, g_disk_md_intel_retaste, task);
		taskqueue_enqueue(taskqueue_swi, task);
	}
}

static void
g_raid_md_intel_start(struct g_raid_softc *sc)
{
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd;
	struct intel_raid_conf *meta;
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct g_raid_disk *disk;
	int i, j, disk_pos;

	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	meta = mdi->mdio_meta;

	/* Create volumes and subdisks. */
	for (i = 0; i < meta->total_volumes; i++) {
		mvol = intel_get_volume(meta, i);
		mmap = intel_get_map(mvol, 0);
		vol = g_raid_create_volume(sc, mvol->name);
		vol->v_md_data = (void *)(intptr_t)i;
		if (mmap->type == INTEL_T_RAID0)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID0;
		else if (mmap->type == INTEL_T_RAID1 &&
		    mmap->total_disks < 4) /* >= 4 disks -> RAID10 */
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID1;
		else if (mmap->type == INTEL_T_RAID1) /* SIC */
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID10;
		else if (mmap->type == INTEL_T_RAID5)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID5;
		else
			vol->v_raid_level = G_RAID_VOLUME_RL_UNKNOWN;
		vol->v_raid_level_qualifier = G_RAID_VOLUME_RLQ_NONE;
		vol->v_strip_size = (u_int)mmap->strip_sectors * 512; //ZZZ
		vol->v_disks_count = mmap->total_disks;
		vol->v_mediasize = (off_t)mvol->total_sectors * 512; //ZZZ
		vol->v_sectorsize = 512; //ZZZ
		for (j = 0; j < vol->v_disks_count; j++) {
			sd = &vol->v_subdisks[j];
			sd->sd_offset = (off_t)mmap->offset * 512; //ZZZ
			sd->sd_size = (off_t)mmap->disk_sectors * 512; //ZZZ
		}
		g_raid_start_volume(vol);
	}

	/* Create disk placeholders to store data for later writing. */
	for (disk_pos = 0; disk_pos < meta->total_disks; disk_pos++) {
		pd = malloc(sizeof(*pd), M_MD_INTEL, M_WAITOK | M_ZERO);
		pd->pd_disk_pos = disk_pos;
		pd->pd_disk_meta = meta->disk[disk_pos];
		disk = g_raid_create_disk(sc);
		disk->d_md_data = (void *)pd;
		g_raid_change_disk_state(disk, G_RAID_DISK_S_OFFLINE);
		for (i = 0; i < meta->total_volumes; i++) {
			mvol = intel_get_volume(meta, i);
			mmap = intel_get_map(mvol, 0);
			for (j = 0; j < mmap->total_disks; j++) {
				if ((mmap->disk_idx[j] & INTEL_DI_IDX) == disk_pos)
					break;
			}
			if (j == mmap->total_disks)
				continue;
			vol = g_raid_md_intel_get_volume(sc, i);
			sd = &vol->v_subdisks[j];
			sd->sd_disk = disk;
			TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
		}
	}

	/* Make all disks found till the moment take their places. */
	do {
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_state == G_RAID_DISK_S_NONE) {
				g_raid_md_intel_start_disk(disk);
				break;
			}
		}
	} while (disk != NULL);

	mdi->mdio_started = 1;

	/* Pickup any STALE/SPARE disks to refill array if needed. */
	g_raid_md_intel_refill(sc);

	callout_stop(&mdi->mdio_start_co);
	G_RAID_DEBUG(1, "root_mount_rel %p", mdi->mdio_rootmount);
	root_mount_rel(mdi->mdio_rootmount);
	mdi->mdio_rootmount = NULL;
}

static void
g_raid_md_intel_new_disk(struct g_raid_disk *disk)
{
	struct g_raid_softc *sc;
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;
	struct intel_raid_conf *pdmeta;
	struct g_raid_md_intel_perdisk *pd;

	sc = disk->d_softc;
	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
	pdmeta = pd->pd_meta;

	if (mdi->mdio_started) {
		if (g_raid_md_intel_start_disk(disk))
			g_raid_md_write_intel(md, NULL, NULL, NULL);
	} else {
		/* If we haven't started yet - check metadata freshness. */
		if (mdi->mdio_meta == NULL ||
		    pdmeta->generation > mdi->mdio_generation) {
			G_RAID_DEBUG(1, "Newer disk");
			if (mdi->mdio_meta != NULL)
				free(mdi->mdio_meta, M_MD_INTEL);
			mdi->mdio_meta = intel_meta_copy(pdmeta);
			mdi->mdio_generation = mdi->mdio_meta->generation;
			mdi->mdio_disks_present = 1;
		} else if (pdmeta->generation == mdi->mdio_generation) {
			mdi->mdio_disks_present++;
			G_RAID_DEBUG(1, "Matching disk (%d up)",
			    mdi->mdio_disks_present);
		} else {
			G_RAID_DEBUG(1, "Older disk");
		}
		/* If we collected all needed disks - start array. */
		if (mdi->mdio_disks_present == mdi->mdio_meta->total_disks)
			g_raid_md_intel_start(sc);
	}
}

static void
g_raid_intel_go(void *arg)
{
	struct g_raid_softc *sc;
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;

	sc = arg;
	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	sx_xlock(&sc->sc_lock);
	if (!mdi->mdio_started) {
		G_RAID_DEBUG(0, "Force node %s start due to timeout.", sc->sc_name);
		g_raid_md_intel_start(sc);
	}
	sx_xunlock(&sc->sc_lock);
}

static int
g_raid_md_create_intel(struct g_raid_md_object *md, struct g_class *mp,
    struct g_geom **gp)
{
	struct g_raid_softc *sc;
	struct g_raid_md_intel_object *mdi;
	char name[16];

	mdi = (struct g_raid_md_intel_object *)md;
	mdi->mdio_config_id = arc4random();
	mdi->mdio_generation = 0;
	snprintf(name, sizeof(name), "Intel-%08x", mdi->mdio_config_id);
	sc = g_raid_create_node(mp, name, md);
	if (sc == NULL)
		return (G_RAID_MD_TASTE_FAIL);
	md->mdo_softc = sc;
	*gp = sc->sc_geom;
	G_RAID_DEBUG(1, "Created new node %s", sc->sc_name);
	return (G_RAID_MD_TASTE_NEW);
}

/*
 * Return the last N characters of the serial label.  The Linux and
 * ataraid(7) code always uses the last 16 characters of the label to
 * store into the Intel meta format.  Generalize this to N characters
 * since that's easy.  Labels can be up to 20 characters for SATA drives
 * and up 251 characters for SAS drives.  Since intel controllers don't
 * support SAS drives, just stick with the SATA limits for stack friendliness.
 */
static int
g_raid_md_get_label(struct g_consumer *cp, char *serial, int serlen)
{
	char serial_buffer[24];
	int len, error;
	
	len = sizeof(serial_buffer);
	error = g_io_getattr("GEOM::ident", cp, &len, serial_buffer);
	if (error != 0)
		return (error);
	len = strlen(serial_buffer);
	if (len > serlen)
		len -= serlen;
	else
		len = 0;
	strncpy(serial, serial_buffer + len, serlen);
	return (0);
}

static int
g_raid_md_taste_intel(struct g_raid_md_object *md, struct g_class *mp,
                              struct g_consumer *cp, struct g_geom **gp)
{
	struct g_consumer *rcp;
	struct g_provider *pp;
	struct g_raid_md_intel_object *mdi, *mdi1;
	struct g_raid_softc *sc;
	struct g_raid_disk *disk;
	struct intel_raid_conf *meta;
	struct g_raid_md_intel_perdisk *pd;
	struct g_geom *geom;
	int error, disk_pos, result, spare, len;
	char serial[INTEL_SERIAL_LEN];
	char name[16];
	uint16_t vendor;

	G_RAID_DEBUG(1, "Tasting Intel on %s", cp->provider->name);
	mdi = (struct g_raid_md_intel_object *)md;
	pp = cp->provider;

	/* Read metadata from device. */
	meta = NULL;
	spare = 0;
	vendor = 0xffff;
	disk_pos = 0;
	if (g_access(cp, 1, 0, 0) != 0)
		return (G_RAID_MD_TASTE_FAIL);
	g_topology_unlock();
	error = g_raid_md_get_label(cp, serial, sizeof(serial));
	if (error != 0) {
		G_RAID_DEBUG(1, "Cannot get serial number from %s (error=%d).",
		    pp->name, error);
		goto fail2;
	}
	len = 2;
	if (pp->geom->rank == 1)
		g_io_getattr("GEOM::hba_vendor", cp, &len, &vendor);
	meta = intel_meta_read(cp);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (meta == NULL) {
		if (g_raid_aggressive_spare) {
			if (vendor == 0x8086) {
				G_RAID_DEBUG(1,
				    "No Intel metadata, forcing spare.");
				spare = 2;
				goto search;
			} else {
				G_RAID_DEBUG(1,
				    "Intel vendor mismatch 0x%04x != 0x8086",
				    vendor);
			}
		}
		return (G_RAID_MD_TASTE_FAIL);
	}

	/* Check this disk position in obtained metadata. */
	disk_pos = intel_meta_find_disk(meta, serial);
	if (disk_pos < 0) {
		G_RAID_DEBUG(1, "Intel serial '%s' not found", serial);
		goto fail1;
	}
	if (meta->disk[disk_pos].sectors !=
	    (pp->mediasize / pp->sectorsize)) {
		G_RAID_DEBUG(1, "Intel size mismatch %u != %u",
		    meta->disk[disk_pos].sectors,
		    (u_int)(pp->mediasize / pp->sectorsize));
		goto fail1;
	}

	/* Metadata valid. Print it. */
	g_raid_md_intel_print(meta);
	G_RAID_DEBUG(1, "Intel disk position %d", disk_pos);
	spare = meta->disk[disk_pos].flags & INTEL_F_SPARE;

search:
	/* Search for matching node. */
	sc = NULL;
	mdi1 = NULL;
	LIST_FOREACH(geom, &mp->geom, geom) {
		sc = geom->softc;
		if (sc == NULL)
			continue;
		if (sc->sc_stopping != 0)
			continue;
		if (sc->sc_md->mdo_class != md->mdo_class)
			continue;
		mdi1 = (struct g_raid_md_intel_object *)sc->sc_md;
		if (spare) {
			if (mdi1->mdio_incomplete)
				break;
		} else {
			if (mdi1->mdio_config_id == meta->config_id)
				break;
		}
	}

	/* Found matching node. */
	if (geom != NULL) {
		G_RAID_DEBUG(1, "Found matching node %s", sc->sc_name);
		result = G_RAID_MD_TASTE_EXISTING;

	} else if (spare) { /* Not found needy node -- left for later. */
		G_RAID_DEBUG(1, "Spare is not needed at this time");
		goto fail1;

	} else { /* Not found matching node -- create one. */
		result = G_RAID_MD_TASTE_NEW;
		mdi->mdio_config_id = meta->config_id;
		snprintf(name, sizeof(name), "Intel-%08x", meta->config_id);
		sc = g_raid_create_node(mp, name, md);
		md->mdo_softc = sc;
		geom = sc->sc_geom;
		G_RAID_DEBUG(1, "Created new node %s", sc->sc_name);
		callout_init(&mdi->mdio_start_co, 1);
		callout_reset(&mdi->mdio_start_co, g_raid_start_timeout * hz,
		    g_raid_intel_go, sc);
		mdi->mdio_rootmount = root_mount_hold("GRAID-Intel");
		G_RAID_DEBUG(1, "root_mount_hold %p", mdi->mdio_rootmount);
	}

	rcp = g_new_consumer(geom);
	g_attach(rcp, pp);
	if (g_access(rcp, 1, 1, 1) != 0)
		; //goto fail1;

	g_topology_unlock();
	sx_xlock(&sc->sc_lock);

	pd = malloc(sizeof(*pd), M_MD_INTEL, M_WAITOK | M_ZERO);
	pd->pd_meta = meta;
	pd->pd_disk_pos = -1;
	if (spare == 2) {
		memcpy(&pd->pd_disk_meta.serial[0], serial, INTEL_SERIAL_LEN);
		pd->pd_disk_meta.sectors = pp->mediasize / pp->sectorsize;
		pd->pd_disk_meta.id = 0;
		pd->pd_disk_meta.flags = INTEL_F_SPARE;
	} else {
		pd->pd_disk_meta = meta->disk[disk_pos];
	}
	disk = g_raid_create_disk(sc);
	disk->d_md_data = (void *)pd;
	disk->d_consumer = rcp;
	rcp->private = disk;

	/* Read kernel dumping information. */
	disk->d_kd.offset = 0;
	disk->d_kd.length = OFF_MAX;
	len = sizeof(disk->d_kd);
	error = g_io_getattr("GEOM::kerneldump", rcp, &len, &disk->d_kd);
	if (disk->d_kd.di.dumper == NULL)
		G_RAID_DEBUG(2, "Dumping not supported: %d.", error);

	g_raid_md_intel_new_disk(disk);

	sx_xunlock(&sc->sc_lock);
	g_topology_lock();
	*gp = geom;
	return (result);
fail2:
	g_topology_lock();
	g_access(cp, -1, 0, 0);
fail1:
	free(meta, M_MD_INTEL);
	return (G_RAID_MD_TASTE_FAIL);
}

static int
g_raid_md_event_intel(struct g_raid_md_object *md,
    struct g_raid_disk *disk, u_int event)
{
	struct g_raid_softc *sc;
	struct g_raid_subdisk *sd;
	struct g_raid_md_intel_perdisk *pd;

	sc = md->mdo_softc;
	pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
	switch (event) {
	case G_RAID_DISK_E_DISCONNECTED:
		/* If disk was assigned, just update statuses. */
		if (pd->pd_disk_pos >= 0) {
			g_raid_change_disk_state(disk, G_RAID_DISK_S_OFFLINE);
			if (disk->d_consumer) {
				g_topology_lock();
				g_raid_kill_consumer(sc, disk->d_consumer);
				g_topology_unlock();
				disk->d_consumer = NULL;
			}
			TAILQ_FOREACH(sd, &disk->d_subdisks, sd_next) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_NONE);
				g_raid_event_send(sd, G_RAID_SUBDISK_E_DISCONNECTED,
				    G_RAID_EVENT_SUBDISK);
			}
		} else {
			/* Otherwise -- delete. */
			g_raid_change_disk_state(disk, G_RAID_DISK_S_NONE);
			g_raid_destroy_disk(disk);
		}

		/* Write updated metadata to all disks. */
		g_raid_md_write_intel(md, NULL, NULL, NULL);

		/* Pickup any STALE/SPARE disks to refill array if needed. */
		g_raid_md_intel_refill(sc);

		/* Check if anything left except placeholders. */
		if (g_raid_ndisks(sc, -1) ==
		    g_raid_ndisks(sc, G_RAID_DISK_S_OFFLINE))
			g_raid_destroy_node(sc, 0);
		break;
	}
	return (0);
}

static int
g_raid_md_ctl_intel(struct g_raid_md_object *md,
    struct gctl_req *req)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct g_raid_disk *disk;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd;
	struct g_consumer *cp;
	struct g_provider *pp;
	char arg[16], serial[INTEL_SERIAL_LEN];
	const char *verb, *volname, *levelname, *diskname;
	int *nargs;
	uint64_t size, sectorsize, strip;
	intmax_t *sizearg, *striparg;
	int numdisks, i, len, level, qual, update;
	int error;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_intel_object *)md;
	verb = gctl_get_param(req, "verb", NULL);
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	error = 0;
	if (strcmp(verb, "label") == 0) {

		if (*nargs < 4) {
			gctl_error(req, "Invalid number of arguments.");
			return (-1);
		}
		volname = gctl_get_asciiparam(req, "arg1");
		if (volname == NULL) {
			gctl_error(req, "No volume name.");
			return (-2);
		}
		levelname = gctl_get_asciiparam(req, "arg2");
		if (levelname == NULL) {
			gctl_error(req, "No RAID level.");
			return (-3);
		}
		if (g_raid_volume_str2level(levelname, &level, &qual)) {
			gctl_error(req, "Unknown RAID level '%s'.", levelname);
			return (-4);
		}
		if (level != G_RAID_VOLUME_RL_RAID0 &&
		    level != G_RAID_VOLUME_RL_RAID1 &&
		    level != G_RAID_VOLUME_RL_RAID5 &&
		    level != G_RAID_VOLUME_RL_RAID10) {
			gctl_error(req, "Unsupported RAID level.");
			return (-5);
		}

		/* Search for disks, connect them and probe. */
		numdisks = *nargs - 3;
		size = 0xffffffffffffffffllu;
		sectorsize = 0;
		for (i = 0; i < numdisks; i++) {
			snprintf(arg, sizeof(arg), "arg%d", i + 3);
			diskname = gctl_get_asciiparam(req, arg);
			if (diskname == NULL) {
				gctl_error(req, "No disk name (%s).", arg);
				error = -6;
				break;
			}
			if (strncmp(diskname, "/dev/", 5) == 0)
				diskname += 5;
			g_topology_lock();
			pp = g_provider_by_name(diskname);
			if (pp == NULL) {
				gctl_error(req, "Provider '%s' not found.",
				    diskname);
				g_topology_unlock();
				error = -7;
				break;
			}
			cp = g_new_consumer(sc->sc_geom);
			if (g_attach(cp, pp) != 0) {
				gctl_error(req, "Can't attach provider '%s'.",
				    diskname);
				g_destroy_consumer(cp);
				g_topology_unlock();
				error = -7;
				break;
			}
			if (g_access(cp, 1, 1, 1) != 0) {
				gctl_error(req, "Can't open provider '%s'.",
				    diskname);
				g_detach(cp);
				g_destroy_consumer(cp);
				g_topology_unlock();
				error = -7;
				break;
			}

			pd = malloc(sizeof(*pd), M_MD_INTEL, M_WAITOK | M_ZERO);
			pd->pd_disk_pos = i;
			disk = g_raid_create_disk(sc);
			disk->d_md_data = (void *)pd;
			disk->d_consumer = cp;
			cp->private = disk;

			g_topology_unlock();

			error = g_raid_md_get_label(cp,
			    &pd->pd_disk_meta.serial[0], INTEL_SERIAL_LEN);
			if (error != 0) {
				gctl_error(req,
				    "Can't get serial for provider '%s'.",
				    diskname);
				error = -8;
				break;
			}

			/* Read kernel dumping information. */
			disk->d_kd.offset = 0;
			disk->d_kd.length = OFF_MAX;
			len = sizeof(disk->d_kd);
			g_io_getattr("GEOM::kerneldump", cp, &len, &disk->d_kd);
			if (disk->d_kd.di.dumper == NULL)
				G_RAID_DEBUG(2, "Dumping not supported.");

			pd->pd_disk_meta.sectors = pp->mediasize / pp->sectorsize;
			if (size > pp->mediasize)
				size = pp->mediasize;
			if (sectorsize < pp->sectorsize)
				sectorsize = pp->sectorsize;
			pd->pd_disk_meta.id = 0;
			pd->pd_disk_meta.flags = INTEL_F_ASSIGNED | INTEL_F_ONLINE;
		}
		if (error != 0)
			return (error);

		/* Reserve some space for metadata. */
		size -= ((4096 + sectorsize - 1) / sectorsize) * sectorsize;

		/* Handle size argument. */
		len = sizeof(*sizearg);
		sizearg = gctl_get_param(req, "size", &len);
		if (sizearg != NULL && len == sizeof(*sizearg)) {
			if (*sizearg > size) {
				gctl_error(req, "Size too big.");
				return (-9);
			}
			size = *sizearg;
		}

		/* Handle strip argument. */
		strip = 131072;
		len = sizeof(*striparg);
		striparg = gctl_get_param(req, "strip", &len);
		if (striparg != NULL && len == sizeof(*striparg)) {
			if (*striparg < sectorsize) {
				gctl_error(req, "Strip size too small.");
				return (-10);
			}
			if (*striparg % sectorsize != 0) {
				gctl_error(req, "Incorrect strip size.");
				return (-11);
			}
			if (strip > 65535 * sectorsize) {
				gctl_error(req, "Strip size too big.");
				return (-12);
			}
			strip = *striparg;
		}
		size -= (size % strip);

		/* We have all we need, create things: volume, ... */
		mdi->mdio_started = 1;
		vol = g_raid_create_volume(sc, volname);
		vol->v_md_data = (void *)(intptr_t)0;
		vol->v_raid_level = level;
		vol->v_raid_level_qualifier = G_RAID_VOLUME_RLQ_NONE;
		vol->v_strip_size = strip;
		vol->v_disks_count = numdisks;
		if (level == G_RAID_VOLUME_RL_RAID0)
			vol->v_mediasize = size * numdisks;
		else if (level == G_RAID_VOLUME_RL_RAID5)
			vol->v_mediasize = size * (numdisks - 1);
		else
			vol->v_mediasize = size * (numdisks / 2);
		vol->v_sectorsize = sectorsize;
		g_raid_start_volume(vol);

		/* , and subdisks. */
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
			sd = &vol->v_subdisks[pd->pd_disk_pos];
			sd->sd_disk = disk;
			sd->sd_offset = 0;
			sd->sd_size = size;
			TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
			g_raid_change_disk_state(disk, G_RAID_DISK_S_ACTIVE);
			g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_ACTIVE);
			g_raid_event_send(sd, G_RAID_SUBDISK_E_NEW,
			    G_RAID_EVENT_SUBDISK);
		}

		/* Write metadata based on created entities. */
		g_raid_md_write_intel(md, NULL, NULL, NULL);
		return (0);
	}
	if (strcmp(verb, "remove") == 0 ||
	    strcmp(verb, "fail") == 0) {
		if (*nargs < 2) {
			gctl_error(req, "Invalid number of arguments.");
			return (-1);
		}
		for (i = 1; i < *nargs; i++) {
			snprintf(arg, sizeof(arg), "arg%d", i);
			diskname = gctl_get_asciiparam(req, arg);
			if (diskname == NULL) {
				gctl_error(req, "No disk name (%s).", arg);
				error = -2;
				break;
			}
			if (strncmp(diskname, "/dev/", 5) == 0)
				diskname += 5;

			TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
				if (disk->d_consumer != NULL && 
				    disk->d_consumer->provider != NULL &&
				    strcmp(disk->d_consumer->provider->name,
				     diskname) == 0)
					break;
			}
			if (disk == NULL) {
				gctl_error(req, "Disk '%s' not found.",
				    diskname);
				error = -3;
				break;
			}

			if (strcmp(verb, "fail") == 0) {
				g_raid_md_fail_disk_intel(md, NULL, disk);
				continue;
			}

			pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;

			/* Erase metadata on deleting disk. */
			intel_meta_erase(disk->d_consumer);

			/* If disk was assigned, just update statuses. */
			if (pd->pd_disk_pos >= 0) {
				g_raid_change_disk_state(disk, G_RAID_DISK_S_OFFLINE);
				if (disk->d_consumer) {
					g_topology_lock();
					g_raid_kill_consumer(sc, disk->d_consumer);
					g_topology_unlock();
					disk->d_consumer = NULL;
				}
				TAILQ_FOREACH(sd, &disk->d_subdisks, sd_next) {
					g_raid_change_subdisk_state(sd,
					    G_RAID_SUBDISK_S_NONE);
					g_raid_event_send(sd, G_RAID_SUBDISK_E_DISCONNECTED,
					    G_RAID_EVENT_SUBDISK);
				}
			} else {
				/* Otherwise -- delete. */
				g_raid_change_disk_state(disk, G_RAID_DISK_S_NONE);
				g_raid_destroy_disk(disk);
			}
		}

		/* Write updated metadata to remaining disks. */
		g_raid_md_write_intel(md, NULL, NULL, NULL);

		/* Pickup any STALE/SPARE disks to refill array if needed. */
		g_raid_md_intel_refill(sc);

		/* Check if anything left except placeholders. */
		if (g_raid_ndisks(sc, -1) ==
		    g_raid_ndisks(sc, G_RAID_DISK_S_OFFLINE))
			g_raid_destroy_node(sc, 0);
		return (error);
	}
	if (strcmp(verb, "insert") == 0) {
		if (*nargs < 2) {
			gctl_error(req, "Invalid number of arguments.");
			return (-1);
		}
		update = 0;
		for (i = 1; i < *nargs; i++) {
			/* Get disk name. */
			snprintf(arg, sizeof(arg), "arg%d", i);
			diskname = gctl_get_asciiparam(req, arg);
			if (diskname == NULL) {
				gctl_error(req, "No disk name (%s).", arg);
				error = -3;
				break;
			}
			if (strncmp(diskname, "/dev/", 5) == 0)
				diskname += 5;

			/* Try to find provider with specified name. */
			g_topology_lock();
			pp = g_provider_by_name(diskname);
			if (pp == NULL) {
				gctl_error(req, "Provider '%s' not found.",
				    diskname);
				g_topology_unlock();
				error = -4;
				break;
			}
			cp = g_new_consumer(sc->sc_geom);
			if (g_attach(cp, pp) != 0) {
				gctl_error(req, "Can't attach provider '%s'.",
				    diskname);
				g_destroy_consumer(cp);
				g_topology_unlock();
				error = -5;
				break;
			}
			if (g_access(cp, 1, 1, 1) != 0) {
				gctl_error(req, "Can't open provider '%s'.",
				    diskname);
				g_detach(cp);
				g_destroy_consumer(cp);
				g_topology_unlock();
				error = -6;
				break;
			}
			g_topology_unlock();

			/* Read disk serial. */
			error = g_raid_md_get_label(cp,
			    &serial[0], INTEL_SERIAL_LEN);
			if (error != 0) {
				gctl_error(req,
				    "Can't get serial for provider '%s'.",
				    diskname);
				g_topology_lock();
				g_raid_kill_consumer(sc, cp);
				g_topology_unlock();
				error = -7;
				break;
			}

			pd = malloc(sizeof(*pd), M_MD_INTEL, M_WAITOK | M_ZERO);
			pd->pd_disk_pos = -1;

			disk = g_raid_create_disk(sc);
			disk->d_consumer = cp;
			disk->d_consumer->private = disk;
			disk->d_md_data = (void *)pd;
			cp->private = disk;

			/* Read kernel dumping information. */
			disk->d_kd.offset = 0;
			disk->d_kd.length = OFF_MAX;
			len = sizeof(disk->d_kd);
			g_io_getattr("GEOM::kerneldump", cp, &len, &disk->d_kd);
			if (disk->d_kd.di.dumper == NULL)
				G_RAID_DEBUG(2, "Dumping not supported.");

			memcpy(&pd->pd_disk_meta.serial[0], &serial[0],
			    INTEL_SERIAL_LEN);
			pd->pd_disk_meta.sectors = pp->mediasize / pp->sectorsize;
			pd->pd_disk_meta.id = 0;
			pd->pd_disk_meta.flags = INTEL_F_SPARE;

			/* Welcome the "new" disk. */
			update += g_raid_md_intel_start_disk(disk);
			if (disk->d_state == G_RAID_DISK_S_SPARE) {
				intel_meta_write_spare(cp, &pd->pd_disk_meta);
				g_raid_destroy_disk(disk);
			} else if (disk->d_state != G_RAID_DISK_S_ACTIVE) {
				gctl_error(req, "Disk '%s' doesn't fit.",
				    diskname);
				g_raid_destroy_disk(disk);
				error = -8;
				break;
			}
		}

		/* Write new metadata if we changed something. */
		if (update)
			g_raid_md_write_intel(md, NULL, NULL, NULL);
		return (error);
	}
	return (-100);
}

static int
g_raid_md_write_intel(struct g_raid_md_object *md, struct g_raid_volume *tvol,
    struct g_raid_subdisk *tsd, struct g_raid_disk *tdisk)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct g_raid_disk *disk;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd;
	struct intel_raid_conf *meta;
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap0, *mmap1;
	off_t sectorsize = 512;
	const char *version, *cv;
	int vi, sdi, numdisks, len, state;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_intel_object *)md;

	/* Bump generation. Newly written metadata may differ from previous. */
	mdi->mdio_generation++;

	/* Count number of disks. */
	numdisks = 0;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
		if (pd->pd_disk_pos < 0)
			continue;
		numdisks++;
		if (disk->d_state == G_RAID_DISK_S_ACTIVE) {
			pd->pd_disk_meta.flags =
			    INTEL_F_ONLINE | INTEL_F_ASSIGNED;
		} else if (disk->d_state == G_RAID_DISK_S_FAILED) {
			pd->pd_disk_meta.flags = INTEL_F_FAILED | INTEL_F_ASSIGNED;
		} else {
			pd->pd_disk_meta.flags = INTEL_F_ASSIGNED;
			if (pd->pd_disk_meta.id != 0xffffffff) {
				pd->pd_disk_meta.id = 0xffffffff;
				len = strlen(pd->pd_disk_meta.serial);
				len = min(len, INTEL_SERIAL_LEN - 3);
				strcpy(pd->pd_disk_meta.serial + len, ":0");
			}
		}
	}

	/* Fill anchor and disks. */
	meta = malloc(INTEL_MAX_MD_SIZE(numdisks),
	    M_MD_INTEL, M_WAITOK | M_ZERO);
	memcpy(&meta->intel_id[0], INTEL_MAGIC, sizeof(INTEL_MAGIC));
	meta->config_size = INTEL_MAX_MD_SIZE(numdisks);
	meta->config_id = mdi->mdio_config_id;
	meta->generation = mdi->mdio_generation;
	meta->attributes = INTEL_ATTR_CHECKSUM;
	meta->total_disks = numdisks;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
		if (pd->pd_disk_pos < 0)
			continue;
		meta->disk[pd->pd_disk_pos] = pd->pd_disk_meta;
	}

	/* Fill volumes and maps. */
	vi = 0;
	version = INTEL_VERSION_1000;
	TAILQ_FOREACH(vol, &sc->sc_volumes, v_next) {
		mvol = intel_get_volume(meta, vi);

		/* New metadata may have different volumes order. */
		vol->v_md_data = (void *)(intptr_t)vi;

		for (sdi = 0; sdi < vol->v_disks_count; sdi++) {
			sd = &vol->v_subdisks[sdi];
			if (sd->sd_disk != NULL)
				break;
		}
		if (sdi >= vol->v_disks_count)
			panic("No any filled subdisk in volume");
		if (vol->v_mediasize >= 0x20000000000llu)
			meta->attributes |= INTEL_ATTR_2TB;
		if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID0)
			meta->attributes |= INTEL_ATTR_RAID0;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1)
			meta->attributes |= INTEL_ATTR_RAID1;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID5)
			meta->attributes |= INTEL_ATTR_RAID5;
		else
			meta->attributes |= INTEL_ATTR_RAID10;

		if (meta->attributes & INTEL_ATTR_2TB)
			cv = INTEL_VERSION_1300;
//		else if (dev->status == DEV_CLONE_N_GO)
//			cv = INTEL_VERSION_1206;
		else if (vol->v_disks_count > 4)
			cv = INTEL_VERSION_1204;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID5)
			cv = INTEL_VERSION_1202;
		else if (vol->v_disks_count > 2)
			cv = INTEL_VERSION_1201;
		else if (vi > 0)
			cv = INTEL_VERSION_1200;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1)
			cv = INTEL_VERSION_1100;
		else
			cv = INTEL_VERSION_1000;
		if (strcmp(cv, version) > 0)
			version = cv;

		strlcpy(&mvol->name[0], vol->v_name, sizeof(mvol->name));
		mvol->total_sectors = vol->v_mediasize / sectorsize;

		/* Check for any recovery in progress. */
		state = G_RAID_SUBDISK_S_ACTIVE;
		for (sdi = 0; sdi < vol->v_disks_count; sdi++) {
			sd = &vol->v_subdisks[sdi];
			if (sd->sd_state == G_RAID_SUBDISK_S_REBUILD)
				state = G_RAID_SUBDISK_S_REBUILD;
			else if (sd->sd_state == G_RAID_SUBDISK_S_RESYNC &&
			    state != G_RAID_SUBDISK_S_REBUILD)
				state = G_RAID_SUBDISK_S_RESYNC;
		}
		if (state == G_RAID_SUBDISK_S_REBUILD) {
			mvol->migr_state = 1;
			mvol->migr_type = INTEL_MT_REBUILD;
		} else if (state == G_RAID_SUBDISK_S_RESYNC) {
			mvol->migr_state = 1;
			mvol->migr_type = INTEL_MT_REPAIR;
		} else
			mvol->migr_state = 0;

		mmap0 = intel_get_map(mvol, 0);

		/* Write map / common part of two maps. */
		mmap0->offset = sd->sd_offset / sectorsize;
		mmap0->disk_sectors = sd->sd_size / sectorsize;
		mmap0->strip_sectors = vol->v_strip_size / sectorsize;
		if (vol->v_state == G_RAID_VOLUME_S_BROKEN)
			mmap0->status = INTEL_S_FAILURE;
		else if (vol->v_state == G_RAID_VOLUME_S_DEGRADED)
			mmap0->status = INTEL_S_DEGRADED;
		else
			mmap0->status = INTEL_S_READY;
		if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID0)
			mmap0->type = INTEL_T_RAID0;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1 ||
		    vol->v_raid_level == G_RAID_VOLUME_RL_RAID10)
			mmap0->type = INTEL_T_RAID1;
		else
			mmap0->type = INTEL_T_RAID5;
		mmap0->total_disks = vol->v_disks_count;
		if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID10)
			mmap0->total_domains = vol->v_disks_count / 2;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1)
			mmap0->total_domains = vol->v_disks_count;
		else
			mmap0->total_domains = 1;
		mmap0->stripe_count = sd->sd_size / vol->v_strip_size /
		    mmap0->total_domains;
		mmap0->failed_disk_num = 0xff;
		mmap0->ddf = 1;

		/* If there are two maps - copy common and update. */
		if (mvol->migr_state) {
			mmap1 = intel_get_map(mvol, 1);
			memcpy(mmap1, mmap0, sizeof(struct intel_raid_map));
			mmap0->status = INTEL_S_READY;
		} else
			mmap1 = NULL;

		/* Write disk indexes and put rebuild flags. */
		for (sdi = 0; sdi < vol->v_disks_count; sdi++) {
			sd = &vol->v_subdisks[sdi];
			pd = (struct g_raid_md_intel_perdisk *)
			    sd->sd_disk->d_md_data;
			mmap0->disk_idx[sdi] = pd->pd_disk_pos;
			if (mvol->migr_state)
				mmap1->disk_idx[sdi] = pd->pd_disk_pos;
			if (sd->sd_state == G_RAID_SUBDISK_S_REBUILD ||
			    sd->sd_state == G_RAID_SUBDISK_S_RESYNC) {
				mmap1->disk_idx[sdi] |= INTEL_DI_RBLD;
			} else if (sd->sd_state != G_RAID_SUBDISK_S_ACTIVE) {
				mmap0->disk_idx[sdi] |= INTEL_DI_RBLD;
				if (mvol->migr_state)
					mmap1->disk_idx[sdi] |= INTEL_DI_RBLD;
			}
			if ((sd->sd_state == G_RAID_SUBDISK_S_NONE ||
			     sd->sd_state == G_RAID_SUBDISK_S_FAILED) &&
			    mmap0->failed_disk_num == 0xff) {
				mmap0->failed_disk_num = sdi;
				if (mvol->migr_state)
					mmap1->failed_disk_num = sdi;
			}
		}
		vi++;
	}
	meta->total_volumes = vi;
	if (strcmp(version, INTEL_VERSION_1300) != 0)
		meta->attributes &= INTEL_ATTR_CHECKSUM;
	memcpy(&meta->version[0], version, sizeof(INTEL_VERSION_1000));

	/* We are done. Print meta data and store them to disks. */
	g_raid_md_intel_print(meta);
	if (mdi->mdio_meta != NULL)
		free(mdi->mdio_meta, M_MD_INTEL);
	mdi->mdio_meta = meta;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
		if (disk->d_state != G_RAID_DISK_S_ACTIVE)
			continue;
		if (pd->pd_meta != NULL) {
			free(pd->pd_meta, M_MD_INTEL);
			pd->pd_meta = NULL;
		}
		pd->pd_meta = intel_meta_copy(meta);
		intel_meta_write(disk->d_consumer, meta);
	}
	return (0);
}

static int
g_raid_md_fail_disk_intel(struct g_raid_md_object *md,
    struct g_raid_subdisk *tsd, struct g_raid_disk *tdisk)
{
	struct g_raid_softc *sc;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd;
	struct g_raid_subdisk *sd;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_intel_object *)md;
	pd = (struct g_raid_md_intel_perdisk *)tdisk->d_md_data;

	/* We can't fail disk that is not a part of array now. */
	if (pd->pd_disk_pos < 0)
		return (-1);

	/*
	 * Mark disk as failed in metadata and try to write that metadata
	 * to the disk itself to prevent it's later resurrection as STALE.
	 */
	mdi->mdio_meta->disk[pd->pd_disk_pos].flags = INTEL_F_FAILED;
	pd->pd_disk_meta.flags = INTEL_F_FAILED;
	g_raid_md_intel_print(mdi->mdio_meta);
	if (tdisk->d_consumer)
		intel_meta_write(tdisk->d_consumer, mdi->mdio_meta);

	/* Change states. */
	g_raid_change_disk_state(tdisk, G_RAID_DISK_S_FAILED);
	TAILQ_FOREACH(sd, &tdisk->d_subdisks, sd_next) {
		g_raid_change_subdisk_state(sd,
		    G_RAID_SUBDISK_S_FAILED);
		g_raid_event_send(sd, G_RAID_SUBDISK_E_FAILED,
		    G_RAID_EVENT_SUBDISK);
	}

	/* Write updated metadata to remaining disks. */
	g_raid_md_write_intel(md, NULL, NULL, tdisk);

	/* Pickup any STALE/SPARE disks to refill array if needed. */
	g_raid_md_intel_refill(sc);

	/* Check if anything left except placeholders. */
	if (g_raid_ndisks(sc, -1) ==
	    g_raid_ndisks(sc, G_RAID_DISK_S_OFFLINE))
		g_raid_destroy_node(sc, 0);
	return (0);
}

static int
g_raid_md_free_disk_intel(struct g_raid_md_object *md,
    struct g_raid_disk *disk)
{
	struct g_raid_md_intel_perdisk *pd;

	pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
	if (pd->pd_meta != NULL) {
		free(pd->pd_meta, M_MD_INTEL);
		pd->pd_meta = NULL;
	}
	free(pd, M_MD_INTEL);
	disk->d_md_data = NULL;
	return (0);
}

static int
g_raid_md_free_intel(struct g_raid_md_object *md)
{
	struct g_raid_md_intel_object *mdi;

	mdi = (struct g_raid_md_intel_object *)md;
	if (!mdi->mdio_started) {
		mdi->mdio_started = 0;
		callout_stop(&mdi->mdio_start_co);
		G_RAID_DEBUG(1, "root_mount_rel %p", mdi->mdio_rootmount);
		root_mount_rel(mdi->mdio_rootmount);
		mdi->mdio_rootmount = NULL;
	}
	if (mdi->mdio_meta != NULL) {
		free(mdi->mdio_meta, M_MD_INTEL);
		mdi->mdio_meta = NULL;
	}
	return (0);
}

G_RAID_MD_DECLARE(g_raid_md_intel);
