/*
 *  FBcon low-level driver for VGA 4-plane modes
 */

#ifndef _VIDEO_FBCON_VGA_PLANES_H
#define _VIDEO_FBCON_VGA_PLANES_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_VGA_PLANES) || defined(CONFIG_FBCON_VGA_PLANES_MODULE)
#define FBCON_HAS_VGA_PLANES
#endif
#else
#if defined(CONFIG_FBCON_VGA_PLANES)
#define FBCON_HAS_VGA_PLANES
#endif
#endif

extern struct display_switch fbcon_vga_planes;
extern struct display_switch fbcon_ega_planes;
extern void fbcon_vga_planes_setup(struct display *p);
extern void fbcon_vga_planes_bmove(struct display *p, int sy, int sx, int dy, int dx,
				   int height, int width);
extern void fbcon_vga_planes_clear(struct vc_data *conp, struct display *p, int sy,
				   int sx, int height, int width);
extern void fbcon_vga_planes_putc(struct vc_data *conp, struct display *p, int c,
				  int yy, int xx);
extern void fbcon_ega_planes_putc(struct vc_data *conp, struct display *p, int c,
				  int yy, int xx);
extern void fbcon_vga_planes_putcs(struct vc_data *conp, struct display *p,
				   const unsigned short *s, int count, int yy, int xx);
extern void fbcon_ega_planes_putcs(struct vc_data *conp, struct display *p,
				   const unsigned short *s, int count, int yy, int xx);
extern void fbcon_vga_planes_revc(struct display *p, int xx, int yy);

#endif /* _VIDEO_FBCON_VGA_PLANES_H */
