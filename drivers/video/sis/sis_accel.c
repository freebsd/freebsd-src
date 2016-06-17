/*
 * SiS 300/630/730/540/315/550/65x/74x/330/760 frame buffer driver
 * for Linux kernels 2.4.x and 2.6.x
 *
 * 2D acceleration part
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Based on the XFree86 driver's sis300_accel.c which is
 *     Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 * and sis310_accel.c which is
 *     Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 *
 * Author: Thomas Winischhofer <thomas@winischhofer.net>
 *			(see http://www.winischhofer.net/
 *			for more information and updates)
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vt_kern.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/agp_backend.h>

#include <linux/types.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <linux/sisfb.h>
#else
#include <video/sisfb.h>
#endif

#include <asm/io.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>
#endif

#include "osdef.h"
#include "vgatypes.h"
#include "vstruct.h"
#include "sis_accel.h"
#include "sis.h"

extern struct   video_info ivideo;
extern int 	sisfb_accel;

static const int sisALUConv[] =
{
    0x00,       /* dest = 0;            0,      GXclear,        0 */
    0x88,       /* dest &= src;         DSa,    GXand,          0x1 */
    0x44,       /* dest = src & ~dest;  SDna,   GXandReverse,   0x2 */
    0xCC,       /* dest = src;          S,      GXcopy,         0x3 */
    0x22,       /* dest &= ~src;        DSna,   GXandInverted,  0x4 */
    0xAA,       /* dest = dest;         D,      GXnoop,         0x5 */
    0x66,       /* dest = ^src;         DSx,    GXxor,          0x6 */
    0xEE,       /* dest |= src;         DSo,    GXor,           0x7 */
    0x11,       /* dest = ~src & ~dest; DSon,   GXnor,          0x8 */
    0x99,       /* dest ^= ~src ;       DSxn,   GXequiv,        0x9 */
    0x55,       /* dest = ~dest;        Dn,     GXInvert,       0xA */
    0xDD,       /* dest = src|~dest ;   SDno,   GXorReverse,    0xB */
    0x33,       /* dest = ~src;         Sn,     GXcopyInverted, 0xC */
    0xBB,       /* dest |= ~src;        DSno,   GXorInverted,   0xD */
    0x77,       /* dest = ~src|~dest;   DSan,   GXnand,         0xE */
    0xFF,       /* dest = 0xFF;         1,      GXset,          0xF */
};
/* same ROP but with Pattern as Source */
static const int sisPatALUConv[] =
{
    0x00,       /* dest = 0;            0,      GXclear,        0 */
    0xA0,       /* dest &= src;         DPa,    GXand,          0x1 */
    0x50,       /* dest = src & ~dest;  PDna,   GXandReverse,   0x2 */
    0xF0,       /* dest = src;          P,      GXcopy,         0x3 */
    0x0A,       /* dest &= ~src;        DPna,   GXandInverted,  0x4 */
    0xAA,       /* dest = dest;         D,      GXnoop,         0x5 */
    0x5A,       /* dest = ^src;         DPx,    GXxor,          0x6 */
    0xFA,       /* dest |= src;         DPo,    GXor,           0x7 */
    0x05,       /* dest = ~src & ~dest; DPon,   GXnor,          0x8 */
    0xA5,       /* dest ^= ~src ;       DPxn,   GXequiv,        0x9 */
    0x55,       /* dest = ~dest;        Dn,     GXInvert,       0xA */
    0xF5,       /* dest = src|~dest ;   PDno,   GXorReverse,    0xB */
    0x0F,       /* dest = ~src;         Pn,     GXcopyInverted, 0xC */
    0xAF,       /* dest |= ~src;        DPno,   GXorInverted,   0xD */
    0x5F,       /* dest = ~src|~dest;   DPan,   GXnand,         0xE */
    0xFF,       /* dest = 0xFF;         1,      GXset,          0xF */
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,34)
static const unsigned char myrops[] = {
   	3, 10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
   };
#endif

/* 300 series ----------------------------------------------------- */
#ifdef CONFIG_FB_SIS_300
static void
SiS300Sync(void)
{
	SiS300Idle
}

static void
SiS300SetupForScreenToScreenCopy(int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color)
{
	SiS300SetupDSTColorDepth(ivideo.DstColor);
	SiS300SetupSRCPitch(ivideo.video_linelength)
	SiS300SetupDSTRect(ivideo.video_linelength, -1)

	if(trans_color != -1) {
		SiS300SetupROP(0x0A)
		SiS300SetupSRCTrans(trans_color)
		SiS300SetupCMDFlag(TRANSPARENT_BITBLT)
	} else {
	        SiS300SetupROP(sisALUConv[rop])
	}
	if(xdir > 0) {
		SiS300SetupCMDFlag(X_INC)
	}
	if(ydir > 0) {
		SiS300SetupCMDFlag(Y_INC)
	}
}

static void
SiS300SubsequentScreenToScreenCopy(int src_x, int src_y, int dst_x, int dst_y,
                                int width, int height)
{
	long srcbase, dstbase;

	srcbase = dstbase = 0;
	if (src_y >= 2048) {
		srcbase = ivideo.video_linelength * src_y;
		src_y = 0;
	}
	if (dst_y >= 2048) {
		dstbase = ivideo.video_linelength * dst_y;
		dst_y = 0;
	}

	SiS300SetupSRCBase(srcbase);
	SiS300SetupDSTBase(dstbase);

	if(!(ivideo.CommandReg & X_INC))  {
		src_x += width-1;
		dst_x += width-1;
	}
	if(!(ivideo.CommandReg & Y_INC))  {
		src_y += height-1;
		dst_y += height-1;
	}
	SiS300SetupRect(width, height)
	SiS300SetupSRCXY(src_x, src_y)
	SiS300SetupDSTXY(dst_x, dst_y)
	SiS300DoCMD
}

static void
SiS300SetupForSolidFill(int color, int rop, unsigned int planemask)
{
	SiS300SetupPATFG(color)
	SiS300SetupDSTRect(ivideo.video_linelength, -1)
	SiS300SetupDSTColorDepth(ivideo.DstColor);
	SiS300SetupROP(sisPatALUConv[rop])
	SiS300SetupCMDFlag(PATFG)
}

static void
SiS300SubsequentSolidFillRect(int x, int y, int w, int h)
{
	long dstbase;

	dstbase = 0;
	if(y >= 2048) {
		dstbase = ivideo.video_linelength * y;
		y = 0;
	}
	SiS300SetupDSTBase(dstbase)
	SiS300SetupDSTXY(x,y)
	SiS300SetupRect(w,h)
	SiS300SetupCMDFlag(X_INC | Y_INC | BITBLT)
	SiS300DoCMD
}
#endif

/* 315/330 series ------------------------------------------------- */

#ifdef CONFIG_FB_SIS_315
static void
SiS310Sync(void)
{
	SiS310Idle
}

static void
SiS310SetupForScreenToScreenCopy(int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color)
{
	SiS310SetupDSTColorDepth(ivideo.DstColor);
	SiS310SetupSRCPitch(ivideo.video_linelength)
	SiS310SetupDSTRect(ivideo.video_linelength, -1)
	if (trans_color != -1) {
		SiS310SetupROP(0x0A)
		SiS310SetupSRCTrans(trans_color)
		SiS310SetupCMDFlag(TRANSPARENT_BITBLT)
	} else {
	        SiS310SetupROP(sisALUConv[rop])
		/* Set command - not needed, both 0 */
		/* SiSSetupCMDFlag(BITBLT | SRCVIDEO) */
	}
	SiS310SetupCMDFlag(ivideo.SiS310_AccelDepth)
	/* The 315 series is smart enough to know the direction */
}

static void
SiS310SubsequentScreenToScreenCopy(int src_x, int src_y, int dst_x, int dst_y,
                                int width, int height)
{
	long srcbase, dstbase;
	int mymin, mymax;

	srcbase = dstbase = 0;
	mymin = min(src_y, dst_y);
	mymax = max(src_y, dst_y);
	
	/* Although the chip knows the direction to use
	 * if the source and destination areas overlap, 
	 * that logic fails if we fiddle with the bitmap
	 * addresses. Therefore, we check if the source
	 * and destination blitting areas overlap and 
	 * adapt the bitmap addresses synchronously 
	 * if the coordinates exceed the valid range.
	 * The the areas do not overlap, we do our 
	 * normal check.
	 */
	if((mymax - mymin) < height) { 
	   if((src_y >= 2048) || (dst_y >= 2048)) {	      
	      srcbase = ivideo.video_linelength * mymin;
	      dstbase = ivideo.video_linelength * mymin;
	      src_y -= mymin;
	      dst_y -= mymin;
	   }
	} else {
	   if(src_y >= 2048) {
	      srcbase = ivideo.video_linelength * src_y;
	      src_y = 0;
	   }
	   if(dst_y >= 2048) {
	      dstbase = ivideo.video_linelength * dst_y;
	      dst_y = 0;
	   }
	}

	SiS310SetupSRCBase(srcbase);
	SiS310SetupDSTBase(dstbase);
	SiS310SetupRect(width, height)
	SiS310SetupSRCXY(src_x, src_y)
	SiS310SetupDSTXY(dst_x, dst_y)
	SiS310DoCMD
}

static void
SiS310SetupForSolidFill(int color, int rop, unsigned int planemask)
{
	SiS310SetupPATFG(color)
	SiS310SetupDSTRect(ivideo.video_linelength, -1)
	SiS310SetupDSTColorDepth(ivideo.DstColor);
	SiS310SetupROP(sisPatALUConv[rop])
	SiS310SetupCMDFlag(PATFG | ivideo.SiS310_AccelDepth)
}

static void
SiS310SubsequentSolidFillRect(int x, int y, int w, int h)
{
	long dstbase;

	dstbase = 0;
	if(y >= 2048) {
		dstbase = ivideo.video_linelength * y;
		y = 0;
	}
	SiS310SetupDSTBase(dstbase)
	SiS310SetupDSTXY(x,y)
	SiS310SetupRect(w,h)
	SiS310SetupCMDFlag(BITBLT)
	SiS310DoCMD
}
#endif

/* --------------------------------------------------------------------- */

/* The exported routines */

int sisfb_initaccel(void)
{
#ifdef SISFB_USE_SPINLOCKS
    spin_lock_init(&ivideo.lockaccel);
#endif
    return(0);
}

void sisfb_syncaccel(void)
{
    if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
    	SiS300Sync();
#endif
    } else {
#ifdef CONFIG_FB_SIS_315
    	SiS310Sync();
#endif
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)  /* --------------- 2.5 --------------- */

int fbcon_sis_sync(struct fb_info *info)
{
   CRITFLAGS

   if(!ivideo.accel)
   	return 0;
   
   if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
      SiS300Sync();
#endif
   } else {
#ifdef CONFIG_FB_SIS_315
      SiS310Sync();
#endif
   }
   CRITEND
   return 0;
}

void fbcon_sis_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
   int col=0;
   CRITFLAGS

   TWDEBUG("Inside sis_fillrect");
   if(!rect->width || !rect->height)
   	return;

   if(!ivideo.accel) {
	cfb_fillrect(info, rect);
	return;
   }
   
   switch(info->var.bits_per_pixel) {
	case 8:  col = rect->color;
		 break;
	case 16: col = ((u32 *)(info->pseudo_palette))[rect->color];
		 break;
	case 32: col = ((u32 *)(info->pseudo_palette))[rect->color];
		 break;
   }

   if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
      CRITBEGIN
      SiS300SetupForSolidFill(col, myrops[rect->rop], 0);
      SiS300SubsequentSolidFillRect(rect->dx, rect->dy, rect->width, rect->height);
      CRITEND
      SiS300Sync();
#endif
   } else {
#ifdef CONFIG_FB_SIS_315
      CRITBEGIN
      SiS310SetupForSolidFill(col, myrops[rect->rop], 0);
      SiS310SubsequentSolidFillRect(rect->dx, rect->dy, rect->width, rect->height);
      CRITEND
      SiS310Sync();
#endif
   }

}

void fbcon_sis_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
   int xdir, ydir;
   CRITFLAGS

   TWDEBUG("Inside sis_copyarea");
   if(!ivideo.accel) {
   	cfb_copyarea(info, area);
	return;
   }

   if(!area->width || !area->height)
   	return;

   if(area->sx < area->dx) xdir = 0;
   else                    xdir = 1;
   if(area->sy < area->dy) ydir = 0;
   else                    ydir = 1;

   if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
      CRITBEGIN
      SiS300SetupForScreenToScreenCopy(xdir, ydir, 3, 0, -1);
      SiS300SubsequentScreenToScreenCopy(area->sx, area->sy, area->dx, area->dy, area->width, area->height);
      CRITEND
      SiS300Sync();
#endif
   } else {
#ifdef CONFIG_FB_SIS_315
      CRITBEGIN
      SiS310SetupForScreenToScreenCopy(xdir, ydir, 3, 0, -1);
      SiS310SubsequentScreenToScreenCopy(area->sx, area->sy, area->dx, area->dy, area->width, area->height);
      CRITEND
      SiS310Sync();
#endif
   }
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)  /* -------------- 2.4 --------------- */

void fbcon_sis_bmove(struct display *p, int srcy, int srcx,
			    int dsty, int dstx, int height, int width)
{
        int xdir, ydir;
	CRITFLAGS

	if(!ivideo.accel) {
	    switch(ivideo.video_bpp) {
	    case 8:
#ifdef FBCON_HAS_CFB8
	       fbcon_cfb8_bmove(p, srcy, srcx, dsty, dstx, height, width);
#endif
	       break;
	    case 16:
#ifdef FBCON_HAS_CFB16
	       fbcon_cfb16_bmove(p, srcy, srcx, dsty, dstx, height, width);
#endif
	       break;
	    case 32:
#ifdef FBCON_HAS_CFB32
	       fbcon_cfb32_bmove(p, srcy, srcx, dsty, dstx, height, width);
#endif
	       break;
            }
	    return;
	}

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	dstx *= fontwidth(p);
	dsty *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	if(srcx < dstx) xdir = 0;
	else            xdir = 1;
	if(srcy < dsty) ydir = 0;
	else            ydir = 1;

	if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
	   CRITBEGIN
	   SiS300SetupForScreenToScreenCopy(xdir, ydir, 3, 0, -1);
	   SiS300SubsequentScreenToScreenCopy(srcx, srcy, dstx, dsty, width, height);
	   CRITEND
	   SiS300Sync();
#endif
	} else {
#ifdef CONFIG_FB_SIS_315
	   CRITBEGIN
	   SiS310SetupForScreenToScreenCopy(xdir, ydir, 3, 0, -1);
	   SiS310SubsequentScreenToScreenCopy(srcx, srcy, dstx, dsty, width, height);
	   CRITEND
	   SiS310Sync();
#endif
	}
}

static void fbcon_sis_clear(struct vc_data *conp, struct display *p,
			int srcy, int srcx, int height, int width, int color)
{
	CRITFLAGS

	srcx *= fontwidth(p);
	srcy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
	   CRITBEGIN
	   SiS300SetupForSolidFill(color, 3, 0);
	   SiS300SubsequentSolidFillRect(srcx, srcy, width, height);
	   CRITEND
	   SiS300Sync();
#endif
	} else {
#ifdef CONFIG_FB_SIS_315
	   CRITBEGIN
	   SiS310SetupForSolidFill(color, 3, 0);
	   SiS310SubsequentSolidFillRect(srcx, srcy, width, height);
	   CRITEND
	   SiS310Sync();
#endif
	}
}

void fbcon_sis_clear8(struct vc_data *conp, struct display *p,
			int srcy, int srcx, int height, int width)
{
	u32 bgx;

	if(!ivideo.accel) {
#ifdef FBCON_HAS_CFB8
	    fbcon_cfb8_clear(conp, p, srcy, srcx, height, width);
#endif
	    return;
	}

	bgx = attr_bgcol_ec(p, conp);
	fbcon_sis_clear(conp, p, srcy, srcx, height, width, bgx);
}

void fbcon_sis_clear16(struct vc_data *conp, struct display *p,
			int srcy, int srcx, int height, int width)
{
	u32 bgx;
	if(!ivideo.accel) {
#ifdef FBCON_HAS_CFB16
	    fbcon_cfb16_clear(conp, p, srcy, srcx, height, width);
#endif
	    return;
	}

	bgx = ((u_int16_t*)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	fbcon_sis_clear(conp, p, srcy, srcx, height, width, bgx);
}

void fbcon_sis_clear32(struct vc_data *conp, struct display *p,
			int srcy, int srcx, int height, int width)
{
	u32 bgx;

	if(!ivideo.accel) {
#ifdef FBCON_HAS_CFB32
	    fbcon_cfb32_clear(conp, p, srcy, srcx, height, width);
#endif
	    return;
	}

	bgx = ((u_int32_t*)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	fbcon_sis_clear(conp, p, srcy, srcx, height, width, bgx);
}

void fbcon_sis_revc(struct display *p, int srcx, int srcy)
{
	CRITFLAGS

	if(!ivideo.accel) {
	    switch(ivideo.video_bpp) {
	    case 16:
#ifdef FBCON_HAS_CFB16
	       fbcon_cfb16_revc(p, srcx, srcy);
#endif
	       break;
	    case 32:
#ifdef FBCON_HAS_CFB32
	       fbcon_cfb32_revc(p, srcx, srcy);
#endif
	       break;
            }
	    return;
	}

	srcx *= fontwidth(p);
	srcy *= fontheight(p);

	if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
	   CRITBEGIN
	   SiS300SetupForSolidFill(0, 0x0a, 0);
	   SiS300SubsequentSolidFillRect(srcx, srcy, fontwidth(p), fontheight(p));
	   CRITEND
	   SiS300Sync();
#endif
	} else {
#ifdef CONFIG_FB_SIS_315
	   CRITBEGIN
	   SiS310SetupForSolidFill(0, 0x0a, 0);
	   SiS310SubsequentSolidFillRect(srcx, srcy, fontwidth(p), fontheight(p));
	   CRITEND
	   SiS310Sync();
#endif
	}
}

#ifdef FBCON_HAS_CFB8
struct display_switch fbcon_sis8 = {
	.setup			= fbcon_cfb8_setup,
	.bmove			= fbcon_sis_bmove,
	.clear			= fbcon_sis_clear8,
	.putc			= fbcon_cfb8_putc,
	.putcs			= fbcon_cfb8_putcs,
	.revc			= fbcon_cfb8_revc,
	.clear_margins		= fbcon_cfb8_clear_margins,
	.fontwidthmask		= FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB16
struct display_switch fbcon_sis16 = {
	.setup			= fbcon_cfb16_setup,
	.bmove			= fbcon_sis_bmove,
	.clear			= fbcon_sis_clear16,
	.putc			= fbcon_cfb16_putc,
	.putcs			= fbcon_cfb16_putcs,
	.revc			= fbcon_sis_revc,
	.clear_margins		= fbcon_cfb16_clear_margins,
	.fontwidthmask		= FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif
#ifdef FBCON_HAS_CFB32
struct display_switch fbcon_sis32 = {
	.setup			= fbcon_cfb32_setup,
	.bmove			= fbcon_sis_bmove,
	.clear			= fbcon_sis_clear32,
	.putc			= fbcon_cfb32_putc,
	.putcs			= fbcon_cfb32_putcs,
	.revc			= fbcon_sis_revc,
	.clear_margins		= fbcon_cfb32_clear_margins,
	.fontwidthmask		= FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};
#endif

#endif /* KERNEL VERSION */


