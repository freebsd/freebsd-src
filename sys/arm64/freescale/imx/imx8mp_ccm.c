/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Tom Jones <thj@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/cdefs.h>

/*
 * Clocks driver for Freescale i.MX 8M Plus SoC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm64/freescale/imx/imx_ccm.h>
#include <arm64/freescale/imx/imx8mp_ccm.h>
#include <arm64/freescale/imx/clk/imx_clk_gate.h>
#include <arm64/freescale/imx/clk/imx_clk_mux.h>
#include <arm64/freescale/imx/clk/imx_clk_composite.h>
#include <arm64/freescale/imx/clk/imx_clk_sscg_pll.h>
#include <arm64/freescale/imx/clk/imx_clk_frac_pll.h>

static const char *pll_ref_p[] = {
	"osc_24m", "dummy", "dummy", "dummy"
};
static const char * audio_pll1_bypass_p[] = {
	"audio_pll1", "audio_pll1_ref_sel"
};
static const char * audio_pll2_bypass_p[] = {
	"audio_pll2", "audio_pll2_ref_sel"
};
static const char * video_pll1_bypass_p[] = {
	"video_pll1", "video_pll1_ref_sel"
};
static const char * dram_pll_bypass_p[] = {
	"dram_pll", "dram_pll_ref_sel"
};
static const char * gpu_pll_bypass_p[] = {
	"gpu_pll", "gpu_pll_ref_sel"
};
static const char * vpu_pll_bypass_p[] = {
	"vpu_pll", "vpu_pll_ref_sel"
};
static const char * arm_pll_bypass_p[] = {
	"arm_pll", "arm_pll_ref_sel"
};
static const char * sys_pll1_bypass_p[] = {
	"sys_pll1", "sys_pll1_ref_sel"
};
static const char * sys_pll2_bypass_p[] = {
	"sys_pll2", "sys_pll2_ref_sel"
};
static const char * sys_pll3_bypass_p[] = {
	"sys_pll3", "sys_pll3_ref_sel"
};

/* 
 * Table 5-1 of "i.MX 8M Plus Applications Processor Reference Manual" provides
 * the Clock Root Table.
 */
static const char *a53_p[] = {
	"osc_24m", "arm_pll_out", "sys_pll2_500m", "sys_pll2_1000m",
	"sys_pll1_800m", "sys_pll1_400m", "audio_pll1_out", "sys_pll3_out"
};
static const char * a53_core_p[] = {
	"arm_a53_div", "arm_pll_out"
};
static const char *ahb_p[] = {
	"osc_24m", "sys_pll1_133m", "sys_pll1_800m", "sys_pll1_400m",
	"sys_pll2_125m", "sys_pll3_out", "audio_pll1_out", "video_pll1_out"
};
static const char *audio_ahb_p[] = {
	"osc_24m", "sys_pll2_500m", "sys_pll1_800m", "sys_pll2_1000m",
	"sys_pll2_166m", "sys_pll3_out", "audio_pll1_out", "video_pll1_out"
};
static const char *audio_axi_p[] = {
	"osc_24m", "gpu_pll_out", "sys_pll1_800m", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *can_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll1_40m", "sys_pll1_160m",
	"sys_pll1_800m", "sys_pll3_out", "sys_pll2_250m", "audio_pll2_out"
};
static const char *clkout_p[] = {
	"audio_pll1_out", "audio_pll2_out", "video_pll1_out", "dummy", "dummy",
	"gpu_pll_out", "vpu_pll_out", "arm_pll_out", "sys_pll1", "sys_pll2",
	"sys_pll3", "dummy", "dummy", "osc_24m", "dummy", "osc_32k"
};
static const char *dram_alt_p[] = {
	"osc_24m", "sys_pll1_800m", "sys_pll1_100m", "sys_pll2_500m",
	"sys_pll2_1000m", "sys_pll3_out", "audio_pll1_out", "sys_pll1_266m"
};
static const char *dram_apb_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll1_40m", "sys_pll1_160m",
	"sys_pll1_800m", "sys_pll3_out", "sys_pll2_250m", "audio_pll2_out"
};
static const char *dram_core_p[] = {
	"dram_pll_out", "dram_alt_root"
};
static const char *ecspi_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll1_40m", "sys_pll1_160m",
	"sys_pll1_800m", "sys_pll3_out", "sys_pll2_250m", "audio_pll2_out"
};
static const char *enet_axi_p[] = {
	"osc_24m", "sys_pll1_266m", "sys_pll1_800m", "sys_pll2_250m",
	"sys_pll2_200m", "audio_pll1_out", "video_pll1_out", "sys_pll3_out"
};
static const char *enet_phy_ref_p[] = {
	"osc_24m", "sys_pll2_50m", "sys_pll2_125m", "sys_pll2_200m",
	"sys_pll2_500m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out",
};
static const char *enet_qos_p[] = {
	"osc_24m", "sys_pll2_125m", "sys_pll2_50m", "sys_pll2_100m",
	"sys_pll1_160m", "audio_pll1_out", "video_pll1_out", "clk_ext4",
};
static const char *enet_qos_timer_p[] = {
	"osc_24m", "sys_pll2_100m", "audio_pll1_out", "clk_ext1", "clk_ext2",
	"clk_ext3", "clk_ext4", "video_pll1_out",
};
static const char *enet_ref_p[] = {
	"osc_24m", "sys_pll2_125m", "sys_pll2_50m", "sys_pll2_100m",
	"sys_pll1_160m", "audio_pll1_out", "video_pll1_out", "clk_ext4",
};
static const char *enet_timer_p[] = {
	"osc_24m", "sys_pll2_100m", "audio_pll1_out", "clk_ext1", "clk_ext2",
	"clk_ext3", "clk_ext4", "video_pll1_out",
};
static const char *gic_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll1_40m", "sys_pll2_100m",
	"sys_pll1_800m", "sys_pll2_500m", "clk_ext4", "audio_pll2_out"
};
static const char *gpt_p[] = {
	"osc_24m", "sys_pll2_100m", "sys_pll1_400m", "sys_pll1_40m",
	"video_pll1_out", "sys_pll1_80m", "audio_pll1_out", "clk_ext1"
};
static const char *gpu_p[] = {
	"osc_24m", "gpu_pll_out", "sys_pll1_800m", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *gpu_ahb_p[] = {
	"osc_24m", "sys_pll1_800m", "gpu_pll_out", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *gpu_axi_p[] = {
	"osc_24m", "sys_pll1_800m", "gpu_pll_out", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *hdmi_24m_p[] = {
	"osc_24m", "sys_pll1_160m", "sys_pll2_50m", "sys_pll3_out",
	"audio_pll1_out", "video_pll1_out", "audio_pll2_out", "sys_pll1_133m"
};
static const char *hdmi_fdcc_tst_p[] = {
	"osc_24m", "sys_pll1_266m", "sys_pll2_250m", "sys_pll1_800m",
	"sys_pll2_1000m", "sys_pll3_out", "audio_pll2_out", "video_pll1_out"
};
static const char *hdmi_ref_266m_p[] = {
	"osc_24m", "sys_pll1_400m", "sys_pll3_out", "sys_pll2_333m",
	"sys_pll1_266m", "sys_pll2_200m", "audio_pll1_out", "video_pll1_out"
};
static const char *hsio_axi_p[] = {
	"osc_24m", "sys_pll2_500m", "sys_pll1_800m", "sys_pll2_100m",
	"sys_pll2_200m", "clk_ext2", "clk_ext4", "audio_pll2_out"
};
static const char *i2c_p[] = {
	"osc_24m", "sys_pll1_160m", "sys_pll2_50m", "sys_pll3_out",
	"audio_pll1_out", "video_pll1_out", "audio_pll2_out", "sys_pll1_133m"
};
static const char *ipp_do_clko1_p[] = {
	"osc_24m", "sys_pll1_800m", "sys_pll1_133m", "sys_pll1_200m",
	"audio_pll2_out", "sys_pll2_500m", "vpu_pll_out", "sys_pll1_80m"
};
static const char *ipp_do_clko2_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll1_400m", "sys_pll2_166m",
	"sys_pll3_out", "audio_pll1_out", "video_pll1_out", "osc_32k"
};
static const char *m7_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll2_250m", "vpu_pll_out",
	"sys_pll1_800m", "audio_pll1_out", "video_pll1_out", "sys_pll3_out"
};
static const char *main_axi_p[] = {
	"osc_24m", "sys_pll2_333m", "sys_pll1_800m", "sys_pll2_250m",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "sys_pll1_100m"
};
static const char *media_apb_p[] = {
	"osc_24m", "sys_pll2_125m", "sys_pll1_800m", "sys_pll3_out",
	"sys_pll1_40m", "audio_pll2_out", "clk_ext1", "sys_pll1_133m"
};
static const char *media_axi_p[] = {
	"osc_24m", "sys_pll2_1000m", "sys_pll1_800m", "sys_pll3_out",
	"sys_pll1_40m", "audio_pll2_out", "clk_ext1", "sys_pll2_500m"
};
static const char *media_cam1_pix_p[] = {
	"osc_24m", "sys_pll1_266m", "sys_pll2_250m", "sys_pll1_800m",
	"sys_pll2_1000m", "sys_pll3_out", "audio_pll2_out", "video_pll1_out"
};
static const char *media_cam2_pix_p[] = {
		"osc_24m", "sys_pll1_266m", "sys_pll2_250m", "sys_pll1_800m",
		"sys_pll2_1000m", "sys_pll3_out", "audio_pll2_out",
		"video_pll1_out"
};
static const char *media_disp_pix_p[] = {
	"osc_24m", "video_pll1_out", "audio_pll2_out", "audio_pll1_out",
	"sys_pll1_800m", "sys_pll2_1000m", "sys_pll3_out", "clk_ext4"
};
static const char *media_isp_p[] = {
	"osc_24m", "sys_pll2_1000m", "sys_pll1_800m", "sys_pll3_out",
	"sys_pll1_400m", "audio_pll2_out", "clk_ext1", "sys_pll2_500m"
};
static const char *media_mipi_phy1_ref_p[] = {
	"osc_24m", "sys_pll2_333m", "sys_pll2_100m", "sys_pll1_800m",
	"sys_pll2_1000m", "clk_ext2", "audio_pll2_out", "video_pll1_out"
};
static const char *media_mipi_test_byte_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll2_50m", "sys_pll3_out",
	"sys_pll2_100m", "sys_pll1_80m", "sys_pll1_160m", "sys_pll1_200m"
};
static const char *media_ldb_p[] = {
	"osc_24m", "sys_pll2_333m", "sys_pll2_100m", "sys_pll1_800m",
	"sys_pll2_1000m", "clk_ext2", "audio_pll2_out", "video_pll1_out"
};
static const char *memrepair_p[] = {
	"osc_24m", "sys_pll2_100m", "sys_pll1_80m", "sys_pll1_800m",
	"sys_pll2_1000m", "sys_pll3_out", "clk_ext3", "audio_pll2_out"
};
static const char *mipi_dsi_esc_rx_p[] = {
	"osc_24m", "sys_pll2_100m", "sys_pll1_80m", "sys_pll1_800m",
	"sys_pll2_1000m", "sys_pll3_out", "clk_ext3", "audio_pll2_out"
};
static const char *ml_p[] = {
	"osc_24m", "gpu_pll_out", "sys_pll1_800m", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *ml_ahb_p[] = {
	"osc_24m", "sys_pll1_800m", "gpu_pll_out", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *ml_axi_p[] = {
	"osc_24m", "sys_pll1_800m", "gpu_pll_out", "sys_pll3_out",
	"sys_pll2_1000m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *nand_p[] = {
	"osc_24m", "sys_pll2_500m", "audio_pll1_out", "sys_pll1_400m",
	"audio_pll2_out", "sys_pll3_out", "sys_pll2_250m", "video_pll1_out"
};
static const char *noc_p[] = {
	"osc_24m", "sys_pll1_800m", "sys_pll3_out", "sys_pll2_1000m",
	"sys_pll2_500m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *noc_io_p[] = {
	"osc_24m", "sys_pll1_800m", "sys_pll3_out", "sys_pll2_1000m",
	"sys_pll2_500m", "audio_pll1_out", "video_pll1_out", "audio_pll2_out"
};
static const char *pcie_aux_p[] = {
	"osc_24m", "sys_pll2_200m", "sys_pll2_50m", "sys_pll3_out",
	"sys_pll2_100m", "sys_pll1_80m", "sys_pll1_160m", "sys_pll1_200m"
};
static const char *pdm_p[] = {
	"osc_24m", "sys_pll2_100m", "audio_pll1_out", "sys_pll1_800m",
	"sys_pll2_1000m", "sys_pll3_out", "clk_ext3", "audio_pll2_out"
};
static const char *pwm_p[] = {
	"osc_24m", "sys_pll2_100m", "sys_pll1_160m", "sys_pll1_40m",
	"sys_pll3_out", "clk_ext1", "sys_pll1_80m", "video_pll1_out"
};
static const char *qspi_p[] = {
	"osc_24m", "sys_pll1_400m", "sys_pll2_333m", "sys_pll2_500m",
	"audio_pll2_out", "sys_pll1_266m", "sys_pll3_out", "sys_pll1_100m"
};
static const char *sai1_p[] = {
	"osc_24m" , "sys_pll1_133m" , "audio_pll1_out", "audio_pll2_out",
	"video_pll1_out", "clk_ext1", "clk_ext2", "dummy",
};
static const char *sai2_p[] = {
	"osc_24m" , "sys_pll1_133m" , "audio_pll1_out", "audio_pll2_out",
	"video_pll1_out", "clk_ext2", "clk_ext3", "dummy",
};
static const char *sai3_p[] = {
	"osc_24m" , "sys_pll1_133m" , "audio_pll1_out", "audio_pll2_out",
	"video_pll1_out", "clk_ext3", "clk_ext4", "dummy",
};
static const char *sai5_p[] = {
	"osc_24m" , "sys_pll1_133m" , "audio_pll1_out", "audio_pll2_out",
	"video_pll1_out", "clk_ext2", "clk_ext3", "dummy",
};
static const char *sai6_p[] = {
	"osc_24m" , "sys_pll1_133m" , "audio_pll1_out", "audio_pll2_out",
	"video_pll1_out", "clk_ext3", "clk_ext4", "dummy",
};
static const char *sai7_p[] = {
	"osc_24m" , "sys_pll1_133m" , "audio_pll1_out", "audio_pll2_out",
	"video_pll1_out", "clk_ext3", "clk_ext4", "dummy",
};
static const char *uart_p[] = {
	"osc_24m", "sys_pll1_80m", "sys_pll2_200m", "sys_pll2_100m",
	"sys_pll3_out", "clk_ext2", "clk_ext4", "audio_pll2_out"
};
static const char *usb_core_ref_p[] = {
	"osc_24m", "sys_pll1_100m", "sys_pll1_40m", "sys_pll2_100m",
	"sys_pll2_200m", "clk_ext2", "clk_ext3", "audio_pll2_out"
};
static const char *usdhc_p[] = {
	"osc_24m", "sys_pll1_400m", "sys_pll1_800m", "sys_pll2_500m",
	"sys_pll3_out", "sys_pll1_266m", "audio_pll2_out", "sys_pll1_100m"
};
static const char *usb_phy_ref_p[] = {
	"osc_24m", "sys_pll1_100m", "sys_pll1_40m", "sys_pll2_100m",
	"sys_pll2_200m", "clk_ext2", "clk_ext3", "audio_pll2_out"
};
static const char *usdhc_nand_p[] = {
	"osc_24m", "sys_pll1_266m", "sys_pll1_800m", "sys_pll2_200m",
	"sys_pll1_133m", "sys_pll3_out", "sys_pll2_250m", "audio_pll1_out"
};
static const char *vpu_bus_p[] = {
	"osc_24m", "sys_pll1_800m", "vpu_pll_out", "audio_pll2_out",
	"sys_pll3_out", "sys_pll2_1000m", "sys_pll2_200m", "sys_pll1_100m"
};
static const char *vpu_g_p[] = {
	"osc_24m", "vpu_pll_out", "sys_pll1_800m", "sys_pll2_1000m",
	"sys_pll1_100m", "sys_pll2_125m", "sys_pll3_out", "audio_pll1_out"
};
static const char *vpu_vc8000e_p[] = {
	"osc_24m", "vpu_pll_out", "sys_pll1_800m", "sys_pll2_1000m",
	"audio_pll2_out", "sys_pll2_125m", "sys_pll3_out", "audio_pll1_out"
};
static const char *wdog_p[] = {
	"osc_24m", "sys_pll1_133m", "sys_pll1_160m", "vpu_pll_out",
	"sys_pll2_125m", "sys_pll3_out", "sys_pll1_80m", "sys_pll2_166m"
};
static const char *wrclk_p[] = {
	"osc_24m", "sys_pll1_40m", "vpu_pll_out", "sys_pll3_out",
	"sys_pll2_200m", "sys_pll1_266m", "sys_pll2_500m", "sys_pll1_100m"
};

static struct imx_clk imx8mp_clks[] = {
	FIXED(IMX8MP_CLK_DUMMY, "dummy", 0),

	LINK(IMX8MP_CLK_32K, "osc_32k"),
	LINK(IMX8MP_CLK_24M, "osc_24m"),
	LINK(IMX8MP_CLK_EXT1, "clk_ext1"),
	LINK(IMX8MP_CLK_EXT2, "clk_ext2"),
	LINK(IMX8MP_CLK_EXT3, "clk_ext3"),
	LINK(IMX8MP_CLK_EXT4, "clk_ext4"),

	MUX(IMX8MP_AUDIO_PLL1_REF_SEL, "audio_pll1_ref_sel", pll_ref_p, 0, 0x00, 0, 2),
	MUX(IMX8MP_AUDIO_PLL2_REF_SEL, "audio_pll2_ref_sel", pll_ref_p, 0, 0x14, 0, 2),
	MUX(IMX8MP_VIDEO_PLL1_REF_SEL, "video_pll1_ref_sel", pll_ref_p, 0, 0x28, 0, 2),
	MUX(IMX8MP_DRAM_PLL_REF_SEL, "dram_pll_ref_sel", pll_ref_p, 0, 0x50, 0, 2),
	MUX(IMX8MP_GPU_PLL_REF_SEL, "gpu_pll_ref_sel", pll_ref_p, 0, 0x64, 0, 2),
	MUX(IMX8MP_VPU_PLL_REF_SEL, "vpu_pll_ref_sel", pll_ref_p, 0, 0x74, 0, 2),
	MUX(IMX8MP_ARM_PLL_REF_SEL, "arm_pll_ref_sel", pll_ref_p, 0, 0x84, 0, 2),
	MUX(IMX8MP_SYS_PLL1_REF_SEL, "sys_pll1_ref_sel", pll_ref_p, 0, 0x94, 0, 2),
	MUX(IMX8MP_SYS_PLL2_REF_SEL, "sys_pll2_ref_sel", pll_ref_p, 0, 0x104, 0, 2),
	MUX(IMX8MP_SYS_PLL3_REF_SEL, "sys_pll3_ref_sel", pll_ref_p, 0, 0x114, 0, 2),

	FRAC_PLL(IMX8MP_AUDIO_PLL1, "audio_pll1", "audio_pll1_ref_sel", 0x00),
	FRAC_PLL(IMX8MP_AUDIO_PLL2, "audio_pll2", "audio_pll2_ref_sel", 0x14),
	FRAC_PLL(IMX8MP_VIDEO_PLL1, "video_pll1", "video_pll1_ref_sel", 0x28),
	FRAC_PLL(IMX8MP_DRAM_PLL, "dram_pll", "dram_pll_ref_sel", 0x50),
	FRAC_PLL(IMX8MP_GPU_PLL, "gpu_pll", "gpu_pll_ref_sel", 0x64),
	FRAC_PLL(IMX8MP_VPU_PLL, "vpu_pll", "vpu_pll_ref_sel", 0x74),
	FRAC_PLL(IMX8MP_ARM_PLL, "arm_pll", "arm_pll_ref_sel", 0x84),
	FRAC_PLL(IMX8MP_SYS_PLL1, "sys_pll1", "sys_pll1_ref_sel", 0x94),
	FRAC_PLL(IMX8MP_SYS_PLL2, "sys_pll2", "sys_pll2_ref_sel", 0x104),
	FRAC_PLL(IMX8MP_SYS_PLL3, "sys_pll3", "sys_pll3_ref_sel", 0x114),

	MUX(IMX8MP_AUDIO_PLL1_BYPASS, "audio_pll1_bypass", audio_pll1_bypass_p, 1, 0x00, 16, 1),
	MUX(IMX8MP_AUDIO_PLL2_BYPASS, "audio_pll2_bypass", audio_pll2_bypass_p, 1, 0x14, 16, 1),
	MUX(IMX8MP_VIDEO_PLL1_BYPASS, "video_pll1_bypass", video_pll1_bypass_p, 1, 0x28, 16, 1),
	MUX(IMX8MP_DRAM_PLL_BYPASS, "dram_pll_bypass", dram_pll_bypass_p, 1, 0x50, 16, 1),
	MUX(IMX8MP_GPU_PLL_BYPASS, "gpu_pll_bypass", gpu_pll_bypass_p, 1, 0x64, 28, 1),
	MUX(IMX8MP_VPU_PLL_BYPASS, "vpu_pll_bypass", vpu_pll_bypass_p, 1, 0x74, 28, 1),
	MUX(IMX8MP_ARM_PLL_BYPASS, "arm_pll_bypass", arm_pll_bypass_p, 1, 0x84, 28, 1),
	MUX(IMX8MP_SYS_PLL1_BYPASS, "sys_pll1_bypass", sys_pll1_bypass_p, 1, 0x94, 28, 1),
	MUX(IMX8MP_SYS_PLL2_BYPASS, "sys_pll2_bypass", sys_pll2_bypass_p, 1, 0x104, 28, 1),
	MUX(IMX8MP_SYS_PLL3_BYPASS, "sys_pll3_bypass", sys_pll3_bypass_p, 1, 0x114, 28, 1),

	GATE(IMX8MP_AUDIO_PLL1_OUT, "audio_pll1_out", "audio_pll1_bypass", 0x00, 13),
	GATE(IMX8MP_AUDIO_PLL2_OUT, "audio_pll2_out", "audio_pll2_bypass", 0x14, 13),
	GATE(IMX8MP_VIDEO_PLL1_OUT, "video_pll1_out", "video_pll1_bypass", 0x28, 13),
	GATE(IMX8MP_DRAM_PLL_OUT, "dram_pll_out", "dram_pll_bypass", 0x50, 13),
	GATE(IMX8MP_GPU_PLL_OUT, "gpu_pll_out", "gpu_pll_bypass", 0x64, 11),
	GATE(IMX8MP_VPU_PLL_OUT, "vpu_pll_out", "vpu_pll_bypass", 0x74, 11),
	GATE(IMX8MP_ARM_PLL_OUT, "arm_pll_out", "arm_pll_bypass", 0x84, 11),
	GATE(IMX8MP_SYS_PLL1_OUT, "sys_pll1_out", "sys_pll1_bypass", 0x94, 11),
	GATE(IMX8MP_SYS_PLL2_OUT, "sys_pll2_out", "sys_pll2_bypass", 0x104, 11),
	GATE(IMX8MP_SYS_PLL3_OUT, "sys_pll3_out", "sys_pll3_bypass", 0x114, 11),

	FFACT(IMX8MP_SYS_PLL1_40M, "sys_pll1_40m", "sys_pll1_out", 1, 20),
	FFACT(IMX8MP_SYS_PLL1_80M, "sys_pll1_80m", "sys_pll1_out", 1, 10),
	FFACT(IMX8MP_SYS_PLL1_100M, "sys_pll1_100m", "sys_pll1_out", 1, 8),
	FFACT(IMX8MP_SYS_PLL1_133M, "sys_pll1_133m", "sys_pll1_out", 1, 6),
	FFACT(IMX8MP_SYS_PLL1_160M, "sys_pll1_160m", "sys_pll1_out", 1, 5),
	FFACT(IMX8MP_SYS_PLL1_200M, "sys_pll1_200m", "sys_pll1_out", 1, 4),
	FFACT(IMX8MP_SYS_PLL1_266M, "sys_pll1_266m", "sys_pll1_out", 1, 3),
	FFACT(IMX8MP_SYS_PLL1_400M, "sys_pll1_400m", "sys_pll1_out", 1, 2),
	FFACT(IMX8MP_SYS_PLL1_800M, "sys_pll1_800m", "sys_pll1_out", 1, 1),

	FFACT(IMX8MP_SYS_PLL2_50M, "sys_pll2_50m", "sys_pll2_out", 1, 20),
	FFACT(IMX8MP_SYS_PLL2_100M, "sys_pll2_100m", "sys_pll2_out", 1, 10),
	FFACT(IMX8MP_SYS_PLL2_125M, "sys_pll2_125m", "sys_pll2_out", 1, 8),
	FFACT(IMX8MP_SYS_PLL2_166M, "sys_pll2_166m", "sys_pll2_out", 1, 6),
	FFACT(IMX8MP_SYS_PLL2_200M, "sys_pll2_200m", "sys_pll2_out", 1, 5),
	FFACT(IMX8MP_SYS_PLL2_250M, "sys_pll2_250m", "sys_pll2_out", 1, 4),
	FFACT(IMX8MP_SYS_PLL2_333M, "sys_pll2_333m", "sys_pll2_out", 1, 3),
	FFACT(IMX8MP_SYS_PLL2_500M, "sys_pll2_500m", "sys_pll2_out", 1, 2),
	FFACT(IMX8MP_SYS_PLL2_1000M, "sys_pll2_1000m", "sys_pll2_out", 1, 1),

	MUX(IMX8MP_CLK_CLKOUT1_SEL, "clkout1_sel", clkout_p, 0x128, 4, 4, 1),
	DIV(IMX8MP_CLK_CLKOUT1_DIV, "clkout1_div", "clkout1_sel", 0x128, 0, 4),
	GATE(IMX8MP_CLK_CLKOUT1, "clkout1", "clkout1_div", 0x128, 8),

	MUX(IMX8MP_CLK_CLKOUT2_SEL, "clkout2_sel", clkout_p, 0x128, 20, 4, 1),
	DIV(IMX8MP_CLK_CLKOUT2_DIV, "clkout2_div", "clkout2_sel", 0x128, 16, 4),
	GATE(IMX8MP_CLK_CLKOUT2, "clkout2", "clkout2_div", 0x128, 24),

	COMPOSITE(IMX8MP_CLK_A53_DIV, "arm_a53_div", a53_p, 0x8000, 0),
	COMPOSITE(IMX8MP_CLK_M7_CORE, "m7_core", m7_p, 0x8080, 0),
	COMPOSITE(IMX8MP_CLK_ML_CORE, "ml_core", ml_p, 0x8100, 0),
	COMPOSITE(IMX8MP_CLK_GPU3D_CORE, "gpu3d_core", gpu_p, 0x8180, 0),
	COMPOSITE(IMX8MP_CLK_GPU3D_SHADER_CORE, "gpu3d_shader", gpu_p, 0x8200, 0),
	COMPOSITE(IMX8MP_CLK_GPU2D_CORE, "gpu2d_core", gpu_p, 0x8280, 0),
	COMPOSITE(IMX8MP_CLK_AUDIO_AXI, "audio_axi", audio_axi_p, 0x8300, 0),
	COMPOSITE(IMX8MP_CLK_HSIO_AXI, "hsio_axi", hsio_axi_p, 0x8380, 0),
	COMPOSITE(IMX8MP_CLK_MEDIA_ISP, "media_isp", media_isp_p, 0x8400, 0),
	COMPOSITE(IMX8MP_CLK_NAND_USDHC_BUS, "nand_usdhc_bus", usdhc_nand_p, 0x8900, 1),

	MUX(IMX8MP_CLK_A53_CORE, "arm_a53_core", a53_core_p, 0x9880, 24, 1, 1),

	COMPOSITE(IMX8MP_CLK_MAIN_AXI, "main_axi", main_axi_p, 0x8800, 1),
	COMPOSITE(IMX8MP_CLK_ENET_AXI, "enet_axi", enet_axi_p, 0x8880, 1),
	COMPOSITE(IMX8MP_CLK_VPU_BUS, "vpu_bus", vpu_bus_p, 0x8980, 1),
	COMPOSITE(IMX8MP_CLK_MEDIA_AXI, "media_axi", media_axi_p, 0x8a00, 1),
	COMPOSITE(IMX8MP_CLK_MEDIA_APB, "media_apb", media_apb_p, 0x8a80, 1),
	COMPOSITE(IMX8MP_CLK_HDMI_APB, "hdmi_apb", media_apb_p, 0x8b00, 1),
	COMPOSITE(IMX8MP_CLK_HDMI_AXI, "hdmi_axi", media_axi_p, 0x8b80, 1),
	COMPOSITE(IMX8MP_CLK_GPU_AXI, "gpu_axi", gpu_axi_p, 0x8c00, 1),
	COMPOSITE(IMX8MP_CLK_GPU_AHB, "gpu_ahb", gpu_ahb_p, 0x8c80, 1),
	COMPOSITE(IMX8MP_CLK_NOC, "noc", noc_p, 0x8d00, 1),
	COMPOSITE(IMX8MP_CLK_NOC_IO, "noc_io", noc_io_p, 0x8d80, 1),
	COMPOSITE(IMX8MP_CLK_ML_AXI, "ml_axi", ml_axi_p, 0x8e00, 1),
	COMPOSITE(IMX8MP_CLK_ML_AHB, "ml_ahb", ml_ahb_p, 0x8e80, 1),

	COMPOSITE(IMX8MP_CLK_AHB, "ahb_root", ahb_p, 0x9000, 1),
	COMPOSITE(IMX8MP_CLK_AUDIO_AHB, "audio_ahb", audio_ahb_p, 0x9100, 1),
	COMPOSITE(IMX8MP_CLK_MIPI_DSI_ESC_RX, "mipi_dsi_esc_rx", mipi_dsi_esc_rx_p, 0x9200, 1),
	COMPOSITE(IMX8MP_CLK_MEDIA_DISP2_PIX, "media_disp2_pix", media_disp_pix_p, 0x9300, 1),

	DIV(IMX8MP_CLK_IPG_ROOT, "ipg_root", "ahb_root", 0x9080, 0, 1),

	COMPOSITE(IMX8MP_CLK_DRAM_ALT, "dram_alt", dram_alt_p, 0xa000, 0),
	COMPOSITE(IMX8MP_CLK_DRAM_APB, "dram_apb", dram_apb_p, 0xa080, 0),

	COMPOSITE(IMX8MP_CLK_VPU_G1, "vpu_g1", vpu_g_p, 0xa100, 0),
	COMPOSITE(IMX8MP_CLK_VPU_G2, "vpu_g2", vpu_g_p, 0xa180, 0),

	COMPOSITE(IMX8MP_CLK_CAN1, "can1", can_p, 0xa200, 0),
	COMPOSITE(IMX8MP_CLK_CAN2, "can2", can_p, 0xa280, 0),

	COMPOSITE(IMX8MP_CLK_PCIE_AUX, "pcie_aux", pcie_aux_p, 0xa400, 0),

	COMPOSITE(IMX8MP_CLK_SAI1, "sai1", sai1_p, 0xa580, 0),
	COMPOSITE(IMX8MP_CLK_SAI2, "sai2", sai2_p, 0xa600, 0),
	COMPOSITE(IMX8MP_CLK_SAI3, "sai3", sai3_p, 0xa680, 0),
	COMPOSITE(IMX8MP_CLK_SAI5, "sai5", sai5_p, 0xa780, 0),
	COMPOSITE(IMX8MP_CLK_SAI6, "sai6", sai6_p, 0xa800, 0),
	COMPOSITE(IMX8MP_CLK_SAI7, "sai7", sai7_p, 0xc300, 0),

	COMPOSITE(IMX8MP_CLK_ENET_QOS, "enet_qos", enet_qos_p, 0xa880, 0),
	COMPOSITE(IMX8MP_CLK_ENET_QOS_TIMER, "enet_qos_timer", enet_qos_timer_p, 0xa900, 0),
	COMPOSITE(IMX8MP_CLK_ENET_REF, "enet_ref", enet_ref_p, 0xa980, 0),
	COMPOSITE(IMX8MP_CLK_ENET_TIMER, "enet_timer", enet_timer_p, 0xaa00, 0),
	COMPOSITE(IMX8MP_CLK_ENET_PHY_REF, "enet_phy_ref", enet_phy_ref_p, 0xaa80, 0),

	COMPOSITE(IMX8MP_CLK_NAND, "nand", nand_p, 0xab00, 0),
	COMPOSITE(IMX8MP_CLK_QSPI, "qspi", qspi_p, 0xab80, 0),

	COMPOSITE(IMX8MP_CLK_USDHC1, "usdhc1", usdhc_p, 0xac00, 0),
	COMPOSITE(IMX8MP_CLK_USDHC2, "usdhc2", usdhc_p, 0xac80, 0),
	COMPOSITE(IMX8MP_CLK_USDHC3, "usdhc3", usdhc_p, 0xbc80, 0),

	COMPOSITE(IMX8MP_CLK_I2C1, "i2c1", i2c_p, 0xad00, 0),
	COMPOSITE(IMX8MP_CLK_I2C2, "i2c2", i2c_p, 0xad80, 0),
	COMPOSITE(IMX8MP_CLK_I2C3, "i2c3", i2c_p, 0xae00, 0),
	COMPOSITE(IMX8MP_CLK_I2C4, "i2c4", i2c_p, 0xae80, 0),
	COMPOSITE(IMX8MP_CLK_I2C5, "i2c5", i2c_p, 0xa480, 0),
	COMPOSITE(IMX8MP_CLK_I2C6, "i2c6", i2c_p, 0xa500, 0),

	COMPOSITE(IMX8MP_CLK_UART1, "uart1", uart_p, 0xaf00, 0),
	COMPOSITE(IMX8MP_CLK_UART2, "uart2", uart_p, 0xaf80, 0),
	COMPOSITE(IMX8MP_CLK_UART3, "uart3", uart_p, 0xb000, 0),
	COMPOSITE(IMX8MP_CLK_UART4, "uart4", uart_p, 0xb080, 0),

	COMPOSITE(IMX8MP_CLK_USB_CORE_REF, "usb_core_ref", usb_core_ref_p, 0xb100, 0),
	COMPOSITE(IMX8MP_CLK_USB_PHY_REF, "usb_phy_ref", usb_phy_ref_p, 0xb180, 0),

	COMPOSITE(IMX8MP_CLK_GIC, "gic", gic_p, 0xb200, 0),

	COMPOSITE(IMX8MP_CLK_ECSPI1, "ecspi1", ecspi_p, 0xb280, 0),
	COMPOSITE(IMX8MP_CLK_ECSPI2, "ecspi2", ecspi_p, 0xb300, 0),
	COMPOSITE(IMX8MP_CLK_ECSPI3, "ecspi3", ecspi_p, 0xc180, 0),

	COMPOSITE(IMX8MP_CLK_PWM1, "pwm1", pwm_p, 0xb380, 0),
	COMPOSITE(IMX8MP_CLK_PWM2, "pwm2", pwm_p, 0xb400, 0),
	COMPOSITE(IMX8MP_CLK_PWM3, "pwm3", pwm_p, 0xb480, 0),
	COMPOSITE(IMX8MP_CLK_PWM4, "pwm4", pwm_p, 0xb500, 0),

	COMPOSITE(IMX8MP_CLK_GPT1, "gpt1", gpt_p, 0xb580, 0),
	COMPOSITE(IMX8MP_CLK_GPT2, "gpt2", gpt_p, 0xb600, 0),
	COMPOSITE(IMX8MP_CLK_GPT3, "gpt3", gpt_p, 0xb680, 0),
	COMPOSITE(IMX8MP_CLK_GPT4, "gpt4", gpt_p, 0xb700, 0),
	COMPOSITE(IMX8MP_CLK_GPT5, "gpt5", gpt_p, 0xb780, 0),
	COMPOSITE(IMX8MP_CLK_GPT6, "gpt6", gpt_p, 0xb800, 0),

	COMPOSITE(IMX8MP_CLK_WDOG, "wdog", wdog_p, 0xb900, 0),
	COMPOSITE(IMX8MP_CLK_WRCLK, "wrclk", wrclk_p, 0xb980, 0),

	COMPOSITE(IMX8MP_CLK_IPP_DO_CLKO1, "ipp_do_clko1", ipp_do_clko1_p, 0xba00, 0),
	COMPOSITE(IMX8MP_CLK_IPP_DO_CLKO2, "ipp_do_clko2", ipp_do_clko2_p, 0xba80, 0),

	COMPOSITE(IMX8MP_CLK_HDMI_FDCC_TST, "hdmi_fdcc_tst", hdmi_fdcc_tst_p, 0xbb00, 0),
	COMPOSITE(IMX8MP_CLK_HDMI_24M, "hdmi_24m", hdmi_24m_p, 0xbb80, 0),
	COMPOSITE(IMX8MP_CLK_HDMI_REF_266M, "hdmi_ref_266m", hdmi_ref_266m_p, 0xbc00, 0),
	COMPOSITE(IMX8MP_CLK_MEDIA_CAM1_PIX, "media_cam1_pix", media_cam1_pix_p, 0xbd00, 0),
	COMPOSITE(IMX8MP_CLK_MEDIA_MIPI_PHY1_REF, "media_mipi_phy1_ref", media_mipi_phy1_ref_p, 0xbd80, 0),
	COMPOSITE(IMX8MP_CLK_MEDIA_DISP1_PIX, "media_disp1_pix", media_disp_pix_p, 0xbe00, 0),
	COMPOSITE(IMX8MP_CLK_MEDIA_CAM2_PIX, "media_cam2_pix", media_cam2_pix_p, 0xbe80, 0),
	COMPOSITE(IMX8MP_CLK_MEDIA_LDB, "media_ldb", media_ldb_p, 0xbf00, 0),

	COMPOSITE(IMX8MP_CLK_MEMREPAIR, "mem_repair", memrepair_p, 0xbf80, 0),

	COMPOSITE(IMX8MP_CLK_MEDIA_MIPI_TEST_BYTE, "media_mipi_test_byte", media_mipi_test_byte_p, 0xc100, 0),
	COMPOSITE(IMX8MP_CLK_PDM, "pdm", pdm_p, 0xc200, 0),
	COMPOSITE(IMX8MP_CLK_VPU_VC8000E, "vpu_vc8000e", vpu_vc8000e_p, 0xc280, 0),

	FFACT(IMX8MP_CLK_DRAM_ALT_ROOT, "dram_alt_root", "dram_alt", 1, 4),

	MUX(IMX8MP_CLK_DRAM_CORE, "dram_core_clk", dram_core_p, 0x9800, 24, 1, 1),
	ROOT_GATE(IMX8MP_CLK_DRAM1_ROOT, "dram1_root_clk", "dram_core_clk", 0x4050),

	ROOT_GATE(IMX8MP_CLK_ECSPI1_ROOT, "ecspi1_root_clk", "ecspi1", 0x4070),
	ROOT_GATE(IMX8MP_CLK_ECSPI2_ROOT, "ecspi2_root_clk", "ecspi2", 0x4080),
	ROOT_GATE(IMX8MP_CLK_ECSPI3_ROOT, "ecspi3_root_clk", "ecspi3", 0x4090),
	ROOT_GATE(IMX8MP_CLK_ENET1_ROOT, "enet1_root_clk", "enet_axi", 0x40a0),

	ROOT_GATE(IMX8MP_CLK_GPIO1_ROOT ,"gpio1_root_clk", "ipg_root", 0x40b0),
	ROOT_GATE(IMX8MP_CLK_GPIO2_ROOT ,"gpio2_root_clk", "ipg_root", 0x40c0),
	ROOT_GATE(IMX8MP_CLK_GPIO3_ROOT ,"gpio3_root_clk", "ipg_root", 0x40d0),
	ROOT_GATE(IMX8MP_CLK_GPIO4_ROOT ,"gpio4_root_clk", "ipg_root", 0x40e0),
	ROOT_GATE(IMX8MP_CLK_GPIO5_ROOT ,"gpio5_root_clk", "ipg_root", 0x40f0),

	ROOT_GATE(IMX8MP_CLK_GPT1_ROOT, "gpt1_root_clk", "gpt1", 0x4100),
	ROOT_GATE(IMX8MP_CLK_GPT2_ROOT, "gpt2_root_clk", "gpt2", 0x4110),
	ROOT_GATE(IMX8MP_CLK_GPT3_ROOT, "gpt3_root_clk", "gpt3", 0x4120),
	ROOT_GATE(IMX8MP_CLK_GPT4_ROOT, "gpt4_root_clk", "gpt4", 0x4130),
	ROOT_GATE(IMX8MP_CLK_GPT5_ROOT, "gpt5_root_clk", "gpt5", 0x4140),
	ROOT_GATE(IMX8MP_CLK_GPT6_ROOT, "gpt6_root_clk", "gpt6", 0x4150),

	ROOT_GATE(IMX8MP_CLK_I2C1_ROOT ,"i2c1_root_clk", "i2c1", 0x4170),
	ROOT_GATE(IMX8MP_CLK_I2C2_ROOT ,"i2c2_root_clk", "i2c2", 0x4180),
	ROOT_GATE(IMX8MP_CLK_I2C3_ROOT ,"i2c3_root_clk", "i2c3", 0x4190),
	ROOT_GATE(IMX8MP_CLK_I2C4_ROOT ,"i2c4_root_clk", "i2c4", 0x41a0),
	ROOT_GATE(IMX8MP_CLK_I2C5_ROOT ,"i2c5_root_clk", "i2c5", 0x4330),
	ROOT_GATE(IMX8MP_CLK_I2C6_ROOT ,"i2c6_root_clk", "i2c6", 0x4340),

	ROOT_GATE(IMX8MP_CLK_MU_ROOT, "mu_root_clk", "ipg_root", 0x4210),
	ROOT_GATE(IMX8MP_CLK_OCOTP_ROOT, "ocotp_root_clk", "ipg_root", 0x4220),
	ROOT_GATE(IMX8MP_CLK_PCIE_ROOT, "pcie_root_clk", "pcie_aux", 0x4250),

	ROOT_GATE(IMX8MP_CLK_PWM1_ROOT, "pwm1_root_clk", "pwm1", 0x4280),
	ROOT_GATE(IMX8MP_CLK_PWM2_ROOT, "pwm2_root_clk", "pwm2", 0x4290),
	ROOT_GATE(IMX8MP_CLK_PWM3_ROOT, "pwm3_root_clk", "pwm3", 0x42a0),
	ROOT_GATE(IMX8MP_CLK_PWM4_ROOT, "pwm4_root_clk", "pwm4", 0x42b0),
	ROOT_GATE(IMX8MP_CLK_QOS_ROOT, "qos_root_clk", "ipg_root", 0x42c0),
	ROOT_GATE(IMX8MP_CLK_QOS_ENET_ROOT, "qos_enet_root_clk", "ipg_root", 0x42e0),
	ROOT_GATE(IMX8MP_CLK_QSPI_ROOT, "qspi_root_clk", "qspi", 0x42f0),

	ROOT_GATE(IMX8MP_CLK_NAND_ROOT, "nand_root_clk", "nand", 0x4300),
	ROOT_GATE(IMX8MP_CLK_NAND_USDHC_BUS_RAWNAND_CLK, "nand_usdhc_rawnand_clk", "nand_usdhc_bus", 0x4300),

	ROOT_GATE(IMX8MP_CLK_CAN1_ROOT, "can1_root_clk", "can1", 0x4350),
	ROOT_GATE(IMX8MP_CLK_CAN2_ROOT, "can2_root_clk", "can2", 0x4360),

	ROOT_GATE(IMX8MP_CLK_SDMA1_ROOT, "sdma1_root_clk", "ipg_root", 0x43a0),
	ROOT_GATE(IMX8MP_CLK_SIM_ENET_ROOT, "sim_enet_root_clk", "enet_axi", 0x4400),
	ROOT_GATE(IMX8MP_CLK_ENET_QOS_ROOT, "enet_qos_root_clk", "sim_enet_root_clk", 0x43b0),
	ROOT_GATE(IMX8MP_CLK_GPU2D_ROOT, "gpu2d_root_clk", "gpu2d_core", 0x4450),
	ROOT_GATE(IMX8MP_CLK_GPU3D_ROOT, "gpu3d_root_clk", "gpu3d_core", 0x4460),

	ROOT_GATE(IMX8MP_CLK_UART1_ROOT ,"uart1_root_clk", "uart1", 0x4490),
	ROOT_GATE(IMX8MP_CLK_UART2_ROOT ,"uart2_root_clk", "uart2", 0x44a0),
	ROOT_GATE(IMX8MP_CLK_UART3_ROOT ,"uart3_root_clk", "uart3", 0x44b0),
	ROOT_GATE(IMX8MP_CLK_UART4_ROOT ,"uart4_root_clk", "uart4", 0x44c0),

	ROOT_GATE(IMX8MP_CLK_USB_ROOT ,"usb_root_clk", "hsio_axi", 0x44d0),
	ROOT_GATE(IMX8MP_CLK_USB_SUSP ,"usb_suspend_clk", "osc_32k", 0x44d0),
	ROOT_GATE(IMX8MP_CLK_USB_PHY_ROOT ,"usb_phy_root_clk", "usb_phy_ref", 0x44f0),
	ROOT_GATE(IMX8MP_CLK_USDHC1_ROOT ,"usdhc1_root_clk", "usdhc1", 0x4510),
	ROOT_GATE(IMX8MP_CLK_USDHC2_ROOT ,"usdhc2_root_clk", "usdhc2", 0x4520),
	ROOT_GATE(IMX8MP_CLK_USDHC3_ROOT ,"usdhc3_root_clk", "usdhc3", 0x45e0),

	ROOT_GATE(IMX8MP_CLK_HSIO_ROOT, "hsio_root_clk", "ipg_root", 0x45c0),

	ROOT_GATE(IMX8MP_CLK_WDOG1_ROOT, "wdog1_root_clk", "wdog", 0x4530),
	ROOT_GATE(IMX8MP_CLK_WDOG2_ROOT, "wdog2_root_clk", "wdog", 0x4540),
	ROOT_GATE(IMX8MP_CLK_WDOG3_ROOT, "wdog3_root_clk", "wdog", 0x4550),
	ROOT_GATE(IMX8MP_CLK_VPU_G1_ROOT, "vpu_g1_root_clk", "vpu_g1", 0x4560),
	ROOT_GATE(IMX8MP_CLK_GPU_ROOT, "gpu_root_clk", "gpu_axi", 0x4570),
	ROOT_GATE(IMX8MP_CLK_VPU_VC8KE_ROOT, "vpu_vc8ke_root_clk", "vpu_vc8000e", 0x4590),
	ROOT_GATE(IMX8MP_CLK_VPU_G2_ROOT, "vpu_g2_root_clk", "vpu_g2", 0x45a0),
	ROOT_GATE(IMX8MP_CLK_NPU_ROOT, "npu_root_clk", "ml_core", 0x45b0),

	ROOT_GATE(IMX8MP_CLK_MEDIA_APB_ROOT, "media_apb_root_clk", "media_apb", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_AXI_ROOT, "media_axi_root_clk", "media_axi", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_CAM1_PIX_ROOT, "media_cam1_pix_root_clk", "media_cam1_pix", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_CAM2_PIX_ROOT, "media_cam2_pix_root_clk", "media_cam2_pix", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_DISP1_PIX_ROOT, "media_disp1_pix_root_clk", "media_disp1_pix", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_DISP2_PIX_ROOT, "media_disp2_pix_root_clk", "media_disp2_pix", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_MIPI_PHY1_REF_ROOT, "media_mipi_phy1_ref_root", "media_mipi_phy1_ref", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_LDB_ROOT, "media_ldb_root_clk", "media_ldb", 0x45d0),
	ROOT_GATE(IMX8MP_CLK_MEDIA_ISP_ROOT, "media_isp_root_clk", "media_isp", 0x45d0),

	ROOT_GATE(IMX8MP_CLK_HDMI_ROOT, "hdmi_root_clk", "hdmi_axi", 0x45f0),
	ROOT_GATE(IMX8MP_CLK_TSENSOR_ROOT, "tsensor_root_clk", "ipg_root", 0x4620),
	ROOT_GATE(IMX8MP_CLK_VPU_ROOT, "vpu_root_clk", "vpu_bus", 0x4630),

	ROOT_GATE(IMX8MP_CLK_AUDIO_AHB_ROOT, "audio_ahb_root", "audio_ahb", 0x4650),
	ROOT_GATE(IMX8MP_CLK_AUDIO_AXI_ROOT, "audio_axi_root", "audio_axi", 0x4650),
	ROOT_GATE(IMX8MP_CLK_SAI1_ROOT, "sai1_root", "sai1", 0x4650),
	ROOT_GATE(IMX8MP_CLK_SAI2_ROOT, "sai2_root", "sai2", 0x4650),
	ROOT_GATE(IMX8MP_CLK_SAI3_ROOT, "sai3_root", "sai3", 0x4650),
	ROOT_GATE(IMX8MP_CLK_SAI5_ROOT, "sai5_root", "sai5", 0x4650),
	ROOT_GATE(IMX8MP_CLK_SAI6_ROOT, "sai6_root", "sai6", 0x4650),
	ROOT_GATE(IMX8MP_CLK_SAI7_ROOT, "sai7_root", "sai7", 0x4650),
	ROOT_GATE(IMX8MP_CLK_PDM_ROOT, "pdm_root", "pdm", 0x4650),
};

static int
imx8mp_ccm_attach(device_t dev)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->clks = imx8mp_clks;
	sc->nclks = nitems(imx8mp_clks);

	return (imx_ccm_attach(dev));
}

static int
imx8mp_ccm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "fsl,imx8mp-ccm") == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX 8M Plus Clock Control Module");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t imx8mp_ccm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  imx8mp_ccm_probe),
	DEVMETHOD(device_attach, imx8mp_ccm_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(imx8mp_ccm, imx8mp_ccm_driver, imx8mp_ccm_methods,
    sizeof(struct imx_ccm_softc), imx_ccm_driver);

EARLY_DRIVER_MODULE(imx8mp_ccm, simplebus, imx8mp_ccm_driver, 0, 0,
    BUS_PASS_CPU + BUS_PASS_ORDER_EARLY);
