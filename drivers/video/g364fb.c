/* $Id: g364fb.c,v 1.3 1998/08/28 22:43:00 tsbogend Exp $
 *
 * linux/drivers/video/g364fb.c -- Mips Magnum frame buffer device
 *
 * (C) 1998 Thomas Bogendoerfer
 *
 *  This driver is based on tgafb.c
 *
 *	Copyright (C) 1997 Geert Uytterhoeven 
 *	Copyright (C) 1995  Jay Estabrook
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <asm/io.h>
#include <asm/jazz.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>

/* 
 * Various defines for the G364
 */
#define G364_MEM_BASE   0xe4400000
#define G364_PORT_BASE  0xe4000000
#define ID_REG 		0xe4000000  	/* Read only */
#define BOOT_REG 	0xe4080000
#define TIMING_REG 	0xe4080108 	/* to 0x080170 - DON'T TOUCH! */
#define DISPLAY_REG 	0xe4080118
#define VDISPLAY_REG 	0xe4080150
#define MASK_REG 	0xe4080200
#define CTLA_REG 	0xe4080300
#define CURS_TOGGLE 	0x800000
#define BIT_PER_PIX	0x700000	/* bits 22 to 20 of Control A */
#define DELAY_SAMPLE    0x080000
#define PORT_INTER	0x040000
#define PIX_PIPE_DEL	0x030000	/* bits 17 and 16 of Control A */
#define PIX_PIPE_DEL2	0x008000	/* same as above - don't ask me why */
#define TR_CYCLE_TOG	0x004000
#define VRAM_ADR_INC	0x003000	/* bits 13 and 12 of Control A */
#define BLANK_OFF	0x000800
#define FORCE_BLANK	0x000400
#define BLK_FUN_SWTCH	0x000200
#define BLANK_IO	0x000100
#define BLANK_LEVEL	0x000080
#define A_VID_FORM	0x000040
#define D_SYNC_FORM	0x000020
#define FRAME_FLY_PAT	0x000010
#define OP_MODE		0x000008
#define INTL_STAND	0x000004
#define SCRN_FORM	0x000002
#define ENABLE_VTG	0x000001	
#define TOP_REG 	0xe4080400
#define CURS_PAL_REG 	0xe4080508 	/* to 0x080518 */
#define CHKSUM_REG 	0xe4080600 	/* to 0x080610 - unused */
#define CURS_POS_REG 	0xe4080638
#define CLR_PAL_REG 	0xe4080800	/* to 0x080ff8 */
#define CURS_PAT_REG 	0xe4081000	/* to 0x081ff8 */
#define MON_ID_REG 	0xe4100000 	/* unused */
#define RESET_REG 	0xe4180000  	/* Write only */

static int currcon = 0;
static struct display disp;
static struct fb_info fb_info;
static struct { u_char red, green, blue, pad; } palette[256];

static struct fb_fix_screeninfo fb_fix = { { "G364 8plane", } };
static struct fb_var_screeninfo fb_var = { 0, };


/*
 *  Interface used by the world
 */
static int g364fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info);
static int g364fb_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
static int g364fb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
static int g364fb_pan_display(struct fb_var_screeninfo *var, int con,
			      struct fb_info *info);
static int g364fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info);
static int g364fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info);


/*
 *  Interface to the low level console driver
 */
int g364fb_init(void);
static int g364fbcon_switch(int con, struct fb_info *info);
static int g364fbcon_updatevar(int con, struct fb_info *info);
static void g364fbcon_blank(int blank, struct fb_info *info);


/*
 *  Internal routines
 */
static int g364fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			    u_int *transp, struct fb_info *info);
static int g364fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			    u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);


static struct fb_ops g364fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	g364fb_get_fix,
	fb_get_var:	g364fb_get_var,
	fb_set_var:	g364fb_set_var,
	fb_get_cmap:	g364fb_get_cmap,
	fb_set_cmap:	g364fb_set_cmap,
	fb_pan_display:	g364fb_pan_display,
};


void fbcon_g364fb_cursor(struct display *p, int mode, int x, int y)
{
    switch (mode) {
     case CM_ERASE:
	*(unsigned int *) CTLA_REG |= CURS_TOGGLE;
	break;

     case CM_MOVE:
     case CM_DRAW:
	*(unsigned int *) CTLA_REG &= ~CURS_TOGGLE;
	*(unsigned int *) CURS_POS_REG = ((x * fontwidth(p)) << 12) | ((y * fontheight(p))-p->var.yoffset);
	break;
    }
}


static struct display_switch fbcon_g364cfb8 = {
    setup:		fbcon_cfb8_setup,
    bmove:		fbcon_cfb8_bmove,
    clear:		fbcon_cfb8_clear,
    putc:		fbcon_cfb8_putc,
    putcs:		fbcon_cfb8_putcs,
    revc:		fbcon_cfb8_revc,
    cursor:		fbcon_g364fb_cursor,
    clear_margins:	fbcon_cfb8_clear_margins,
    fontwidthmask:	FONTWIDTH(8)
};

/*
 *  Get the Fixed Part of the Display
 */
static int g364fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info)
{
    memcpy(fix, &fb_fix, sizeof(fb_fix));
    return 0;
}


/*
 *  Get the User Defined Part of the Display
 */
static int g364fb_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
    memcpy(var, &fb_var, sizeof(fb_var));
    return 0;
}


/*
 *  Set the User Defined Part of the Display
 */
static int g364fb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
    struct display *display;
    int oldbpp = -1, err;

    if (con >= 0)
	display = &fb_display[con];
    else
	display = &disp;	/* used during initialization */

    if (var->xres > fb_var.xres || var->yres > fb_var.yres ||
	var->xres_virtual > fb_var.xres_virtual ||
	var->yres_virtual > fb_var.yres_virtual ||
	var->bits_per_pixel > fb_var.bits_per_pixel ||
	var->nonstd ||
	(var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED)
	return -EINVAL;
    memcpy(var, &fb_var, sizeof(fb_var));
    
    if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
	oldbpp = display->var.bits_per_pixel;
	display->var = *var;
	*(unsigned int *)TOP_REG = var->yoffset * var->xres;	
    }
    if (oldbpp != var->bits_per_pixel) {
	if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
	    return err;
	do_install_cmap(con, info);
    }
    return 0;
}


/*
 *  Pan or Wrap the Display
 *
 *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 */
static int g364fb_pan_display(struct fb_var_screeninfo *var, int con,
			      struct fb_info *info)
{
    if (var->xoffset || var->yoffset+var->yres > var->yres_virtual)
	return -EINVAL;
    
    *(unsigned int *)TOP_REG = var->yoffset * var->xres;
    return 0;
}

/*
 *  Get the Colormap
 */
static int g364fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
    if (con == currcon) /* current console? */
	return fb_get_cmap(cmap, kspc, g364fb_getcolreg, info);
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
static int g364fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
    int err;

    if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
	if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				 1<<fb_display[con].var.bits_per_pixel, 0)))
	    return err;
    }
    if (con == currcon) {		/* current console? */
	return fb_set_cmap(cmap, kspc, g364fb_setcolreg, info);
    } else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}


/*
 *  Initialisation
 */
int __init g364fb_init(void)
{
    int i,j;
    volatile unsigned int *pal_ptr = (volatile unsigned int *) CLR_PAL_REG;
    volatile unsigned int *curs_pal_ptr = (volatile unsigned int *) CURS_PAL_REG;
    unsigned int xres, yres;
    int mem;

    /* TBD: G364 detection */
    
    /* get the resolution set by ARC console */
    *(volatile unsigned int *)CTLA_REG &= ~ENABLE_VTG;
    xres = (*((volatile unsigned int*)DISPLAY_REG) & 0x00ffffff) * 4;
    yres = (*((volatile unsigned int*)VDISPLAY_REG) & 0x00ffffff) / 2;
    *(volatile unsigned int *)CTLA_REG |= ENABLE_VTG;    

    /* initialise color palette */
    for (i = 0; i < 16; i++) {
	j = color_table[i];
	palette[i].red=default_red[j];
	palette[i].green=default_grn[j];
	palette[i].blue=default_blu[j];
	pal_ptr[i << 1] = (palette[i].red << 16) | (palette[i].green << 8) | palette[i].blue;
    }
    
    /* setup cursor */
    curs_pal_ptr[0] |= 0x00ffffff;
    curs_pal_ptr[2] |= 0x00ffffff;
    curs_pal_ptr[4] |= 0x00ffffff;

    /*
     * first set the whole cursor to transparent
     */
    for (i = 0; i < 512; i++)
	*(unsigned short *)(CURS_PAT_REG+i*8) = 0;

    /*
     * switch the last two lines to cursor palette 3
     * we assume here, that FONTSIZE_X is 8
     */
    *(unsigned short *)(CURS_PAT_REG + 14*64) = 0xffff;
    *(unsigned short *)(CURS_PAT_REG + 15*64) = 0xffff;			    

    fb_var.bits_per_pixel = 8;
    fb_var.xres = fb_var.xres_virtual = xres;
    fb_var.yres = yres;

    fb_fix.line_length = (xres / 8) * fb_var.bits_per_pixel;
    fb_fix.smem_start = 0x40000000; /* physical address */
    /* get size of video memory; this is special for the JAZZ hardware */
    mem = (r4030_read_reg32(JAZZ_R4030_CONFIG) >> 8) & 3;
    fb_fix.smem_len = (1 << (mem*2)) * 512 * 1024;
    fb_fix.type = FB_TYPE_PACKED_PIXELS;
    fb_fix.type_aux = 0;
    fb_fix.visual = FB_VISUAL_PSEUDOCOLOR;    
    fb_fix.xpanstep = 0;
    fb_fix.ypanstep = 1;
    fb_fix.ywrapstep = 0;
    fb_fix.mmio_start = 0;
    fb_fix.mmio_len = 0;
    fb_fix.accel = FB_ACCEL_NONE;
    
    fb_var.yres_virtual = fb_fix.smem_len / xres;
    fb_var.xoffset = fb_var.yoffset = 0;
    fb_var.grayscale = 0;

    fb_var.red.offset = 0;
    fb_var.green.offset = 0;
    fb_var.blue.offset = 0;

    fb_var.red.length = fb_var.green.length = fb_var.blue.length = 8;
    fb_var.red.msb_right = fb_var.green.msb_right = fb_var.blue.msb_right = 0;
    fb_var.transp.offset = fb_var.transp.length = fb_var.transp.msb_right = 0;
    fb_var.nonstd = 0;
    fb_var.activate = 0;
    fb_var.height = fb_var.width = -1;
    fb_var.accel_flags = 0;
    fb_var.pixclock = 39722;
    fb_var.left_margin = 40;
    fb_var.right_margin = 24;
    fb_var.upper_margin = 32;
    fb_var.lower_margin = 11;
    fb_var.hsync_len = 96;
    fb_var.vsync_len = 2;
    fb_var.sync = 0;
    fb_var.vmode = FB_VMODE_NONINTERLACED;

    disp.var = fb_var;
    disp.cmap.start = 0;
    disp.cmap.len = 0;
    disp.cmap.red = disp.cmap.green = disp.cmap.blue = disp.cmap.transp = NULL;
    disp.screen_base = (char *)G364_MEM_BASE; /* virtual kernel address */
    disp.visual = fb_fix.visual;
    disp.type = fb_fix.type;
    disp.type_aux = fb_fix.type_aux;
    disp.ypanstep = fb_fix.ypanstep;
    disp.ywrapstep = fb_fix.ywrapstep;
    disp.line_length = fb_fix.line_length;
    disp.can_soft_blank = 1;
    disp.inverse = 0;
    disp.dispsw = &fbcon_g364cfb8;

    strcpy(fb_info.modename, fb_fix.id);
    fb_info.node = -1;
    fb_info.fbops = &g364fb_ops;
    fb_info.disp = &disp;
    fb_info.fontname[0] = '\0';
    fb_info.changevar = NULL;
    fb_info.switch_con = &g364fbcon_switch;
    fb_info.updatevar = &g364fbcon_updatevar;
    fb_info.blank = &g364fbcon_blank;
    fb_info.flags = FBINFO_FLAG_DEFAULT;

    g364fb_set_var(&fb_var, -1, &fb_info);

    if (register_framebuffer(&fb_info) < 0)
	return -EINVAL;

    printk("fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.node),
	   fb_fix.id);
    return 0;
}


static int g364fbcon_switch(int con, struct fb_info *info)
{
    /* Do we have to save the colormap? */
    if (fb_display[currcon].cmap.len)
	fb_get_cmap(&fb_display[currcon].cmap, 1, g364fb_getcolreg, info);

    currcon = con;
    /* Install new colormap */
    do_install_cmap(con, info);
    g364fbcon_updatevar(con, info);    
    return 0;
}

/*
 *  Update the `var' structure (called by fbcon.c)
 */
static int g364fbcon_updatevar(int con, struct fb_info *info)
{
    if (con == currcon) {
	struct fb_var_screeninfo *var = &fb_display[currcon].var;

	/* hardware scrolling */
	*(unsigned int *)TOP_REG = var->yoffset * var->xres;	
    }
    return 0;
}

/*
 *  Blank the display.
 */
static void g364fbcon_blank(int blank, struct fb_info *info)
{
    if (blank)
	*(unsigned int *) CTLA_REG |= FORCE_BLANK;	
    else
	*(unsigned int *) CTLA_REG &= ~FORCE_BLANK;	
}

/*
 *  Read a single color register and split it into
 *  colors/transparent. Return != 0 for invalid regno.
 */
static int g364fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			    u_int *transp, struct fb_info *info)
{
    if (regno > 255)
	return 1;
    *red = (palette[regno].red << 8) | palette[regno].red;
    *green = (palette[regno].green << 8) | palette[regno].green;
    *blue = (palette[regno].blue << 8) | palette[regno].blue;
    *transp = 0;
    return 0;
}

/*
 *  Set a single color register. Return != 0 for invalid regno.
 */
static int g364fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			    u_int transp, struct fb_info *info)
{
    volatile unsigned int *ptr = (volatile unsigned int *) CLR_PAL_REG;

    if (regno > 255)
	return 1;

    red >>= 8;
    green >>= 8;
    blue >>=8;
    palette[regno].red = red;
    palette[regno].green = green;
    palette[regno].blue = blue;
    
    ptr[regno << 1] = (red << 16) | (green << 8) | blue;

    return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
    if (con != currcon)
	return;
    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, g364fb_setcolreg, info);
    else
	fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1,
		    g364fb_setcolreg, info);
}

MODULE_LICENSE("GPL");
