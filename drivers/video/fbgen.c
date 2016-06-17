/*
 * linux/drivers/video/fbgen.c -- Generic routines for frame buffer devices
 *
 *  Created 3 Jan 1998 by Geert Uytterhoeven
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/fb.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <video/fbcon.h>

static int currcon = 0;


/* ---- `Generic' versions of the frame buffer device operations ----------- */


/**
 *	fbgen_get_fix - get fixed part of display
 *	@fix: fb_fix_screeninfo structure
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Get the fixed information part of the display and place it
 *	into @fix for virtual console @con on device @info.
 *
 *	Returns negative errno on error, or zero on success.
 *
 */

int fbgen_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;
    char par[info2->parsize];

    if (con == -1)
	fbhw->get_par(&par, info2);
    else {
	int err;

	if ((err = fbhw->decode_var(&fb_display[con].var, &par, info2)))
	    return err;
    }
    memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    return fbhw->encode_fix(fix, &par, info2);
}


/**
 *	fbgen_get_var - get user defined part of display
 *	@var: fb_var_screeninfo structure
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Get the user defined part of the display and place it into @var
 *	for virtual console @con on device @info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int fbgen_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;
    char par[info2->parsize];

    if (con == -1) {
	fbhw->get_par(&par, info2);
	fbhw->encode_var(var, &par, info2);
    } else
	*var = fb_display[con].var;
    return 0;
}


/**
 *	fbgen_set_var - set the user defined part of display
 *	@var: fb_var_screeninfo user defined part of the display
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Set the user defined part of the display as dictated by @var
 *	for virtual console @con on device @info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int fbgen_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    int err;
    int oldxres, oldyres, oldbpp, oldxres_virtual, oldyres_virtual, oldyoffset;
    struct fb_bitfield oldred, oldgreen, oldblue;

    if ((err = fbgen_do_set_var(var, con == currcon, info2)))
	return err;
    if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
	oldxres = fb_display[con].var.xres;
	oldyres = fb_display[con].var.yres;
	oldxres_virtual = fb_display[con].var.xres_virtual;
	oldyres_virtual = fb_display[con].var.yres_virtual;
	oldbpp = fb_display[con].var.bits_per_pixel;
	oldred = fb_display[con].var.red;
	oldgreen = fb_display[con].var.green;
	oldblue = fb_display[con].var.blue;
	oldyoffset = fb_display[con].var.yoffset;
	fb_display[con].var = *var;
	if (oldxres != var->xres || oldyres != var->yres ||
	    oldxres_virtual != var->xres_virtual ||
	    oldyres_virtual != var->yres_virtual ||
	    oldbpp != var->bits_per_pixel ||
	    (!(memcmp(&oldred, &(var->red), sizeof(struct fb_bitfield)))) || 
	    (!(memcmp(&oldgreen, &(var->green), sizeof(struct fb_bitfield)))) ||
	    (!(memcmp(&oldblue, &(var->blue), sizeof(struct fb_bitfield)))) ||
	    oldyoffset != var->yoffset) {
	    fbgen_set_disp(con, info2);
	    if (info->changevar)
		(*info->changevar)(con);
	    if ((err = fb_alloc_cmap(&fb_display[con].cmap, 0, 0)))
		return err;
	    fbgen_install_cmap(con, info2);
	}
    }
    var->activate = 0;
    return 0;
}


/**
 *	fbgen_get_cmap - get the colormap
 *	@cmap: frame buffer colormap structure
 *	@kspc: boolean, 0 copy local, 1 put_user() function
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Gets the colormap for virtual console @con and places it into
 *	@cmap for device @info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int fbgen_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		   struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;

    if (con == currcon)			/* current console ? */
	return fb_get_cmap(cmap, kspc, fbhw->getcolreg, info);
    else
	if (fb_display[con].cmap.len)	/* non default colormap ? */
	    fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else {
	    int size = fb_display[con].var.bits_per_pixel == 16 ? 64 : 256;
	    fb_copy_cmap(fb_default_cmap(size), cmap, kspc ? 0 : 2);
	}
    return 0;
}


/**
 *	fbgen_set_cmap - set the colormap
 *	@cmap: frame buffer colormap structure
 *	@kspc: boolean, 0 copy local, 1 get_user() function
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Sets the colormap @cmap for virtual console @con on
 *	device @info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int fbgen_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		   struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;
    int err;

    if (!fb_display[con].cmap.len) {	/* no colormap allocated ? */
	int size = fb_display[con].var.bits_per_pixel == 16 ? 64 : 256;
	if ((err = fb_alloc_cmap(&fb_display[con].cmap, size, 0)))
	    return err;
    }
    if (con == currcon)			/* current console ? */
	return fb_set_cmap(cmap, kspc, fbhw->setcolreg, info);
    else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}


/**
 *	fbgen_pan_display - pan or wrap the display
 *	@var: frame buffer user defined part of display
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Pan or wrap virtual console @con for device @info.
 *
 *	This call looks only at xoffset, yoffset and the
 *	FB_VMODE_YWRAP flag in @var.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int fbgen_pan_display(struct fb_var_screeninfo *var, int con,
		      struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;
    int xoffset = var->xoffset;
    int yoffset = var->yoffset;
    int err;

    if (xoffset < 0 ||
	xoffset+fb_display[con].var.xres > fb_display[con].var.xres_virtual ||
	yoffset < 0 ||
	yoffset+fb_display[con].var.yres > fb_display[con].var.yres_virtual)
	return -EINVAL;
    if (con == currcon) {
	if (fbhw->pan_display) {
	    if ((err = fbhw->pan_display(var, info2)))
		return err;
	} else
	    return -EINVAL;
    }
    fb_display[con].var.xoffset = var->xoffset;
    fb_display[con].var.yoffset = var->yoffset;
    if (var->vmode & FB_VMODE_YWRAP)
	fb_display[con].var.vmode |= FB_VMODE_YWRAP;
    else
	fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;

    return 0;
}


/* ---- Helper functions --------------------------------------------------- */


/**
 *	fbgen_do_set_var - change the video mode
 *	@var: frame buffer user defined part of display
 *	@isactive: boolean, 0 inactive, 1 active
 *	@info: generic frame buffer info structure
 *
 *	Change the video mode settings for device @info.  If @isactive
 *	is non-zero, the changes will be activated immediately.
 *
 *	Return negative errno on error, or zero for success.
 *
 */

int fbgen_do_set_var(struct fb_var_screeninfo *var, int isactive,
		     struct fb_info_gen *info)
{
    struct fbgen_hwswitch *fbhw = info->fbhw;
    int err, activate;
    char par[info->parsize];

    if ((err = fbhw->decode_var(var, &par, info)))
	return err;
    activate = var->activate;
    if (((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) && isactive)
	fbhw->set_par(&par, info);
    fbhw->encode_var(var, &par, info);
    var->activate = activate;
    return 0;
}


/**
 *	fbgen_set_disp - set generic display
 *	@con: virtual console number
 *	@info: generic frame buffer info structure
 *
 *	Sets a display on virtual console @con for device @info.
 *
 */

void fbgen_set_disp(int con, struct fb_info_gen *info)
{
    struct fbgen_hwswitch *fbhw = info->fbhw;
    struct fb_fix_screeninfo fix;
    char par[info->parsize];
    struct display *display;

    if (con >= 0)
	display = &fb_display[con];
    else
	display = info->info.disp;	/* used during initialization */

    if (con == -1)
	fbhw->get_par(&par, info);
    else
	fbhw->decode_var(&fb_display[con].var, &par, info);
    memset(&fix, 0, sizeof(struct fb_fix_screeninfo));
    fbhw->encode_fix(&fix, &par, info);

    display->visual = fix.visual;
    display->type = fix.type;
    display->type_aux = fix.type_aux;
    display->ypanstep = fix.ypanstep;
    display->ywrapstep = fix.ywrapstep;
    display->line_length = fix.line_length;
    if (info->fbhw->blank || fix.visual == FB_VISUAL_PSEUDOCOLOR ||
	fix.visual == FB_VISUAL_DIRECTCOLOR)
	display->can_soft_blank = 1;
    else
	display->can_soft_blank = 0;
    fbhw->set_disp(&par, display, info);
#if 0 /* FIXME: generic inverse is not supported yet */
    display->inverse = (fix.visual == FB_VISUAL_MONO01 ? !inverse : inverse);
#else
    display->inverse = fix.visual == FB_VISUAL_MONO01;
#endif
}


/**
 *	fbgen_install_cmap - install the current colormap
 *	@con: virtual console number
 *	@info: generic frame buffer info structure
 *
 *	Installs the current colormap for virtual console @con on
 *	device @info.
 *
 */

void fbgen_install_cmap(int con, struct fb_info_gen *info)
{
    struct fbgen_hwswitch *fbhw = info->fbhw;
    if (con != currcon)
	return;
    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, fbhw->setcolreg, &info->info);
    else {
	int size = fb_display[con].var.bits_per_pixel == 16 ? 64 : 256;
	fb_set_cmap(fb_default_cmap(size), 1, fbhw->setcolreg, &info->info);
    }
}


/**
 *	fbgen_update_var - update user defined part of display
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Updates the user defined part of the display ('var'
 *	structure) on virtual console @con for device @info.
 *	This function is called by fbcon.c.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int fbgen_update_var(int con, struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;
    int err;

    if (fbhw->pan_display) {
        if ((err = fbhw->pan_display(&fb_display[con].var, info2)))
            return err;
    }
    return 0;
}


/**
 *	fbgen_switch - switch to a different virtual console.
 *	@con: virtual console number
 *	@info: frame buffer info structure
 *
 *	Switch to virtuall console @con on device @info.
 *
 *	Returns zero.
 *
 */

int fbgen_switch(int con, struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;

    /* Do we have to save the colormap ? */
    if (fb_display[currcon].cmap.len)
	fb_get_cmap(&fb_display[currcon].cmap, 1, fbhw->getcolreg,
		    &info2->info);
    fbgen_do_set_var(&fb_display[con].var, 1, info2);
    currcon = con;
    /* Install new colormap */
    fbgen_install_cmap(con, info2);
    return 0;
}


/**
 *	fbgen_blank - blank the screen
 *	@blank: boolean, 0 unblank, 1 blank
 *	@info: frame buffer info structure
 *
 *	Blank the screen on device @info.
 *
 */

void fbgen_blank(int blank, struct fb_info *info)
{
    struct fb_info_gen *info2 = (struct fb_info_gen *)info;
    struct fbgen_hwswitch *fbhw = info2->fbhw;
    u16 black[16];
    struct fb_cmap cmap;

    if (fbhw->blank && !fbhw->blank(blank, info2))
	return;
    if (blank) {
	memset(black, 0, 16*sizeof(u16));
	cmap.red = black;
	cmap.green = black;
	cmap.blue = black;
	cmap.transp = NULL;
	cmap.start = 0;
	cmap.len = 16;
	fb_set_cmap(&cmap, 1, fbhw->setcolreg, info);
    } else
	fbgen_install_cmap(currcon, info2);
}
MODULE_LICENSE("GPL");


    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(fbgen_get_var);
EXPORT_SYMBOL(fbgen_get_cmap);
EXPORT_SYMBOL(fbgen_get_fix);
EXPORT_SYMBOL(fbgen_set_var);
EXPORT_SYMBOL(fbgen_set_cmap);
EXPORT_SYMBOL(fbgen_set_disp);
EXPORT_SYMBOL(fbgen_install_cmap);
EXPORT_SYMBOL(fbgen_pan_display);
EXPORT_SYMBOL(fbgen_update_var);
EXPORT_SYMBOL(fbgen_do_set_var);
EXPORT_SYMBOL(fbgen_switch);
EXPORT_SYMBOL(fbgen_blank);
