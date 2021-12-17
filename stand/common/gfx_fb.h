/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Toomas Soome
 * Copyright 2020 RackTop Systems, Inc.
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
 *
 * $FreeBSD$
 */

#ifndef _GFX_FB_H
#define	_GFX_FB_H

#include <sys/font.h>
#include <teken.h>
#include <stdbool.h>
#include <machine/metadata.h>
#include <pnglite.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	EDID_MAGIC	{ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 }

struct edid_header {
	uint8_t header[8];	/* fixed header pattern */
	uint16_t manufacturer_id;
	uint16_t product_code;
	uint32_t serial_number;
	uint8_t week_of_manufacture;
	uint8_t year_of_manufacture;
	uint8_t version;
	uint8_t revision;
};

struct edid_basic_display_parameters {
	uint8_t video_input_parameters;
	uint8_t max_horizontal_image_size;
	uint8_t max_vertical_image_size;
	uint8_t display_gamma;
	uint8_t supported_features;
};

struct edid_chromaticity_coordinates {
	uint8_t red_green_lo;
	uint8_t blue_white_lo;
	uint8_t red_x_hi;
	uint8_t red_y_hi;
	uint8_t green_x_hi;
	uint8_t green_y_hi;
	uint8_t blue_x_hi;
	uint8_t blue_y_hi;
	uint8_t white_x_hi;
	uint8_t white_y_hi;
};

struct edid_detailed_timings {
	uint16_t pixel_clock;
	uint8_t horizontal_active_lo;
	uint8_t horizontal_blanking_lo;
	uint8_t horizontal_hi;
	uint8_t vertical_active_lo;
	uint8_t vertical_blanking_lo;
	uint8_t vertical_hi;
	uint8_t horizontal_sync_offset_lo;
	uint8_t horizontal_sync_pulse_width_lo;
	uint8_t vertical_sync_lo;
	uint8_t sync_hi;
	uint8_t horizontal_image_size_lo;
	uint8_t vertical_image_size_lo;
	uint8_t image_size_hi;
	uint8_t horizontal_border;
	uint8_t vertical_border;
	uint8_t features;
};

struct vesa_edid_info {
	struct edid_header header;
	struct edid_basic_display_parameters display;
#define	EDID_FEATURE_PREFERRED_TIMING_MODE	(1 << 1)
	struct edid_chromaticity_coordinates chromaticity;
	uint8_t established_timings_1;
	uint8_t established_timings_2;
	uint8_t manufacturer_reserved_timings;
	uint16_t standard_timings[8];
	struct edid_detailed_timings detailed_timings[4];
	uint8_t number_of_extensions;
	uint8_t checksum;
} __packed;

extern struct vesa_edid_info *edid_info;

#define	STD_TIMINGS	8
#define	DET_TIMINGS	4

#define	HSIZE(x)	(((x & 0xff) + 31) * 8)
#define	RATIO(x)	((x & 0xC000) >> 14)
#define	RATIO1_1	0
/* EDID Ver. 1.3 redefined this */
#define	RATIO16_10	RATIO1_1
#define	RATIO4_3	1
#define	RATIO5_4	2
#define	RATIO16_9	3

/*
 * Number of pixels and lines is 12-bit int, valid values 0-4095.
 */
#define	EDID_MAX_PIXELS	4095
#define	EDID_MAX_LINES	4095

#define	GET_EDID_INFO_WIDTH(edid_info, timings_num) \
    ((edid_info)->detailed_timings[(timings_num)].horizontal_active_lo | \
    (((uint32_t)(edid_info)->detailed_timings[(timings_num)].horizontal_hi & \
    0xf0) << 4))

#define	GET_EDID_INFO_HEIGHT(edid_info, timings_num) \
    ((edid_info)->detailed_timings[(timings_num)].vertical_active_lo | \
    (((uint32_t)(edid_info)->detailed_timings[(timings_num)].vertical_hi & \
    0xf0) << 4))

struct resolution {
	uint32_t width;
	uint32_t height;
	TAILQ_ENTRY(resolution) next;
};

typedef TAILQ_HEAD(edid_resolution, resolution) edid_res_list_t;

struct vesa_flat_panel_info {
	uint16_t HSize;			/* Horizontal Size in Pixels */
	uint16_t VSize;			/* Vertical Size in Lines */
	uint16_t FPType;		/* Flat Panel Type */
	uint8_t RedBPP;			/* Red Bits Per Primary */
	uint8_t GreenBPP;		/* Green Bits Per Primary */
	uint8_t BlueBPP;		/* Blue Bits Per Primary */
	uint8_t ReservedBPP;		/* Reserved Bits Per Primary */
	uint32_t RsvdOffScrnMemSize;	/* Size in KB of Offscreen Memory */
	uint32_t RsvdOffScrnMemPtr; /* Pointer to reserved offscreen memory */
	uint8_t Reserved[14];		/* remainder of FPInfo */
} __packed;

#define	COLOR_FORMAT_VGA 0
#define	COLOR_FORMAT_RGB 1
#define	NCOLORS	16
#define	NCMAP	256
extern uint32_t cmap[NCMAP];

/*
 * VT_FB_MAX_WIDTH and VT_FB_MAX_HEIGHT are dimensions from where
 * we will not auto select smaller font than 8x16.
 * See also sys/dev/vt/vt.h
 */
#ifndef VT_FB_MAX_WIDTH
#define	VT_FB_MAX_WIDTH		4096
#endif
#ifndef VT_FB_MAX_HEIGHT
#define	VT_FB_MAX_HEIGHT	2400
#endif

enum FB_TYPE {
	FB_TEXT = -1,
	FB_GOP,
	FB_UGA,
	FB_VBE
};

enum COLOR_TYPE {
	CT_INDEXED,
	CT_RGB
};

struct gen_fb {
	uint64_t	fb_addr;
	uint64_t	fb_size;
	uint32_t	fb_height;
	uint32_t	fb_width;
	uint32_t	fb_stride;
	uint32_t	fb_mask_red;
	uint32_t	fb_mask_green;
	uint32_t	fb_mask_blue;
	uint32_t	fb_mask_reserved;
	uint32_t	fb_bpp;
};

typedef struct teken_gfx {
	enum FB_TYPE	tg_fb_type;
	enum COLOR_TYPE tg_ctype;
	unsigned	tg_mode;
	teken_t		tg_teken;		/* Teken core */
	teken_pos_t	tg_cursor;		/* Where cursor was drawn */
	bool		tg_cursor_visible;
	teken_pos_t	tg_tp;			/* Terminal dimensions */
	teken_pos_t	tg_origin;		/* Point of origin in pixels */
	uint8_t		*tg_glyph;		/* Memory for glyph */
	size_t		tg_glyph_size;
	struct vt_font	tg_font;
	struct gen_fb	tg_fb;
	uint32_t	*tg_shadow_fb;		/* units of 4 bytes */
	size_t		tg_shadow_sz;		/* units of pages */
	teken_funcs_t	*tg_functions;
	void		*tg_private;
} teken_gfx_t;

extern font_list_t fonts;
extern teken_gfx_t gfx_state;

typedef enum {
	GfxFbBltVideoFill,
	GfxFbBltVideoToBltBuffer,
	GfxFbBltBufferToVideo,
	GfxFbBltVideoToVideo,
	GfxFbBltOperationMax,
} GFXFB_BLT_OPERATION;

int gfxfb_blt(void *, GFXFB_BLT_OPERATION, uint32_t, uint32_t,
    uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

int generate_cons_palette(uint32_t *, int, uint32_t, int, uint32_t, int,
    uint32_t, int);
bool console_update_mode(bool);
void setup_font(teken_gfx_t *, teken_unit_t, teken_unit_t);
uint8_t *font_lookup(const struct vt_font *, teken_char_t,
    const teken_attr_t *);
void bios_text_font(bool);

/* teken callbacks. */
tf_cursor_t gfx_fb_cursor;
tf_putchar_t gfx_fb_putchar;
tf_fill_t gfx_fb_fill;
tf_copy_t gfx_fb_copy;
tf_param_t gfx_fb_param;

/* Screen buffer element */
struct text_pixel {
	teken_char_t c;
	teken_attr_t a;
};

extern const int cons_to_vga_colors[NCOLORS];

/* Screen buffer to track changes on the terminal screen. */
extern struct text_pixel *screen_buffer;
bool is_same_pixel(struct text_pixel *, struct text_pixel *);

bool gfx_get_edid_resolution(struct vesa_edid_info *, edid_res_list_t *);
void gfx_framework_init(void);
void gfx_fb_cons_display(uint32_t, uint32_t, uint32_t, uint32_t, void *);
void gfx_fb_setpixel(uint32_t, uint32_t);
void gfx_fb_drawrect(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void gfx_term_drawrect(uint32_t, uint32_t, uint32_t, uint32_t);
void gfx_fb_line(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void gfx_fb_bezier(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t);

#define	FL_PUTIMAGE_BORDER	0x1
#define	FL_PUTIMAGE_NOSCROLL	0x2
#define	FL_PUTIMAGE_DEBUG	0x80

int gfx_fb_putimage(png_t *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
bool gfx_parse_mode_str(char *, int *, int *, int *);
void term_image_display(teken_gfx_t *, const teken_rect_t *);

void reset_font_flags(void);

#ifdef __cplusplus
}
#endif

#endif /* _GFX_FB_H */
