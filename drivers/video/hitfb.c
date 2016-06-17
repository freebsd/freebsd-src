/*
 * $Id: hitfb.c,v 1.2 2000/07/04 06:24:46 yaegashi Exp $
 * linux/drivers/video/hitfb.c -- Hitachi LCD frame buffer device
 * (C) 1999 Mihai Spatar
 * (C) 2000 YAEGASHI Takeshi
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
 
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/nubus.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/hd64461.h>

#include <linux/fb.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>


struct hitfb_par
{
    int x, y;
    int bpp;
};


struct hitfb_info {
    struct fb_info_gen gen;
    struct display disp;
    struct hitfb_par current_par;
    struct fb_var_screeninfo default_var;
    int current_par_valid;
    unsigned long hit_videobase, hit_videosize;
    union {
#ifdef FBCON_HAS_CFB16
	u16 cfb16[16];
#endif
    } fbcon_cmap;
} fb_info = {
    {},
    {},
    {},
    {},
    0, 0, 0,
    {},
};


static void hitfb_set_par(const void *fb_par, struct fb_info_gen *info);
static int hitfb_encode_var(struct fb_var_screeninfo *var, const void *fb_par,
			    struct fb_info_gen *info);


static void hitfb_detect(void)
{
    struct hitfb_par par;
    unsigned short lcdclor, ldr3, ldvntr;

    fb_info.hit_videobase = CONFIG_HD64461_IOBASE + 0x02000000;
    fb_info.hit_videosize = (MACH_HP680 || MACH_HP690) ? 1024*1024 : 512*1024;

    lcdclor = inw(HD64461_LCDCLOR);
    ldvntr = inw(HD64461_LDVNTR);
    ldr3 = inw(HD64461_LDR3);

    switch(ldr3&15) {
    default:
    case 4:
        par.bpp = 8;
	par.x = lcdclor;
	break;
    case 8:
        par.bpp = 16;
	par.x = lcdclor/2;
	break;
    }

    par.y = ldvntr+1;

    hitfb_set_par(&par, NULL);
    hitfb_encode_var(&fb_info.default_var, &par, NULL);
}


static int hitfb_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par,
			     struct fb_info_gen *info)
{
    const struct hitfb_par *par = fb_par;

    memset(fix, 0, sizeof(struct fb_fix_screeninfo));

    strcpy(fix->id, "Hitachi HD64461");
    fix->smem_start = fb_info.hit_videobase;
    fix->smem_len = fb_info.hit_videosize;
    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->type_aux = 0;
    fix->visual = (par->bpp == 8) ?
	FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
    fix->xpanstep = 0;
    fix->ypanstep = 0;
    fix->ywrapstep = 0;

    switch(par->bpp) {
    default:
    case 8:
	fix->line_length = par->x;
	break;
    case 16:
	fix->line_length = par->x*2;
	break;
    }

    return 0;
}


static int hitfb_decode_var(const struct fb_var_screeninfo *var, void *fb_par,
			     struct fb_info_gen *info)
{
    struct hitfb_par *par = fb_par;

    par->x = var->xres;
    par->y = var->yres;
    par->bpp = var->bits_per_pixel;
    return 0;
}


static int hitfb_encode_var(struct fb_var_screeninfo *var, const void *fb_par,
			     struct fb_info_gen *info)
{
    const struct hitfb_par *par = fb_par;

    memset(var, 0, sizeof(*var));

    var->xres = par->x;
    var->yres = par->y;
    var->xres_virtual = var->xres;
    var->yres_virtual = var->yres;
    var->xoffset = 0;
    var->yoffset = 0;
    var->bits_per_pixel = par->bpp;
    var->grayscale = 0;
    var->transp.offset = 0;
    var->transp.length = 0;
    var->transp.msb_right = 0;
    var->nonstd = 0;
    var->activate = 0;
    var->height = -1;
    var->width = -1;
    var->vmode = FB_VMODE_NONINTERLACED;
    var->pixclock = 0;
    var->sync = 0;
    var->left_margin = 0;
    var->right_margin = 0;
    var->upper_margin = 0;
    var->lower_margin = 0;
    var->hsync_len = 0;
    var->vsync_len = 0;

    switch (var->bits_per_pixel) {

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

	case 16:	/* RGB 565 */
	    var->red.offset = 11;
	    var->red.length = 5;
	    var->green.offset = 5;
	    var->green.length = 6;
	    var->blue.offset = 0;
	    var->blue.length = 5;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
    }

    var->red.msb_right = 0;
    var->green.msb_right = 0;
    var->blue.msb_right = 0;
    var->transp.msb_right = 0;

    return 0;
}


static void hitfb_get_par(void *par, struct fb_info_gen *info)
{
    *(struct hitfb_par *)par = fb_info.current_par;
}


static void hitfb_set_par(const void *fb_par, struct fb_info_gen *info)
{
    const struct hitfb_par *par = fb_par;
    fb_info.current_par = *par;
    fb_info.current_par_valid = 1;
}


static int hitfb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			   unsigned *blue, unsigned *transp,
			   struct fb_info *info)
{
    if (regno > 255)
	return 1;	

    outw(regno<<8, HD64461_CPTRAR);
    *red = inw(HD64461_CPTRDR)<<10;
    *green = inw(HD64461_CPTRDR)<<10;
    *blue = inw(HD64461_CPTRDR)<<10;
    *transp = 0;
    
    return 0;
}


static int hitfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
    if (regno > 255)
	return 1;
    
    outw(regno<<8, HD64461_CPTWAR);
    outw(red>>10, HD64461_CPTWDR);
    outw(green>>10, HD64461_CPTWDR);
    outw(blue>>10, HD64461_CPTWDR);
    
    if(regno<16) {
	switch(fb_info.current_par.bpp) {
#ifdef FBCON_HAS_CFB16
	case 16:
	    fb_info.fbcon_cmap.cfb16[regno] =
		((red   & 0xf800)      ) |
		((green & 0xfc00) >>  5) |
		((blue  & 0xf800) >> 11);
	    break;
#endif
	}
    }

    return 0;
}


static int hitfb_pan_display(const struct fb_var_screeninfo *var,
			     struct fb_info_gen *info)
{
    if (!fb_info.current_par_valid)
	return -EINVAL;

    return 0;
}


static int hitfb_blank(int blank_mode, struct fb_info_gen *info)
{
    if (!fb_info.current_par_valid)
	return 1;

    return 0;
}


static void hitfb_set_disp(const void *fb_par, struct display *disp,
			    struct fb_info_gen *info)
{
    const struct hitfb_par *par = fb_par;

    disp->screen_base = (void *)fb_info.hit_videobase;
    disp->scrollmode = SCROLL_YREDRAW;

    switch(((struct hitfb_par *)par)->bpp) {
#ifdef FBCON_HAS_CFB8
    case 8:
	disp->dispsw = &fbcon_cfb8;
	break;
#endif
#ifdef FBCON_HAS_CFB16
    case 16:
	disp->dispsw = &fbcon_cfb16;
	disp->dispsw_data = fb_info.fbcon_cmap.cfb16;
	break;
#endif
    default:
	disp->dispsw = &fbcon_dummy;
    }
}


struct fbgen_hwswitch hitfb_switch = {
    hitfb_detect,
    hitfb_encode_fix,
    hitfb_decode_var,
    hitfb_encode_var,
    hitfb_get_par,
    hitfb_set_par,
    hitfb_getcolreg,
    hitfb_setcolreg,
    hitfb_pan_display,
    hitfb_blank,
    hitfb_set_disp
};


static struct fb_ops hitfb_ops = {
    owner:		THIS_MODULE,
    fb_get_fix:		fbgen_get_fix,
    fb_get_var:		fbgen_get_var,
    fb_set_var:		fbgen_set_var,
    fb_get_cmap:	fbgen_get_cmap,
    fb_set_cmap:	fbgen_set_cmap,
    fb_pan_display:	fbgen_pan_display,
};


int __init hitfb_init(void)
{
    strcpy(fb_info.gen.info.modename, "Hitachi HD64461");
    fb_info.gen.info.node = -1;
    fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
    fb_info.gen.info.fbops = &hitfb_ops;
    fb_info.gen.info.disp = &fb_info.disp;
    fb_info.gen.info.changevar = NULL;
    fb_info.gen.info.switch_con = &fbgen_switch;
    fb_info.gen.info.updatevar = &fbgen_update_var;
    fb_info.gen.info.blank = &fbgen_blank;
    fb_info.gen.parsize = sizeof(struct hitfb_par);
    fb_info.gen.fbhw = &hitfb_switch;
    fb_info.gen.fbhw->detect();
    
    fbgen_get_var(&fb_info.disp.var, -1, &fb_info.gen.info);
    fb_info.disp.var.activate = FB_ACTIVATE_NOW;
    fbgen_do_set_var(&fb_info.disp.var, 1, &fb_info.gen);
    fbgen_set_disp(-1, &fb_info.gen);
    fbgen_install_cmap(0, &fb_info.gen);
    
    if(register_framebuffer(&fb_info.gen.info)<0) return -EINVAL;
    
    printk(KERN_INFO "fb%d: %s frame buffer device\n",
	   GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);
    
    return 0;
}


void hitfb_cleanup(struct fb_info *info)
{
    unregister_framebuffer(info);
}


#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
    return hitfb_init();
}

void cleanup_module(void)
{
  hitfb_cleanup(void);
}
#endif


/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
