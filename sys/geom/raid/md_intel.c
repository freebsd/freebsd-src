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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <geom/geom.h>
#include "geom/raid/g_raid.h"
#include "g_raid_md_if.h"

static MALLOC_DEFINE(M_MD_INTEL, "md_intel_data", "GEOM_RAID Intel metadata");

struct intel_raid_map {
	uint32_t	offset;
	uint32_t	disk_sectors;
	uint32_t	stripe_count;
	uint16_t	stripe_sectors;
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
#define INTEL_F_SPARE           0x01
#define INTEL_F_ASSIGNED        0x02
#define INTEL_F_DOWN            0x04
#define INTEL_F_ONLINE          0x08

	uint32_t	filler[5];
} __packed;

struct intel_raid_conf {
	uint8_t		intel_id[24];
#define INTEL_MAGIC             "Intel Raid ISM Cfg Sig. "

	uint8_t		version[6];
#define INTEL_VERSION_1100      "1.1.00"
#define INTEL_VERSION_1200      "1.2.00"
#define INTEL_VERSION_1201      "1.2.01"
#define INTEL_VERSION_1202      "1.2.02"

	uint8_t		dummy_0[2];
	uint32_t	checksum;
	uint32_t	config_size;
	uint32_t	config_id;
	uint32_t	generation;
	uint32_t	dummy_1[2];
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
};

struct g_raid_md_intel_object {
	struct g_raid_md_object	 mdio_base;
	uint32_t		 mdio_config_id;
	struct intel_raid_conf	*mdio_meta;
	struct callout		 mdio_start_co;	/* STARTING state timer. */
	int			 mdio_disks_present;
	int			 mdio_started;
	struct root_hold_token	*mdio_rootmount; /* Root mount delay token. */
};

static g_raid_md_create_t g_raid_md_create_intel;
static g_raid_md_taste_t g_raid_md_taste_intel;
static g_raid_md_event_t g_raid_md_event_intel;
static g_raid_md_ctl_t g_raid_md_ctl_intel;
static g_raid_md_write_t g_raid_md_write_intel;
static g_raid_md_free_disk_t g_raid_md_free_disk_intel;
static g_raid_md_free_t g_raid_md_free_intel;

static kobj_method_t g_raid_md_intel_methods[] = {
	KOBJMETHOD(g_raid_md_create,	g_raid_md_create_intel),
	KOBJMETHOD(g_raid_md_taste,	g_raid_md_taste_intel),
	KOBJMETHOD(g_raid_md_event,	g_raid_md_event_intel),
	KOBJMETHOD(g_raid_md_ctl,	g_raid_md_ctl_intel),
	KOBJMETHOD(g_raid_md_write,	g_raid_md_write_intel),
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

	printf("********* ATA Intel MatrixRAID Metadata *********\n");
	printf("intel_id            <%.24s>\n", meta->intel_id);
	printf("version             <%.6s>\n", meta->version);
	printf("checksum            0x%08x\n", meta->checksum);
	printf("config_size         0x%08x\n", meta->config_size);
	printf("config_id           0x%08x\n", meta->config_id);
	printf("generation          0x%08x\n", meta->generation);
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
			printf("  stripe_sectors    %u\n", mmap->stripe_sectors);
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

#if 0
static struct g_raid_disk *
g_raid_md_intel_get_disk(struct g_raid_softc *sc, int id)
{
	struct g_raid_disk	*disk;

	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if ((intptr_t)(disk->d_md_data) == id)
			break;
	}
	return (disk);
}
#endif

static struct g_raid_volume *
g_raid_md_intel_get_volume(struct g_raid_softc *sc, int id)
{
	struct g_raid_volume	*mvol;

	LIST_FOREACH(mvol, &sc->sc_volumes, v_next) {
		if ((intptr_t)(mvol->v_md_data) == id)
			break;
	}
	return (mvol);
}

static void
g_raid_md_intel_start_disk(struct g_raid_disk *disk)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd;
	struct intel_raid_conf *meta, *pdmeta;
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap;
	int i, j;

	sc = disk->d_softc;
	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	meta = mdi->mdio_meta;
	pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
	pdmeta = pd->pd_meta;

	if (pdmeta->generation != meta->generation) {
		g_raid_change_disk_state(disk, G_RAID_DISK_S_STALE);
		return;
	}

	/* Update disk state. */
	g_raid_change_disk_state(disk, G_RAID_DISK_S_ACTIVE);

	/* Create subdisks. */
	for (i = 0; i < meta->total_volumes; i++) {
		mvol = intel_get_volume(meta, i);
		mmap = intel_get_map(mvol, 0);
		for (j = 0; j < mmap->total_disks; j++) {
			if ((mmap->disk_idx[j] & INTEL_DI_IDX) == pd->pd_disk_pos)
				break;
		}
		if (j == mmap->total_disks)
			continue;
		vol = g_raid_md_intel_get_volume(sc, i);
		sd = &vol->v_subdisks[j];
		sd->sd_disk = disk;
		sd->sd_offset = mmap->offset * 512; //ZZZ
		sd->sd_size = mmap->disk_sectors;
		LIST_INSERT_HEAD(&disk->d_subdisks, sd, sd_next);
		g_raid_event_send(sd, G_RAID_SUBDISK_E_NEW,
		    G_RAID_EVENT_SUBDISK);
	}

}

static void
g_raid_md_intel_start(struct g_raid_softc *sc)
{
	struct g_raid_md_object *md;
	struct g_raid_md_intel_object *mdi;
	struct intel_raid_conf *meta;
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap;
	struct g_raid_volume *vol;
	struct g_raid_disk *disk;
	int i;

	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	meta = mdi->mdio_meta;

	/* Create volumes */
	for (i = 0; i < meta->total_volumes; i++) {
		mvol = intel_get_volume(meta, i);
		mmap = intel_get_map(mvol, 0);
		vol = g_raid_create_volume(sc, mvol->name);
		vol->v_md_data = (void *)(intptr_t)i;
		if (mmap->type == INTEL_T_RAID0)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID0;
		else if (mmap->type == INTEL_T_RAID1 &&
		    mmap->total_disks < 4)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID1;
		else if (mmap->type == INTEL_T_RAID1)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID10;
		else if (mmap->type == INTEL_T_RAID5)
			vol->v_raid_level = G_RAID_VOLUME_RL_RAID5;
		else
			vol->v_raid_level = G_RAID_VOLUME_RL_UNKNOWN;
		vol->v_raid_level_qualifier = G_RAID_VOLUME_RLQ_NONE;
		vol->v_strip_size = mmap->stripe_sectors * 512; //ZZZ
		vol->v_disks_count = mmap->total_disks;
		vol->v_mediasize = mvol->total_sectors * 512; //ZZZ
		vol->v_sectorsize = 512; //ZZZ
		g_raid_start_volume(vol);
	}
	LIST_FOREACH(disk, &sc->sc_disks, d_next)
		g_raid_md_intel_start_disk(disk);

	mdi->mdio_started = 1;
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
	struct intel_raid_conf *meta, *pdmeta;
	struct g_raid_md_intel_perdisk *pd;

	sc = disk->d_softc;
	md = sc->sc_md;
	mdi = (struct g_raid_md_intel_object *)md;
	pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
	pdmeta = pd->pd_meta;

	if (mdi->mdio_meta == NULL ||
	    pdmeta->generation > mdi->mdio_meta->generation) {
		if (mdi->mdio_started) {
			G_RAID_DEBUG(1, "Newer disk, but already started");
		} else {
			G_RAID_DEBUG(1, "Newer disk");
			if (mdi->mdio_meta != NULL)
				free(mdi->mdio_meta, M_MD_INTEL);
			mdi->mdio_meta = intel_meta_copy(pdmeta);
			mdi->mdio_disks_present = 1;
		}
	} else if (pdmeta->generation == mdi->mdio_meta->generation) {
		mdi->mdio_disks_present++;
		G_RAID_DEBUG(1, "Matching disk (%d up)",
		    mdi->mdio_disks_present);
	} else {
		G_RAID_DEBUG(1, "Stale disk");
	}

	meta = mdi->mdio_meta;
	if (mdi->mdio_started) {
		g_raid_md_intel_start_disk(disk);
	} else {
		if (mdi->mdio_disks_present == meta->total_disks)
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
	int error, disk_pos, result;
	char serial[INTEL_SERIAL_LEN];
	char name[16];

	G_RAID_DEBUG(1, "Tasting Intel on %s", cp->provider->name);
	mdi = (struct g_raid_md_intel_object *)md;
	pp = cp->provider;

	/* Read metadata from device. */
	meta = NULL;
	if (g_access(cp, 1, 0, 0) != 0)
		return (G_RAID_MD_TASTE_FAIL);
	g_topology_unlock();
	error = g_raid_md_get_label(cp, serial, sizeof(serial));
	if (error != 0) {
		G_RAID_DEBUG(1, "Cannot get serial number from %s (error=%d).",
		    pp->name, error);
		goto fail2;
	}
	meta = intel_meta_read(cp);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (meta == NULL)
		return (G_RAID_MD_TASTE_FAIL);

	/* Check this disk position in obtained metadata. */
	for (disk_pos = 0; disk_pos < meta->total_disks; disk_pos++) {
		if (strncmp(meta->disk[disk_pos].serial, serial, sizeof(serial))) {
			G_RAID_DEBUG(1, "Intel serial mismatch '%s' '%s'",
			    meta->disk[disk_pos].serial, serial);
			continue;
		}
		if (meta->disk[disk_pos].sectors !=
		    (pp->mediasize / pp->sectorsize)) {
			G_RAID_DEBUG(1, "Intel size mismatch '%u' '%u'",
			    meta->disk[disk_pos].sectors, (u_int)(pp->mediasize / pp->sectorsize));
			continue;
		}
		break;
	}
	if (disk_pos >= meta->total_disks) {
		G_RAID_DEBUG(1, "Intel disk params check failed on %s", pp->name);
		goto fail1;
	}

	/* Metadata valid. Print it. */
	g_raid_md_intel_print(meta);
	G_RAID_DEBUG(1, "Intel disk position %d", disk_pos);

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
		if (mdi1->mdio_config_id != meta->config_id)
			continue;
		break;
	}

	/* Found matching node. */
	if (geom != NULL) {
		G_RAID_DEBUG(1, "Found matching node %s", sc->sc_name);
		result = G_RAID_MD_TASTE_EXISTING;

	} else { /* Not found matching node. */
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
	pd->pd_disk_pos = disk_pos;
	disk = g_raid_create_disk(sc);
	disk->d_md_data = (void *)pd;
	disk->d_consumer = rcp;
	rcp->private = disk;

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

	sc = md->mdo_softc;
	switch (event) {
	case G_RAID_DISK_E_DISCONNECTED:
		g_raid_change_disk_state(disk, G_RAID_DISK_S_NONE);
		g_raid_destroy_disk(disk);
		if (g_raid_ndisks(sc, -1) == 0)
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
	struct g_raid_disk *disk;
	struct g_raid_md_intel_object *mdi;
	struct g_raid_md_intel_perdisk *pd;
	struct g_consumer *cp;
	struct g_provider *pp;
	struct intel_raid_conf *meta;
	struct intel_raid_vol *mvol;
	struct intel_raid_map *mmap;
	char arg[16];
	const char *verb, *volname, *levelname, *diskname;
	int *nargs;
	uint64_t size, sectorsize;
	int numdisks, i, level, qual;
	int error;

	sc = md->mdo_softc;
	mdi = (struct g_raid_md_intel_object *)md;
	verb = gctl_get_param(req, "verb", NULL);
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
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
		numdisks = *nargs - 3;
		meta = mdi->mdio_meta = malloc(INTEL_MAX_MD_SIZE(numdisks),
		    M_MD_INTEL, M_WAITOK | M_ZERO);
		memcpy(&meta->intel_id[0], INTEL_MAGIC, sizeof(INTEL_MAGIC));
		memcpy(&meta->version[0], INTEL_VERSION_1200,
		    sizeof(INTEL_VERSION_1200));
		meta->config_size = INTEL_MAX_MD_SIZE(numdisks);
		meta->config_id = mdi->mdio_config_id;
		meta->generation = 1;
		meta->total_disks = numdisks;
		meta->total_volumes = 1;
		error = 0;
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
			    &meta->disk[i].serial[0], INTEL_SERIAL_LEN);
			if (error != 0) {
				gctl_error(req,
				    "Can't get serial for provider '%s'.",
				    diskname);
				error = -8;
				break;
			}
			meta->disk[i].sectors = pp->mediasize / pp->sectorsize;
			if (size > meta->disk[i].sectors)
				size = meta->disk[i].sectors;
			if (sectorsize < pp->sectorsize)
				sectorsize = pp->sectorsize;
			meta->disk[i].id = 0xffffffff;
			meta->disk[i].flags = INTEL_F_ASSIGNED | INTEL_F_ONLINE;
		}
		if (error != 0)
			return (error);

		/* Reserve some space for metadata. */
		size -= (4096 + sectorsize - 1) / sectorsize + 1;

		size &= ~127;	/* Assume stripe size 64K */
		mvol = intel_get_volume(meta, 0);
		strlcpy(&mvol->name[0], volname, sizeof(mvol->name));
		if (level == G_RAID_VOLUME_RL_RAID0)
			mvol->total_sectors = size * numdisks;
		else if (level == G_RAID_VOLUME_RL_RAID5)
			mvol->total_sectors = size * (numdisks - 1);
		else
			mvol->total_sectors = size * (numdisks / 2);
		mmap = intel_get_map(mvol, 0);
		mmap->offset = 0;
		mmap->disk_sectors = size;
		mmap->stripe_count = size / 128;
		mmap->stripe_sectors = 128;
		mmap->status = INTEL_S_READY;
		if (level == G_RAID_VOLUME_RL_RAID0)
			mmap->type = INTEL_T_RAID0;
		else if (level == G_RAID_VOLUME_RL_RAID1 ||
		    level == G_RAID_VOLUME_RL_RAID10)
			mmap->type = INTEL_T_RAID1;
		else
			mmap->type = INTEL_T_RAID5;
		mmap->total_disks = numdisks;
		if (level == G_RAID_VOLUME_RL_RAID10)
			mmap->total_domains = numdisks / 2;
		else if (level == G_RAID_VOLUME_RL_RAID1)
			mmap->total_domains = numdisks;
		else
			mmap->total_domains = 1;
		mmap->failed_disk_num = ~0;
		mmap->ddf = 1;
		for (i = 0; i < numdisks; i++)
			mmap->disk_idx[i] = i;
		g_raid_md_intel_print(meta);
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			pd = (struct g_raid_md_intel_perdisk *)disk->d_md_data;
			pd->pd_meta = intel_meta_copy(meta);
			intel_meta_write(disk->d_consumer, meta);
		}
		g_raid_md_intel_start(sc);
		return (0);
	}
	return (-100);
}

static int
g_raid_md_write_intel(struct g_raid_md_object *md,
                              struct g_raid_disk *disk)
{

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
