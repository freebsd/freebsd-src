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

static MALLOC_DEFINE(M_MD_PROMISE, "md_promise_data", "GEOM_RAID Promise metadata");

#define	PROMISE_MAX_DISKS	8
#define	PROMISE_MAX_SUBDISKS	4

struct promise_raid_disk {
	uint8_t		flags;			/* Subdisk status. */
#define PROMISE_F_VALID              0x00000001
#define PROMISE_F_ONLINE             0x00000002
#define PROMISE_F_ASSIGNED           0x00000004
#define PROMISE_F_SPARE              0x00000008
#define PROMISE_F_DUPLICATE          0x00000010
#define PROMISE_F_REDIR              0x00000020
#define PROMISE_F_DOWN               0x00000040
#define PROMISE_F_READY              0x00000080

	uint8_t		number;			/* Position in a volume. */
	uint8_t		channel;		/* ATA channel number. */
	uint8_t		device;			/* ATA device number. */
	uint64_t	id __packed;		/* Subdisk ID. */
} __packed;

struct promise_raid_conf {
	char		promise_id[24];
#define PROMISE_MAGIC                "Promise Technology, Inc."
#define FREEBSD_MAGIC                "FreeBSD ATA driver RAID "

	uint32_t	dummy_0;
	uint64_t	magic_0;
#define PROMISE_MAGIC0(x)            (((uint64_t)(x.channel) << 48) | \
				((uint64_t)(x.device != 0) << 56))
	uint16_t	magic_1;
	uint32_t	magic_2;
	uint8_t		filler1[470];

	uint32_t	integrity;
#define PROMISE_I_VALID              0x00000080

	struct promise_raid_disk	disk;	/* This subdisk info. */
	uint32_t	disk_offset;		/* Subdisk offset. */
	uint32_t	disk_sectors;		/* Subdisk size */
	uint32_t	rebuild_lba;		/* Rebuild position. */
	uint16_t	generation;		/* Generation number. */
	uint8_t		status;			/* Volume status. */
#define PROMISE_S_VALID              0x01
#define PROMISE_S_ONLINE             0x02
#define PROMISE_S_INITED             0x04
#define PROMISE_S_READY              0x08
#define PROMISE_S_DEGRADED           0x10
#define PROMISE_S_MARKED             0x20
#define PROMISE_S_FUNCTIONAL         0x80

	uint8_t		type;			/* Voluem type. */
#define PROMISE_T_RAID0              0x00
#define PROMISE_T_RAID1              0x01
#define PROMISE_T_RAID3              0x02
#define PROMISE_T_RAID5              0x04
#define PROMISE_T_SPAN               0x08
#define PROMISE_T_JBOD               0x10

	uint8_t		total_disks;		/* Disks in this volume. */
	uint8_t		stripe_shift;		/* Strip size. */
	uint8_t		array_width;		/* Number of RAID0 stripes. */
	uint8_t		array_number;		/* Global volume number. */
	uint32_t	total_sectors;		/* Volume size. */
	uint16_t	cylinders;		/* Volume geometry: C. */
	uint8_t		heads;			/* Volume geometry: H. */
	uint8_t		sectors;		/* Volume geometry: S. */
	uint64_t	volume_id __packed;	/* Volume ID, */
	struct promise_raid_disk	disks[PROMISE_MAX_DISKS];
						/* Subdisks in this volume. */
	char		name[32];		/* Volume label. */

	uint32_t		filler2[338];
	uint32_t		checksum;
} __packed;

struct g_raid_md_promise_perdisk {
	int		 pd_subdisks;
	struct {
		struct promise_raid_conf	*pd_meta;
		int				 pd_disk_pos;
		struct promise_raid_disk	 pd_disk_meta;
	} pd_subdisk[PROMISE_MAX_SUBDISKS];
};

struct g_raid_md_promise_object {
	struct g_raid_md_object	 mdio_base;
	uint32_t		 mdio_generation;
	struct promise_raid_conf	*mdio_meta;
	struct callout		 mdio_start_co;	/* STARTING state timer. */
	int			 mdio_disks_present;
	int			 mdio_started;
	int			 mdio_incomplete;
	struct root_hold_token	*mdio_rootmount; /* Root mount delay token. */
};

static g_raid_md_create_t g_raid_md_create_promise;
static g_raid_md_taste_t g_raid_md_taste_promise;
static g_raid_md_event_t g_raid_md_event_promise;
static g_raid_md_ctl_t g_raid_md_ctl_promise;
static g_raid_md_write_t g_raid_md_write_promise;
static g_raid_md_fail_disk_t g_raid_md_fail_disk_promise;
static g_raid_md_free_disk_t g_raid_md_free_disk_promise;
static g_raid_md_free_t g_raid_md_free_promise;

static kobj_method_t g_raid_md_promise_methods[] = {
	KOBJMETHOD(g_raid_md_create,	g_raid_md_create_promise),
	KOBJMETHOD(g_raid_md_taste,	g_raid_md_taste_promise),
	KOBJMETHOD(g_raid_md_event,	g_raid_md_event_promise),
	KOBJMETHOD(g_raid_md_ctl,	g_raid_md_ctl_promise),
	KOBJMETHOD(g_raid_md_write,	g_raid_md_write_promise),
	KOBJMETHOD(g_raid_md_fail_disk,	g_raid_md_fail_disk_promise),
	KOBJMETHOD(g_raid_md_free_disk,	g_raid_md_free_disk_promise),
	KOBJMETHOD(g_raid_md_free,	g_raid_md_free_promise),
	{ 0, 0 }
};

static struct g_raid_md_class g_raid_md_promise_class = {
	"Promise",
	g_raid_md_promise_methods,
	sizeof(struct g_raid_md_promise_object),
	.mdc_priority = 100
};


static void
g_raid_md_promise_print(struct promise_raid_conf *meta)
{
	int i;

	if (g_raid_debug < 1)
		return;

	printf("********* ATA Promise Metadata *********\n");
	printf("promise_id          <%.24s>\n", meta->promise_id);
	printf("disk                %02x %02x %02x %02x %016jx\n",
	    meta->disk.flags, meta->disk.number, meta->disk.channel,
	    meta->disk.device, meta->disk.id);
	printf("disk_offset         %u\n", meta->disk_offset);
	printf("disk_sectors        %u\n", meta->disk_sectors);
	printf("rebuild_lba         %u\n", meta->rebuild_lba);
	printf("generation          %u\n", meta->generation);
	printf("status              0x%02x\n", meta->status);
	printf("type                %u\n", meta->type);
	printf("total_disks         %u\n", meta->total_disks);
	printf("stripe_shift        %u\n", meta->stripe_shift);
	printf("array_width         %u\n", meta->array_width);
	printf("array_number        %u\n", meta->array_number);
	printf("total_sectors       %u\n", meta->total_sectors);
	printf("cylinders           %u\n", meta->cylinders);
	printf("heads               %u\n", meta->heads);
	printf("sectors             %u\n", meta->sectors);
	printf("volume_id           0x%016jx\n", meta->volume_id);
	printf("disks:\n");
	for (i = 0; i < PROMISE_MAX_DISKS; i++ ) {
		printf("                    %02x %02x %02x %02x %016jx\n",
		    meta->disks[i].flags, meta->disks[i].number,
		    meta->disks[i].channel, meta->disks[i].device,
		    meta->disks[i].id);
	}
	printf("name                <%.24s>\n", meta->name);
	printf("=================================================\n");
}

#if 0
static struct promise_raid_conf *
promise_meta_copy(struct promise_raid_conf *meta)
{
	struct promise_raid_conf *nmeta;

	nmeta = malloc(sizeof(*nmeta), M_MD_PROMISE, M_WAITOK);
	memcpy(nmeta, meta, sizeof(*nmeta));
	return (nmeta);
}
#endif

static int
promise_meta_find_disk(struct promise_raid_conf *meta, uint64_t id)
{
	int pos;

	for (pos = 0; pos < meta->total_disks; pos++) {
		if (meta->disks[pos].id == id)
			return (pos);
	}
	return (-1);
}

static struct promise_raid_conf *
promise_meta_read(struct g_consumer *cp)
{
	struct g_provider *pp;
	struct promise_raid_conf *meta;
	char *buf;
	int error, i;
	uint32_t checksum, *ptr;

	pp = cp->provider;

	/* Read the anchor sector. */
	buf = g_read_data(cp,
	    pp->mediasize - pp->sectorsize * 63,
	    pp->sectorsize * 4, &error);
	if (buf == NULL) {
		G_RAID_DEBUG(1, "Cannot read metadata from %s (error=%d).",
		    pp->name, error);
		return (NULL);
	}
	meta = (struct promise_raid_conf *)buf;

	/* Check if this is an Promise RAID struct */
	if (strncmp(meta->promise_id, PROMISE_MAGIC, strlen(PROMISE_MAGIC)) &&
	    strncmp(meta->promise_id, FREEBSD_MAGIC, strlen(FREEBSD_MAGIC))) {
		G_RAID_DEBUG(1, "Promise signature check failed on %s", pp->name);
		g_free(buf);
		return (NULL);
	}
	meta = malloc(sizeof(*meta), M_MD_PROMISE, M_WAITOK);
	memcpy(meta, buf, min(sizeof(*meta), pp->sectorsize * 4));
	g_free(buf);

	/* Check metadata checksum. */
	for (checksum = 0, ptr = (uint32_t *)meta, i = 0; i < 511; i++)
		checksum += *ptr++;
	if (checksum != meta->checksum) {
		G_RAID_DEBUG(1, "Promise checksum check failed on %s", pp->name);
		free(meta, M_MD_PROMISE);
		return (NULL);
	}

	if ((meta->integrity & PROMISE_I_VALID) == 0) {
		G_RAID_DEBUG(1, "Promise metadata is invalid on %s", pp->name);
		free(meta, M_MD_PROMISE);
		return (NULL);
	}

	return (meta);
}

#if 0
static int
promise_meta_write(struct g_consumer *cp, struct promise_raid_conf *meta)
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
	buf = malloc(sectors * pp->sectorsize, M_MD_PROMISE, M_WAITOK | M_ZERO);
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

	free(buf, M_MD_PROMISE);
	return (error);
}
#endif

static int
promise_meta_erase(struct g_consumer *cp)
{
	struct g_provider *pp;
	char *buf;
	int error;

	pp = cp->provider;
	buf = malloc(pp->sectorsize, M_MD_PROMISE, M_WAITOK | M_ZERO);
	error = g_write_data(cp,
	    pp->mediasize - 2 * pp->sectorsize,
	    buf, pp->sectorsize);
	if (error != 0) {
		G_RAID_DEBUG(1, "Cannot erase metadata on %s (error=%d).",
		    pp->name, error);
	}
	free(buf, M_MD_PROMISE);
	return (error);
}

#if 0
static int
promise_meta_write_spare(struct g_consumer *cp, struct promise_raid_disk *d)
{
	struct promise_raid_conf *meta;
	int error;

	/* Fill anchor and single disk. */
	meta = malloc(sizeof(*meta), M_MD_PROMISE, M_WAITOK | M_ZERO);
	memcpy(&meta->promise_id[0], PROMISE_MAGIC, sizeof(PROMISE_MAGIC));
	memcpy(&meta->version[0], PROMISE_VERSION_1000,
	    sizeof(PROMISE_VERSION_1000));
	meta->generation = 1;
	meta->total_disks = 1;
	meta->disk[0] = *d;
	error = promise_meta_write(cp, meta);
	free(meta, M_MD_PROMISE);
	return (error);
}

static struct g_raid_disk *
g_raid_md_promise_get_disk(struct g_raid_softc *sc, int id)
{
	struct g_raid_disk	*disk;
	struct g_raid_md_promise_perdisk *pd;

	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
		if (pd->pd_disk_pos == id)
			break;
	}
	return (disk);
}

static struct g_raid_volume *
g_raid_md_promise_get_volume(struct g_raid_softc *sc, int id)
{
	struct g_raid_volume	*mvol;

	TAILQ_FOREACH(mvol, &sc->sc_volumes, v_next) {
		if ((intptr_t)(mvol->v_md_data) == id)
			break;
	}
	return (mvol);
}
#endif

static int
g_raid_md_promise_supported(int level, int qual, int disks, int force)
{

	switch (level) {
	case G_RAID_VOLUME_RL_RAID0:
		if (disks < 1)
			return (0);
		if (!force && (disks < 2 || disks > 6))
			return (0);
		break;
	case G_RAID_VOLUME_RL_RAID1:
		if (disks < 1)
			return (0);
		if (!force && (disks != 2))
			return (0);
		break;
	case G_RAID_VOLUME_RL_RAID1E:
		if (disks < 2)
			return (0);
		if (!force && (disks != 4))
			return (0);
		break;
	case G_RAID_VOLUME_RL_RAID5:
		if (disks < 3)
			return (0);
		if (!force && disks > 6)
			return (0);
		break;
	default:
		return (0);
	}
	if (qual != G_RAID_VOLUME_RLQ_NONE)
		return (0);
	return (1);
}

static int
g_raid_md_promise_start_disk(struct g_raid_disk *disk)
{
#if 0
	struct g_raid_softc *sc;
	struct g_raid_subdisk *sd, *tmpsd;
	struct g_raid_disk *olddisk, *tmpdisk;
	struct g_raid_md_object *md;
	struct g_raid_md_promise_object *mdi;
	struct g_raid_md_promise_perdisk *pd, *oldpd;
	struct promise_raid_conf *meta;
	struct promise_raid_vol *mvol;
	struct promise_raid_map *mmap0, *mmap1;
	int disk_pos, resurrection = 0;

	sc = disk->d_softc;
	md = sc->sc_md;
	mdi = (struct g_raid_md_promise_object *)md;
	meta = mdi->mdio_meta;
	pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
	olddisk = NULL;

	/* Find disk position in metadata by it's serial. */
	disk_pos = promise_meta_find_disk(meta, pd->pd_disk_meta.id);
	if (disk_pos < 0) {
		G_RAID_DEBUG1(1, sc, "Unknown, probably new or stale disk");
		/* Failed stale disk is useless for us. */
		if (pd->pd_disk_meta.flags & PROMISE_F_FAILED) {
			g_raid_change_disk_state(disk, G_RAID_DISK_S_STALE_FAILED);
			return (0);
		}
		/* If we are in the start process, that's all for now. */
		if (!mdi->mdio_started)
			goto nofit;
		/*
		 * If we have already started - try to get use of the disk.
		 * Try to replace OFFLINE disks first, then FAILED.
		 */
		TAILQ_FOREACH(tmpdisk, &sc->sc_disks, d_next) {
			if (tmpdisk->d_state != G_RAID_DISK_S_OFFLINE &&
			    tmpdisk->d_state != G_RAID_DISK_S_FAILED)
				continue;
			/* Make sure this disk is big enough. */
			TAILQ_FOREACH(sd, &tmpdisk->d_subdisks, sd_next) {
				if (sd->sd_offset + sd->sd_size + 4096 >
				    (off_t)pd->pd_disk_meta.sectors * 512) {
					G_RAID_DEBUG1(1, sc,
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
			if (tmpdisk->d_state == G_RAID_DISK_S_OFFLINE) {
				olddisk = tmpdisk;
				break;
			} else if (olddisk == NULL)
				olddisk = tmpdisk;
		}
		if (olddisk == NULL) {
nofit:
			if (pd->pd_disk_meta.flags & PROMISE_F_SPARE) {
				g_raid_change_disk_state(disk,
				    G_RAID_DISK_S_SPARE);
				return (1);
			} else {
				g_raid_change_disk_state(disk,
				    G_RAID_DISK_S_STALE);
				return (0);
			}
		}
		oldpd = (struct g_raid_md_promise_perdisk *)olddisk->d_md_data;
		disk_pos = oldpd->pd_disk_pos;
		resurrection = 1;
	}

	if (olddisk == NULL) {
		/* Find placeholder by position. */
		olddisk = g_raid_md_promise_get_disk(sc, disk_pos);
		if (olddisk == NULL)
			panic("No disk at position %d!", disk_pos);
		if (olddisk->d_state != G_RAID_DISK_S_OFFLINE) {
			G_RAID_DEBUG1(1, sc, "More then one disk for pos %d",
			    disk_pos);
			g_raid_change_disk_state(disk, G_RAID_DISK_S_STALE);
			return (0);
		}
		oldpd = (struct g_raid_md_promise_perdisk *)olddisk->d_md_data;
	}

	/* Replace failed disk or placeholder with new disk. */
	TAILQ_FOREACH_SAFE(sd, &olddisk->d_subdisks, sd_next, tmpsd) {
		TAILQ_REMOVE(&olddisk->d_subdisks, sd, sd_next);
		TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
		sd->sd_disk = disk;
	}
	oldpd->pd_disk_pos = -2;
	pd->pd_disk_pos = disk_pos;

	/* If it was placeholder -- destroy it. */
	if (olddisk->d_state == G_RAID_DISK_S_OFFLINE) {
		g_raid_destroy_disk(olddisk);
	} else {
		/* Otherwise, make it STALE_FAILED. */
		g_raid_change_disk_state(olddisk, G_RAID_DISK_S_STALE_FAILED);
		/* Update global metadata just in case. */
		memcpy(&meta->disk[disk_pos], &pd->pd_disk_meta,
		    sizeof(struct promise_raid_disk));
	}

	/* Welcome the new disk. */
	if (resurrection)
		g_raid_change_disk_state(disk, G_RAID_DISK_S_ACTIVE);
	else if (meta->disk[disk_pos].flags & PROMISE_F_FAILED)
		g_raid_change_disk_state(disk, G_RAID_DISK_S_FAILED);
	else if (meta->disk[disk_pos].flags & PROMISE_F_SPARE)
		g_raid_change_disk_state(disk, G_RAID_DISK_S_SPARE);
	else
		g_raid_change_disk_state(disk, G_RAID_DISK_S_ACTIVE);
	TAILQ_FOREACH(sd, &disk->d_subdisks, sd_next) {
		mvol = promise_get_volume(meta,
		    (uintptr_t)(sd->sd_volume->v_md_data));
		mmap0 = promise_get_map(mvol, 0);
		if (mvol->migr_state)
			mmap1 = promise_get_map(mvol, 1);
		else
			mmap1 = mmap0;

		if (resurrection) {
			/* Stale disk, almost same as new. */
			g_raid_change_subdisk_state(sd,
			    G_RAID_SUBDISK_S_NEW);
		} else if (meta->disk[disk_pos].flags & PROMISE_F_FAILED) {
			/* Failed disk, almost useless. */
			g_raid_change_subdisk_state(sd,
			    G_RAID_SUBDISK_S_FAILED);
		} else if (mvol->migr_state == 0) {
			if (mmap0->status == PROMISE_S_UNINITIALIZED) {
				/* Freshly created uninitialized volume. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_UNINITIALIZED);
			} else if (mmap0->disk_idx[sd->sd_pos] & PROMISE_DI_RBLD) {
				/* Freshly inserted disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_NEW);
			} else if (mvol->dirty) {
				/* Dirty volume (unclean shutdown). */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_STALE);
			} else {
				/* Up to date disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
			}
		} else if (mvol->migr_type == PROMISE_MT_INIT ||
			   mvol->migr_type == PROMISE_MT_REBUILD) {
			if (mmap0->disk_idx[sd->sd_pos] & PROMISE_DI_RBLD) {
				/* Freshly inserted disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_NEW);
			} else if (mmap1->disk_idx[sd->sd_pos] & PROMISE_DI_RBLD) {
				/* Rebuilding disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_REBUILD);
				if (mvol->dirty) {
					sd->sd_rebuild_pos = 0;
				} else {
					sd->sd_rebuild_pos =
					    (off_t)mvol->curr_migr_unit *
					    sd->sd_volume->v_strip_size *
					    mmap0->total_domains;
				}
			} else if (mvol->dirty) {
				/* Dirty volume (unclean shutdown). */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_STALE);
			} else {
				/* Up to date disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
			}
		} else if (mvol->migr_type == PROMISE_MT_VERIFY ||
			   mvol->migr_type == PROMISE_MT_REPAIR) {
			if (mmap0->disk_idx[sd->sd_pos] & PROMISE_DI_RBLD) {
				/* Freshly inserted disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_NEW);
			} else if (mmap1->disk_idx[sd->sd_pos] & PROMISE_DI_RBLD) {
				/* Resyncing disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_RESYNC);
				if (mvol->dirty) {
					sd->sd_rebuild_pos = 0;
				} else {
					sd->sd_rebuild_pos =
					    (off_t)mvol->curr_migr_unit *
					    sd->sd_volume->v_strip_size *
					    mmap0->total_domains;
				}
			} else if (mvol->dirty) {
				/* Dirty volume (unclean shutdown). */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_STALE);
			} else {
				/* Up to date disk. */
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
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
#endif
	return (0);
}

static void
g_disk_md_promise_retaste(void *arg, int pending)
{

	G_RAID_DEBUG(1, "Array is not complete, trying to retaste.");
	g_retaste(&g_raid_class);
	free(arg, M_MD_PROMISE);
}

static void
g_raid_md_promise_refill(struct g_raid_softc *sc)
{
	struct g_raid_md_object *md;
	struct g_raid_md_promise_object *mdi;
	struct promise_raid_conf *meta;
	struct g_raid_disk *disk;
	struct task *task;
	int update, na;

	md = sc->sc_md;
	mdi = (struct g_raid_md_promise_object *)md;
	meta = mdi->mdio_meta;
	update = 0;
	do {
		/* Make sure we miss anything. */
		na = g_raid_ndisks(sc, G_RAID_DISK_S_ACTIVE);
		if (na == meta->total_disks)
			break;

		G_RAID_DEBUG1(1, md->mdo_softc,
		    "Array is not complete (%d of %d), "
		    "trying to refill.", na, meta->total_disks);

		/* Try to get use some of STALE disks. */
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_state == G_RAID_DISK_S_STALE) {
				update += g_raid_md_promise_start_disk(disk);
				if (disk->d_state == G_RAID_DISK_S_ACTIVE)
					break;
			}
		}
		if (disk != NULL)
			continue;

		/* Try to get use some of SPARE disks. */
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_state == G_RAID_DISK_S_SPARE) {
				update += g_raid_md_promise_start_disk(disk);
				if (disk->d_state == G_RAID_DISK_S_ACTIVE)
					break;
			}
		}
	} while (disk != NULL);

	/* Write new metadata if we changed something. */
	if (update) {
		g_raid_md_write_promise(md, NULL, NULL, NULL);
		meta = mdi->mdio_meta;
	}

	/* Update status of our need for spare. */
	mdi->mdio_incomplete = (g_raid_ndisks(sc, G_RAID_DISK_S_ACTIVE) <
	    meta->total_disks);

	/* Request retaste hoping to find spare. */
	if (mdi->mdio_incomplete) {
		task = malloc(sizeof(struct task),
		    M_MD_PROMISE, M_WAITOK | M_ZERO);
		TASK_INIT(task, 0, g_disk_md_promise_retaste, task);
		taskqueue_enqueue(taskqueue_swi, task);
	}
}

static void
g_raid_md_promise_start(struct g_raid_softc *sc)
{
	struct g_raid_md_object *md;
	struct g_raid_md_promise_object *mdi;
#if 0
	struct g_raid_md_promise_perdisk *pd;
	struct promise_raid_conf *meta;
	struct promise_raid_vol *mvol;
	struct promise_raid_map *mmap;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct g_raid_disk *disk;
	int i, j, disk_pos;
#endif

	md = sc->sc_md;
	mdi = (struct g_raid_md_promise_object *)md;
#if 0
	meta = mdi->mdio_meta;

	/* Create volumes and subdisks. */
	for (i = 0; i < meta->total_volumes; i++) {
		mvol = promise_get_volume(meta, i);
		mmap = promise_get_map(mvol, 0);
		vol = g_raid_create_volume(sc, mvol->name);
		vol->v_md_data = (void *)(intptr_t)i;
		if (mmap->type == PROMISE_T_RAID0)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID0;
		else if (mmap->type == PROMISE_T_RAID1 &&
		    mmap->total_domains >= 2 &&
		    mmap->total_domains <= mmap->total_disks) {
			/* Assume total_domains is correct. */
			if (mmap->total_domains == mmap->total_disks)
				vol->v_raid_level = G_RAID_VOLUME_RL_RAID1;
			else
				vol->v_raid_level = G_RAID_VOLUME_RL_RAID1E;
		} else if (mmap->type == PROMISE_T_RAID1) {
			/* total_domains looks wrong. */
			if (mmap->total_disks <= 2)
				vol->v_raid_level = G_RAID_VOLUME_RL_RAID1;
			else
				vol->v_raid_level = G_RAID_VOLUME_RL_RAID1E;
		} else if (mmap->type == PROMISE_T_RAID5)
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
		pd = malloc(sizeof(*pd), M_MD_PROMISE, M_WAITOK | M_ZERO);
		pd->pd_disk_pos = disk_pos;
		pd->pd_disk_meta = meta->disk[disk_pos];
		disk = g_raid_create_disk(sc);
		disk->d_md_data = (void *)pd;
		disk->d_state = G_RAID_DISK_S_OFFLINE;
		for (i = 0; i < meta->total_volumes; i++) {
			mvol = promise_get_volume(meta, i);
			mmap = promise_get_map(mvol, 0);
			for (j = 0; j < mmap->total_disks; j++) {
				if ((mmap->disk_idx[j] & PROMISE_DI_IDX) == disk_pos)
					break;
			}
			if (j == mmap->total_disks)
				continue;
			vol = g_raid_md_promise_get_volume(sc, i);
			sd = &vol->v_subdisks[j];
			sd->sd_disk = disk;
			TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
		}
	}

	/* Make all disks found till the moment take their places. */
	do {
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_state == G_RAID_DISK_S_NONE) {
				g_raid_md_promise_start_disk(disk);
				break;
			}
		}
	} while (disk != NULL);

	mdi->mdio_started = 1;
	G_RAID_DEBUG1(0, sc, "Array started.");
	g_raid_md_write_promise(md, NULL, NULL, NULL);

	/* Pickup any STALE/SPARE disks to refill array if needed. */
	g_raid_md_promise_refill(sc);

	TAILQ_FOREACH(vol, &sc->sc_volumes, v_next) {
		g_raid_event_send(vol, G_RAID_VOLUME_E_START,
		    G_RAID_EVENT_VOLUME);
	}
#endif

	callout_stop(&mdi->mdio_start_co);
	G_RAID_DEBUG1(1, sc, "root_mount_rel %p", mdi->mdio_rootmount);
	root_mount_rel(mdi->mdio_rootmount);
	mdi->mdio_rootmount = NULL;
}

static void
g_raid_md_promise_new_disk(struct g_raid_disk *disk)
{
#if 0
	struct g_raid_softc *sc;
	struct g_raid_md_object *md;
	struct g_raid_md_promise_object *mdi;
	struct promise_raid_conf *pdmeta;
	struct g_raid_md_promise_perdisk *pd;

	sc = disk->d_softc;
	md = sc->sc_md;
	mdi = (struct g_raid_md_promise_object *)md;
	pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
	pdmeta = pd->pd_meta;

	if (mdi->mdio_started) {
		if (g_raid_md_promise_start_disk(disk))
			g_raid_md_write_promise(md, NULL, NULL, NULL);
	} else {
		/* If we haven't started yet - check metadata freshness. */
		if (mdi->mdio_meta == NULL ||
		    ((int32_t)(pdmeta->generation - mdi->mdio_generation)) > 0) {
			G_RAID_DEBUG1(1, sc, "Newer disk");
			if (mdi->mdio_meta != NULL)
				free(mdi->mdio_meta, M_MD_PROMISE);
			mdi->mdio_meta = promise_meta_copy(pdmeta);
			mdi->mdio_generation = mdi->mdio_meta->generation;
			mdi->mdio_disks_present = 1;
		} else if (pdmeta->generation == mdi->mdio_generation) {
			mdi->mdio_disks_present++;
			G_RAID_DEBUG1(1, sc, "Matching disk (%d of %d up)",
			    mdi->mdio_disks_present,
			    mdi->mdio_meta->total_disks);
		} else {
			G_RAID_DEBUG1(1, sc, "Older disk");
		}
		/* If we collected all needed disks - start array. */
		if (mdi->mdio_disks_present == mdi->mdio_meta->total_disks)
			g_raid_md_promise_start(sc);
	}
#endif
}

static void
g_raid_promise_go(void *arg)
{
	struct g_raid_softc *sc;
	struct g_raid_md_object *md;
	struct g_raid_md_promise_object *mdi;

	sc = arg;
	md = sc->sc_md;
	mdi = (struct g_raid_md_promise_object *)md;
	sx_xlock(&sc->sc_lock);
	if (!mdi->mdio_started) {
		G_RAID_DEBUG1(0, sc, "Force array start due to timeout.");
		g_raid_event_send(sc, G_RAID_NODE_E_START, 0);
	}
	sx_xunlock(&sc->sc_lock);
}

static int
g_raid_md_create_promise(struct g_raid_md_object *md, struct g_class *mp,
    struct g_geom **gp)
{
	struct g_raid_softc *sc;
	struct g_raid_md_promise_object *mdi;
	char name[16];

	mdi = (struct g_raid_md_promise_object *)md;
	mdi->mdio_generation = 0;
	snprintf(name, sizeof(name), "Promise");
	sc = g_raid_create_node(mp, name, md);
	if (sc == NULL)
		return (G_RAID_MD_TASTE_FAIL);
	md->mdo_softc = sc;
	*gp = sc->sc_geom;
	return (G_RAID_MD_TASTE_NEW);
}

static int
g_raid_md_taste_promise(struct g_raid_md_object *md, struct g_class *mp,
                              struct g_consumer *cp, struct g_geom **gp)
{
	struct g_consumer *rcp;
	struct g_provider *pp;
	struct g_raid_md_promise_object *mdi, *mdi1;
	struct g_raid_softc *sc;
	struct g_raid_disk *disk;
	struct promise_raid_conf *meta;
	struct g_raid_md_promise_perdisk *pd;
	struct g_geom *geom;
	int error, disk_pos, result, spare, len;
	char name[16];
	uint16_t vendor;

	G_RAID_DEBUG(1, "Tasting Promise on %s", cp->provider->name);
	mdi = (struct g_raid_md_promise_object *)md;
	pp = cp->provider;

	/* Read metadata from device. */
	meta = NULL;
	spare = 0;
	vendor = 0xffff;
	disk_pos = 0;
	if (g_access(cp, 1, 0, 0) != 0)
		return (G_RAID_MD_TASTE_FAIL);
	g_topology_unlock();
	len = 2;
	if (pp->geom->rank == 1)
		g_io_getattr("GEOM::hba_vendor", cp, &len, &vendor);
	meta = promise_meta_read(cp);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (meta == NULL) {
		if (g_raid_aggressive_spare) {
			if (vendor == 0x8086) {
				G_RAID_DEBUG(1,
				    "No Promise metadata, forcing spare.");
				spare = 2;
				goto search;
			} else {
				G_RAID_DEBUG(1,
				    "Promise vendor mismatch 0x%04x != 0x8086",
				    vendor);
			}
		}
		return (G_RAID_MD_TASTE_FAIL);
	}

	/* Check this disk position in obtained metadata. */
	disk_pos = promise_meta_find_disk(meta, meta->disk.id);
	if (disk_pos < 0) {
		G_RAID_DEBUG(1, "Promise id 0x%016jx not found", meta->disk.id);
		goto fail1;
	}

	/* Metadata valid. Print it. */
	g_raid_md_promise_print(meta);
	G_RAID_DEBUG(1, "Promise disk position %d", disk_pos);
	spare = meta->disks[disk_pos].flags & PROMISE_F_SPARE;

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
		mdi1 = (struct g_raid_md_promise_object *)sc->sc_md;
		break;
	}

	/* Found matching node. */
	if (geom != NULL) {
		G_RAID_DEBUG(1, "Found matching array %s", sc->sc_name);
		result = G_RAID_MD_TASTE_EXISTING;

	} else { /* Not found matching node -- create one. */
		result = G_RAID_MD_TASTE_NEW;
		snprintf(name, sizeof(name), "Promise");
		sc = g_raid_create_node(mp, name, md);
		md->mdo_softc = sc;
		geom = sc->sc_geom;
		callout_init(&mdi->mdio_start_co, 1);
		callout_reset(&mdi->mdio_start_co, g_raid_start_timeout * hz,
		    g_raid_promise_go, sc);
		mdi->mdio_rootmount = root_mount_hold("GRAID-Promise");
		G_RAID_DEBUG1(1, sc, "root_mount_hold %p", mdi->mdio_rootmount);
	}

	rcp = g_new_consumer(geom);
	g_attach(rcp, pp);
	if (g_access(rcp, 1, 1, 1) != 0)
		; //goto fail1;

	g_topology_unlock();
	sx_xlock(&sc->sc_lock);

	pd = malloc(sizeof(*pd), M_MD_PROMISE, M_WAITOK | M_ZERO);
#if 0
	pd->pd_meta = meta;
	pd->pd_disk_pos = -1;
	if (spare == 2) {
//		pd->pd_disk_meta.sectors = pp->mediasize / pp->sectorsize;
		pd->pd_disk_meta.id = 0;
		pd->pd_disk_meta.flags = PROMISE_F_SPARE;
	} else {
		pd->pd_disk_meta = meta->disks[disk_pos];
	}
#endif
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
		G_RAID_DEBUG1(2, sc, "Dumping not supported by %s: %d.", 
		    rcp->provider->name, error);

	g_raid_md_promise_new_disk(disk);

	sx_xunlock(&sc->sc_lock);
	g_topology_lock();
	*gp = geom;
	return (result);
fail1:
	free(meta, M_MD_PROMISE);
	return (G_RAID_MD_TASTE_FAIL);
}

static int
g_raid_md_event_promise(struct g_raid_md_object *md,
    struct g_raid_disk *disk, u_int event)
{
	struct g_raid_softc *sc;
	struct g_raid_subdisk *sd;
	struct g_raid_md_promise_object *mdi;
	struct g_raid_md_promise_perdisk *pd;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_promise_object *)md;
	if (disk == NULL) {
		switch (event) {
		case G_RAID_NODE_E_START:
			if (!mdi->mdio_started)
				g_raid_md_promise_start(sc);
			return (0);
		}
		return (-1);
	}
	pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
	switch (event) {
	case G_RAID_DISK_E_DISCONNECTED:
		/* If disk was assigned, just update statuses. */
		if (pd->pd_subdisks >= 0) {
			g_raid_change_disk_state(disk, G_RAID_DISK_S_OFFLINE);
			if (disk->d_consumer) {
				g_raid_kill_consumer(sc, disk->d_consumer);
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
		g_raid_md_write_promise(md, NULL, NULL, NULL);

		/* Check if anything left except placeholders. */
		if (g_raid_ndisks(sc, -1) ==
		    g_raid_ndisks(sc, G_RAID_DISK_S_OFFLINE))
			g_raid_destroy_node(sc, 0);
		else
			g_raid_md_promise_refill(sc);
		return (0);
	}
	return (-2);
}

static int
g_raid_md_ctl_promise(struct g_raid_md_object *md,
    struct gctl_req *req)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol, *vol1;
	struct g_raid_subdisk *sd;
	struct g_raid_disk *disk;
	struct g_raid_md_promise_object *mdi;
	struct g_raid_md_promise_perdisk *pd;
	struct g_consumer *cp;
	struct g_provider *pp;
	char arg[16];
	const char *verb, *volname, *levelname, *diskname;
	char *tmp;
	int *nargs, *force;
	off_t off, size, sectorsize, strip;
	intmax_t *sizearg, *striparg;
	int numdisks, i, len, level, qual, update;
	int error;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_promise_object *)md;
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
		numdisks = *nargs - 3;
		force = gctl_get_paraml(req, "force", sizeof(*force));
		if (!g_raid_md_promise_supported(level, qual, numdisks,
		    force ? *force : 0)) {
			gctl_error(req, "Unsupported RAID level "
			    "(0x%02x/0x%02x), or number of disks (%d).",
			    level, qual, numdisks);
			return (-5);
		}

		/* Search for disks, connect them and probe. */
		size = 0x7fffffffffffffffllu;
		sectorsize = 0;
		for (i = 0; i < numdisks; i++) {
			snprintf(arg, sizeof(arg), "arg%d", i + 3);
			diskname = gctl_get_asciiparam(req, arg);
			if (diskname == NULL) {
				gctl_error(req, "No disk name (%s).", arg);
				error = -6;
				break;
			}
			if (strcmp(diskname, "NONE") == 0) {
				cp = NULL;
				pp = NULL;
			} else {
				g_topology_lock();
				cp = g_raid_open_consumer(sc, diskname);
				if (cp == NULL) {
					gctl_error(req, "Can't open disk '%s'.",
					    diskname);
					g_topology_unlock();
					error = -4;
					break;
				}
				pp = cp->provider;
			}
			pd = malloc(sizeof(*pd), M_MD_PROMISE, M_WAITOK | M_ZERO);
//			pd->pd_disk_pos = i;
			disk = g_raid_create_disk(sc);
			disk->d_md_data = (void *)pd;
			disk->d_consumer = cp;
			if (cp == NULL) {
//				pd->pd_disk_meta.id = 0xffffffff;
//				pd->pd_disk_meta.flags = PROMISE_F_ASSIGNED;
				continue;
			}
			cp->private = disk;
			g_topology_unlock();

			/* Read kernel dumping information. */
			disk->d_kd.offset = 0;
			disk->d_kd.length = OFF_MAX;
			len = sizeof(disk->d_kd);
			g_io_getattr("GEOM::kerneldump", cp, &len, &disk->d_kd);
			if (disk->d_kd.di.dumper == NULL)
				G_RAID_DEBUG1(2, sc,
				    "Dumping not supported by %s.",
				    cp->provider->name);

//			pd->pd_disk_meta.sectors = pp->mediasize / pp->sectorsize;
			if (size > pp->mediasize)
				size = pp->mediasize;
			if (sectorsize < pp->sectorsize)
				sectorsize = pp->sectorsize;
//			pd->pd_disk_meta.id = 0;
//			pd->pd_disk_meta.flags = PROMISE_F_ASSIGNED | PROMISE_F_ONLINE;
		}
		if (error != 0)
			return (error);

		/* Reserve some space for metadata. */
		size -= ((4096 + sectorsize - 1) / sectorsize) * sectorsize;

		/* Handle size argument. */
		len = sizeof(*sizearg);
		sizearg = gctl_get_param(req, "size", &len);
		if (sizearg != NULL && len == sizeof(*sizearg) &&
		    *sizearg > 0) {
			if (*sizearg > size) {
				gctl_error(req, "Size too big %lld > %lld.",
				    (long long)*sizearg, (long long)size);
				return (-9);
			}
			size = *sizearg;
		}

		/* Handle strip argument. */
		strip = 131072;
		len = sizeof(*striparg);
		striparg = gctl_get_param(req, "strip", &len);
		if (striparg != NULL && len == sizeof(*striparg) &&
		    *striparg > 0) {
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

		/* Round size down to strip or sector. */
		if (level == G_RAID_VOLUME_RL_RAID1)
			size -= (size % sectorsize);
		else if (level == G_RAID_VOLUME_RL_RAID1E &&
		    (numdisks & 1) != 0)
			size -= (size % (2 * strip));
		else
			size -= (size % strip);
		if (size <= 0) {
			gctl_error(req, "Size too small.");
			return (-13);
		}
		if (size > 0xffffffffllu * sectorsize) {
			gctl_error(req, "Size too big.");
			return (-14);
		}

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
		else if (level == G_RAID_VOLUME_RL_RAID1)
			vol->v_mediasize = size;
		else if (level == G_RAID_VOLUME_RL_RAID5)
			vol->v_mediasize = size * (numdisks - 1);
		else { /* RAID1E */
			vol->v_mediasize = ((size * numdisks) / strip / 2) *
			    strip;
		}
		vol->v_sectorsize = sectorsize;
		g_raid_start_volume(vol);

		/* , and subdisks. */
#if 0
		TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
			pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
			sd = &vol->v_subdisks[pd->pd_disk_pos];
			sd->sd_disk = disk;
			sd->sd_offset = 0;
			sd->sd_size = size;
			TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
			if (sd->sd_disk->d_consumer != NULL) {
				g_raid_change_disk_state(disk,
				    G_RAID_DISK_S_ACTIVE);
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
				g_raid_event_send(sd, G_RAID_SUBDISK_E_NEW,
				    G_RAID_EVENT_SUBDISK);
			} else {
				g_raid_change_disk_state(disk, G_RAID_DISK_S_OFFLINE);
			}
		}
#endif

		/* Write metadata based on created entities. */
		G_RAID_DEBUG1(0, sc, "Array started.");
		g_raid_md_write_promise(md, NULL, NULL, NULL);

		/* Pickup any STALE/SPARE disks to refill array if needed. */
		g_raid_md_promise_refill(sc);

		g_raid_event_send(vol, G_RAID_VOLUME_E_START,
		    G_RAID_EVENT_VOLUME);
		return (0);
	}
	if (strcmp(verb, "add") == 0) {

		if (*nargs != 3) {
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

		/* Look for existing volumes. */
		i = 0;
		vol1 = NULL;
		TAILQ_FOREACH(vol, &sc->sc_volumes, v_next) {
			vol1 = vol;
			i++;
		}
		if (i > 1) {
			gctl_error(req, "Maximum two volumes supported.");
			return (-6);
		}
		if (vol1 == NULL) {
			gctl_error(req, "At least one volume must exist.");
			return (-7);
		}

		numdisks = vol1->v_disks_count;
		force = gctl_get_paraml(req, "force", sizeof(*force));
		if (!g_raid_md_promise_supported(level, qual, numdisks,
		    force ? *force : 0)) {
			gctl_error(req, "Unsupported RAID level "
			    "(0x%02x/0x%02x), or number of disks (%d).",
			    level, qual, numdisks);
			return (-5);
		}

		/* Collect info about present disks. */
		size = 0x7fffffffffffffffllu;
		sectorsize = 512;
		for (i = 0; i < numdisks; i++) {
			disk = vol1->v_subdisks[i].sd_disk;
			pd = (struct g_raid_md_promise_perdisk *)
			    disk->d_md_data;
//			if ((off_t)pd->pd_disk_meta.sectors * 512 < size)
//				size = (off_t)pd->pd_disk_meta.sectors * 512;
			if (disk->d_consumer != NULL &&
			    disk->d_consumer->provider != NULL &&
			    disk->d_consumer->provider->sectorsize >
			     sectorsize) {
				sectorsize =
				    disk->d_consumer->provider->sectorsize;
			}
		}

		/* Reserve some space for metadata. */
		size -= ((4096 + sectorsize - 1) / sectorsize) * sectorsize;

		/* Decide insert before or after. */
		sd = &vol1->v_subdisks[0];
		if (sd->sd_offset >
		    size - (sd->sd_offset + sd->sd_size)) {
			off = 0;
			size = sd->sd_offset;
		} else {
			off = sd->sd_offset + sd->sd_size;
			size = size - (sd->sd_offset + sd->sd_size);
		}

		/* Handle strip argument. */
		strip = 131072;
		len = sizeof(*striparg);
		striparg = gctl_get_param(req, "strip", &len);
		if (striparg != NULL && len == sizeof(*striparg) &&
		    *striparg > 0) {
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

		/* Round offset up to strip. */
		if (off % strip != 0) {
			size -= strip - off % strip;
			off += strip - off % strip;
		}

		/* Handle size argument. */
		len = sizeof(*sizearg);
		sizearg = gctl_get_param(req, "size", &len);
		if (sizearg != NULL && len == sizeof(*sizearg) &&
		    *sizearg > 0) {
			if (*sizearg > size) {
				gctl_error(req, "Size too big %lld > %lld.",
				    (long long)*sizearg, (long long)size);
				return (-9);
			}
			size = *sizearg;
		}

		/* Round size down to strip or sector. */
		if (level == G_RAID_VOLUME_RL_RAID1)
			size -= (size % sectorsize);
		else
			size -= (size % strip);
		if (size <= 0) {
			gctl_error(req, "Size too small.");
			return (-13);
		}
		if (size > 0xffffffffllu * sectorsize) {
			gctl_error(req, "Size too big.");
			return (-14);
		}

		/* We have all we need, create things: volume, ... */
		vol = g_raid_create_volume(sc, volname);
		vol->v_md_data = (void *)(intptr_t)i;
		vol->v_raid_level = level;
		vol->v_raid_level_qualifier = G_RAID_VOLUME_RLQ_NONE;
		vol->v_strip_size = strip;
		vol->v_disks_count = numdisks;
		if (level == G_RAID_VOLUME_RL_RAID0)
			vol->v_mediasize = size * numdisks;
		else if (level == G_RAID_VOLUME_RL_RAID1)
			vol->v_mediasize = size;
		else if (level == G_RAID_VOLUME_RL_RAID5)
			vol->v_mediasize = size * (numdisks - 1);
		else { /* RAID1E */
			vol->v_mediasize = ((size * numdisks) / strip / 2) *
			    strip;
		}
		vol->v_sectorsize = sectorsize;
		g_raid_start_volume(vol);

		/* , and subdisks. */
		for (i = 0; i < numdisks; i++) {
			disk = vol1->v_subdisks[i].sd_disk;
			sd = &vol->v_subdisks[i];
			sd->sd_disk = disk;
			sd->sd_offset = off;
			sd->sd_size = size;
			TAILQ_INSERT_TAIL(&disk->d_subdisks, sd, sd_next);
			if (disk->d_state == G_RAID_DISK_S_ACTIVE) {
				g_raid_change_subdisk_state(sd,
				    G_RAID_SUBDISK_S_ACTIVE);
				g_raid_event_send(sd, G_RAID_SUBDISK_E_NEW,
				    G_RAID_EVENT_SUBDISK);
			}
		}

		/* Write metadata based on created entities. */
		g_raid_md_write_promise(md, NULL, NULL, NULL);

		g_raid_event_send(vol, G_RAID_VOLUME_E_START,
		    G_RAID_EVENT_VOLUME);
		return (0);
	}
	if (strcmp(verb, "delete") == 0) {

		/* Full node destruction. */
		if (*nargs == 1) {
			/* Check if some volume is still open. */
			force = gctl_get_paraml(req, "force", sizeof(*force));
			if (force != NULL && *force == 0 &&
			    g_raid_nopens(sc) != 0) {
				gctl_error(req, "Some volume is still open.");
				return (-4);
			}

			TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
				if (disk->d_consumer)
					promise_meta_erase(disk->d_consumer);
			}
			g_raid_destroy_node(sc, 0);
			return (0);
		}

		/* Destroy specified volume. If it was last - all node. */
		if (*nargs != 2) {
			gctl_error(req, "Invalid number of arguments.");
			return (-1);
		}
		volname = gctl_get_asciiparam(req, "arg1");
		if (volname == NULL) {
			gctl_error(req, "No volume name.");
			return (-2);
		}

		/* Search for volume. */
		TAILQ_FOREACH(vol, &sc->sc_volumes, v_next) {
			if (strcmp(vol->v_name, volname) == 0)
				break;
		}
		if (vol == NULL) {
			i = strtol(volname, &tmp, 10);
			if (verb != volname && tmp[0] == 0) {
				TAILQ_FOREACH(vol, &sc->sc_volumes, v_next) {
					if ((intptr_t)vol->v_md_data == i)
						break;
				}
			}
		}
		if (vol == NULL) {
			gctl_error(req, "Volume '%s' not found.", volname);
			return (-3);
		}

		/* Check if volume is still open. */
		force = gctl_get_paraml(req, "force", sizeof(*force));
		if (force != NULL && *force == 0 &&
		    vol->v_provider_open != 0) {
			gctl_error(req, "Volume is still open.");
			return (-4);
		}

		/* Destroy volume and potentially node. */
		i = 0;
		TAILQ_FOREACH(vol1, &sc->sc_volumes, v_next)
			i++;
		if (i >= 2) {
			g_raid_destroy_volume(vol);
			g_raid_md_write_promise(md, NULL, NULL, NULL);
		} else {
			TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
				if (disk->d_consumer)
					promise_meta_erase(disk->d_consumer);
			}
			g_raid_destroy_node(sc, 0);
		}
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
				g_raid_md_fail_disk_promise(md, NULL, disk);
				continue;
			}

			pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;

			/* Erase metadata on deleting disk. */
			promise_meta_erase(disk->d_consumer);

			/* If disk was assigned, just update statuses. */
			if (pd->pd_subdisks >= 0) {
				g_raid_change_disk_state(disk, G_RAID_DISK_S_OFFLINE);
				if (disk->d_consumer) {
					g_raid_kill_consumer(sc, disk->d_consumer);
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
		g_raid_md_write_promise(md, NULL, NULL, NULL);

		/* Check if anything left except placeholders. */
		if (g_raid_ndisks(sc, -1) ==
		    g_raid_ndisks(sc, G_RAID_DISK_S_OFFLINE))
			g_raid_destroy_node(sc, 0);
		else
			g_raid_md_promise_refill(sc);
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

			/* Try to find provider with specified name. */
			g_topology_lock();
			cp = g_raid_open_consumer(sc, diskname);
			if (cp == NULL) {
				gctl_error(req, "Can't open disk '%s'.",
				    diskname);
				g_topology_unlock();
				error = -4;
				break;
			}
			pp = cp->provider;
			g_topology_unlock();

			pd = malloc(sizeof(*pd), M_MD_PROMISE, M_WAITOK | M_ZERO);
//			pd->pd_disk_pos = -1;

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
				G_RAID_DEBUG1(2, sc,
				    "Dumping not supported by %s.",
				    cp->provider->name);

//			pd->pd_disk_meta.sectors = pp->mediasize / pp->sectorsize;
//			pd->pd_disk_meta.id = 0;
//			pd->pd_disk_meta.flags = PROMISE_F_SPARE;

			/* Welcome the "new" disk. */
			update += g_raid_md_promise_start_disk(disk);
			if (disk->d_state == G_RAID_DISK_S_SPARE) {
//				promise_meta_write_spare(cp, &pd->pd_disk_meta);
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
			g_raid_md_write_promise(md, NULL, NULL, NULL);
		return (error);
	}
	return (-100);
}

static int
g_raid_md_write_promise(struct g_raid_md_object *md, struct g_raid_volume *tvol,
    struct g_raid_subdisk *tsd, struct g_raid_disk *tdisk)
{
#if 0
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct g_raid_disk *disk;
	struct g_raid_md_promise_object *mdi;
	struct g_raid_md_promise_perdisk *pd;
	struct promise_raid_conf *meta;
	struct promise_raid_vol *mvol;
	struct promise_raid_map *mmap0, *mmap1;
	off_t sectorsize = 512, pos;
	const char *version, *cv;
	int vi, sdi, numdisks, len, state, stale;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_promise_object *)md;

	if (sc->sc_stopping == G_RAID_DESTROY_HARD)
		return (0);

	/* Bump generation. Newly written metadata may differ from previous. */
	mdi->mdio_generation++;

	/* Count number of disks. */
	numdisks = 0;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
		if (pd->pd_disk_pos < 0)
			continue;
		numdisks++;
		if (disk->d_state == G_RAID_DISK_S_ACTIVE) {
			pd->pd_disk_meta.flags =
			    PROMISE_F_ONLINE | PROMISE_F_ASSIGNED;
		} else if (disk->d_state == G_RAID_DISK_S_FAILED) {
			pd->pd_disk_meta.flags = PROMISE_F_FAILED | PROMISE_F_ASSIGNED;
		} else {
			pd->pd_disk_meta.flags = PROMISE_F_ASSIGNED;
			if (pd->pd_disk_meta.id != 0xffffffff) {
				pd->pd_disk_meta.id = 0xffffffff;
			}
		}
	}

	/* Fill anchor and disks. */
	meta = malloc(sizeof(*meta), M_MD_PROMISE, M_WAITOK | M_ZERO);
	memcpy(&meta->promise_id[0], PROMISE_MAGIC, sizeof(PROMISE_MAGIC));
	meta->generation = mdi->mdio_generation;
	meta->attributes = PROMISE_ATTR_CHECKSUM;
	meta->total_disks = numdisks;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
		if (pd->pd_disk_pos < 0)
			continue;
		meta->disk[pd->pd_disk_pos] = pd->pd_disk_meta;
	}

	/* Fill volumes and maps. */
	vi = 0;
	version = PROMISE_VERSION_1000;
	TAILQ_FOREACH(vol, &sc->sc_volumes, v_next) {
		if (vol->v_stopping)
			continue;
		mvol = promise_get_volume(meta, vi);

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
			meta->attributes |= PROMISE_ATTR_2TB;
		if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID0)
			meta->attributes |= PROMISE_ATTR_RAID0;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1)
			meta->attributes |= PROMISE_ATTR_RAID1;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID5)
			meta->attributes |= PROMISE_ATTR_RAID5;
		else
			meta->attributes |= PROMISE_ATTR_RAID10;

		if (meta->attributes & PROMISE_ATTR_2TB)
			cv = PROMISE_VERSION_1300;
//		else if (dev->status == DEV_CLONE_N_GO)
//			cv = PROMISE_VERSION_1206;
		else if (vol->v_disks_count > 4)
			cv = PROMISE_VERSION_1204;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID5)
			cv = PROMISE_VERSION_1202;
		else if (vol->v_disks_count > 2)
			cv = PROMISE_VERSION_1201;
		else if (vi > 0)
			cv = PROMISE_VERSION_1200;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1)
			cv = PROMISE_VERSION_1100;
		else
			cv = PROMISE_VERSION_1000;
		if (strcmp(cv, version) > 0)
			version = cv;

		strlcpy(&mvol->name[0], vol->v_name, sizeof(mvol->name));
		mvol->total_sectors = vol->v_mediasize / sectorsize;

		/* Check for any recovery in progress. */
		state = G_RAID_SUBDISK_S_ACTIVE;
		pos = 0x7fffffffffffffffllu;
		stale = 0;
		for (sdi = 0; sdi < vol->v_disks_count; sdi++) {
			sd = &vol->v_subdisks[sdi];
			if (sd->sd_state == G_RAID_SUBDISK_S_REBUILD)
				state = G_RAID_SUBDISK_S_REBUILD;
			else if (sd->sd_state == G_RAID_SUBDISK_S_RESYNC &&
			    state != G_RAID_SUBDISK_S_REBUILD)
				state = G_RAID_SUBDISK_S_RESYNC;
			else if (sd->sd_state == G_RAID_SUBDISK_S_STALE)
				stale = 1;
			if ((sd->sd_state == G_RAID_SUBDISK_S_REBUILD ||
			    sd->sd_state == G_RAID_SUBDISK_S_RESYNC) &&
			     sd->sd_rebuild_pos < pos)
			        pos = sd->sd_rebuild_pos;
		}
		if (state == G_RAID_SUBDISK_S_REBUILD) {
			mvol->migr_state = 1;
			mvol->migr_type = PROMISE_MT_REBUILD;
		} else if (state == G_RAID_SUBDISK_S_RESYNC) {
			mvol->migr_state = 1;
			/* mvol->migr_type = PROMISE_MT_REPAIR; */
			mvol->migr_type = PROMISE_MT_VERIFY;
			mvol->state |= PROMISE_ST_VERIFY_AND_FIX;
		} else
			mvol->migr_state = 0;
		mvol->dirty = (vol->v_dirty || stale);

		mmap0 = promise_get_map(mvol, 0);

		/* Write map / common part of two maps. */
		mmap0->offset = sd->sd_offset / sectorsize;
		mmap0->disk_sectors = sd->sd_size / sectorsize;
		mmap0->strip_sectors = vol->v_strip_size / sectorsize;
		if (vol->v_state == G_RAID_VOLUME_S_BROKEN)
			mmap0->status = PROMISE_S_FAILURE;
		else if (vol->v_state == G_RAID_VOLUME_S_DEGRADED)
			mmap0->status = PROMISE_S_DEGRADED;
		else
			mmap0->status = PROMISE_S_READY;
		if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID0)
			mmap0->type = PROMISE_T_RAID0;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1 ||
		    vol->v_raid_level == G_RAID_VOLUME_RL_RAID1E)
			mmap0->type = PROMISE_T_RAID1;
		else
			mmap0->type = PROMISE_T_RAID5;
		mmap0->total_disks = vol->v_disks_count;
		if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1)
			mmap0->total_domains = vol->v_disks_count;
		else if (vol->v_raid_level == G_RAID_VOLUME_RL_RAID1E)
			mmap0->total_domains = 2;
		else
			mmap0->total_domains = 1;
		mmap0->stripe_count = sd->sd_size / vol->v_strip_size /
		    mmap0->total_domains;
		mmap0->failed_disk_num = 0xff;
		mmap0->ddf = 1;

		/* If there are two maps - copy common and update. */
		if (mvol->migr_state) {
			mvol->curr_migr_unit = pos /
			    vol->v_strip_size / mmap0->total_domains;
			mmap1 = promise_get_map(mvol, 1);
			memcpy(mmap1, mmap0, sizeof(struct promise_raid_map));
			mmap0->status = PROMISE_S_READY;
		} else
			mmap1 = NULL;

		/* Write disk indexes and put rebuild flags. */
		for (sdi = 0; sdi < vol->v_disks_count; sdi++) {
			sd = &vol->v_subdisks[sdi];
			pd = (struct g_raid_md_promise_perdisk *)
			    sd->sd_disk->d_md_data;
			mmap0->disk_idx[sdi] = pd->pd_disk_pos;
			if (mvol->migr_state)
				mmap1->disk_idx[sdi] = pd->pd_disk_pos;
			if (sd->sd_state == G_RAID_SUBDISK_S_REBUILD ||
			    sd->sd_state == G_RAID_SUBDISK_S_RESYNC) {
				mmap1->disk_idx[sdi] |= PROMISE_DI_RBLD;
			} else if (sd->sd_state != G_RAID_SUBDISK_S_ACTIVE &&
			    sd->sd_state != G_RAID_SUBDISK_S_STALE) {
				mmap0->disk_idx[sdi] |= PROMISE_DI_RBLD;
				if (mvol->migr_state)
					mmap1->disk_idx[sdi] |= PROMISE_DI_RBLD;
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
	if (strcmp(version, PROMISE_VERSION_1300) != 0)
		meta->attributes &= PROMISE_ATTR_CHECKSUM;
	memcpy(&meta->version[0], version, sizeof(PROMISE_VERSION_1000));

	/* We are done. Print meta data and store them to disks. */
	g_raid_md_promise_print(meta);
	if (mdi->mdio_meta != NULL)
		free(mdi->mdio_meta, M_MD_PROMISE);
	mdi->mdio_meta = meta;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
		if (disk->d_state != G_RAID_DISK_S_ACTIVE)
			continue;
		if (pd->pd_meta != NULL) {
			free(pd->pd_meta, M_MD_PROMISE);
			pd->pd_meta = NULL;
		}
		pd->pd_meta = promise_meta_copy(meta);
		promise_meta_write(disk->d_consumer, meta);
	}
#endif
	return (0);
}

static int
g_raid_md_fail_disk_promise(struct g_raid_md_object *md,
    struct g_raid_subdisk *tsd, struct g_raid_disk *tdisk)
{
	struct g_raid_softc *sc;
	struct g_raid_md_promise_object *mdi;
	struct g_raid_md_promise_perdisk *pd;
	struct g_raid_subdisk *sd;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_promise_object *)md;
	pd = (struct g_raid_md_promise_perdisk *)tdisk->d_md_data;

	/* We can't fail disk that is not a part of array now. */
//	if (pd->pd_disk_pos < 0)
//		return (-1);

	/*
	 * Mark disk as failed in metadata and try to write that metadata
	 * to the disk itself to prevent it's later resurrection as STALE.
	 */
#if 0
	mdi->mdio_meta->disk[pd->pd_disk_pos].flags = PROMISE_F_FAILED;
	pd->pd_disk_meta.flags = PROMISE_F_FAILED;
	g_raid_md_promise_print(mdi->mdio_meta);
	if (tdisk->d_consumer)
		promise_meta_write(tdisk->d_consumer, mdi->mdio_meta);
#endif

	/* Change states. */
	g_raid_change_disk_state(tdisk, G_RAID_DISK_S_FAILED);
	TAILQ_FOREACH(sd, &tdisk->d_subdisks, sd_next) {
		g_raid_change_subdisk_state(sd,
		    G_RAID_SUBDISK_S_FAILED);
		g_raid_event_send(sd, G_RAID_SUBDISK_E_FAILED,
		    G_RAID_EVENT_SUBDISK);
	}

	/* Write updated metadata to remaining disks. */
	g_raid_md_write_promise(md, NULL, NULL, tdisk);

	/* Check if anything left except placeholders. */
	if (g_raid_ndisks(sc, -1) ==
	    g_raid_ndisks(sc, G_RAID_DISK_S_OFFLINE))
		g_raid_destroy_node(sc, 0);
	else
		g_raid_md_promise_refill(sc);
	return (0);
}

static int
g_raid_md_free_disk_promise(struct g_raid_md_object *md,
    struct g_raid_disk *disk)
{
	struct g_raid_md_promise_perdisk *pd;
	int i;

	pd = (struct g_raid_md_promise_perdisk *)disk->d_md_data;
	for (i = 0; i < pd->pd_subdisks; i++) {
		if (pd->pd_subdisk[i].pd_meta != NULL) {
			free(pd->pd_subdisk[i].pd_meta, M_MD_PROMISE);
			pd->pd_subdisk[i].pd_meta = NULL;
		}
	}
	free(pd, M_MD_PROMISE);
	disk->d_md_data = NULL;
	return (0);
}

static int
g_raid_md_free_promise(struct g_raid_md_object *md)
{
	struct g_raid_md_promise_object *mdi;

	mdi = (struct g_raid_md_promise_object *)md;
	if (!mdi->mdio_started) {
		mdi->mdio_started = 0;
		callout_stop(&mdi->mdio_start_co);
		G_RAID_DEBUG1(1, md->mdo_softc,
		    "root_mount_rel %p", mdi->mdio_rootmount);
		root_mount_rel(mdi->mdio_rootmount);
		mdi->mdio_rootmount = NULL;
	}
	if (mdi->mdio_meta != NULL) {
		free(mdi->mdio_meta, M_MD_PROMISE);
		mdi->mdio_meta = NULL;
	}
	return (0);
}

G_RAID_MD_DECLARE(g_raid_md_promise);
