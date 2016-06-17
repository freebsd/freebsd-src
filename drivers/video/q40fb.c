#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>
#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/system.h>
/*#include <asm/irq.h>*/
#include <asm/q40_master.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <asm/pgtable.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>

#define FBIOSETSCROLLMODE   0x4611

#define Q40_PHYS_SCREEN_ADDR 0xFE800000
static unsigned long q40_screen_addr;

static u16 fbcon_cmap_cfb16[16];

/* frame buffer operations */

static int q40fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			struct fb_info *info);
static int q40fb_get_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info);
static int q40fb_set_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info);
static int q40fb_get_cmap(struct fb_cmap *cmap,int kspc,int con,
			 struct fb_info *info);
static int q40fb_set_cmap(struct fb_cmap *cmap,int kspc,int con,
			 struct fb_info *info);
static int q40fb_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg, int con,
		      struct fb_info *info);

static int q40con_switch(int con, struct fb_info *info);
static int q40con_updatevar(int con, struct fb_info *info);
static void q40con_blank(int blank, struct fb_info *info);

static void q40fb_set_disp(int con, struct fb_info *info);

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;
static struct fb_ops q40fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	q40fb_get_fix,
	fb_get_var:	q40fb_get_var,
	fb_set_var:	q40fb_set_var,
	fb_get_cmap:	q40fb_get_cmap,
	fb_set_cmap:	q40fb_set_cmap,
	fb_ioctl:	q40fb_ioctl,
};

static int currcon=0;

static char q40fb_name[]="Q40";

static int q40fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id,"Q40");
	fix->smem_start=q40_screen_addr;
	fix->smem_len=1024*1024;
	fix->type=FB_TYPE_PACKED_PIXELS;
	fix->type_aux=0;
	fix->visual=FB_VISUAL_TRUECOLOR;  /* good approximation so far ..*/;
	fix->xpanstep=0;
	fix->ypanstep=0;
	fix->ywrapstep=0;
        fix->line_length=1024*2;

	/* no mmio,accel ...*/

	return 0;

}
        
static int q40fb_get_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info)
{
	memset(var, 0, sizeof(struct fb_var_screeninfo));

	var->xres=1024;
	var->yres=512;
	var->xres_virtual=1024;
	var->yres_virtual=512;
	var->xoffset=0;
	var->yoffset=0;
	var->bits_per_pixel=16;
	var->grayscale=0;
	var->nonstd=0;
	var->activate=FB_ACTIVATE_NOW;
	var->height=230;     /* approx for my 17" monitor, more important */
	var->width=300;      /* than the absolute values is the unusual aspect ratio*/

	var->red.offset=6; /*6*/
	var->red.length=5;
	var->green.offset=11; /*11*/
	var->green.length=5;
	var->blue.offset=0;
	var->blue.length=6;
	var->transp.length=0;

	var->pixclock=0;
	var->left_margin=0;
	var->right_margin=0;
	var->hsync_len=0;
	var->vsync_len=0;
	var->sync=0;
	var->vmode=FB_VMODE_NONINTERLACED;

	return 0;

}

static int q40fb_set_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info)
{
	if(var->xres!=1024) 
		return -EINVAL;
	if(var->yres!=512)
		return -EINVAL;
	if(var->xres_virtual!=1024)
		return -EINVAL;
	if(var->yres_virtual!=512)
		return -EINVAL;
	if(var->xoffset!=0)
		return -EINVAL;
	if(var->yoffset!=0)
		return -EINVAL;
	if(var->bits_per_pixel!=16)
		return -EINVAL;
	if(var->grayscale!=0)
		return -EINVAL;
	if(var->nonstd!=0)
		return -EINVAL;
	if(var->activate!=FB_ACTIVATE_NOW)
		return -EINVAL;
// ignore broken tools trying to set these values
#if 0
	if(var->pixclock!=0)
		return -EINVAL;
	if(var->left_margin!=0)
		return -EINVAL;
	if(var->right_margin!=0)
		return -EINVAL;
	if(var->hsync_len!=0)
		return -EINVAL;
	if(var->vsync_len!=0)
		return -EINVAL;
	if(var->sync!=0)
		return -EINVAL;
	if(var->vmode!=FB_VMODE_NONINTERLACED)
		return -EINVAL;
#endif

	return 0;

}

static int q40_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			 unsigned *blue, unsigned *transp,
			 struct fb_info *info)
{
    /*
     *  Read a single color register and split it into colors/transparent.
     *  The return values must have a 16 bit magnitude.
     *  Return != 0 for invalid regno.
     */
    if (regno>=16) return 1;

    *transp=0;
    *green = ((fbcon_cmap_cfb16[regno]>>11) & 31)<<11;
    *red   = ((fbcon_cmap_cfb16[regno]>>6) & 31)<<11;
    *blue  = ((fbcon_cmap_cfb16[regno]) & 63)<<10;

    return 0;
}

static int q40_setcolreg(unsigned regno, unsigned red, unsigned green,
			 unsigned blue, unsigned transp, struct fb_info *info)
{
    /*
     *  Set a single color register. The values supplied have a 16 bit
     *  magnitude.
     *  Return != 0 for invalid regno.
     */
  
  red>>=11;
  green>>=11;
  blue>>=10;

    if (regno < 16) {
      fbcon_cmap_cfb16[regno] = ((red & 31) <<6) |
	                         ((green & 31) << 11) |
	                         (blue & 63);
    }
    return 0;
}

static int q40fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			 struct fb_info *info)
{
#if 1
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, q40_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
			     cmap, kspc ? 0 : 2);
	return 0;
#else
	printk(KERN_ERR "get cmap not supported\n");

	return -EINVAL;
#endif
}

static int q40fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			 struct fb_info *info)
{
#if 1
	int err;

	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
					 1<<fb_display[con].var.bits_per_pixel,
					 0)))
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, q40_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
#else
	printk(KERN_ERR "set cmap not supported\n");

	return -EINVAL;
#endif
}

static int q40fb_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg, int con,
		      struct fb_info *info)
{
#if 0
        unsigned long i;
	struct display *display;

	if (con>=0)
	  display = &fb_display[con];
	else
	  display = &disp[0];

        if (cmd == FBIOSETSCROLLMODE)
	  {
	    i = verify_area(VERIFY_READ, (void *)arg, sizeof(unsigned long));
	    if (!i) 
	      {
		copy_from_user(&i, (void *)arg, sizeof(unsigned long));
		display->scrollmode = i;
	      }
	    q40_updatescrollmode(display);
	    return i;
	  }
#endif
	return -EINVAL;
}

static void q40fb_set_disp(int con, struct fb_info *info)
{
  struct fb_fix_screeninfo fix;
  struct display *display;

  q40fb_get_fix(&fix, con, info);

  if (con>=0)
    display = &fb_display[con];
  else 	
    display = &disp[0];

  if (con<0) con=0;

   display->screen_base = (char *)fix.smem_start;
   display->visual = fix.visual;
   display->type = fix.type;
   display->type_aux = fix.type_aux;
   display->ypanstep = fix.ypanstep;
   display->ywrapstep = fix.ywrapstep;
   display->can_soft_blank = 0;
   display->inverse = 0;
   display->line_length = fix.line_length;

   display->scrollmode = SCROLL_YREDRAW;

#ifdef FBCON_HAS_CFB16
   display->dispsw = &fbcon_cfb16;
   disp->dispsw_data = fbcon_cmap_cfb16;
#else
   display->dispsw = &fbcon_dummy;
#endif
}
  
int __init q40fb_init(void)
{

        if ( !MACH_IS_Q40)
	  return -ENXIO;
#if 0
        q40_screen_addr = kernel_map(Q40_PHYS_SCREEN_ADDR, 1024*1024,
					   KERNELMAP_NO_COPYBACK, NULL);
#else
	q40_screen_addr = Q40_PHYS_SCREEN_ADDR; /* mapped in q40/config.c */
#endif

	fb_info.changevar=NULL;
	strcpy(&fb_info.modename[0],q40fb_name);
	fb_info.fontname[0]=0;
	fb_info.disp=disp;
	fb_info.switch_con=&q40con_switch;
	fb_info.updatevar=&q40con_updatevar;
	fb_info.blank=&q40con_blank;	
	fb_info.node = -1;
	fb_info.fbops = &q40fb_ops;
	fb_info.flags = FBINFO_FLAG_DEFAULT;  /* not as module for now */
	
	master_outb(3,DISPLAY_CONTROL_REG);

        q40fb_get_var(&disp[0].var, 0, &fb_info);
	q40fb_set_disp(-1, &fb_info);

	if (register_framebuffer(&fb_info) < 0) {
		printk(KERN_ERR "unable to register Q40 frame buffer\n");
		return -EINVAL;
	}

        printk(KERN_INFO "fb%d: Q40 frame buffer alive and kicking !\n",
	       GET_FB_IDX(fb_info.node));
	return 0;
}	

	
static int q40con_switch(int con, struct fb_info *info)
{ 
	currcon=con;
	
	return 0;

}

static int q40con_updatevar(int con, struct fb_info *info)
{
	return 0;
}

static void q40con_blank(int blank, struct fb_info *info)
{
}

MODULE_LICENSE("GPL");
