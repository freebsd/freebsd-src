#ifndef __MATROXFB_CRTC2_H__
#define __MATROXFB_CRTC2_H__

#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "matroxfb_base.h"

struct matroxfb_dh_fb_info {
	struct fb_info		fbcon;
	int			fbcon_registered;

	struct matrox_fb_info*	primary_dev;

	struct {
		unsigned long	base;	/* physical */
		vaddr_t		vbase;	/* virtual */
		unsigned int	len;
		unsigned int	len_usable;
		unsigned int	len_maximum;
		unsigned int 	offbase;
		unsigned int	borrowed;
			      } video;
	struct {
		unsigned long	base;
		vaddr_t		vbase;
		unsigned int	len;
			      } mmio;

	int			currcon;
	struct display*		currcon_display;
	
	int			interlaced:1;

	union {
#ifdef FBCON_HAS_CFB16
		u_int16_t	cfb16[16];
#endif
#ifdef FBCON_HAS_CFB32
		u_int32_t	cfb32[16];
#endif
	} cmap;
	struct { unsigned red, green, blue, transp; } palette[16];
};

#endif /* __MATROXFB_CRTC2_H__ */
