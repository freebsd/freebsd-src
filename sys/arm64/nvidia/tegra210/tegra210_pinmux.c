/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Pin multiplexer driver for Tegra SoCs.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/* Pin multipexor register. */
#define	TEGRA_MUX_FUNCTION_MASK  0x03
#define	TEGRA_MUX_FUNCTION_SHIFT 0
#define	TEGRA_MUX_PUPD_MASK  0x03
#define	TEGRA_MUX_PUPD_SHIFT 2
#define	TEGRA_MUX_TRISTATE_SHIFT 4
#define	TEGRA_MUX_ENABLE_INPUT_SHIFT 5
#define	TEGRA_MUX_OPEN_DRAIN_SHIFT 6
#define	TEGRA_MUX_LOCK_SHIFT 7
#define	TEGRA_MUX_IORESET_SHIFT 8
#define	TEGRA_MUX_RCV_SEL_SHIFT 9


/* Pin goup register. */
#define	TEGRA_GRP_HSM_SHIFT 2
#define	TEGRA_GRP_SCHMT_SHIFT 3
#define	TEGRA_GRP_DRV_TYPE_SHIFT 6
#define	TEGRA_GRP_DRV_TYPE_MASK 0x03
#define	TEGRA_GRP_DRV_DRVDN_SLWR_SHIFT 28
#define	TEGRA_GRP_DRV_DRVDN_SLWR_MASK 0x03
#define	TEGRA_GRP_DRV_DRVUP_SLWF_SHIFT 30
#define	TEGRA_GRP_DRV_DRVUP_SLWF_MASK 0x03

struct pinmux_softc {
	device_t	dev;
	struct resource	*pad_mem_res;
	struct resource	*mux_mem_res;
};

static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra210-pinmux",	1},
	{NULL,				0},
};

enum prop_id {
	PROP_ID_PULL,
	PROP_ID_TRISTATE,
	PROP_ID_ENABLE_INPUT,
	PROP_ID_OPEN_DRAIN,
	PROP_ID_LOCK,
	PROP_ID_IORESET,
	PROP_ID_RCV_SEL,
	PROP_ID_HIGH_SPEED_MODE,
	PROP_ID_SCHMITT,
	PROP_ID_LOW_POWER_MODE,
	PROP_ID_DRIVE_DOWN_STRENGTH,
	PROP_ID_DRIVE_UP_STRENGTH,
	PROP_ID_SLEW_RATE_FALLING,
	PROP_ID_SLEW_RATE_RISING,
	PROP_ID_DRIVE_TYPE,

	PROP_ID_MAX_ID
};

/* Numeric based parameters. */
static const struct prop_name {
	const char *name;
	enum prop_id id;
} prop_names[] = {
	{"nvidia,pull",			PROP_ID_PULL},
	{"nvidia,tristate",		PROP_ID_TRISTATE},
	{"nvidia,enable-input",		PROP_ID_ENABLE_INPUT},
	{"nvidia,open-drain",		PROP_ID_OPEN_DRAIN},
	{"nvidia,lock",			PROP_ID_LOCK},
	{"nvidia,io-reset",		PROP_ID_IORESET},
	{"nvidia,rcv-sel",		PROP_ID_RCV_SEL},
	{"nvidia,io-hv",		PROP_ID_RCV_SEL},
	{"nvidia,high-speed-mode",	PROP_ID_HIGH_SPEED_MODE},
	{"nvidia,schmitt",		PROP_ID_SCHMITT},
	{"nvidia,low-power-mode",	PROP_ID_LOW_POWER_MODE},
	{"nvidia,pull-down-strength",	PROP_ID_DRIVE_DOWN_STRENGTH},
	{"nvidia,pull-up-strength",	PROP_ID_DRIVE_UP_STRENGTH},
	{"nvidia,slew-rate-falling",	PROP_ID_SLEW_RATE_FALLING},
	{"nvidia,slew-rate-rising",	PROP_ID_SLEW_RATE_RISING},
	{"nvidia,drive-type",		PROP_ID_DRIVE_TYPE},
};

/*
 * configuration for one pin group.
 */
struct pincfg {
	char	*function;
	int	params[PROP_ID_MAX_ID];
};
#define	GPIO_BANK_A	 0
#define	GPIO_BANK_B	 1
#define	GPIO_BANK_C	 2
#define	GPIO_BANK_D	 3
#define	GPIO_BANK_E	 4
#define	GPIO_BANK_F	 5
#define	GPIO_BANK_G	 6
#define	GPIO_BANK_H	 7
#define	GPIO_BANK_I	 8
#define	GPIO_BANK_J	 9
#define	GPIO_BANK_K	10
#define	GPIO_BANK_L	11
#define	GPIO_BANK_M	12
#define	GPIO_BANK_N	13
#define	GPIO_BANK_O	14
#define	GPIO_BANK_P	15
#define	GPIO_BANK_Q	16
#define	GPIO_BANK_R	17
#define	GPIO_BANK_S	18
#define	GPIO_BANK_T	19
#define	GPIO_BANK_U	20
#define	GPIO_BANK_V	21
#define	GPIO_BANK_W	22
#define	GPIO_BANK_X	23
#define	GPIO_BANK_Y	24
#define	GPIO_BANK_Z	25
#define	GPIO_BANK_AA	26
#define	GPIO_BANK_BB	27
#define	GPIO_BANK_CC	28
#define	GPIO_BANK_DD	29
#define	GPIO_BANK_EE	30
#define	GPIO_NUM(b, p) (8 * (b) + (p))

struct tegra_grp {
	char *name;
	bus_size_t reg;
	int drvdn_shift;
	int drvdn_mask;
	int drvup_shift;
	int drvup_mask;
};

#define	GRP(r, nm, dn_s, dn_w, up_s, up_w)				\
{									\
	.name = #nm,							\
	.reg = r - 0x8D4,						\
	.drvdn_shift = dn_s,						\
	.drvdn_mask = (1 << dn_w) - 1,					\
	.drvup_shift = up_s,						\
	.drvup_mask = (1 << up_w) - 1,					\
}

/* Use register offsets from TRM */
static const struct tegra_grp pin_grp_tbl[] = {
	GRP(0x9c0,    pa6, 12,  5, 20,  5),
	GRP(0x9c4,   pcc7, 12,  5, 20,  5),
	GRP(0x9c8,    pe6, 12,  5, 20,  5),
	GRP(0x9cc,    pe7, 12,  5, 20,  5),
	GRP(0x9d0,    ph6, 12,  5, 20,  5),
	GRP(0x9d4,    pk0,  0,  0,  0,  0),
	GRP(0x9d8,    pk1,  0,  0,  0,  0),
	GRP(0x9dc,    pk2,  0,  0,  0,  0),
	GRP(0x9e0,    pk3,  0,  0,  0,  0),
	GRP(0x9e4,    pk4,  0,  0,  0,  0),
	GRP(0x9e8,    pk5,  0,  0,  0,  0),
	GRP(0x9ec,    pk6,  0,  0,  0,  0),
	GRP(0x9f0,    pk7,  0,  0,  0,  0),
	GRP(0x9f4,    pl0,  0,  0,  0,  0),
	GRP(0x9f8,    pl1,  0,  0,  0,  0),
	GRP(0x9fc,    pz0, 12,  7, 20,  7),
	GRP(0xa00,    pz1, 12,  7, 20,  7),
	GRP(0xa04,    pz2, 12,  7, 20,  7),
	GRP(0xa08,    pz3, 12,  7, 20,  7),
	GRP(0xa0c,    pz4, 12,  7, 20,  7),
	GRP(0xa10,    pz5, 12,  7, 20,  7),
	GRP(0xa98, sdmmc1, 12,  7, 20,  7),
	GRP(0xa9c, sdmmc2,  2,  6,  8,  6),
	GRP(0xab0, sdmmc3, 12,  7, 20,  7),
	GRP(0xab4, sdmmc4,  2,  6,  8,  6),
};

struct tegra_mux {
	struct tegra_grp grp;
	char *name;
	bus_size_t reg;
	char *functions[4];
	int gpio_num;

};

#define	GMUX(r, gb, gi, nm, f1, f2, f3, f4, gr, dn_s, dn_w, up_s, up_w)	\
{									\
	.name = #nm,							\
	.reg = r,							\
	.gpio_num = GPIO_NUM(GPIO_BANK_##gb, gi),			\
	.functions = {#f1, #f2, #f3, #f4},				\
	.grp.name = #nm,						\
	.grp.reg = gr - 0x8D4,						\
	.grp.drvdn_shift = dn_s,					\
	.grp.drvdn_mask = (1 << dn_w) - 1,				\
	.grp.drvup_shift = up_s,					\
	.grp.drvup_mask = (1 << up_w) - 1,				\
}

#define	FMUX(r, nm, f1, f2, f3, f4, gr, dn_s, dn_w, up_s, up_w)		\
{									\
	.name = #nm,							\
	.reg = r,							\
	.gpio_num = -1,							\
	.functions = {#f1, #f2, #f3, #f4},				\
	.grp.name = #nm,						\
	.grp.reg = gr - 0x8D4,						\
	.grp.drvdn_shift = dn_s,					\
	.grp.drvdn_mask = (1 << dn_w) - 1,				\
	.grp.drvup_shift = up_s,					\
	.grp.drvup_mask = (1 << up_w) - 1,				\
}

static const struct tegra_mux pin_mux_tbl[] = {
	GMUX(0x000,  M, 0, sdmmc1_clk_pm0,       sdmmc1,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x004,  M, 1, sdmmc1_cmd_pm1,       sdmmc1,   spi3, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x008,  M, 2, sdmmc1_dat3_pm2,      sdmmc1,   spi3, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x00c,  M, 3, sdmmc1_dat2_pm3,      sdmmc1,   spi3, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x010,  M, 4, sdmmc1_dat1_pm4,      sdmmc1,   spi3, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x014,  M, 5, sdmmc1_dat0_pm5,      sdmmc1,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x01c,  P, 0, sdmmc3_clk_pp0,       sdmmc3,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x020,  P, 1, sdmmc3_cmd_pp1,       sdmmc3,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x024,  P, 5, sdmmc3_dat0_pp5,      sdmmc3,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x028,  P, 4, sdmmc3_dat1_pp4,      sdmmc3,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x02c,  P, 3, sdmmc3_dat2_pp3,      sdmmc3,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x030,  P, 2, sdmmc3_dat3_pp2,      sdmmc3,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x038,  A, 0, pex_l0_rst_n_pa0,        pe0,  rsvd1, rsvd2, rsvd3,   0xa5c, 12,  5, 20,  5),
	GMUX(0x03c,  A, 1, pex_l0_clkreq_n_pa1,     pe0,  rsvd1, rsvd2, rsvd3,   0xa58, 12,  5, 20,  5),
	GMUX(0x040,  A, 2, pex_wake_n_pa2,           pe,  rsvd1, rsvd2, rsvd3,   0xa68, 12,  5, 20,  5),
	GMUX(0x044,  A, 3, pex_l1_rst_n_pa3,        pe1,  rsvd1, rsvd2, rsvd3,   0xa64, 12,  5, 20,  5),
	GMUX(0x048,  A, 4, pex_l1_clkreq_n_pa4,     pe1,  rsvd1, rsvd2, rsvd3,   0xa60, 12,  5, 20,  5),
	GMUX(0x04c,  A, 5, sata_led_active_pa5,    sata,  rsvd1, rsvd2, rsvd3,   0xa94, 12,  5, 20,  5),
	GMUX(0x050,  C, 0, spi1_mosi_pc0,          spi1,  rsvd1, rsvd2, rsvd3,   0xae0,  0,  0,  0,  0),
	GMUX(0x054,  C, 1, spi1_miso_pc1,          spi1,  rsvd1, rsvd2, rsvd3,   0xadc,  0,  0,  0,  0),
	GMUX(0x058,  C, 2, spi1_sck_pc2,           spi1,  rsvd1, rsvd2, rsvd3,   0xae4,  0,  0,  0,  0),
	GMUX(0x05c,  C, 3, spi1_cs0_pc3,           spi1,  rsvd1, rsvd2, rsvd3,   0xad4,  0,  0,  0,  0),
	GMUX(0x060,  C, 4, spi1_cs1_pc4,           spi1,  rsvd1, rsvd2, rsvd3,   0xad8,  0,  0,  0,  0),
	GMUX(0x064,  B, 4, spi2_mosi_pb4,          spi2,    dtv, rsvd2, rsvd3,   0xaf4,  0,  0,  0,  0),
	GMUX(0x068,  B, 5, spi2_miso_pb5,          spi2,    dtv, rsvd2, rsvd3,   0xaf0,  0,  0,  0,  0),
	GMUX(0x06c,  B, 6, spi2_sck_pb6,           spi2,    dtv, rsvd2, rsvd3,   0xaf8,  0,  0,  0,  0),
	GMUX(0x070,  B, 7, spi2_cs0_pb7,           spi2,    dtv, rsvd2, rsvd3,   0xae8,  0,  0,  0,  0),
	GMUX(0x074, DD, 0, spi2_cs1_pdd0,          spi2,  rsvd1, rsvd2, rsvd3,   0xaec,  0,  0,  0,  0),
	GMUX(0x078,  C, 7, spi4_mosi_pc7,          spi4,  rsvd1, rsvd2, rsvd3,   0xb04,  0,  0,  0,  0),
	GMUX(0x07c,  D, 0, spi4_miso_pd0,          spi4,  rsvd1, rsvd2, rsvd3,   0xb00,  0,  0,  0,  0),
	GMUX(0x080,  C, 5, spi4_sck_pc5,           spi4,  rsvd1, rsvd2, rsvd3,   0xb08,  0,  0,  0,  0),
	GMUX(0x084,  C, 6, spi4_cs0_pc6,           spi4,  rsvd1, rsvd2, rsvd3,   0xafc,  0,  0,  0,  0),
	GMUX(0x088, EE, 0, qspi_sck_pee0,          qspi,  rsvd1, rsvd2, rsvd3,   0xa90,  0,  0,  0,  0),
	GMUX(0x08c, EE, 1, qspi_cs_n_pee1,         qspi,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x090, EE, 2, qspi_io0_pee2,          qspi,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x094, EE, 3, qspi_io1_pee3,          qspi,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x098, EE, 4, qspi_io2_pee4,          qspi,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x09c, EE, 5, qspi_io3_pee5,          qspi,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x0a4,  E, 0, dmic1_clk_pe0,         dmic1,   i2s3, rsvd2, rsvd3,   0x984, 12,  5, 20,  5),
	GMUX(0x0a8,  E, 1, dmic1_dat_pe1,         dmic1,   i2s3, rsvd2, rsvd3,   0x988, 12,  5, 20,  5),
	GMUX(0x0ac,  E, 2, dmic2_clk_pe2,         dmic2,   i2s3, rsvd2, rsvd3,   0x98c, 12,  5, 20,  5),
	GMUX(0x0b0,  E, 3, dmic2_dat_pe3,         dmic2,   i2s3, rsvd2, rsvd3,   0x990, 12,  5, 20,  5),
	GMUX(0x0b4,  E, 4, dmic3_clk_pe4,         dmic3,  i2s5a, rsvd2, rsvd3,   0x994, 12,  5, 20,  5),
	GMUX(0x0b8,  E, 5, dmic3_dat_pe5,         dmic3,  i2s5a, rsvd2, rsvd3,   0x998, 12,  5, 20,  5),
	GMUX(0x0bc,  J, 1, gen1_i2c_scl_pj1,       i2c1,  rsvd1, rsvd2, rsvd3,   0x9a8, 12,  5, 20,  5),
	GMUX(0x0c0,  J, 0, gen1_i2c_sda_pj0,       i2c1,  rsvd1, rsvd2, rsvd3,   0x9ac, 12,  5, 20,  5),
	GMUX(0x0c4,  J, 2, gen2_i2c_scl_pj2,       i2c2,  rsvd1, rsvd2, rsvd3,   0x9b0, 12,  5, 20,  5),
	GMUX(0x0c8,  J, 3, gen2_i2c_sda_pj3,       i2c2,  rsvd1, rsvd2, rsvd3,   0x9b4, 12,  5, 20,  5),
	GMUX(0x0cc,  F, 0, gen3_i2c_scl_pf0,       i2c3,  rsvd1, rsvd2, rsvd3,   0x9b8, 12,  5, 20,  5),
	GMUX(0x0d0,  F, 1, gen3_i2c_sda_pf1,       i2c3,  rsvd1, rsvd2, rsvd3,   0x9bc, 12,  5, 20,  5),
	GMUX(0x0d4,  S, 2, cam_i2c_scl_ps2,        i2c3,  i2cvi, rsvd2, rsvd3,   0x934, 12,  5, 20,  5),
	GMUX(0x0d8,  S, 3, cam_i2c_sda_ps3,        i2c3,  i2cvi, rsvd2, rsvd3,   0x938, 12,  5, 20,  5),
	GMUX(0x0dc,  Y, 3, pwr_i2c_scl_py3,      i2cpmu,  rsvd1, rsvd2, rsvd3,   0xa6c, 12,  5, 20,  5),
	GMUX(0x0e0,  Y, 4, pwr_i2c_sda_py4,      i2cpmu,  rsvd1, rsvd2, rsvd3,   0xa70, 12,  5, 20,  5),
	GMUX(0x0e4,  U, 0, uart1_tx_pu0,          uarta,  rsvd1, rsvd2, rsvd3,   0xb28, 12,  5, 20,  5),
	GMUX(0x0e8,  U, 1, uart1_rx_pu1,          uarta,  rsvd1, rsvd2, rsvd3,   0xb24, 12,  5, 20,  5),
	GMUX(0x0ec,  U, 2, uart1_rts_pu2,         uarta,  rsvd1, rsvd2, rsvd3,   0xb20, 12,  5, 20,  5),
	GMUX(0x0f0,  U, 3, uart1_cts_pu3,         uarta,  rsvd1, rsvd2, rsvd3,   0xb1c, 12,  5, 20,  5),
	GMUX(0x0f4,  G, 0, uart2_tx_pg0,          uartb,  i2s4a, spdif,  uart,   0xb38, 12,  5, 20,  5),
	GMUX(0x0f8,  G, 1, uart2_rx_pg1,          uartb,  i2s4a, spdif,  uart,   0xb34, 12,  5, 20,  5),
	GMUX(0x0fc,  G, 2, uart2_rts_pg2,         uartb,  i2s4a, rsvd2,  uart,   0xb30, 12,  5, 20,  5),
	GMUX(0x100,  G, 3, uart2_cts_pg3,         uartb,  i2s4a, rsvd2,  uart,   0xb2c, 12,  5, 20,  5),
	GMUX(0x104,  D, 1, uart3_tx_pd1,          uartc,   spi4, rsvd2, rsvd3,   0xb48, 12,  5, 20,  5),
	GMUX(0x108,  D, 2, uart3_rx_pd2,          uartc,   spi4, rsvd2, rsvd3,   0xb44, 12,  5, 20,  5),
	GMUX(0x10c,  D, 3, uart3_rts_pd3,         uartc,   spi4, rsvd2, rsvd3,   0xb40, 12,  5, 20,  5),
	GMUX(0x110,  D, 4, uart3_cts_pd4,         uartc,   spi4, rsvd2, rsvd3,   0xb3c, 12,  5, 20,  5),
	GMUX(0x114,  I, 4, uart4_tx_pi4,          uartd,   uart, rsvd2, rsvd3,   0xb58, 12,  5, 20,  5),
	GMUX(0x118,  I, 5, uart4_rx_pi5,          uartd,   uart, rsvd2, rsvd3,   0xb54, 12,  5, 20,  5),
	GMUX(0x11c,  I, 6, uart4_rts_pi6,         uartd,   uart, rsvd2, rsvd3,   0xb50, 12,  5, 20,  5),
	GMUX(0x120,  I, 7, uart4_cts_pi7,         uartd,   uart, rsvd2, rsvd3,   0xb4c, 12,  5, 20,  5),
	GMUX(0x124,  B, 0, dap1_fs_pb0,            i2s1,  rsvd1, rsvd2, rsvd3,   0x95c,  0,  0,  0,  0),
	GMUX(0x128,  B, 1, dap1_din_pb1,           i2s1,  rsvd1, rsvd2, rsvd3,   0x954,  0,  0,  0,  0),
	GMUX(0x12c,  B, 2, dap1_dout_pb2,          i2s1,  rsvd1, rsvd2, rsvd3,   0x958,  0,  0,  0,  0),
	GMUX(0x130,  B, 3, dap1_sclk_pb3,          i2s1,  rsvd1, rsvd2, rsvd3,   0x960,  0,  0,  0,  0),
	GMUX(0x134, AA, 0, dap2_fs_paa0,           i2s2,  rsvd1, rsvd2, rsvd3,   0x96c,  0,  0,  0,  0),
	GMUX(0x138, AA, 2, dap2_din_paa2,          i2s2,  rsvd1, rsvd2, rsvd3,   0x964,  0,  0,  0,  0),
	GMUX(0x13c, AA, 3, dap2_dout_paa3,         i2s2,  rsvd1, rsvd2, rsvd3,   0x968,  0,  0,  0,  0),
	GMUX(0x140, AA, 1, dap2_sclk_paa1,         i2s2,  rsvd1, rsvd2, rsvd3,   0x970,  0,  0,  0,  0),
	GMUX(0x144,  J, 4, dap4_fs_pj4,           i2s4b,  rsvd1, rsvd2, rsvd3,   0x97c, 12,  5, 20,  5),
	GMUX(0x148,  J, 5, dap4_din_pj5,          i2s4b,  rsvd1, rsvd2, rsvd3,   0x974, 12,  5, 20,  5),
	GMUX(0x14c,  J, 6, dap4_dout_pj6,         i2s4b,  rsvd1, rsvd2, rsvd3,   0x978, 12,  5, 20,  5),
	GMUX(0x150,  J, 7, dap4_sclk_pj7,         i2s4b,  rsvd1, rsvd2, rsvd3,   0x980, 12,  5, 20,  5),
	GMUX(0x154,  S, 0, cam1_mclk_ps0,    extperiph3,  rsvd1, rsvd2, rsvd3,   0x918, 12,  5, 20,  5),
	GMUX(0x158,  S, 1, cam2_mclk_ps1,    extperiph3,  rsvd1, rsvd2, rsvd3,   0x924, 12,  5, 20,  5),
	FMUX(0x15c,        jtag_rtck,              jtag,  rsvd1, rsvd2, rsvd3,   0xa2c, 12,  5, 20,  5),
	FMUX(0x160,        clk_32k_in,              clk,  rsvd1, rsvd2, rsvd3,   0x940, 12,  5, 20,  5),
	GMUX(0x164,  Y, 5, clk_32k_out_py5,         soc,  blink, rsvd2, rsvd3,   0x944, 12,  5, 20,  5),
	FMUX(0x168,        batt_bcl,                bcl,  rsvd1, rsvd2, rsvd3,   0x8f8, 12,  5, 20,  5),
	FMUX(0x16c,        clk_req,                 sys,  rsvd1, rsvd2, rsvd3,   0x948, 12,  5, 20,  5),
	FMUX(0x170,        cpu_pwr_req,             cpu,  rsvd1, rsvd2, rsvd3,   0x950, 12,  5, 20,  5),
	FMUX(0x174,        pwr_int_n,               pmi,  rsvd1, rsvd2, rsvd3,   0xa74, 12,  5, 20,  5),
	FMUX(0x178,        shutdown,           shutdown,  rsvd1, rsvd2, rsvd3,   0xac8, 12,  5, 20,  5),
	FMUX(0x17c,        core_pwr_req,           core,  rsvd1, rsvd2, rsvd3,   0x94c, 12,  5, 20,  5),
	GMUX(0x180, BB, 0, aud_mclk_pbb0,           aud,  rsvd1, rsvd2, rsvd3,   0x8f4, 12,  5, 20,  5),
	GMUX(0x184, BB, 1, dvfs_pwm_pbb1,         rsvd0, cldvfs,  spi3, rsvd3,   0x9a4, 12,  5, 20,  5),
	GMUX(0x188, BB, 2, dvfs_clk_pbb2,         rsvd0, cldvfs,  spi3, rsvd3,   0x9a0, 12,  5, 20,  5),
	GMUX(0x18c, BB, 3, gpio_x1_aud_pbb3,      rsvd0,  rsvd1,  spi3, rsvd3,   0xa14, 12,  5, 20,  5),
	GMUX(0x190, BB, 4, gpio_x3_aud_pbb4,      rsvd0,  rsvd1,  spi3, rsvd3,   0xa18, 12,  5, 20,  5),
	GMUX(0x194, CC, 7, pcc7,                  rsvd0,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x198, CC, 0, hdmi_cec_pcc0,           cec,  rsvd1, rsvd2, rsvd3,   0xa24, 12,  5, 20,  5),
	GMUX(0x19c, CC, 1, hdmi_int_dp_hpd_pcc1,     dp,  rsvd1, rsvd2, rsvd3,   0xa28, 12,  5, 20,  5),
	GMUX(0x1a0, CC, 2, spdif_out_pcc2,        spdif,  rsvd1, rsvd2, rsvd3,   0xad0, 12,  5, 20,  5),
	GMUX(0x1a4, CC, 3, spdif_in_pcc3,         spdif,  rsvd1, rsvd2, rsvd3,   0xacc, 12,  5, 20,  5),
	GMUX(0x1a8, CC, 4, usb_vbus_en0_pcc4,       usb,  rsvd1, rsvd2, rsvd3,   0xb5c, 12,  5, 20,  5),
	GMUX(0x1ac, CC, 5, usb_vbus_en1_pcc5,       usb,  rsvd1, rsvd2, rsvd3,   0xb60, 12,  5, 20,  5),
	GMUX(0x1b0, CC, 6, dp_hpd0_pcc6,             dp,  rsvd1, rsvd2, rsvd3,   0x99c, 12,  5, 20,  5),
	GMUX(0x1b4,  H, 0, wifi_en_ph0,           rsvd0,  rsvd1, rsvd2, rsvd3,   0xb64, 12,  5, 20,  5),
	GMUX(0x1b8,  H, 1, wifi_rst_ph1,          rsvd0,  rsvd1, rsvd2, rsvd3,   0xb68, 12,  5, 20,  5),
	GMUX(0x1bc,  H, 2, wifi_wake_ap_ph2,      rsvd0,  rsvd1, rsvd2, rsvd3,   0xb6c, 12,  5, 20,  5),
	GMUX(0x1c0,  H, 3, ap_wake_bt_ph3,        rsvd0,  uartb, spdif, rsvd3,   0x8ec, 12,  5, 20,  5),
	GMUX(0x1c4,  H, 4, bt_rst_ph4,            rsvd0,  uartb, spdif, rsvd3,   0x8fc, 12,  5, 20,  5),
	GMUX(0x1c8,  H, 5, bt_wake_ap_ph5,        rsvd0,  rsvd1, rsvd2, rsvd3,   0x900, 12,  5, 20,  5),
	GMUX(0x1cc,  H, 7, ap_wake_nfc_ph7,       rsvd0,  rsvd1, rsvd2, rsvd3,   0x8f0, 12,  5, 20,  5),
	GMUX(0x1d0,  I, 0, nfc_en_pi0,            rsvd0,  rsvd1, rsvd2, rsvd3,   0xa50, 12,  5, 20,  5),
	GMUX(0x1d4,  I, 1, nfc_int_pi1,           rsvd0,  rsvd1, rsvd2, rsvd3,   0xa54, 12,  5, 20,  5),
	GMUX(0x1d8,  I, 2, gps_en_pi2,            rsvd0,  rsvd1, rsvd2, rsvd3,   0xa1c, 12,  5, 20,  5),
	GMUX(0x1dc,  I, 3, gps_rst_pi3,           rsvd0,  rsvd1, rsvd2, rsvd3,   0xa20, 12,  5, 20,  5),
	GMUX(0x1e0,  S, 4, cam_rst_ps4,            vgp1,  rsvd1, rsvd2, rsvd3,   0x93c, 12,  5, 20,  5),
	GMUX(0x1e4,  S, 5, cam_af_en_ps5,        vimclk,   vgp2, rsvd2, rsvd3,   0x92c, 12,  5, 20,  5),
	GMUX(0x1e8,  S, 6, cam_flash_en_ps6,     vimclk,   vgp3, rsvd2, rsvd3,   0x930, 12,  5, 20,  5),
	GMUX(0x1ec,  S, 7, cam1_pwdn_ps7,          vgp4,  rsvd1, rsvd2, rsvd3,   0x91c, 12,  5, 20,  5),
	GMUX(0x1f0,  T, 0, cam2_pwdn_pt0,          vgp5,  rsvd1, rsvd2, rsvd3,   0x928, 12,  5, 20,  5),
	GMUX(0x1f4,  T, 1, cam1_strobe_pt1,        vgp6,  rsvd1, rsvd2, rsvd3,   0x920, 12,  5, 20,  5),
	GMUX(0x1f8,  Y, 2, lcd_te_py2,         displaya,  rsvd1, rsvd2, rsvd3,   0xa44, 12,  5, 20,  5),
	GMUX(0x1fc,  V, 0, lcd_bl_pwm_pv0,     displaya,   pwm0,  sor0, rsvd3,   0xa34, 12,  5, 20,  5),
	GMUX(0x200,  V, 1, lcd_bl_en_pv1,         rsvd0,  rsvd1, rsvd2, rsvd3,   0xa30, 12,  5, 20,  5),
	GMUX(0x204,  V, 2, lcd_rst_pv2,           rsvd0,  rsvd1, rsvd2, rsvd3,   0xa40, 12,  5, 20,  5),
	GMUX(0x208,  V, 3, lcd_gpio1_pv3,      displayb,  rsvd1, rsvd2, rsvd3,   0xa38, 12,  5, 20,  5),
	GMUX(0x20c,  V, 4, lcd_gpio2_pv4,      displayb,   pwm1, rsvd2,  sor1,   0xa3c, 12,  5, 20,  5),
	GMUX(0x210,  V, 5, ap_ready_pv5,          rsvd0,  rsvd1, rsvd2, rsvd3,   0x8e8, 12,  5, 20,  5),
	GMUX(0x214,  V, 6, touch_rst_pv6,         rsvd0,  rsvd1, rsvd2, rsvd3,   0xb18, 12,  5, 20,  5),
	GMUX(0x218,  V, 7, touch_clk_pv7,         touch,  rsvd1, rsvd2, rsvd3,   0xb10, 12,  5, 20,  5),
	GMUX(0x21c,  X, 0, modem_wake_ap_px0,     rsvd0,  rsvd1, rsvd2, rsvd3,   0xa48, 12,  5, 20,  5),
	GMUX(0x220,  X, 1, touch_int_px1,         rsvd0,  rsvd1, rsvd2, rsvd3,   0xb14, 12,  5, 20,  5),
	GMUX(0x224,  X, 2, motion_int_px2,        rsvd0,  rsvd1, rsvd2, rsvd3,   0xa4c, 12,  5, 20,  5),
	GMUX(0x228,  X, 3, als_prox_int_px3,      rsvd0,  rsvd1, rsvd2, rsvd3,   0x8e4, 12,  5, 20,  5),
	GMUX(0x22c,  X, 4, temp_alert_px4,        rsvd0,  rsvd1, rsvd2, rsvd3,   0xb0c, 12,  5, 20,  5),
	GMUX(0x230,  X, 5, button_power_on_px5,   rsvd0,  rsvd1, rsvd2, rsvd3,   0x908, 12,  5, 20,  5),
	GMUX(0x234,  X, 6, button_vol_up_px6,     rsvd0,  rsvd1, rsvd2, rsvd3,   0x914, 12,  5, 20,  5),
	GMUX(0x238,  X, 7, button_vol_down_px7,   rsvd0,  rsvd1, rsvd2, rsvd3,   0x910, 12,  5, 20,  5),
	GMUX(0x23c,  Y, 0, button_slide_sw_py0,   rsvd0,  rsvd1, rsvd2, rsvd3,   0x90c, 12,  5, 20,  5),
	GMUX(0x240,  Y, 1, button_home_py1,       rsvd0,  rsvd1, rsvd2, rsvd3,   0x904, 12,  5, 20,  5),
	GMUX(0x244,  A, 6, pa6,                   sata,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x248,  E, 6, pe6,                   rsvd0,  i2s5a,  pwm2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x24c,  E, 7, pe7,                   rsvd0,  i2s5a,  pwm3, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x250,  H, 6, ph6,                   rsvd0,  rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x254,  K, 0, pk0,                   iqc0,   i2s5b, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x258,  K, 1, pk1,                   iqc0,   i2s5b, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x25c,  K, 2, pk2,                   iqc0,   i2s5b, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x260,  K, 3, pk3,                   iqc0,   i2s5b, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x264,  K, 4, pk4,                   iqc1,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x268,  K, 5, pk5,                   iqc1,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x26c,  K, 6, pk6,                   iqc1,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x270,  K, 7, pk7,                   iqc1,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x274,  L, 0, pl0,                  rsvd0,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x278,  L, 1, pl1,                    soc,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x27c,  Z, 0, pz0,                vimclk2,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x280,  Z, 1, pz1,                vimclk2,  sdmmc1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x284,  Z, 2, pz2,                 sdmmc3,    ccla, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x288,  Z, 3, pz3,                 sdmmc3,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x28c,  Z, 4, pz4,                 sdmmc1,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
	GMUX(0x290,  Z, 5, pz5,                    soc,   rsvd1, rsvd2, rsvd3,      -1,  0,  0,  0,  0),
};


static const struct tegra_grp *
pinmux_search_grp(char *grp_name)
{
	int i;

	for (i = 0; i < nitems(pin_grp_tbl); i++) {
		if (strcmp(grp_name, pin_grp_tbl[i].name) == 0)
			return 	(&pin_grp_tbl[i]);
	}
	return (NULL);
}

static const struct tegra_mux *
pinmux_search_mux(char *pin_name)
{
	int i;

	for (i = 0; i < nitems(pin_mux_tbl); i++) {
		if (strcmp(pin_name, pin_mux_tbl[i].name) == 0)
			return 	(&pin_mux_tbl[i]);
	}
	return (NULL);
}

static int
pinmux_mux_function(const struct tegra_mux *mux, char *fnc_name)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (strcmp(fnc_name, mux->functions[i]) == 0)
			return 	(i);
	}
	return (-1);
}

static int
pinmux_config_mux(struct pinmux_softc *sc, char *pin_name,
    const struct tegra_mux *mux, struct pincfg *cfg)
{
	int tmp;
	uint32_t reg;

	reg = bus_read_4(sc->mux_mem_res, mux->reg);

	if (cfg->function != NULL) {
		tmp = pinmux_mux_function(mux, cfg->function);
		if (tmp == -1) {
			device_printf(sc->dev,
			    "Unknown function %s for pin %s\n", cfg->function,
			    pin_name);
			return (ENXIO);
		}
		reg &= ~(TEGRA_MUX_FUNCTION_MASK << TEGRA_MUX_FUNCTION_SHIFT);
		reg |=  (tmp & TEGRA_MUX_FUNCTION_MASK) <<
		    TEGRA_MUX_FUNCTION_SHIFT;
	}
	if (cfg->params[PROP_ID_PULL] != -1) {
		reg &= ~(TEGRA_MUX_PUPD_MASK << TEGRA_MUX_PUPD_SHIFT);
		reg |=  (cfg->params[PROP_ID_PULL] & TEGRA_MUX_PUPD_MASK) <<
		    TEGRA_MUX_PUPD_SHIFT;
	}
	if (cfg->params[PROP_ID_TRISTATE] != -1) {
		reg &= ~(1 << TEGRA_MUX_TRISTATE_SHIFT);
		reg |=  (cfg->params[PROP_ID_TRISTATE] & 1) <<
		    TEGRA_MUX_TRISTATE_SHIFT;
	}
	if (cfg->params[TEGRA_MUX_ENABLE_INPUT_SHIFT] != -1) {
		reg &= ~(1 << TEGRA_MUX_ENABLE_INPUT_SHIFT);
		reg |=  (cfg->params[TEGRA_MUX_ENABLE_INPUT_SHIFT] & 1) <<
		    TEGRA_MUX_ENABLE_INPUT_SHIFT;
	}
	if (cfg->params[PROP_ID_ENABLE_INPUT] != -1) {
		reg &= ~(1 << TEGRA_MUX_ENABLE_INPUT_SHIFT);
		reg |=  (cfg->params[PROP_ID_ENABLE_INPUT] & 1) <<
		    TEGRA_MUX_ENABLE_INPUT_SHIFT;
	}
	if (cfg->params[PROP_ID_ENABLE_INPUT] != -1) {
		reg &= ~(1 << TEGRA_MUX_ENABLE_INPUT_SHIFT);
		reg |=  (cfg->params[PROP_ID_OPEN_DRAIN] & 1) <<
		    TEGRA_MUX_ENABLE_INPUT_SHIFT;
	}
	if (cfg->params[PROP_ID_LOCK] != -1) {
		reg &= ~(1 << TEGRA_MUX_LOCK_SHIFT);
		reg |=  (cfg->params[PROP_ID_LOCK] & 1) <<
		    TEGRA_MUX_LOCK_SHIFT;
	}
	if (cfg->params[PROP_ID_IORESET] != -1) {
		reg &= ~(1 << TEGRA_MUX_IORESET_SHIFT);
		reg |=  (cfg->params[PROP_ID_IORESET] & 1) <<
		    TEGRA_MUX_IORESET_SHIFT;
	}
	if (cfg->params[PROP_ID_RCV_SEL] != -1) {
		reg &= ~(1 << TEGRA_MUX_RCV_SEL_SHIFT);
		reg |=  (cfg->params[PROP_ID_RCV_SEL] & 1) <<
		    TEGRA_MUX_RCV_SEL_SHIFT;
	}
	bus_write_4(sc->mux_mem_res, mux->reg, reg);
	return (0);
}

static int
pinmux_config_grp(struct pinmux_softc *sc, char *grp_name,
    const struct tegra_grp *grp, struct pincfg *cfg)
{
	uint32_t reg;

	reg = bus_read_4(sc->pad_mem_res, grp->reg);

	if (cfg->params[PROP_ID_HIGH_SPEED_MODE] != -1) {
		reg &= ~(1 << TEGRA_GRP_HSM_SHIFT);
		reg |=  (cfg->params[PROP_ID_HIGH_SPEED_MODE] & 1) <<
		    TEGRA_GRP_HSM_SHIFT;
	}
	if (cfg->params[PROP_ID_SCHMITT] != -1) {
		reg &= ~(1 << TEGRA_GRP_SCHMT_SHIFT);
		reg |=  (cfg->params[PROP_ID_SCHMITT] & 1) <<
		    TEGRA_GRP_SCHMT_SHIFT;
	}
	if (cfg->params[PROP_ID_DRIVE_TYPE] != -1) {
		reg &= ~(TEGRA_GRP_DRV_TYPE_MASK << TEGRA_GRP_DRV_TYPE_SHIFT);
		reg |=  (cfg->params[PROP_ID_DRIVE_TYPE] &
		    TEGRA_GRP_DRV_TYPE_MASK) << TEGRA_GRP_DRV_TYPE_SHIFT;
	}
	if (cfg->params[PROP_ID_SLEW_RATE_RISING] != -1) {
		reg &= ~(TEGRA_GRP_DRV_DRVDN_SLWR_MASK <<
		    TEGRA_GRP_DRV_DRVDN_SLWR_SHIFT);
		reg |=  (cfg->params[PROP_ID_SLEW_RATE_RISING] &
		    TEGRA_GRP_DRV_DRVDN_SLWR_MASK) <<
		    TEGRA_GRP_DRV_DRVDN_SLWR_SHIFT;
	}
	if (cfg->params[PROP_ID_SLEW_RATE_FALLING] != -1) {
		reg &= ~(TEGRA_GRP_DRV_DRVUP_SLWF_MASK <<
		    TEGRA_GRP_DRV_DRVUP_SLWF_SHIFT);
		reg |=  (cfg->params[PROP_ID_SLEW_RATE_FALLING] &
		    TEGRA_GRP_DRV_DRVUP_SLWF_MASK) <<
		    TEGRA_GRP_DRV_DRVUP_SLWF_SHIFT;
	}
	if ((cfg->params[PROP_ID_DRIVE_DOWN_STRENGTH] != -1) &&
		 (grp->drvdn_mask != 0)) {
		reg &= ~(grp->drvdn_shift << grp->drvdn_mask);
		reg |=  (cfg->params[PROP_ID_DRIVE_DOWN_STRENGTH] &
		    grp->drvdn_mask) << grp->drvdn_shift;
	}
	if ((cfg->params[PROP_ID_DRIVE_UP_STRENGTH] != -1) &&
		 (grp->drvup_mask != 0)) {
		reg &= ~(grp->drvup_shift << grp->drvup_mask);
		reg |=  (cfg->params[PROP_ID_DRIVE_UP_STRENGTH] &
		    grp->drvup_mask) << grp->drvup_shift;
	}
	bus_write_4(sc->pad_mem_res, grp->reg, reg);
	return (0);
}

static int
pinmux_config_node(struct pinmux_softc *sc, char *pin_name, struct pincfg *cfg)
{
	const struct tegra_mux *mux;
	const struct tegra_grp *grp;
	bool handled;
	int rv;

	/* Handle pin muxes */
	mux = pinmux_search_mux(pin_name);
	handled = false;
	if (mux != NULL) {
		if (mux->gpio_num != -1) {
			/* XXXX TODO: Reserve gpio here */
		}
		rv = pinmux_config_mux(sc, pin_name, mux, cfg);
		if (rv != 0)
			return (rv);
		if (mux->grp.reg <= 0) {
			rv = pinmux_config_grp(sc, pin_name, &mux->grp, cfg);
			return (rv);
		}
		handled = true;
	}

	/* And/or handle pin groups */
	grp = pinmux_search_grp(pin_name);
	if (grp != NULL) {
		rv = pinmux_config_grp(sc, pin_name, grp, cfg);
		if (rv != 0)
			return (rv);
		handled = true;
	}

	if (!handled) {
		device_printf(sc->dev, "Unknown pin: %s\n", pin_name);
		return (ENXIO);
	}
	return (0);
}

static int
pinmux_read_node(struct pinmux_softc *sc, phandle_t node, struct pincfg *cfg,
    char **pins, int *lpins)
{
	int rv, i;

	*lpins = OF_getprop_alloc(node, "nvidia,pins", (void **)pins);
	if (*lpins <= 0)
		return (ENOENT);

	/* Read function (mux) settings. */
	rv = OF_getprop_alloc(node, "nvidia,function", (void **)&cfg->function);
	if (rv <= 0)
		cfg->function = NULL;

	/* Read numeric properties. */
	for (i = 0; i < PROP_ID_MAX_ID; i++) {
		rv = OF_getencprop(node, prop_names[i].name, &cfg->params[i],
		    sizeof(cfg->params[i]));
		if (rv <= 0)
			cfg->params[i] = -1;
	}
	return (0);
}

static int
pinmux_process_node(struct pinmux_softc *sc, phandle_t node)
{
	struct pincfg cfg;
	char *pins, *pname;
	int i, len, lpins, rv;

	rv = pinmux_read_node(sc, node, &cfg, &pins, &lpins);
	if (rv != 0)
		return (rv);

	len = 0;
	pname = pins;
	do {
		i = strlen(pname) + 1;
		rv = pinmux_config_node(sc, pname, &cfg);
		if (rv != 0)
			device_printf(sc->dev, "Cannot configure pin: %s: %d\n",
			    pname, rv);
		len += i;
		pname += i;
	} while (len < lpins);

	if (pins != NULL)
		OF_prop_free(pins);
	if (cfg.function != NULL)
		OF_prop_free(cfg.function);
	return (rv);
}

static int pinmux_configure(device_t dev, phandle_t cfgxref)
{
	struct pinmux_softc *sc;
	phandle_t node, cfgnode;
	int rv;

	sc = device_get_softc(dev);
	cfgnode = OF_node_from_xref(cfgxref);


	for (node = OF_child(cfgnode); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;
		rv = pinmux_process_node(sc, node);
	}
	return (0);
}

static int
pinmux_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Tegra pin configuration");
	return (BUS_PROBE_DEFAULT);
}

static int
pinmux_detach(device_t dev)
{

	/* This device is always present. */
	return (EBUSY);
}

static int
pinmux_attach(device_t dev)
{
	struct pinmux_softc * sc;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->pad_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->pad_mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}

	rid = 1;
	sc->mux_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mux_mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}


	/* Register as a pinctrl device and process default configuration */
	fdt_pinctrl_register(dev, NULL);
	fdt_pinctrl_configure_by_name(dev, "boot");

	return (0);
}


static device_method_t tegra210_pinmux_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         pinmux_probe),
	DEVMETHOD(device_attach,        pinmux_attach),
	DEVMETHOD(device_detach,        pinmux_detach),

	/* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure,pinmux_configure),

	DEVMETHOD_END
};

static devclass_t tegra210_pinmux_devclass;
static DEFINE_CLASS_0(pinmux, tegra210_pinmux_driver, tegra210_pinmux_methods,
    sizeof(struct pinmux_softc));
EARLY_DRIVER_MODULE(tegra210_pinmux, simplebus, tegra210_pinmux_driver,
    tegra210_pinmux_devclass, NULL, NULL, 71);
