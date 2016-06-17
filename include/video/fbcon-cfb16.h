/*
 *  FBcon low-level driver for 16 bpp packed pixel (cfb16)
 */

#ifndef _VIDEO_FBCON_CFB16_H
#define _VIDEO_FBCON_CFB16_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_CFB16) || defined(CONFIG_FBCON_CFB16_MODULE)
#define FBCON_HAS_CFB16
#endif
#else
#if defined(CONFIG_FBCON_CFB16)
#define FBCON_HAS_CFB16
#endif
#endif

extern struct display_switch fbcon_cfb16;
extern void fbcon_cfb16_setup(struct display *p);
extern void fbcon_cfb16_bmove(struct display *p, int sy, int sx, int dy,
			      int dx, int height, int width);
extern void fbcon_cfb16_clear(struct vc_data *conp, struct display *p, int sy,
			      int sx, int height, int width);
extern void fbcon_cfb16_putc(struct vc_data *conp, struct display *p, int c,
			     int yy, int xx);
extern void fbcon_cfb16_putcs(struct vc_data *conp, struct display *p,
			      const unsigned short *s, int count, int yy, int xx);
extern void fbcon_cfb16_revc(struct display *p, int xx, int yy);
extern void fbcon_cfb16_clear_margins(struct vc_data *conp, struct display *p,
				      int bottom_only);

#endif /* _VIDEO_FBCON_CFB16_H */
