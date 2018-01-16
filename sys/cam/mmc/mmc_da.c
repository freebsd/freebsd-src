/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006 Bernd Walter <tisco@FreeBSD.org>
 * Copyright (c) 2006 M. Warner Losh <imp@FreeBSD.org>
 * Copyright (c) 2009 Alexander Motin <mav@FreeBSD.org>
 * Copyright (c) 2015-2017 Ilya Bakulin <kibab@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Some code derived from the sys/dev/mmc and sys/cam/ata
 * Thanks to Warner Losh <imp@FreeBSD.org>, Alexander Motin <mav@FreeBSD.org>
 * Bernd Walter <tisco@FreeBSD.org>, and other authors.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

//#include "opt_sdda.h"

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/endian.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/cons.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <geom/geom_disk.h>
#include <machine/_inttypes.h>  /* for PRIu64 */
#endif /* _KERNEL */

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif /* _KERNEL */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#include <cam/cam_debug.h>


#include <cam/mmc/mmc_all.h>

#include <machine/md_var.h>	/* geometry translation */

#ifdef _KERNEL

typedef enum {
	SDDA_FLAG_OPEN		= 0x0002,
	SDDA_FLAG_DIRTY		= 0x0004
} sdda_flags;

typedef enum {
        SDDA_STATE_INIT,
        SDDA_STATE_INVALID,
        SDDA_STATE_NORMAL
} sdda_state;

struct sdda_softc {
	struct	 bio_queue_head bio_queue;
	int	 outstanding_cmds;	/* Number of active commands */
	int	 refcount;		/* Active xpt_action() calls */
	sdda_state state;
	sdda_flags flags;
	struct mmc_data *mmcdata;
//	sdda_quirks quirks;
	struct task start_init_task;
	struct	 disk *disk;
        uint32_t raw_csd[4];
	uint8_t raw_ext_csd[512]; /* MMC only? */
        struct mmc_csd csd;
        struct mmc_cid cid;
	struct mmc_scr scr;
        /* Calculated from CSD */
        uint64_t sector_count;
        uint64_t mediasize;

        /* Calculated from CID */
	char card_id_string[64];/* Formatted CID info (serial, MFG, etc) */
	char card_sn_string[16];/* Formatted serial # for disk->d_ident */
	/* Determined from CSD + is highspeed card*/
	uint32_t card_f_max;
};

#define ccb_bp		ppriv_ptr1

static	disk_strategy_t	sddastrategy;
static	periph_init_t	sddainit;
static	void		sddaasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	periph_ctor_t	sddaregister;
static	periph_dtor_t	sddacleanup;
static	periph_start_t	sddastart;
static	periph_oninv_t	sddaoninvalidate;
static	void		sddadone(struct cam_periph *periph,
			       union ccb *done_ccb);
static  int		sddaerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);

static uint16_t get_rca(struct cam_periph *periph);
static cam_status sdda_hook_into_geom(struct cam_periph *periph);
static void sdda_start_init(void *context, union ccb *start_ccb);
static void sdda_start_init_task(void *context, int pending);

static struct periph_driver sddadriver =
{
	sddainit, "sdda",
	TAILQ_HEAD_INITIALIZER(sddadriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(sdda, sddadriver);

static MALLOC_DEFINE(M_SDDA, "sd_da", "sd_da buffers");

static const int exp[8] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};

static const int mant[16] = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80
};

static const int cur_min[8] = {
	500, 1000, 5000, 10000, 25000, 35000, 60000, 100000
};

static const int cur_max[8] = {
	1000, 5000, 10000, 25000, 35000, 45000, 800000, 200000
};

static uint16_t
get_rca(struct cam_periph *periph) {
	return periph->path->device->mmc_ident_data.card_rca;
}

static uint32_t
mmc_get_bits(uint32_t *bits, int bit_len, int start, int size)
{
	const int i = (bit_len / 32) - (start / 32) - 1;
	const int shift = start & 31;
	uint32_t retval = bits[i] >> shift;
	if (size + shift > 32)
		retval |= bits[i - 1] << (32 - shift);
	return (retval & ((1llu << size) - 1));
}


static void
mmc_decode_csd_sd(uint32_t *raw_csd, struct mmc_csd *csd)
{
	int v;
	int m;
	int e;

	memset(csd, 0, sizeof(*csd));
	csd->csd_structure = v = mmc_get_bits(raw_csd, 128, 126, 2);
	if (v == 0) {
		m = mmc_get_bits(raw_csd, 128, 115, 4);
		e = mmc_get_bits(raw_csd, 128, 112, 3);
		csd->tacc = (exp[e] * mant[m] + 9) / 10;
		csd->nsac = mmc_get_bits(raw_csd, 128, 104, 8) * 100;
		m = mmc_get_bits(raw_csd, 128, 99, 4);
		e = mmc_get_bits(raw_csd, 128, 96, 3);
		csd->tran_speed = exp[e] * 10000 * mant[m];
		csd->ccc = mmc_get_bits(raw_csd, 128, 84, 12);
		csd->read_bl_len = 1 << mmc_get_bits(raw_csd, 128, 80, 4);
		csd->read_bl_partial = mmc_get_bits(raw_csd, 128, 79, 1);
		csd->write_blk_misalign = mmc_get_bits(raw_csd, 128, 78, 1);
		csd->read_blk_misalign = mmc_get_bits(raw_csd, 128, 77, 1);
		csd->dsr_imp = mmc_get_bits(raw_csd, 128, 76, 1);
		csd->vdd_r_curr_min = cur_min[mmc_get_bits(raw_csd, 128, 59, 3)];
		csd->vdd_r_curr_max = cur_max[mmc_get_bits(raw_csd, 128, 56, 3)];
		csd->vdd_w_curr_min = cur_min[mmc_get_bits(raw_csd, 128, 53, 3)];
		csd->vdd_w_curr_max = cur_max[mmc_get_bits(raw_csd, 128, 50, 3)];
		m = mmc_get_bits(raw_csd, 128, 62, 12);
		e = mmc_get_bits(raw_csd, 128, 47, 3);
		csd->capacity = ((1 + m) << (e + 2)) * csd->read_bl_len;
		csd->erase_blk_en = mmc_get_bits(raw_csd, 128, 46, 1);
		csd->erase_sector = mmc_get_bits(raw_csd, 128, 39, 7) + 1;
		csd->wp_grp_size = mmc_get_bits(raw_csd, 128, 32, 7);
		csd->wp_grp_enable = mmc_get_bits(raw_csd, 128, 31, 1);
		csd->r2w_factor = 1 << mmc_get_bits(raw_csd, 128, 26, 3);
		csd->write_bl_len = 1 << mmc_get_bits(raw_csd, 128, 22, 4);
		csd->write_bl_partial = mmc_get_bits(raw_csd, 128, 21, 1);
	} else if (v == 1) {
		m = mmc_get_bits(raw_csd, 128, 115, 4);
		e = mmc_get_bits(raw_csd, 128, 112, 3);
		csd->tacc = (exp[e] * mant[m] + 9) / 10;
		csd->nsac = mmc_get_bits(raw_csd, 128, 104, 8) * 100;
		m = mmc_get_bits(raw_csd, 128, 99, 4);
		e = mmc_get_bits(raw_csd, 128, 96, 3);
		csd->tran_speed = exp[e] * 10000 * mant[m];
		csd->ccc = mmc_get_bits(raw_csd, 128, 84, 12);
		csd->read_bl_len = 1 << mmc_get_bits(raw_csd, 128, 80, 4);
		csd->read_bl_partial = mmc_get_bits(raw_csd, 128, 79, 1);
		csd->write_blk_misalign = mmc_get_bits(raw_csd, 128, 78, 1);
		csd->read_blk_misalign = mmc_get_bits(raw_csd, 128, 77, 1);
		csd->dsr_imp = mmc_get_bits(raw_csd, 128, 76, 1);
		csd->capacity = ((uint64_t)mmc_get_bits(raw_csd, 128, 48, 22) + 1) *
		    512 * 1024;
		csd->erase_blk_en = mmc_get_bits(raw_csd, 128, 46, 1);
		csd->erase_sector = mmc_get_bits(raw_csd, 128, 39, 7) + 1;
		csd->wp_grp_size = mmc_get_bits(raw_csd, 128, 32, 7);
		csd->wp_grp_enable = mmc_get_bits(raw_csd, 128, 31, 1);
		csd->r2w_factor = 1 << mmc_get_bits(raw_csd, 128, 26, 3);
		csd->write_bl_len = 1 << mmc_get_bits(raw_csd, 128, 22, 4);
		csd->write_bl_partial = mmc_get_bits(raw_csd, 128, 21, 1);
	} else
		panic("unknown SD CSD version");
}

static void
mmc_decode_csd_mmc(uint32_t *raw_csd, struct mmc_csd *csd)
{
	int m;
	int e;

	memset(csd, 0, sizeof(*csd));
	csd->csd_structure = mmc_get_bits(raw_csd, 128, 126, 2);
	csd->spec_vers = mmc_get_bits(raw_csd, 128, 122, 4);
	m = mmc_get_bits(raw_csd, 128, 115, 4);
	e = mmc_get_bits(raw_csd, 128, 112, 3);
	csd->tacc = exp[e] * mant[m] + 9 / 10;
	csd->nsac = mmc_get_bits(raw_csd, 128, 104, 8) * 100;
	m = mmc_get_bits(raw_csd, 128, 99, 4);
	e = mmc_get_bits(raw_csd, 128, 96, 3);
	csd->tran_speed = exp[e] * 10000 * mant[m];
	csd->ccc = mmc_get_bits(raw_csd, 128, 84, 12);
	csd->read_bl_len = 1 << mmc_get_bits(raw_csd, 128, 80, 4);
	csd->read_bl_partial = mmc_get_bits(raw_csd, 128, 79, 1);
	csd->write_blk_misalign = mmc_get_bits(raw_csd, 128, 78, 1);
	csd->read_blk_misalign = mmc_get_bits(raw_csd, 128, 77, 1);
	csd->dsr_imp = mmc_get_bits(raw_csd, 128, 76, 1);
	csd->vdd_r_curr_min = cur_min[mmc_get_bits(raw_csd, 128, 59, 3)];
	csd->vdd_r_curr_max = cur_max[mmc_get_bits(raw_csd, 128, 56, 3)];
	csd->vdd_w_curr_min = cur_min[mmc_get_bits(raw_csd, 128, 53, 3)];
	csd->vdd_w_curr_max = cur_max[mmc_get_bits(raw_csd, 128, 50, 3)];
	m = mmc_get_bits(raw_csd, 128, 62, 12);
	e = mmc_get_bits(raw_csd, 128, 47, 3);
	csd->capacity = ((1 + m) << (e + 2)) * csd->read_bl_len;
	csd->erase_blk_en = 0;
	csd->erase_sector = (mmc_get_bits(raw_csd, 128, 42, 5) + 1) *
	    (mmc_get_bits(raw_csd, 128, 37, 5) + 1);
	csd->wp_grp_size = mmc_get_bits(raw_csd, 128, 32, 5);
	csd->wp_grp_enable = mmc_get_bits(raw_csd, 128, 31, 1);
	csd->r2w_factor = 1 << mmc_get_bits(raw_csd, 128, 26, 3);
	csd->write_bl_len = 1 << mmc_get_bits(raw_csd, 128, 22, 4);
	csd->write_bl_partial = mmc_get_bits(raw_csd, 128, 21, 1);
}

static void
mmc_decode_cid_sd(uint32_t *raw_cid, struct mmc_cid *cid)
{
	int i;

	/* There's no version info, so we take it on faith */
	memset(cid, 0, sizeof(*cid));
	cid->mid = mmc_get_bits(raw_cid, 128, 120, 8);
	cid->oid = mmc_get_bits(raw_cid, 128, 104, 16);
	for (i = 0; i < 5; i++)
		cid->pnm[i] = mmc_get_bits(raw_cid, 128, 96 - i * 8, 8);
	cid->pnm[5] = 0;
	cid->prv = mmc_get_bits(raw_cid, 128, 56, 8);
	cid->psn = mmc_get_bits(raw_cid, 128, 24, 32);
	cid->mdt_year = mmc_get_bits(raw_cid, 128, 12, 8) + 2000;
	cid->mdt_month = mmc_get_bits(raw_cid, 128, 8, 4);
}

static void
mmc_decode_cid_mmc(uint32_t *raw_cid, struct mmc_cid *cid)
{
	int i;

	/* There's no version info, so we take it on faith */
	memset(cid, 0, sizeof(*cid));
	cid->mid = mmc_get_bits(raw_cid, 128, 120, 8);
	cid->oid = mmc_get_bits(raw_cid, 128, 104, 8);
	for (i = 0; i < 6; i++)
		cid->pnm[i] = mmc_get_bits(raw_cid, 128, 96 - i * 8, 8);
	cid->pnm[6] = 0;
	cid->prv = mmc_get_bits(raw_cid, 128, 48, 8);
	cid->psn = mmc_get_bits(raw_cid, 128, 16, 32);
	cid->mdt_month = mmc_get_bits(raw_cid, 128, 12, 4);
	cid->mdt_year = mmc_get_bits(raw_cid, 128, 8, 4) + 1997;
}

static void
mmc_format_card_id_string(struct sdda_softc *sc, struct mmc_params *mmcp)
{
	char oidstr[8];
	uint8_t c1;
	uint8_t c2;

	/*
	 * Format a card ID string for use by the mmcsd driver, it's what
	 * appears between the <> in the following:
	 * mmcsd0: 968MB <SD SD01G 8.0 SN 2686905 Mfg 08/2008 by 3 TN> at mmc0
	 * 22.5MHz/4bit/128-block
	 *
	 * Also format just the card serial number, which the mmcsd driver will
	 * use as the disk->d_ident string.
	 *
	 * The card_id_string in mmc_ivars is currently allocated as 64 bytes,
	 * and our max formatted length is currently 55 bytes if every field
	 * contains the largest value.
	 *
	 * Sometimes the oid is two printable ascii chars; when it's not,
	 * format it as 0xnnnn instead.
	 */
	c1 = (sc->cid.oid >> 8) & 0x0ff;
	c2 = sc->cid.oid & 0x0ff;
	if (c1 > 0x1f && c1 < 0x7f && c2 > 0x1f && c2 < 0x7f)
		snprintf(oidstr, sizeof(oidstr), "%c%c", c1, c2);
	else
		snprintf(oidstr, sizeof(oidstr), "0x%04x", sc->cid.oid);
	snprintf(sc->card_sn_string, sizeof(sc->card_sn_string),
	    "%08X", sc->cid.psn);
	snprintf(sc->card_id_string, sizeof(sc->card_id_string),
                 "%s%s %s %d.%d SN %08X MFG %02d/%04d by %d %s",
                 mmcp->card_features & CARD_FEATURE_MMC ? "MMC" : "SD",
                 mmcp->card_features & CARD_FEATURE_SDHC ? "HC" : "",
                 sc->cid.pnm, sc->cid.prv >> 4, sc->cid.prv & 0x0f,
                 sc->cid.psn, sc->cid.mdt_month, sc->cid.mdt_year,
                 sc->cid.mid, oidstr);
}

static int
sddaopen(struct disk *dp)
{
	struct cam_periph *periph;
	struct sdda_softc *softc;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		return(ENXIO);
	}

	cam_periph_lock(periph);
	if ((error = cam_periph_hold(periph, PRIBIO|PCATCH)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddaopen\n"));

	softc = (struct sdda_softc *)periph->softc;
	softc->flags |= SDDA_FLAG_OPEN;

	cam_periph_unhold(periph);
	cam_periph_unlock(periph);
	return (0);
}

static int
sddaclose(struct disk *dp)
{
	struct	cam_periph *periph;
	struct	sdda_softc *softc;
//	union ccb *ccb;
//	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	softc = (struct sdda_softc *)periph->softc;
        softc->flags &= ~SDDA_FLAG_OPEN;

	cam_periph_lock(periph);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddaclose\n"));

	while (softc->refcount != 0)
		cam_periph_sleep(periph, &softc->refcount, PRIBIO, "sddaclose", 1);
	cam_periph_unlock(periph);
	cam_periph_release(periph);
	return (0);
}

static void
sddaschedule(struct cam_periph *periph)
{
	struct sdda_softc *softc = (struct sdda_softc *)periph->softc;

	/* Check if we have more work to do. */
	if (bioq_first(&softc->bio_queue)) {
		xpt_schedule(periph, CAM_PRIORITY_NORMAL);
	}
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
sddastrategy(struct bio *bp)
{
	struct cam_periph *periph;
	struct sdda_softc *softc;

	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	softc = (struct sdda_softc *)periph->softc;

	cam_periph_lock(periph);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddastrategy(%p)\n", bp));

	/*
	 * If the device has been made invalid, error out
	 */
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, ENXIO);
		return;
	}

	/*
	 * Place it in the queue of disk activities for this disk
	 */
        bioq_disksort(&softc->bio_queue, bp);

	/*
	 * Schedule ourselves for performing the work.
	 */
	sddaschedule(periph);
	cam_periph_unlock(periph);

	return;
}

static void
sddainit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, sddaasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("sdda: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
}

/*
 * Callback from GEOM, called when it has finished cleaning up its
 * resources.
 */
static void
sddadiskgonecb(struct disk *dp)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)dp->d_drv1;
        CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddadiskgonecb\n"));

	cam_periph_release(periph);
}

static void
sddaoninvalidate(struct cam_periph *periph)
{
	struct sdda_softc *softc;

	softc = (struct sdda_softc *)periph->softc;

        CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddaoninvalidate\n"));

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, sddaasync, periph, periph->path);

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
        CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("bioq_flush start\n"));
	bioq_flush(&softc->bio_queue, NULL, ENXIO);
        CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("bioq_flush end\n"));

	disk_gone(softc->disk);
}

static void
sddacleanup(struct cam_periph *periph)
{
	struct sdda_softc *softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddacleanup\n"));
	softc = (struct sdda_softc *)periph->softc;

	cam_periph_unlock(periph);

	disk_destroy(softc->disk);
	free(softc, M_DEVBUF);
	cam_periph_lock(periph);
}

static void
sddaasync(void *callback_arg, u_int32_t code,
	struct cam_path *path, void *arg)
{
	struct ccb_getdev cgd;
	struct cam_periph *periph;
	struct sdda_softc *softc;

	periph = (struct cam_periph *)callback_arg;
        CAM_DEBUG(path, CAM_DEBUG_TRACE, ("sddaasync(code=%d)\n", code));
	switch (code) {
	case AC_FOUND_DEVICE:
	{
                CAM_DEBUG(path, CAM_DEBUG_TRACE, ("=> AC_FOUND_DEVICE\n"));
		struct ccb_getdev *cgd;
		cam_status status;

		cgd = (struct ccb_getdev *)arg;
		if (cgd == NULL)
			break;

		if (cgd->protocol != PROTO_MMCSD)
			break;

                if (!(path->device->mmc_ident_data.card_features & CARD_FEATURE_MEMORY)) {
                        CAM_DEBUG(path, CAM_DEBUG_TRACE, ("No memory on the card!\n"));
                        break;
                }

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(sddaregister, sddaoninvalidate,
					  sddacleanup, sddastart,
					  "sdda", CAM_PERIPH_BIO,
					  path, sddaasync,
					  AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("sddaasync: Unable to attach to new device "
				"due to status 0x%x\n", status);
		break;
	}
	case AC_GETDEV_CHANGED:
	{
		CAM_DEBUG(path, CAM_DEBUG_TRACE, ("=> AC_GETDEV_CHANGED\n"));
		softc = (struct sdda_softc *)periph->softc;
		xpt_setup_ccb(&cgd.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		cgd.ccb_h.func_code = XPT_GDEV_TYPE;
		xpt_action((union ccb *)&cgd);
		cam_periph_async(periph, code, path, arg);
		break;
	}
	case AC_ADVINFO_CHANGED:
	{
		uintptr_t buftype;
		CAM_DEBUG(path, CAM_DEBUG_TRACE, ("=> AC_ADVINFO_CHANGED\n"));
		buftype = (uintptr_t)arg;
		if (buftype == CDAI_TYPE_PHYS_PATH) {
			struct sdda_softc *softc;

			softc = periph->softc;
			disk_attr_changed(softc->disk, "GEOM::physpath",
					  M_NOWAIT);
		}
		break;
	}
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("AC_BUS_RESET"));
	}
	default:
		CAM_DEBUG(path, CAM_DEBUG_TRACE, ("=> default?!\n"));
		cam_periph_async(periph, code, path, arg);
		break;
	}
}


static int
sddagetattr(struct bio *bp)
{
	int ret;
	struct cam_periph *periph;

	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	cam_periph_lock(periph);
	ret = xpt_getattr(bp->bio_data, bp->bio_length, bp->bio_attribute,
	    periph->path);
	cam_periph_unlock(periph);
	if (ret == 0)
		bp->bio_completed = bp->bio_length;
	return ret;
}

static cam_status
sddaregister(struct cam_periph *periph, void *arg)
{
	struct sdda_softc *softc;
//	struct ccb_pathinq cpi;
	struct ccb_getdev *cgd;
//	char   announce_buf[80], buf1[32];
//	caddr_t match;
	union ccb *request_ccb;	/* CCB representing the probe request */

        CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddaregister\n"));
	cgd = (struct ccb_getdev *)arg;
	if (cgd == NULL) {
		printf("sddaregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct sdda_softc *)malloc(sizeof(*softc), M_DEVBUF,
	    M_NOWAIT|M_ZERO);

	if (softc == NULL) {
		printf("sddaregister: Unable to probe new device. "
		    "Unable to allocate softc\n");
		return(CAM_REQ_CMP_ERR);
	}

	bioq_init(&softc->bio_queue);
	softc->state = SDDA_STATE_INIT;
	softc->mmcdata =
		(struct mmc_data *) malloc(sizeof(struct mmc_data), M_DEVBUF, M_NOWAIT|M_ZERO);
	periph->softc = softc;

	request_ccb = (union ccb*) arg;
	xpt_schedule(periph, CAM_PRIORITY_XPT);
	TASK_INIT(&softc->start_init_task, 0, sdda_start_init_task, periph);
	taskqueue_enqueue(taskqueue_thread, &softc->start_init_task);

	return (CAM_REQ_CMP);
}

static cam_status
sdda_hook_into_geom(struct cam_periph *periph)
{
	struct sdda_softc *softc;
	struct ccb_pathinq cpi;
	struct ccb_getdev cgd;
	u_int maxio;

	softc = (struct sdda_softc*) periph->softc;

	xpt_path_inq(&cpi, periph->path);

	bzero(&cgd, sizeof(cgd));
	xpt_setup_ccb(&cgd.ccb_h, periph->path, CAM_PRIORITY_NONE);
	cpi.ccb_h.func_code = XPT_GDEV_TYPE;
	xpt_action((union ccb *)&cgd);

	/*
	 * Register this media as a disk
	 */
	(void)cam_periph_hold(periph, PRIBIO);
	cam_periph_unlock(periph);

	softc->disk = disk_alloc();
	softc->disk->d_rotation_rate = 0;
	softc->disk->d_devstat = devstat_new_entry(periph->periph_name,
			  periph->unit_number, 512,
			  DEVSTAT_ALL_SUPPORTED,
			  DEVSTAT_TYPE_DIRECT |
			  XPORT_DEVSTAT_TYPE(cpi.transport),
			  DEVSTAT_PRIORITY_DISK);
	softc->disk->d_open = sddaopen;
	softc->disk->d_close = sddaclose;
	softc->disk->d_strategy = sddastrategy;
	softc->disk->d_getattr = sddagetattr;
//	softc->disk->d_dump = sddadump;
	softc->disk->d_gone = sddadiskgonecb;
	softc->disk->d_name = "sdda";
	softc->disk->d_drv1 = periph;
	maxio = cpi.maxio;		/* Honor max I/O size of SIM */
	if (maxio == 0)
		maxio = DFLTPHYS;	/* traditional default */
	else if (maxio > MAXPHYS)
		maxio = MAXPHYS;	/* for safety */
	softc->disk->d_maxsize = maxio;
	softc->disk->d_unit = periph->unit_number;
	softc->disk->d_flags = DISKFLAG_CANDELETE;
	strlcpy(softc->disk->d_descr, softc->card_id_string,
	    MIN(sizeof(softc->disk->d_descr), sizeof(softc->card_id_string)));
	strlcpy(softc->disk->d_ident, softc->card_sn_string,
	    MIN(sizeof(softc->disk->d_ident), sizeof(softc->card_sn_string)));
	softc->disk->d_hba_vendor = cpi.hba_vendor;
	softc->disk->d_hba_device = cpi.hba_device;
	softc->disk->d_hba_subvendor = cpi.hba_subvendor;
	softc->disk->d_hba_subdevice = cpi.hba_subdevice;

	softc->disk->d_sectorsize = 512;
	softc->disk->d_mediasize = softc->mediasize;
	softc->disk->d_stripesize = 0;
	softc->disk->d_fwsectors = 0;
	softc->disk->d_fwheads = 0;

	/*
	 * Acquire a reference to the periph before we register with GEOM.
	 * We'll release this reference once GEOM calls us back (via
	 * sddadiskgonecb()) telling us that our provider has been freed.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}
	disk_create(softc->disk, DISK_VERSION);
	cam_periph_lock(periph);
	cam_periph_unhold(periph);

	xpt_announce_periph(periph, softc->card_id_string);

	/*
	 * Add async callbacks for bus reset and
	 * bus device reset calls.  I don't bother
	 * checking if this fails as, in most cases,
	 * the system will function just fine without
	 * them and the only alternative would be to
	 * not attach the device on failure.
	 */
	xpt_register_async(AC_SENT_BDR | AC_BUS_RESET | AC_LOST_DEVICE |
	    AC_GETDEV_CHANGED | AC_ADVINFO_CHANGED,
	    sddaasync, periph, periph->path);

	return(CAM_REQ_CMP);
}

static int
mmc_exec_app_cmd(struct cam_periph *periph, union ccb *ccb,
	struct mmc_command *cmd) {
	int err;

	/* Send APP_CMD first */
	memset(&ccb->mmcio.cmd, 0, sizeof(struct mmc_command));
	memset(&ccb->mmcio.stop, 0, sizeof(struct mmc_command));
	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_NONE,
		       /*mmc_opcode*/ MMC_APP_CMD,
		       /*mmc_arg*/ get_rca(periph) << 16,
		       /*mmc_flags*/ MMC_RSP_R1 | MMC_CMD_AC,
		       /*mmc_data*/ NULL,
		       /*timeout*/ 0);

	err = cam_periph_runccb(ccb, sddaerror, CAM_FLAG_NONE, /*sense_flags*/0, NULL);
	if (err != 0)
		return err;
	if (!(ccb->mmcio.cmd.resp[0] & R1_APP_CMD))
		return MMC_ERR_FAILED;

	/* Now exec actual command */
	int flags = 0;
	if (cmd->data != NULL) {
		ccb->mmcio.cmd.data = cmd->data;
		if (cmd->data->flags & MMC_DATA_READ)
			flags |= CAM_DIR_IN;
		if (cmd->data->flags & MMC_DATA_WRITE)
			flags |= CAM_DIR_OUT;
	} else flags = CAM_DIR_NONE;

	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ flags,
		       /*mmc_opcode*/ cmd->opcode,
		       /*mmc_arg*/ cmd->arg,
		       /*mmc_flags*/ cmd->flags,
		       /*mmc_data*/ cmd->data,
		       /*timeout*/ 0);

	err = cam_periph_runccb(ccb, sddaerror, CAM_FLAG_NONE, /*sense_flags*/0, NULL);
	memcpy(cmd->resp, ccb->mmcio.cmd.resp, sizeof(cmd->resp));
	cmd->error = ccb->mmcio.cmd.error;
	if (err != 0)
		return err;
	return 0;
}

static int
mmc_app_get_scr(struct cam_periph *periph, union ccb *ccb, uint32_t *rawscr) {
	int err;
	struct mmc_command cmd;
	struct mmc_data d;

	memset(&cmd, 0, sizeof(cmd));

	memset(rawscr, 0, 8);
	cmd.opcode = ACMD_SEND_SCR;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.arg = 0;

	d.data = rawscr;
	d.len = 8;
	d.flags = MMC_DATA_READ;
	cmd.data = &d;

	err = mmc_exec_app_cmd(periph, ccb, &cmd);
	rawscr[0] = be32toh(rawscr[0]);
	rawscr[1] = be32toh(rawscr[1]);
	return (err);
}

static int
mmc_send_ext_csd(struct cam_periph *periph, union ccb *ccb,
		 uint8_t *rawextcsd, size_t buf_len) {
	int err;
	struct mmc_data d;

	KASSERT(buf_len == 512, ("Buffer for ext csd must be 512 bytes"));
	d.data = rawextcsd;
	d.len = buf_len;
	d.flags = MMC_DATA_READ;
	memset(d.data, 0, d.len);

	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_IN,
		       /*mmc_opcode*/ MMC_SEND_EXT_CSD,
		       /*mmc_arg*/ 0,
		       /*mmc_flags*/ MMC_RSP_R1 | MMC_CMD_ADTC,
		       /*mmc_data*/ &d,
		       /*timeout*/ 0);

	err = cam_periph_runccb(ccb, sddaerror, CAM_FLAG_NONE, /*sense_flags*/0, NULL);
	if (err != 0)
		return err;
	if (!(ccb->mmcio.cmd.resp[0] & R1_APP_CMD))
		return MMC_ERR_FAILED;

	return MMC_ERR_NONE;
}

static void
mmc_app_decode_scr(uint32_t *raw_scr, struct mmc_scr *scr)
{
	unsigned int scr_struct;

	memset(scr, 0, sizeof(*scr));

	scr_struct = mmc_get_bits(raw_scr, 64, 60, 4);
	if (scr_struct != 0) {
		printf("Unrecognised SCR structure version %d\n",
		    scr_struct);
		return;
	}
	scr->sda_vsn = mmc_get_bits(raw_scr, 64, 56, 4);
	scr->bus_widths = mmc_get_bits(raw_scr, 64, 48, 4);
}

static int
mmc_switch(struct cam_periph *periph, union ccb *ccb,
	   uint8_t set, uint8_t index, uint8_t value)
{
	int arg = (MMC_SWITCH_FUNC_WR << 24) |
	    (index << 16) |
	    (value << 8) |
	    set;
	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_NONE,
		       /*mmc_opcode*/ MMC_SWITCH_FUNC,
		       /*mmc_arg*/ arg,
		       /*mmc_flags*/ MMC_RSP_R1B | MMC_CMD_AC,
		       /*mmc_data*/ NULL,
		       /*timeout*/ 0);

	cam_periph_runccb(ccb, sddaerror, CAM_FLAG_NONE, /*sense_flags*/0, NULL);

	if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)) {
		if (ccb->mmcio.cmd.error != 0) {
			CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_PERIPH,
				  ("%s: MMC command failed", __func__));
			return EIO;
		}
		return 0; /* Normal return */
	} else {
		CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_PERIPH,
			  ("%s: CAM request failed\n", __func__));
		return EIO;
	}

}

static int
mmc_sd_switch(struct cam_periph *periph, union ccb *ccb,
	      uint8_t mode, uint8_t grp, uint8_t value,
	      uint8_t *res) {

	struct mmc_data mmc_d;

	memset(res, 0, 64);
	mmc_d.len = 64;
	mmc_d.data = res;
	mmc_d.flags = MMC_DATA_READ;

	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_IN,
		       /*mmc_opcode*/ SD_SWITCH_FUNC,
		       /*mmc_arg*/ mode << 31,
		       /*mmc_flags*/ MMC_RSP_R1 | MMC_CMD_ADTC,
		       /*mmc_data*/ &mmc_d,
		       /*timeout*/ 0);

	cam_periph_runccb(ccb, sddaerror, CAM_FLAG_NONE, /*sense_flags*/0, NULL);

	if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)) {
		if (ccb->mmcio.cmd.error != 0) {
			CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_PERIPH,
				  ("%s: MMC command failed", __func__));
			return EIO;
		}
		return 0; /* Normal return */
	} else {
		CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_PERIPH,
			  ("%s: CAM request failed\n", __func__));
		return EIO;
	}
}

static int
mmc_set_timing(struct cam_periph *periph,
	       union ccb *ccb,
	       enum mmc_bus_timing timing)
{
	u_char switch_res[64];
	int err;
	uint8_t	value;
	struct mmc_params *mmcp = &periph->path->device->mmc_ident_data;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("mmc_set_timing(timing=%d)", timing));
	switch (timing) {
	case bus_timing_normal:
		value = 0;
		break;
	case bus_timing_hs:
		value = 1;
		break;
	default:
		return (MMC_ERR_INVALID);
	}
	if (mmcp->card_features & CARD_FEATURE_MMC) {
		err = mmc_switch(periph, ccb, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_HS_TIMING, value);
	} else {
		err = mmc_sd_switch(periph, ccb, SD_SWITCH_MODE_SET, SD_SWITCH_GROUP1, value, switch_res);
	}

	/* Set high-speed timing on the host */
	struct ccb_trans_settings_mmc *cts;
	cts = &ccb->cts.proto_specific.mmc;
	ccb->ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
	ccb->ccb_h.flags = CAM_DIR_NONE;
	ccb->ccb_h.retry_count = 0;
	ccb->ccb_h.timeout = 100;
	ccb->ccb_h.cbfcnp = NULL;
	cts->ios.timing = timing;
	cts->ios_valid = MMC_BT;
	xpt_action(ccb);

	return (err);
}

static void
sdda_start_init_task(void *context, int pending) {
	union ccb *new_ccb;
	struct cam_periph *periph;

	periph = (struct cam_periph *)context;
	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sdda_start_init_task\n"));
	new_ccb = xpt_alloc_ccb();
	xpt_setup_ccb(&new_ccb->ccb_h, periph->path,
		      CAM_PRIORITY_NONE);

	cam_periph_lock(periph);
	sdda_start_init(context, new_ccb);
	cam_periph_unlock(periph);
	xpt_free_ccb(new_ccb);
}

static void
sdda_set_bus_width(struct cam_periph *periph, union ccb *ccb, int width) {
	struct mmc_params *mmcp = &periph->path->device->mmc_ident_data;
	int err;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sdda_set_bus_width\n"));

	/* First set for the card, then for the host */
	if (mmcp->card_features & CARD_FEATURE_MMC) {
		uint8_t	value;
		switch (width) {
		case bus_width_1:
			value = EXT_CSD_BUS_WIDTH_1;
			break;
		case bus_width_4:
			value = EXT_CSD_BUS_WIDTH_4;
			break;
		case bus_width_8:
			value = EXT_CSD_BUS_WIDTH_8;
			break;
		default:
			panic("Invalid bus width %d", width);
		}
		err = mmc_switch(periph, ccb, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_BUS_WIDTH, value);
	} else {
		/* For SD cards we send ACMD6 with the required bus width in arg */
		struct mmc_command cmd;
		memset(&cmd, 0, sizeof(struct mmc_command));
		cmd.opcode = ACMD_SET_BUS_WIDTH;
		cmd.arg = width;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		err = mmc_exec_app_cmd(periph, ccb, &cmd);
	}

	if (err != MMC_ERR_NONE) {
		CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH, ("Error %d when setting bus width on the card\n", err));
		return;
	}
	/* Now card is done, set the host to the same width */
	struct ccb_trans_settings_mmc *cts;
	cts = &ccb->cts.proto_specific.mmc;
	ccb->ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
	ccb->ccb_h.flags = CAM_DIR_NONE;
	ccb->ccb_h.retry_count = 0;
	ccb->ccb_h.timeout = 100;
	ccb->ccb_h.cbfcnp = NULL;
	cts->ios.bus_width = width;
	cts->ios_valid = MMC_BW;
	xpt_action(ccb);
}

static inline const char *bus_width_str(enum mmc_bus_width w) {
	switch (w) {
	case bus_width_1:
		return "1-bit";
	case bus_width_4:
		return "4-bit";
	case bus_width_8:
		return "8-bit";
	}
}

static void
sdda_start_init(void *context, union ccb *start_ccb) {
	struct cam_periph *periph;
	periph = (struct cam_periph *)context;
	int err;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sdda_start_init\n"));
	/* periph was held for us when this task was enqueued */
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_release(periph);
		return;
	}

	struct sdda_softc *softc = (struct sdda_softc *)periph->softc;
	//struct ccb_mmcio *mmcio = &start_ccb->mmcio;
	struct mmc_params *mmcp = &periph->path->device->mmc_ident_data;
	struct cam_ed *device = periph->path->device;

	if (mmcp->card_features & CARD_FEATURE_MMC) {
		mmc_decode_csd_mmc(mmcp->card_csd, &softc->csd);
		mmc_decode_cid_mmc(mmcp->card_cid, &softc->cid);
		if (softc->csd.spec_vers >= 4)
			err = mmc_send_ext_csd(periph, start_ccb,
					       (uint8_t *)&softc->raw_ext_csd,
					       sizeof(softc->raw_ext_csd));
	} else {
		mmc_decode_csd_sd(mmcp->card_csd, &softc->csd);
		mmc_decode_cid_sd(mmcp->card_cid, &softc->cid);
	}

	softc->sector_count = softc->csd.capacity / 512;
	softc->mediasize = softc->csd.capacity;

	/* MMC >= 4.x have EXT_CSD that has its own opinion about capacity */
	if (softc->csd.spec_vers >= 4) {
		uint32_t sec_count = softc->raw_ext_csd[EXT_CSD_SEC_CNT] +
			(softc->raw_ext_csd[EXT_CSD_SEC_CNT + 1] << 8) +
			(softc->raw_ext_csd[EXT_CSD_SEC_CNT + 2] << 16) +
			(softc->raw_ext_csd[EXT_CSD_SEC_CNT + 3] << 24);
		if (sec_count != 0) {
			softc->sector_count = sec_count;
			softc->mediasize = softc->sector_count * 512;
			/* FIXME: there should be a better name for this option...*/
			mmcp->card_features |= CARD_FEATURE_SDHC;
		}

	}
	CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH,
		  ("Capacity: %"PRIu64", sectors: %"PRIu64"\n",
		   softc->mediasize,
		   softc->sector_count));
	mmc_format_card_id_string(softc, mmcp);

	/* Update info for CAM */
	device->serial_num_len = strlen(softc->card_sn_string);
	device->serial_num =
		(u_int8_t *)malloc((device->serial_num_len + 1),
				   M_CAMXPT, M_NOWAIT);
	strlcpy(device->serial_num, softc->card_sn_string, device->serial_num_len);

	device->device_id_len = strlen(softc->card_id_string);
	device->device_id =
		(u_int8_t *)malloc((device->device_id_len + 1),
				   M_CAMXPT, M_NOWAIT);
	strlcpy(device->device_id, softc->card_id_string, device->device_id_len);

	strlcpy(mmcp->model, softc->card_id_string, sizeof(mmcp->model));

	/* Set the clock frequency that the card can handle */
	struct ccb_trans_settings_mmc *cts;
	cts = &start_ccb->cts.proto_specific.mmc;

	/* First, get the host's max freq */
	start_ccb->ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	start_ccb->ccb_h.flags = CAM_DIR_NONE;
	start_ccb->ccb_h.retry_count = 0;
	start_ccb->ccb_h.timeout = 100;
	start_ccb->ccb_h.cbfcnp = NULL;
	xpt_action(start_ccb);

	if (start_ccb->ccb_h.status != CAM_REQ_CMP)
		panic("Cannot get max host freq");
	int host_f_max = cts->host_f_max;
	uint32_t host_caps = cts->host_caps;
	if (cts->ios.bus_width != bus_width_1)
		panic("Bus width in ios is not 1-bit");

	/* Now check if the card supports High-speed */
	softc->card_f_max = softc->csd.tran_speed;

	if (host_caps & MMC_CAP_HSPEED) {
		/* Find out if the card supports High speed timing */
		if (mmcp->card_features & CARD_FEATURE_SD20) {
			/* Get and decode SCR */
			uint32_t rawscr;
			uint8_t res[64];
			if (mmc_app_get_scr(periph, start_ccb, &rawscr)) {
				CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH, ("Cannot get SCR\n"));
				goto finish_hs_tests;
			}
			mmc_app_decode_scr(&rawscr, &softc->scr);

			if ((softc->scr.sda_vsn >= 1) && (softc->csd.ccc & (1<<10))) {
				mmc_sd_switch(periph, start_ccb, SD_SWITCH_MODE_CHECK,
					      SD_SWITCH_GROUP1, SD_SWITCH_NOCHANGE, res);
				if (res[13] & 2) {
					CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH, ("Card supports HS\n"));
					softc->card_f_max = SD_HS_MAX;
				}
			} else {
				CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH, ("Not trying the switch\n"));
				goto finish_hs_tests;
			}
		}

		if (mmcp->card_features & CARD_FEATURE_MMC && softc->csd.spec_vers >= 4) {
			if (softc->raw_ext_csd[EXT_CSD_CARD_TYPE]
			    & EXT_CSD_CARD_TYPE_HS_52)
				softc->card_f_max = MMC_TYPE_HS_52_MAX;
			else if (softc->raw_ext_csd[EXT_CSD_CARD_TYPE]
				 & EXT_CSD_CARD_TYPE_HS_26)
				softc->card_f_max = MMC_TYPE_HS_26_MAX;
		}
	}
	int f_max;
finish_hs_tests:
	f_max = min(host_f_max, softc->card_f_max);
	CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH, ("Set SD freq to %d MHz (min out of host f=%d MHz and card f=%d MHz)\n", f_max  / 1000000, host_f_max / 1000000, softc->card_f_max / 1000000));

	start_ccb->ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
	start_ccb->ccb_h.flags = CAM_DIR_NONE;
	start_ccb->ccb_h.retry_count = 0;
	start_ccb->ccb_h.timeout = 100;
	start_ccb->ccb_h.cbfcnp = NULL;
	cts->ios.clock = f_max;
	cts->ios_valid = MMC_CLK;
	xpt_action(start_ccb);

	/* Set bus width */
	enum mmc_bus_width desired_bus_width = bus_width_1;
	enum mmc_bus_width max_host_bus_width =
		(host_caps & MMC_CAP_8_BIT_DATA ? bus_width_8 :
		 host_caps & MMC_CAP_4_BIT_DATA ? bus_width_4 : bus_width_1);
	enum mmc_bus_width max_card_bus_width = bus_width_1;
	if (mmcp->card_features & CARD_FEATURE_SD20 &&
	    softc->scr.bus_widths & SD_SCR_BUS_WIDTH_4)
		max_card_bus_width = bus_width_4;
	/*
	 * Unlike SD, MMC cards don't have any information about supported bus width...
	 * So we need to perform read/write test to find out the width.
	 */
	/* TODO: figure out bus width for MMC; use 8-bit for now (to test on BBB) */
	if (mmcp->card_features & CARD_FEATURE_MMC)
		max_card_bus_width = bus_width_8;

	desired_bus_width = min(max_host_bus_width, max_card_bus_width);
	CAM_DEBUG(periph->path, CAM_DEBUG_PERIPH,
		  ("Set bus width to %s (min of host %s and card %s)\n",
		   bus_width_str(desired_bus_width),
		   bus_width_str(max_host_bus_width),
		   bus_width_str(max_card_bus_width)));
	sdda_set_bus_width(periph, start_ccb, desired_bus_width);

	if (f_max > 25000000) {
		err = mmc_set_timing(periph, start_ccb, bus_timing_hs);
		if (err != MMC_ERR_NONE)
			CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("Cannot switch card to high-speed mode"));
	}
	softc->state = SDDA_STATE_NORMAL;
	sdda_hook_into_geom(periph);
}

/* Called with periph lock held! */
static void
sddastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct sdda_softc *softc = (struct sdda_softc *)periph->softc;
	struct mmc_params *mmcp = &periph->path->device->mmc_ident_data;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sddastart\n"));

	if (softc->state != SDDA_STATE_NORMAL) {
		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("device is not in SDDA_STATE_NORMAL yet"));
		xpt_release_ccb(start_ccb);
		return;
	}
	struct bio *bp;

	/* Run regular command. */
	bp = bioq_first(&softc->bio_queue);
	if (bp == NULL) {
		xpt_release_ccb(start_ccb);
		return;
	}
	bioq_remove(&softc->bio_queue, bp);

	switch (bp->bio_cmd) {
	case BIO_WRITE:
		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("BIO_WRITE\n"));
		softc->flags |= SDDA_FLAG_DIRTY;
		/* FALLTHROUGH */
	case BIO_READ:
	{
		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("BIO_READ\n"));
		uint64_t blockno = bp->bio_pblkno;
		uint16_t count = bp->bio_bcount / 512;
		uint16_t opcode;

		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("Block %"PRIu64" cnt %u\n", blockno, count));

		/* Construct new MMC command */
		if (bp->bio_cmd == BIO_READ) {
			if (count > 1)
				opcode = MMC_READ_MULTIPLE_BLOCK;
			else
				opcode = MMC_READ_SINGLE_BLOCK;
		} else {
			if (count > 1)
				opcode = MMC_WRITE_MULTIPLE_BLOCK;
			else
				opcode = MMC_WRITE_BLOCK;
		}

		start_ccb->ccb_h.func_code = XPT_MMC_IO;
		start_ccb->ccb_h.flags = (bp->bio_cmd == BIO_READ ? CAM_DIR_IN : CAM_DIR_OUT);
		start_ccb->ccb_h.retry_count = 0;
		start_ccb->ccb_h.timeout = 15 * 1000;
		start_ccb->ccb_h.cbfcnp = sddadone;
		struct ccb_mmcio *mmcio;

		mmcio = &start_ccb->mmcio;
		mmcio->cmd.opcode = opcode;
		mmcio->cmd.arg = blockno;
		if (!(mmcp->card_features & CARD_FEATURE_SDHC))
			mmcio->cmd.arg <<= 9;

		mmcio->cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		mmcio->cmd.data = softc->mmcdata;
		mmcio->cmd.data->data = bp->bio_data;
		mmcio->cmd.data->len = 512 * count;
		mmcio->cmd.data->flags = (bp->bio_cmd == BIO_READ ? MMC_DATA_READ : MMC_DATA_WRITE);
		/* Direct h/w to issue CMD12 upon completion */
		if (count > 1) {
			mmcio->stop.opcode = MMC_STOP_TRANSMISSION;
			mmcio->stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
			mmcio->stop.arg = 0;
		}

		break;
	}
	case BIO_FLUSH:
		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("BIO_FLUSH\n"));
		sddaschedule(periph);
		break;
	case BIO_DELETE:
		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("BIO_DELETE\n"));
		sddaschedule(periph);
		break;
	}
	start_ccb->ccb_h.ccb_bp = bp;
	softc->outstanding_cmds++;
	softc->refcount++;
	cam_periph_unlock(periph);
	xpt_action(start_ccb);
	cam_periph_lock(periph);
	softc->refcount--;

	/* May have more work to do, so ensure we stay scheduled */
	sddaschedule(periph);
}

static void
sddadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct sdda_softc *softc;
	struct ccb_mmcio *mmcio;
//	struct ccb_getdev *cgd;
	struct cam_path *path;
//	int state;

	softc = (struct sdda_softc *)periph->softc;
	mmcio = &done_ccb->mmcio;
	path = done_ccb->ccb_h.path;

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("sddadone\n"));

	struct bio *bp;
	int error = 0;

//        cam_periph_lock(periph);
	if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		CAM_DEBUG(path, CAM_DEBUG_TRACE, ("Error!!!\n"));
		if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);
		error = 5; /* EIO */
	} else {
		if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
			panic("REQ_CMP with QFRZN");
		error = 0;
	}


	bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
	bp->bio_error = error;
	if (error != 0) {
		bp->bio_resid = bp->bio_bcount;
		bp->bio_flags |= BIO_ERROR;
	} else {
		/* XXX: How many bytes remaining? */
		bp->bio_resid = 0;
		if (bp->bio_resid > 0)
			bp->bio_flags |= BIO_ERROR;
	}

	uint32_t card_status = mmcio->cmd.resp[0];
	CAM_DEBUG(path, CAM_DEBUG_TRACE,
		  ("Card status: %08x\n", R1_STATUS(card_status)));
	CAM_DEBUG(path, CAM_DEBUG_TRACE,
		  ("Current state: %d\n", R1_CURRENT_STATE(card_status)));

	softc->outstanding_cmds--;
	xpt_release_ccb(done_ccb);
	biodone(bp);
}

static int
sddaerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	return(cam_periph_error(ccb, cam_flags, sense_flags));
}
#endif /* _KERNEL */
