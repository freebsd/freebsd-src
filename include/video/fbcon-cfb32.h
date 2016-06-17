/*
 *  FBcon low-level driver for 32 bpp packed pixel (cfb32)
 */

#ifndef _VIDEO_FBCON_CFB32_H
#define _VIDEO_FBCON_CFB32_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_CFB32) || defined(CONFIG_FBCON_CFB32_MODULE)
#define FBCON_HAS_CFB32
#endif
#else
#if defined(CONFIG_FBCON_CFB32)
#define FBCON_HAS_CFB32
#endif
#endif

extern struct display_switch fbcon_cfb32;
extern void fbcon_cfb32_setup(struct display *p);
extern void fbcon_cfb32_bmove(struct display *p, int sy, int sx, int dy,
			      int dx, int height, int width);
extern void fbcon_cfb32_clear(struct vc_data *conp, struct display *p, int sy,
			      int sx, int height, int width);
extern void fbcon_cfb32_putc(struct vc_data *conp, struct display *p, int c,
			     int yy, int xx);
extern void fbcon_cfb32_putcs(struct vc_data *conp, struct display *p,
			      const unsigned short *s, int count, int yy, int xx);
extern void fbcon_cfb32_revc(struct display *p, int xx, int yy);
extern void fbcon_cfb32_clear_margins(struct vc_data *conp, struct display *p,
				      int bottom_only);

#endif /* _VIDEO_FBCON_CFB32_H */
