/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DEV_MMC_HOST_DWMMC_VAR_H
#define DEV_MMC_HOST_DWMMC_VAR_H

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>

#include "opt_mmccam.h"

#include <cam/mmc/mmc_sim.h>

enum {
	HWTYPE_NONE,
	HWTYPE_ALTERA,
	HWTYPE_EXYNOS,
	HWTYPE_HISILICON,
	HWTYPE_ROCKCHIP,
};

struct dwmmc_softc {
	struct resource		*res[2];
	device_t		dev;
	void			*intr_cookie;
	struct mmc_host		host;
	struct mmc_helper	mmc_helper;
	struct mtx		sc_mtx;
#ifdef MMCCAM
	union ccb *		ccb;
	struct mmc_sim		mmc_sim;
#else
	struct mmc_request	*req;
#endif
	struct mmc_command	*curcmd;
	uint32_t		flags;
	uint32_t		hwtype;
	uint32_t		use_auto_stop;
	uint32_t		use_pio;
	device_t		child;
	struct task		card_task;	/* Card presence check task */
	struct timeout_task	card_delayed_task;/* Card insert delayed task */

	int			(*update_ios)(struct dwmmc_softc *sc, struct mmc_ios *ios);

	bus_dma_tag_t		desc_tag;
	bus_dmamap_t		desc_map;
	struct idmac_desc	*desc_ring;
	bus_addr_t		desc_ring_paddr;
	bus_dma_tag_t		buf_tag;
	bus_dmamap_t		buf_map;

	uint32_t		bus_busy;
	uint32_t		dto_rcvd;
	uint32_t		acd_rcvd;
	uint32_t		cmd_done;
	uint64_t		bus_hz;
	uint32_t		fifo_depth;
	uint32_t		num_slots;
	uint32_t		sdr_timing;
	uint32_t		ddr_timing;

	clk_t			biu;
	clk_t			ciu;
	hwreset_t		hwreset;
	regulator_t		vmmc;
	regulator_t		vqmmc;
};

DECLARE_CLASS(dwmmc_driver);

int dwmmc_attach(device_t);
int dwmmc_detach(device_t);

#endif
