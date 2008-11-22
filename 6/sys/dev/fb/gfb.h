/*-
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
 *
 * Copyright (c) 2000 Andrew Miklic
 *
 * $FreeBSD$
 */

#ifndef _FB_GFB_H_
#define _FB_GFB_H_

#define MAX_NUM_GFB_CARDS	16

#define GFB_UNIT(dev)		minor(dev)
#define GFB_MKMINOR(unit)	(unit)
#define BIT_REVERSE(byte)		\
	((((byte) & 0x01) << 7) |	\
	 (((byte) & 0x02) << 5) |	\
	 (((byte) & 0x04) << 3) |	\
	 (((byte) & 0x08) << 1) |	\
	 (((byte) & 0x10) >> 1) |	\
	 (((byte) & 0x20) >> 3) |	\
	 (((byte) & 0x40) >> 5) |	\
	 (((byte) & 0x80) >> 7))
#define probe_done(adp)         ((adp)->va_flags & V_ADP_PROBED)
#define init_done(adp)          ((adp)->va_flags & V_ADP_INITIALIZED)
#define config_done(adp)        ((adp)->va_flags & V_ADP_REGISTERED)

struct gfb_softc;

int gfb_error(void);
vi_probe_t gfb_probe;
vi_init_t gfb_init;

vi_get_info_t gfb_get_info;
/*vi_query_mode_t gfb_query_mode;*/
vi_set_mode_t gfb_set_mode;
vi_save_font_t gfb_save_font;
vi_load_font_t gfb_load_font;
vi_show_font_t gfb_show_font;
vi_save_palette_t gfb_save_palette;
vi_load_palette_t gfb_load_palette;
vi_save_state_t gfb_save_state;
vi_load_state_t gfb_load_state;
vi_read_hw_cursor_t gfb_read_hw_cursor;
vi_set_hw_cursor_t gfb_set_hw_cursor;
vi_set_hw_cursor_shape_t gfb_set_hw_cursor_shape;
vi_mmap_t gfb_mmap;
vi_ioctl_t gfb_ioctl;
vi_set_border_t gfb_set_border;
vi_set_win_org_t gfb_set_win_org;
vi_fill_rect_t gfb_fill_rect;
vi_bitblt_t gfb_bitblt;
vi_clear_t gfb_clear;
vi_diag_t gfb_diag;
vi_save_cursor_palette_t gfb_save_cursor_palette;
vi_load_cursor_palette_t gfb_load_cursor_palette;
vi_copy_t gfb_copy;
vi_putp_t gfb_putp;
vi_putc_t gfb_putc;
vi_puts_t gfb_puts;
vi_putm_t gfb_putm;

typedef void gfb_ramdac_init_t(struct gfb_softc *);
typedef u_int8_t gfb_ramdac_rd_t(struct gfb_softc *, u_int);
typedef void gfb_ramdac_wr_t(struct gfb_softc *, u_int, u_int8_t);
typedef void gfb_ramdac_intr_t(struct gfb_softc *);
typedef int gfb_ramdac_save_palette_t(video_adapter_t *, video_color_palette_t *);
typedef int gfb_ramdac_load_palette_t(video_adapter_t *, video_color_palette_t *);
typedef int gfb_ramdac_save_cursor_palette_t(video_adapter_t *, struct fbcmap *);
typedef int gfb_ramdac_load_cursor_palette_t(video_adapter_t *, struct fbcmap *);
typedef int gfb_ramdac_read_hw_cursor_t(video_adapter_t *, int *, int *);
typedef int gfb_ramdac_set_hw_cursor_t(video_adapter_t *, int, int);
typedef int gfb_ramdac_set_hw_cursor_shape_t(video_adapter_t *, int, int, int, int);
typedef int gfb_builtin_save_palette_t(video_adapter_t *, video_color_palette_t *);
typedef int gfb_builtin_load_palette_t(video_adapter_t *, video_color_palette_t *);
typedef int gfb_builtin_save_cursor_palette_t(video_adapter_t *, struct fbcmap *);
typedef int gfb_builtin_load_cursor_palette_t(video_adapter_t *, struct fbcmap *);
typedef int gfb_builtin_read_hw_cursor_t(video_adapter_t *, int *, int *);
typedef int gfb_builtin_set_hw_cursor_t(video_adapter_t *, int, int);
typedef int gfb_builtin_set_hw_cursor_shape_t(video_adapter_t *, int, int, int, int);

struct monitor {
	u_int16_t	cols;		/* Columns */
	u_int16_t	hfp;		/* Horizontal Front Porch */
	u_int16_t	hsync;		/* Horizontal Sync */
	u_int16_t	hbp;		/* Horizontal Back Porch */
	u_int16_t	rows;		/* Rows */
	u_int16_t	vfp;		/* Vertical Front Porch */
	u_int16_t	vsync;		/* Vertical Sync */
	u_int16_t	vbp;		/* Vertical Back Porch */
	u_int32_t	dotclock;	/* Dot Clock */
};

struct gfb_font {
	int width;
	int height;
	u_char data[256 * 32];
};

struct gfb_conf {
	char *name;                     /* name for this board type */
	char *ramdac_name;              /* name for this RAMDAC */
	u_char *font;
	struct gfb_font fonts[4];
	video_color_palette_t palette;
	struct fbcmap cursor_palette;
	gfb_ramdac_init_t *ramdac_init;
	gfb_ramdac_rd_t *ramdac_rd;
	gfb_ramdac_wr_t *ramdac_wr;
	gfb_ramdac_intr_t *ramdac_intr;
	gfb_ramdac_save_palette_t *ramdac_save_palette;
	gfb_ramdac_load_palette_t *ramdac_load_palette;
	gfb_ramdac_save_cursor_palette_t *ramdac_save_cursor_palette;
	gfb_ramdac_load_cursor_palette_t *ramdac_load_cursor_palette;
	gfb_ramdac_read_hw_cursor_t *ramdac_read_hw_cursor;
	gfb_ramdac_set_hw_cursor_t *ramdac_set_hw_cursor;
	gfb_ramdac_set_hw_cursor_shape_t *ramdac_set_hw_cursor_shape;
	gfb_builtin_save_palette_t *builtin_save_palette;
	gfb_builtin_load_palette_t *builtin_load_palette;
	gfb_builtin_save_cursor_palette_t *builtin_save_cursor_palette;
	gfb_builtin_load_cursor_palette_t *builtin_load_cursor_palette;
	gfb_builtin_read_hw_cursor_t *builtin_read_hw_cursor;
	gfb_builtin_set_hw_cursor_t *builtin_set_hw_cursor;
	gfb_builtin_set_hw_cursor_shape_t *builtin_set_hw_cursor_shape;
};

struct video_adapter;
struct genfb_softc;

typedef struct gfb_softc {
	char *driver_name;              /* name for this driver */
	struct video_adapter *adp;
	struct genfb_softc *gensc;
	struct gfb_conf *gfbc;
	bus_space_handle_t bhandle;
	bus_space_tag_t btag;
	bus_space_handle_t regs;
	void *intrhand;
	struct resource *irq;
	struct resource *res;
	u_int8_t rev;                   /* GFB revision */
	int type;
	int model;
	struct cdevsw *cdevsw;
	struct cdev *devt;
} *gfb_softc_t;

#endif /* _FB_GFB_H_ */
