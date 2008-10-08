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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/endian.h>

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
	uint32_t raw_scr[2];	/* Raw bits of the SCR */
	uint8_t raw_ext_csd[512];	/* Raw bits of the EXT_CSD */
	uint16_t rca;
	enum mmc_card_mode mode;
	struct mmc_cid cid;	/* cid decoded */
	struct mmc_csd csd;	/* csd decoded */
	struct mmc_scr scr;	/* scr decoded */
	u_char read_only;	/* True when the device is read-only */
	u_char bus_width;	/* Bus width to use */
	u_char timing;		/* Bus timing support */
	u_char high_cap;	/* High Capacity card */
	uint32_t tran_speed;	/* Max speed in normal mode */
	uint32_t hs_tran_speed;	/* Max speed in high speed mode */
};

#define CMD_RETRIES	3

/* bus entry points */
static int mmc_probe(device_t dev);
static int mmc_attach(device_t dev);
static int mmc_detach(device_t dev);

#define MMC_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MMC_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define MMC_LOCK_INIT(_sc)					\
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev),	\
	    "mmc", MTX_DEF)
#define MMC_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define MMC_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define MMC_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static void mmc_delayed_attach(void *);
static void mmc_power_down(struct mmc_softc *sc);
static int mmc_wait_for_cmd(struct mmc_softc *sc, struct mmc_command *cmd,
    int retries);
static int mmc_wait_for_command(struct mmc_softc *sc, uint32_t opcode,
    uint32_t arg, uint32_t flags, uint32_t *resp, int retries);
static int mmc_select_card(struct mmc_softc *sc, uint16_t rca);
static int mmc_set_bus_width(struct mmc_softc *sc, uint16_t rca, int width);
static int mmc_app_send_scr(struct mmc_softc *sc, uint16_t rca, uint32_t *rawscr);
static void mmc_app_decode_scr(uint32_t *raw_scr, struct mmc_scr *scr);
static int mmc_send_ext_csd(struct mmc_softc *sc, uint8_t *rawextcsd);

static void
mmc_ms_delay(int ms)
{
	DELAY(1000 * ms);	/* XXX BAD */
}

static int
mmc_probe(device_t dev)
{

	device_set_desc(dev, "MMC/SD bus");
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
		return (0);
	for (i = 0; i < nkid; i++) {
		device_t kid = kids[i];
		void *ivar = device_get_ivars(kid);
		
		device_detach(kid);
		device_delete_child(sc->dev, kid);
		free(ivar, M_DEVBUF);
	}
	free(kids, M_TEMP);
	mmc_power_down(sc);

	MMC_LOCK_DESTROY(sc);

	return (0);
}

static int
mmc_acquire_bus(device_t busdev, device_t dev)
{
	struct mmc_softc *sc;
	struct mmc_ivars *ivar;
	int err;
	int rca;

	err = MMCBR_ACQUIRE_HOST(device_get_parent(busdev), busdev);
	if (err)
		return (err);
	sc = device_get_softc(busdev);
	MMC_LOCK(sc);
	if (sc->owner)
		panic("mmc: host bridge didn't seralize us.");
	sc->owner = dev;
	MMC_UNLOCK(sc);

	if (busdev != dev) {
		/*
		 * Keep track of the last rca that we've selected.  If
		 * we're asked to do it again, don't.  We never
		 * unselect unless the bus code itself wants the mmc
		 * bus, and constantly reselecting causes problems.
		 */
		rca = mmc_get_rca(dev);
		if (sc->last_rca != rca) {
			mmc_select_card(sc, rca);
			sc->last_rca = rca;
			/* Prepare bus width for the new card. */
			ivar = device_get_ivars(dev);
			device_printf(busdev,
			    "setting bus width to %d bits\n",
			    (ivar->bus_width == bus_width_4)?4:
			    (ivar->bus_width == bus_width_8)?8:1);
			mmc_set_bus_width(sc, rca, ivar->bus_width);
			mmcbr_set_bus_width(busdev, ivar->bus_width);
			mmcbr_update_ios(busdev);
		}
	} else {
		/*
		 * If there's a card selected, stand down.
		 */
		if (sc->last_rca != 0) {
			mmc_select_card(sc, 0);
			sc->last_rca = 0;
		}
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
	err = MMCBR_RELEASE_HOST(device_get_parent(busdev), busdev);
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

	return (ocr & MMC_OCR_VOLTAGE);
}

static int
mmc_highest_voltage(uint32_t ocr)
{
	int i;

	for (i = 30; i >= 0; i--)
		if (ocr & (1 << i))
			return (i);
	return (-1);
}

static void
mmc_wakeup(struct mmc_request *req)
{
	struct mmc_softc *sc;

/*	printf("Wakeup for req %p done_data %p\n", req, req->done_data); */
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
/*	printf("Submitting request %p sc %p\n", req, sc); */
	MMCBR_REQUEST(device_get_parent(sc->dev), sc->dev, req);
	MMC_LOCK(sc);
	do {
		err = msleep(req, &sc->sc_mtx, PZERO | PCATCH, "mmcreq",
		    hz / 10);
	} while (!(req->flags & MMC_REQ_DONE) && err == EAGAIN);
/*	printf("Request %p done with error %d\n", req, err); */
	MMC_UNLOCK(sc);
	return (err);
}

static int
mmc_wait_for_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct mmc_softc *sc = device_get_softc(brdev);

	return (mmc_wait_for_req(sc, req));
}

static int
mmc_wait_for_cmd(struct mmc_softc *sc, struct mmc_command *cmd, int retries)
{
	struct mmc_request mreq;

	memset(&mreq, 0, sizeof(mreq));
	memset(cmd->resp, 0, sizeof(cmd->resp));
	cmd->retries = retries;
	mreq.cmd = cmd;
/*	printf("CMD: %x ARG %x\n", cmd->opcode, cmd->arg); */
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
		appcmd.data = NULL;
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
	cmd.data = NULL;
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
	cmd.data = NULL;
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
	cmd.data = NULL;

	for (i = 0; i < 100; i++) {
		err = mmc_wait_for_app_cmd(sc, 0, &cmd, CMD_RETRIES);
		if (err != MMC_ERR_NONE)
			break;
		if ((cmd.resp[0] & MMC_OCR_CARD_BUSY) ||
		    (ocr & MMC_OCR_VOLTAGE) == 0)
			break;
		err = MMC_ERR_TIMEOUT;
		mmc_ms_delay(10);
	}
	if (rocr && err == MMC_ERR_NONE)
		*rocr = cmd.resp[0];
	return (err);
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
	cmd.data = NULL;

	for (i = 0; i < 100; i++) {
		err = mmc_wait_for_cmd(sc, &cmd, CMD_RETRIES);
		if (err != MMC_ERR_NONE)
			break;
		if ((cmd.resp[0] & MMC_OCR_CARD_BUSY) ||
		    (ocr & MMC_OCR_VOLTAGE) == 0)
			break;
		err = MMC_ERR_TIMEOUT;
		mmc_ms_delay(10);
	}
	if (rocr && err == MMC_ERR_NONE)
		*rocr = cmd.resp[0];
	return (err);
}

static int
mmc_send_if_cond(struct mmc_softc *sc, uint8_t vhs)
{
	struct mmc_command cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = SD_SEND_IF_COND;
	cmd.arg = (vhs << 8) + 0xAA;
	cmd.flags = MMC_RSP_R7 | MMC_CMD_BCR;
	cmd.data = NULL;

	err = mmc_wait_for_cmd(sc, &cmd, CMD_RETRIES);
	return (err);
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
	mmcbr_set_timing(dev, bus_timing_normal);
	mmcbr_set_power_mode(dev, power_on);
	mmcbr_update_ios(dev);
	mmc_ms_delay(2);
}

static void
mmc_power_down(struct mmc_softc *sc)
{
	device_t dev = sc->dev;

	mmcbr_set_bus_mode(dev, opendrain);
	mmcbr_set_chip_select(dev, cs_dontcare);
	mmcbr_set_bus_width(dev, bus_width_1);
	mmcbr_set_power_mode(dev, power_off);
	mmcbr_set_clock(dev, 0);
	mmcbr_set_timing(dev, bus_timing_normal);
	mmcbr_update_ios(dev);
}

static int
mmc_select_card(struct mmc_softc *sc, uint16_t rca)
{
	return (mmc_wait_for_command(sc, MMC_SELECT_CARD, ((uint32_t)rca) << 16,
	    MMC_RSP_R1B | MMC_CMD_AC, NULL, CMD_RETRIES));
}

static int
mmc_switch(struct mmc_softc *sc, uint8_t set, uint8_t index, uint8_t value)
{
	struct mmc_command cmd;
	int err;

	cmd.opcode = MMC_SWITCH_FUNC;
	cmd.arg = (MMC_SWITCH_FUNC_WR << 24) |
	    (index << 16) |
	    (value << 8) |
	    set;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	cmd.data = NULL;
	err = mmc_wait_for_cmd(sc, &cmd, 0);
	return (err);
}

static int
mmc_sd_switch(struct mmc_softc *sc, uint8_t mode, uint8_t grp, uint8_t value, uint8_t *res)
{
	int err;
	struct mmc_command cmd;
	struct mmc_data data;

	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	memset(res, 0, 64);
	cmd.opcode = SD_SWITCH_FUNC;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.arg = mode << 31;
	cmd.arg |= 0x00FFFFFF;
	cmd.arg &= ~(0xF << (grp * 4));
	cmd.arg |= value << (grp * 4);
	cmd.data = &data;

	data.data = res;
	data.len = 64;
	data.flags = MMC_DATA_READ;

	err = mmc_wait_for_cmd(sc, &cmd, CMD_RETRIES);
	return (err);
}

static int
mmc_set_bus_width(struct mmc_softc *sc, uint16_t rca, int width)
{
	int err;

	if (mmcbr_get_mode(sc->dev) == mode_sd) {
		struct mmc_command cmd;

		memset(&cmd, 0, sizeof(struct mmc_command));
		cmd.opcode = ACMD_SET_BUS_WIDTH;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		switch (width) {
		case bus_width_1:
			cmd.arg = SD_BUS_WIDTH_1;
			break;
		case bus_width_4:
			cmd.arg = SD_BUS_WIDTH_4;
			break;
		default:
			return (MMC_ERR_INVALID);
		}
		err = mmc_wait_for_app_cmd(sc, rca, &cmd, CMD_RETRIES);
	} else {
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
			return (MMC_ERR_INVALID);
		}
		err = mmc_switch(sc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, value);
	}
	return (err);
}

static int
mmc_set_timing(struct mmc_softc *sc, int timing)
{
	int err;
	uint8_t	value;

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
	if (mmcbr_get_mode(sc->dev) == mode_sd) {
		u_char switch_res[64];

		err = mmc_sd_switch(sc, 1, 0, value, switch_res);
	} else {
		err = mmc_switch(sc, EXT_CSD_CMD_SET_NORMAL,
		    EXT_CSD_HS_TIMING, value);
	}
	return (err);
}

static int
mmc_test_bus_width(struct mmc_softc *sc)
{
	struct mmc_command cmd;
	struct mmc_data data;
	int err;
	uint8_t buf[8];
	uint8_t	p8[8] =   { 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	uint8_t	p8ok[8] = { 0xAA, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	uint8_t	p4[4] =   { 0x5A, 0x00, 0x00, 0x00, };
	uint8_t	p4ok[4] = { 0xA5, 0x00, 0x00, 0x00, };

	if (mmcbr_get_caps(sc->dev) & MMC_CAP_8_BIT_DATA) {
		mmcbr_set_bus_width(sc->dev, bus_width_8);
		mmcbr_update_ios(sc->dev);

		cmd.opcode = MMC_BUSTEST_W;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.data = &data;

		data.data = p8;
		data.len = 8;
		data.flags = MMC_DATA_WRITE;
		mmc_wait_for_cmd(sc, &cmd, 0);
		
		cmd.opcode = MMC_BUSTEST_R;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.data = &data;

		data.data = buf;
		data.len = 8;
		data.flags = MMC_DATA_READ;
		err = mmc_wait_for_cmd(sc, &cmd, 0);
		
		mmcbr_set_bus_width(sc->dev, bus_width_1);
		mmcbr_update_ios(sc->dev);

		if (err == MMC_ERR_NONE && memcmp(buf, p8ok, 8) == 0)
			return (bus_width_8);
	}

	if (mmcbr_get_caps(sc->dev) & MMC_CAP_4_BIT_DATA) {
		mmcbr_set_bus_width(sc->dev, bus_width_4);
		mmcbr_update_ios(sc->dev);

		cmd.opcode = MMC_BUSTEST_W;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.data = &data;

		data.data = p4;
		data.len = 4;
		data.flags = MMC_DATA_WRITE;
		mmc_wait_for_cmd(sc, &cmd, 0);
		
		cmd.opcode = MMC_BUSTEST_R;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.data = &data;

		data.data = buf;
		data.len = 4;
		data.flags = MMC_DATA_READ;
		err = mmc_wait_for_cmd(sc, &cmd, 0);

		mmcbr_set_bus_width(sc->dev, bus_width_1);
		mmcbr_update_ios(sc->dev);

		if (err == MMC_ERR_NONE && memcmp(buf, p4ok, 4) == 0)
			return (bus_width_4);
	}
	return (bus_width_1);
}

static uint32_t
mmc_get_bits(uint32_t *bits, int start, int size)
{
	const int i = 3 - (start / 32);
	const int shift = start & 31;
	uint32_t retval = bits[i] >> shift;
	if (size + shift > 32)
		retval |= bits[i - 1] << (32 - shift);
	return (retval & ((1 << size) - 1));
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
		cid->mid = mmc_get_bits(raw_cid, 120, 8);
		cid->oid = mmc_get_bits(raw_cid, 104, 8);
		for (i = 0; i < 6; i++)
			cid->pnm[i] = mmc_get_bits(raw_cid, 96 - i * 8, 8);
		cid->prv = mmc_get_bits(raw_cid, 48, 8);
		cid->psn = mmc_get_bits(raw_cid, 16, 32);
		cid->mdt_month = mmc_get_bits(raw_cid, 12, 4);
		cid->mdt_year = mmc_get_bits(raw_cid, 8, 4) + 1997;
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
			csd->capacity = ((uint64_t)mmc_get_bits(raw_csd, 48, 22) + 1) *
			    512 * 1024;
			csd->erase_blk_en = mmc_get_bits(raw_csd, 46, 1);
			csd->sector_size = mmc_get_bits(raw_csd, 39, 7);
			csd->wp_grp_size = mmc_get_bits(raw_csd, 32, 7);
			csd->wp_grp_enable = mmc_get_bits(raw_csd, 31, 1);
			csd->r2w_factor = 1 << mmc_get_bits(raw_csd, 26, 3);
			csd->write_bl_len = 1 << mmc_get_bits(raw_csd, 22, 4);
			csd->write_bl_partial = mmc_get_bits(raw_csd, 21, 1);
		} else 
			panic("unknown SD CSD version");
	} else {
		csd->csd_structure = mmc_get_bits(raw_csd, 126, 2);
		csd->spec_vers = mmc_get_bits(raw_csd, 122, 4);
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
//		csd->erase_blk_en = mmc_get_bits(raw_csd, 46, 1);
//		csd->sector_size = mmc_get_bits(raw_csd, 39, 7);
		csd->wp_grp_size = mmc_get_bits(raw_csd, 32, 5);
		csd->wp_grp_enable = mmc_get_bits(raw_csd, 31, 1);
		csd->r2w_factor = 1 << mmc_get_bits(raw_csd, 26, 3);
		csd->write_bl_len = 1 << mmc_get_bits(raw_csd, 22, 4);
		csd->write_bl_partial = mmc_get_bits(raw_csd, 21, 1);
	}
}

static void
mmc_app_decode_scr(uint32_t *raw_scr, struct mmc_scr *scr)
{
	unsigned int scr_struct;
	uint32_t tmp[4];

	tmp[3] = raw_scr[1];
	tmp[2] = raw_scr[0];

	memset(scr, 0, sizeof(*scr));
	
	scr_struct = mmc_get_bits(tmp, 60, 4);
	if (scr_struct != 0) {
		printf("Unrecognised SCR structure version %d\n",
		    scr_struct);
		return;
	}
	scr->sda_vsn = mmc_get_bits(tmp, 56, 4);
	scr->bus_widths = mmc_get_bits(tmp, 48, 4);
}

static int
mmc_all_send_cid(struct mmc_softc *sc, uint32_t *rawcid)
{
	struct mmc_command cmd;
	int err;

	cmd.opcode = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;
	cmd.data = NULL;
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
	cmd.data = NULL;
	err = mmc_wait_for_cmd(sc, &cmd, 0);
	memcpy(rawcid, cmd.resp, 4 * sizeof(uint32_t));
	return (err);
}

static int
mmc_app_send_scr(struct mmc_softc *sc, uint16_t rca, uint32_t *rawscr)
{
	int err;
	struct mmc_command cmd;
	struct mmc_data data;

	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	memset(rawscr, 0, 8);
	cmd.opcode = ACMD_SEND_SCR;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.arg = 0;
	cmd.data = &data;

	data.data = rawscr;
	data.len = 8;
	data.flags = MMC_DATA_READ;

	err = mmc_wait_for_app_cmd(sc, rca, &cmd, CMD_RETRIES);
	rawscr[0] = be32toh(rawscr[0]);
	rawscr[1] = be32toh(rawscr[1]);
	return (err);
}

static int
mmc_send_ext_csd(struct mmc_softc *sc, uint8_t *rawextcsd)
{
	int err;
	struct mmc_command cmd;
	struct mmc_data data;

	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	memset(rawextcsd, 0, 512);
	cmd.opcode = MMC_SEND_EXT_CSD;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.arg = 0;
	cmd.data = &data;

	data.data = rawextcsd;
	data.len = 512;
	data.flags = MMC_DATA_READ;

	err = mmc_wait_for_cmd(sc, &cmd, CMD_RETRIES);
	return (err);
}

static int
mmc_set_relative_addr(struct mmc_softc *sc, uint16_t resp)
{
	struct mmc_command cmd;
	int err;

	cmd.opcode = MMC_SET_RELATIVE_ADDR;
	cmd.arg = resp << 16;
	cmd.flags = MMC_RSP_R6 | MMC_CMD_BCR;
	cmd.data = NULL;
	err = mmc_wait_for_cmd(sc, &cmd, 0);
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
	cmd.data = NULL;
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
	uint16_t rca = 2;
	u_char switch_res[64];

	while (1) {
		ivar = malloc(sizeof(struct mmc_ivars), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		if (!ivar)
			return;
		err = mmc_all_send_cid(sc, ivar->raw_cid);
		if (err == MMC_ERR_TIMEOUT)
			break;
		if (err != MMC_ERR_NONE) {
			device_printf(sc->dev, "Error reading CID %d\n", err);
			break;
		}
		if (mmcbr_get_ro(sc->dev))
			ivar->read_only = 1;
		ivar->bus_width = bus_width_1;
		ivar->mode = mmcbr_get_mode(sc->dev);
		if (ivar->mode == mode_sd) {
			mmc_decode_cid(1, ivar->raw_cid, &ivar->cid);
			mmc_send_relative_addr(sc, &resp);
			ivar->rca = resp >> 16;
			/* Get card CSD. */
			mmc_send_csd(sc, ivar->rca, ivar->raw_csd);
			mmc_decode_csd(1, ivar->raw_csd, &ivar->csd);
			if (ivar->csd.csd_structure > 0)
				ivar->high_cap = 1;
			ivar->tran_speed = ivar->csd.tran_speed;
			/* Get card SCR. Card must be selected to fetch it. */
			mmc_select_card(sc, ivar->rca);
			mmc_app_send_scr(sc, ivar->rca, ivar->raw_scr);
			mmc_app_decode_scr(ivar->raw_scr, &ivar->scr);
			/* Get card switch capabilities. */
			if ((ivar->scr.sda_vsn >= 1) &&
			    (ivar->csd.ccc & (1<<10))) {
				mmc_sd_switch(sc, 0, 0, 0xF, switch_res);
				if (switch_res[13] & 2) {
					ivar->timing = bus_timing_hs;
					ivar->hs_tran_speed = 50000000;
				}
			}
			mmc_select_card(sc, 0);
			/* Find max supported bus width. */
			if ((mmcbr_get_caps(sc->dev) & MMC_CAP_4_BIT_DATA) &&
			    (ivar->scr.bus_widths & SD_SCR_BUS_WIDTH_4))
				ivar->bus_width = bus_width_4;
			/* Add device. */
			child = device_add_child(sc->dev, NULL, -1);
			device_set_ivars(child, ivar);
			return;
		}
		mmc_decode_cid(0, ivar->raw_cid, &ivar->cid);
		ivar->rca = rca++;
		mmc_set_relative_addr(sc, ivar->rca);
		/* Get card CSD. */
		mmc_send_csd(sc, ivar->rca, ivar->raw_csd);
		mmc_decode_csd(0, ivar->raw_csd, &ivar->csd);
		ivar->tran_speed = ivar->csd.tran_speed;
		/* Only MMC >= 4.x cards support EXT_CSD. */
		if (ivar->csd.spec_vers >= 4) {
			/* Card must be selected to fetch EXT_CSD. */
			mmc_select_card(sc, ivar->rca);
			mmc_send_ext_csd(sc, ivar->raw_ext_csd);
			/* Get card speed in high speed mode. */
			ivar->timing = bus_timing_hs;
			if (((uint8_t *)(ivar->raw_ext_csd))[EXT_CSD_CARD_TYPE]
			    & EXT_CSD_CARD_TYPE_52)
				ivar->hs_tran_speed = 52000000;
			else if (((uint8_t *)(ivar->raw_ext_csd))[EXT_CSD_CARD_TYPE]
			    & EXT_CSD_CARD_TYPE_26)
				ivar->hs_tran_speed = 26000000;
			else
				ivar->hs_tran_speed = ivar->tran_speed;
			/* Find max supported bus width. */
			ivar->bus_width = mmc_test_bus_width(sc);
			mmc_select_card(sc, 0);
		} else {
			ivar->bus_width = bus_width_1;
			ivar->timing = bus_timing_normal;
		}
		/* Add device. */
		child = device_add_child(sc->dev, NULL, -1);
		device_set_ivars(child, ivar);
	}
	free(ivar, M_DEVBUF);
}

static void
mmc_go_discovery(struct mmc_softc *sc)
{
	uint32_t ocr;
	device_t dev;
	int err;

	dev = sc->dev;
	if (mmcbr_get_power_mode(dev) != power_on) {
		/*
		 * First, try SD modes
		 */
		mmcbr_set_mode(dev, mode_sd);
		mmc_power_up(sc);
		mmcbr_set_bus_mode(dev, pushpull);
		mmc_idle_cards(sc);
		err = mmc_send_if_cond(sc, 1);
		if (mmc_send_app_op_cond(sc, err?0:MMC_OCR_CCS, &ocr) !=
		    MMC_ERR_NONE) {
			/*
			 * Failed, try MMC
			 */
			mmcbr_set_mode(dev, mode_mmc);
			if (mmc_send_op_cond(sc, 0, &ocr) != MMC_ERR_NONE)
				return;	/* Failed both, punt! XXX powerdown? */
		}
		mmcbr_set_ocr(dev, mmc_select_vdd(sc, ocr));
		if (mmcbr_get_ocr(dev) != 0)
			mmc_idle_cards(sc);
	} else {
		mmcbr_set_bus_mode(dev, opendrain);
		mmcbr_set_clock(dev, mmcbr_get_f_min(dev));
		mmcbr_update_ios(dev);
		/* XXX recompute vdd based on new cards? */
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
	if (mmcbr_get_mode(dev) == mode_sd) {
		err = mmc_send_if_cond(sc, 1);
		mmc_send_app_op_cond(sc,
		    (err?0:MMC_OCR_CCS)|mmcbr_get_ocr(dev), NULL);
	} else
		mmc_send_op_cond(sc, mmcbr_get_ocr(dev), NULL);
	mmc_discover_cards(sc);

	mmcbr_set_bus_mode(dev, pushpull);
	mmcbr_update_ios(dev);
	bus_generic_attach(dev);
/*	mmc_update_children_sysctl(dev);*/
}

static int
mmc_calculate_clock(struct mmc_softc *sc)
{
	int max_dtr, max_hs_dtr, max_timing;
	int nkid, i, f_min, f_max;
	device_t *kids;
	struct mmc_ivars *ivar;
	
	f_min = mmcbr_get_f_min(sc->dev);
	f_max = mmcbr_get_f_max(sc->dev);
	max_dtr = max_hs_dtr = f_max;
	if ((mmcbr_get_caps(sc->dev) & MMC_CAP_HSPEED))
		max_timing = bus_timing_hs;
	else
		max_timing = bus_timing_normal;
	if (device_get_children(sc->dev, &kids, &nkid) != 0)
		panic("can't get children");
	for (i = 0; i < nkid; i++) {
		ivar = device_get_ivars(kids[i]);
		if (ivar->timing < max_timing)
			max_timing = ivar->timing;
		if (ivar->tran_speed < max_dtr)
			max_dtr = ivar->tran_speed;
		if (ivar->hs_tran_speed < max_dtr)
			max_hs_dtr = ivar->hs_tran_speed;
	}
	for (i = 0; i < nkid; i++) {
		ivar = device_get_ivars(kids[i]);
		if (ivar->timing == bus_timing_normal)
			continue;
		mmc_select_card(sc, ivar->rca);
		mmc_set_timing(sc, max_timing);
	}
	mmc_select_card(sc, 0);
	free(kids, M_TEMP);
	if (max_timing == bus_timing_hs)
		max_dtr = max_hs_dtr;
	device_printf(sc->dev, "setting transfer rate to %d.%03dMHz%s\n",
	    max_dtr / 1000000, (max_dtr / 1000) % 1000,
	    (max_timing == bus_timing_hs)?" with high speed timing":"");
	mmcbr_set_timing(sc->dev, max_timing);
	mmcbr_set_clock(sc->dev, max_dtr);
	mmcbr_update_ios(sc->dev);
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
	mmc_calculate_clock(sc);

	mmc_release_bus(dev, dev);
	/* XXX probe/attach/detach children? */
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
		*(off_t *)result = ivar->csd.capacity / MMC_SECTOR_SIZE;
		break;
	case MMC_IVAR_RCA:
		*(int *)result = ivar->rca;
		break;
	case MMC_IVAR_SECTOR_SIZE:
		*(int *)result = MMC_SECTOR_SIZE;
		break;
	case MMC_IVAR_TRAN_SPEED:
		*(int *)result = ivar->csd.tran_speed;
		break;
	case MMC_IVAR_READ_ONLY:
		*(int *)result = ivar->read_only;
		break;
	case MMC_IVAR_HIGH_CAP:
		*(int *)result = ivar->high_cap;
		break;
	}
	return (0);
}

static int
mmc_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	/*
	 * None are writable ATM
	 */
	return (EINVAL);
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
DRIVER_MODULE(mmc, sdhci, mmc_driver, mmc_devclass, 0, 0);
