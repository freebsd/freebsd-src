/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

/* Driver for Qualcomm IPQ4018 clock and reset device */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/random.h>
#include <sys/stdatomic.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk_div.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_mux.h>
#include <dev/clk/clk_link.h>

#include <dt-bindings/clock/qcom,gcc-ipq4019.h>

#include <dev/qcom_clk/qcom_clk_freqtbl.h>
#include <dev/qcom_clk/qcom_clk_fepll.h>
#include <dev/qcom_clk/qcom_clk_fdiv.h>
#include <dev/qcom_clk/qcom_clk_apssdiv.h>
#include <dev/qcom_clk/qcom_clk_rcg2.h>
#include <dev/qcom_clk/qcom_clk_branch2.h>
#include <dev/qcom_clk/qcom_clk_ro_div.h>

#include "qcom_gcc_var.h"
#include "qcom_gcc_ipq4018.h"

/* Fixed rate clock. */
#define F_RATE(_id, cname, _freq)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = NULL,					\
	.clkdef.parent_cnt = 0,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.freq = _freq,							\
}

/* Linked clock. */
#define F_LINK(_id, _cname)						\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = NULL,					\
	.clkdef.parent_cnt = 0,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
}


/* FEPLL clock */
#define F_FEPLL(_id, _cname, _parent, _reg, _fs, _fw, _rs, _rw)		\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = (const char *[]){_parent},		\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = _reg,							\
	.fdbkdiv_shift = _fs,						\
	.fdbkdiv_width = _fw,						\
	.refclkdiv_shift = _rs,						\
	.refclkdiv_width = _rw,						\
}

/* Fixed divisor clock */
#define F_FDIV(_id, _cname, _parent, _divisor)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = (const char *[]){_parent},		\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.divisor = _divisor,						\
}

/* APSS DIV clock */
#define F_APSSDIV(_id, _cname, _parent, _doffset, _dshift, _dwidth,	\
    _eoffset, _eshift, _freqtbl)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = (const char *[]){_parent},		\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.div_offset = _doffset,						\
	.div_width = _dwidth,						\
	.div_shift = _dshift,						\
	.enable_offset = _eoffset,					\
	.enable_shift = _eshift,					\
	.freq_tbl = _freqtbl,						\
}

/* read-only div table */
#define	F_RO_DIV(_id, _cname, _parent, _offset, _shift, _width, _tbl)	\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = (const char *[]){_parent},		\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = _offset,						\
	.width = _width,						\
	.shift = _shift,						\
	.div_tbl = _tbl,						\
}

/* RCG2 clock */
#define F_RCG2(_id, _cname, _parents, _rcgr, _hid_width, _mnd_width,	\
    _safe_src_idx, _safe_pre_parent_idx, _cfg_offset, _flags,		\
    _freq_tbl)								\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = _parents,				\
	.clkdef.parent_cnt = nitems(_parents),				\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.cmd_rcgr = _rcgr,						\
	.hid_width = _hid_width,					\
	.mnd_width = _mnd_width,					\
	.safe_src_idx = _safe_src_idx,					\
	.flags= _flags,							\
	.safe_pre_parent_idx = _safe_pre_parent_idx,			\
	.freq_tbl = _freq_tbl,						\
}

/* branch2 gate nodes */
#define	F_BRANCH2(_id, _cname, _parent, _eo, _es, _hr, _hs, _haltreg,	\
    _type, _voted, _flags)						\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = _cname,						\
	.clkdef.parent_names = (const char *[]){_parent},		\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.enable_offset = _eo,						\
	.enable_shift = _es,						\
	.hwcg_reg = _hr,						\
	.hwcg_bit = _hs,						\
	.halt_reg = _haltreg,						\
	.halt_check_type = _type,					\
	.halt_check_voted = _voted,					\
	.flags = _flags,						\
}

/*
 * Fixed "gcc_fepll_vco" PLL derived sources:
 *
 * P_FEPLL125 - 125MHz
 * P_FEPLL125DLY - 125MHz
 * P_FEPLL200 - 200MHz
 * "fepll500" - 500MHz
 *
 * Fixed "gcc_apps_ddrpll_vco" PLL derived sources:
 *
 * P_DDRPLL - 192MHz
 */
static struct qcom_clk_fdiv_def fdiv_tbl[] = {
	F_FDIV(GCC_FEPLL125_CLK, "fepll125", "gcc_fepll_vco", 32),
	F_FDIV(GCC_FEPLL125DLY_CLK, "fepll125dly", "gcc_fepll_vco", 32),
	F_FDIV(GCC_FEPLL200_CLK, "fepll200", "gcc_fepll_vco", 20),
	F_FDIV(GCC_FEPLL500_CLK, "fepll500", "gcc_fepll_vco", 8),
	F_FDIV(GCC_SDCC_PLLDIV_CLK, "ddrpllsdcc", "gcc_apps_ddrpll_vco", 28),
};

/*
 * FEPLL - 48MHz (xo) input, 4GHz output
 * DDRPLL - 48MHz (xo) input, 5.376GHz output
 */
static struct qcom_clk_fepll_def fepll_tbl[] = {
	F_FEPLL(GCC_FEPLL_VCO, "gcc_fepll_vco", "xo", 0x2f020, 16, 8, 24, 5),
	F_FEPLL(GCC_APSS_DDRPLL_VCO, "gcc_apps_ddrpll_vco", "xo", 0x2e020,
	    16, 8, 24, 5),
};

/*
 * Frequency table for the APSS PLL/DIV path for the CPU frequency.
 *
 * Note - the APSS DIV code only needs the frequency and pre-divisor,
 * not the other fields.
 */
static struct qcom_clk_freq_tbl apss_freq_tbl[] = {
	{ 384000000, "gcc_apps_ddrpll_vco", 0xd, 0, 0 },
	{ 413000000, "gcc_apps_ddrpll_vco", 0xc, 0, 0 },
	{ 448000000, "gcc_apps_ddrpll_vco", 0xb, 0, 0 },
	{ 488000000, "gcc_apps_ddrpll_vco", 0xa, 0, 0 },
	{ 512000000, "gcc_apps_ddrpll_vco", 0x9, 0, 0 },
	{ 537000000, "gcc_apps_ddrpll_vco", 0x8, 0, 0 },
	{ 565000000, "gcc_apps_ddrpll_vco", 0x7, 0, 0 },
	{ 597000000, "gcc_apps_ddrpll_vco", 0x6, 0, 0 },
	{ 632000000, "gcc_apps_ddrpll_vco", 0x5, 0, 0 },
	{ 672000000, "gcc_apps_ddrpll_vco", 0x4, 0, 0 },
	{ 716000000, "gcc_apps_ddrpll_vco", 0x3, 0, 0 },
	{ 768000000, "gcc_apps_ddrpll_vco", 0x2, 0, 0 },
	{ 823000000, "gcc_apps_ddrpll_vco", 0x1, 0, 0 },
	{ 896000000, "gcc_apps_ddrpll_vco", 0x0, 0, 0 },
	{ 0, }
};

/*
 * APSS div/gate
 */
static struct qcom_clk_apssdiv_def apssdiv_tbl[] = {
	F_APSSDIV(GCC_APSS_CPU_PLLDIV_CLK, "ddrpllapss",
	    "gcc_apps_ddrpll_vco", 0x2e020,
	    4, 4, 0x2e000, 0, &apss_freq_tbl[0]),
};

/*
 * Parent clocks for the apps_clk_src clock.
 */
static const char * apps_clk_src_parents[] = {
	"xo", "ddrpllapss", "fepll500", "fepll200"
};

/*
 * Parents lists for a variety of blocks.
 */
static const char * gcc_xo_200_parents[] = {
	"xo", "fepll200"
};
static const char * gcc_xo_200_500_parents[] = {
	"xo", "fepll200", "fepll500"
};
static const char * gcc_xo_200_spi_parents[] = {
	"xo", NULL, "fepll200"
};
static const char * gcc_xo_sdcc1_500_parents[] = {
	"xo", "ddrpllsdcc", "fepll500"
};

static const char * gcc_xo_125_dly_parents[] = {
	"xo", "fepll125dly"
};

static const char * gcc_xo_wcss2g_parents[] = {
	"xo", "fepllwcss2g"
};

static const char * gcc_xo_wcss5g_parents[] = {
	"xo", "fepllwcss5g"
};

static struct qcom_clk_freq_tbl apps_clk_src_freq_tbl[] = {
	{ 48000000, "xo", 1, 0, 0 },
	{ 200000000, "fepll200", 1, 0, 0 },
	{ 384000000, "ddrpllapss", 1, 0, 0 },
	{ 413000000, "ddrpllapss", 1, 0, 0 },
	{ 448000000, "ddrpllapss", 1, 0, 0 },
	{ 488000000, "ddrpllapss", 1, 0, 0 },
	{ 500000000, "fepll500", 1, 0, 0 },
	{ 512000000, "ddrpllapss", 1, 0, 0 },
	{ 537000000, "ddrpllapss", 1, 0, 0 },
	{ 565000000, "ddrpllapss", 1, 0, 0 },
	{ 597000000, "ddrpllapss", 1, 0, 0 },
	{ 632000000, "ddrpllapss", 1, 0, 0 },
	{ 672000000, "ddrpllapss", 1, 0, 0 },
	{ 716000000, "ddrpllapss", 1, 0, 0 },
	{ 0,}

};

static struct qcom_clk_freq_tbl audio_clk_src_freq_tbl[] = {
	{ 48000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 200000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0,}
};

static struct qcom_clk_freq_tbl blsp1_qup1_i2c_apps_clk_src_freq_tbl[] = {
	{ 19050000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(10.5), 1, 1 },
	{ 0,}
};

static struct qcom_clk_freq_tbl blsp1_qup1_spi_apps_clk_src_freq_tbl[] = {
	{ 960000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(12), 1, 4 },
	{ 4800000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 10 },
	{ 9600000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 5 },
	{ 15000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 3 },
	{ 19200000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 2, 5 },
	{ 24000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 2 },
	{ 48000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0,}
};

static struct qcom_clk_freq_tbl gcc_pcnoc_ahb_clk_src_freq_tbl[] = {
	{ 48000000,  "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 100000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(2), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl blsp1_uart1_apps_clk_src_freq_tbl[] = {
	{ 1843200, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 144, 15625 },
	{ 3686400, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 288, 15625 },
	{ 7372800, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 576, 15625 },
	{ 14745600, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1152, 15625 },
	{ 16000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 2, 25 },
	{ 24000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 2 },
	{ 32000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 4, 25 },
	{ 40000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 5 },
	{ 46400000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 29, 125 },
	{ 48000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl gp1_clk_src_freq_tbl[] = {
	{ 1250000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 16, 0 },
	{ 2500000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1),  8, 0 },
	{ 5000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(1),  4, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl sdcc1_apps_clk_src_freq_tbl[] = {
	{ 144000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 3, 240 },
	{ 400000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 0 },
	{ 20000000, "fepll500", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 25 },
	{ 25000000, "fepll500", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 20 },
	{ 50000000, "fepll500", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 10 },
	{ 100000000, "fepll500", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 1, 5 },
	{ 192000000, "ddrpllsdcc", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl apps_ahb_clk_src_freq_tbl[] = {
	{ 48000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 100000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(2), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl usb30_mock_utmi_clk_src_freq_tbl[] = {
	{ 2000000, "fepll200", QCOM_CLK_FREQTBL_PREDIV_RCG2(10), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl fephy_125m_dly_clk_src_freq_tbl[] = {
	{ 125000000, "fepll125dly", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl wcss2g_clk_src_freq_tbl[] = {
	{ 48000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 250000000, "fepllwcss2g", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0, }
};

static struct qcom_clk_freq_tbl wcss5g_clk_src_freq_tbl[] = {
	{ 48000000, "xo", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 250000000, "fepllwcss5g", QCOM_CLK_FREQTBL_PREDIV_RCG2(1), 0, 0 },
	{ 0, }
};

/*
 * Divisor table for the 2g/5g wifi clock divisors.
 */
static struct qcom_clk_ro_div_tbl fepllwcss_clk_div_tbl[] = {
	{ 0, 15 },
	{ 1, 16 },
	{ 2, 18 },
	{ 3, 20 },
	{ 0, 0 }
};

/*
 * Read-only divisor table clocks.
 */
static struct qcom_clk_ro_div_def ro_div_tbl[] = {
	F_RO_DIV(GCC_FEPLL_WCSS2G_CLK, "fepllwcss2g", "gcc_fepll_vco",
	     0x2f020, 8, 2, &fepllwcss_clk_div_tbl[0]),
	F_RO_DIV(GCC_FEPLL_WCSS5G_CLK, "fepllwcss5g", "gcc_fepll_vco",
	     0x2f020, 12, 2, &fepllwcss_clk_div_tbl[0]),
};

/*
 * RCG2 clocks
 */
static struct qcom_clk_rcg2_def rcg2_tbl[] = {
	F_RCG2(AUDIO_CLK_SRC, "audio_clk_src", gcc_xo_200_parents,
	    0x1b000, 5, 0, -1, -1, 0, 0, &audio_clk_src_freq_tbl[0]),
	F_RCG2(BLSP1_QUP1_I2C_APPS_CLK_SRC, "blsp1_qup1_i2c_apps_clk_src",
	    gcc_xo_200_parents, 0x200c, 5, 0, -1, -1, 0, 0,
	    &blsp1_qup1_i2c_apps_clk_src_freq_tbl[0]),
	F_RCG2(BLSP1_QUP2_I2C_APPS_CLK_SRC, "blsp1_qup2_i2c_apps_clk_src",
	    gcc_xo_200_parents, 0x3000, 5, 0, -1, -1, 0, 0,
	    &blsp1_qup1_i2c_apps_clk_src_freq_tbl[0]),
	F_RCG2(BLSP1_QUP1_SPI_APPS_CLK_SRC, "blsp1_qup1_spi_apps_clk_src",
	    gcc_xo_200_spi_parents, 0x2024, 5, 8, -1, -1, 0, 0,
	    &blsp1_qup1_spi_apps_clk_src_freq_tbl[0]),
	F_RCG2(BLSP1_QUP2_SPI_APPS_CLK_SRC, "blsp1_qup2_spi_apps_clk_src",
	    gcc_xo_200_spi_parents, 0x3014, 5, 8, -1, -1, 0, 0,
	    &blsp1_qup1_spi_apps_clk_src_freq_tbl[0]),
	F_RCG2(BLSP1_UART1_APPS_CLK_SRC, "blsp1_uart1_apps_clk_src",
	    gcc_xo_200_spi_parents, 0x2044, 5, 16, -1, -1, 0, 0,
	    &blsp1_uart1_apps_clk_src_freq_tbl[0]),
	F_RCG2(BLSP1_UART2_APPS_CLK_SRC, "blsp1_uart2_apps_clk_src",
	    gcc_xo_200_spi_parents, 0x3034, 5, 16, -1, -1, 0, 0,
	    &blsp1_uart1_apps_clk_src_freq_tbl[0]),
	F_RCG2(GP1_CLK_SRC, "gp1_clk_src", gcc_xo_200_parents, 0x8004,
	    5, 8, -1, -1, 0, 0,
	    &gp1_clk_src_freq_tbl[0]),
	F_RCG2(GP2_CLK_SRC, "gp2_clk_src", gcc_xo_200_parents, 0x9004,
	    5, 8, -1, -1, 0, 0,
	    &gp1_clk_src_freq_tbl[0]),
	F_RCG2(GP3_CLK_SRC, "gp3_clk_src", gcc_xo_200_parents, 0xa004,
	    5, 8, -1, -1, 0, 0,
	    &gp1_clk_src_freq_tbl[0]),
	F_RCG2(SDCC1_APPS_CLK_SRC, "sdcc1_apps_clk_src",
	    gcc_xo_sdcc1_500_parents, 0x18004, 5, 0, -1, -1, 0, 0,
	    &sdcc1_apps_clk_src_freq_tbl[0]),
	F_RCG2(GCC_APPS_CLK_SRC, "apps_clk_src", apps_clk_src_parents,
	    0x1900c, 5, 0, -1, 2, 0,
	    QCOM_CLK_RCG2_FLAGS_SET_RATE_PARENT,
	    &apps_clk_src_freq_tbl[0]),
	F_RCG2(GCC_APPS_AHB_CLK_SRC, "apps_ahb_clk_src",
	    gcc_xo_200_500_parents, 0x19014, 5, 0, -1, -1, 0,
	    0, &apps_ahb_clk_src_freq_tbl[0]),
	F_RCG2(GCC_USB3_MOCK_UTMI_CLK_SRC, "usb30_mock_utmi_clk_src",
	    gcc_xo_200_parents, 0x1e000, 5, 0, -1, -1, 0, 0,
	    &usb30_mock_utmi_clk_src_freq_tbl[0]),
	F_RCG2(FEPHY_125M_DLY_CLK_SRC, "fephy_125m_dly_clk_src",
	    gcc_xo_125_dly_parents, 0x12000, 5, 0, -1, -1, 0, 0,
	    &fephy_125m_dly_clk_src_freq_tbl[0]),
	F_RCG2(WCSS2G_CLK_SRC, "wcss2g_clk_src", gcc_xo_wcss2g_parents,
	    0x1f000, 5, 0, -1, -1, 0, 0,
	    &wcss2g_clk_src_freq_tbl[0]),
	F_RCG2(WCSS5G_CLK_SRC, "wcss5g_clk_src", gcc_xo_wcss5g_parents,
	    0x20000, 5, 0, -1, -1, 0, 0,
	    &wcss5g_clk_src_freq_tbl[0]),
	F_RCG2(GCC_PCNOC_AHB_CLK_SRC, "gcc_pcnoc_ahb_clk_src",
	    gcc_xo_200_500_parents, 0x21024, 5, 0, -1, -1, 0, 0,
	    &gcc_pcnoc_ahb_clk_src_freq_tbl[0]),
};

/*
 * branch2 clocks
 */
static struct qcom_clk_branch2_def branch2_tbl[] = {
	F_BRANCH2(GCC_AUDIO_AHB_CLK, "gcc_audio_ahb_clk", "pcnoc_clk_src",
	    0x1b010, 0, 0, 0, 0x1b010, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_AUDIO_PWM_CLK, "gcc_audio_pwm_clk", "audio_clk_src",
	    0x1b00c, 0, 0, 0, 0x1b00c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_QUP1_I2C_APPS_CLK, "gcc_blsp1_qup1_i2c_apps_clk",
	    "blsp1_qup1_i2c_apps_clk_src",
	    0x2008, 0, 0, 0, 0x2008, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_QUP2_I2C_APPS_CLK, "gcc_blsp1_qup2_i2c_apps_clk",
	    "blsp1_qup2_i2c_apps_clk_src",
	    0x3010, 0, 0, 0, 0x3010, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_QUP1_SPI_APPS_CLK, "gcc_blsp1_qup1_spi_apps_clk",
	    "blsp1_qup1_spi_apps_clk_src",
	    0x2004, 0, 0, 0, 0x2004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_QUP2_SPI_APPS_CLK, "gcc_blsp1_qup2_spi_apps_clk",
	    "blsp1_qup2_spi_apps_clk_src",
	    0x300c, 0, 0, 0, 0x300c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_UART1_APPS_CLK, "gcc_blsp1_uart1_apps_clk",
	    "blsp1_uart1_apps_clk_src",
	    0x203c, 0, 0, 0, 0x203c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_UART2_APPS_CLK, "gcc_blsp1_uart2_apps_clk",
	    "blsp1_uart2_apps_clk_src",
	    0x302c, 0, 0, 0, 0x302c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_GP1_CLK, "gcc_gp1_clk", "gp1_clk_src",
	    0x8000, 0, 0, 0, 0x8000, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_GP2_CLK, "gcc_gp2_clk", "gp2_clk_src",
	    0x9000, 0, 0, 0, 0x9000, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_GP3_CLK, "gcc_gp3_clk", "gp3_clk_src",
	    0xa000, 0, 0, 0, 0xa000, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	/* BRANCH_HALT_VOTED; note the different enable/halt */
	F_BRANCH2(GCC_APPS_AHB_CLK_SRC, "gcc_apss_ahb_clk",
	    "apps_ahb_clk_src",
	    0x6000, 14, 0, 0, 0x19004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_BLSP1_AHB_CLK, "gcc_blsp1_ahb_clk",
	    "pcnoc_clk_src",
	    0x6000, 10, 0, 0, 0x1008, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, 0), /* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_DCD_XO_CLK, "gcc_dcd_xo_clk", "xo",
	    0x2103c, 0, 0, 0, 0x2103c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_BOOT_ROM_AHB_CLK, "gcc_boot_rom_ahb_clk",
	    "pcnoc_clk_src", 0x1300c, 0, 0, 0, 0x1300c,
	    QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_CRYPTO_AHB_CLK, "gcc_crypto_ahb_clk",
	    "pcnoc_clk_src", 0x6000, 0, 0, 0, 0x16024,
	    QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, 0), /* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_CRYPTO_AXI_CLK, "gcc_crypto_axi_clk",
	    "fepll125", 0x6000, 1, 0, 0, 0x16020,
	    QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, 0), /* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_CRYPTO_CLK, "gcc_crypto_clk", "fepll125",
	    0x6000, 2, 0, 0, 0x1601c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, 0), /* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_ESS_CLK, "gcc_ess_clk", "fephy_125m_dly_clk_src",
	    0x12010, 0, 0, 0, 0x12010, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	/* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_IMEM_AXI_CLK, "gcc_imem_axi_clk", "fepll200",
	    0x6000, 17, 0, 0, 0xe004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_IMEM_CFG_AHB_CLK, "gcc_imem_cfg_ahb_clk",
	    "pcnoc_clk_src",
	    0xe008, 0, 0, 0, 0xe008, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_PCIE_AHB_CLK, "gcc_pcie_ahb_clk", "pcnoc_clk_src",
	    0x1d00c, 0, 0, 0, 0x1d00c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_PCIE_AXI_M_CLK, "gcc_pcie_axi_m_clk", "fepll200",
	    0x1d004, 0, 0, 0, 0x1d004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_PCIE_AXI_S_CLK, "gcc_pcie_axi_s_clk", "fepll200",
	    0x1d008, 0, 0, 0, 0x1d008, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_PRNG_AHB_CLK, "gcc_prng_ahb_clk", "pcnoc_clk_src",
	    0x6000, 8, 0, 0, 0x13004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, 0), /* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_QPIC_AHB_CLK, "gcc_qpic_ahb_clk", "pcnoc_clk_src",
	    0x1c008, 0, 0, 0, 0x1c008, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_QPIC_CLK, "gcc_qpic_clk", "pcnoc_clk_src",
	    0x1c004, 0, 0, 0, 0x1c004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_SDCC1_AHB_CLK, "gcc_sdcc1_ahb_clk", "pcnoc_clk_src",
	    0x18010, 0, 0, 0, 0x18010, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_SDCC1_APPS_CLK, "gcc_sdcc1_apps_clk",
	    "sdcc1_apps_clk_src", 0x1800c, 0, 0, 0, 0x1800c,
	    QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_TLMM_AHB_CLK, "gcc_tlmm_ahb_clk", "pcnoc_clk_src",
	    0x6000, 5, 0, 0, 0x5004, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    true, 0), /* BRANCH_HALT_VOTED */
	F_BRANCH2(GCC_USB2_MASTER_CLK, "gcc_usb2_master_clk", "pcnoc_clk_src",
	    0x1e00c, 0, 0, 0, 0x1e00c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_USB2_SLEEP_CLK, "gcc_usb2_sleep_clk",
	    "sleep_clk", 0x1e010, 0, 0, 0, 0x1e010,
	    QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_USB2_MOCK_UTMI_CLK, "gcc_usb2_mock_utmi_clk",
	    "usb30_mock_utmi_clk_src", 0x1e014, 0, 0, 0, 0x1e014,
	    QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_USB3_MASTER_CLK, "gcc_usb3_master_clk", "fepll125",
	    0x1e028, 0, 0, 0, 0x1e028, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_USB3_SLEEP_CLK, "gcc_usb3_sleep_clk", "sleep_clk",
	    0x1e02c, 0, 0, 0, 0x1e02c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),
	F_BRANCH2(GCC_USB3_MOCK_UTMI_CLK, "gcc_usb3_mock_utmi_clk",
	    "usb30_mock_utmi_clk_src",
	    0x1e030, 0, 0, 0, 0x1e030, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	/* Note - yes, these two have the same registers in linux */
	F_BRANCH2(GCC_WCSS2G_CLK, "gcc_wcss2g_clk", "wcss2g_clk_src",
	    0x1f00c, 0, 0, 0, 0x1f00c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_WCSS2G_REF_CLK, "gcc_wcss2g_ref_clk", "xo",
	    0x1f00c, 0, 0, 0, 0x1f00c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	/*
	 * TODO: figure out whether gcc_sleep_clk_src -> sleep_clk is right;
	 * will need to go consult the openwrt ipq4018 device tree / code
	 * again!
	 */
	F_BRANCH2(GCC_WCSS2G_RTC_CLK, "gcc_wcss2g_rtc_clk", "sleep_clk",
	    0x1f010, 0, 0, 0, 0x1f010, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),

	/* Note - yes, these two have the same registers in linux */
	F_BRANCH2(GCC_WCSS5G_CLK, "gcc_wcss5g_clk", "wcss5g_clk_src",
	    0x1f00c, 0, 0, 0, 0x2000c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_WCSS5G_REF_CLK, "gcc_wcss5g_ref_clk", "xo",
	    0x1f00c, 0, 0, 0, 0x2000c, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
	F_BRANCH2(GCC_WCSS5G_RTC_CLK, "gcc_wcss5g_rtc_clk", "sleep_clk",
	    0x1f010, 0, 0, 0, 0x20010, QCOM_CLK_BRANCH2_BRANCH_HALT,
	    false, 0),

	F_BRANCH2(GCC_PCNOC_AHB_CLK, "pcnoc_clk_src", "gcc_pcnoc_ahb_clk_src",
	    0x21030, 0, 0, 0, 0x21030, QCOM_CLK_BRANCH2_BRANCH_HALT, false,
	    QCOM_CLK_BRANCH2_FLAGS_CRITICAL |
	    QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT),
};

static void
qcom_gcc_ipq4018_clock_init_fepll(struct qcom_gcc_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(fepll_tbl); i++) {
		rv = qcom_clk_fepll_register(sc->clkdom, fepll_tbl + i);
		if (rv != 0)
			panic("qcom_clk_fepll_register failed");
	}
}

static void
qcom_gcc_ipq4018_clock_init_fdiv(struct qcom_gcc_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(fdiv_tbl); i++) {
		rv = qcom_clk_fdiv_register(sc->clkdom, fdiv_tbl + i);
		if (rv != 0)
			panic("qcom_clk_fdiv_register failed");
	}
}

static void
qcom_gcc_ipq4018_clock_init_apssdiv(struct qcom_gcc_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(apssdiv_tbl); i++) {
		rv = qcom_clk_apssdiv_register(sc->clkdom, apssdiv_tbl + i);
		if (rv != 0)
			panic("qcom_clk_apssdiv_register failed");
	}
}

static void
qcom_gcc_ipq4018_clock_init_rcg2(struct qcom_gcc_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(rcg2_tbl); i++) {
		rv = qcom_clk_rcg2_register(sc->clkdom, rcg2_tbl + i);
		if (rv != 0)
			panic("qcom_clk_rcg2_register failed");
	}
}

static void
qcom_gcc_ipq4018_clock_init_branch2(struct qcom_gcc_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(branch2_tbl); i++) {
		rv = qcom_clk_branch2_register(sc->clkdom, branch2_tbl + i);
		if (rv != 0)
			panic("qcom_clk_branch2_register failed");
	}
}

static void
qcom_gcc_ipq4018_clock_init_ro_div(struct qcom_gcc_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(ro_div_tbl); i++) {
		rv = qcom_clk_ro_div_register(sc->clkdom, ro_div_tbl + i);
		if (rv != 0)
			panic("qcom_clk_ro_div_register failed");
	}
}

void
qcom_gcc_ipq4018_clock_setup(struct qcom_gcc_softc *sc)
{

	sc->clkdom = clkdom_create(sc->dev);

	/* Setup stuff */
	qcom_gcc_ipq4018_clock_init_fepll(sc);
	qcom_gcc_ipq4018_clock_init_fdiv(sc);
	qcom_gcc_ipq4018_clock_init_apssdiv(sc);
	qcom_gcc_ipq4018_clock_init_rcg2(sc);
	qcom_gcc_ipq4018_clock_init_branch2(sc);
	qcom_gcc_ipq4018_clock_init_ro_div(sc);

	/* Finalise clock tree */
	clkdom_finit(sc->clkdom);
}
