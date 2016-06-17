/*
 * linux/drivers/video/neofb.c -- NeoMagic Framebuffer Driver
 *
 * Copyright (c) 2001  Denis Oliver Kropp <dok@convergence.de>
 *
 *
 * Card specific code is based on XFree86's neomagic driver.
 * Framebuffer framework code is based on code of cyber2000fb.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 *
 * 0.3.2
 *  - got rid of all floating point (dok)
 *
 * 0.3.1
 *  - added module license (dok)
 *
 * 0.3
 *  - hardware accelerated clear and move for 2200 and above (dok)
 *  - maximum allowed dotclock is handled now (dok)
 *
 * 0.2.1
 *  - correct panning after X usage (dok)
 *  - added module and kernel parameters (dok)
 *  - no stretching if external display is enabled (dok)
 *
 * 0.2
 *  - initial version (dok)
 *
 *
 * TODO
 * - ioctl for internal/external switching
 * - blanking
 * - 32bit depth support, maybe impossible
 * - disable pan-on-sync, need specs
 *
 * BUGS
 * - white margin on bootup like with tdfxfb (colormap problem?)
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "neofb.h"


#define NEOFB_VERSION "0.3.2"

/* --------------------------------------------------------------------- */

static int disabled   = 0;
static int internal   = 0;
static int external   = 0;
static int nostretch  = 0;
static int nopciburst = 0;


#ifdef MODULE

MODULE_AUTHOR("(c) 2001-2002  Denis Oliver Kropp <dok@convergence.de>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FBDev driver for NeoMagic PCI Chips");
MODULE_PARM(disabled, "i");
MODULE_PARM_DESC(disabled, "Disable this driver's initialization.");
MODULE_PARM(internal, "i");
MODULE_PARM_DESC(internal, "Enable output on internal LCD Display.");
MODULE_PARM(external, "i");
MODULE_PARM_DESC(external, "Enable output on external CRT.");
MODULE_PARM(nostretch, "i");
MODULE_PARM_DESC(nostretch, "Disable stretching of modes smaller than LCD.");
MODULE_PARM(nopciburst, "i");
MODULE_PARM_DESC(nopciburst, "Disable PCI burst mode.");

#endif


/* --------------------------------------------------------------------- */

static biosMode bios8[] = {	
    { 320, 240, 0x40 },
    { 300, 400, 0x42 },    
    { 640, 400, 0x20 },
    { 640, 480, 0x21 },
    { 800, 600, 0x23 },
    { 1024, 768, 0x25 },
};

static biosMode bios16[] = {
    { 320, 200, 0x2e },
    { 320, 240, 0x41 },
    { 300, 400, 0x43 },
    { 640, 480, 0x31 },
    { 800, 600, 0x34 },
    { 1024, 768, 0x37 },
};

static biosMode bios24[] = {
    { 640, 480, 0x32 },
    { 800, 600, 0x35 },
    { 1024, 768, 0x38 }
};

#ifdef NO_32BIT_SUPPORT_YET
/* FIXME: guessed values, wrong */
static biosMode bios32[] = {
    { 640, 480, 0x33 },
    { 800, 600, 0x36 },
    { 1024, 768, 0x39 }
    };
#endif

static int neoFindMode (int xres, int yres, int depth)
{
  int xres_s;
  int i, size;
  biosMode *mode;

  switch (depth)
    {
    case 8:
      size = sizeof(bios8) / sizeof(biosMode);
      mode = bios8;
      break;
    case 16:
      size = sizeof(bios16) / sizeof(biosMode);
      mode = bios16;
      break;
    case 24:
      size = sizeof(bios24) / sizeof(biosMode);
      mode = bios24;
      break;
#ifdef NO_32BIT_SUPPORT_YET
    case 32:
      size = sizeof(bios32) / sizeof(biosMode);
      mode = bios32;
      break;
#endif
    default:
      return 0;
    }

  for (i = 0; i < size; i++)
    {
      if (xres <= mode[i].x_res)
	{
	  xres_s = mode[i].x_res;
	  for (; i < size; i++)
	    {
	      if (mode[i].x_res != xres_s)
		return mode[i-1].mode;
	      if (yres <= mode[i].y_res)
		return mode[i].mode;
	    }
	}
    }
  return mode[size - 1].mode;
}

/* -------------------- Hardware specific routines ------------------------- */

/*
 * Hardware Acceleration for Neo2200+
 */
static inline void neo2200_wait_idle (struct neofb_info *fb)
{
  int waitcycles;

  while (fb->neo2200->bltStat & 1)
    waitcycles++;
}

static inline void neo2200_wait_fifo (struct neofb_info *fb,
                                      int requested_fifo_space)
{
  //  ndev->neo.waitfifo_calls++;
  //  ndev->neo.waitfifo_sum += requested_fifo_space;

  /* FIXME: does not work
  if (neo_fifo_space < requested_fifo_space)
    {
      neo_fifo_waitcycles++;

      while (1)
    {
      neo_fifo_space = (neo2200->bltStat >> 8);
      if (neo_fifo_space >= requested_fifo_space)
        break;
    }
    }
  else
    {
      neo_fifo_cache_hits++;
    }

  neo_fifo_space -= requested_fifo_space;
  */

  neo2200_wait_idle (fb);
}

static inline void neo2200_accel_init (struct neofb_info        *fb,
				       struct fb_var_screeninfo *var)
{
  Neo2200 *neo2200 = fb->neo2200;
  u32 bltMod, pitch;

  neo2200_wait_idle (fb);

  switch (var->bits_per_pixel)
    {
    case 8:
      bltMod = NEO_MODE1_DEPTH8;
      pitch  = var->xres_virtual;
      break;
    case 15:
    case 16:
      bltMod = NEO_MODE1_DEPTH16;
      pitch  = var->xres_virtual * 2;
      break;
    default:
      printk( KERN_ERR "neofb: neo2200_accel_init: unexpected bits per pixel!\n" );
      return;
    }

  neo2200->bltStat = bltMod << 16;
  neo2200->pitch   = (pitch << 16) | pitch;
}

static void neo2200_accel_setup (struct display *p)
{
  struct neofb_info        *fb  = (struct neofb_info *)p->fb_info;
  struct fb_var_screeninfo *var = &p->fb_info->var;

  fb->dispsw->setup(p);

  neo2200_accel_init (fb, var);
}

static void
neo2200_accel_bmove (struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
  struct neofb_info *fb = (struct neofb_info *)p->fb_info;
  struct fb_var_screeninfo *var = &p->fb_info->var;
  Neo2200 *neo2200 = fb->neo2200;
  u_long src, dst;
  int bpp, pitch, inc_y;
  u_int fh, fw;

  if (sx != dx)
    {
      neo2200_wait_idle (fb);
      fb->dispsw->bmove(p, sy, sx, dy, dx, height, width);
      return;
    }

  bpp    = (var->bits_per_pixel+7) / 8;
  pitch  = var->xres_virtual * bpp;

  fw     = fontwidth(p);
  sx    *= fw * bpp;
  dx    *= fw * bpp;
  width *= fw;

  fh     = fontheight(p);
  sy    *= fh;
  dy    *= fh;

  if (sy > dy)
    inc_y = fh;
  else
    {
      inc_y  = -fh;
      sy    += (height - 1) * fh;
      dy    += (height - 1) * fh;
    }

  neo2200_wait_fifo (fb, 1);

  /* set blt control */
  neo2200->bltCntl = NEO_BC3_FIFO_EN      |
                     NEO_BC3_SKIP_MAPPING |  0x0c0000;

  while (height--)
    {
      src = sx + sy * pitch;
      dst = dx + dy * pitch;

      neo2200_wait_fifo (fb, 3);

      neo2200->srcStart = src;
      neo2200->dstStart = dst;
      neo2200->xyExt = (fh << 16) | (width & 0xffff);

      sy += inc_y;
      dy += inc_y;
    }
}

static void
neo2200_accel_clear (struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
  struct neofb_info *fb = (struct neofb_info *)p->fb_info;
  struct fb_var_screeninfo *var = &p->fb_info->var;
  Neo2200 *neo2200 = fb->neo2200;
  u_long dst;
  u_int fw, fh;
  u32 bgx = attr_bgcol_ec(p, conp);

  fw = fontwidth(p);
  fh = fontheight(p);

  dst    = sx * fw + sy * var->xres_virtual * fh;
  width  = width * fw;
  height = height * fh;

  neo2200_wait_fifo (fb, 4);

  /* set blt control */
  neo2200->bltCntl  = NEO_BC3_FIFO_EN      |
                      NEO_BC0_SRC_IS_FG    |
                      NEO_BC3_SKIP_MAPPING |  0x0c0000;

  switch (var->bits_per_pixel)
    {
    case 8:
      neo2200->fgColor = bgx;
      break;
    case 16:
      neo2200->fgColor = ((u16 *)(p->fb_info)->pseudo_palette)[bgx];
      break;
    }

  neo2200->dstStart = dst * ((var->bits_per_pixel+7) / 8);

  neo2200->xyExt    = (height << 16) | (width & 0xffff);
}

static void
neo2200_accel_putc (struct vc_data *conp, struct display *p, int c,
		    int yy, int xx)
{
  struct neofb_info *fb = (struct neofb_info *)p->fb_info;

  neo2200_wait_idle (fb);
  fb->dispsw->putc(conp, p, c, yy, xx);
}

static void
neo2200_accel_putcs (struct vc_data *conp, struct display *p,
		     const unsigned short *s, int count, int yy, int xx)
{
  struct neofb_info *fb = (struct neofb_info *)p->fb_info;

  neo2200_wait_idle (fb);
  fb->dispsw->putcs(conp, p, s, count, yy, xx);
}

static void neo2200_accel_revc (struct display *p, int xx, int yy)
{
  struct neofb_info *fb = (struct neofb_info *)p->fb_info;
	
  neo2200_wait_idle (fb);
  fb->dispsw->revc (p, xx, yy);
}

static void
neo2200_accel_clear_margins (struct vc_data *conp, struct display *p,
			     int bottom_only)
{
  struct neofb_info *fb = (struct neofb_info *)p->fb_info;

  fb->dispsw->clear_margins (conp, p, bottom_only);
}

static struct display_switch fbcon_neo2200_accel = {
  setup:		neo2200_accel_setup,
  bmove:		neo2200_accel_bmove,
  clear:		neo2200_accel_clear,
  putc:			neo2200_accel_putc,
  putcs:		neo2200_accel_putcs,
  revc:			neo2200_accel_revc,
  clear_margins:	neo2200_accel_clear_margins,
  fontwidthmask:	FONTWIDTH(8)|FONTWIDTH(16)
};


/* --------------------------------------------------------------------- */

/*
 *    Set a single color register. Return != 0 for invalid regno.
 */
static int neo_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *fb)
{
  struct neofb_info *info = (struct neofb_info *)fb;

  if (regno >= NR_PALETTE)
    return -EINVAL;

  info->palette[regno].red    = red;
  info->palette[regno].green  = green;
  info->palette[regno].blue   = blue;
  info->palette[regno].transp = transp;

  switch (fb->var.bits_per_pixel)
    {
#ifdef FBCON_HAS_CFB8
    case 8:
      outb(regno, 0x3c8);

      outb(red   >> 10, 0x3c9);
      outb(green >> 10, 0x3c9);
      outb(blue  >> 10, 0x3c9);
      break;
#endif

#ifdef FBCON_HAS_CFB16
    case 16:
      if (regno < 16)
	((u16 *)fb->pseudo_palette)[regno] = ((red   & 0xf800)      ) |
	                                     ((green & 0xfc00) >>  5) |
                                             ((blue  & 0xf800) >> 11);
      break;
#endif

#ifdef FBCON_HAS_CFB24
    case 24:
      if (regno < 16)
	((u32 *)fb->pseudo_palette)[regno] = ((red   & 0xff00) << 8) |
                                             ((green & 0xff00)     ) |
	                                     ((blue  & 0xff00) >> 8);
      break;
#endif

#ifdef NO_32BIT_SUPPORT_YET
#ifdef FBCON_HAS_CFB32
    case 32:
      if (regno < 16)
	((u32 *)fb->pseudo_palette)[regno] = ((transp & 0xff00) << 16) |
	                                     ((red    & 0xff00) <<  8) |
                                             ((green  & 0xff00)      ) |
	                                     ((blue   & 0xff00) >>  8);
      break;
#endif
#endif

    default:
      return 1;
    }

  return 0;
}

static void vgaHWLock (void)
{
  /* Protect CRTC[0-7] */
  VGAwCR (0x11, VGArCR (0x11) | 0x80);
}

static void vgaHWUnlock (void)
{
  /* Unprotect CRTC[0-7] */
  VGAwCR (0x11, VGArCR (0x11) & ~0x80);
}

static void neoLock (void)
{
  VGAwGR (0x09, 0x00);
  vgaHWLock();
}

static void neoUnlock (void)
{
  vgaHWUnlock();
  VGAwGR (0x09, 0x26);
}

/*
 * vgaHWSeqReset
 *      perform a sequencer reset.
 */
void
vgaHWSeqReset(int start)
{
  if (start)
    VGAwSEQ (0x00, 0x01);		/* Synchronous Reset */
  else
    VGAwSEQ (0x00, 0x03);		/* End Reset */
}

void
vgaHWProtect(int on)
{
  unsigned char tmp;
  
  if (on)
    {
      /*
       * Turn off screen and disable sequencer.
       */
      tmp = VGArSEQ (0x01);

      vgaHWSeqReset (1);			/* start synchronous reset */
      VGAwSEQ (0x01, tmp | 0x20);	/* disable the display */

      VGAenablePalette();
    }
  else
    {
      /*
       * Reenable sequencer, then turn on screen.
       */
  
      tmp = VGArSEQ (0x01);

      VGAwSEQ (0x01, tmp & ~0x20);	/* reenable display */
      vgaHWSeqReset (0);		/* clear synchronousreset */

      VGAdisablePalette();
    }
}

static void vgaHWRestore (const struct neofb_info *info,
			  const struct neofb_par  *par)
{
  int i;

  VGAwMISC (par->MiscOutReg);

  for (i = 1; i < 5; i++)
    VGAwSEQ (i, par->Sequencer[i]);
  
  /* Ensure CRTC registers 0-7 are unlocked by clearing bit 7 or CRTC[17] */
  VGAwCR (17, par->CRTC[17] & ~0x80);

  for (i = 0; i < 25; i++)
    VGAwCR (i, par->CRTC[i]);

  for (i = 0; i < 9; i++)
    VGAwGR (i, par->Graphics[i]);

  VGAenablePalette();

  for (i = 0; i < 21; i++)
    VGAwATTR (i, par->Attribute[i]);

  VGAdisablePalette();
}

static void neofb_set_par (struct neofb_info       *info,
			   const struct neofb_par  *par)
{
  unsigned char temp;
  int i;
  int clock_hi = 0;
    
  DBG("neofb_set_par");

  neoUnlock();

  vgaHWProtect (1);		/* Blank the screen */

  /* linear colormap for non palettized modes */
  switch (par->depth)
    {
    case 8:
      break;
    case 16:
      for (i=0; i<64; i++)
	{
	  outb(i, 0x3c8);
	  
	  outb(i << 1, 0x3c9);
	  outb(i, 0x3c9);
	  outb(i << 1, 0x3c9);
	}
      break;
    case 24:
#ifdef NO_32BIT_SUPPORT_YET
    case 32:
#endif
      for (i=0; i<256; i++)
	{
	  outb(i, 0x3c8);
	  
	  outb(i, 0x3c9);
	  outb(i, 0x3c9);
	  outb(i, 0x3c9);
	}
      break;
    }
    
  /* alread unlocked above */
  /* BOGUS  VGAwGR (0x09, 0x26);*/
    
  /* don't know what this is, but it's 0 from bootup anyway */
  VGAwGR (0x15, 0x00);

  /* was set to 0x01 by my bios in text and vesa modes */
  VGAwGR (0x0A, par->GeneralLockReg);

  /*
   * The color mode needs to be set before calling vgaHWRestore
   * to ensure the DAC is initialized properly.
   *
   * NOTE: Make sure we don't change bits make sure we don't change
   * any reserved bits.
   */
  temp = VGArGR(0x90);
  switch (info->accel)
    {
    case FB_ACCEL_NEOMAGIC_NM2070:
      temp &= 0xF0; /* Save bits 7:4 */
      temp |= (par->ExtColorModeSelect & ~0xF0);
      break;
    case FB_ACCEL_NEOMAGIC_NM2090:
    case FB_ACCEL_NEOMAGIC_NM2093:
    case FB_ACCEL_NEOMAGIC_NM2097:
    case FB_ACCEL_NEOMAGIC_NM2160:
    case FB_ACCEL_NEOMAGIC_NM2200:
    case FB_ACCEL_NEOMAGIC_NM2230:
    case FB_ACCEL_NEOMAGIC_NM2360:
    case FB_ACCEL_NEOMAGIC_NM2380:
      temp &= 0x70; /* Save bits 6:4 */
      temp |= (par->ExtColorModeSelect & ~0x70);
      break;
    }

  VGAwGR(0x90,temp);

  /*
   * In some rare cases a lockup might occur if we don't delay
   * here. (Reported by Miles Lane)
   */
  //mdelay(200);

  /*
   * Disable horizontal and vertical graphics and text expansions so
   * that vgaHWRestore works properly.
   */
  temp = VGArGR(0x25);
  temp &= 0x39;
  VGAwGR (0x25, temp);

  /*
   * Sleep for 200ms to make sure that the two operations above have
   * had time to take effect.
   */
  mdelay(200);

  /*
   * This function handles restoring the generic VGA registers.  */
  vgaHWRestore (info, par);


  VGAwGR(0x0E, par->ExtCRTDispAddr);
  VGAwGR(0x0F, par->ExtCRTOffset);
  temp = VGArGR(0x10);
  temp &= 0x0F; /* Save bits 3:0 */
  temp |= (par->SysIfaceCntl1 & ~0x0F); /* VESA Bios sets bit 1! */
  VGAwGR(0x10, temp);

  VGAwGR(0x11, par->SysIfaceCntl2);
  VGAwGR(0x15, 0 /*par->SingleAddrPage*/);
  VGAwGR(0x16, 0 /*par->DualAddrPage*/);

  temp = VGArGR(0x20);
  switch (info->accel)
    {
    case FB_ACCEL_NEOMAGIC_NM2070:
      temp &= 0xFC; /* Save bits 7:2 */
      temp |= (par->PanelDispCntlReg1 & ~0xFC);
      break;
    case FB_ACCEL_NEOMAGIC_NM2090:
    case FB_ACCEL_NEOMAGIC_NM2093:
    case FB_ACCEL_NEOMAGIC_NM2097:
    case FB_ACCEL_NEOMAGIC_NM2160:
      temp &= 0xDC; /* Save bits 7:6,4:2 */
      temp |= (par->PanelDispCntlReg1 & ~0xDC);
      break;
    case FB_ACCEL_NEOMAGIC_NM2200:
    case FB_ACCEL_NEOMAGIC_NM2230:
    case FB_ACCEL_NEOMAGIC_NM2360:
    case FB_ACCEL_NEOMAGIC_NM2380:
      temp &= 0x98; /* Save bits 7,4:3 */
      temp |= (par->PanelDispCntlReg1 & ~0x98);
      break;
    }
  VGAwGR(0x20, temp);

  temp = VGArGR(0x25);
  temp &= 0x38; /* Save bits 5:3 */
  temp |= (par->PanelDispCntlReg2 & ~0x38);
  VGAwGR(0x25, temp);

  if (info->accel != FB_ACCEL_NEOMAGIC_NM2070)
    {
      temp = VGArGR(0x30);
      temp &= 0xEF; /* Save bits 7:5 and bits 3:0 */
      temp |= (par->PanelDispCntlReg3 & ~0xEF);
      VGAwGR(0x30, temp);
    }

  VGAwGR(0x28, par->PanelVertCenterReg1);
  VGAwGR(0x29, par->PanelVertCenterReg2);
  VGAwGR(0x2a, par->PanelVertCenterReg3);

  if (info->accel != FB_ACCEL_NEOMAGIC_NM2070)
    {
      VGAwGR(0x32, par->PanelVertCenterReg4);
      VGAwGR(0x33, par->PanelHorizCenterReg1);
      VGAwGR(0x34, par->PanelHorizCenterReg2);
      VGAwGR(0x35, par->PanelHorizCenterReg3);
    }

  if (info->accel == FB_ACCEL_NEOMAGIC_NM2160)
    VGAwGR(0x36, par->PanelHorizCenterReg4);

  if (info->accel == FB_ACCEL_NEOMAGIC_NM2200 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2230 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2360 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2380)
    {
      VGAwGR(0x36, par->PanelHorizCenterReg4);
      VGAwGR(0x37, par->PanelVertCenterReg5);
      VGAwGR(0x38, par->PanelHorizCenterReg5);

      clock_hi = 1;
    }

  /* Program VCLK3 if needed. */
  if (par->ProgramVCLK
      && ((VGArGR(0x9B) != par->VCLK3NumeratorLow)
	  || (VGArGR(0x9F) !=  par->VCLK3Denominator)
	  || (clock_hi && ((VGArGR(0x8F) & ~0x0f)
			   != (par->VCLK3NumeratorHigh & ~0x0F)))))
    {
      VGAwGR(0x9B, par->VCLK3NumeratorLow);
      if (clock_hi)
	{
	  temp = VGArGR(0x8F);
	  temp &= 0x0F; /* Save bits 3:0 */
	  temp |= (par->VCLK3NumeratorHigh & ~0x0F);
	  VGAwGR(0x8F, temp);
	}
      VGAwGR(0x9F, par->VCLK3Denominator);
    }

  if (par->biosMode)
    VGAwCR(0x23, par->biosMode);
    
  VGAwGR (0x93, 0xc0); /* Gives 5x faster framebuffer writes !!! */

  /* Program vertical extension register */
  if (info->accel == FB_ACCEL_NEOMAGIC_NM2200 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2230 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2360 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2380)
    {
      VGAwCR(0x70, par->VerticalExt);
    }


  vgaHWProtect (0);		/* Turn on screen */

  /* Calling this also locks offset registers required in update_start */
  neoLock();
}

static void neofb_update_start (struct neofb_info *info, struct fb_var_screeninfo *var)
{
  int oldExtCRTDispAddr;
  int Base; 

  DBG("neofb_update_start");

  Base = (var->yoffset * var->xres_virtual + var->xoffset) >> 2;
  Base *= (var->bits_per_pixel + 7) / 8;

  neoUnlock();

  /*
   * These are the generic starting address registers.
   */
  VGAwCR(0x0C, (Base & 0x00FF00) >> 8);
  VGAwCR(0x0D, (Base & 0x00FF));

  /*
   * Make sure we don't clobber some other bits that might already
   * have been set. NOTE: NM2200 has a writable bit 3, but it shouldn't
   * be needed.
   */
  oldExtCRTDispAddr = VGArGR(0x0E);
  VGAwGR(0x0E,(((Base >> 16) & 0x0f) | (oldExtCRTDispAddr & 0xf0)));

  neoLock();
}

/*
 * Set the Colormap
 */
static int neofb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *fb)
{
  struct neofb_info *info = (struct neofb_info *)fb;
  struct display* disp = (con < 0) ? fb->disp : (fb_display + con);
  struct fb_cmap *dcmap = &disp->cmap;
  int err = 0;

  /* no colormap allocated? */
  if (!dcmap->len)
    {
      int size;

      if (fb->var.bits_per_pixel == 8)
	size = NR_PALETTE;
      else
	size = 32;

      err = fb_alloc_cmap (dcmap, size, 0);
    }

  /*
   * we should be able to remove this test once fbcon has been
   * "improved" --rmk
   */
  if (!err && con == info->currcon)
    {
      err = fb_set_cmap (cmap, kspc, neo_setcolreg, fb);
      dcmap = &fb->cmap;
    }

  if (!err)
    fb_copy_cmap (cmap, dcmap, kspc ? 0 : 1);

  return err;
}

/*
 * neoCalcVCLK --
 *
 * Determine the closest clock frequency to the one requested.
 */
#define REF_FREQ 0xe517  /* 14.31818 in 20.12 fixed point */
#define MAX_N 127
#define MAX_D 31
#define MAX_F 1

static void neoCalcVCLK (const struct neofb_info *info, struct neofb_par *par, long freq)
{
  int n, d, f;
  int n_best = 0, d_best = 0, f_best = 0;
  long f_best_diff = (0x7ffff << 12); /* 20.12 */
  long f_target = (freq << 12) / 1000; /* 20.12 */

  for (f = 0; f <= MAX_F; f++)
    for (n = 0; n <= MAX_N; n++)
      for (d = 0; d <= MAX_D; d++)
	{
          long f_out;  /* 20.12 */
          long f_diff; /* 20.12 */

	  f_out = ((((n+1) << 12)  /  ((d+1)*(1<<f))) >> 12)  *  REF_FREQ;
	  f_diff = abs(f_out-f_target);
	  if (f_diff < f_best_diff)
	    {
	      f_best_diff = f_diff;
	      n_best = n;
	      d_best = d;
	      f_best = f;
	    }
	}

  if (info->accel == FB_ACCEL_NEOMAGIC_NM2200 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2230 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2360 ||
      info->accel == FB_ACCEL_NEOMAGIC_NM2380)
    {
      /* NOT_DONE:  We are trying the full range of the 2200 clock.
	 We should be able to try n up to 2047 */
      par->VCLK3NumeratorLow  = n_best;
      par->VCLK3NumeratorHigh = (f_best << 7);
    }
  else
    par->VCLK3NumeratorLow  = n_best | (f_best << 7);

  par->VCLK3Denominator = d_best;

#ifdef NEOFB_DEBUG
  printk ("neoVCLK: f:%d NumLow=%d NumHi=%d Den=%d Df=%d\n",
	  f_target >> 12,
	  par->VCLK3NumeratorLow,
	  par->VCLK3NumeratorHigh,
	  par->VCLK3Denominator,
	  f_best_diff >> 12);
#endif
}

/*
 * vgaHWInit --
 *      Handle the initialization, etc. of a screen.
 *      Return FALSE on failure.
 */

static int vgaHWInit (const struct fb_var_screeninfo *var,
		      const struct neofb_info        *info,
		      struct neofb_par               *par,
		      struct xtimings                *timings)
{
  par->MiscOutReg = 0x23;

  if (!(timings->sync & FB_SYNC_HOR_HIGH_ACT))
    par->MiscOutReg |= 0x40;

  if (!(timings->sync & FB_SYNC_VERT_HIGH_ACT))
    par->MiscOutReg |= 0x80;
    
  /*
   * Time Sequencer
   */
  par->Sequencer[0] = 0x00;
  par->Sequencer[1] = 0x01;
  par->Sequencer[2] = 0x0F;
  par->Sequencer[3] = 0x00;                             /* Font select */
  par->Sequencer[4] = 0x0E;                             /* Misc */

  /*
   * CRTC Controller
   */
  par->CRTC[0]  = (timings->HTotal >> 3) - 5;
  par->CRTC[1]  = (timings->HDisplay >> 3) - 1;
  par->CRTC[2]  = (timings->HDisplay >> 3) - 1;
  par->CRTC[3]  = (((timings->HTotal >> 3) - 1) & 0x1F) | 0x80;
  par->CRTC[4]  = (timings->HSyncStart >> 3);
  par->CRTC[5]  = ((((timings->HTotal >> 3) - 1) & 0x20) << 2)
    | (((timings->HSyncEnd >> 3)) & 0x1F);
  par->CRTC[6]  = (timings->VTotal - 2) & 0xFF;
  par->CRTC[7]  = (((timings->VTotal - 2) & 0x100) >> 8)
    | (((timings->VDisplay - 1) & 0x100) >> 7)
    | ((timings->VSyncStart & 0x100) >> 6)
    | (((timings->VDisplay - 1) & 0x100) >> 5)
    | 0x10
    | (((timings->VTotal - 2) & 0x200)   >> 4)
    | (((timings->VDisplay - 1) & 0x200) >> 3)
    | ((timings->VSyncStart & 0x200) >> 2);
  par->CRTC[8]  = 0x00;
  par->CRTC[9]  = (((timings->VDisplay - 1) & 0x200) >> 4) | 0x40;

  if (timings->dblscan)
    par->CRTC[9] |= 0x80;

  par->CRTC[10] = 0x00;
  par->CRTC[11] = 0x00;
  par->CRTC[12] = 0x00;
  par->CRTC[13] = 0x00;
  par->CRTC[14] = 0x00;
  par->CRTC[15] = 0x00;
  par->CRTC[16] = timings->VSyncStart & 0xFF;
  par->CRTC[17] = (timings->VSyncEnd & 0x0F) | 0x20;
  par->CRTC[18] = (timings->VDisplay - 1) & 0xFF;
  par->CRTC[19] = var->xres_virtual >> 4;
  par->CRTC[20] = 0x00;
  par->CRTC[21] = (timings->VDisplay - 1) & 0xFF; 
  par->CRTC[22] = (timings->VTotal - 1) & 0xFF;
  par->CRTC[23] = 0xC3;
  par->CRTC[24] = 0xFF;

  /*
   * are these unnecessary?
   * vgaHWHBlankKGA(mode, regp, 0, KGA_FIX_OVERSCAN | KGA_ENABLE_ON_ZERO);
   * vgaHWVBlankKGA(mode, regp, 0, KGA_FIX_OVERSCAN | KGA_ENABLE_ON_ZERO);
   */

  /*
   * Graphics Display Controller
   */
  par->Graphics[0] = 0x00;
  par->Graphics[1] = 0x00;
  par->Graphics[2] = 0x00;
  par->Graphics[3] = 0x00;
  par->Graphics[4] = 0x00;
  par->Graphics[5] = 0x40;
  par->Graphics[6] = 0x05;   /* only map 64k VGA memory !!!! */
  par->Graphics[7] = 0x0F;
  par->Graphics[8] = 0xFF;
  

  par->Attribute[0]  = 0x00; /* standard colormap translation */
  par->Attribute[1]  = 0x01;
  par->Attribute[2]  = 0x02;
  par->Attribute[3]  = 0x03;
  par->Attribute[4]  = 0x04;
  par->Attribute[5]  = 0x05;
  par->Attribute[6]  = 0x06;
  par->Attribute[7]  = 0x07;
  par->Attribute[8]  = 0x08;
  par->Attribute[9]  = 0x09;
  par->Attribute[10] = 0x0A;
  par->Attribute[11] = 0x0B;
  par->Attribute[12] = 0x0C;
  par->Attribute[13] = 0x0D;
  par->Attribute[14] = 0x0E;
  par->Attribute[15] = 0x0F;
  par->Attribute[16] = 0x41;
  par->Attribute[17] = 0xFF;
  par->Attribute[18] = 0x0F;
  par->Attribute[19] = 0x00;
  par->Attribute[20] = 0x00;

  return 0;
}

static int neofb_decode_var (struct fb_var_screeninfo        *var,
                             const struct neofb_info         *info,
                             struct neofb_par                *par)
{
  struct xtimings timings;
  int lcd_stretch;
  int hoffset, voffset;
  int memlen, vramlen;
  int mode_ok = 0;
  unsigned int pixclock = var->pixclock;

  DBG("neofb_decode_var");

  if (!pixclock) pixclock = 10000;	/* 10ns = 100MHz */
  timings.pixclock = 1000000000 / pixclock;
  if (timings.pixclock < 1) timings.pixclock = 1;
  timings.dblscan = var->vmode & FB_VMODE_DOUBLE;
  timings.interlaced = var->vmode & FB_VMODE_INTERLACED;
  timings.HDisplay = var->xres;
  timings.HSyncStart = timings.HDisplay + var->right_margin;
  timings.HSyncEnd = timings.HSyncStart + var->hsync_len;
  timings.HTotal = timings.HSyncEnd + var->left_margin;
  timings.VDisplay = var->yres;
  timings.VSyncStart = timings.VDisplay + var->lower_margin;
  timings.VSyncEnd = timings.VSyncStart + var->vsync_len;
  timings.VTotal = timings.VSyncEnd + var->upper_margin;
  timings.sync = var->sync;

  if (timings.pixclock > info->maxClock)
    return -EINVAL;

  /* Is the mode larger than the LCD panel? */
  if ((var->xres > info->NeoPanelWidth) ||
      (var->yres > info->NeoPanelHeight))
    {
      printk (KERN_INFO "Mode (%dx%d) larger than the LCD panel (%dx%d)\n",
	      var->xres,
	      var->yres,
	      info->NeoPanelWidth,
	      info->NeoPanelHeight);
      return -EINVAL;
    }

  /* Is the mode one of the acceptable sizes? */
  switch (var->xres)
    {
    case 1280:
      if (var->yres == 1024)
	mode_ok = 1;
      break;
    case 1024:
      if (var->yres == 768)
	mode_ok = 1;
      break;
    case  800:
      if (var->yres == 600)
	mode_ok = 1;
      break;
    case  640:
      if (var->yres == 480)
	mode_ok = 1;
      break;
    }

  if (!mode_ok)
    {
      printk (KERN_INFO "Mode (%dx%d) won't display properly on LCD\n",
	      var->xres, var->yres);
      return -EINVAL;
    }


  switch (var->bits_per_pixel)
    {
#ifdef FBCON_HAS_CFB8
    case 8:
      break;
#endif

#ifdef FBCON_HAS_CFB16
    case 16:
      break;
#endif

#ifdef FBCON_HAS_CFB24
    case 24:
      break;
#endif

#ifdef NO_32BIT_SUPPORT_YET
# ifdef FBCON_HAS_CFB32
    case 32:
      break;
# endif
#endif

    default:
      return -EINVAL;
    }

  par->depth = var->bits_per_pixel;

  vramlen = info->video.len;
  if (vramlen > 4*1024*1024)
    vramlen = 4*1024*1024;

  if (var->yres_virtual < var->yres)
    var->yres_virtual = var->yres;
  if (var->xres_virtual < var->xres)
    var->xres_virtual = var->xres;

  memlen = var->xres_virtual * var->bits_per_pixel * var->yres_virtual / 8;
  if (memlen > vramlen)
    {
      var->yres_virtual = vramlen * 8 / (var->xres_virtual * var->bits_per_pixel);
      memlen = var->xres_virtual * var->bits_per_pixel * var->yres_virtual / 8;
    }

  /* we must round yres/xres down, we already rounded y/xres_virtual up
     if it was possible. We should return -EINVAL, but I disagree */
  if (var->yres_virtual < var->yres)
    var->yres = var->yres_virtual;
  if (var->xres_virtual < var->xres)
    var->xres = var->xres_virtual;
  if (var->xoffset + var->xres > var->xres_virtual)
    var->xoffset = var->xres_virtual - var->xres;
  if (var->yoffset + var->yres > var->yres_virtual)
    var->yoffset = var->yres_virtual - var->yres;


  /*
   * This will allocate the datastructure and initialize all of the
   * generic VGA registers.
   */

  if (vgaHWInit (var, info, par, &timings))
    return -EINVAL;

  /*
   * The default value assigned by vgaHW.c is 0x41, but this does
   * not work for NeoMagic.
   */
  par->Attribute[16] = 0x01;

  switch (var->bits_per_pixel)
    {
    case  8:
      par->CRTC[0x13]   = var->xres_virtual >> 3;
      par->ExtCRTOffset = var->xres_virtual >> 11;
      par->ExtColorModeSelect = 0x11;
      break;
    case 16:
      par->CRTC[0x13]   = var->xres_virtual >> 2;
      par->ExtCRTOffset = var->xres_virtual >> 10;
      par->ExtColorModeSelect = 0x13;
      break;
    case 24:
      par->CRTC[0x13]   = (var->xres_virtual * 3) >> 3;
      par->ExtCRTOffset = (var->xres_virtual * 3) >> 11;
      par->ExtColorModeSelect = 0x14;
      break;
#ifdef NO_32BIT_SUPPORT_YET
    case 32: /* FIXME: guessed values */
      par->CRTC[0x13]   = var->xres_virtual >> 1;
      par->ExtCRTOffset = var->xres_virtual >> 9;
      par->ExtColorModeSelect = 0x15;
      break;
#endif
    default:
      break;
    }
	
  par->ExtCRTDispAddr = 0x10;

  /* Vertical Extension */
  par->VerticalExt = (((timings.VTotal -2) & 0x400) >> 10 )
    | (((timings.VDisplay -1) & 0x400) >> 9 )
    | (((timings.VSyncStart) & 0x400) >> 8 )
    | (((timings.VSyncStart) & 0x400) >> 7 );

  /* Fast write bursts on unless disabled. */
  if (info->pci_burst)
    par->SysIfaceCntl1 = 0x30; 
  else
    par->SysIfaceCntl1 = 0x00; 

  par->SysIfaceCntl2 = 0xc0; /* VESA Bios sets this to 0x80! */

  /* Enable any user specified display devices. */
  par->PanelDispCntlReg1 = 0x00;
  if (info->internal_display)
    par->PanelDispCntlReg1 |= 0x02;
  if (info->external_display)
    par->PanelDispCntlReg1 |= 0x01;

  /* If the user did not specify any display devices, then... */
  if (par->PanelDispCntlReg1 == 0x00) {
    /* Default to internal (i.e., LCD) only. */
    par->PanelDispCntlReg1 |= 0x02;
  }

  /* If we are using a fixed mode, then tell the chip we are. */
  switch (var->xres)
    {
    case 1280:
      par->PanelDispCntlReg1 |= 0x60;
      break;
    case 1024:
      par->PanelDispCntlReg1 |= 0x40;
      break;
    case 800:
      par->PanelDispCntlReg1 |= 0x20;
      break;
    case 640:
    default:
      break;
    }
  
  /* Setup shadow register locking. */
  switch (par->PanelDispCntlReg1 & 0x03)
    {
    case 0x01: /* External CRT only mode: */
      par->GeneralLockReg = 0x00;
      /* We need to program the VCLK for external display only mode. */
      par->ProgramVCLK = 1;
      break;
    case 0x02: /* Internal LCD only mode: */
    case 0x03: /* Simultaneous internal/external (LCD/CRT) mode: */
      par->GeneralLockReg = 0x01;
      /* Don't program the VCLK when using the LCD. */
      par->ProgramVCLK = 0;
      break;
    }

  /*
   * If the screen is to be stretched, turn on stretching for the
   * various modes.
   *
   * OPTION_LCD_STRETCH means stretching should be turned off!
   */
  par->PanelDispCntlReg2 = 0x00;
  par->PanelDispCntlReg3 = 0x00;

  if (info->lcd_stretch &&
      (par->PanelDispCntlReg1 == 0x02) &&  /* LCD only */
      (var->xres != info->NeoPanelWidth))
    {
      switch (var->xres)
	{
	case  320: /* Needs testing.  KEM -- 24 May 98 */
	case  400: /* Needs testing.  KEM -- 24 May 98 */
	case  640:
	case  800:
	case 1024:
	  lcd_stretch = 1;
	  par->PanelDispCntlReg2 |= 0xC6;
	  break;
	default:
	  lcd_stretch = 0;
	  /* No stretching in these modes. */
	}
    }
  else
    lcd_stretch = 0;

  /*
   * If the screen is to be centerd, turn on the centering for the
   * various modes.
   */
  par->PanelVertCenterReg1  = 0x00;
  par->PanelVertCenterReg2  = 0x00;
  par->PanelVertCenterReg3  = 0x00;
  par->PanelVertCenterReg4  = 0x00;
  par->PanelVertCenterReg5  = 0x00;
  par->PanelHorizCenterReg1 = 0x00;
  par->PanelHorizCenterReg2 = 0x00;
  par->PanelHorizCenterReg3 = 0x00;
  par->PanelHorizCenterReg4 = 0x00;
  par->PanelHorizCenterReg5 = 0x00;


  if (par->PanelDispCntlReg1 & 0x02)
    {
      if (var->xres == info->NeoPanelWidth)
	{
	  /*
	   * No centering required when the requested display width
	   * equals the panel width.
	   */
	}
      else
	{
	  par->PanelDispCntlReg2 |= 0x01;
	  par->PanelDispCntlReg3 |= 0x10;

	  /* Calculate the horizontal and vertical offsets. */
	  if (!lcd_stretch)
	    {
	      hoffset = ((info->NeoPanelWidth - var->xres) >> 4) - 1;
	      voffset = ((info->NeoPanelHeight - var->yres) >> 1) - 2;
	    }
	  else
	    {
	      /* Stretched modes cannot be centered. */
	      hoffset = 0;
	      voffset = 0;
	    }

	  switch (var->xres)
	    {
	    case  320: /* Needs testing.  KEM -- 24 May 98 */
	      par->PanelHorizCenterReg3 = hoffset;
	      par->PanelVertCenterReg2  = voffset;
	      break;
	    case  400: /* Needs testing.  KEM -- 24 May 98 */
	      par->PanelHorizCenterReg4 = hoffset;
	      par->PanelVertCenterReg1  = voffset;
	      break;
	    case  640:
	      par->PanelHorizCenterReg1 = hoffset;
	      par->PanelVertCenterReg3  = voffset;
	      break;
	    case  800:
	      par->PanelHorizCenterReg2 = hoffset;
	      par->PanelVertCenterReg4  = voffset;
	      break;
	    case 1024:
	      par->PanelHorizCenterReg5 = hoffset;
	      par->PanelVertCenterReg5  = voffset;
	      break;
	    case 1280:
	    default:
	      /* No centering in these modes. */
	      break;
	    }
	}
    }

  par->biosMode = neoFindMode (var->xres, var->yres, var->bits_per_pixel);
    
  /*
   * Calculate the VCLK that most closely matches the requested dot
   * clock.
   */
  neoCalcVCLK (info, par, timings.pixclock);

  /* Since we program the clocks ourselves, always use VCLK3. */
  par->MiscOutReg |= 0x0C;

  return 0;
}

static int neofb_set_var (struct fb_var_screeninfo *var, int con,
                          struct fb_info *fb)
{
  struct neofb_info *info = (struct neofb_info *)fb;
  struct display *display;
  struct neofb_par par;
  int err, chgvar = 0;

  DBG("neofb_set_var");

  err = neofb_decode_var (var, info, &par);
  if (err)
    return err;

  if (var->activate & FB_ACTIVATE_TEST)
    return 0;

  if (con < 0)
    {
      display = fb->disp;
      chgvar = 0;
    }
  else
    {
      display = fb_display + con;

      if (fb->var.xres != var->xres)
	chgvar = 1;
      if (fb->var.yres != var->yres)
	chgvar = 1;
      if (fb->var.xres_virtual != var->xres_virtual)
	chgvar = 1;
      if (fb->var.yres_virtual != var->yres_virtual)
	chgvar = 1;
      if (fb->var.bits_per_pixel != var->bits_per_pixel)
	chgvar = 1;
    }

  if (!info->neo2200)
    var->accel_flags &= ~FB_ACCELF_TEXT;

  var->red.msb_right	= 0;
  var->green.msb_right	= 0;
  var->blue.msb_right	= 0;

  switch (var->bits_per_pixel)
    {
#ifdef FBCON_HAS_CFB8
    case 8:	/* PSEUDOCOLOUR, 256 */
      var->transp.offset   = 0;
      var->transp.length   = 0;
      var->red.offset      = 0;
      var->red.length      = 8;
      var->green.offset	   = 0;
      var->green.length	   = 8;
      var->blue.offset	   = 0;
      var->blue.length	   = 8;
      
      fb->fix.visual       = FB_VISUAL_PSEUDOCOLOR;
      info->dispsw         = &fbcon_cfb8;
      display->dispsw_data = NULL;
      display->next_line   = var->xres_virtual;
      break;
#endif

#ifdef FBCON_HAS_CFB16
    case 16: /* DIRECTCOLOUR, 64k */
      var->transp.offset   = 0;
      var->transp.length   = 0;
      var->red.offset      = 11;
      var->red.length      = 5;
      var->green.offset    = 5;
      var->green.length    = 6;
      var->blue.offset     = 0;
      var->blue.length     = 5;

      fb->fix.visual       = FB_VISUAL_DIRECTCOLOR;
      info->dispsw         = &fbcon_cfb16;
      display->dispsw_data = fb->pseudo_palette;
      display->next_line   = var->xres_virtual * 2;
      break;
#endif

#ifdef FBCON_HAS_CFB24
    case 24: /* TRUECOLOUR, 16m */
      var->transp.offset   = 0;
      var->transp.length   = 0;
      var->red.offset      = 16;
      var->red.length      = 8;
      var->green.offset    = 8;
      var->green.length    = 8;
      var->blue.offset     = 0;
      var->blue.length     = 8;

      fb->fix.visual       = FB_VISUAL_TRUECOLOR;
      info->dispsw         = &fbcon_cfb24;
      display->dispsw_data = fb->pseudo_palette;
      display->next_line   = var->xres_virtual * 3;

      var->accel_flags    &= ~FB_ACCELF_TEXT;
      break;
#endif

#ifdef NO_32BIT_SUPPORT_YET
# ifdef FBCON_HAS_CFB32
    case 32: /* TRUECOLOUR, 16m */
      var->transp.offset   = 24;
      var->transp.length   = 8;
      var->red.offset      = 16;
      var->red.length      = 8;
      var->green.offset    = 8;
      var->green.length    = 8;
      var->blue.offset     = 0;
      var->blue.length     = 8;

      fb->fix.visual       = FB_VISUAL_TRUECOLOR;
      info->dispsw         = &fbcon_cfb32;
      display->dispsw_data = fb->pseudo_palette;
      display->next_line   = var->xres_virtual * 4;

      var->accel_flags    &= ~FB_ACCELF_TEXT;
      break;
# endif
#endif

    default:
      printk (KERN_WARNING "neofb: no support for %dbpp\n", var->bits_per_pixel);
      info->dispsw      = &fbcon_dummy;
      var->accel_flags &= ~FB_ACCELF_TEXT;
      break;
    }

  if (var->accel_flags & FB_ACCELF_TEXT)
    display->dispsw = &fbcon_neo2200_accel;
  else
    display->dispsw = info->dispsw;

  fb->fix.line_length = display->next_line;

  display->screen_base    = fb->screen_base;
  display->line_length    = fb->fix.line_length;
  display->visual         = fb->fix.visual;
  display->type	          = fb->fix.type;
  display->type_aux       = fb->fix.type_aux;
  display->ypanstep       = fb->fix.ypanstep;
  display->ywrapstep      = fb->fix.ywrapstep;
  display->can_soft_blank = 1;
  display->inverse        = 0;

  fb->var = *var;
  fb->var.activate &= ~FB_ACTIVATE_ALL;

  /*
   * Update the old var.  The fbcon drivers still use this.
   * Once they are using cfb->fb.var, this can be dropped.
   *					--rmk
   */
  display->var = fb->var;

  /*
   * If we are setting all the virtual consoles, also set the
   * defaults used to create new consoles.
   */
  if (var->activate & FB_ACTIVATE_ALL)
    fb->disp->var = fb->var;

  if (chgvar && fb && fb->changevar)
    fb->changevar (con);

  if (con == info->currcon)
    {
      if (chgvar || con < 0)
        neofb_set_par (info, &par);

      neofb_update_start (info, var);
      fb_set_cmap (&fb->cmap, 1, neo_setcolreg, fb);

      if (var->accel_flags & FB_ACCELF_TEXT)
	neo2200_accel_init (info, var);
    }

  return 0;
}

/*
 *    Pan or Wrap the Display
 */
static int neofb_pan_display (struct fb_var_screeninfo *var, int con,
			      struct fb_info *fb)
{
  struct neofb_info *info = (struct neofb_info *)fb;
  u_int y_bottom;

  y_bottom = var->yoffset;

  if (!(var->vmode & FB_VMODE_YWRAP))
    y_bottom += var->yres;

  if (var->xoffset > (var->xres_virtual - var->xres))
    return -EINVAL;
  if (y_bottom > fb->var.yres_virtual)
    return -EINVAL;

  neofb_update_start (info, var);

  fb->var.xoffset = var->xoffset;
  fb->var.yoffset = var->yoffset;

  if (var->vmode & FB_VMODE_YWRAP)
    fb->var.vmode |= FB_VMODE_YWRAP;
  else
    fb->var.vmode &= ~FB_VMODE_YWRAP;

  return 0;
}


/*
 *    Update the `var' structure (called by fbcon.c)
 *
 *    This call looks only at yoffset and the FB_VMODE_YWRAP flag in `var'.
 *    Since it's called by a kernel driver, no range checking is done.
 */
static int neofb_updatevar (int con, struct fb_info *fb)
{
  struct neofb_info *info = (struct neofb_info *)fb;

  neofb_update_start (info, &fb_display[con].var);

  return 0;
}

static int neofb_switch (int con, struct fb_info *fb)
{
  struct neofb_info *info = (struct neofb_info *)fb;
  struct display *disp;
  struct fb_cmap *cmap;

  if (info->currcon >= 0)
    {
      disp = fb_display + info->currcon;

      /*
       * Save the old colormap and video mode.
       */
      disp->var = fb->var;
      if (disp->cmap.len)
	fb_copy_cmap(&fb->cmap, &disp->cmap, 0);
    }

  info->currcon = con;
  disp = fb_display + con;

  /*
   * Install the new colormap and change the video mode.  By default,
   * fbcon sets all the colormaps and video modes to the default
   * values at bootup.
   *
   * Really, we want to set the colourmap size depending on the
   * depth of the new video mode.  For now, we leave it at its
   * default 256 entry.
   */
  if (disp->cmap.len)
    cmap = &disp->cmap;
  else
    cmap = fb_default_cmap(1 << disp->var.bits_per_pixel);

  fb_copy_cmap(cmap, &fb->cmap, 0);

  disp->var.activate = FB_ACTIVATE_NOW;
  neofb_set_var(&disp->var, con, fb);

  return 0;
}

/*
 *    (Un)Blank the display.
 */
static void neofb_blank (int blank, struct fb_info *fb)
{
  //  struct neofb_info *info = (struct neofb_info *)fb;

  /*
   *  Blank the screen if blank_mode != 0, else unblank. If
   *  blank == NULL then the caller blanks by setting the CLUT
   *  (Color Look Up Table) to all black. Return 0 if blanking
   *  succeeded, != 0 if un-/blanking failed due to e.g. a
   *  video mode which doesn't support it. Implements VESA
   *  suspend and powerdown modes on hardware that supports
   *  disabling hsync/vsync:
   *    blank_mode == 2: suspend vsync
   *    blank_mode == 3: suspend hsync
   *    blank_mode == 4: powerdown
   *
   *  wms...Enable VESA DMPS compatible powerdown mode
   *  run "setterm -powersave powerdown" to take advantage
   */
     
  switch (blank)
    {
    case 4:	/* powerdown - both sync lines down */
      break;	
    case 3:	/* hsync off */
      break;	
    case 2:	/* vsync off */
      break;	
    case 1:	/* just software blanking of screen */
      break;
    default: /* case 0, or anything else: unblank */
      break;
    }
}

/*
 * Get the currently displayed virtual consoles colormap.
 */
static int gen_get_cmap (struct fb_cmap *cmap, int kspc, int con, struct fb_info *fb)
{
  fb_copy_cmap (&fb->cmap, cmap, kspc ? 0 : 2);
  return 0;
}

/*
 * Get the currently displayed virtual consoles fixed part of the display.
 */
static int gen_get_fix (struct fb_fix_screeninfo *fix, int con, struct fb_info *fb)
{
  *fix = fb->fix;
  return 0;
}

/*
 * Get the current user defined part of the display.
 */
static int gen_get_var (struct fb_var_screeninfo *var, int con, struct fb_info *fb)
{
  *var = fb->var;
  return 0;
}

static struct fb_ops neofb_ops = {
  owner:          THIS_MODULE,
  fb_set_var:     neofb_set_var,
  fb_set_cmap:    neofb_set_cmap,
  fb_pan_display: neofb_pan_display,
  fb_get_fix:     gen_get_fix,
  fb_get_var:     gen_get_var,
  fb_get_cmap:    gen_get_cmap,
};

/* --------------------------------------------------------------------- */

static struct fb_var_screeninfo __devinitdata neofb_var640x480x8 = {
	accel_flags:	FB_ACCELF_TEXT,
	xres:		640,
	yres:		480,
	xres_virtual:   640,
	yres_virtual:   30000,
	bits_per_pixel: 8,
	pixclock:	39722,
	left_margin:	48,
	right_margin:	16,
	upper_margin:	33,
	lower_margin:	10,
	hsync_len:	96,
	vsync_len:	2,
	sync:		0,
	vmode:		FB_VMODE_NONINTERLACED
};

static struct fb_var_screeninfo __devinitdata neofb_var800x600x8 = {
	accel_flags:	FB_ACCELF_TEXT,
	xres:		800,
	yres:		600,
	xres_virtual:   800,
	yres_virtual:   30000,
	bits_per_pixel: 8,
	pixclock:	25000,
	left_margin:	88,
	right_margin:	40,
	upper_margin:	23,
	lower_margin:	1,
	hsync_len:	128,
	vsync_len:	4,
	sync:		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	vmode:		FB_VMODE_NONINTERLACED
};

static struct fb_var_screeninfo __devinitdata neofb_var1024x768x8 = {
	accel_flags:	FB_ACCELF_TEXT,
	xres:		1024,
	yres:		768,
	xres_virtual:   1024,
	yres_virtual:   30000,
	bits_per_pixel: 8,
	pixclock:	15385,
	left_margin:	160,
	right_margin:	24,
	upper_margin:	29,
	lower_margin:	3,
	hsync_len:	136,
	vsync_len:	6,
	sync:		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	vmode:		FB_VMODE_NONINTERLACED
};

#ifdef NOT_DONE
static struct fb_var_screeninfo __devinitdata neofb_var1280x1024x8 = {
	accel_flags:	FB_ACCELF_TEXT,
	xres:		1280,
	yres:		1024,
	xres_virtual:   1280,
	yres_virtual:   30000,
	bits_per_pixel: 8,
	pixclock:	9260,
	left_margin:	248,
	right_margin:	48,
	upper_margin:	38,
	lower_margin:	1,
	hsync_len:	112,
	vsync_len:	3,
	sync:		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	vmode:		FB_VMODE_NONINTERLACED
};
#endif

static struct fb_var_screeninfo *neofb_var = NULL;


static int __devinit neo_map_mmio (struct neofb_info *info)
{
  DBG("neo_map_mmio");

  info->mmio.pbase = pci_resource_start (info->pcidev, 1);
  info->mmio.len   = MMIO_SIZE;

  if (!request_mem_region (info->mmio.pbase, MMIO_SIZE, "memory mapped I/O"))
    {
      printk ("neofb: memory mapped IO in use\n");
      return -EBUSY;
    }

  info->mmio.vbase = ioremap (info->mmio.pbase, MMIO_SIZE);
  if (!info->mmio.vbase)
    {
      printk ("neofb: unable to map memory mapped IO\n");
      release_mem_region (info->mmio.pbase, info->mmio.len);
      return -ENOMEM;
    }
  else
    printk (KERN_INFO "neofb: mapped io at %p\n", info->mmio.vbase);

  info->fb.fix.mmio_start = info->mmio.pbase;
  info->fb.fix.mmio_len   = info->mmio.len;

  return 0;
}

static void __devinit neo_unmap_mmio (struct neofb_info *info)
{
  DBG("neo_unmap_mmio");

  if (info->mmio.vbase)
    {
      iounmap (info->mmio.vbase);
      info->mmio.vbase = NULL;

      release_mem_region (info->mmio.pbase, info->mmio.len);
    }
}

static int __devinit neo_map_video (struct neofb_info *info, int video_len)
{
  DBG("neo_map_video");

  info->video.pbase = pci_resource_start (info->pcidev, 0);
  info->video.len   = video_len;

  if (!request_mem_region (info->video.pbase, info->video.len, "frame buffer"))
    {
      printk ("neofb: frame buffer in use\n");
      return -EBUSY;
    }

  info->video.vbase = ioremap (info->video.pbase, info->video.len);
  if (!info->video.vbase)
    {
      printk ("neofb: unable to map screen memory\n");
      release_mem_region (info->video.pbase, info->video.len);
      return -ENOMEM;
    }
  else
    printk (KERN_INFO "neofb: mapped framebuffer at %p\n", info->video.vbase);

  info->fb.fix.smem_start = info->video.pbase;
  info->fb.fix.smem_len   = info->video.len;
  info->fb.screen_base    = info->video.vbase;

#ifdef CONFIG_MTRR
  info->video.mtrr = mtrr_add (info->video.pbase, pci_resource_len (info->pcidev, 0), MTRR_TYPE_WRCOMB, 1);
#endif

  /* Clear framebuffer, it's all white in memory after boot */
  memset (info->video.vbase, 0, info->video.len);

  return 0;
}

static void __devinit neo_unmap_video (struct neofb_info *info)
{
  DBG("neo_unmap_video");

  if (info->video.vbase)
    {
#ifdef CONFIG_MTRR
      mtrr_del (info->video.mtrr, info->video.pbase, info->video.len);
#endif

      iounmap (info->video.vbase);
      info->video.vbase = NULL;
      info->fb.screen_base = NULL;

      release_mem_region (info->video.pbase, info->video.len);
    }
}

static int __devinit neo_init_hw (struct neofb_info *info)
{
  int videoRam = 896;
  int maxClock = 65000;
  int CursorMem = 1024;
  int CursorOff = 0x100;
  int linearSize = 1024;
  int maxWidth = 1024;
  int maxHeight = 1024;
  unsigned char type, display;
  int w;
    
  DBG("neo_init_hw");

  neoUnlock();

#if 0
  printk (KERN_DEBUG "--- Neo extended register dump ---\n");
  for (w=0; w<0x85; w++)
    printk (KERN_DEBUG "CR %p: %p\n", (void*)w, (void*)VGArCR (w));
  for (w=0; w<0xC7; w++)
    printk (KERN_DEBUG "GR %p: %p\n", (void*)w, (void*)VGArGR (w));
#endif

  /* Determine the panel type */
  VGAwGR(0x09,0x26);
  type = VGArGR(0x21);
  display = VGArGR(0x20);
    
  /* Determine panel width -- used in NeoValidMode. */
  w = VGArGR(0x20);
  VGAwGR(0x09,0x00);
  switch ((w & 0x18) >> 3)
    {
    case 0x00:
      info->NeoPanelWidth  = 640;
      info->NeoPanelHeight = 480;
      neofb_var = &neofb_var640x480x8;
      break;
    case 0x01:
      info->NeoPanelWidth  = 800;
      info->NeoPanelHeight = 600;
      neofb_var = &neofb_var800x600x8;
      break;
    case 0x02:
      info->NeoPanelWidth  = 1024;
      info->NeoPanelHeight = 768;
      neofb_var = &neofb_var1024x768x8;
      break;
    case 0x03:
      /* 1280x1024 panel support needs to be added */
#ifdef NOT_DONE
      info->NeoPanelWidth  = 1280;
      info->NeoPanelHeight = 1024;
      neofb_var = &neofb_var1280x1024x8;
      break;
#else
      printk (KERN_ERR "neofb: Only 640x480, 800x600 and 1024x768 panels are currently supported\n");
      return -1;
#endif
    default:
      info->NeoPanelWidth  = 640;
      info->NeoPanelHeight = 480;
      neofb_var = &neofb_var640x480x8;
      break;
    }

  printk (KERN_INFO "Panel is a %dx%d %s %s display\n",
	  info->NeoPanelWidth,
	  info->NeoPanelHeight,
	  (type & 0x02) ? "color" : "monochrome",
	  (type & 0x10) ? "TFT" : "dual scan");

  switch (info->accel)
    {
    case FB_ACCEL_NEOMAGIC_NM2070:
      videoRam   = 896;
      maxClock   = 65000;
      CursorMem  = 2048;
      CursorOff  = 0x100;
      linearSize = 1024;
      maxWidth   = 1024;
      maxHeight  = 1024;
      break;
    case FB_ACCEL_NEOMAGIC_NM2090:
    case FB_ACCEL_NEOMAGIC_NM2093:
      videoRam   = 1152;
      maxClock   = 80000;
      CursorMem  = 2048;
      CursorOff  = 0x100;
      linearSize = 2048;
      maxWidth   = 1024;
      maxHeight  = 1024;
      break;
    case FB_ACCEL_NEOMAGIC_NM2097:
      videoRam   = 1152;
      maxClock   = 80000;
      CursorMem  = 1024;
      CursorOff  = 0x100;
      linearSize = 2048;
      maxWidth   = 1024;
      maxHeight  = 1024;
      break;
    case FB_ACCEL_NEOMAGIC_NM2160:
      videoRam   = 2048;
      maxClock   = 90000;
      CursorMem  = 1024;
      CursorOff  = 0x100;
      linearSize = 2048;
      maxWidth   = 1024;
      maxHeight  = 1024;
      break;
    case FB_ACCEL_NEOMAGIC_NM2200:
      videoRam   = 2560;
      maxClock   = 110000;
      CursorMem  = 1024;
      CursorOff  = 0x1000;
      linearSize = 4096;
      maxWidth   = 1280;
      maxHeight  = 1024;  /* ???? */

      info->neo2200 = (Neo2200*) info->mmio.vbase;
      break;
    case FB_ACCEL_NEOMAGIC_NM2230:
      videoRam   = 3008;
      maxClock   = 110000;
      CursorMem  = 1024;
      CursorOff  = 0x1000;
      linearSize = 4096;
      maxWidth   = 1280;
      maxHeight  = 1024;  /* ???? */

      info->neo2200 = (Neo2200*) info->mmio.vbase;
      break;
    case FB_ACCEL_NEOMAGIC_NM2360:
      videoRam   = 4096;
      maxClock   = 110000;
      CursorMem  = 1024;
      CursorOff  = 0x1000;
      linearSize = 4096;
      maxWidth   = 1280;
      maxHeight  = 1024;  /* ???? */

      info->neo2200 = (Neo2200*) info->mmio.vbase;
      break;
    case FB_ACCEL_NEOMAGIC_NM2380:
      videoRam   = 6144;
      maxClock   = 110000;
      CursorMem  = 1024;
      CursorOff  = 0x1000;
      linearSize = 8192;
      maxWidth   = 1280;
      maxHeight  = 1024;  /* ???? */

      info->neo2200 = (Neo2200*) info->mmio.vbase;
      break;
    }

  info->maxClock = maxClock;

  return videoRam * 1024;
}


static struct neofb_info * __devinit neo_alloc_fb_info (struct pci_dev *dev,
							const struct pci_device_id *id)
{
  struct neofb_info *info;

  info = kmalloc (sizeof(struct neofb_info) + sizeof(struct display) +
		  sizeof(u32) * 16, GFP_KERNEL);

  if (!info)
    return NULL;

  memset (info, 0, sizeof(struct neofb_info) + sizeof(struct display));

  info->currcon = -1;
  info->pcidev  = dev;
  info->accel   = id->driver_data;

  info->pci_burst   = !nopciburst;
  info->lcd_stretch = !nostretch;

  if (!internal && !external)
    {
      info->internal_display = 1;
      info->external_display = 0;
    }
  else
    {
      info->internal_display = internal;
      info->external_display = external;
    }

  switch (info->accel)
    {
    case FB_ACCEL_NEOMAGIC_NM2070:
      sprintf (info->fb.fix.id, "MagicGraph 128");
      break;
    case FB_ACCEL_NEOMAGIC_NM2090:
      sprintf (info->fb.fix.id, "MagicGraph 128V");
      break;
    case FB_ACCEL_NEOMAGIC_NM2093:
      sprintf (info->fb.fix.id, "MagicGraph 128ZV");
      break;
    case FB_ACCEL_NEOMAGIC_NM2097:
      sprintf (info->fb.fix.id, "MagicGraph 128ZV+");
      break;
    case FB_ACCEL_NEOMAGIC_NM2160:
      sprintf (info->fb.fix.id, "MagicGraph 128XD");
      break;
    case FB_ACCEL_NEOMAGIC_NM2200:
      sprintf (info->fb.fix.id, "MagicGraph 256AV");
      break;
    case FB_ACCEL_NEOMAGIC_NM2230:
      sprintf (info->fb.fix.id, "MagicGraph 256AV+");
      break;
    case FB_ACCEL_NEOMAGIC_NM2360:
      sprintf (info->fb.fix.id, "MagicGraph 256ZX");
      break;
    case FB_ACCEL_NEOMAGIC_NM2380:
      sprintf (info->fb.fix.id, "MagicGraph 256XL+");
      break;
    }

  info->fb.fix.type	   = FB_TYPE_PACKED_PIXELS;
  info->fb.fix.type_aux	   = 0;
  info->fb.fix.xpanstep	   = 0;
  info->fb.fix.ypanstep	   = 4;
  info->fb.fix.ywrapstep   = 0;
  info->fb.fix.accel       = id->driver_data;

  info->fb.var.nonstd      = 0;
  info->fb.var.activate    = FB_ACTIVATE_NOW;
  info->fb.var.height      = -1;
  info->fb.var.width       = -1;
  info->fb.var.accel_flags = 0;

  strcpy (info->fb.modename, info->fb.fix.id);

  info->fb.fbops          = &neofb_ops;
  info->fb.changevar      = NULL;
  info->fb.switch_con     = neofb_switch;
  info->fb.updatevar      = neofb_updatevar;
  info->fb.blank          = neofb_blank;
  info->fb.flags          = FBINFO_FLAG_DEFAULT;
  info->fb.disp           = (struct display *)(info + 1);
  info->fb.pseudo_palette = (void *)(info->fb.disp + 1);

  fb_alloc_cmap (&info->fb.cmap, NR_PALETTE, 0);

  return info;
}

static void __devinit neo_free_fb_info (struct neofb_info *info)
{
  if (info)
    {
      /*
       * Free the colourmap
       */
      fb_alloc_cmap (&info->fb.cmap, 0, 0);

      kfree (info);
    }
}

/* --------------------------------------------------------------------- */

static int __devinit neofb_probe (struct pci_dev* dev, const struct pci_device_id* id)
{
  struct neofb_info *info;
  u_int h_sync, v_sync;
  int err;
  int video_len;

  DBG("neofb_probe");

  err = pci_enable_device (dev);
  if (err)
    return err;

  err = -ENOMEM;
  info = neo_alloc_fb_info (dev, id);
  if (!info)
    goto failed;

  err = neo_map_mmio (info);
  if (err)
    goto failed;

  video_len = neo_init_hw (info);
  if (video_len < 0)
    {
      err = video_len;
      goto failed;
    }

  err = neo_map_video (info, video_len);
  if (err)
    goto failed;

  neofb_set_var (neofb_var, -1, &info->fb);

  /*
   * Calculate the hsync and vsync frequencies.  Note that
   * we split the 1e12 constant up so that we can preserve
   * the precision and fit the results into 32-bit registers.
   *  (1953125000 * 512 = 1e12)
   */
  h_sync = 1953125000 / info->fb.var.pixclock;
  h_sync = h_sync * 512 / (info->fb.var.xres + info->fb.var.left_margin +
			   info->fb.var.right_margin + info->fb.var.hsync_len);
  v_sync = h_sync / (info->fb.var.yres + info->fb.var.upper_margin +
		     info->fb.var.lower_margin + info->fb.var.vsync_len);

  printk(KERN_INFO "neofb v" NEOFB_VERSION ": %dkB VRAM, using %dx%d, %d.%03dkHz, %dHz\n",
	 info->fb.fix.smem_len >> 10,
	 info->fb.var.xres, info->fb.var.yres,
	 h_sync / 1000, h_sync % 1000, v_sync);


  err = register_framebuffer (&info->fb);
  if (err < 0)
    goto failed;

  printk (KERN_INFO "fb%d: %s frame buffer device\n",
	  GET_FB_IDX(info->fb.node), info->fb.modename);

  /*
   * Our driver data
   */
  pci_set_drvdata(dev, info);

  return 0;

failed:
  neo_unmap_video (info);
  neo_unmap_mmio (info);
  neo_free_fb_info (info);

  return err;
}

static void __devexit neofb_remove (struct pci_dev *dev)
{
  struct neofb_info *info = pci_get_drvdata(dev);

  DBG("neofb_remove");

  if (info)
    {
      /*
       * If unregister_framebuffer fails, then
       * we will be leaving hooks that could cause
       * oopsen laying around.
       */
      if (unregister_framebuffer (&info->fb))
	printk (KERN_WARNING "neofb: danger danger!  Oopsen imminent!\n");

      neo_unmap_video (info);
      neo_unmap_mmio (info);
      neo_free_fb_info (info);

      /*
       * Ensure that the driver data is no longer
       * valid.
       */
      pci_set_drvdata(dev, NULL);
    }
}

static struct pci_device_id neofb_devices[] __devinitdata = {
  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2070,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2070},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2090,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2090},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2093,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2093},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2097,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2097},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2160,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2160},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2200,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2200},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2230,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2230},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2360,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2360},

  {PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2380,
   PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2380},

  {0, 0, 0, 0, 0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, neofb_devices);

static struct pci_driver neofb_driver = {
  name:      "neofb",
  id_table:  neofb_devices,
  probe:     neofb_probe,
  remove:    __devexit_p(neofb_remove)
};

/* **************************** init-time only **************************** */

static void __init neo_init (void)
{
  DBG("neo_init");
  pci_register_driver (&neofb_driver);
}

/* **************************** exit-time only **************************** */

static void __exit neo_done (void)
{
  DBG("neo_done");
  pci_unregister_driver (&neofb_driver);
}


#ifndef MODULE

/* ************************* init in-kernel code ************************** */

int __init neofb_setup (char *options)
{
  char *this_opt;

  DBG("neofb_setup");

  if (!options || !*options)
    return 0;

  for (this_opt=strtok(options,","); this_opt; this_opt=strtok(NULL,","))
    {
      if (!*this_opt) continue;

      if (!strncmp(this_opt, "disabled", 8))
	disabled = 1;
      if (!strncmp(this_opt, "internal", 8))
	internal = 1;
      if (!strncmp(this_opt, "external", 8))
	external = 1;
      if (!strncmp(this_opt, "nostretch", 9))
	nostretch = 1;
      if (!strncmp(this_opt, "nopciburst", 10))
	nopciburst = 1;
    }

  return 0;
}

static int __initdata initialized = 0;

int __init neofb_init(void)
{
  DBG("neofb_init");

  if (disabled)
    return -ENXIO;

  if (!initialized)
    {
      initialized = 1;
      neo_init();
    }

  /* never return failure, user can hotplug card later... */
  return 0;
}

#else

/* *************************** init module code **************************** */

int __init init_module(void)
{
  DBG("init_module");

  if (disabled)
    return -ENXIO;

  neo_init();

  /* never return failure; user can hotplug card later... */
  return 0;
}

#endif	/* MODULE */

module_exit(neo_done);
