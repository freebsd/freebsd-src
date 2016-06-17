/*
 *  FBcon low-level driver for Amiga bitplanes (afb)
 */

#ifndef _VIDEO_FBCON_AFB_H
#define _VIDEO_FBCON_AFB_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_AFB) || defined(CONFIG_FBCON_AFB_MODULE)
#define FBCON_HAS_AFB
#endif
#else
#if defined(CONFIG_FBCON_AFB)
#define FBCON_HAS_AFB
#endif
#endif

extern struct display_switch fbcon_afb;
extern void fbcon_afb_setup(struct display *p);
extern void fbcon_afb_bmove(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width);
extern void fbcon_afb_clear(struct vc_data *conp, struct display *p, int sy,
			    int sx, int height, int width);
extern void fbcon_afb_putc(struct vc_data *conp, struct display *p, int c,
			   int yy, int xx);
extern void fbcon_afb_putcs(struct vc_data *conp, struct display *p,
			    const unsigned short *s, int count, int yy, int xx);
extern void fbcon_afb_revc(struct display *p, int xx, int yy);

#endif
