/*-
 * Copyright (c) 2018 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by BAE Systems, the University of Cambridge
 * Computer Laboratory, and Memorial University under DARPA/AFRL contract
 * FA8650-15-C-7558 ("CADETS"), as part of the DARPA Transparent Computing
 * (TC) research program.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kthread.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "qcom_gcc_var.h"
#include "qcom_gcc_msm8916.h"

#define	GCC_QDSS_BCR			0x29000
#define	 GCC_QDSS_BCR_BLK_ARES		(1 << 0) /* Async software reset. */
#define	GCC_QDSS_CFG_AHB_CBCR		0x29008
#define	 AHB_CBCR_CLK_ENABLE		(1 << 0) /* AHB clk branch ctrl */
#define	GCC_QDSS_ETR_USB_CBCR		0x29028
#define	 ETR_USB_CBCR_CLK_ENABLE	(1 << 0) /* ETR USB clk branch ctrl */
#define	GCC_QDSS_DAP_CBCR		0x29084
#define	 DAP_CBCR_CLK_ENABLE		(1 << 0) /* DAP clk branch ctrl */

/*
 * Qualcomm Debug Subsystem (QDSS)
 * block enabling routine.
 */
static void
qcom_msm8916_qdss_enable(struct qcom_gcc_softc *sc)
{

	/* Put QDSS block to reset */
	bus_write_4(sc->reg, GCC_QDSS_BCR, GCC_QDSS_BCR_BLK_ARES);

	/* Enable AHB clock branch */
	bus_write_4(sc->reg, GCC_QDSS_CFG_AHB_CBCR, AHB_CBCR_CLK_ENABLE);

	/* Enable DAP clock branch */
	bus_write_4(sc->reg, GCC_QDSS_DAP_CBCR, DAP_CBCR_CLK_ENABLE);

	/* Enable ETR USB clock branch */
	bus_write_4(sc->reg, GCC_QDSS_ETR_USB_CBCR, ETR_USB_CBCR_CLK_ENABLE);

	/* Out of reset */
	bus_write_4(sc->reg, GCC_QDSS_BCR, 0);
}

void
qcom_gcc_msm8916_clock_setup(struct qcom_gcc_softc *sc)
{
	qcom_msm8916_qdss_enable(sc);
}
