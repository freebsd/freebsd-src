/*
 *  FBcon low-level driver for Atari interleaved bitplanes (8 planes) (iplan2p8)
 */

#ifndef _VIDEO_FBCON_IPLAN2P8_H
#define _VIDEO_FBCON_IPLAN2P8_H

#include <linux/config.h>

#ifdef MODULE
#if defined(CONFIG_FBCON_IPLAN2P8) || defined(CONFIG_FBCON_IPLAN2P8_MODULE)
#define FBCON_HAS_IPLAN2P8
#endif
#else
#if defined(CONFIG_FBCON_IPLAN2P8)
#define FBCON_HAS_IPLAN2P8
#endif
#endif

extern struct display_switch fbcon_iplan2p8;
extern void fbcon_iplan2p8_setup(struct display *p);
extern void fbcon_iplan2p8_bmove(struct display *p, int sy, int sx, int dy,
				 int dx, int height, int width);
extern void fbcon_iplan2p8_clear(struct vc_data *conp, struct display *p,
				 int sy, int sx, int height, int width);
extern void fbcon_iplan2p8_putc(struct vc_data *conp, struct display *p, int c,
				int yy, int xx);
extern void fbcon_iplan2p8_putcs(struct vc_data *conp, struct display *p,
				 const unsigned short *s, int count, int yy, int xx);
extern void fbcon_iplan2p8_revc(struct display *p, int xx, int yy);

#endif /* _VIDEO_FBCON_IPLAN2P8_H */
