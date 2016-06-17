/*
 *  FBcon low-level driver for Amiga interleaved bitplanes (ilbm)
 */

#ifndef _VIDEO_FBCON_ILBM_H
#define _VIDEO_FBCON_ILBM_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_ILBM) || defined(CONFIG_FBCON_ILBM_MODULE)  
#define FBCON_HAS_ILBM 
#endif
#else
#if defined(CONFIG_FBCON_ILBM) 
#define FBCON_HAS_ILBM 
#endif
#endif

extern struct display_switch fbcon_ilbm;
extern void fbcon_ilbm_setup(struct display *p);
extern void fbcon_ilbm_bmove(struct display *p, int sy, int sx, int dy, int dx,
			     int height, int width);
extern void fbcon_ilbm_clear(struct vc_data *conp, struct display *p, int sy,
			     int sx, int height, int width);
extern void fbcon_ilbm_putc(struct vc_data *conp, struct display *p, int c,
			    int yy, int xx);
extern void fbcon_ilbm_putcs(struct vc_data *conp, struct display *p,
			     const unsigned short *s, int count, int yy, int xx);
extern void fbcon_ilbm_revc(struct display *p, int xx, int yy);

#endif /* _VIDEO_FBCON_ILBM_H */
