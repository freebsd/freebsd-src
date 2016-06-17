/*
 *  FBcon low-level driver for Hercules Graphics Adaptor (hga)
 */

#ifndef _VIDEO_FBCON_HGA_H
#define _VIDEO_FBCON_HGA_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_HGA) || defined(CONFIG_FBCON_HGA_MODULE)
#define FBCON_HAS_HGA
#endif
#else
#if defined(CONFIG_FBCON_HGA)
#define FBCON_HAS_HGA
#endif
#endif

extern struct display_switch fbcon_hga;
extern void fbcon_hga_setup(struct display *p);
extern void fbcon_hga_bmove(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width);
extern void fbcon_hga_clear(struct vc_data *conp, struct display *p, int sy,
			    int sx, int height, int width);
extern void fbcon_hga_putc(struct vc_data *conp, struct display *p, int c,
			   int yy, int xx);
extern void fbcon_hga_putcs(struct vc_data *conp, struct display *p,
			    const unsigned short *s, int count, int yy, int xx);
extern void fbcon_hga_revc(struct display *p, int xx, int yy);

#endif /* _VIDEO_FBCON_HGA_H */
