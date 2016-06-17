/*
 * intelfb
 *
 * Linux framebuffer driver for Intel(R) 865G integrated graphics chips.
 *
 * Copyright (C) 2002, 2003 David Dawes <dawes@tungstengraphics.com>
 *
 * This driver consists of two parts.  The first part (intelfbdrv.c) provides
 * the basic fbdev interfaces, is derived in part from the radeonfb and
 * vesafb drivers, and is covered by the GPL.  The second part (intelfbhw.c)
 * provides the code to program the hardware.  Most of it is derived from
 * the i810/i830 XFree86 driver.  The HW-specific code is covered here
 * under a dual license (GPL and MIT/XFree86 license).
 *
 * Author: David Dawes
 *
 */

/* $DHD: intelfb/intelfbhw.c,v 1.7 2003/02/06 00:53:11 dawes Exp $ */
/* $TG$ */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>
#include <linux/pagemap.h>
#include <linux/version.h>

#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb32.h>

#include "intelfb.h"
#include "intelfbhw.h"

int
intelfbhw_get_chipset(struct pci_dev *pdev, const char **name, int *chipset,
		      int *mobile)
{
	u32 tmp;

	if (!pdev || !name || !chipset || !mobile)
		return 1;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_830M:
		*name = "Intel(R) 830M";
		*chipset = INTEL_830M;
		*mobile = 1;
		return 0;
	case PCI_DEVICE_ID_INTEL_845G:
		*name = "Intel(R) 845G";
		*chipset = INTEL_845G;
		*mobile = 0;
		return 0;
	case PCI_DEVICE_ID_INTEL_85XGM:
		tmp = 0;
		*mobile = 1;
		pci_read_config_dword(pdev, INTEL_85X_CAPID, &tmp);
		switch ((tmp >> INTEL_85X_VARIANT_SHIFT) &
			INTEL_85X_VARIANT_MASK) {
		case INTEL_VAR_855GME:
			*name = "Intel(R) 855GME";
			*chipset = INTEL_855GME;
			return 0;
		case INTEL_VAR_855GM:
			*name = "Intel(R) 855GM";
			*chipset = INTEL_855GM;
			return 0;
		case INTEL_VAR_852GME:
			*name = "Intel(R) 852GME";
			*chipset = INTEL_852GME;
			return 0;
		case INTEL_VAR_852GM:
			*name = "Intel(R) 852GM";
			*chipset = INTEL_852GM;
			return 0;
		default:
			*name = "Intel(R) 852GM/855GM";
			*chipset = INTEL_85XGM;
			return 0;
		}
		break;
	case PCI_DEVICE_ID_INTEL_865G:
		*name = "Intel(R) 865G";
		*chipset = INTEL_865G;
		*mobile = 0;
		return 0;
	default:
		return 1;
	}
}

int
intelfbhw_get_memory(struct pci_dev *pdev, int *aperture_size,
		     int *stolen_size)
{
	struct pci_dev *bridge_dev;
	u16 tmp;

	if (!pdev || !aperture_size || !stolen_size)
		return 1;

	/* Find the bridge device.  It is always 0:0.0 */
	if (!(bridge_dev = pci_find_slot(0, PCI_DEVFN(0, 0)))) {
		ERR_MSG("cannot find bridge device\n");
		return 1;
	}

	/* Get the fb aperture size and "stolen" memory amount. */
	tmp = 0;
	pci_read_config_word(bridge_dev, INTEL_GMCH_CTRL, &tmp);
	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_830M:
	case PCI_DEVICE_ID_INTEL_845G:
		if ((tmp & INTEL_GMCH_MEM_MASK) == INTEL_GMCH_MEM_64M)
			*aperture_size = MB(64);
		else
			*aperture_size = MB(128);
		switch (tmp & INTEL_830_GMCH_GMS_MASK) {
		case INTEL_830_GMCH_GMS_STOLEN_512:
			*stolen_size = KB(512) - KB(132);
			return 0;
		case INTEL_830_GMCH_GMS_STOLEN_1024:
			*stolen_size = MB(1) - KB(132);
			return 0;
		case INTEL_830_GMCH_GMS_STOLEN_8192:
			*stolen_size = MB(8) - KB(132);
			return 0;
		case INTEL_830_GMCH_GMS_LOCAL:
			ERR_MSG("only local memory found\n");
			return 1;
		case INTEL_830_GMCH_GMS_DISABLED:
			ERR_MSG("video memory is disabled\n");
			return 1;
		default:
			ERR_MSG("unexpected GMCH_GMS value: 0x%02x\n",
				tmp & INTEL_830_GMCH_GMS_MASK);
			return 1;
		}
		break;
	default:
		*aperture_size = MB(128);
		switch (tmp & INTEL_855_GMCH_GMS_MASK) {
		case INTEL_855_GMCH_GMS_STOLEN_1M:
			*stolen_size = MB(1) - KB(132);
			return 0;
		case INTEL_855_GMCH_GMS_STOLEN_4M:
			*stolen_size = MB(4) - KB(132);
			return 0;
		case INTEL_855_GMCH_GMS_STOLEN_8M:
			*stolen_size = MB(8) - KB(132);
			return 0;
		case INTEL_855_GMCH_GMS_STOLEN_16M:
			*stolen_size = MB(16) - KB(132);
			return 0;
		case INTEL_855_GMCH_GMS_STOLEN_32M:
			*stolen_size = MB(32) - KB(132);
			return 0;
		case INTEL_855_GMCH_GMS_DISABLED:
			ERR_MSG("video memory is disabled\n");
			return 0;
		default:
			ERR_MSG("unexpected GMCH_GMS value: 0x%02x\n",
				tmp & INTEL_855_GMCH_GMS_MASK);
			return 1;
		}
	}
}

const char *
intelfbhw_check_non_crt(struct intelfb_info *dinfo)
{
	if (INREG(LVDS) & PORT_ENABLE)
		return "LVDS port";
	else if (INREG(DVOA) & PORT_ENABLE)
		return "DVO port A";
	else if (INREG(DVOB) & PORT_ENABLE)
		return "DVO port B";
	else if (INREG(DVOC) & PORT_ENABLE)
		return "DVO port C";
	else
		return NULL;
}

int
intelfbhw_validate_mode(struct intelfb_info *dinfo, int con,
			struct fb_var_screeninfo *var)
{
	int bytes_per_pixel;
	int tmp;

	DBG_MSG("intelfbhw_validate_mode\n");

	bytes_per_pixel = var->bits_per_pixel / 8;
	if (bytes_per_pixel == 3)
		bytes_per_pixel = 4;

	/* Check if enough video memory. */
	tmp = var->yres_virtual * var->xres_virtual * bytes_per_pixel;
	if (tmp > dinfo->video_ram) {
		if (con >= 0)
			WRN_MSG("Not enough video ram for mode "
				"(%d KByte vs %d KByte).\n",
				BtoKB(tmp), BtoKB(dinfo->video_ram));
		return 1;
	}

	/* Check if x/y limits are OK. */
	if (var->xres - 1 > HACTIVE_MASK) {
		if (con >= 0)
			WRN_MSG("X resolution too large (%d vs %d).\n",
				var->xres, HACTIVE_MASK + 1);
		return 1;
	}
	if (var->yres - 1 > VACTIVE_MASK) {
		if (con >= 0)
			WRN_MSG("Y resolution too large (%d vs %d).\n",
				var->yres, VACTIVE_MASK + 1);
		return 1;
	}

	/* Check for interlaced/doublescan modes. */
	if (var->vmode & FB_VMODE_INTERLACED) {
		if (con >= 0)
			WRN_MSG("Mode is interlaced.\n");
		return 1;
	}
	if (var->vmode & FB_VMODE_DOUBLE) {
		if (con >= 0)
			WRN_MSG("Mode is double-scan.\n");
		return 1;
	}

	/* Check if clock is OK. */
	tmp = 1000000000 / var->pixclock;
	if (tmp < MIN_CLOCK) {
		if (con >= 0)
			WRN_MSG("Pixel clock is too low (%d MHz vs %d MHz).\n",
				(tmp + 500) / 1000, MIN_CLOCK / 1000);
		return 1;
	}
	if (tmp > MAX_CLOCK) {
		if (con >= 0)
			WRN_MSG("Pixel clock is too high (%d MHz vs %d MHz).\n",
				(tmp + 500) / 1000, MAX_CLOCK / 1000);
		return 1;
	}

	return 0;
}

int
intelfbhw_pan_display(struct fb_var_screeninfo *var, int con,
		      struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	u32 offset, xoffset, yoffset;

	DBG_MSG("intelfbhw_pan_display\n");

	if (con != dinfo->currcon)
		return 0;

	xoffset = ROUND_DOWN_TO(var->xoffset, 8);
	yoffset = var->yoffset;

	if ((xoffset + var->xres > var->xres_virtual) ||
	    (yoffset + var->yres > var->yres_virtual))
		return EINVAL;

	offset = (yoffset * dinfo->pitch) +
		 (xoffset * var->bits_per_pixel) / 8;

	OUTREG(DSPABASE, offset);

	return 0;
}

/* Blank the screen. */
void
intelfbhw_do_blank(int blank, struct fb_info *info)
{
	struct intelfb_info *dinfo = GET_DINFO(info);
	u32 tmp;

	DBG_MSG("intelfbhw_do_blank: blank is %d\n", blank);

	/* Turn plane A on or off */
	tmp = INREG(DSPACNTR);
	if (blank)
		tmp &= ~DISPPLANE_PLANE_ENABLE;
	else
		tmp |= DISPPLANE_PLANE_ENABLE;
	OUTREG(DSPACNTR, tmp);
	/* Flush */
	tmp = INREG(DSPABASE);
	OUTREG(DSPABASE, tmp);

	/* Turn off/on the HW cursor */
#if VERBOSE > 0
	DBG_MSG("cursor.enabled is %d\n", dinfo->cursor.enabled);
#endif
	if (dinfo->cursor.enabled) {
		if (blank) {
			intelfbhw_cursor_hide(dinfo);
		} else {
			intelfbhw_cursor_show(dinfo);
		}
		dinfo->cursor.enabled = 1;
	}
	dinfo->cursor.blanked = blank;

	/* Set DPMS level */
	tmp = INREG(ADPA) & ~ADPA_DPMS_CONTROL_MASK;
	switch (blank) {
	case 0:
	case 1:
		tmp |= ADPA_DPMS_D0;
		break;
	case 2:
		tmp |= ADPA_DPMS_D1;
		break;
	case 3:
		tmp |= ADPA_DPMS_D2;
		break;
	case 4:
		tmp |= ADPA_DPMS_D3;
		break;
	}
	OUTREG(ADPA, tmp);
	
	return;
}


void
intelfbhw_setcolreg(struct intelfb_info *dinfo, unsigned regno,
		    unsigned red, unsigned green, unsigned blue,
		    unsigned transp)
{
#if VERBOSE > 1
	DBG_MSG("intelfbhw_setcolreg: %d: (%d, %d, %d)\n",
		regno, red, green, blue);
#endif

	u32 palette_reg = (dinfo->pipe == PIPE_A) ?
			  PALETTE_A : PALETTE_B;

	OUTREG(palette_reg + (regno << 2), 
	       (red << PALETTE_8_RED_SHIFT) |
	       (green << PALETTE_8_GREEN_SHIFT) |
	       (blue << PALETTE_8_BLUE_SHIFT));
}


int
intelfbhw_read_hw_state(struct intelfb_info *dinfo, struct intelfb_hwstate *hw,
			int flag)
{
	int i;

	DBG_MSG("intelfbhw_read_hw_state\n");

	if (!hw || !dinfo)
		return -1;

	/* Read in as much of the HW state as possible. */
	hw->vga0_divisor = INREG(VGA0_DIVISOR);
	hw->vga1_divisor = INREG(VGA1_DIVISOR);
	hw->vga_pd = INREG(VGAPD);
	hw->dpll_a = INREG(DPLL_A);
	hw->dpll_b = INREG(DPLL_B);
	hw->fpa0 = INREG(FPA0);
	hw->fpa1 = INREG(FPA1);
	hw->fpb0 = INREG(FPB0);
	hw->fpb1 = INREG(FPB1);

	if (flag == 1)
		return flag;

#if 0
	/* This seems to be a problem with the 852GM/855GM */
	for (i = 0; i < PALETTE_8_ENTRIES; i++) {
		hw->palette_a[i] = INREG(PALETTE_A + (i << 2));
		hw->palette_b[i] = INREG(PALETTE_B + (i << 2));
	}
#endif

	if (flag == 2)
		return flag;

	hw->htotal_a = INREG(HTOTAL_A);
	hw->hblank_a = INREG(HBLANK_A);
	hw->hsync_a = INREG(HSYNC_A);
	hw->vtotal_a = INREG(VTOTAL_A);
	hw->vblank_a = INREG(VBLANK_A);
	hw->vsync_a = INREG(VSYNC_A);
	hw->src_size_a = INREG(SRC_SIZE_A);
	hw->bclrpat_a = INREG(BCLRPAT_A);
	hw->htotal_b = INREG(HTOTAL_B);
	hw->hblank_b = INREG(HBLANK_B);
	hw->hsync_b = INREG(HSYNC_B);
	hw->vtotal_b = INREG(VTOTAL_B);
	hw->vblank_b = INREG(VBLANK_B);
	hw->vsync_b = INREG(VSYNC_B);
	hw->src_size_b = INREG(SRC_SIZE_B);
	hw->bclrpat_b = INREG(BCLRPAT_B);

	if (flag == 3)
		return flag;

	hw->adpa = INREG(ADPA);
	hw->dvoa = INREG(DVOA);
	hw->dvob = INREG(DVOB);
	hw->dvoc = INREG(DVOC);
	hw->dvoa_srcdim = INREG(DVOA_SRCDIM);
	hw->dvob_srcdim = INREG(DVOB_SRCDIM);
	hw->dvoc_srcdim = INREG(DVOC_SRCDIM);
	hw->lvds = INREG(LVDS);

	if (flag == 4)
		return flag;

	hw->pipe_a_conf = INREG(PIPEACONF);
	hw->pipe_b_conf = INREG(PIPEBCONF);
	hw->disp_arb = INREG(DISPARB);

	if (flag == 5)
		return flag;

	hw->cursor_a_control = INREG(CURSOR_A_CONTROL);
	hw->cursor_b_control = INREG(CURSOR_B_CONTROL);
	hw->cursor_a_base = INREG(CURSOR_A_BASEADDR);
	hw->cursor_b_base = INREG(CURSOR_B_BASEADDR);

	if (flag == 6)
		return flag;

	for (i = 0; i < 4; i++) {
		hw->cursor_a_palette[i] = INREG(CURSOR_A_PALETTE0 + (i << 2));
		hw->cursor_b_palette[i] = INREG(CURSOR_B_PALETTE0 + (i << 2));
	}

	if (flag == 7)
		return flag;

	hw->cursor_size = INREG(CURSOR_SIZE);

	if (flag == 8)
		return flag;

	hw->disp_a_ctrl = INREG(DSPACNTR);
	hw->disp_b_ctrl = INREG(DSPBCNTR);
	hw->disp_a_base = INREG(DSPABASE);
	hw->disp_b_base = INREG(DSPBBASE);
	hw->disp_a_stride = INREG(DSPASTRIDE);
	hw->disp_b_stride = INREG(DSPBSTRIDE);

	if (flag == 9)
		return flag;

	hw->vgacntrl = INREG(VGACNTRL);

	if (flag == 10)
		return flag;

	hw->add_id = INREG(ADD_ID);

	if (flag == 11)
		return flag;

	for (i = 0; i < 7; i++) {
		hw->swf0x[i] = INREG(SWF00 + (i << 2));
		hw->swf1x[i] = INREG(SWF10 + (i << 2));
		if (i < 3)
			hw->swf3x[i] = INREG(SWF30 + (i << 2));
	}

	for (i = 0; i < 8; i++)
		hw->fence[i] = INREG(FENCE + (i << 2));

	hw->instpm = INREG(INSTPM);
	hw->mem_mode = INREG(MEM_MODE);
	hw->fw_blc_0 = INREG(FW_BLC_0);
	hw->fw_blc_1 = INREG(FW_BLC_1);

	return 0;
}


void
intelfbhw_print_hw_state(struct intelfb_info *dinfo, struct intelfb_hwstate *hw)
{
#if REGDUMP
	int i, m1, m2, n, p1, p2;

	DBG_MSG("intelfbhw_print_hw_state\n");

	if (!hw || !dinfo)
		return;
	/* Read in as much of the HW state as possible. */
	printk("hw state dump start\n");
	printk("	VGA0_DIVISOR:		0x%08x\n", hw->vga0_divisor);
	printk("	VGA1_DIVISOR:		0x%08x\n", hw->vga1_divisor);
	printk("	VGAPD: 			0x%08x\n", hw->vga_pd);
	n = (hw->vga0_divisor >> FP_N_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m1 = (hw->vga0_divisor >> FP_M1_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m2 = (hw->vga0_divisor >> FP_M2_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	if (hw->vga_pd & VGAPD_0_P1_FORCE_DIV2)
		p1 = 0;
	else
		p1 = (hw->vga_pd >> VGAPD_0_P1_SHIFT) & DPLL_P1_MASK;
	p2 = (hw->vga_pd >> VGAPD_0_P2_SHIFT) & DPLL_P2_MASK;
	printk("	VGA0: (m1, m2, n, p1, p2) = (%d, %d, %d, %d, %d)\n",
		m1, m2, n, p1, p2);
	printk("	VGA0: clock is %d\n", CALC_VCLOCK(m1, m2, n, p1, p2));
	
	n = (hw->vga1_divisor >> FP_N_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m1 = (hw->vga1_divisor >> FP_M1_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m2 = (hw->vga1_divisor >> FP_M2_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	if (hw->vga_pd & VGAPD_1_P1_FORCE_DIV2)
		p1 = 0;
	else
		p1 = (hw->vga_pd >> VGAPD_1_P1_SHIFT) & DPLL_P1_MASK;
	p2 = (hw->vga_pd >> VGAPD_1_P2_SHIFT) & DPLL_P2_MASK;
	printk("	VGA1: (m1, m2, n, p1, p2) = (%d, %d, %d, %d, %d)\n",
		m1, m2, n, p1, p2);
	printk("	VGA1: clock is %d\n", CALC_VCLOCK(m1, m2, n, p1, p2));
	
	printk("	DPLL_A:			0x%08x\n", hw->dpll_a);
	printk("	DPLL_B:			0x%08x\n", hw->dpll_b);
	printk("	FPA0:			0x%08x\n", hw->fpa0);
	printk("	FPA1:			0x%08x\n", hw->fpa1);
	printk("	FPB0:			0x%08x\n", hw->fpb0);
	printk("	FPB1:			0x%08x\n", hw->fpb1);

	n = (hw->fpa0 >> FP_N_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m1 = (hw->fpa0 >> FP_M1_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m2 = (hw->fpa0 >> FP_M2_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	if (hw->dpll_a & DPLL_P1_FORCE_DIV2)
		p1 = 0;
	else
		p1 = (hw->dpll_a >> DPLL_P1_SHIFT) & DPLL_P1_MASK;
	p2 = (hw->dpll_a >> DPLL_P2_SHIFT) & DPLL_P2_MASK;
	printk("	PLLA0: (m1, m2, n, p1, p2) = (%d, %d, %d, %d, %d)\n",
		m1, m2, n, p1, p2);
	printk("	PLLA0: clock is %d\n", CALC_VCLOCK(m1, m2, n, p1, p2));

	n = (hw->fpa1 >> FP_N_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m1 = (hw->fpa1 >> FP_M1_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	m2 = (hw->fpa1 >> FP_M2_DIVISOR_SHIFT) & FP_DIVISOR_MASK;
	if (hw->dpll_a & DPLL_P1_FORCE_DIV2)
		p1 = 0;
	else
		p1 = (hw->dpll_a >> DPLL_P1_SHIFT) & DPLL_P1_MASK;
	p2 = (hw->dpll_a >> DPLL_P2_SHIFT) & DPLL_P2_MASK;
	printk("	PLLA1: (m1, m2, n, p1, p2) = (%d, %d, %d, %d, %d)\n",
		m1, m2, n, p1, p2);
	printk("	PLLA1: clock is %d\n", CALC_VCLOCK(m1, m2, n, p1, p2));
	
#if 0
	printk("	PALETTE_A:\n");
	for (i = 0; i < PALETTE_8_ENTRIES)
		printk("	%3d:	0x%08x\n", i, hw->palette_a[i];
	printk("	PALETTE_B:\n");
	for (i = 0; i < PALETTE_8_ENTRIES)
		printk("	%3d:	0x%08x\n", i, hw->palette_b[i];
#endif

	printk("	HTOTAL_A:		0x%08x\n", hw->htotal_a);
	printk("	HBLANK_A:		0x%08x\n", hw->hblank_a);
	printk("	HSYNC_A:		0x%08x\n", hw->hsync_a);
	printk("	VTOTAL_A:		0x%08x\n", hw->vtotal_a);
	printk("	VBLANK_A:		0x%08x\n", hw->vblank_a);
	printk("	VSYNC_A:		0x%08x\n", hw->vsync_a);
	printk("	SRC_SIZE_A:		0x%08x\n", hw->src_size_a);
	printk("	BCLRPAT_A:		0x%08x\n", hw->bclrpat_a);
	printk("	HTOTAL_B:		0x%08x\n", hw->htotal_b);
	printk("	HBLANK_B:		0x%08x\n", hw->hblank_b);
	printk("	HSYNC_B:		0x%08x\n", hw->hsync_b);
	printk("	VTOTAL_B:		0x%08x\n", hw->vtotal_b);
	printk("	VBLANK_B:		0x%08x\n", hw->vblank_b);
	printk("	VSYNC_B:		0x%08x\n", hw->vsync_b);
	printk("	SRC_SIZE_B:		0x%08x\n", hw->src_size_b);
	printk("	BCLRPAT_B:		0x%08x\n", hw->bclrpat_b);

	printk("	ADPA:			0x%08x\n", hw->adpa);
	printk("	DVOA:			0x%08x\n", hw->dvoa);
	printk("	DVOB:			0x%08x\n", hw->dvob);
	printk("	DVOC:			0x%08x\n", hw->dvoc);
	printk("	DVOA_SRCDIM:		0x%08x\n", hw->dvoa_srcdim);
	printk("	DVOB_SRCDIM:		0x%08x\n", hw->dvob_srcdim);
	printk("	DVOC_SRCDIM:		0x%08x\n", hw->dvoc_srcdim);
	printk("	LVDS:			0x%08x\n", hw->lvds);

	printk("	PIPEACONF:		0x%08x\n", hw->pipe_a_conf);
	printk("	PIPEBCONF:		0x%08x\n", hw->pipe_b_conf);
	printk("	DISPARB:		0x%08x\n", hw->disp_arb);

	printk("	CURSOR_A_CONTROL:	0x%08x\n", hw->cursor_a_control);
	printk("	CURSOR_B_CONTROL:	0x%08x\n", hw->cursor_b_control);
	printk("	CURSOR_A_BASEADDR:	0x%08x\n", hw->cursor_a_base);
	printk("	CURSOR_B_BASEADDR:	0x%08x\n", hw->cursor_b_base);

	printk("	CURSOR_A_PALETTE:	");
	for (i = 0; i < 4; i++) {
		printk("0x%08x", hw->cursor_a_palette[i]);
		if (i < 3)
			printk(", ");
	}
	printk("\n");
	printk("	CURSOR_B_PALETTE:	");
	for (i = 0; i < 4; i++) {
		printk("0x%08x", hw->cursor_b_palette[i]);
		if (i < 3)
			printk(", ");
	}
	printk("\n");

	printk("	CURSOR_SIZE:		0x%08x\n", hw->cursor_size);

	printk("	DSPACNTR:		0x%08x\n", hw->disp_a_ctrl);
	printk("	DSPBCNTR:		0x%08x\n", hw->disp_b_ctrl);
	printk("	DSPABASE:		0x%08x\n", hw->disp_a_base);
	printk("	DSPBBASE:		0x%08x\n", hw->disp_b_base);
	printk("	DSPASTRIDE:		0x%08x\n", hw->disp_a_stride);
	printk("	DSPBSTRIDE:		0x%08x\n", hw->disp_b_stride);

	printk("	VGACNTRL:		0x%08x\n", hw->vgacntrl);
	printk("	ADD_ID:			0x%08x\n", hw->add_id);

	for (i = 0; i < 7; i++) {
		printk("	SWF0%d			0x%08x\n", i,
			hw->swf0x[i]);
	}
	for (i = 0; i < 7; i++) {
		printk("	SWF1%d			0x%08x\n", i,
			hw->swf1x[i]);
	}
	for (i = 0; i < 3; i++) {
		printk("	SWF3%d			0x%08x\n", i,
			hw->swf3x[i]);
	}
	for (i = 0; i < 8; i++)
		printk("	FENCE%d			0x%08x\n", i,
			hw->fence[i]);

	printk("	INSTPM			0x%08x\n", hw->instpm);
	printk("	MEM_MODE		0x%08x\n", hw->mem_mode);
	printk("	FW_BLC_0		0x%08x\n", hw->fw_blc_0);
	printk("	FW_BLC_1		0x%08x\n", hw->fw_blc_1);

	printk("hw state dump end\n");
#endif
}

/* Split the M parameter into M1 and M2. */
static int
splitm(unsigned int m, unsigned int *retm1, unsigned int *retm2)
{
	int m1, m2;

	m1 = (m - 2 - (MIN_M2 + MAX_M2) / 2) / 5 - 2;
	if (m1 < MIN_M1)
		m1 = MIN_M1;
	if (m1 > MAX_M1)
		m1 = MAX_M1;
	m2 = m - 5 * (m1 + 2) - 2;
	if (m2 < MIN_M2 || m2 > MAX_M2 || m2 >= m1) {
		return 1;
	} else {
		*retm1 = (unsigned int)m1;
		*retm2 = (unsigned int)m2;
		return 0;
	}
}

/* Split the P parameter into P1 and P2. */
static int
splitp(unsigned int p, unsigned int *retp1, unsigned int *retp2)
{
	int p1, p2;

	if (p % 4 == 0)
		p2 = 1;
	else
		p2 = 0;
	p1 = (p / (1 << (p2 + 1))) - 2;
	if (p % 4 == 0 && p1 < MIN_P1) {
		p2 = 0;
		p1 = (p / (1 << (p2 + 1))) - 2;
	}
	if (p1  < MIN_P1 || p1 > MAX_P1 || (p1 + 2) * (1 << (p2 + 1)) != p) {
		return 1;
	} else {
		*retp1 = (unsigned int)p1;
		*retp2 = (unsigned int)p2;
		return 0;
	}
}

static int
calc_pll_params(int clock, u32 *retm1, u32 *retm2, u32 *retn, u32 *retp1,
		u32 *retp2, u32 *retclock)
{
	u32 m1, m2, n, p1, p2, n1;
	u32 f_vco, p, p_best = 0, m, f_out;
	u32 err_max, err_target, err_best = 10000000;
	u32 n_best = 0, m_best = 0, f_best, f_err;
	u32 p_min, p_max, p_inc, div_min, div_max;

	/* Accept 0.5% difference, but aim for 0.1% */
	err_max = 5 * clock / 1000;
	err_target = clock / 1000;

	DBG_MSG("Clock is %d\n", clock);

	div_max = MAX_VCO_FREQ / clock;
	div_min = ROUND_UP_TO(MIN_VCO_FREQ, clock) / clock;

	if (clock <= P_TRANSITION_CLOCK)
		p_inc = 4;
	else
		p_inc = 2;
	p_min = ROUND_UP_TO(div_min, p_inc);
	p_max = ROUND_DOWN_TO(div_max, p_inc);
	if (p_min < MIN_P)
		p_min = 4;
	if (p_max > MAX_P)
		p_max = 128;

	DBG_MSG("p range is %d-%d (%d)\n", p_min, p_max, p_inc);

	p = p_min;
	do {
		if (splitp(p, &p1, &p2)) {
			WRN_MSG("cannot split p = %d\n", p);
			p += p_inc;
			continue;
		}
		n = MIN_N;
		f_vco = clock * p;

		do {
			m = ROUND_UP_TO(f_vco * n, PLL_REFCLK) / PLL_REFCLK;
			if (m < MIN_M)
				m = MIN_M;
			if (m > MAX_M)
				m = MAX_M;
			f_out = CALC_VCLOCK3(m, n, p);
			if (splitm(m, &m1, &m2)) {
				WRN_MSG("cannot split m = %d\n", m);
				n++;
				continue;
			}
			if (clock > f_out)
				f_err = clock - f_out;
			else
				f_err = f_out - clock;
			
			if (f_err < err_best) {
				m_best = m;
				n_best = n;
				p_best = p;
				f_best = f_out;
				err_best = f_err;
			}
			n++;
		} while ((n <= MAX_N) && (f_out >= clock));
		p += p_inc;
	} while ((p <= p_max));

	if (!m_best) {
		WRN_MSG("cannot find parameters for clock %d\n", clock);
		return 1;
	}
	m = m_best;
	n = n_best;
	p = p_best;
	splitm(m, &m1, &m2);
	splitp(p, &p1, &p2);
	n1 = n - 2;
	
	DBG_MSG("m, n, p: %d (%d,%d), %d (%d), %d (%d,%d), "
		"f: %d (%d), VCO: %d\n",
		m, m1, m2, n, n1, p, p1, p2,
		CALC_VCLOCK3(m, n, p), CALC_VCLOCK(m1, m2, n1, p1, p2),
		CALC_VCLOCK3(m, n, p) * p);
	*retm1 = m1;
	*retm2 = m2;
	*retn = n1;
	*retp1 = p1;
	*retp2 = p2;
	*retclock = CALC_VCLOCK(m1, m2, n1, p1, p2);
	
	return 0;
}

static __inline__ int
check_overflow(u32 value, u32 limit, const char *description)
{
	if (value > limit) {
		WRN_MSG("%s value %d exceeds limit %d\n",
			description, value, limit);
		return 1;
	}
	return 0;
}

/* It is assumed that hw is filled in with the initial state information. */
int
intelfbhw_mode_to_hw(struct intelfb_info *dinfo, struct intelfb_hwstate *hw,
		     struct fb_var_screeninfo *var)
{
	int pipe = PIPE_A;
	u32 *dpll, *fp0, *fp1;
	u32 m1, m2, n, p1, p2, clock_target, clock;
	u32 hsync_start, hsync_end, hblank_start, hblank_end, htotal, hactive;
	u32 vsync_start, vsync_end, vblank_start, vblank_end, vtotal, vactive;
	u32 vsync_pol, hsync_pol;
	u32 *vs, *vb, *vt, *hs, *hb, *ht, *ss, *pipe_conf;
	struct display *disp;

	DBG_MSG("intelfbhw_mode_to_hw\n");

	disp = GET_DISP(&dinfo->info, dinfo->currcon);

	/* Disable VGA */
	hw->vgacntrl |= VGA_DISABLE;

	/* Check whether pipe A or pipe B is enabled. */
	if (hw->pipe_a_conf & PIPECONF_ENABLE)
		pipe = PIPE_A;
	else if (hw->pipe_b_conf & PIPECONF_ENABLE)
		pipe = PIPE_B;
	
	/* Set which pipe's registers will be set. */
	if (pipe == PIPE_B) {
		dpll = &hw->dpll_b;
		fp0 = &hw->fpb0;
		fp1 = &hw->fpb1;
		hs = &hw->hsync_b;
		hb = &hw->hblank_b;
		ht = &hw->htotal_b;
		vs = &hw->vsync_b;
		vb = &hw->vblank_b;
		vt = &hw->vtotal_b;
		ss = &hw->src_size_b;
		pipe_conf = &hw->pipe_b_conf;
	} else {
		dpll = &hw->dpll_a;
		fp0 = &hw->fpa0;
		fp1 = &hw->fpa1;
		hs = &hw->hsync_a;
		hb = &hw->hblank_a;
		ht = &hw->htotal_a;
		vs = &hw->vsync_a;
		vb = &hw->vblank_a;
		vt = &hw->vtotal_a;
		ss = &hw->src_size_a;
		pipe_conf = &hw->pipe_a_conf;
	}

	/* Use ADPA register for sync control. */
	hw->adpa &= ~ADPA_USE_VGA_HVPOLARITY;

	/* sync polarity */
	hsync_pol = (var->sync & FB_SYNC_HOR_HIGH_ACT) ?
			ADPA_SYNC_ACTIVE_HIGH : ADPA_SYNC_ACTIVE_LOW;
	vsync_pol = (var->sync & FB_SYNC_VERT_HIGH_ACT) ?
			ADPA_SYNC_ACTIVE_HIGH : ADPA_SYNC_ACTIVE_LOW;
	hw->adpa &= ~((ADPA_SYNC_ACTIVE_MASK << ADPA_VSYNC_ACTIVE_SHIFT) |
		      (ADPA_SYNC_ACTIVE_MASK << ADPA_HSYNC_ACTIVE_SHIFT));
	hw->adpa |= (hsync_pol << ADPA_HSYNC_ACTIVE_SHIFT) |
		    (vsync_pol << ADPA_VSYNC_ACTIVE_SHIFT);

	/* Connect correct pipe to the analog port DAC */
	hw->adpa &= ~(PIPE_MASK << ADPA_PIPE_SELECT_SHIFT);
	hw->adpa |= (pipe << ADPA_PIPE_SELECT_SHIFT);

	/* Set DPMS state to D0 (on) */
	hw->adpa &= ~ADPA_DPMS_CONTROL_MASK;
	hw->adpa |= ADPA_DPMS_D0;

	*dpll |= (DPLL_VCO_ENABLE | DPLL_VGA_MODE_DISABLE);
	*dpll &= ~(DPLL_RATE_SELECT_MASK | DPLL_REFERENCE_SELECT_MASK);
	*dpll |= (DPLL_REFERENCE_DEFAULT | DPLL_RATE_SELECT_FP0);

	/* Desired clock in kHz */
	clock_target = 1000000000 / var->pixclock;
	if (calc_pll_params(clock_target, &m1, &m2, &n, &p1, &p2, &clock)) {
		WRN_MSG("calc_pll_params failed\n");
		return 1;
	}

	/* Check for overflow. */
	if (check_overflow(p1, DPLL_P1_MASK, "PLL P1 parameter"))
		return 1;
	if (check_overflow(p2, DPLL_P2_MASK, "PLL P2 parameter"))
		return 1;
	if (check_overflow(m1, FP_DIVISOR_MASK, "PLL M1 parameter"))
		return 1;
	if (check_overflow(m2, FP_DIVISOR_MASK, "PLL M2 parameter"))
		return 1;
	if (check_overflow(n, FP_DIVISOR_MASK, "PLL N parameter"))
		return 1;

	*dpll &= ~DPLL_P1_FORCE_DIV2;
	*dpll &= ~((DPLL_P2_MASK << DPLL_P2_SHIFT) |
		   (DPLL_P1_MASK << DPLL_P1_SHIFT));
	*dpll |= (p2 << DPLL_P2_SHIFT) | (p1 << DPLL_P1_SHIFT);
	*fp0 = (n << FP_N_DIVISOR_SHIFT) |
	       (m1 << FP_M1_DIVISOR_SHIFT) |
	       (m2 << FP_M2_DIVISOR_SHIFT);
	*fp1 = *fp0;

	/* Make sure DVOB and DVOC are disabled for now. */
	hw->dvob &= ~PORT_ENABLE;
	hw->dvoc &= ~PORT_ENABLE;

	/* Use display plane A. */
	hw->disp_a_ctrl |= DISPPLANE_PLANE_ENABLE;
	hw->disp_a_ctrl &= ~DISPPLANE_GAMMA_ENABLE;
	hw->disp_a_ctrl &= ~DISPPLANE_PIXFORMAT_MASK;
	switch (intelfb_var_to_depth(var)) {
	case 8:
		hw->disp_a_ctrl |= DISPPLANE_8BPP | DISPPLANE_GAMMA_ENABLE;
		break;
	case 15:
		hw->disp_a_ctrl |= DISPPLANE_15_16BPP;
		break;
	case 16:
		hw->disp_a_ctrl |= DISPPLANE_16BPP;
		break;
	case 24:
		hw->disp_a_ctrl |= DISPPLANE_32BPP_NO_ALPHA;
		break;
	}
	hw->disp_a_ctrl &= ~(PIPE_MASK << DISPPLANE_SEL_PIPE_SHIFT);
	hw->disp_a_ctrl |= (pipe << DISPPLANE_SEL_PIPE_SHIFT);

	/* Set CRTC registers. */
	hactive = var->xres;
	hsync_start = hactive + var->right_margin;
	hsync_end = hsync_start + var->hsync_len;
	htotal = hsync_end + var->left_margin;
	hblank_start = hactive;
	hblank_end = htotal;

	DBG_MSG("H: act %d, ss %d, se %d, tot %d bs %d, be %d\n",
		hactive, hsync_start, hsync_end, htotal, hblank_start,
		hblank_end);

	vactive = var->yres;
	vsync_start = vactive + var->lower_margin;
	vsync_end = vsync_start + var->vsync_len;
	vtotal = vsync_end + var->upper_margin;
	vblank_start = vactive;
	vblank_end = vtotal;
	vblank_end = vsync_end + 1;

	DBG_MSG("V: act %d, ss %d, se %d, tot %d bs %d, be %d\n",
		vactive, vsync_start, vsync_end, vtotal, vblank_start,
		vblank_end);

	/* Adjust for register values, and check for overflow. */
	hactive--;
	if (check_overflow(hactive, HACTIVE_MASK, "CRTC hactive"))
		return 1;
	hsync_start--;
	if (check_overflow(hsync_start, HSYNCSTART_MASK, "CRTC hsync_start"))
		return 1;
	hsync_end--;
	if (check_overflow(hsync_end, HSYNCEND_MASK, "CRTC hsync_end"))
		return 1;
	htotal--;
	if (check_overflow(htotal, HTOTAL_MASK, "CRTC htotal"))
		return 1;
	hblank_start--;
	if (check_overflow(hblank_start, HBLANKSTART_MASK, "CRTC hblank_start"))
		return 1;
	hblank_end--;
	if (check_overflow(hblank_end, HBLANKEND_MASK, "CRTC hblank_end"))
		return 1;

	vactive--;
	if (check_overflow(vactive, VACTIVE_MASK, "CRTC vactive"))
		return 1;
	vsync_start--;
	if (check_overflow(vsync_start, VSYNCSTART_MASK, "CRTC vsync_start"))
		return 1;
	vsync_end--;
	if (check_overflow(vsync_end, VSYNCEND_MASK, "CRTC vsync_end"))
		return 1;
	vtotal--;
	if (check_overflow(vtotal, VTOTAL_MASK, "CRTC vtotal"))
		return 1;
	vblank_start--;
	if (check_overflow(vblank_start, VBLANKSTART_MASK, "CRTC vblank_start"))
		return 1;
	vblank_end--;
	if (check_overflow(vblank_end, VBLANKEND_MASK, "CRTC vblank_end"))
		return 1;

	*ht = (htotal << HTOTAL_SHIFT) | (hactive << HACTIVE_SHIFT);
	*hb = (hblank_start << HBLANKSTART_SHIFT) |
	      (hblank_end << HSYNCEND_SHIFT);
	*hs = (hsync_start << HSYNCSTART_SHIFT) | (hsync_end << HSYNCEND_SHIFT);

	*vt = (vtotal << VTOTAL_SHIFT) | (vactive << VACTIVE_SHIFT);
	*vb = (vblank_start << VBLANKSTART_SHIFT) |
	      (vblank_end << VSYNCEND_SHIFT);
	*vs = (vsync_start << VSYNCSTART_SHIFT) | (vsync_end << VSYNCEND_SHIFT);
	*ss = (hactive << SRC_SIZE_HORIZ_SHIFT) |
	      (vactive << SRC_SIZE_VERT_SHIFT);

	/* Start address and stride. */
	if (dinfo->pitch)
		hw->disp_a_stride = dinfo->pitch;
	else
		hw->disp_a_stride = var->xres_virtual * var->bits_per_pixel / 8;
	DBG_MSG("pitch is %d\n", hw->disp_a_stride);

	hw->disp_a_base = hw->disp_a_stride * var->yoffset +
			  var->xoffset * var->bits_per_pixel / 8;

	/* Check stride alignment. */
	if (hw->disp_a_stride % STRIDE_ALIGNMENT != 0) {
		WRN_MSG("display stride %d has bad alignment %d\n",
			hw->disp_a_stride, STRIDE_ALIGNMENT);
		return 1;
	}

	/* Set the palette to 8-bit mode. */
	*pipe_conf &= ~PIPECONF_GAMMA;
	return 0;
}

/* Program a (non-VGA) video mode. */
int
intelfbhw_program_mode(struct intelfb_info *dinfo,
		     const struct intelfb_hwstate *hw, int blank)
{
	int pipe = PIPE_A;
	u32 tmp;
	const u32 *dpll, *fp0, *fp1, *pipe_conf;
	const u32 *hs, *ht, *hb, *vs, *vt, *vb, *ss;
	u32 dpll_reg, fp0_reg, fp1_reg, pipe_conf_reg;
	u32 hsync_reg, htotal_reg, hblank_reg;
	u32 vsync_reg, vtotal_reg, vblank_reg;
	u32 src_size_reg;

	/* Assume single pipe, display plane A, analog CRT. */

	DBG_MSG("intelfbhw_program_mode\n");

	/* Disable VGA */
	tmp = INREG(VGACNTRL);
	tmp |= VGA_DISABLE;
	OUTREG(VGACNTRL, tmp);

	/* Check whether pipe A or pipe B is enabled. */
	if (hw->pipe_a_conf & PIPECONF_ENABLE)
		pipe = PIPE_A;
	else if (hw->pipe_b_conf & PIPECONF_ENABLE)
		pipe = PIPE_B;

	dinfo->pipe = pipe;

	if (pipe == PIPE_B) {
		dpll = &hw->dpll_b;
		fp0 = &hw->fpb0;
		fp1 = &hw->fpb1;
		pipe_conf = &hw->pipe_b_conf;
		hs = &hw->hsync_b;
		hb = &hw->hblank_b;
		ht = &hw->htotal_b;
		vs = &hw->vsync_b;
		vb = &hw->vblank_b;
		vt = &hw->vtotal_b;
		ss = &hw->src_size_b;
		dpll_reg = DPLL_B;
		fp0_reg = FPB0;
		fp1_reg = FPB1;
		pipe_conf_reg = PIPEBCONF;
		hsync_reg = HSYNC_B;
		htotal_reg = HTOTAL_B;
		hblank_reg = HBLANK_B;
		vsync_reg = VSYNC_B;
		vtotal_reg = VTOTAL_B;
		vblank_reg = VBLANK_B;
		src_size_reg = SRC_SIZE_B;
	} else {
		dpll = &hw->dpll_a;
		fp0 = &hw->fpa0;
		fp1 = &hw->fpa1;
		pipe_conf = &hw->pipe_a_conf;
		hs = &hw->hsync_a;
		hb = &hw->hblank_a;
		ht = &hw->htotal_a;
		vs = &hw->vsync_a;
		vb = &hw->vblank_a;
		vt = &hw->vtotal_a;
		ss = &hw->src_size_a;
		dpll_reg = DPLL_A;
		fp0_reg = FPA0;
		fp1_reg = FPA1;
		pipe_conf_reg = PIPEACONF;
		hsync_reg = HSYNC_A;
		htotal_reg = HTOTAL_A;
		hblank_reg = HBLANK_A;
		vsync_reg = VSYNC_A;
		vtotal_reg = VTOTAL_A;
		vblank_reg = VBLANK_A;
		src_size_reg = SRC_SIZE_A;
	}

	/* Disable planes A and B. */
	tmp = INREG(DSPACNTR);
	tmp &= ~DISPPLANE_PLANE_ENABLE;
	OUTREG(DSPACNTR, tmp);
	tmp = INREG(DSPBCNTR);
	tmp &= ~DISPPLANE_PLANE_ENABLE;
	OUTREG(DSPBCNTR, tmp);

	/* Wait for vblank.  For now, just wait for a 50Hz cycle (20ms)) */
	mdelay(20);

	/* Disable Sync */
	tmp = INREG(ADPA);
	tmp &= ~ADPA_DPMS_CONTROL_MASK;
	tmp |= ADPA_DPMS_D3;
	OUTREG(ADPA, tmp);

	/* turn off pipe */
	tmp = INREG(pipe_conf_reg);
	tmp &= ~PIPECONF_ENABLE;
	OUTREG(pipe_conf_reg, tmp);

	/* turn off PLL */
	tmp = INREG(dpll_reg);
	dpll_reg &= ~DPLL_VCO_ENABLE;
	OUTREG(dpll_reg, tmp);

	/* Set PLL parameters */
	OUTREG(dpll_reg, *dpll & ~DPLL_VCO_ENABLE);
	OUTREG(fp0_reg, *fp0);
	OUTREG(fp1_reg, *fp1);

	/* Set pipe parameters */
	OUTREG(hsync_reg, *hs);
	OUTREG(hblank_reg, *hb);
	OUTREG(htotal_reg, *ht);
	OUTREG(vsync_reg, *vs);
	OUTREG(vblank_reg, *vb);
	OUTREG(vtotal_reg, *vt);
	OUTREG(src_size_reg, *ss);
	
	/* Set ADPA */
	OUTREG(ADPA, (hw->adpa & ~(ADPA_DPMS_CONTROL_MASK)) | ADPA_DPMS_D3);

	/* Enable PLL */
	tmp = INREG(dpll_reg);
	tmp |= DPLL_VCO_ENABLE;
	OUTREG(dpll_reg, tmp);

	/* Enable pipe */
	OUTREG(pipe_conf_reg, *pipe_conf | PIPECONF_ENABLE);

	/* Enable sync */
	tmp = INREG(ADPA);
	tmp &= ~ADPA_DPMS_CONTROL_MASK;
	tmp |= ADPA_DPMS_D0;
	OUTREG(ADPA, tmp);

	/* setup display plane */
	OUTREG(DSPACNTR, hw->disp_a_ctrl & ~DISPPLANE_PLANE_ENABLE);
	OUTREG(DSPASTRIDE, hw->disp_a_stride);
	OUTREG(DSPABASE, hw->disp_a_base);

	/* Enable plane */
	if (!blank) {
		tmp = INREG(DSPACNTR);
		tmp |= DISPPLANE_PLANE_ENABLE;
		OUTREG(DSPACNTR, tmp);
		OUTREG(DSPABASE, hw->disp_a_base);
	}

	return 0;
}

static int
wait_ring(struct intelfb_info *dinfo, int n)
{
	int i = 0;
	unsigned long end;
	u32 last_head = INREG(PRI_RING_HEAD) & RING_HEAD_MASK;

#if 0
	DBG_MSG("wait_ring: %d\n", n);
#endif

	end = jiffies + (HZ * 3);
	while (dinfo->ring_space < n) {
		dinfo->ring_head = INREG(PRI_RING_HEAD) & RING_HEAD_MASK;
		dinfo->ring_space = dinfo->ring_head -
				    (dinfo->ring_tail + RING_MIN_FREE);
		if (dinfo->ring_space < 0)
			dinfo->ring_space += dinfo->ring_size;
		if (dinfo->ring_head != last_head) {
			end = jiffies + (HZ * 3);
			last_head = dinfo->ring_head;
		}
		i++;
		if (time_before(end, jiffies)) {
			WRN_MSG("space: %d wanted %d\n", dinfo->ring_space, n);
			WRN_MSG("lockup\n");
			break;
		}
		udelay(1);
	}
	return i;
}

void
intelfbhw_do_sync(struct intelfb_info *dinfo)
{
#if USE_SYNC_PAGE
	u32 newval;
#endif
	u32 tmp;
	int i = 0;

#if VERBOSE > 0
	DBG_MSG("intelfbhw_do_sync\n");
#endif

#if USE_SYNC_PAGE
	/*
	 * Although doing MI_STORE_DWORD_IMM after the MI_FLUSH is supposed
	 * to make sure everything is synchronised, there is still some
	 * mis-ordering of operations when mixing 2D with direct CPU
	 * writes to the framebuffer.
	 */
	newval = readl(dinfo->syncpage_virt);
	newval++;
#if VERBOSE > 0
	DBG_MSG("intelfbhw_do_sync: %d\n", newval);
#endif
	START_RING(6);
	OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE);
	OUT_RING(MI_NOOP);
	OUT_RING(MI_STORE_DWORD_IMM);
	OUT_RING(dinfo->syncpage_phys);
	OUT_RING(newval);
	OUT_RING(MI_NOOP);
	ADVANCE_RING();
	while ((tmp = readl(dinfo->syncpage_virt)) != newval && i < 10000) {
		i++;
		udelay(10);
	}
	if (tmp != newval) {
		DBG_MSG("intelfbhw_do_sync: STORE_DWORD_IMM returns %d "
			"instead of %d\n", tmp, newval);
	} else {
#if VERBOSE > 1
		DBG_MSG("intelfbhw_do_sync: done in %d iterations\n", i);
#endif
	}
#else
	/*
	 * Send a flush, then wait until the ring is empty.  This is what
	 * the XFree86 driver does, and actually it doesn't seem a lot worse
	 * than the recommended method (both have problems).
	 */
	START_RING(2);
	OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE);
	OUT_RING(MI_NOOP);
	ADVANCE_RING();
	wait_ring(dinfo, dinfo->ring_size - RING_MIN_FREE);
	dinfo->ring_space = dinfo->ring_size - RING_MIN_FREE;
#endif
}

static void
refresh_ring(struct intelfb_info *dinfo)
{
	DBG_MSG("refresh_ring\n");

	dinfo->ring_head = INREG(PRI_RING_HEAD) & RING_HEAD_MASK;
	dinfo->ring_tail = INREG(PRI_RING_TAIL) & RING_TAIL_MASK;
	dinfo->ring_space = dinfo->ring_head -
			    (dinfo->ring_tail + RING_MIN_FREE);
	if (dinfo->ring_space < 0)
		dinfo->ring_space += dinfo->ring_size;
}

static void
reset_state(struct intelfb_info *dinfo)
{
	int i;
	u32 tmp;

	DBG_MSG("reset_state\n");

	for (i = 0; i < FENCE_NUM; i++)
		OUTREG(FENCE + (i << 2), 0);

	/* Flush the ring buffer if it's enabled. */
	tmp = INREG(PRI_RING_LENGTH);
	if (tmp & RING_ENABLE) {
		DBG_MSG("reset_state: ring was enabled\n");
		refresh_ring(dinfo);
		intelfbhw_do_sync(dinfo);
		DO_RING_IDLE();
	}

	OUTREG(PRI_RING_LENGTH, 0);
	OUTREG(PRI_RING_HEAD, 0);
	OUTREG(PRI_RING_TAIL, 0);
	OUTREG(PRI_RING_START, 0);
}

/* Stop the 2D engine, and turn off the ring buffer. */
void
intelfbhw_2d_stop(struct intelfb_info *dinfo)
{
	DBG_MSG("intelfbhw_2d_stop: accel: %d, ring_active: %d\n", dinfo->accel,
		dinfo->ring_active);

	if (!dinfo->accel)
		return;

	dinfo->ring_active = 0;
	reset_state(dinfo);
}

/*
 * Enable the ring buffer, and initialise the 2D engine.
 * It is assumed that the graphics engine has been stopped by previously
 * calling intelfb_2d_stop().
 */
void
intelfbhw_2d_start(struct intelfb_info *dinfo)
{
	DBG_MSG("intelfbhw_2d_start: accel: %d, ring_active: %d\n",
		dinfo->accel, dinfo->ring_active);

	if (!dinfo->accel)
		return;

	/* Initialise the primary ring buffer. */
	OUTREG(PRI_RING_LENGTH, 0);
	OUTREG(PRI_RING_TAIL, 0);
	OUTREG(PRI_RING_HEAD, 0);

	OUTREG(PRI_RING_START, dinfo->ring_base_phys & RING_START_MASK);
	OUTREG(PRI_RING_LENGTH,
		((dinfo->ring_size - GTT_PAGE_SIZE) & RING_LENGTH_MASK) |
		RING_NO_REPORT | RING_ENABLE);
	refresh_ring(dinfo);
	dinfo->ring_active = 1;

	DBG_MSG("INSTPM was 0x%08x, setting to 0x%08x\n", INREG(INSTPM),
		0x1f << 16);
	OUTREG(INSTPM, 0x1f << 16);
	OUTREG(INSTPM, 0x1f << 16);
}

/* 2D fillrect (solid fill or invert) */
void
intelfbhw_do_fillrect(struct intelfb_info *dinfo, u32 x, u32 y, u32 w, u32 h,
		      u32 color, u32 pitch, u32 bpp, u32 rop)
{
	u32 br00, br09, br13, br14, br16;

#if VERBOSE > 1
	DBG_MSG("intelfbhw_do_fillrect: (%d,%d) %dx%d, c 0x%06x, p %d bpp %d, "
		"rop 0x%02x\n", x, y, w, h, color, pitch, bpp, rop);
#endif

	br00 = COLOR_BLT_CMD;
	br09 = dinfo->fb_offset + (y * pitch + x * (bpp / 8));
	br13 = (rop << ROP_SHIFT) | pitch;
	br14 = (h << HEIGHT_SHIFT) | ((w * bpp / 8) << WIDTH_SHIFT);
	br16 = color;

	switch (bpp) {
	case 8:
		br13 |= COLOR_DEPTH_8;
		break;
	case 16:
		br13 |= COLOR_DEPTH_16;
		break;
	case 32:
		br13 |= COLOR_DEPTH_32;
		br00 |= WRITE_ALPHA | WRITE_RGB;
		break;
	}

	START_RING(6);
	OUT_RING(br00);
	OUT_RING(br13);
	OUT_RING(br14);
	OUT_RING(br09);
	OUT_RING(br16);
	OUT_RING(MI_NOOP);
	ADVANCE_RING();
}

void
intelfbhw_do_bitblt(struct intelfb_info *dinfo, u32 curx, u32 cury,
		    u32 dstx, u32 dsty, u32 w, u32 h, u32 pitch, u32 bpp)
{
	u32 br00, br09, br11, br12, br13, br22, br23, br26;

#if VERBOSE > 0
	DBG_MSG("intelfbhw_do_bitblt: (%d,%d)->(%d,%d) %dx%d, p %d bpp %d\n",
		curx, cury, dstx, dsty, w, h, pitch, bpp);
#endif

	br00 = XY_SRC_COPY_BLT_CMD;
	br09 = dinfo->fb_offset;
	br11 = (pitch << PITCH_SHIFT);
	br12 = dinfo->fb_offset;
	br13 = (SRC_ROP_GXCOPY << ROP_SHIFT) | (pitch << PITCH_SHIFT);
	br22 = (dstx << WIDTH_SHIFT) | (dsty << HEIGHT_SHIFT);
	br23 = ((dstx + w) << WIDTH_SHIFT) |
	       ((dsty + h) << HEIGHT_SHIFT);
	br26 = (curx << WIDTH_SHIFT) | (cury << HEIGHT_SHIFT);

	switch (bpp) {
	case 8:
		br13 |= COLOR_DEPTH_8;
		break;
	case 16:
		br13 |= COLOR_DEPTH_16;
		break;
	case 32:
		br13 |= COLOR_DEPTH_32;
		br00 |= WRITE_ALPHA | WRITE_RGB;
		break;
	}

	START_RING(8);
	OUT_RING(br00);
	OUT_RING(br13);
	OUT_RING(br22);
	OUT_RING(br23);
	OUT_RING(br09);
	OUT_RING(br26);
	OUT_RING(br11);
	OUT_RING(br12);
	ADVANCE_RING();
}

int
intelfbhw_do_drawglyph(struct intelfb_info *dinfo, u32 fg, u32 bg, u32 w, u32 h,
		       u8* cdat, u32 x, u32 y, u32 pitch, u32 bpp)
{
	int i, n = 0, fw_bytes, bytes_per_src_line;
	int nbytes, ndwords, pad, tmp;
	u16 *wcdat;
	u32 *dwcdat;
	u32 br00, br09, br13, br18, br19, br22, br23;

#if 0
	DBG_MSG("intelfbhw_do_drawglyph: (%d,%d) %dx%d\n", x, y, w, h);
#endif

	/* Support fonts up to 32 pixels wide. */
	if (w > 32)
		return 0;

	/* Number of bytes required for each cdat scanline. */
	fw_bytes = ROUND_UP_TO(w, 8) / 8;

	/* Src scanlines are word (16-bit) padded. */
	bytes_per_src_line = ROUND_UP_TO(fw_bytes, 2);

	/* Total bytes of padded scanline data to write out. */
	nbytes = h * bytes_per_src_line;

	/*
	 * Check if the glyph data exceeds the immediate mode limit.
	 * It would take a large font (1K pixels) to hit this limit.
	 */
	if (nbytes > MAX_MONO_IMM_SIZE)
		return 0;

	/* Src data is packaged a dword (32-bit) at a time. */
	ndwords = ROUND_UP_TO(nbytes, 4) / 4;

	/* Ring has to be padded to a quad word. */
	pad = ndwords % 2;

	/* For easy reference of the glyph data in different sized chunks. */
	wcdat = (u16 *)cdat;
	dwcdat = (u32 *)cdat;

	tmp = (XY_MONO_SRC_IMM_BLT_CMD & DW_LENGTH_MASK) + ndwords;
	br00 = (XY_MONO_SRC_IMM_BLT_CMD & ~DW_LENGTH_MASK) | tmp;
	br09 = dinfo->fb_offset;
	br13 = (SRC_ROP_GXCOPY << ROP_SHIFT) | (pitch << PITCH_SHIFT);
	br18 = bg;
	br19 = fg;
	br22 = (x << WIDTH_SHIFT) | (y << HEIGHT_SHIFT);
	br23 = ((x + w) << WIDTH_SHIFT) | ((y + h) << HEIGHT_SHIFT);

	switch (bpp) {
	case 8:
		br13 |= COLOR_DEPTH_8;
		break;
	case 16:
		br13 |= COLOR_DEPTH_16;
		break;
	case 32:
		br13 |= COLOR_DEPTH_32;
		br00 |= WRITE_ALPHA | WRITE_RGB;
		break;
	}

#if 0
	DBG_MSG("ndwords + pad is %d, pad is %d\n", ndwords + pad, pad);
#endif
	START_RING(ndwords + pad);
	OUT_RING(br00);
	OUT_RING(br13);
	OUT_RING(br22);
	OUT_RING(br23);
	OUT_RING(br09);
	OUT_RING(br18);
	OUT_RING(br19);
	i = h;
	switch (fw_bytes) {
	case 1:
		while (i >= 2) {
			OUT_RING(cdat[0] | cdat[1] << 16);
			cdat +=2;
			i -= 2;
			n++;
		}
		if (i) {
			OUT_RING(cdat[0]);
			n++;
		}
		break;
	case 2:
		while (i >= 2) {
			OUT_RING(wcdat[0] | wcdat[1] << 16);
			wcdat += 2;
			i -= 2;
			n++;
		}
		if (i) {
			OUT_RING(wcdat[0]);
			n++;
		}
		break;
	case 3:
		while (i) {
			OUT_RING(wcdat[0] | cdat[3] << 16);
			wcdat += 2;
			cdat += 4;
			i--;
			n++;
		}
		break;
	case 4:
		while(i) {
			OUT_RING(dwcdat[0]);
			i--;
			n++;
		}
	}
	if (pad) {
		OUT_RING(MI_NOOP);
		n++;
	}
#if 0
	DBG_MSG("%d immediate bytes + pad\n", n);
#endif
	ADVANCE_RING();
	return 1;
}

/* HW cursor functions. */
void
intelfbhw_cursor_init(struct intelfb_info *dinfo)
{
	u32 tmp;

	DBG_MSG("intelfbhw_cursor_init\n");

	if (!dinfo->cursor_base)
		return;

	if (dinfo->mobile) {
		if (!dinfo->cursor_base_real)
			return;
		tmp = INREG(CURSOR_A_CONTROL);
		tmp &= ~(CURSOR_MODE_MASK | CURSOR_MOBILE_GAMMA_ENABLE |
			 CURSOR_MEM_TYPE_LOCAL |
			 (1 << CURSOR_PIPE_SELECT_SHIFT));
		tmp |= CURSOR_MODE_DISABLE;
		OUTREG(CURSOR_A_CONTROL, tmp);
		OUTREG(CURSOR_A_BASEADDR, dinfo->cursor_base_real);
	} else {
#if 0
		tmp = INREG(CURSOR_CONTROL);
		tmp &= ~(CURSOR_FORMAT_MASK | CURSOR_GAMMA_ENABLE |
			 CURSOR_ENABLE | CURSOR_STRIDE_MASK);
#endif
		tmp = CURSOR_FORMAT_3C;
		OUTREG(CURSOR_CONTROL, tmp);
		OUTREG(CURSOR_A_BASEADDR, dinfo->cursor_offset);
		tmp = (64 << CURSOR_SIZE_H_SHIFT) |
		      (64 << CURSOR_SIZE_V_SHIFT);
		OUTREG(CURSOR_SIZE, tmp);
	}
}

void
intelfbhw_cursor_hide(struct intelfb_info *dinfo)
{
	u32 tmp;

#if VERBOSE > 1
	DBG_MSG("intelfbhw_cursor_hide\n");
#endif

	if (!dinfo->cursor_base)
		return;

	dinfo->cursor.enabled = 0;
	dinfo->cursor.on = 0;
	if (dinfo->mobile) {
		if (!dinfo->cursor_base_real)
			return;
		tmp = INREG(CURSOR_A_CONTROL);
		tmp &= ~CURSOR_MODE_MASK;
		tmp |= CURSOR_MODE_DISABLE;
		OUTREG(CURSOR_A_CONTROL, tmp);
		/* Flush changes */
		OUTREG(CURSOR_A_BASEADDR, dinfo->cursor_base_real);
	} else {
		tmp = INREG(CURSOR_CONTROL);
		tmp &= ~CURSOR_ENABLE;
		OUTREG(CURSOR_CONTROL, tmp);
	}
}

void
intelfbhw_cursor_show(struct intelfb_info *dinfo)
{
	u32 tmp;

#if VERBOSE > 1
	DBG_MSG("intelfbhw_cursor_show\n");
#endif

	if (!dinfo->cursor_base)
		return;

	dinfo->cursor.on = 1;
	dinfo->cursor.enabled = 1;

	if (dinfo->cursor.blanked)
		return;

	if (dinfo->mobile) {
		if (!dinfo->cursor_base_real)
			return;
		tmp = INREG(CURSOR_A_CONTROL);
		tmp &= ~CURSOR_MODE_MASK;
		tmp |= CURSOR_MODE_64_4C_AX;
		OUTREG(CURSOR_A_CONTROL, tmp);
		/* Flush changes */
		OUTREG(CURSOR_A_BASEADDR, dinfo->cursor_base_real);
	} else {
		tmp = INREG(CURSOR_CONTROL);
		tmp |= CURSOR_ENABLE;
		OUTREG(CURSOR_CONTROL, tmp);
	}
}

void
intelfbhw_cursor_setpos(struct intelfb_info *dinfo, int x, int y)
{
	u32 tmp;

#if VERBOSE > 1
	DBG_MSG("intelfbhw_cursor_setpos: (%d, %d)\n", x, y);
#endif

	/*
	 * Sets the position.  The coordinates are assumed to already
	 * have any offset adjusted.  Assume that the cursor is never
	 * completely off-screen, and that x, y are always >= 0.
	 */

	if (!dinfo->cursor_base)
		return;

	tmp = ((x & CURSOR_POS_MASK) << CURSOR_X_SHIFT) |
	      ((y & CURSOR_POS_MASK) << CURSOR_Y_SHIFT);
	OUTREG(CURSOR_A_POSITION, tmp);
}

void
intelfbhw_cursor_setcolor(struct intelfb_info *dinfo, u32 bg, u32 fg)
{
#if VERBOSE > 1
	DBG_MSG("intelfbhw_cursor_setcolor\n");
#endif

	if (!dinfo->cursor_base)
		return;

	OUTREG(CURSOR_A_PALETTE0, bg & CURSOR_PALETTE_MASK);
	OUTREG(CURSOR_A_PALETTE1, fg & CURSOR_PALETTE_MASK);
	OUTREG(CURSOR_A_PALETTE2, fg & CURSOR_PALETTE_MASK);
	OUTREG(CURSOR_A_PALETTE3, bg & CURSOR_PALETTE_MASK);
}

void
intelfbhw_cursor_load(struct intelfb_info *dinfo, struct display *disp)
{
	u32 xline, mline;
	int i;

	DBG_MSG("intelfbhw_cursor_load\n");

	if (!dinfo->cursor_base)
		return;

	intelfb_create_cursor_shape(dinfo, disp);
	xline = (1 << dinfo->cursor.w) - 1;
	mline = ~xline;
	for (i = 0; i < dinfo->cursor.u; i++) {
		writel(~0, dinfo->cursor_base + i * 16);
		writel(~0, dinfo->cursor_base + i * 16 + 4);
		writel(0, dinfo->cursor_base + i * 16 + 8);
		writel(0, dinfo->cursor_base + i * 16 + 12);
	}
	for (; i < dinfo->cursor.d; i++) {
		writel(mline, dinfo->cursor_base + i * 16);
		writel(~0, dinfo->cursor_base + i * 16 + 4);
		writel(xline, dinfo->cursor_base + i * 16 + 8);
		writel(0, dinfo->cursor_base + i * 16 + 12);
	}
	for (; i < 64; i++) {
		writel(~0, dinfo->cursor_base + i * 16);
		writel(~0, dinfo->cursor_base + i * 16 + 4);
		writel(0, dinfo->cursor_base + i * 16 + 8);
		writel(0, dinfo->cursor_base + i * 16 + 12);
	}
}


