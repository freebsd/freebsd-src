/*-
 * Copyright (c) 2006 Bernd Walter.  All rights reserved.
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mmc/mmc.c,v 1.4 2007/06/05 17:04:44 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcvar.h>
#include "mmcbr_if.h"
#include "mmcbus_if.h"

struct mmc_softc {
	device_t dev;
	struct mtx sc_mtx;
	struct intr_config_hook config_intrhook;
	device_t owner;
	uint32_t last_rca;
};

/*
 * Per-card data
 */
struct mmc_ivars {
	uint32_t raw_cid[4];	/* Raw bits of the CID */
	uint32_t raw_csd[4];	/* Raw bits of the CSD */
	uint16_t rca;
	enum mmc_card_mode mode;
	struct mmc_cid cid;	/* cid decoded */
	struct mmc_csd csd;	/* csd decoded */
};

#define CMD_RETRIES	3

/* bus entry points */
static int mmc_probe(device_t dev);
static int mmc_attach(device_t dev);
static int mmc_detach(device_t dev);

#define MMC_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MMC_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define MMC_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "mmc", MTX_DEF)
#define MMC_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define MMC_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define MMC_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static void mmc_delayed_attach(void *);
static int mmc_wait_for_cmd(struct mmc_softc *sc, struct mmc_command *cmd,
    int retries);
static int mmc_wait_for_command(struct mmc_softc *sc, uint32_t opcode,
    uint32_t arg, uint32_t flags, uint32_t *resp, int retries);

static void
mmc_ms_delay(int ms)
{
	DELAY(1000 * ms);	/* XXX BAD */
}

static int
mmc_probe(device_t dev)
{

	device_set_desc(dev, "mmc/sd bus");
	return (0);
}

static int
mmc_attach(device_t dev)
{
	struct mmc_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	MMC_LOCK_INIT(sc);

	/* We'll probe and attach our children later, but before / mount */
	sc->config_intrhook.ich_func = mmc_delayed_attach;
	sc->config_intrhook.ich_arg = sc;
	if (config_intrhook_establish(&sc->config_intrhook) != 0)
		device_printf(dev, "config_intrhook_establish failed\n");
	return (0);
}

static int
mmc_detach(device_t dev)
{
	struct mmc_softc *sc = device_get_softc(dev);
	device_t *kids;
	int i, nkid;

	/* kill children [ph33r].  -sorbo */
	if (device_get_children(sc->dev, &kids, &nkid) != 0)
		return 0;
	for (i = 0; i < nkid; i++) {
		device_t kid = kids[i];
		void *ivar = device_get_ivars(kid);
		
		device_detach(kid);
		device_delete_child(sc->dev, kid);
		free(ivar, M_DEVBUF);
	}
	free(kids, M_TEMP);

	MMC_LOCK_DESTROY(sc);

	return 0;
}

static int
mmc_acquire_bus(device_t busdev, device_t dev)
{
	struct mmc_softc *sc;
	int err;
	int rca;

	err = MMCBR_ACQUIRE_HOST(device_get_parent(busdev), dev);
	if (err)
		return (err);
	sc = device_get_softc(busdev);
	MMC_LOCK(sc);
	if (sc->owner)
		panic("mmc: host bridge didn't seralize us.");
	sc->owner = dev;
	MMC_UNLOCK(sc);

	if (busdev != dev) {
		// Keep track of the last rca that we've selected.  If
		// we're asked to do it again, don't.  We never unselect
		// unless the bus code itself wants the mmc bus.
		rca = mmc_get_rca(dev);
		if (sc->last_rca != rca) {
			mmc_wait_for_command(sc, MMC_SELECT_CARD, rca << 16,
			    MMC_RSP_R1 | MMC_CMD_AC, NULL, CMD_RETRIES);
			sc->last_rca = rca;
		}
		// XXX should set bus width here?
	} else {
		// If there's a card selected, stand down.
		if (sc->last_rca != 0) {
			mmc_wait_for_command(sc, MMC_SELECT_CARD, 0,
			    MMC_RSP_R1 | MMC_CMD_AC, NULL, CMD_RETRIES);
			sc->last_rca = 0;
		}
		// XXX should set bus width here?
	}

	return (0);
}

static int
mmc_release_bus(device_t busdev, device_t dev)
{
	struct mmc_softc *sc;
	int err;

	sc = device_get_softc(busdev);

	MMC_LOCK(sc);
	if (!sc->owner)
		panic("mmc: releasing unowned bus.");
	if (sc->owner != dev)
		panic("mmc: you don't own the bus.  game over.");
	MMC_UNLOCK(sc);
	err = MMCBR_RELEASE_HOST(device_get_parent(busdev), dev);
	if (err)
		return (err);
	MMC_LOCK(sc);
	sc->owner = NULL;
	MMC_UNLOCK(sc);
	return (0);
}

static void
mmc_rescan_cards(struct mmc_softc *sc)
{
	/* XXX: Look at the children and see if they respond to status */
}

static uint32_t
mmc_select_vdd(struct mmc_softc *sc, uint32_t ocr)
{
    // XXX
	return ocr;
}

static int
mmc_highest_voltage(uint32_t ocr)
{
	int i;

	for (i = 30; i >= 0; i--)
		if (ocr & (1 << i))
			return i;
	return (-1);
}

static void
mmc_wakeup(struct mmc_request *req)
{
	struct mmc_softc *sc;

//	printf("Wakeup for req %p done_data %p\n", req, req->done_data);
	sc = (struct mmc_softc *)req->done_data;
	MMC_LOCK(sc);
	req->flags |= MMC_REQ_DONE;
	wakeup(req);
	MMC_UNLOCK(sc);
}

static int
mmc_wait_for_req(struct mmc_softc *sc, struct mmc_request *req)
{
	int err;

	req->done = mmc_wakeup;
	req->done_data = sc;
//	printf("Submitting request %p sc %p\n", req, sc);
	MMCBR_REQUEST(device_get_parent(sc->dev), sc->dev, req);
	MMC_LOCK(sc);
	do {
		err = msleep(req, &sc->sc_mtx, PZERO | PCATCH, "mmcreq",
		    hz / 10);
	} while (!(req->flags & MMC_REQ_DONE) && err == EAGAIN);
//	printf("Request %p done with error %d\n", req, err);
	MMC_UNLOCK(sc);
	return (err);
}

static int
mmc_wait_for_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct mmc_softc *sc = device_get_softc(brdev);

	return mmc_wait_for_req(sc, req);
}

static int
mmc_wait_for_cmd(struct mmc_softc *sc, struct mmc_command *cmd, int retries)
{
	struct mmc_request mreq;

	memset(&mreq, 0, sizeof(mreq));
	memset(cmd->resp, 0, sizeof(cmd->resp));
	cmd->retries = retries;
	cmd->data = NULL;
	mreq.cmd = cmd;
//	printf("CMD: %x ARG %x\n", cmd->opcode, cmd->arg);
	mmc_wait_for_req(sc, &mreq);
	return (cmd->error);
}

static int
mmc_wait_for_app_cmd(struct mmc_softc *sc, uint32_t rca,
    struct mmc_command *cmd, int retries)
{
	struct mmc_command appcmd;
	int err = MMC_ERR_NONE, i;

	for (i = 0; i <= retries; i++) {
		appcmd.opcode = MMC_APP_CMD;
		appcmd.arg = rca << 16;
		appcmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		mmc_wait_for_cmd(sc, &appcmd, 0);
		err = appcmd.error;
		if (err != MMC_ERR_NONE)
			continue;
		if (!(appcmd.resp[0] & R1_APP_CMD))
			return MMC_ERR_FAILED;
		mmc_wait_for_cmd(sc, cmd, 0);
		err = cmd->error;
		if (err == MMC_ERR_NONE)
			break;
	}
	return (err);
}

static int
mmc_wait_for_command(struct mmc_softc *sc, uint32_t opcode,
    uint32_t arg, uint32_t flags, uint32_t *resp, int retries)
{
	struct mmc_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = flags;
	err = mmc_wait_for_cmd(sc, &cmd, retries);
	if (err)
		return (err);
	if (cmd.error)
		return (cmd.error);
	if (resp) {
		if (flags & MMC_RSP_136)
			memcpy(resp, cmd.resp, 4 * sizeof(uint32_t));
		else
			*resp = cmd.resp[0];
	}
	return (0);
}

static void
mmc_idle_cards(struct mmc_softc *sc)
{
	device_t dev;
	struct mmc_command cmd;
	
	dev = sc->dev;
	mmcbr_set_chip_select(dev, cs_high);
	mmcbr_update_ios(dev);
	mmc_ms_delay(1);

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_NONE | MMC_CMD_BC;
	mmc_wait_for_cmd(sc, &cmd, 0);
	mmc_ms_delay(1);

	mmcbr_set_chip_select(dev, cs_dontcare);
	mmcbr_update_ios(dev);
	mmc_ms_delay(1);
}

static int
mmc_send_app_op_cond(struct mmc_softc *sc, uint32_t ocr, uint32_t *rocr)
{
	struct mmc_command cmd;
	int err = MMC_ERR_NONE, i;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = ACMD_SD_SEND_OP_COND;
	cmd.arg = ocr;
	cmd.flags = MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 0; i < 100; i++) {
		err = mmc_wait_for_app_cmd(sc, 0, &cmd, CMD_RETRIES);
		if (err != MMC_ERR_NONE)
			break;
		if ((cmd.resp[0] & MMC_OCR_CARD_BUSY) || ocr == 0)
			break;
		err = MMC_ERR_TIMEOUT;
		mmc_ms_delay(10);
	}
	if (rocr && err == MMC_ERR_NONE)
		*rocr = cmd.resp[0];
	return err;
}

static int
mmc_send_op_cond(struct mmc_softc *sc, uint32_t ocr, uint32_t *rocr)
{
	struct mmc_command cmd;
	int err = MMC_ERR_NONE, i;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = MMC_SEND_OP_COND;
	cmd.arg = ocr;
	cmd.flags = MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 0; i < 100; i++) {
		err = mmc_wait_for_cmd(sc, &cmd, CMD_RETRIES);
		if (err != MMC_ERR_NONE)
			break;
		if ((cmd.resp[0] & MMC_OCR_CARD_BUSY) || ocr == 0)
			break;
		err = MMC_ERR_TIMEOUT;
		mmc_ms_delay(10);
	}
	if (rocr && err == MMC_ERR_NONE)
		*rocr = cmd.resp[0];
	return err;
}

static void
mmc_power_up(struct mmc_softc *sc)
{
	device_t dev;

	dev = sc->dev;
	mmcbr_set_vdd(dev, mmc_highest_voltage(mmcbr_get_host_ocr(dev)));
	mmcbr_set_bus_mode(dev, opendrain);
	mmcbr_set_chip_select(dev, cs_dontcare);
	mmcbr_set_bus_width(dev, bus_width_1);
	mmcbr_set_power_mode(dev, power_up);
	mmcbr_set_clock(dev, 0);
	mmcbr_update_ios(dev);
	mmc_ms_delay(1);

	mmcbr_set_clock(dev, mmcbr_get_f_min(sc->dev));
	mmcbr_set_power_mode(dev, power_on);
	mmcbr_update_ios(dev);
	mmc_ms_delay(2);
}

// I wonder if the following is endian safe.
static uint32_t
mmc_get_bits(uint32_t *bits, int start, int size)
{
	const int i = 3 - (start / 32);
	const int shift = start & 31;
	uint32_t retval = bits[i] >> shift;
	if (size + shift > 32)
		retval |= bits[i - 1] << (32 - shift);
	return retval & ((1 << size) - 1);
}

static void
mmc_decode_cid(int is_sd, uint32_t *raw_cid, struct mmc_cid *cid)
{
	int i;

	memset(cid, 0, sizeof(*cid));
	if (is_sd) {
		/* There's no version info, so we take it on faith */
		cid->mid = mmc_get_bits(raw_cid, 120, 8);
		cid->oid = mmc_get_bits(raw_cid, 104, 16);
		for (i = 0; i < 5; i++)
			cid->pnm[i] = mmc_get_bits(raw_cid, 96 - i * 8, 8);
		cid->prv = mmc_get_bits(raw_cid, 56, 8);
		cid->psn = mmc_get_bits(raw_cid, 24, 32);
		cid->mdt_year = mmc_get_bits(raw_cid, 12, 8) + 2001;
		cid->mdt_month = mmc_get_bits(raw_cid, 8, 4);
	} else {
		// XXX write me
		panic("write mmc cid decoder");
	}
}

static const int exp[8] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};
static const int mant[16] = {
	10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80
};
static const int cur_min[8] = {
	500, 1000, 5000, 10000, 25000, 35000, 60000, 100000
};
static const int cur_max[8] = {
	1000, 5000, 10000, 25000, 35000, 45000, 800000, 200000
};

static void
mmc_decode_csd(int is_sd, uint32_t *raw_csd, struct mmc_csd *csd)
{
	int v;
	int m;
	int e;

	memset(csd, 0, sizeof(*csd));
	if (is_sd) {
		csd->csd_structure = v = mmc_get_bits(raw_csd, 126, 2);
		if (v == 0) {
			m = mmc_get_bits(raw_csd, 115, 4);
			e = mmc_get_bits(raw_csd, 112, 3);
			csd->tacc = exp[e] * mant[m] + 9 / 10;
			csd->nsac = mmc_get_bits(raw_csd, 104, 8) * 100;
			m = mmc_get_bits(raw_csd, 99, 4);
			e = mmc_get_bits(raw_csd, 96, 3);
			csd->tran_speed = exp[e] * 10000 * mant[m];
			csd->ccc = mmc_get_bits(raw_csd, 84, 12);
			csd->read_bl_len = 1 << mmc_get_bits(raw_csd, 80, 4);
			csd->read_bl_partial = mmc_get_bits(raw_csd, 79, 1);
			csd->write_blk_misalign = mmc_get_bits(raw_csd, 78, 1);
			csd->read_blk_misalign = mmc_get_bits(raw_csd, 77, 1);
			csd->dsr_imp = mmc_get_bits(raw_csd, 76, 1);
			csd->vdd_r_curr_min = cur_min[mmc_get_bits(raw_csd, 59, 3)];
			csd->vdd_r_curr_max = cur_max[mmc_get_bits(raw_csd, 56, 3)];
			csd->vdd_w_curr_min = cur_min[mmc_get_bits(raw_csd, 53, 3)];
			csd->vdd_w_curr_max = cur_max[mmc_get_bits(raw_csd, 50, 3)];
			m = mmc_get_bits(raw_csd, 62, 12);
			e = mmc_get_bits(raw_csd, 47, 3);
			csd->capacity = ((1 + m) << (e + 2)) * csd->read_bl_len;
			csd->erase_blk_en = mmc_get_bits(raw_csd, 46, 1);
			csd->sector_size = mmc_get_bits(raw_csd, 39, 7);
			csd->wp_grp_size = mmc_get_bits(raw_csd, 32, 7);
			csd->wp_grp_enable = mmc_get_bits(raw_csd, 31, 1);
			csd->r2w_factor = 1 << mmc_get_bits(raw_csd, 26, 3);
			csd->write_bl_len = 1 << mmc_get_bits(raw_csd, 22, 4);
			csd->write_bl_partial = mmc_get_bits(raw_csd, 21, 1);
		} else if (v == 1) {
			panic("Write SDHC CSD parser");
		} else 
			panic("unknown SD CSD version");
	} else {
		panic("Write a MMC CSD parser");
	}
}

static int
mmc_all_send_cid(struct mmc_softc *sc, uint32_t *rawcid)
{
	struct mmc_command cmd;
	int err;

	cmd.opcode = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;
	err = mmc_wait_for_cmd(sc, &cmd, 0);
	memcpy(rawcid, cmd.resp, 4 * sizeof(uint32_t));
	return (err);
}

static int
mmc_send_csd(struct mmc_softc *sc, uint16_t rca, uint32_t *rawcid)
{
	struct mmc_command cmd;
	int err;

	cmd.opcode = MMC_SEND_CSD;
	cmd.arg = rca << 16;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;
	err = mmc_wait_for_cmd(sc, &cmd, 0);
	memcpy(rawcid, cmd.resp, 4 * sizeof(uint32_t));
	return (err);
}

static int
mmc_send_relative_addr(struct mmc_softc *sc, uint32_t *resp)
{
	struct mmc_command cmd;
	int err;

	cmd.opcode = SD_SEND_RELATIVE_ADDR;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R6 | MMC_CMD_BCR;
	err = mmc_wait_for_cmd(sc, &cmd, 0);
	*resp = cmd.resp[0];
	return (err);
}

static void
mmc_discover_cards(struct mmc_softc *sc)
{
	struct mmc_ivars *ivar;
	int err;
	uint32_t resp;
	device_t child;

	while (1) {
		ivar = malloc(sizeof(struct mmc_ivars), M_DEVBUF, M_WAITOK);
		if (!ivar)
			return;
		err = mmc_all_send_cid(sc, ivar->raw_cid);
		if (err == MMC_ERR_TIMEOUT)
			break;
		if (err != MMC_ERR_NONE) {
			printf("Error reading CID %d\n", err);
			break;
		}
		if (mmcbr_get_mode(sc->dev) == mode_sd) {
			ivar->mode = mode_sd;
			mmc_decode_cid(1, ivar->raw_cid, &ivar->cid);
			mmc_send_relative_addr(sc, &resp);
			ivar->rca = resp >> 16;
			// RO check
			mmc_send_csd(sc, ivar->rca, ivar->raw_csd);
			mmc_decode_csd(1, ivar->raw_csd, &ivar->csd);
			printf("SD CARD: %lld bytes\n", (long long)
			    ivar->csd.capacity);
			child = device_add_child(sc->dev, NULL, -1);
			device_set_ivars(child, ivar);
			return;
		}
		panic("Write MMC card code here");
	}
	free(ivar, M_DEVBUF);
}

static void
mmc_go_discovery(struct mmc_softc *sc)
{
	uint32_t ocr;
	device_t dev;

	dev = sc->dev;
	if (mmcbr_get_power_mode(dev) != power_on) {
		// First, try SD modes
		mmcbr_set_mode(dev, mode_sd);
		mmc_power_up(sc);
		mmcbr_set_bus_mode(dev, pushpull);
		mmc_idle_cards(sc);
		if (mmc_send_app_op_cond(sc, 0, &ocr) != MMC_ERR_NONE) {
			// Failed, try MMC
			mmcbr_set_mode(dev, mode_mmc);
			if (mmc_send_op_cond(sc, 0, &ocr) != MMC_ERR_NONE)
				return;	// Failed both, punt! XXX power down?
		}
		mmcbr_set_ocr(dev, mmc_select_vdd(sc, ocr));
		if (mmcbr_get_ocr(dev) != 0)
			mmc_idle_cards(sc);
	} else {
		mmcbr_set_bus_mode(dev, opendrain);
		mmcbr_set_clock(dev, mmcbr_get_f_min(dev));
		mmcbr_update_ios(dev);
		// XXX recompute vdd based on new cards?
	}
	/*
	 * Make sure that we have a mutually agreeable voltage to at least
	 * one card on the bus.
	 */
	if (mmcbr_get_ocr(dev) == 0)
		return;
	/*
	 * Reselect the cards after we've idled them above.
	 */
	if (mmcbr_get_mode(dev) == mode_sd)
		mmc_send_app_op_cond(sc, mmcbr_get_ocr(dev), NULL);
	else
		mmc_send_op_cond(sc, mmcbr_get_ocr(dev), NULL);
	mmc_discover_cards(sc);

	mmcbr_set_bus_mode(dev, pushpull);
	mmcbr_update_ios(dev);
	bus_generic_attach(dev);
//	mmc_update_children_sysctl(dev);
}

static int
mmc_calculate_clock(struct mmc_softc *sc)
{
	int max_dtr = 0;
	int nkid, i, f_min, f_max;
	device_t *kids;
	
	f_min = mmcbr_get_f_min(sc->dev);
	f_max = mmcbr_get_f_max(sc->dev);
	max_dtr = f_max;
	if (device_get_children(sc->dev, &kids, &nkid) != 0)
		panic("can't get children");
	for (i = 0; i < nkid; i++)
		if (mmc_get_tran_speed(kids[i]) < max_dtr)
			max_dtr = mmc_get_tran_speed(kids[i]);
	free(kids, M_TEMP);
	device_printf(sc->dev, "setting transfer rate to %d.%03dMHz\n",
	    max_dtr / 1000000, (max_dtr / 1000) % 1000);
	return max_dtr;
}

static void
mmc_scan(struct mmc_softc *sc)
{
	device_t dev;

	dev = sc->dev;
	mmc_acquire_bus(dev, dev);

	if (mmcbr_get_power_mode(dev) == power_on)
		mmc_rescan_cards(sc);
	mmc_go_discovery(sc);
	mmcbr_set_clock(dev, mmc_calculate_clock(sc));
	mmcbr_update_ios(dev);

	mmc_release_bus(dev, dev);
	// XXX probe/attach/detach children?
}

static int
mmc_read_ivar(device_t bus, device_t child, int which, u_char *result)
{
	struct mmc_ivars *ivar = device_get_ivars(child);

	switch (which) {
	default:
		return (EINVAL);
	case MMC_IVAR_DSR_IMP:
		*(int *)result = ivar->csd.dsr_imp;
		break;
	case MMC_IVAR_MEDIA_SIZE:
		*(int *)result = ivar->csd.capacity;
		break;
	case MMC_IVAR_RCA:
		*(int *)result = ivar->rca;
		break;
	case MMC_IVAR_SECTOR_SIZE:
		*(int *)result = 512;
		break;
	case MMC_IVAR_TRAN_SPEED:
		*(int *)result = ivar->csd.tran_speed;
		break;
	}
	return (0);
}

static int
mmc_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	// None are writable ATM
	switch (which) {
	default:
		return (EINVAL);
	}
	return (0);
}


static void
mmc_delayed_attach(void *xsc)
{
	struct mmc_softc *sc = xsc;
	
	mmc_scan(sc);
	config_intrhook_disestablish(&sc->config_intrhook);
}

static device_method_t mmc_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, mmc_probe),
	DEVMETHOD(device_attach, mmc_attach),
	DEVMETHOD(device_detach, mmc_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar, mmc_read_ivar),
	DEVMETHOD(bus_write_ivar, mmc_write_ivar),

	/* MMC Bus interface */
	DEVMETHOD(mmcbus_wait_for_request, mmc_wait_for_request),
	DEVMETHOD(mmcbus_acquire_bus, mmc_acquire_bus),
	DEVMETHOD(mmcbus_release_bus, mmc_release_bus),

	{0, 0},
};

static driver_t mmc_driver = {
	"mmc",
	mmc_methods,
	sizeof(struct mmc_softc),
};
static devclass_t mmc_devclass;


DRIVER_MODULE(mmc, at91_mci, mmc_driver, mmc_devclass, 0, 0);
DRIVER_MODULE(mmc, sdh, mmc_driver, mmc_devclass, 0, 0);
