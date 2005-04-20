/*-
 * Copyright (c) 2000, 2001 Andrew Miklic, Andrew Gallatin, Peter Jeremy,
 * and Thomas V. Crimi
 * All rights reserved.
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
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/stdarg.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <isa/isareg.h>
#include <dev/fb/vgareg.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/pc/bios.h>
#include <machine/clock.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/pc/vesa.h>
#include <machine/resource.h>
#include <machine/rpb.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/fb/fbreg.h>
#include <dev/syscons/syscons.h>
#include <dev/fb/gfb.h>
#include <dev/gfb/gfb_pci.h>
#include <dev/fb/tga.h>
#include <dev/tga/tga_pci.h>

#include "opt_fb.h"

/* TGA-specific FB video driver function declarations */
static int tga_error(void);
static vi_init_t tga_init;
static void tga2_init(struct gfb_softc *, int);

/* TGA-specific functionality. */
static gfb_builtin_save_palette_t tga_builtin_save_palette;
static gfb_builtin_load_palette_t tga_builtin_load_palette;
#ifdef TGA2
static gfb_builtin_save_palette_t tga2_builtin_save_palette;
static gfb_builtin_load_palette_t tga2_builtin_load_palette;
static gfb_builtin_save_cursor_palette_t tga2_builtin_save_cursor_palette;
static gfb_builtin_load_cursor_palette_t tga2_builtin_load_cursor_palette;
#endif 
static gfb_builtin_read_hw_cursor_t tga_builtin_read_hw_cursor;
static gfb_builtin_set_hw_cursor_t tga_builtin_set_hw_cursor;
static gfb_builtin_set_hw_cursor_shape_t tga_builtin_set_hw_cursor_shape;
static void bt463_load_palette_intr(struct gfb_softc *);
static void bt463_load_cursor_palette_intr(struct gfb_softc *);
static int tga_schedule_intr(struct gfb_softc *, void (*)(struct gfb_softc *));

/* RAMDAC interface functions */
static gfb_ramdac_wr_t tga_bt485_wr;
static gfb_ramdac_rd_t tga_bt485_rd;
static gfb_ramdac_wr_t tga_bt463_wr;
static gfb_ramdac_rd_t tga_bt463_rd;
static gfb_ramdac_wr_t tga2_ibm561_wr;
static gfb_ramdac_rd_t tga2_ibm561_rd;
static void tga2_ics9110_wr(struct gfb_softc *, int);

/* RAMDAC-specific functions */
static gfb_ramdac_init_t bt463_init;
static void bt463_update_window_type(struct gfb_softc *);
#if 0
static gfb_ramdac_save_palette_t bt463_save_palette;
static gfb_ramdac_load_palette_t bt463_load_palette;
#endif 
static gfb_ramdac_save_cursor_palette_t bt463_save_cursor_palette;
static gfb_ramdac_load_cursor_palette_t bt463_load_cursor_palette;
static gfb_ramdac_init_t bt485_init;
static gfb_ramdac_save_palette_t bt485_save_palette;
static gfb_ramdac_load_palette_t bt485_load_palette;
static gfb_ramdac_save_cursor_palette_t bt485_save_cursor_palette;
static gfb_ramdac_load_cursor_palette_t bt485_load_cursor_palette;
static gfb_ramdac_read_hw_cursor_t bt485_read_hw_cursor;
static gfb_ramdac_set_hw_cursor_t bt485_set_hw_cursor;
static gfb_ramdac_set_hw_cursor_shape_t bt485_set_hw_cursor_shape;
static gfb_ramdac_init_t ibm561_init;
static gfb_ramdac_save_palette_t ibm561_save_palette;
static gfb_ramdac_load_palette_t ibm561_load_palette;
static gfb_ramdac_save_cursor_palette_t ibm561_save_cursor_palette;
static gfb_ramdac_load_cursor_palette_t ibm561_load_cursor_palette;

/* Video Driver-generic functions */
static vi_query_mode_t tga_query_mode;
static vi_set_mode_t tga_set_mode;
static vi_blank_display_t tga_blank_display;
#if 0
static vi_ioctl_t tga_ioctl;
#endif
static vi_set_border_t tga_set_border;
static vi_set_win_org_t tga_set_win_org;
static vi_fill_rect_t tga_fill_rect;
static vi_bitblt_t tga_bitblt;
static vi_clear_t tga_clear;
static vi_putc_t tga_putc;
static vi_puts_t tga_puts;
static vi_putm_t tga_putm;

static video_switch_t tgavidsw = {
	gfb_probe,
	tga_init,
	gfb_get_info,
	tga_query_mode,
	tga_set_mode,
	gfb_save_font,
	gfb_load_font,
	gfb_show_font,
	gfb_save_palette,
	gfb_load_palette,
	tga_set_border,
	gfb_save_state,
	gfb_load_state,
	tga_set_win_org,
	gfb_read_hw_cursor,
	gfb_set_hw_cursor,
	gfb_set_hw_cursor_shape,
	tga_blank_display,
	gfb_mmap,
	gfb_ioctl,
	tga_clear,
	tga_fill_rect,
	tga_bitblt,
	tga_error,
	tga_error,
	gfb_diag,
	gfb_save_cursor_palette,
	gfb_load_cursor_palette,
	gfb_copy,
	gfb_putp,
	tga_putc,
	tga_puts,
	tga_putm,
};

VIDEO_DRIVER(tga, tgavidsw, NULL);

extern sc_rndr_sw_t txtrndrsw;
RENDERER(tga, 0, txtrndrsw, gfb_set);

#ifdef SC_PIXEL_MODE
extern sc_rndr_sw_t gfbrndrsw;
RENDERER(tga, PIXEL_MODE, gfbrndrsw, gfb_set);
#endif /* SC_PIXEL_MODE */

#ifndef SC_NO_MODE_CHANGE
extern sc_rndr_sw_t grrndrsw;
RENDERER(tga, GRAPHICS_MODE, grrndrsw, gfb_set);
#endif /* SC_NO_MODE_CHANGE */

RENDERER_MODULE(tga, gfb_set);

#define	MHz	* 1000000
#define	KHz	* 1000

extern struct gfb_softc *gfb_device_softcs[2][MAX_NUM_GFB_CARDS];

/*
   The following 3 variables exist only because we need statically
   allocated structures very early in boot to support tga_configure()...
*/
extern struct gfb_softc console;
extern video_adapter_t console_adp;
extern struct gfb_conf console_gfbc;
extern u_char console_palette_red[256];
extern u_char console_palette_green[256];
extern u_char console_palette_blue[256];
extern u_char console_cursor_palette_red[3];
extern u_char console_cursor_palette_green[3];
extern u_char console_cursor_palette_blue[3];

static struct monitor decmonitors[] = {
	/* 0x0: 1280 x 1024 @ 72Hz */
	{ 1280,	32,	160,	232,
	  1024,	3,	3,	33,
	  130808 KHz },

	/* 0x1: 1280 x 1024 @ 66Hz */
	{ 1280,	32,	160,	232,
	  1024,	3,	3,	33,
	  119840 KHz },

	/* 0x2: 1280 x 1024 @ 60Hz */
	{ 1280,	44,	184,	200,
	  1024,	3,	3,	26,
	  108180 KHz },

	/* 0x3: 1152 x  900 @ 72Hz */
	{ 1152,	64,	112,	176,
	  900,	6,	10,	44,
	  103994 KHz },

	/* 0x4: 1600 x 1200 @ 65Hz */
	{ 1600,	32,	192,	336,
	  1200,	1,	3,	46,
	  175 MHz },

	/* 0x5: 1024 x  768 @ 70Hz */
	{ 1024,	24,	136,	144,
	  768,	3,	6,	29,
	  75 MHz },

	/* 0x6: 1024 x  768 @ 72Hz */
	{ 1024,	16,	128,	128,
	  768,	1,	6,	22,
	  74 MHz },

	/* 0x7: 1024 x  864 @ 60Hz */
	{ 1024,	12,	128,	116,
	  864,	0,	3,	34,
	  69 MHz },

	/* 0x8: 1024 x  768 @ 60Hz */
	{ 1024,	56,	64,	200,
	  768,	7,	9,	26,
	  65 MHz },

	/* 0x9:  800 x  600 @ 72Hz */
	{ 800,	56,	120,	64,
	  600,	37,	6,	23,
	  50 MHz },

	/* 0xa:  800 x  600 @ 60Hz */
	{ 800,	40,	128,	88,
	  600,	1,	4,	23,
	  40 MHz },

	/* 0xb:  640 x  480 @ 72Hz */
	{ 640,	24,	40,	128,
	  480,	9,	3,	28,
	  31500 KHz },

	/* 0xc:  640 x  480 @ 60Hz */
	{ 640,	16,	96,	48,
	  480,	10,	2,	33,
	  25175 KHz },

	/* 0xd: 1280 x 1024 @ 75Hz */
	{ 1280,	16,	144,	248,
	  1024,	1,	3,	38,
	  135 MHz  },

	/* 0xe: 1280 x 1024 @ 60Hz */
	{ 1280,	19,	163,	234,
	  1024,	6,	7,	44,
	  110 MHz },

	/* 0xf: 1600 x 1200 @ 75Hz */
	/* XXX -- this one's weird.  rcd */
	{ 1600,	32,	192,	336,
	  1200,	1,	3,	46,
	  202500 KHz }
};

#undef MHz
#undef KHz

#undef KB
#define	KB	* 1024
#undef MB
#define	MB	* 1024 * 1024

/*
 * These are the 16 default VGA colors--these are replicated 16 times as the
 * initial (default) color-map.  The text rendering functions use entries
 * 0..15 for normal foreground/background colors.  The entries 128..255 are
 * used for blinking entries--when "on," they contain the foreground color
 * entries; when "off," they contain the background color entries...
 */
static const struct cmap {
	u_char red;
	u_char green;
	u_char blue;
} default_cmap[16] = {
	{0x00, 0x00, 0x00},	/* Black */
	{0x00, 0x00, 0xff},	/* Blue */
	{0x00, 0xff, 0x00},	/* Green */
	{0x00, 0xc0, 0xc0},	/* Cyan */
	{0xff, 0x00, 0x00},	/* Red */
	{0xc0, 0x00, 0xc0},	/* Magenta */
	{0xc0, 0xc0, 0x00},	/* Brown */
	{0xc0, 0xc0, 0xc0},	/* Light Grey */
	{0x80, 0x80, 0x80},	/* Dark Grey */
	{0x80, 0x80, 0xff},	/* Light Blue */
	{0x80, 0xff, 0x80},	/* Light Green */
	{0x80, 0xff, 0xff},	/* Light Cyan */
	{0xff, 0x80, 0x80},	/* Light Red */
	{0xff, 0x80, 0xff},	/* Light Magenta */
	{0xff, 0xff, 0x80},	/* Yellow */
	{0xff, 0xff, 0xff}	/* White */
};

extern struct gfb_font bold8x16;

/*****************************************************************************
 *
 * FB-generic functions
 *
 ****************************************************************************/

static int
tga_init(int unit, video_adapter_t *adp, int flags)
{
	struct gfb_softc *sc;
	struct gfb_conf *gfbc;
	unsigned int monitor;
	int card_type;
	int gder;
	int deep;
	int addrmask;
	int cs;
	int error;
	gfb_reg_t ccbr;

	/* Assume the best... */
	error = 0;

	sc = gfb_device_softcs[adp->va_model][unit];
	gfbc = sc->gfbc;

	/* Initialize palette counts... */
	gfbc->palette.count = 256;
	gfbc->cursor_palette.count = 3;

	/* Initialize the adapter... */
	gder = BASIC_READ_TGA_REGISTER(adp, TGA_REG_GDER);
	addrmask = (gder & GDER_ADDR_MASK) >> GDER_ADDR_SHIFT;
	deep = (gder & GDER_DEEP) != 0;
	cs = (gder & GDER_CS) == 0;
	card_type = TGA_TYPE_UNKNOWN;
	adp->va_little_bitian = 1;
	adp->va_little_endian = 0;
	adp->va_initial_mode = 0;
	adp->va_initial_bios_mode = 0;
	adp->va_mode = 0;
	adp->va_info.vi_mem_model = V_INFO_MM_TEXT;
	adp->va_info.vi_mode = M_VGA_M80x30;
	adp->va_info.vi_flags = V_INFO_COLOR;
	adp->va_buffer = adp->va_mem_base;
	adp->va_buffer_size = 4 MB * (1 + addrmask);
	adp->va_registers = adp->va_buffer + TGA_REG_SPACE_OFFSET;
	adp->va_registers_size = 2 KB;
	adp->va_window = adp->va_buffer + (adp->va_buffer_size / 2);
	adp->va_info.vi_window = vtophys(adp->va_window);
	adp->va_window_size = (deep ? 4 MB : 2 MB);
	adp->va_info.vi_window_size = adp->va_window_size;
	adp->va_window_gran = adp->va_window_size;
	adp->va_info.vi_window_gran = adp->va_window_gran;
	adp->va_info.vi_buffer = vtophys(adp->va_buffer);
	adp->va_info.vi_buffer_size = adp->va_buffer_size;
	adp->va_disp_start.x = 0;
	adp->va_disp_start.y = 0;
	adp->va_info.vi_depth = (deep ? 32 : 8);
	adp->va_info.vi_planes = adp->va_info.vi_depth / 8;
	adp->va_info.vi_width = (READ_GFB_REGISTER(adp, TGA_REG_VHCR) &
				   VHCR_ACTIVE_MASK);
	adp->va_info.vi_width |= (READ_GFB_REGISTER(adp, TGA_REG_VHCR) &
				   0x30000000) >> 19;
	switch(adp->va_info.vi_width) {
	case 0:
		adp->va_info.vi_width = 8192;
		break;
	case 1:
		adp->va_info.vi_width = 8196;
		break;
	default:
		adp->va_info.vi_width *= 4;
		break;
	}
	adp->va_info.vi_height = (READ_GFB_REGISTER(adp, TGA_REG_VVCR) &
				   VVCR_ACTIVE_MASK);
	adp->va_line_width = adp->va_info.vi_width * adp->va_info.vi_depth / 8;
	if(READ_GFB_REGISTER(adp, TGA_REG_VHCR) & VHCR_ODD)
		adp->va_info.vi_width -= 4;

	/*
	   Set the video base address and the cursor base address to
	   something known such that the video base address is at
	   least 1 KB past the cursor base address (the cursor is 1 KB
	   in size, so leave room for it)...we pick 4 KB  and 0 KB,
	   respectively, since they begin at the top of the framebuffer
	   for minimal fragmentation of the address space, and this will
	   always leave enough room for the cursor for all implementations...
	*/

	/* Set the video base address... */	
	tga_set_win_org(sc->adp, 4 KB);

	/* Set the cursor base address... */	
	ccbr = READ_GFB_REGISTER(sc->adp, TGA_REG_CCBR);
	ccbr = (ccbr & 0xfffffc0f) | (0 << 4);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_CCBR, ccbr);

	/* Type the card... */
	if(adp->va_type == KD_TGA) {
		if(!deep) {

			/* 8bpp frame buffer */
			gfbc->ramdac_name = "BT485";
			gfbc->ramdac_init = bt485_init;
			gfbc->ramdac_rd = tga_bt485_rd;
			gfbc->ramdac_wr = tga_bt485_wr;
			gfbc->ramdac_save_palette = bt485_save_palette;
			gfbc->ramdac_load_palette = bt485_load_palette;
			gfbc->ramdac_save_cursor_palette =
			    bt485_save_cursor_palette;
			gfbc->ramdac_load_cursor_palette =
			    bt485_load_cursor_palette;
			gfbc->ramdac_read_hw_cursor = bt485_read_hw_cursor;
			gfbc->ramdac_set_hw_cursor = bt485_set_hw_cursor;
			gfbc->ramdac_set_hw_cursor_shape =
			    bt485_set_hw_cursor_shape;

			if(addrmask == GDER_ADDR_4MB) {

				/* 4MB core map; T8-01 or T8-02 */
				if(!cs) {
					card_type = TGA_TYPE_T8_01;
					gfbc->name = "T8-01";
				} else {
					card_type = TGA_TYPE_T8_02;
					gfbc->name = "T8-02";
				}
			} else if(addrmask == GDER_ADDR_8MB) {

				/* 8MB core map; T8-22 */
				if(cs) {/* sanity */
					card_type = TGA_TYPE_T8_22;
					gfbc->name = "T8-22";
				}
			} else if(addrmask == GDER_ADDR_16MB) {

				/* 16MB core map; T8-44 */
				if(cs) {/* sanity */
					card_type = TGA_TYPE_T8_44;
					gfbc->name = "T8-44";
				}
			} else if(addrmask == GDER_ADDR_32MB) {

				/* 32MB core map; ??? */
				card_type = TGA_TYPE_UNKNOWN;
			}
		} else {

			/* 32bpp frame buffer */
			gfbc->ramdac_name = "BT463";
			gfbc->ramdac_init = bt463_init;
			gfbc->ramdac_rd = tga_bt463_rd;
			gfbc->ramdac_wr = tga_bt463_wr;
			gfbc->builtin_save_palette = tga_builtin_save_palette;
			gfbc->builtin_load_palette = tga_builtin_load_palette;
			gfbc->ramdac_save_cursor_palette =
			    bt463_save_cursor_palette;
			gfbc->ramdac_load_cursor_palette =
			    bt463_load_cursor_palette;
			gfbc->builtin_read_hw_cursor =
			    tga_builtin_read_hw_cursor;
			gfbc->builtin_set_hw_cursor = tga_builtin_set_hw_cursor;
			gfbc->builtin_set_hw_cursor_shape =
			    tga_builtin_set_hw_cursor_shape;

			/* 32bpp frame buffer */
			if(addrmask == GDER_ADDR_4MB) {

				/* 4MB core map; ??? */
				card_type = TGA_TYPE_UNKNOWN;
			} else if(addrmask == GDER_ADDR_8MB) {

				/* 8MB core map; ??? */
				card_type = TGA_TYPE_UNKNOWN;
			} else if(addrmask == GDER_ADDR_16MB) {

				/* 16MB core map; T32-04 or T32-08 */
				if(!cs) {
					card_type = TGA_TYPE_T32_04;
					gfbc->name = "T32-04";
				} else {
					card_type = TGA_TYPE_T32_08;
					gfbc->name = "T32-08";
				}
			} else if(addrmask == GDER_ADDR_32MB) {

				/* 32MB core map; T32-88 */
				if(cs) {/* sanity */
					card_type = TGA_TYPE_T32_88;
					gfbc->name = "T32-88";
				}
			}
		}
	}
	else if(adp->va_type == KD_TGA2) {
		gfbc->ramdac_name = "IBM561";
		gfbc->ramdac_init = ibm561_init;
		gfbc->ramdac_rd = tga2_ibm561_rd;
		gfbc->ramdac_wr = tga2_ibm561_wr;
		gfbc->ramdac_save_palette = ibm561_save_palette;
		gfbc->ramdac_load_palette = ibm561_load_palette;
		gfbc->ramdac_save_cursor_palette = ibm561_save_cursor_palette;
		gfbc->ramdac_load_cursor_palette = ibm561_load_cursor_palette;
		gfbc->builtin_read_hw_cursor = tga_builtin_read_hw_cursor;
		gfbc->builtin_set_hw_cursor = tga_builtin_set_hw_cursor;
		gfbc->builtin_set_hw_cursor_shape =
		    tga_builtin_set_hw_cursor_shape;

		/* 4MB core map */
		if(addrmask == GDER_ADDR_4MB)
			card_type = TGA_TYPE_UNKNOWN;

		/* 8MB core map */
		else if(addrmask == GDER_ADDR_8MB) {
			card_type = TGA2_TYPE_3D30;
			gfbc->name = "3D30";
		}

		/* 16MB core map */
		else if(addrmask == GDER_ADDR_16MB) {
			card_type = TGA2_TYPE_4D20;
			gfbc->name = "4D20";
		}
		else if(addrmask == GDER_ADDR_32MB)
			card_type = TGA_TYPE_UNKNOWN;
	}

	/*
	  For now, just return for TGA2 cards (i.e.,
	  allow syscons to treat this device as a normal
	  VGA device, and don't do anything TGA2-specific,
	  e.g., only use the TGA2 card in VGA mode for now
	  as opposed to 2DA mode...
	*/
	if(adp->va_type == KD_TGA2)
		return(error);

	/* If we couldn't identify the card, err-out... */
	if(card_type == TGA_TYPE_UNKNOWN) {
		printf("tga%d: Unknown TGA type\n", unit);
		error = ENODEV;
		goto done;
	} 

	/* Clear and disable interrupts... */
	WRITE_GFB_REGISTER(adp, TGA_REG_SISR, 0x00000001);

	/* Perform TGA2-specific initialization, if necessary... */
	if(adp->va_type == KD_TGA2) {
		monitor = (~READ_GFB_REGISTER(adp, TGA_REG_GREV) >> 16 ) & 0x0f;
		tga2_init(sc, monitor);
	}
done:
	return(error);
}

static void
tga2_init(sc, monitor)
	struct gfb_softc *sc;
	int monitor;
{
	return;
	tga2_ics9110_wr(sc, decmonitors[monitor].dotclock);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_VHCR,
	    ((decmonitors[monitor].hbp / 4) << VHCR_BPORCH_SHIFT) |
	    ((decmonitors[monitor].hsync / 4) << VHCR_HSYNC_SHIFT) |
	    (((decmonitors[monitor].hfp) / 4) << VHCR_FPORCH_SHIFT) |
	    ((decmonitors[monitor].cols) / 4));
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_VVCR,
	    (decmonitors[monitor].vbp << VVCR_BPORCH_SHIFT) |
	    (decmonitors[monitor].vsync << VVCR_VSYNC_SHIFT) |
	    (decmonitors[monitor].vfp << VVCR_FPORCH_SHIFT) |
	    (decmonitors[monitor].rows));
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_VVBR, 1);
	GFB_REGISTER_READWRITE_BARRIER(sc, TGA_REG_VHCR, 3);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_VVVR,
	    READ_GFB_REGISTER(sc->adp, TGA_REG_VVVR) | 1);
	GFB_REGISTER_READWRITE_BARRIER(sc, TGA_REG_VVVR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_GPMR, 0xffffffff);
	GFB_REGISTER_READWRITE_BARRIER(sc, TGA_REG_GPMR, 1);
}

static int
tga_query_mode(video_adapter_t *adp, video_info_t *info)
{
	int error;

	/* Assume the best... */
	error = 0;

	/* Verify that this mode is supported on this adapter... */
	if(adp->va_type == KD_TGA2) {
		if((info->vi_mode != TGA2_2DA_MODE) &&
		    (info->vi_mode != TGA2_VGA_MODE))
			error = ENODEV;
	}
	else {
		if(info->vi_mode != 0)
			error = ENODEV;
	}
	return(error);
}

static int
tga_set_mode(video_adapter_t *adp, int mode)
{
	int error;
	gfb_reg_t gder;
	gfb_reg_t vgae_mask;

	/* Assume the best... */
	error = 0;
	
	gder = READ_GFB_REGISTER(adp, TGA_REG_GDER);

	/*
	   Determine the adapter type first
	   so we know which modes are valid for it...
	*/
	switch(adp->va_type) {
	case KD_TGA2:

		/*
		   Verify that this mode is supported
		   on this adapter...
		*/
		switch(mode) {
		case TGA2_2DA_MODE:
			vgae_mask = ~0x00400000;
			WRITE_GFB_REGISTER(adp, TGA_REG_GDER,
			    gder & vgae_mask);
			adp->va_mode = mode;
			break;
		case TGA2_VGA_MODE:
			vgae_mask = 0x00400000;
			WRITE_GFB_REGISTER(adp, TGA_REG_GDER,
			    gder | vgae_mask);
			adp->va_mode = mode;
			break;
		default:
			error = ENODEV;
		}
		break;
	case KD_TGA:

		/*
		   Verify that this mode is supported
		   on this adapter...
		*/
		switch(mode) {
		case 0:
			break;
		default:
			error = ENXIO;
		}
		break;
	default:
		error = ENODEV;
	}
	return(error);
}

static int
tga_blank_display(video_adapter_t *adp, int mode)
{
	gfb_reg_t blanked;
	int error;

	/* Assume the best... */
	error = 0;

	blanked = READ_GFB_REGISTER(adp, TGA_REG_VVVR) &
	    (VVR_BLANK | VVR_VIDEOVALID | VVR_CURSOR);

	/* If we're not already blanked, then blank...*/
	switch(mode) {
	case V_DISPLAY_BLANK:
		if(blanked != (VVR_VIDEOVALID | VVR_BLANK)) {
			blanked = VVR_VIDEOVALID | VVR_BLANK;
			WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, blanked);
		}
		break;
	case V_DISPLAY_STAND_BY:
		if(blanked != VVR_BLANK) {
			blanked = VVR_BLANK;
			WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, blanked);
		}
		break;
	case V_DISPLAY_ON:
		if(blanked != (VVR_VIDEOVALID | VVR_CURSOR)) {
			blanked = VVR_VIDEOVALID | VVR_CURSOR;
			WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, blanked);
		}
		break;
	default:
		break;
	}
	return(0);
}

#if 0

static int
tga_ioctl(video_adapter_t *adp, u_long cmd, caddr_t arg)
{
	struct gfb_softc *sc;
	int error;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];
	switch (cmd) {
	case FBIOPUTCMAP:
#if 0
		tga_schedule_intr(sc, bt463_load_palette_intr);
		break;
#endif
	case FBIO_GETWINORG:
	case FBIO_SETWINORG:
	case FBIO_SETDISPSTART:
	case FBIO_SETLINEWIDTH:
	case FBIO_GETPALETTE:
	case FBIOGTYPE:
	case FBIOGETCMAP:
	default:
		error = fb_commonioctl(adp, cmd, arg); 
	}
	return(error);
}

#endif /* 0 */

static int
tga_set_border(video_adapter_t *adp, int color) {
	return(ENODEV);
}

static int
tga_set_win_org(video_adapter_t *adp, off_t offset) {
	gfb_reg_t vvbr;
	u_int16_t window_orig;
	int gder;
	int deep;
	int cs;

	/* Get the adapter's parameters... */
	gder = BASIC_READ_TGA_REGISTER(adp, TGA_REG_GDER);
	deep = (gder & 0x1) != 0;
	cs = (gder & 0x200) == 0;

	/*
	   Set the window (framebuffer) origin according to the video
	   base address...
	*/
	window_orig = offset / ((1 + cs) * (1 + deep) * (1 + deep) * 2 KB);
	adp->va_window_orig = window_orig * ((1 + cs) * (1 + deep) *
	    (1 + deep) * 2 KB);

	/* Set the video base address... */
	vvbr = READ_GFB_REGISTER(adp, TGA_REG_VVBR);
	vvbr = (vvbr & 0xfffffe00) | window_orig;
	WRITE_GFB_REGISTER(adp, TGA_REG_VVBR, vvbr);
	return(0);
}

static int
tga_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy) {
	int off;
	gfb_reg_t gpxr;
	gfb_reg_t gmor;
	gfb_reg_t gbcr0;
	gfb_reg_t gbcr1;
	gfb_reg_t color;

	/* Save the pixel mode... */
	gmor = READ_GFB_REGISTER(adp, TGA_REG_GMOR);

	/* Save the pixel mask... */
	gpxr = READ_GFB_REGISTER(adp, TGA_REG_GPXR_P);

	/* Save the block-color... */
	gbcr0 = READ_GFB_REGISTER(adp, TGA_REG_GBCR0);
	gbcr1 = READ_GFB_REGISTER(adp, TGA_REG_GBCR1);

	/* Set the pixel mode (block-fill)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR,
			   (gmor & ~GMOR_MODE_MASK) | GMOR_MODE_BLK_FILL);

	/* Set the pixel mask (enable writes to all pixels)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, 0xffffffff);

	color = ((val & 0xff00) << 24) || ((val & 0xff00) << 16) ||
	    ((val & 0xff00) << 8) || ((val & 0xff00) << 0);

	/* Set the color for the block-fill... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GBCR0, color);
	WRITE_GFB_REGISTER(adp, TGA_REG_GBCR1, color);

	/*
	   Just traverse the buffer, one 2K-pixel span at a time, setting
	   each pixel to the bolck-color...
	*/
	for(off = (x * y); off < ((x + cx) * (y + cy)); off += (2 KB))
		WRITE_GFB_BUFFER(adp, off >> 2L, 0x000007ff);

	/* Restore the pixel mode... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, gpxr);

	/* Restore the pixel mask... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, gmor);

	/* Restore the block-color... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GBCR0, gbcr0);
	WRITE_GFB_REGISTER(adp, TGA_REG_GBCR1, gbcr1);

	return(0);
}

static int
tga_bitblt(video_adapter_t *adp, ...) {
	va_list args;
	int i, count;
	gfb_reg_t gmor;
	gfb_reg_t gopr;
	vm_offset_t src, dst;

	va_start(args, adp);

	/* Save the pixel mode... */
	gmor = READ_GFB_REGISTER(adp, TGA_REG_GMOR);

	/* Save the raster op... */
	gopr = READ_GFB_REGISTER(adp, TGA_REG_GOPR);

	/* Set the pixel mode (copy)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR,
	    (gmor & ~GMOR_MODE_MASK) | GMOR_MODE_COPY);

	/* Set the raster op (src)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, (gopr & 0xfffffff0) | 0x3);

	src = (va_arg(args, vm_offset_t) + adp->va_window_orig) &
	    0x0000000000fffff8;
	dst = (va_arg(args, vm_offset_t) + adp->va_window_orig) &
	    0x0000000000fffff8;
	count = va_arg(args, int);
	for(i = 0; i < count; i+= 64, src += 64, dst += 64) {
		WRITE_GFB_REGISTER(adp, TGA_REG_GCSR, src);
		WRITE_GFB_REGISTER(adp, TGA_REG_GCDR, dst);
	}

	/* Restore the raster op... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, gopr);

	/* Restore the pixel mode... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, gmor);

	va_end(args);
	return(0);
}

static int
#if 0
tga_clear(video_adapter_t *adp, int n)
#else
tga_clear(video_adapter_t *adp)
#endif
{
	int off;
	gfb_reg_t gpxr;
	gfb_reg_t gmor;
	gfb_reg_t gopr;

#if 0
	if(n == 0) return(0);
#endif

	/* Save the pixel mode... */
	gmor = READ_GFB_REGISTER(adp, TGA_REG_GMOR);

	/* Save the pixel mask... */
	gpxr = READ_GFB_REGISTER(adp, TGA_REG_GPXR_P);

	/* Save the raster op... */
	gopr = READ_GFB_REGISTER(adp, TGA_REG_GOPR);

	/* Set the pixel mode (opaque-fill)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR,
			   (gmor & ~GMOR_MODE_MASK) | GMOR_MODE_OPQ_FILL);

	/* Set the pixel mask (enable writes to all pixels)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, 0xffffffff);

	/* Set the raster op (clear)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, (gopr & 0xfffffff0) | 0x00);

	/*
	   Just traverse the buffer, one 2K-pixel span at a time, clearing
	   each pixel...
	*/
#if 0
	for(off = 0; off < (n * adp->va_line_width); off += (2 KB))
#endif
	for(off = 0; off < adp->va_window_size; off += (2 KB))
		WRITE_GFB_BUFFER(adp, off >> 2L, 0x000007ff);

	/* Restore the pixel mask... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, gpxr);

	/* Restore the raster op... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, gopr);

	/* Restore the pixel mode... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, gmor);

	return(0);
}

int
tga_putc(video_adapter_t *adp, vm_offset_t off, u_int8_t c, u_int8_t a)
{
	int i;
	gfb_reg_t gpxr;
	gfb_reg_t gmor;
	gfb_reg_t gopr;
	gfb_reg_t gbgr;
	gfb_reg_t gfgr;
	gfb_reg_t mask;
	int row, col;
	u_int8_t *pixel;
	vm_offset_t poff;
	struct gfb_softc *sc;
	int pixel_size;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];
	pixel_size = adp->va_info.vi_depth / 8;

	/* Save the pixel mode... */
	gmor = READ_GFB_REGISTER(adp, TGA_REG_GMOR);

	/* Save the pixel mask... */
	gpxr = READ_GFB_REGISTER(adp, TGA_REG_GPXR_P);

	/* Save the raster op... */
	gopr = READ_GFB_REGISTER(adp, TGA_REG_GOPR);

	/* Save the background color... */
	gbgr = READ_GFB_REGISTER(adp, TGA_REG_GBGR);

	/* Save the foreground color... */
	gfgr = READ_GFB_REGISTER(adp, TGA_REG_GFGR);

	/* Set the pixel mode (opaque-stipple)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR,
	    (gmor & ~GMOR_MODE_MASK) | GMOR_MODE_OPQ_STPL);

	/* Set the pixel mask (enable writes to the first cwidth pixels)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P,
	    (1 << adp->va_info.vi_cwidth) - 1);

	/* Set the raster op (src)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, (gopr & 0xfffffff0) | 0x3);

	/* Set the foreground color mask from the attribute byte... */
	mask = (a & 0x80) ? a : (a & 0x0f);

	/* Propagate the 8-bit mask across the full 32 bits... */
	mask |= (mask << 24) | (mask << 16) | (mask << 8);

	/* Set the foreground color... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GFGR, mask);

	/* Set the background color mask from the attribute byte... */
	mask = (a >> 4) & 0x07;

	/* Propagate the 8-bit mask across the full 32 bits... */
	mask |= (mask << 24) | (mask << 16) | (mask << 8);

	/* Set the background color... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GBGR, mask);

	/* Get the start of the array of pixels rows for this character... */
	pixel = sc->gfbc->font + (c * adp->va_info.vi_cheight);

	/* Calculate the new cursor position... */
	row = off / adp->va_info.vi_width;
	col = off % adp->va_info.vi_width;

	/* Iterate over all the pixel rows for this character... */
	for(i = 0; i < adp->va_info.vi_cheight; i++) {

		/* Get the address of the character's pixel-row... */
		poff = ((col * adp->va_info.vi_cwidth * pixel_size) +
		    (((row * adp->va_info.vi_cheight) + i) *
		    adp->va_line_width)) / sizeof(gfb_reg_t);

		/* Now display the current pixel row... */
		WRITE_GFB_BUFFER(adp, poff, pixel[i]);
	}

	/* Restore the foreground color... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GFGR, gfgr);

	/* Restore the background color... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GBGR, gbgr);

	/* Restore the pixel mode... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, gpxr);

	/* Restore the pixel mask... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, gmor);

	/* Restore the raster op... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, gopr);

	return(0);
}

int
tga_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i, j, k;
	gfb_reg_t gpxr;
	gfb_reg_t gmor;
	gfb_reg_t gopr;
	gfb_reg_t row, col;
	u_int8_t *pixel;
	u_int8_t c;
	u_int8_t a;
	gfb_reg_t p;
	vm_offset_t poff;
	struct gfb_softc *sc;
	int pixel_size;

	sc = gfb_device_softcs[adp->va_model][adp->va_unit];
	pixel_size = adp->va_info.vi_depth / 8;

	/* If the string in empty, just return now... */
	if(len == 0) return(0);

	for(i = 0; i < len; i++)
		tga_putc(adp, off + i, s[i] & 0x00ff, (s[i] & 0xff00) >> 8);
	return(0);

	/* Save the pixel mode... */
	gmor = READ_GFB_REGISTER(adp, TGA_REG_GMOR);

	/* Save the pixel mask... */
	gpxr = READ_GFB_REGISTER(adp, TGA_REG_GPXR_P);

	/* Save the raster op... */
	gopr = READ_GFB_REGISTER(adp, TGA_REG_GOPR);

	/* Set the pixel mode (simple)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, (gmor & 0xffffffc0) | 0x00);

	/* Set the pixel mask (enable writes to all 32 pixels)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, (gpxr & 0xfffffff0) | 0xf);

	/* Set the raster op (src)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, (gopr & 0xfffffff0) | 0x03);

	/*
	   First, do as many characters-rows at a time as possible (as exist)...
	*/
	for(i = 0; (len - i) > adp->va_info.vi_width;
	    i += adp->va_info.vi_width) {

		/*
		   Iterate over all the pixels for each character in the
		   character-row, doing a scan-line at-a-time, rather than
		   a character at-a-time (like tga_putc())...
		*/
		for(j = 0; j < adp->va_info.vi_cheight; j++) {
			p = 0;
			for(k = 0; k < adp->va_info.vi_width; k++) {

				/*
				   Get this character...
				*/
				c = s[i + k] & 0x00ff;

				/*
				   Get the attribute for this character...
				*/
				a = (s[i + k] & 0xff00) >> 8;

				/*
				   Get the start of the array of pixels rows for
				   this character...
				*/
				pixel = sc->gfbc->font +
					(c * adp->va_info.vi_cheight);

				/* Shift the other pre-existing pixel rows... */
				p <<= 8;

				/*
				   Get the first pixel row for
				   this character...
				*/
				p |= pixel[j];

				if (((k + 1) % sizeof(gfb_reg_t)) == 0) {

					/*
					   Calculate the new cursor
					   position...
					*/
					row = (off + i + (k -
					    (sizeof(gfb_reg_t) - 1))) /
					    adp->va_info.vi_width;
					col = (off + i + (k -
					    (sizeof(gfb_reg_t) - 1))) %
					    adp->va_info.vi_width;

					/*
					   Get the address of the current
					   character's pixel-row...
					*/
					poff = ((col * adp->va_info.vi_cwidth * 
					    pixel_size) + (((row *
					    adp->va_info.vi_cheight) + j) *
				 	    adp->va_line_width)) /
					    sizeof(gfb_reg_t);

					/*
					   Now display the current
					   pixel row...
					*/
					(*vidsw[adp->va_index]->putp)(adp, poff,
					    p, a, sizeof(gfb_reg_t),
					    adp->va_info.vi_depth, 1, 0);

					/* Reset (clear) p... */
					p = 0;
				}
			}
		}
	}

	/*
	   Next, do as many character-sets at a time as possible (as exist)...
	*/
	for(; (len - i) > sizeof(gfb_reg_t); i += sizeof(gfb_reg_t)) {

		/*
		   Iterate over all the pixels for each character in the
		   character-row, doing a scan-line at-a-time, rather than
		   a character at-a-time (like tga_putc())...
		*/
		for(j = 0; j < adp->va_info.vi_cheight; j++) {
			p = 0;
			for(k = 0; k < sizeof(gfb_reg_t); k++) {

				/*
				   Get this character...
				*/
				c = s[i + k] & 0x00ff;

				/*
				   Get the attribute for this character...
				*/
				a = (s[i + k] & 0xff00) >> 8;

				/*
				   Get the start of the array of pixels rows for
				   this character...
				*/
				pixel = sc->gfbc->font +
				    (c * adp->va_info.vi_cheight);

				/* Shift the other pre-existing pixel rows... */
				p <<= 8;

				/*
				   Get the first pixel row for
				   this character...
				*/
				p |= pixel[j];

				if (((k + 1) % sizeof(gfb_reg_t)) == 0) {

					/*
					   Calculate the new cursor
					   position...
					*/
					row = (off + i) / adp->va_info.vi_width;
					col = (off + i) % adp->va_info.vi_width;

					/*
					   Get the address of the current
					   character's pixel-row...
					*/
					poff = ((col * adp->va_info.vi_cwidth * 
					    pixel_size) + (((row *
					    adp->va_info.vi_cheight) + j) *
					    adp->va_line_width)) /
					    sizeof(gfb_reg_t);

					/*
					   Now display the current
					   pixel row...
					*/
					(*vidsw[adp->va_index]->putp)(adp, poff,
					    p, a, sizeof(gfb_reg_t),
					    adp->va_info.vi_depth, 1, 0);

					/* Reset (clear) p... */
					p = 0;
				}
			}
		}
	}

	/* Restore the pixel mode... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, gpxr);

	/* Restore the pixel mask... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, gmor);

	/* Restore the raster op... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, gopr);

	/* Finally, do the remaining characters a character at-a-time... */
	for(; i < len; i++) {
		/*
		   Get this character...
		*/
		c = s[i] & 0x00ff;

		/*
		   Get the attribute for this character...
		*/
		a = (s[i] & 0xff00) >> 8;

		/*
		   Display this character...
		*/
		tga_putc(adp, off + i, c, a);
	}
	return(0);
}

int
tga_putm(video_adapter_t *adp, int x, int y, u_int8_t *pixel_image,
	    gfb_reg_t pixel_mask, int size)
{
	gfb_reg_t gpxr;
	gfb_reg_t gmor;
	gfb_reg_t gopr;
	int i, pixel_size;
	vm_offset_t poff;

	pixel_size = adp->va_info.vi_depth / 8;

	/* Save the pixel mode... */
	gmor = READ_GFB_REGISTER(adp, TGA_REG_GMOR);

	/* Save the pixel mask... */
	gpxr = READ_GFB_REGISTER(adp, TGA_REG_GPXR_P);

	/* Save the raster op... */
	gopr = READ_GFB_REGISTER(adp, TGA_REG_GOPR);

	/* Set the pixel mode (simple)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR,
	    (gmor & ~GMOR_MODE_MASK) | GMOR_MODE_SIMPLE);

	/* Set the pixel mask (enable writes to the first 8 pixels)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, (gpxr & 0xfffffff0) | 0xf);

	/* Set the raster op (src)... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, (gopr & 0xfffffff0) | 0x3);

	/* Iterate over all the pixel rows for the mouse pointer... */
	for(i = 0; i < size; i++) {

		/* Get the address of the mouse pointer's pixel-row... */
		poff = ((x * pixel_size) + ((y + i) * adp->va_line_width)) /
		    sizeof(gfb_reg_t);

		/* Now display the current pixel-row... */
		(*vidsw[adp->va_index]->putp)(adp, poff, pixel_image[i],
		    pixel_mask, sizeof(u_int8_t), adp->va_info.vi_depth, 1, 0);
	}

	/* Restore the pixel mode... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GPXR_P, gpxr);

	/* Restore the pixel mask... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GMOR, gmor);

	/* Restore the raster op... */
	WRITE_GFB_REGISTER(adp, TGA_REG_GOPR, gopr);

	return(0);
}

static int
tga_error(void)
{
	return(0);
}

/*****************************************************************************
 *
 * TGA-specific functions
 *
 ****************************************************************************/

static int
tga_builtin_save_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	int i;
	int error;
	struct gfb_softc *sc;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/*
	 * We store 8 bit values in the palette buffer, while the standard
	 * VGA has 6 bit DAC .
	 */
	outb(PALRADR, 0x00);
	for(i = 0; i < palette->count; ++i) {
		palette->red[i] = inb(PALDATA) << 2;
		palette->green[i] = inb(PALDATA) << 2;
		palette->blue[i] = inb(PALDATA) << 2;
	}
	return(error);
}

static int
tga_builtin_load_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	int i;
	int error;
	struct gfb_softc *sc;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/*
	 * We store 8 bit values in the palette buffer, while the standard
	 * VGA has 6 bit DAC .
	*/
	outb(PIXMASK, 0xff);
	outb(PALWADR, 0x00);
	for(i = 0; i < palette->count; ++i) {
		outb(PALDATA, palette->red[i] >> 2);
		outb(PALDATA, palette->green[i] >> 2);
		outb(PALDATA, palette->blue[i] >> 2);
	}
	return(error);
}

#ifdef TGA2
static int
tga2_builtin_save_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	int i;
	int error;
	struct gfb_softc *sc;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CPALETTE_RAM & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CPALETTE_RAM >> 8) & 0xff);

	/* spit out the colormap data */
	for(i = 0; i < palette->count; i++) {
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA,
		    palette->red[i]);
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, 
		    palette->green[i]);
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, 
		    palette->blue[i]);
	}
	return(error);
}

static int
tga2_builtin_load_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	int i;
	int error;
	struct gfb_softc *sc;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CPALETTE_RAM & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CPALETTE_RAM >> 8) & 0xff);

	/* spit out the colormap data */
	for(i = 0; i < palette->count; i++) {
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA,
		    palette->red[i]);
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, 
		    palette->green[i]);
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, 
		    palette->blue[i]);
	}
	return(error);
}

static int
tga2_builtin_save_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	int i;
	int error;
	struct gfb_softc *sc;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CURSOR_COLOR_0 & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CURSOR_COLOR_0 >> 8) & 0xff);

	/* spit out the cursor data */
	for(i = 0; i < palette->count; i++) {
		BTWNREG(sc, palette->red[i]);
		BTWNREG(sc, palette->green[i]);
		BTWNREG(sc, palette->blue[i]);
	}
	return(error);
}

static int
tga2_builtin_load_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	int i;
	int error;
	struct gfb_softc *sc;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CURSOR_COLOR_0 & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CURSOR_COLOR_0 >> 8) & 0xff);

	/* spit out the cursor data */
	for(i = 0; i < palette->count; i++) {
		BTWNREG(sc, palette->red[i]);
		BTWNREG(sc, palette->green[i]);
		BTWNREG(sc, palette->blue[i]);
	}
	return(error);
}

#endif /* TGA2 */

static int
tga_builtin_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	gfb_reg_t cxyr;
	int error;

	/* Assume the best... */
	error = 0;

	cxyr = READ_GFB_REGISTER(adp, TGA_REG_CXYR) | 0x00ffffff;
	*col = (cxyr & 0x00000fff) / adp->va_info.vi_cwidth;
	*row = ((cxyr & 0x00fff000) >> 12) / adp->va_info.vi_cheight;
	return(error);
}

static int
tga_builtin_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	int error;
	gfb_reg_t cxyr;
	gfb_reg_t vvvr;

	/* Assume the best... */
	error = 0;

	vvvr = READ_GFB_REGISTER(adp, TGA_REG_VVVR);

	/*
	   Make sure the parameters are in range for the screen
	   size...
	*/
	if((row > adp->va_info.vi_height) ||
	    (col > adp->va_info.vi_width))
		error = EINVAL;
	else if(((row * adp->va_info.vi_cheight) > 0x0fff) ||
	    ((col * adp->va_info.vi_cwidth) > 0x0fff))
		error = EINVAL;
	/*
	   If either of the parameters is less than 0,
	   then hide the cursor...
	*/
	else if((row < 0) || (col < 0)) {
		if((vvvr & 0x00000004) != 0) {
			vvvr &= 0xfffffffb;
			WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, vvvr);
		}
	}
		
	/* Otherwise, just move the cursor as requested... */
	else {
		cxyr = READ_GFB_REGISTER(adp, TGA_REG_CXYR) & 0xff000000;
		cxyr |= ((row * adp->va_info.vi_cheight) << 12);
		cxyr |= (col * adp->va_info.vi_cwidth);
		WRITE_GFB_REGISTER(adp, TGA_REG_CXYR, cxyr);
		if((vvvr & 0x00000004) == 0) {
			vvvr |= 0x00000004;
		WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, vvvr);
		}
	}
	return(error);
}

static int
tga_builtin_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
	    int cellsize, int blink)
{
	int i, j;
	vm_offset_t cba;
	gfb_reg_t window_orig;
	gfb_reg_t ccbr;
	gfb_reg_t vvvr;
	int error;

	/* Assume the best... */
	error = 0;

	vvvr = READ_GFB_REGISTER(adp, TGA_REG_VVVR);

	/*
	   Make sure the parameters are in range for the cursor
	   (it's a 64x64 cursor)...
	*/
	if(height > 64)
		error = EINVAL;

	/* If height is less than or equal to 0, then hide the cursor... */
	else if(height <= 0) {
		if((vvvr & 0x00000004) != 0) {
			vvvr &= 0xfffffffb;
			WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, vvvr);
		}
	}

	/* Otherwise, just resize the cursor as requested... */
	else {
		ccbr = READ_GFB_REGISTER(adp, TGA_REG_CCBR);
		ccbr &= 0xffff03ff;
		ccbr |= ((height - 1) << 10);
		WRITE_GFB_REGISTER(adp, TGA_REG_CCBR, ccbr);
		if((vvvr & 0x00000004) == 0) {
			vvvr |= 0x00000004;
			WRITE_GFB_REGISTER(adp, TGA_REG_VVVR, vvvr);
		}

		/* Save the window origin... */
		window_orig = adp->va_window_orig;

		/*
		   Fill in the cursor image (64 rows of 64 pixels per cursor
		   row at 2 bits-per-pixel, so 64 rows of 16 bytes each)--we
		   set va_window_orig to the cursor base address temporarily
		   so that we can write to the cursor image...
		*/
		cba = (READ_GFB_REGISTER(adp, TGA_REG_CCBR) & 0xfffffc0f) >> 4;
		adp->va_window_orig = cba;
		for(i = 0; i < (64 - height); i++) {
			WRITE_GFB_BUFFER(adp, cba++, 0x00000000);
			WRITE_GFB_BUFFER(adp, cba++, 0x00000000);
		}
		for(; i < 64; i++) {
			for(j = 0; j < (((64 - cellsize) / 8) /
			    sizeof(gfb_reg_t)); j++)
				WRITE_GFB_BUFFER(adp, cba++, 0x00000000);
			for(; j < ((64 / 8) / sizeof(gfb_reg_t)); j++)
				WRITE_GFB_BUFFER(adp, cba++, 0xffffffff);
		}

		/* Restore the window origin... */
		adp->va_window_orig = window_orig;

	}
	return(error);
}

static void
bt463_load_palette_intr(struct gfb_softc *sc)
{
	sc->gfbc->ramdac_save_palette(sc->adp, &sc->gfbc->palette);
}

static void
bt463_load_cursor_palette_intr(struct gfb_softc *sc)
{
	sc->gfbc->ramdac_load_cursor_palette(sc->adp, &sc->gfbc->cursor_palette);
}

static int
tga_schedule_intr(struct gfb_softc *sc, void (*f)(struct gfb_softc *))
{
	/* Busy-wait for the previous interrupt to complete... */
	while((READ_GFB_REGISTER(sc->adp, TGA_REG_SISR) & 0x00000001) != 0);

	/* Arrange for f to be called at the next end-of-frame interrupt... */
	sc->gfbc->ramdac_intr = f;

	/* Enable the interrupt... */
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_SISR, 0x00010000);
	return(0);
}

static u_int8_t
tga_bt485_rd(struct gfb_softc *sc, u_int btreg)
{
	gfb_reg_t rdval;

	if(btreg > BT485_REG_MAX)
		panic("tga_ramdac_rd: reg %d out of range\n", btreg);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPSR, (btreg << 1) | 0x1);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPSR, 1);
	rdval = READ_GFB_REGISTER(sc->adp, TGA_REG_EPDR);
	return((rdval >> 16) & 0xff);
}

static void
tga_bt485_wr(struct gfb_softc *sc, u_int btreg, u_int8_t val)
{
	if(btreg > BT485_REG_MAX)
		panic("tga_ramdac_wr: reg %d out of range\n", btreg);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPDR,
	    (btreg << 9) | (0 << 8 ) | val);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPDR, 1);
}

static u_int8_t
tga2_ibm561_rd(struct gfb_softc *sc, u_int btreg)
{
	bus_space_handle_t ramdac;
	u_int8_t retval;

	if(btreg > BT485_REG_MAX)
		panic("tga_ramdac_rd: reg %d out of range\n", btreg);
	ramdac = sc->bhandle + TGA2_MEM_RAMDAC + (0xe << 12) + (btreg << 8);
	retval = bus_space_read_4(sc->btag, ramdac, 0) & 0xff;
	bus_space_barrier(sc->btag, ramdac, 0, 4, BUS_SPACE_BARRIER_READ);
	return(retval);
}

static void
tga2_ibm561_wr(struct gfb_softc *sc, u_int btreg, u_int8_t val)
{
	bus_space_handle_t ramdac;

	if(btreg > BT485_REG_MAX)
		panic("tga_ramdac_wr: reg %d out of range\n", btreg);
	ramdac = sc->bhandle + TGA2_MEM_RAMDAC + (0xe << 12) + (btreg << 8);
	bus_space_write_4(sc->btag, ramdac, 0, val & 0xff);
	bus_space_barrier(sc->btag, ramdac, 0, 4, BUS_SPACE_BARRIER_WRITE);
}

static u_int8_t
tga_bt463_rd(struct gfb_softc *sc, u_int btreg)
{
	gfb_reg_t rdval;

	/* 
	 * Strobe CE# (high->low->high) since status and data are latched on 
	 * the falling and rising edges (repsectively) of this active-low
	 * signal.
	 */
	
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPSR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPSR, (btreg << 2) | 2 | 1);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPSR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPSR, (btreg << 2) | 2 | 0);
	GFB_REGISTER_READ_BARRIER(sc, TGA_REG_EPSR, 1);
	rdval = READ_GFB_REGISTER(sc->adp, TGA_REG_EPDR);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPSR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPSR, (btreg << 2) | 2 | 1);
	return((rdval >> 16) & 0xff);
}

static void
tga_bt463_wr(struct gfb_softc *sc, u_int btreg, u_int8_t val)
{
	/* 
	 * In spite of the 21030 documentation, to set the MPU bus bits for
	 * a write, you set them in the upper bits of EPDR, not EPSR.
	 */
	
	/* 
	 * Strobe CE# (high->low->high) since status and data are latched on
	 * the falling and rising edges of this active-low signal.
	 */

	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPDR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPDR, (btreg << 10) | 0x100 | val);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPDR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPDR, (btreg << 10) | 0x000 | val);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_EPDR, 1);
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_EPDR, (btreg << 10) | 0x100 | val);
}

static void
tga2_ics9110_wr(struct gfb_softc *sc, int dotclock)
{
	bus_space_handle_t clock;
	gfb_reg_t valU;
	int N, M, R, V, X;
	int i;

	switch(dotclock) {
	case 130808000:
		N = 0x40; M = 0x7; V = 0x0; X = 0x1; R = 0x1; break;
	case 119840000:
		N = 0x2d; M = 0x2b; V = 0x1; X = 0x1; R = 0x1; break;
	case 108180000:
		N = 0x11; M = 0x9; V = 0x1; X = 0x1; R = 0x2; break;
	case 103994000:
		N = 0x6d; M = 0xf; V = 0x0; X = 0x1; R = 0x1; break;
	case 175000000:
		N = 0x5F; M = 0x3E; V = 0x1; X = 0x1; R = 0x1; break;
	case  75000000:
		N = 0x6e; M = 0x15; V = 0x0; X = 0x1; R = 0x1; break;
	case  74000000:
		N = 0x2a; M = 0x41; V = 0x1; X = 0x1; R = 0x1; break;
	case  69000000:
		N = 0x35; M = 0xb; V = 0x0; X = 0x1; R = 0x1; break;
	case  65000000:
		N = 0x6d; M = 0x0c; V = 0x0; X = 0x1; R = 0x2; break;
	case  50000000:
		N = 0x37; M = 0x3f; V = 0x1; X = 0x1; R = 0x2; break;
	case  40000000:
		N = 0x5f; M = 0x11; V = 0x0; X = 0x1; R = 0x2; break;
	case  31500000:
		N = 0x16; M = 0x05; V = 0x0; X = 0x1; R = 0x2; break;
	case  25175000:
		N = 0x66; M = 0x1d; V = 0x0; X = 0x1; R = 0x2; break;
	case 135000000:
		N = 0x42; M = 0x07; V = 0x0; X = 0x1; R = 0x1; break;
	case 110000000:
		N = 0x60; M = 0x32; V = 0x1; X = 0x1; R = 0x2; break;
	case 202500000:
		N = 0x60; M = 0x32; V = 0x1; X = 0x1; R = 0x2; break;
	default:
		panic("unrecognized clock rate %d\n", dotclock);
	}

	/* XXX -- hard coded, bad */
	valU  = N | ( M << 7 ) | (V << 14);
	valU |= (X << 15) | (R << 17);
	valU |= 0x17 << 19;
	clock = sc->bhandle + TGA2_MEM_EXTDEV + TGA2_MEM_CLOCK + (0xe << 12);
	for(i = 24; i > 0; i--) {
		gfb_reg_t       writeval;
                
		writeval = valU & 0x1;
		if (i == 1)  
			writeval |= 0x2; 
		valU >>= 1;
		bus_space_write_4(sc->btag, clock, 0, writeval);
		bus_space_barrier(sc->btag, clock, 0, 4,
		    BUS_SPACE_BARRIER_WRITE);
        }       
	clock = sc->bhandle + TGA2_MEM_EXTDEV + TGA2_MEM_CLOCK + (0xe << 12) +
	    (0x1 << 11) + (0x1 << 11);
	bus_space_write_4(sc->btag, clock, 0, 0x0);
	bus_space_barrier(sc->btag, clock, 0, 0, BUS_SPACE_BARRIER_WRITE);
}

/*****************************************************************************
 *
 * BrookTree RAMDAC-specific functions
 *
 ****************************************************************************/

static void
bt463_init(struct gfb_softc *sc)
{
	int i;

	return;

	/*
	 * Init the BT463 for normal operation.
	 */

	/*
	 * Setup:
	 * reg 0: 4:1 multiplexing, 25/75 blink.
	 * reg 1: Overlay mapping: mapped to common palette, 
	 *        14 window type entries, 24-plane configuration mode,
	 *        4 overlay planes, underlays disabled, no cursor. 
	 * reg 2: sync-on-green enabled, pedestal enabled.
	 */

	BTWREG(sc, BT463_IREG_COMMAND_0, 0x40);
	BTWREG(sc, BT463_IREG_COMMAND_1, 0x48);
	BTWREG(sc, BT463_IREG_COMMAND_2, 0xC0);

	/*
	 * Initialize the read mask.
	 */
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_READ_MASK_P0_P7 & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_READ_MASK_P0_P7 >> 8) & 0xff);
	for(i = 0; i < 4; i++)
		BTWNREG(sc, 0xff);

	/*
	 * Initialize the blink mask.
	 */
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_READ_MASK_P0_P7 & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_READ_MASK_P0_P7 >> 8) & 0xff);
	for(i = 0; i < 4; i++)
		BTWNREG(sc, 0);

	/*
	 * Clear test register
	 */
	BTWREG(sc, BT463_IREG_TEST, 0);

	/*
	 * Initalize the RAMDAC info struct to hold all of our
	 * data, and fill it in.
	 */

	/* Initialize the window type table:
	 *
	 * Entry 0: 24-plane truecolor, overlays enabled, bypassed.
	 *
	 *  Lookup table bypass:      yes (    1 << 23 & 0x800000)  800000
	 *  Colormap address:       0x000 (0x000 << 17 & 0x7e0000)       0 
	 *  Overlay mask:             0xf (  0xf << 13 & 0x01e000)   1e000
	 *  Overlay location:    P<27:24> (    0 << 12 & 0x001000)       0
	 *  Display mode:       Truecolor (    0 <<  9 & 0x000e00)     000
	 *  Number of planes:           8 (    8 <<  5 & 0x0001e0)     100
	 *  Plane shift:                0 (    0 <<  0 & 0x00001f)       0
	 *                                                        --------
	 *                                                        0x81e100
	 */	  
#if 0
	data->window_type[0] = 0x81e100;
#endif

	/* Entry 1: 8-plane pseudocolor in the bottom 8 bits, 
	 *          overlays enabled, colormap starting at 0. 
	 *
	 *  Lookup table bypass:       no (    0 << 23 & 0x800000)       0
	 *  Colormap address:       0x000 (0x000 << 17 & 0x7e0000)       0 
	 *  Overlay mask:             0xf (  0xf << 13 & 0x01e000) 0x1e000
	 *  Overlay location:    P<27:24> (    0 << 12 & 0x001000)       0
	 *  Display mode:     Pseudocolor (    1 <<  9 & 0x000e00)   0x200
	 *  Number of planes:           8 (    8 <<  5 & 0x0001e0)   0x100
	 *  Plane shift:               16 ( 0x10 <<  0 & 0x00001f)      10
	 *                                                        --------
	 *                                                        0x01e310
	 */	  
#if 0
	data->window_type[1] = 0x01e310;
#endif
	/* The colormap interface to the world only supports one colormap, 
	 * so having an entry for the 'alternate' colormap in the bt463 
	 * probably isn't useful.
	 */

	/* Fill the remaining table entries with clones of entry 0 until we 
	 * figure out a better use for them.
	 */
#if 0
	for(i = 2; i < BT463_NWTYPE_ENTRIES; i++) {
		data->window_type[i] = 0x81e100;
	}
#endif

	tga_schedule_intr(sc, bt463_update_window_type);
	tga_schedule_intr(sc, bt463_load_cursor_palette_intr);
	tga_schedule_intr(sc, bt463_load_palette_intr);
}

static void
bt463_update_window_type(struct gfb_softc *sc)
{
	int i;

	/* The Bt463 won't accept window type data except during a blanking
	 * interval, so we do this early in the interrupt.
	 * Blanking the screen might also be a good idea, but it can cause 
	 * unpleasant flashing and is hard to do from this side of the
	 * ramdac interface.
	 */
	/* spit out the window type data */
	for(i = 0; i < BT463_NWTYPE_ENTRIES; i++) {
#if 0
		sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
		    (BT463_IREG_WINDOW_TYPE_TABLE + i) & 0xff);
		sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
		    ((BT463_IREG_WINDOW_TYPE_TABLE + i) >> 8) & 0xff);
		BTWNREG(sc, (data->window_type[i]) & 0xff);
		BTWNREG(sc, (data->window_type[i] >> 8) & 0xff);
		BTWNREG(sc, (data->window_type[i] >> 16) & 0xff);
#endif
	}
}

#if 0
static int
bt463_save_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CPALETTE_RAM & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CPALETTE_RAM >> 8) & 0xff);

	/* get the colormap data */
	for(i = 0; i < palette->count; i++) {
		palette->red[i] = sc->gfbc->ramdac_rd(sc, BT463_REG_CMAP_DATA);
		palette->green[i] = sc->gfbc->ramdac_rd(sc,
		    BT463_REG_CMAP_DATA);
		palette->blue[i] = sc->gfbc->ramdac_rd(sc, BT463_REG_CMAP_DATA);
	}
	return(error);
}

static int
bt463_load_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CPALETTE_RAM & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CPALETTE_RAM >> 8) & 0xff);

	/* spit out the colormap data */
	for(i = 0; i < palette->count; i++) {
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, palette->red[i]);
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, palette->green[i]);
		sc->gfbc->ramdac_wr(sc, BT463_REG_CMAP_DATA, palette->blue[i]);
	}
	return(error);
}

#endif /* 0 */

static int
bt463_save_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CURSOR_COLOR_0 & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CURSOR_COLOR_0 >> 8) & 0xff);

	/* spit out the cursor data */
	for(i = 0; i < palette->count; i++) {
		palette->red[i] = BTRNREG(sc);
		palette->green[i] = BTRNREG(sc);
		palette->blue[i] = BTRNREG(sc);
	}
	return(error);
}

static int
bt463_load_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_LOW,
	    BT463_IREG_CURSOR_COLOR_0 & 0xff);
	sc->gfbc->ramdac_wr(sc, BT463_REG_ADDR_HIGH,
	    (BT463_IREG_CURSOR_COLOR_0 >> 8) & 0xff);

	/* spit out the cursor data */
	for(i = 0; i < palette->count; i++) {
		BTWNREG(sc, palette->red[i]);
		BTWNREG(sc, palette->green[i]);
		BTWNREG(sc, palette->blue[i]);
	}
	return(error);
}

static void
bt485_init(struct gfb_softc *sc)
{
	int i, j, num_cmap_entries;
	u_int8_t regval;

	regval = sc->gfbc->ramdac_rd(sc, BT485_REG_COMMAND_0);

	/*
	 * Set the RAMDAC to 8 bit resolution, rather than 6 bit
	 * resolution.
	 */
	regval |= 0x02;

	/*
	 * Set the RAMDAC to sync-on-green.
	 */
	regval |= 0x08;
	sc->gfbc->ramdac_wr(sc, BT485_REG_COMMAND_0, regval);

#if 0
	/* Set the RAMDAC to 8BPP (no interesting options). */
	sc->gfbc->ramdac_wr(sc, BT485_REG_COMMAND_1, 0x40);

	/* Disable the cursor (for now) */
	regval = sc->gfbc->ramdac_rd(sc, BT485_REG_COMMAND_2);
	regval &= ~0x03;
	regval |= 0x24;
	sc->gfbc->ramdac_wr(sc, BT485_REG_COMMAND_2, regval);

	/* Use a 64x64x2 cursor */
	sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR, BT485_IREG_COMMAND_3);
	regval = sc->gfbc->ramdac_rd(sc, BT485_REG_EXTENDED);
	regval |= 0x04;
	regval |= 0x08;
	sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR, BT485_IREG_COMMAND_3);
	sc->gfbc->ramdac_wr(sc, BT485_REG_EXTENDED, regval);

	/* Set the Pixel Mask to something useful */
	sc->gfbc->ramdac_wr(sc, BT485_REG_PIXMASK, 0xff);
#endif

	/* Generate the cursor color map (Light-Grey)... */
	for(i = 0; i < sc->gfbc->cursor_palette.count; i++) {
		sc->gfbc->cursor_palette.red[i] = default_cmap[7].red;
		sc->gfbc->cursor_palette.green[i] = default_cmap[7].green;
		sc->gfbc->cursor_palette.blue[i] = default_cmap[7].blue;
	}

#if 0
	/* Enable cursor... */
	regval = sc->gfbc->ramdac_rd(sc, BT485_REG_COMMAND_2);
	if(!(regval & 0x01)) {
		regval |= 0x01;
		sc->gfbc->ramdac_wr(sc, BT485_REG_COMMAND_2, regval);
	}
	else if(regval & 0x03) {
		regval &= ~0x03;
		sc->gfbc->ramdac_wr(sc, BT485_REG_COMMAND_2, regval);
	}
#endif

	/* Generate the screen color map... */
	num_cmap_entries = sizeof(default_cmap) / sizeof(struct cmap);
	for(i = 0; i < sc->gfbc->palette.count / num_cmap_entries; i++)
		for(j = 0; j < num_cmap_entries; j++) {
			sc->gfbc->palette.red[(num_cmap_entries * i) + j] =
			    default_cmap[j].red;
			sc->gfbc->palette.green[(num_cmap_entries * i) + j] =
			    default_cmap[j].green;
			sc->gfbc->palette.blue[(num_cmap_entries * i) + j] =
			    default_cmap[j].blue;
		}
}

static int
bt485_save_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* addr[9:0] assumed to be 0 */
	/* set addr[7:0] to 0 */
	sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR, 0x00);

	/* spit out the color data */
	for(i = 0; i < palette->count; i++) {
		palette->red[i] = sc->gfbc->ramdac_rd(sc, BT485_REG_PALETTE);
		palette->green[i] = sc->gfbc->ramdac_rd(sc, BT485_REG_PALETTE);
		palette->blue[i] = sc->gfbc->ramdac_rd(sc, BT485_REG_PALETTE);
	}
	return(error);
}

static int
bt485_load_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* addr[9:0] assumed to be 0 */
	/* set addr[7:0] to 0 */
	sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR, 0x00);

	/* spit out the color data */
	for(i = 0; i < palette->count; i++) {
		sc->gfbc->ramdac_wr(sc, BT485_REG_PALETTE, palette->red[i]);
		sc->gfbc->ramdac_wr(sc, BT485_REG_PALETTE, palette->green[i]);
		sc->gfbc->ramdac_wr(sc, BT485_REG_PALETTE, palette->blue[i]);
	}
	return(error);
}

static int
bt485_save_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* addr[9:0] assumed to be 0 */
	/* set addr[7:0] to 1 */
	sc->gfbc->ramdac_wr(sc, BT485_REG_COC_WRADDR, 0x01);

	/* spit out the cursor color data */
	for(i = 0; i < palette->count; i++) {
		palette->red[i] = sc->gfbc->ramdac_rd(sc, BT485_REG_COCDATA);
		palette->green[i] = sc->gfbc->ramdac_rd(sc, BT485_REG_COCDATA);
		palette->blue[i] = sc->gfbc->ramdac_rd(sc, BT485_REG_COCDATA);
	}
	return(error);
}

static int
bt485_load_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	struct gfb_softc *sc;
	int error, i;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/* addr[9:0] assumed to be 0 */
	/* set addr[7:0] to 1 */
	sc->gfbc->ramdac_wr(sc, BT485_REG_COC_WRADDR, 0x01);

	/* spit out the cursor color data */
	for(i = 0; i < palette->count; i++) {
		sc->gfbc->ramdac_wr(sc, BT485_REG_COCDATA, palette->red[i]);
		sc->gfbc->ramdac_wr(sc, BT485_REG_COCDATA, palette->green[i]);
		sc->gfbc->ramdac_wr(sc, BT485_REG_COCDATA, palette->blue[i]);
	}
	return(error);
}

static int
bt485_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	struct gfb_softc *sc;
	int error, s;

	error = 0;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];
	s = spltty();
	*col = (sc->gfbc->ramdac_rd(sc, BT485_REG_CURSOR_X_HIGH) & 0x0f) << 8;
	*col |= sc->gfbc->ramdac_rd(sc, BT485_REG_CURSOR_X_LOW) & 0xff;
	*col /= adp->va_info.vi_cwidth;
	*col -= 8;
	*row = (sc->gfbc->ramdac_rd(sc, BT485_REG_CURSOR_Y_HIGH) & 0x0f) << 8;
	*row |= sc->gfbc->ramdac_rd(sc, BT485_REG_CURSOR_Y_LOW) & 0xff;
	*row /= adp->va_info.vi_cheight;
	*row -= 4;
	splx(s);
	return(error);
}

static int
bt485_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	struct gfb_softc *sc;
	int error, s;

	error = 0;

	/* Make sure the parameters are in range for the screen
	   size... */
	if((row > adp->va_info.vi_height) || (col > adp->va_info.vi_width))
		error = EINVAL;
	else if(((row * adp->va_info.vi_cheight) > 0x0fff) ||
	    ((col * adp->va_info.vi_cwidth) > 0x0fff))
		error = EINVAL;
	else if((row < 0) || (col < 0)) {
		/* If either of the parameters is less than 0, then hide the
		   cursor... */
		col = -8;
		row = -4;
	} else {
		/* Otherwise, just move the cursor as requested... */
		sc = gfb_device_softcs[adp->va_model][adp->va_unit];
		s = spltty();
		sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_X_LOW,
		    ((col + 8) * adp->va_info.vi_cwidth) & 0xff);
		sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_X_HIGH,
		    (((col + 8) * adp->va_info.vi_cwidth) >> 8) & 0x0f);
		sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_Y_LOW,
		    ((row + 4) * adp->va_info.vi_cheight) & 0xff);
		sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_Y_HIGH,
		    (((row + 4) * adp->va_info.vi_cheight) >> 8) & 0x0f);
		splx(s);
	}
	return(error);
}

static int
bt485_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int cellsize, int blink)
{
	struct gfb_softc *sc;
	int error, cell_count, count, i, j;
	u_int8_t regval;

	error = 0;
	cellsize /= 2;
	sc = gfb_device_softcs[adp->va_model][adp->va_unit];

	/*
	   Make sure the parameters are in range for the cursor
	   (it's a 64x64 cursor)...
	*/
	if(height > 64)
		error = EINVAL;
	else if(height <= 0) {
		/* If height is less than or equal to 0, then hide the
		   cursor... */
	} else {
		/* Otherwise, just resize the cursor as requested... */

		/* 64 pixels per cursor-row, 2 bits-per-pixel, so counts in
		   bytes... */
		cell_count = cellsize / 8;
		count = 64 / 8;
	
		 /* 
		  * Write the cursor image data:
		  *	set addr[9:8] to 0,
		  *	set addr[7:0] to 0,
		  *	spit it all out.
		  */
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR,
		    BT485_IREG_COMMAND_3);
		regval = sc->gfbc->ramdac_rd(sc, BT485_REG_EXTENDED);
		regval &= ~0x03;
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR,
		    BT485_IREG_COMMAND_3);
		sc->gfbc->ramdac_wr(sc, BT485_REG_EXTENDED, regval);
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR, 0);

		/* Fill-in the desired pixels in the specified pixel-rows... */
		for(i = 0; i < height; i++) {
			for(j = 0; j < cell_count; j++)
				sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_RAM,
				    0xff);
			for(j = 0; j < count - cell_count; j++)
				sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_RAM,
				    0x00);
		}

		/* Clear the remaining pixel rows... */
		for(; i < 64; i++)
			for(j = 0; j < count; j++)
				sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_RAM,
				    0x00);

		/*
		 * Write the cursor mask data:
		 *	set addr[9:8] to 2,
		 *	set addr[7:0] to 0,
		 *	spit it all out.
		 */
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR,
		    BT485_IREG_COMMAND_3);
		regval = sc->gfbc->ramdac_rd(sc, BT485_REG_EXTENDED);
		regval &= ~0x03; regval |= 0x02;
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR,
		    BT485_IREG_COMMAND_3);
		sc->gfbc->ramdac_wr(sc, BT485_REG_EXTENDED, regval);
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR, 0);

		/* Fill-in the desired pixels in the specified pixel-rows... */
		for(i = 0; i < height; i++) {
			for(j = 0; j < cell_count; j++)
				sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_RAM,
				    0xff);
			for(j = 0; j < count - cell_count; j++)
				sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_RAM,
				    0x00);
		}

		/* Clear the remaining pixel rows... */
		for(; i < 64; i++)
			for(j = 0; j < count; j++)
				sc->gfbc->ramdac_wr(sc, BT485_REG_CURSOR_RAM,
				    0x00);

		/* set addr[9:0] back to 0 */
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR,
		    BT485_IREG_COMMAND_3);
		regval = sc->gfbc->ramdac_rd(sc, BT485_REG_EXTENDED);
		regval &= ~0x03;
		sc->gfbc->ramdac_wr(sc, BT485_REG_PCRAM_WRADDR,
		    BT485_IREG_COMMAND_3);
		sc->gfbc->ramdac_wr(sc, BT485_REG_EXTENDED, regval);
	}
	return(error);
}

static void
ibm561_init(struct gfb_softc *sc)
{
}

static int
ibm561_save_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	int error;

	error = 0;
	return(error);
}

static int
ibm561_load_palette(video_adapter_t *adp, video_color_palette_t *palette)
{
	int error;

	error = 0;
	return(error);
}

static int
ibm561_save_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	int error;

	error = 0;
	return(error);
}

static int
ibm561_load_cursor_palette(video_adapter_t *adp, struct fbcmap *palette)
{
	int error;

	error = 0;
	return(error);
}

#undef MB
#undef KB
