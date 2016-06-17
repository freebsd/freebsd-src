/*
 *  FBcon low-level driver for VGA characters/attributes
 */

#ifndef _VIDEO_FBCON_VGA_H
#define _VIDEO_FBCON_VGA_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_VGA) || defined(CONFIG_FBCON_VGA_MODULE)
#define FBCON_HAS_VGA
#endif
#else
#if defined(CONFIG_FBCON_VGA)
#define FBCON_HAS_VGA
#endif
#endif

extern struct display_switch fbcon_vga;
extern void fbcon_vga_setup(struct display *p);
extern void fbcon_vga_bmove(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width);
extern void fbcon_vga_clear(struct vc_data *conp, struct display *p, int sy,
			    int sx, int height, int width);
extern void fbcon_vga_putc(struct vc_data *conp, struct display *p, int c,
			   int yy, int xx);
extern void fbcon_vga_putcs(struct vc_data *conp, struct display *p,
			    const unsigned short *s, int count, int yy, int xx);
extern void fbcon_vga_revc(struct display *p, int xx, int yy);

#endif /* _VIDEO_FBCON_VGA_H */
