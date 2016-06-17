/*
 *  linux/drivers/video/sgivwfb.c -- SGI DBE frame buffer device
 *
 *	Copyright (C) 1999 Silicon Graphics, Inc.
 *      Jeffrey Newquist, newquist@engr.sgi.som
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/mtrr.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb32.h>

#define INCLUDE_TIMING_TABLE_DATA
#define DBE_REG_BASE regs
#include "sgivwfb.h"

struct sgivwfb_par {
  struct fb_var_screeninfo var;
  u_long timing_num;
  int valid;
};

/*
 *  RAM we reserve for the frame buffer. This defines the maximum screen
 *  size
 *
 *  The default can be overridden if the driver is compiled as a module
 */

/* set by arch/i386/kernel/setup.c */
u_long                sgivwfb_mem_phys;
u_long                sgivwfb_mem_size;

static volatile char  *fbmem;
static asregs         *regs;
static struct fb_info fb_info;
static struct { u_char red, green, blue, pad; } palette[256];
static char           sgivwfb_name[16] = "SGI Vis WS FB";
static u32            cmap_fifo;
static int            ypan       = 0;
static int            ywrap      = 0;

/* console related variables */
static int currcon = 0;
static struct display disp;

static union {
#ifdef FBCON_HAS_CFB16
  u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB32
  u32 cfb32[16];
#endif
} fbcon_cmap;

static struct sgivwfb_par par_current = {
  {                             /* var (screeninfo) */
    /* 640x480, 8 bpp */
    640, 480, 640, 480, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 20000, 64, 64, 32, 32, 64, 2,
    0, FB_VMODE_NONINTERLACED
  },
  0,                            /* timing_num */
  0				/* par not activated */
};

/*
 *  Interface used by the world
 */
int sgivwfb_setup(char*);

static int sgivwfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info);
static int sgivwfb_get_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
static int sgivwfb_set_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
static int sgivwfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
static int sgivwfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
static int sgivwfb_mmap(struct fb_info *info, struct file *file,
                        struct vm_area_struct *vma);

static struct fb_ops sgivwfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	sgivwfb_get_fix,
	fb_get_var:	sgivwfb_get_var,
	fb_set_var:	sgivwfb_set_var,
	fb_get_cmap:	sgivwfb_get_cmap,
	fb_set_cmap:	sgivwfb_set_cmap,
	fb_mmap:	sgivwfb_mmap,
};

/*
 *  Interface to the low level console driver
 */
int sgivwfb_init(void);
static int sgivwfbcon_switch(int con, struct fb_info *info);
static int sgivwfbcon_updatevar(int con, struct fb_info *info);
static void sgivwfbcon_blank(int blank, struct fb_info *info);

/*
 *  Internal routines
 */
static u_long get_line_length(int xres_virtual, int bpp);
static unsigned long bytes_per_pixel(int bpp);
static void activate_par(struct sgivwfb_par *par);
static void sgivwfb_encode_fix(struct fb_fix_screeninfo *fix,
			       struct fb_var_screeninfo *var);
static int sgivwfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			     u_int *transp, struct fb_info *info);
static int sgivwfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			     u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);

static unsigned long get_line_length(int xres_virtual, int bpp)
{
  return(xres_virtual * bytes_per_pixel(bpp));
}

static unsigned long bytes_per_pixel(int bpp)
{
  unsigned long length;

  switch (bpp) {
  case 8:
    length = 1;
    break;
  case 16:
    length = 2;
    break;
  case 32:
    length = 4;
    break;
  default:
    printk(KERN_INFO "sgivwfb: unsupported bpp=%d\n", bpp);
    length = 0;
    break;
  }
  return(length);
}

/*
 * Function:	dbe_TurnOffDma
 * Parameters:	(None)
 * Description:	This should turn off the monitor and dbe.  This is used
 *              when switching between the serial console and the graphics
 *              console.
 */

static void dbe_TurnOffDma(void)
{
  int i;
  unsigned int readVal;

  // Check to see if things are already turned off:
  // 1) Check to see if dbe is not using the internal dotclock.
  // 2) Check to see if the xy counter in dbe is already off.

  DBE_GETREG(ctrlstat, readVal);
  if (GET_DBE_FIELD(CTRLSTAT, PCLKSEL, readVal) < 2)
    return;

  DBE_GETREG(vt_xy, readVal);
  if (GET_DBE_FIELD(VT_XY, VT_FREEZE, readVal) == 1)
    return;

  // Otherwise, turn off dbe

  DBE_GETREG(ovr_control, readVal);
  SET_DBE_FIELD(OVR_CONTROL, OVR_DMA_ENABLE, readVal, 0);
  DBE_SETREG(ovr_control, readVal);
  udelay(1000);
  DBE_GETREG(frm_control, readVal);
  SET_DBE_FIELD(FRM_CONTROL, FRM_DMA_ENABLE, readVal, 0);
  DBE_SETREG(frm_control, readVal);
  udelay(1000);
  DBE_GETREG(did_control, readVal);
  SET_DBE_FIELD(DID_CONTROL, DID_DMA_ENABLE, readVal, 0);
  DBE_SETREG(did_control, readVal);
  udelay(1000);

  // XXX HACK:
  //
  //    This was necessary for GBE--we had to wait through two
  //    vertical retrace periods before the pixel DMA was
  //    turned off for sure.  I've left this in for now, in
  //    case dbe needs it.

  for (i = 0; i < 10000; i++)
    {
      DBE_GETREG(frm_inhwctrl, readVal);
      if (GET_DBE_FIELD(FRM_INHWCTRL, FRM_DMA_ENABLE, readVal) == 0)
        udelay(10);
      else
        {
          DBE_GETREG(ovr_inhwctrl, readVal);
          if (GET_DBE_FIELD(OVR_INHWCTRL, OVR_DMA_ENABLE, readVal) == 0)
            udelay(10);
          else
            {
              DBE_GETREG(did_inhwctrl, readVal);
              if (GET_DBE_FIELD(DID_INHWCTRL, DID_DMA_ENABLE, readVal) == 0)
                udelay(10);
              else
                break;
            }
        }
    }
}

/*
 *  Set the hardware according to 'par'.
 */
static void activate_par(struct sgivwfb_par *par)
{
  int i,j, htmp, temp;
  u32 readVal, outputVal;
  int wholeTilesX, maxPixelsPerTileX;
  int frmWrite1, frmWrite2, frmWrite3b;
  dbe_timing_info_t *currentTiming;    /* Current Video Timing */
  int xpmax, ypmax;                       // Monitor resolution
  int bytesPerPixel;                      // Bytes per pixel

  currentTiming = &dbeVTimings[par->timing_num];
  bytesPerPixel = bytes_per_pixel(par->var.bits_per_pixel);
  xpmax = currentTiming->width;
  ypmax = currentTiming->height;

  /* dbe_InitGraphicsBase(); */
  /* Turn on dotclock PLL */
  DBE_SETREG(ctrlstat, 0x20000000);

  dbe_TurnOffDma();

  /* dbe_CalculateScreenParams(); */
  maxPixelsPerTileX = 512/bytesPerPixel;
  wholeTilesX = xpmax/maxPixelsPerTileX;
  if (wholeTilesX*maxPixelsPerTileX < xpmax)
    wholeTilesX++;

  printk(KERN_DEBUG "sgivwfb: pixPerTile=%d wholeTilesX=%d\n",
	 maxPixelsPerTileX, wholeTilesX);

  /* dbe_InitGammaMap(); */
  udelay(10);

  for (i = 0; i < 256; i++)
    {
      DBE_ISETREG(gmap, i, (i << 24) | (i << 16) | (i << 8));
    }

  /* dbe_TurnOn(); */
  DBE_GETREG(vt_xy, readVal);
  if (GET_DBE_FIELD(VT_XY, VT_FREEZE, readVal) == 1)
    {
      DBE_SETREG(vt_xy, 0x00000000);
      udelay(1);
    }
  else
    dbe_TurnOffDma();

  /* dbe_Initdbe(); */
  for (i = 0; i < 256; i++)
    {
      for (j = 0; j < 100; j++)
        {
          DBE_GETREG(cm_fifo, readVal);
          if (readVal != 0x00000000)
            break;
          else
            udelay(10);
        }

      // DBE_ISETREG(cmap, i, 0x00000000);
      DBE_ISETREG(cmap, i, (i<<8)|(i<<16)|(i<<24));
    }

  /* dbe_InitFramebuffer(); */
  frmWrite1 = 0;
  SET_DBE_FIELD(FRM_SIZE_TILE, FRM_WIDTH_TILE, frmWrite1, wholeTilesX);
  SET_DBE_FIELD(FRM_SIZE_TILE, FRM_RHS, frmWrite1, 0);

  switch(bytesPerPixel)
    {
      case 1:
        SET_DBE_FIELD(FRM_SIZE_TILE, FRM_DEPTH, frmWrite1, DBE_FRM_DEPTH_8);
        break;
      case 2:
        SET_DBE_FIELD(FRM_SIZE_TILE, FRM_DEPTH, frmWrite1, DBE_FRM_DEPTH_16);
        break;
      case 4:
        SET_DBE_FIELD(FRM_SIZE_TILE, FRM_DEPTH, frmWrite1, DBE_FRM_DEPTH_32);
        break;
    }

  frmWrite2 = 0;
  SET_DBE_FIELD(FRM_SIZE_PIXEL, FB_HEIGHT_PIX, frmWrite2, ypmax);

  // Tell dbe about the framebuffer location and type
  // XXX What format is the FRM_TILE_PTR??  64K aligned address?
  frmWrite3b = 0;
  SET_DBE_FIELD(FRM_CONTROL, FRM_TILE_PTR, frmWrite3b, sgivwfb_mem_phys>>9);
  SET_DBE_FIELD(FRM_CONTROL, FRM_DMA_ENABLE, frmWrite3b, 1);
  SET_DBE_FIELD(FRM_CONTROL, FRM_LINEAR, frmWrite3b, 1);

  /* Initialize DIDs */

  outputVal = 0;
  switch(bytesPerPixel)
    {
      case 1:
        SET_DBE_FIELD(WID, TYP, outputVal, DBE_CMODE_I8);
        break;
      case 2:
        SET_DBE_FIELD(WID, TYP, outputVal, DBE_CMODE_RGBA5);
        break;
      case 4:
        SET_DBE_FIELD(WID, TYP, outputVal, DBE_CMODE_RGB8);
        break;
    }
  SET_DBE_FIELD(WID, BUF, outputVal, DBE_BMODE_BOTH);

  for (i = 0; i < 32; i++)
    {
      DBE_ISETREG(mode_regs, i, outputVal);
    }

  /* dbe_InitTiming(); */
  DBE_SETREG(vt_intr01, 0xffffffff);
  DBE_SETREG(vt_intr23, 0xffffffff);

  DBE_GETREG(dotclock, readVal);
  DBE_SETREG(dotclock, readVal & 0xffff);

  DBE_SETREG(vt_xymax, 0x00000000);
  outputVal = 0;
  SET_DBE_FIELD(VT_VSYNC, VT_VSYNC_ON, outputVal, currentTiming->vsync_start);
  SET_DBE_FIELD(VT_VSYNC, VT_VSYNC_OFF, outputVal, currentTiming->vsync_end);
  DBE_SETREG(vt_vsync, outputVal);
  outputVal = 0;
  SET_DBE_FIELD(VT_HSYNC, VT_HSYNC_ON, outputVal, currentTiming->hsync_start);
  SET_DBE_FIELD(VT_HSYNC, VT_HSYNC_OFF, outputVal, currentTiming->hsync_end);
  DBE_SETREG(vt_hsync, outputVal);
  outputVal = 0;
  SET_DBE_FIELD(VT_VBLANK, VT_VBLANK_ON, outputVal, currentTiming->vblank_start);
  SET_DBE_FIELD(VT_VBLANK, VT_VBLANK_OFF, outputVal, currentTiming->vblank_end);
  DBE_SETREG(vt_vblank, outputVal);
  outputVal = 0;
  SET_DBE_FIELD(VT_HBLANK, VT_HBLANK_ON, outputVal, currentTiming->hblank_start);
  SET_DBE_FIELD(VT_HBLANK, VT_HBLANK_OFF, outputVal, currentTiming->hblank_end-3);
  DBE_SETREG(vt_hblank, outputVal);
  outputVal = 0;
  SET_DBE_FIELD(VT_VCMAP, VT_VCMAP_ON, outputVal, currentTiming->vblank_start);
  SET_DBE_FIELD(VT_VCMAP, VT_VCMAP_OFF, outputVal, currentTiming->vblank_end);
  DBE_SETREG(vt_vcmap, outputVal);
  outputVal = 0;
  SET_DBE_FIELD(VT_HCMAP, VT_HCMAP_ON, outputVal, currentTiming->hblank_start);
  SET_DBE_FIELD(VT_HCMAP, VT_HCMAP_OFF, outputVal, currentTiming->hblank_end-3);
  DBE_SETREG(vt_hcmap, outputVal);

  outputVal = 0;
  temp = currentTiming->vblank_start - currentTiming->vblank_end - 1;
  if (temp > 0)
    temp = -temp;

  SET_DBE_FIELD(DID_START_XY, DID_STARTY, outputVal, (u32)temp);
  if (currentTiming->hblank_end >= 20)
    SET_DBE_FIELD(DID_START_XY, DID_STARTX, outputVal,
                      currentTiming->hblank_end - 20);
  else
    SET_DBE_FIELD(DID_START_XY, DID_STARTX, outputVal,
                      currentTiming->htotal - (20 - currentTiming->hblank_end));
  DBE_SETREG(did_start_xy, outputVal);

  outputVal = 0;
  SET_DBE_FIELD(CRS_START_XY, CRS_STARTY, outputVal, (u32)(temp+1));
  if (currentTiming->hblank_end >= DBE_CRS_MAGIC)
    SET_DBE_FIELD(CRS_START_XY, CRS_STARTX, outputVal,
                      currentTiming->hblank_end - DBE_CRS_MAGIC);
  else
    SET_DBE_FIELD(CRS_START_XY, CRS_STARTX, outputVal,
                      currentTiming->htotal - (DBE_CRS_MAGIC - currentTiming->hblank_end));
  DBE_SETREG(crs_start_xy, outputVal);

  outputVal = 0;
  SET_DBE_FIELD(VC_START_XY, VC_STARTY, outputVal, (u32)temp);
  SET_DBE_FIELD(VC_START_XY, VC_STARTX, outputVal,
                    currentTiming->hblank_end - 4);
  DBE_SETREG(vc_start_xy, outputVal);

  DBE_SETREG(frm_size_tile, frmWrite1);
  DBE_SETREG(frm_size_pixel, frmWrite2);

  outputVal = 0;
  SET_DBE_FIELD(DOTCLK, M, outputVal, currentTiming->pll_m-1);
  SET_DBE_FIELD(DOTCLK, N, outputVal, currentTiming->pll_n-1);
  SET_DBE_FIELD(DOTCLK, P, outputVal, currentTiming->pll_p);
  SET_DBE_FIELD(DOTCLK, RUN, outputVal, 1);
  DBE_SETREG(dotclock, outputVal);

  udelay(11*1000);

  DBE_SETREG(vt_vpixen, 0xffffff);
  DBE_SETREG(vt_hpixen, 0xffffff);

  outputVal = 0;
  SET_DBE_FIELD(VT_XYMAX, VT_MAXX, outputVal, currentTiming->htotal);
  SET_DBE_FIELD(VT_XYMAX, VT_MAXY, outputVal, currentTiming->vtotal);
  DBE_SETREG(vt_xymax, outputVal);

  outputVal = frmWrite1;
  SET_DBE_FIELD(FRM_SIZE_TILE, FRM_FIFO_RESET, outputVal, 1);
  DBE_SETREG(frm_size_tile, outputVal);
  DBE_SETREG(frm_size_tile, frmWrite1);

  outputVal = 0;
  SET_DBE_FIELD(OVR_WIDTH_TILE, OVR_FIFO_RESET, outputVal, 1);
  DBE_SETREG(ovr_width_tile, outputVal);
  DBE_SETREG(ovr_width_tile, 0);

  DBE_SETREG(frm_control, frmWrite3b);
  DBE_SETREG(did_control, 0);

  // Wait for dbe to take frame settings
  for (i=0; i<100000; i++)
    {
      DBE_GETREG(frm_inhwctrl, readVal);
      if (GET_DBE_FIELD(FRM_INHWCTRL, FRM_DMA_ENABLE, readVal) != 0)
        break;
      else
        udelay(1);
    }

  if (i==100000)
    printk(KERN_INFO "sgivwfb: timeout waiting for frame DMA enable.\n");

  outputVal = 0;
  htmp = currentTiming->hblank_end - 19;
  if (htmp < 0)
    htmp += currentTiming->htotal;    /* allow blank to wrap around */
  SET_DBE_FIELD(VT_HPIXEN, VT_HPIXEN_ON, outputVal, htmp);
  SET_DBE_FIELD(VT_HPIXEN, VT_HPIXEN_OFF, outputVal,
                    ((htmp + currentTiming->width - 2) % currentTiming->htotal));
  DBE_SETREG(vt_hpixen, outputVal);

  outputVal = 0;
  SET_DBE_FIELD(VT_VPIXEN, VT_VPIXEN_OFF, outputVal,
                    currentTiming->vblank_start);
  SET_DBE_FIELD(VT_VPIXEN, VT_VPIXEN_ON, outputVal,
                    currentTiming->vblank_end);
  DBE_SETREG(vt_vpixen, outputVal);

  // Turn off mouse cursor
  regs->crs_ctl = 0;

  // XXX What's this section for??
  DBE_GETREG(ctrlstat, readVal);
  readVal &= 0x02000000;

  if (readVal != 0)
    {
      DBE_SETREG(ctrlstat, 0x30000000);
    }
}

static void sgivwfb_encode_fix(struct fb_fix_screeninfo *fix,
			       struct fb_var_screeninfo *var)
{
  memset(fix, 0, sizeof(struct fb_fix_screeninfo));
  strcpy(fix->id, sgivwfb_name);
  fix->smem_start = sgivwfb_mem_phys;
  fix->smem_len = sgivwfb_mem_size;
  fix->type = FB_TYPE_PACKED_PIXELS;
  fix->type_aux = 0;
  switch (var->bits_per_pixel) {
  case 8:
    fix->visual = FB_VISUAL_PSEUDOCOLOR;
    break;
  default:
    fix->visual = FB_VISUAL_TRUECOLOR;
    break;
  }
  fix->ywrapstep = ywrap;
  fix->xpanstep = 0;
  fix->ypanstep = ypan;
  fix->line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
  fix->mmio_start = DBE_REG_PHYS;
  fix->mmio_len = DBE_REG_SIZE;
}

/*
 *  Read a single color register and split it into
 *  colors/transparent. Return != 0 for invalid regno.
 */
static int sgivwfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                             u_int *transp, struct fb_info *info)
{
  if (regno > 255)
    return 1;

  *red =   palette[regno].red << 8;
  *green = palette[regno].green << 8;
  *blue =  palette[regno].blue << 8;
  *transp = 0;
  return 0;
}

/*
 *  Set a single color register. The values supplied are already
 *  rounded down to the hardware's capabilities (according to the
 *  entries in the var structure). Return != 0 for invalid regno.
 */

static int sgivwfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                             u_int transp, struct fb_info *info)
{
  if (regno > 255)
    return 1;
  red >>= 8;
  green >>= 8;
  blue >>= 8;
  palette[regno].red = red;
  palette[regno].green = green;
  palette[regno].blue = blue;

  /* wait for the color map FIFO to have a free entry */
  while (cmap_fifo == 0)
    cmap_fifo = regs->cm_fifo;

  regs->cmap[regno] = (red << 24) | (green << 16) | (blue << 8);
  cmap_fifo--;			/* assume FIFO is filling up */
  return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
    if (con != currcon)
	return;
    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, sgivwfb_setcolreg, info);
    else
	fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1,
		    sgivwfb_setcolreg, info);
}

/* ---------------------------------------------------- */

/*
 *  Get the Fixed Part of the Display
 */
static int sgivwfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info)
{
  struct fb_var_screeninfo *var;

  if (con == -1)
    var = &par_current.var;
  else
    var = &fb_display[con].var;
  sgivwfb_encode_fix(fix, var);
  return 0;
}

/*
 *  Get the User Defined Part of the Display. If a real par get it form there
 */
static int sgivwfb_get_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info)
{
  if (con == -1)
    *var = par_current.var;
  else
    *var = fb_display[con].var;
  return 0;
}

/*
 *  Set the User Defined Part of the Display. Again if par use it to get
 *  real video mode.
 */
static int sgivwfb_set_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info)
{
  int err, activate = var->activate;
  int oldxres, oldyres, oldvxres, oldvyres, oldbpp;
  u_long line_length;
  u_long min_mode;
  int req_dot;
  int test_mode;

  struct dbe_timing_info *timing;

  struct display *display;

  if (con >= 0)
    display = &fb_display[con];
  else
    display = &disp;	/* used during initialization */

  /*
   *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
   *  as FB_VMODE_SMOOTH_XPAN is only used internally
   */

  if (var->vmode & FB_VMODE_CONUPDATE) {
    var->vmode |= FB_VMODE_YWRAP;
    var->xoffset = display->var.xoffset;
    var->yoffset = display->var.yoffset;
  }

  /* XXX FIXME - forcing var's */
  var->xoffset = 0;
  var->yoffset = 0;

  /* Limit bpp to 8, 16, and 32 */
  if (var->bits_per_pixel <= 8)
    var->bits_per_pixel = 8;
  else if (var->bits_per_pixel <= 16)
    var->bits_per_pixel = 16;
  else if (var->bits_per_pixel <= 32)
    var->bits_per_pixel = 32;
  else
    return -EINVAL;

  var->grayscale = 0;           /* No grayscale for now */

  /* determine valid resolution and timing */
  for (min_mode=0; min_mode<DBE_VT_SIZE; min_mode++) {
    if (dbeVTimings[min_mode].width >= var->xres &&
        dbeVTimings[min_mode].height >= var->yres)
      break;
  }

  if (min_mode == DBE_VT_SIZE)
    return -EINVAL;             /* Resolution to high */

  /* XXX FIXME - should try to pick best refresh rate */
  /* for now, pick closest dot-clock within 3MHz*/
#error "Floating point not allowed in kernel"  
  req_dot = (int)((1.0e3/1.0e6) / (1.0e-12 * (float)var->pixclock));
  printk(KERN_INFO "sgivwfb: requested pixclock=%d ps (%d KHz)\n", var->pixclock,
	 req_dot);
  test_mode=min_mode;
  while (dbeVTimings[min_mode].width == dbeVTimings[test_mode].width) {
    if (dbeVTimings[test_mode].cfreq+3000 > req_dot)
      break;
    test_mode++;
  }
  if (dbeVTimings[min_mode].width != dbeVTimings[test_mode].width)
    test_mode--;
  min_mode = test_mode;
  timing = &dbeVTimings[min_mode];
  printk(KERN_INFO "sgivwfb: granted dot-clock=%d KHz\n", timing->cfreq);

  /* Adjust virtual resolution, if necessary */
  if (var->xres > var->xres_virtual || (!ywrap && !ypan))
    var->xres_virtual = var->xres;
  if (var->yres > var->yres_virtual || (!ywrap && !ypan))
    var->yres_virtual = var->yres;

  /*
   *  Memory limit
   */
  line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
  if (line_length*var->yres_virtual > sgivwfb_mem_size)
    return -ENOMEM;             /* Virtual resolution to high */

  switch (var->bits_per_pixel)
    {
    case 8:
      var->red.offset = 0;
      var->red.length = 8;
      var->green.offset = 0;
      var->green.length = 8;
      var->blue.offset = 0;
      var->blue.length = 8;
      var->transp.offset = 0;
      var->transp.length = 0;
      break;
    case 16:	/* RGBA 5551 */
      var->red.offset = 11;
      var->red.length = 5;
      var->green.offset = 6;
      var->green.length = 5;
      var->blue.offset = 1;
      var->blue.length = 5;
      var->transp.offset = 0;
      var->transp.length = 0;
      break;
    case 32:	/* RGB 8888 */
      var->red.offset = 0;
      var->red.length = 8;
      var->green.offset = 8;
      var->green.length = 8;
      var->blue.offset = 16;
      var->blue.length = 8;
      var->transp.offset = 24;
      var->transp.length = 8;
      break;
    }
  var->red.msb_right = 0;
  var->green.msb_right = 0;
  var->blue.msb_right = 0;
  var->transp.msb_right = 0;

  /* set video timing information */
  var->pixclock = (__u32)(1.0e+9/(float)timing->cfreq);
  var->left_margin = timing->htotal - timing->hsync_end;
  var->right_margin = timing->hsync_start - timing->width;
  var->upper_margin = timing->vtotal - timing->vsync_end;
  var->lower_margin = timing->vsync_start - timing->height;
  var->hsync_len = timing->hsync_end - timing->hsync_start;
  var->vsync_len = timing->vsync_end -  timing->vsync_start;

  if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
    oldxres = display->var.xres;
    oldyres = display->var.yres;
    oldvxres = display->var.xres_virtual;
    oldvyres = display->var.yres_virtual;
    oldbpp = display->var.bits_per_pixel;
    display->var = *var;
    par_current.var = *var;
    par_current.timing_num = min_mode;
    if (oldxres != var->xres || oldyres != var->yres ||
	oldvxres != var->xres_virtual || oldvyres != var->yres_virtual ||
	oldbpp != var->bits_per_pixel || !par_current.valid) {
      struct fb_fix_screeninfo fix;
      printk(KERN_INFO "sgivwfb: new video mode xres=%d yres=%d bpp=%d\n",
	     var->xres, var->yres, var->bits_per_pixel);
      printk(KERN_INFO "         vxres=%d vyres=%d\n",
	     var->xres_virtual, var->yres_virtual);
      activate_par(&par_current);
      sgivwfb_encode_fix(&fix, var);
      display->screen_base = (char *)fbmem;
      display->visual = fix.visual;
      display->type = fix.type;
      display->type_aux = fix.type_aux;
      display->ypanstep = fix.ypanstep;
      display->ywrapstep = fix.ywrapstep;
      display->line_length = fix.line_length;
      display->can_soft_blank = 1;
      display->inverse = 0;
      if (oldbpp != var->bits_per_pixel || !par_current.valid) {
	if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
	  return err;
	do_install_cmap(con, info);
      }
      switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
      case 8:
	display->dispsw = &fbcon_cfb8;
	break;
#endif
#ifdef FBCON_HAS_CFB16
      case 16:
	display->dispsw = &fbcon_cfb16;
	display->dispsw_data = fbcon_cmap.cfb16;
	break;
#endif
#ifdef FBCON_HAS_CFB32
      case 32:
	display->dispsw = &fbcon_cfb32;
	display->dispsw_data = fbcon_cmap.cfb32;
	break;
#endif
      default:
	display->dispsw = &fbcon_dummy;
	break;
      }
      par_current.valid = 1;
      if (fb_info.changevar)
	(*fb_info.changevar)(con);
    }
  }
  return 0;
}

/*
 *  Get the Colormap
 */
static int sgivwfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info)
{
  if (con == currcon) /* current console? */
    return fb_get_cmap(cmap, kspc, sgivwfb_getcolreg, info);
  else if (fb_display[con].cmap.len) /* non default colormap? */
    fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
  else
    fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
		 cmap, kspc ? 0 : 2);
  return 0;
}

/*
 *  Set the Colormap
 */
static int sgivwfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info)
{
  int err;

  if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
    int size = fb_display[con].var.bits_per_pixel == 16 ? 32 : 256;
    if ((err = fb_alloc_cmap(&fb_display[con].cmap, size, 0)))
      return err;
  }
  if (con == currcon)			/* current console? */
    return fb_set_cmap(cmap, kspc, sgivwfb_setcolreg, info);
  else
    fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
  return 0;
}

static int sgivwfb_mmap(struct fb_info *info, struct file *file,
                        struct vm_area_struct *vma)
{
  unsigned long size = vma->vm_end - vma->vm_start;
  unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
  if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
	return -EINVAL;
  if (offset+size > sgivwfb_mem_size)
    return -EINVAL;
  offset += sgivwfb_mem_phys;
  pgprot_val(vma->vm_page_prot) = pgprot_val(vma->vm_page_prot) | _PAGE_PCD;
  if (remap_page_range(vma->vm_start, offset, size, vma->vm_page_prot))
    return -EAGAIN;
  vma->vm_file = file;
  printk(KERN_DEBUG "sgivwfb: mmap framebuffer P(%lx)->V(%lx)\n", offset, vma->vm_start);
  return 0;
}

int __init sgivwfb_setup(char *options)
{
  char *this_opt;

  fb_info.fontname[0] = '\0';

  if (!options || !*options)
    return 0;

  while ((this_opt = strsep(&options, ",")) != NULL) {
    if (!strncmp(this_opt, "font:", 5))
      strcpy(fb_info.fontname, this_opt+5);
  }
  return 0;
}

/*
 *  Initialisation
 */
int __init sgivwfb_init(void)
{
  printk(KERN_INFO "sgivwfb: framebuffer at 0x%lx, size %ldk\n",
	 sgivwfb_mem_phys, sgivwfb_mem_size/1024);

  regs = (asregs*)ioremap_nocache(DBE_REG_PHYS, DBE_REG_SIZE);
  if (!regs) {
    printk(KERN_ERR "sgivwfb: couldn't ioremap registers\n");
    goto fail_ioremap_regs;
  }

#ifdef CONFIG_MTRR
  mtrr_add((unsigned long)sgivwfb_mem_phys, sgivwfb_mem_size, MTRR_TYPE_WRCOMB, 1);
#endif

  strcpy(fb_info.modename, sgivwfb_name);
  fb_info.changevar = NULL;
  fb_info.node = -1;
  fb_info.fbops = &sgivwfb_ops;
  fb_info.disp = &disp;
  fb_info.switch_con = &sgivwfbcon_switch;
  fb_info.updatevar = &sgivwfbcon_updatevar;
  fb_info.blank = &sgivwfbcon_blank;
  fb_info.flags = FBINFO_FLAG_DEFAULT;

  fbmem = ioremap_nocache((unsigned long)sgivwfb_mem_phys, sgivwfb_mem_size);
  if (!fbmem) {
    printk(KERN_ERR "sgivwfb: couldn't ioremap fbmem\n");
    goto fail_ioremap_fbmem;
  }

  /* turn on default video mode */
  sgivwfb_set_var(&par_current.var, -1, &fb_info);

  if (register_framebuffer(&fb_info) < 0) {
    printk(KERN_ERR "sgivwfb: couldn't register framebuffer\n");
    goto fail_register_framebuffer;
  }

  printk(KERN_INFO "fb%d: Virtual frame buffer device, using %ldK of video memory\n",
	 GET_FB_IDX(fb_info.node), sgivwfb_mem_size>>10);

  return 0;

 fail_register_framebuffer:
  iounmap((char*)fbmem);
 fail_ioremap_fbmem:
  iounmap(regs);
 fail_ioremap_regs:
  return -ENXIO;
}

static int sgivwfbcon_switch(int con, struct fb_info *info)
{
  /* Do we have to save the colormap? */
  if (fb_display[currcon].cmap.len)
    fb_get_cmap(&fb_display[currcon].cmap, 1, sgivwfb_getcolreg, info);

  currcon = con;
  /* Install new colormap */
  do_install_cmap(con, info);
  return 0;
}

/*
 *  Update the `var' structure (called by fbcon.c)
 */
static int sgivwfbcon_updatevar(int con, struct fb_info *info)
{
    /* Nothing */
    return 0;
}

/*
 *  Blank the display.
 */
static void sgivwfbcon_blank(int blank, struct fb_info *info)
{
    /* Nothing */
}

#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
  return sgivwfb_init();
}

void cleanup_module(void)
{
  unregister_framebuffer(&fb_info);
  dbe_TurnOffDma();
  iounmap(regs);
  iounmap(fbmem);
}

#endif /* MODULE */
