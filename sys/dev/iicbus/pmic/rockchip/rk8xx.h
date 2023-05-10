/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Emmanuel Vadot <manu@FreeBSD.org>
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

#ifndef _RK8XX_H_
#define	_RK8XX_H_

#include <sys/kernel.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

enum rk_pmic_type {
	RK805 = 1,
	RK808,
	RK809,
	RK817,
};

struct rk8xx_regdef {
	intptr_t		id;
	char			*name;
	uint8_t			enable_reg;
	uint8_t			enable_mask;
	uint8_t			voltage_reg;
	uint8_t			voltage_mask;
	int			voltage_min;
	int			voltage_max;
	int			voltage_min2;
	int			voltage_max2;
	int			voltage_step;
	int			voltage_step2;
	int			voltage_nstep;
};

struct rk8xx_reg_sc {
	struct regnode		*regnode;
	device_t		base_dev;
	struct rk8xx_regdef	*def;
	phandle_t		xref;
	struct regnode_std_param *param;
};

struct reg_list {
	TAILQ_ENTRY(reg_list)	next;
	struct rk8xx_reg_sc	*reg;
};

struct rk8xx_rtc_reg {
	uint8_t	secs;
	uint8_t	secs_mask;
	uint8_t	minutes;
	uint8_t	minutes_mask;
	uint8_t	hours;
	uint8_t	hours_mask;
	uint8_t	days;
	uint8_t	days_mask;
	uint8_t	months;
	uint8_t	months_mask;
	uint8_t	years;
	uint8_t	weeks;
	uint8_t	weeks_mask;
	uint8_t	ctrl;
	uint8_t	ctrl_stop_mask;
	uint8_t	ctrl_ampm_mask;
	uint8_t	ctrl_gettime_mask;
	uint8_t	ctrl_readsel_mask;
};

struct rk8xx_dev_ctrl {
	uint8_t	dev_ctrl_reg;
	uint8_t	pwr_off_mask;
	uint8_t	pwr_rst_mask;
};

struct rk8xx_softc {
	device_t		dev;
	struct mtx		mtx;
	struct resource *	res[1];
	void *			intrcookie;
	struct intr_config_hook	intr_hook;
	enum rk_pmic_type	type;

	struct rk8xx_regdef	*regdefs;
	TAILQ_HEAD(, reg_list)	regs;
	int			nregs;

	struct rk8xx_rtc_reg	rtc_regs;
	struct rk8xx_dev_ctrl	dev_ctrl;
};

int rk8xx_read(device_t dev, uint8_t reg, uint8_t *data, uint8_t size);
int rk8xx_write(device_t dev, uint8_t reg, uint8_t *data, uint8_t size);

DECLARE_CLASS(rk8xx_driver);

int rk8xx_attach(struct rk8xx_softc *sc);

/* rk8xx_clocks.c */
int rk8xx_attach_clocks(struct rk8xx_softc *sc);

/* rk8xx_regulators.c */
void rk8xx_attach_regulators(struct rk8xx_softc *sc);
int rk8xx_map(device_t dev, phandle_t xref, int ncells,
    pcell_t *cells, intptr_t *id);

/* rk8xx_rtc.c */
int rk8xx_gettime(device_t dev, struct timespec *ts);
int rk8xx_settime(device_t dev, struct timespec *ts);

#endif	/* _RK8XX_H_ */
