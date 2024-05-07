/* SPDX-License-Identifier: MIT */
/*
 * StarFive JH7110 PLL Clock Generator Driver
 *
 * Copyright (C) 2022 Xingyu Wu <xingyu.wu@starfivetech.com>
 */

#define PLL0_DACPD_SHIFT	24
#define PLL0_DACPD_MASK		0x1000000
#define PLL_0_DACPD_SHIFT	24
#define PLL_0_DACPD_MASK		0x1000000

#define PLL0_DSMPD_SHIFT	25
#define PLL0_DSMPD_MASK		0x2000000
#define PLL0_FBDIV_SHIFT	0
#define PLL0_FBDIV_MASK		0xFFF
#define PLL0_FRAC_SHIFT		0
#define PLL0_FRAC_MASK		0xFFFFFF
#define PLL0_POSTDIV1_SHIFT	28
#define PLL0_POSTDIV1_MASK	0x30000000
#define PLL0_PREDIV_SHIFT	0
#define PLL0_PREDIV_MASK	0x3F

#define PLL1_DACPD_SHIFT	15
#define PLL1_DACPD_MASK		0x8000
#define PLL1_DSMPD_SHIFT	16
#define PLL1_DSMPD_MASK		0x10000
#define PLL1_FBDIV_SHIFT	17
#define PLL1_FBDIV_MASK		0x1FFE0000
#define PLL1_FRAC_SHIFT		0
#define PLL1_FRAC_MASK		0xFFFFFF
#define PLL1_POSTDIV1_SHIFT	28
#define PLL1_POSTDIV1_MASK	0x30000000
#define PLL1_PREDIV_SHIFT	0
#define PLL1_PREDIV_MASK	0x3F

#define PLL2_DACPD_SHIFT	15
#define PLL2_DACPD_MASK		0x8000
#define PLL2_DSMPD_SHIFT	16
#define PLL2_DSMPD_MASK		0x10000
#define PLL2_FBDIV_SHIFT	17
#define PLL2_FBDIV_MASK		0x1FFE0000
#define PLL2_FRAC_SHIFT		0
#define PLL2_FRAC_MASK		0xFFFFFF
#define PLL2_POSTDIV1_SHIFT	28
#define PLL2_POSTDIV1_MASK	0x30000000
#define PLL2_PREDIV_SHIFT	0
#define PLL2_PREDIV_MASK	0x3F

#define FRAC_PATR_SIZE		1000

struct jh7110_pll_syscon_value {
	uint64_t freq;
	uint32_t prediv;
	uint32_t fbdiv;
	uint32_t postdiv1;
	uint32_t dacpd;
	uint32_t dsmpd;
	uint32_t frac;
};

enum starfive_pll0_freq_value {
	PLL0_FREQ_375_VALUE = 375000000,
	PLL0_FREQ_500_VALUE = 500000000,
	PLL0_FREQ_625_VALUE = 625000000,
	PLL0_FREQ_750_VALUE = 750000000,
	PLL0_FREQ_875_VALUE = 875000000,
	PLL0_FREQ_1000_VALUE = 1000000000,
	PLL0_FREQ_1250_VALUE = 1250000000,
	PLL0_FREQ_1375_VALUE = 1375000000,
	PLL0_FREQ_1500_VALUE = 1500000000
};

enum starfive_pll0_freq {
	PLL0_FREQ_375 = 0,
	PLL0_FREQ_500,
	PLL0_FREQ_625,
	PLL0_FREQ_750,
	PLL0_FREQ_875,
	PLL0_FREQ_1000,
	PLL0_FREQ_1250,
	PLL0_FREQ_1375,
	PLL0_FREQ_1500,
	PLL0_FREQ_MAX = PLL0_FREQ_1500
};

enum starfive_pll1_freq_value {
	PLL1_FREQ_1066_VALUE = 1066000000,
};

enum starfive_pll1_freq {
	PLL1_FREQ_1066 = 0,
};

enum starfive_pll2_freq_value {
	PLL2_FREQ_1188_VALUE = 1188000000,
	PLL2_FREQ_12288_VALUE = 1228800000,
};

enum starfive_pll2_freq {
	PLL2_FREQ_1188 = 0,
	PLL2_FREQ_12288,
};

static const struct jh7110_pll_syscon_value
	jh7110_pll0_syscon_freq[] = {
	[PLL0_FREQ_375] = {
		.freq = PLL0_FREQ_375_VALUE,
		.prediv = 8,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_500] = {
		.freq = PLL0_FREQ_500_VALUE,
		.prediv = 6,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_625] = {
		.freq = PLL0_FREQ_625_VALUE,
		.prediv = 24,
		.fbdiv = 625,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_750] = {
		.freq = PLL0_FREQ_750_VALUE,
		.prediv = 4,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_875] = {
		.freq = PLL0_FREQ_875_VALUE,
		.prediv = 24,
		.fbdiv = 875,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_1000] = {
		.freq = PLL0_FREQ_1000_VALUE,
		.prediv = 3,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_1250] = {
		.freq = PLL0_FREQ_1250_VALUE,
		.prediv = 12,
		.fbdiv = 625,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_1375] = {
		.freq = PLL0_FREQ_1375_VALUE,
		.prediv = 24,
		.fbdiv = 1375,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL0_FREQ_1500] = {
		.freq = PLL0_FREQ_1500_VALUE,
		.prediv = 2,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
};

static const struct jh7110_pll_syscon_value
	jh7110_pll1_syscon_freq[] = {
	[PLL1_FREQ_1066] = {
		.freq = PLL1_FREQ_1066_VALUE,
		.prediv = 12,
		.fbdiv = 533,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
};

static const struct jh7110_pll_syscon_value
	jh7110_pll2_syscon_freq[] = {
	[PLL2_FREQ_1188] = {
		.freq = PLL2_FREQ_1188_VALUE,
		.prediv = 2,
		.fbdiv = 99,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[PLL2_FREQ_12288] = {
		.freq = PLL2_FREQ_12288_VALUE,
		.prediv = 5,
		.fbdiv = 256,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
};
