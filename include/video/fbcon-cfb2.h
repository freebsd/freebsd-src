/*
 *  FBcon low-level driver for 2 bpp packed pixel (cfb2)
 */

#ifndef _VIDEO_FBCON_CFB2_H
#define _VIDEO_FBCON_CFB2_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_CFB2) || defined(CONFIG_FBCON_CFB2_MODULE)
#define FBCON_HAS_CFB2
#endif
#else
#if defined(CONFIG_FBCON_CFB2)
#define FBCON_HAS_CFB2
#endif
#endif

extern struct display_switch fbcon_cfb2;
extern void fbcon_cfb2_setup(struct display *p);
extern void fbcon_cfb2_bmove(struct display *p, int sy, int sx, int dy, int dx,
			     int height, int width);
extern void fbcon_cfb2_clear(struct vc_data *conp, struct display *p, int sy,
			     int sx, int height, int width);
extern void fbcon_cfb2_putc(struct vc_data *conp, struct display *p, int c,
			    int yy, int xx);
extern void fbcon_cfb2_putcs(struct vc_data *conp, struct display *p,
			     const unsigned short *s, int count, int yy, int xx);
extern void fbcon_cfb2_revc(struct display *p, int xx, int yy);

#endif /* _VIDEO_FBCON_CFB2_H */
