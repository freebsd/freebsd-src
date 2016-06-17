/*
 *  linux/drivers/video/sbusfb.c -- SBUS or UPA based frame buffer device
 *
 *	Copyright (C) 1998 Jakub Jelinek
 *
 *  This driver is partly based on the Open Firmware console driver
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  and SPARC console subsystem
 *
 *      Copyright (C) 1995 Peter Zaitcev (zaitcev@yahoo.com)
 *      Copyright (C) 1995-1997 David S. Miller (davem@caip.rutgers.edu)
 *      Copyright (C) 1995-1996 Miguel de Icaza (miguel@nuclecu.unam.mx)
 *      Copyright (C) 1996 Dave Redman (djhr@tadpole.co.uk)
 *      Copyright (C) 1996-1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *      Copyright (C) 1996 Eddie C. Dost (ecd@skynet.be)
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
#include <linux/fb.h>
#include <linux/selection.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>	/* io_remap_page_range() */

#include <video/sbusfb.h>

#define DEFAULT_CURSOR_BLINK_RATE       (2*HZ/5)

#define CURSOR_SHAPE			1
#define CURSOR_BLINK			2

    /*
     *  Interface used by the world
     */

int sbusfb_init(void);
int sbusfb_setup(char*);

static int currcon;
static int defx_margin = -1, defy_margin = -1;
static char fontname[40] __initdata = { 0 };
static int curblink __initdata = 1;
static struct {
	int depth;
	int xres, yres;
	int x_margin, y_margin;
} def_margins [] = {
	{ 8, 1280, 1024, 64, 80 },
	{ 8, 1152, 1024, 64, 80 },
	{ 8, 1152, 900,  64, 18 },
	{ 8, 1024, 768,  0,  0 },
	{ 8, 800, 600, 16, 12 },
	{ 8, 640, 480, 0, 0 },
	{ 1, 1152, 900,  8,  18 },
	{ 0 },
};

static int sbusfb_open(struct fb_info *info, int user);
static int sbusfb_release(struct fb_info *info, int user);
static int sbusfb_mmap(struct fb_info *info, struct file *file, 
			struct vm_area_struct *vma);
static int sbusfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			struct fb_info *info);
static int sbusfb_get_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info);
static int sbusfb_set_var(struct fb_var_screeninfo *var, int con,
			struct fb_info *info);
static int sbusfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info);
static int sbusfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info);
static int sbusfb_ioctl(struct inode *inode, struct file *file, u_int cmd,
			    u_long arg, int con, struct fb_info *info);
static void sbusfb_cursor(struct display *p, int mode, int x, int y);
static void sbusfb_clear_margin(struct display *p, int s);
			    

    /*
     *  Interface to the low level console driver
     */

static int sbusfbcon_switch(int con, struct fb_info *info);
static int sbusfbcon_updatevar(int con, struct fb_info *info);
static void sbusfbcon_blank(int blank, struct fb_info *info);


    /*
     *  Internal routines
     */

static int sbusfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			    u_int *transp, struct fb_info *info);
static int sbusfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			    u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);

static struct fb_ops sbusfb_ops = {
	owner:		THIS_MODULE,
	fb_open:	sbusfb_open,
	fb_release:	sbusfb_release,
	fb_get_fix:	sbusfb_get_fix,
	fb_get_var:	sbusfb_get_var,
	fb_set_var:	sbusfb_set_var,
	fb_get_cmap:	sbusfb_get_cmap,
	fb_set_cmap:	sbusfb_set_cmap,
	fb_ioctl:	sbusfb_ioctl,
	fb_mmap:	sbusfb_mmap,
};

    /*
     *  Open/Release the frame buffer device
     */

static int sbusfb_open(struct fb_info *info, int user)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	
	if (user) {
		if (fb->open == 0) {
			fb->mmaped = 0;
			fb->vtconsole = -1;
		}
		fb->open++;
	} else
		fb->consolecnt++;
	return 0;
}

static int sbusfb_release(struct fb_info *info, int user)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);

	if (user) {	
		fb->open--;
		if (fb->open == 0) {
			if (fb->vtconsole != -1) {
				vt_cons[fb->vtconsole]->vc_mode = KD_TEXT;
				if (fb->mmaped) {
					fb->graphmode--;
					sbusfb_clear_margin(&fb_display[fb->vtconsole], 0);
				}
			}
			if (fb->reset)
				fb->reset(fb);
		}
	} else
		fb->consolecnt--;
	return 0;
}

static unsigned long sbusfb_mmapsize(struct fb_info_sbusfb *fb, long size)
{
	if (size == SBUS_MMAP_EMPTY) return 0;
	if (size >= 0) return size;
	return fb->type.fb_size * (-size);
}

static int sbusfb_mmap(struct fb_info *info, struct file *file, 
			struct vm_area_struct *vma)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	unsigned int size, page, r, map_size;
	unsigned long map_offset = 0;
	unsigned long off;
	int i;
                                        
	size = vma->vm_end - vma->vm_start;
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	off = vma->vm_pgoff << PAGE_SHIFT;

	/* To stop the swapper from even considering these pages */
	vma->vm_flags |= (VM_SHM| VM_LOCKED);
	

	/* Each page, see which map applies */
	for (page = 0; page < size; ){
		map_size = 0;
		for (i = 0; fb->mmap_map[i].size; i++)
			if (fb->mmap_map[i].voff == off+page) {
				map_size = sbusfb_mmapsize(fb,fb->mmap_map[i].size);
#ifdef __sparc_v9__
#define POFF_MASK	(PAGE_MASK|0x1UL)
#else
#define POFF_MASK	(PAGE_MASK)
#endif				
				map_offset = (fb->physbase + fb->mmap_map[i].poff) & POFF_MASK;
				break;
			}
		if (!map_size){
			page += PAGE_SIZE;
			continue;
		}
		if (page + map_size > size)
			map_size = size - page;
		r = io_remap_page_range (vma->vm_start+page, map_offset, map_size, vma->vm_page_prot, fb->iospace);
		if (r)
			return -EAGAIN;
		page += map_size;
	}
	
	vma->vm_flags |= VM_IO;
	if (!fb->mmaped) {
		int lastconsole = 0;
		
		if (info->display_fg)
			lastconsole = info->display_fg->vc_num;
		fb->mmaped = 1;
		if (fb->consolecnt && fb_display[lastconsole].fb_info == info) {
			fb->vtconsole = lastconsole;
			fb->graphmode++;
			vt_cons [lastconsole]->vc_mode = KD_GRAPHICS;
			vc_cons[lastconsole].d->vc_sw->con_cursor(vc_cons[lastconsole].d,CM_ERASE);
		} else if (fb->unblank && !fb->blanked)
			(*fb->unblank)(fb);
	}
	return 0;
}

static void sbusfb_clear_margin(struct display *p, int s)
{
	struct fb_info_sbusfb *fb = sbusfbinfod(p);

	if (fb->switch_from_graph)
		(*fb->switch_from_graph)(fb);
	if (fb->fill) {
		unsigned short rects [16];

		rects [0] = 0;
		rects [1] = 0;
		rects [2] = fb->var.xres_virtual;
		rects [3] = fb->y_margin;
		rects [4] = 0;
		rects [5] = fb->y_margin;
		rects [6] = fb->x_margin;
		rects [7] = fb->var.yres_virtual;
		rects [8] = fb->var.xres_virtual - fb->x_margin;
		rects [9] = fb->y_margin;
		rects [10] = fb->var.xres_virtual;
		rects [11] = fb->var.yres_virtual;
		rects [12] = fb->x_margin;
		rects [13] = fb->var.yres_virtual - fb->y_margin;
		rects [14] = fb->var.xres_virtual - fb->x_margin;
		rects [15] = fb->var.yres_virtual;
		(*fb->fill)(fb, p, s, 4, rects);
	} else {
		unsigned char *fb_base = p->screen_base, *q;
		int skip_bytes = fb->y_margin * fb->var.xres_virtual;
		int scr_size = fb->var.xres_virtual * fb->var.yres_virtual;
		int h, he, incr, size;

		he = fb->var.yres;
		if (fb->var.bits_per_pixel == 1) {
			fb_base -= (skip_bytes + fb->x_margin) / 8;
			skip_bytes /= 8;
			scr_size /= 8;
			fb_memset255 (fb_base, skip_bytes - fb->x_margin / 8);
			fb_memset255 (fb_base + scr_size - skip_bytes + fb->x_margin / 8, skip_bytes - fb->x_margin / 8);
			incr = fb->var.xres_virtual / 8;
			size = fb->x_margin / 8 * 2;
			for (q = fb_base + skip_bytes - fb->x_margin / 8, h = 0;
			     h <= he; q += incr, h++)
				fb_memset255 (q, size);
		} else {
			fb_base -= (skip_bytes + fb->x_margin);
			memset (fb_base, attr_bgcol(p,s), skip_bytes - fb->x_margin);
			memset (fb_base + scr_size - skip_bytes + fb->x_margin, attr_bgcol(p,s), skip_bytes - fb->x_margin);
			incr = fb->var.xres_virtual;
			size = fb->x_margin * 2;
			for (q = fb_base + skip_bytes - fb->x_margin, h = 0;
			     h <= he; q += incr, h++)
				memset (q, attr_bgcol(p,s), size);
		}
	}
}

static void sbusfb_disp_setup(struct display *p)
{
	struct fb_info_sbusfb *fb = sbusfbinfod(p);

	if (fb->setup)
		fb->setup(p);	
	sbusfb_clear_margin(p, 0);
}

    /*
     *  Get the Fixed Part of the Display
     */

static int sbusfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);

	memcpy(fix, &fb->fix, sizeof(struct fb_fix_screeninfo));
	return 0;
}

    /*
     *  Get the User Defined Part of the Display
     */

static int sbusfb_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);

	memcpy(var, &fb->var, sizeof(struct fb_var_screeninfo));
	return 0;
}

    /*
     *  Set the User Defined Part of the Display
     */

static int sbusfb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
       struct display *display;
       int activate = var->activate;

       if(con >= 0)
               display = &fb_display[con];
       else
               display = info->disp;

       /* simple check for equality until fully implemented -E */
       if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
               if (display->var.xres != var->xres ||
                       display->var.yres != var->yres ||
                       display->var.xres_virtual != var->xres_virtual ||
                       display->var.yres_virtual != var->yres_virtual ||
                       display->var.bits_per_pixel != var->bits_per_pixel ||
                       display->var.accel_flags != var->accel_flags) {
                       return -EINVAL;
               }
       }
       return 0;

}

    /*
     *  Hardware cursor
     */
     
static int sbus_hw_scursor (struct fbcursor *cursor, struct fb_info_sbusfb *fb)
{
	int op;
	int i, bytes = 0;
	struct fbcursor f;
	char red[2], green[2], blue[2];
	
	if (copy_from_user (&f, cursor, sizeof(struct fbcursor)))
		return -EFAULT;
	op = f.set;
	if (op & FB_CUR_SETSHAPE){
		if ((u32) f.size.fbx > fb->cursor.hwsize.fbx)
			return -EINVAL;
		if ((u32) f.size.fby > fb->cursor.hwsize.fby)
			return -EINVAL;
		if (f.size.fbx > 32)
			bytes = f.size.fby << 3;
		else
			bytes = f.size.fby << 2;
	}
	if (op & FB_CUR_SETCMAP){
		if (f.cmap.index || f.cmap.count != 2)
			return -EINVAL;
		if (copy_from_user (red, f.cmap.red, 2) ||
		    copy_from_user (green, f.cmap.green, 2) ||
		    copy_from_user (blue, f.cmap.blue, 2))
			return -EFAULT;
	}
	if (op & FB_CUR_SETCMAP)
		(*fb->setcursormap) (fb, red, green, blue);
	if (op & FB_CUR_SETSHAPE){
		u32 u;
		
		fb->cursor.size = f.size;
		memset ((void *)&fb->cursor.bits, 0, sizeof (fb->cursor.bits));
		if (copy_from_user (fb->cursor.bits [0], f.mask, bytes) ||
		    copy_from_user (fb->cursor.bits [1], f.image, bytes))
			return -EFAULT;
		if (f.size.fbx <= 32) {
			u = 0xffffffff << (32 - f.size.fbx);
			for (i = fb->cursor.size.fby - 1; i >= 0; i--) {
				fb->cursor.bits [0][i] &= u;
				fb->cursor.bits [1][i] &= fb->cursor.bits [0][i];
			}
		} else {
			u = 0xffffffff << (64 - f.size.fbx);
			for (i = fb->cursor.size.fby - 1; i >= 0; i--) {
				fb->cursor.bits [0][2*i+1] &= u;
				fb->cursor.bits [1][2*i] &= fb->cursor.bits [0][2*i];
				fb->cursor.bits [1][2*i+1] &= fb->cursor.bits [0][2*i+1];
			}
		}
		(*fb->setcurshape) (fb);
	}
	if (op & (FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT)){
		if (op & FB_CUR_SETCUR)
			fb->cursor.enable = f.enable;
		if (op & FB_CUR_SETPOS)
			fb->cursor.cpos = f.pos;
		if (op & FB_CUR_SETHOT)
			fb->cursor.chot = f.hot;
		(*fb->setcursor) (fb);
	}
	return 0;
}

static unsigned char hw_cursor_cmap[2] = { 0, 0xff };

static void
sbusfb_cursor_timer_handler(unsigned long dev_addr)
{
	struct fb_info_sbusfb *fb = (struct fb_info_sbusfb *)dev_addr;
        
	if (!fb->setcursor) return;
                                
	if (fb->cursor.mode & CURSOR_BLINK) {
		fb->cursor.enable ^= 1;
		fb->setcursor(fb);
	}
	
	fb->cursor.timer.expires = jiffies + fb->cursor.blink_rate;
	add_timer(&fb->cursor.timer);
}

static void sbusfb_cursor(struct display *p, int mode, int x, int y)
{
	struct fb_info_sbusfb *fb = sbusfbinfod(p);
	
	switch (mode) {
	case CM_ERASE:
		fb->cursor.mode &= ~CURSOR_BLINK;
		fb->cursor.enable = 0;
		(*fb->setcursor)(fb);
		break;
				  
	case CM_MOVE:
	case CM_DRAW:
		if (fb->cursor.mode & CURSOR_SHAPE) {
			fb->cursor.size.fbx = fontwidth(p);
			fb->cursor.size.fby = fontheight(p);
			fb->cursor.chot.fbx = 0;
			fb->cursor.chot.fby = 0;
			fb->cursor.enable = 1;
			memset (fb->cursor.bits, 0, sizeof (fb->cursor.bits));
			fb->cursor.bits[0][fontheight(p) - 2] = (0xffffffff << (32 - fontwidth(p)));
			fb->cursor.bits[1][fontheight(p) - 2] = (0xffffffff << (32 - fontwidth(p)));
			fb->cursor.bits[0][fontheight(p) - 1] = (0xffffffff << (32 - fontwidth(p)));
			fb->cursor.bits[1][fontheight(p) - 1] = (0xffffffff << (32 - fontwidth(p)));
			(*fb->setcursormap) (fb, hw_cursor_cmap, hw_cursor_cmap, hw_cursor_cmap);
			(*fb->setcurshape) (fb);
		}
		fb->cursor.mode = CURSOR_BLINK;
		if (fontwidthlog(p))
			fb->cursor.cpos.fbx = (x << fontwidthlog(p)) + fb->x_margin;
		else
			fb->cursor.cpos.fbx = (x * fontwidth(p)) + fb->x_margin;
		if (fontheightlog(p))
			fb->cursor.cpos.fby = (y << fontheightlog(p)) + fb->y_margin;
		else
			fb->cursor.cpos.fby = (y * fontheight(p)) + fb->y_margin;
		(*fb->setcursor)(fb);
		break;
	}
}

    /*
     *  Get the Colormap
     */

static int sbusfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			 struct fb_info *info)
{
	if (!info->display_fg || con == info->display_fg->vc_num) /* current console? */
		return fb_get_cmap(cmap, kspc, sbusfb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), cmap, kspc ? 0 : 2);
	return 0;
}

    /*
     *  Set the Colormap
     */

static int sbusfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			 struct fb_info *info)
{
	int err;
	struct display *disp;

	if (con >= 0)
		disp = &fb_display[con];
	else
		disp = info->disp;
	if (!disp->cmap.len) {	/* no colormap allocated? */
		if ((err = fb_alloc_cmap(&disp->cmap, 1<<disp->var.bits_per_pixel, 0)))
			return err;
	}
	if (con == currcon) {			/* current console? */
		err = fb_set_cmap(cmap, kspc, sbusfb_setcolreg, info);
		if (!err) {
			struct fb_info_sbusfb *fb = sbusfbinfo(info);
			
			if (fb->loadcmap)
				(*fb->loadcmap)(fb, &fb_display[con], cmap->start, cmap->len);
		}
		return err;
	} else
		fb_copy_cmap(cmap, &disp->cmap, kspc ? 0 : 1);
	return 0;
}

static int sbusfb_ioctl(struct inode *inode, struct file *file, u_int cmd,
			u_long arg, int con, struct fb_info *info)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	int i;
	int lastconsole;
	
	switch (cmd){
	case FBIOGTYPE:		/* return frame buffer type */
		if (copy_to_user((struct fbtype *)arg, &fb->type, sizeof(struct fbtype)))
			return -EFAULT;
		break;
	case FBIOGATTR: {
		struct fbgattr *fba = (struct fbgattr *) arg;
		
		i = verify_area (VERIFY_WRITE, (void *) arg, sizeof (struct fbgattr));
		if (i) return i;
		if (__put_user(fb->emulations[0], &fba->real_type) ||
		    __put_user(0, &fba->owner) ||
		    __copy_to_user(&fba->fbtype, &fb->type,
				   sizeof(struct fbtype)) ||
		    __put_user(0, &fba->sattr.flags) ||
		    __put_user(fb->type.fb_type, &fba->sattr.emu_type) ||
		    __put_user(-1, &fba->sattr.dev_specific[0]))
			return -EFAULT;
		for (i = 0; i < 4; i++) {
			if (put_user(fb->emulations[i], &fba->emu_types[i]))
				return -EFAULT;
		}
		break;
	}
	case FBIOSATTR:
		i = verify_area (VERIFY_READ, (void *) arg, sizeof (struct fbsattr));
		if (i) return i;
		return -EINVAL;
	case FBIOSVIDEO:
		if (fb->consolecnt) {
			lastconsole = info->display_fg->vc_num;
			if (vt_cons[lastconsole]->vc_mode == KD_TEXT)
 				break;
 		}
		if (get_user(i, (int *)arg))
			return -EFAULT;
		if (i){
			if (!fb->blanked || !fb->unblank)
				break;
			if (fb->consolecnt || (fb->open && fb->mmaped))
				(*fb->unblank)(fb);
			fb->blanked = 0;
		} else {
			if (fb->blanked || !fb->blank)
				break;
			(*fb->blank)(fb);
			fb->blanked = 1;
		}
		break;
	case FBIOGVIDEO:
		if (put_user(fb->blanked, (int *) arg))
			return -EFAULT;
		break;
	case FBIOGETCMAP_SPARC: {
		char *rp, *gp, *bp;
		int end, count, index;
		struct fbcmap *cmap;

		if (!fb->loadcmap)
			return -EINVAL;
		i = verify_area (VERIFY_READ, (void *) arg, sizeof (struct fbcmap));
		if (i) return i;
		cmap = (struct fbcmap *) arg;
		if (__get_user(count, &cmap->count) ||
		    __get_user(index, &cmap->index))
			return -EFAULT;
		if ((index < 0) || (index > 255))
			return -EINVAL;
		if (index + count > 256)
			count = 256 - index;
		if (__get_user(rp, &cmap->red) ||
		    __get_user(gp, &cmap->green) ||
		    __get_user(bp, &cmap->blue))
			return -EFAULT;
		if (verify_area (VERIFY_WRITE, rp, count))
			return -EFAULT;
		if (verify_area (VERIFY_WRITE, gp, count))
			return -EFAULT;
		if (verify_area (VERIFY_WRITE, bp, count))
			return -EFAULT;
		end = index + count;
		for (i = index; i < end; i++){
			if (__put_user(fb->color_map CM(i,0), rp) ||
			    __put_user(fb->color_map CM(i,1), gp) ||
			    __put_user(fb->color_map CM(i,2), bp))
				return -EFAULT;
			rp++; gp++; bp++;
		}
		(*fb->loadcmap)(fb, NULL, index, count);
		break;			
	}
	case FBIOPUTCMAP_SPARC: {	/* load color map entries */
		char *rp, *gp, *bp;
		int end, count, index;
		struct fbcmap *cmap;
		
		if (!fb->loadcmap || !fb->color_map)
			return -EINVAL;
		i = verify_area (VERIFY_READ, (void *) arg, sizeof (struct fbcmap));
		if (i) return i;
		cmap = (struct fbcmap *) arg;
		if (__get_user(count, &cmap->count) ||
		    __get_user(index, &cmap->index))
			return -EFAULT;
		if ((index < 0) || (index > 255))
			return -EINVAL;
		if (index + count > 256)
			count = 256 - index;
		if (__get_user(rp, &cmap->red) ||
		    __get_user(gp, &cmap->green) ||
		    __get_user(bp, &cmap->blue))
			return -EFAULT;
		if (verify_area (VERIFY_READ, rp, count))
			return -EFAULT;
		if (verify_area (VERIFY_READ, gp, count))
			return -EFAULT;
		if (verify_area (VERIFY_READ, bp, count))
			return -EFAULT;

		end = index + count;
		for (i = index; i < end; i++){
			if (__get_user(fb->color_map CM(i,0), rp))
				return -EFAULT;
			if (__get_user(fb->color_map CM(i,1), gp))
				return -EFAULT;
			if (__get_user(fb->color_map CM(i,2), bp))
				return -EFAULT;
			rp++; gp++; bp++;
		}
		(*fb->loadcmap)(fb, NULL, index, count);
		break;			
	}
	case FBIOGCURMAX: {
		struct fbcurpos *p = (struct fbcurpos *) arg;
		if (!fb->setcursor) return -EINVAL;
		if(verify_area (VERIFY_WRITE, p, sizeof (struct fbcurpos)))
			return -EFAULT;
		if (__put_user(fb->cursor.hwsize.fbx, &p->fbx) ||
		    __put_user(fb->cursor.hwsize.fby, &p->fby))
			return -EFAULT;
		break;
	}
	case FBIOSCURSOR:
		if (!fb->setcursor) return -EINVAL;
 		if (fb->consolecnt) {
 			lastconsole = info->display_fg->vc_num; 
 			if (vt_cons[lastconsole]->vc_mode == KD_TEXT)
 				return -EINVAL; /* Don't let graphics programs hide our nice text cursor */
			fb->cursor.mode = CURSOR_SHAPE; /* Forget state of our text cursor */
		}
		return sbus_hw_scursor ((struct fbcursor *) arg, fb);

	case FBIOSCURPOS:
		if (!fb->setcursor) return -EINVAL;
		/* Don't let graphics programs move our nice text cursor */
 		if (fb->consolecnt) {
 			lastconsole = info->display_fg->vc_num; 
 			if (vt_cons[lastconsole]->vc_mode == KD_TEXT)
 				return -EINVAL; /* Don't let graphics programs move our nice text cursor */
 		}
		if (copy_from_user(&fb->cursor.cpos, (void *)arg, sizeof(struct fbcurpos)))
			return -EFAULT;
		(*fb->setcursor) (fb);
		break;
	default:
		if (fb->ioctl)
			return fb->ioctl(fb, cmd, arg);
		return -EINVAL;
	}		
	return 0;
}

    /*
     *  Setup: parse used options
     */

int __init sbusfb_setup(char *options)
{
	char *p;
	
	for (p = options;;) {
		if (!strncmp(p, "nomargins", 9)) {
			defx_margin = 0; defy_margin = 0;
		} else if (!strncmp(p, "margins=", 8)) {
			int i, j;
			char *q;
			
			i = simple_strtoul(p+8,&q,10);
			if (i >= 0 && *q == 'x') {
			    j = simple_strtoul(q+1,&q,10);
			    if (j >= 0 && (*q == ' ' || !*q)) {
			    	defx_margin = i; defy_margin = j;
			    }
			}
		} else if (!strncmp(p, "font=", 5)) {
			int i;
			
			for (i = 0; i < sizeof(fontname) - 1; i++)
				if (p[i+5] == ' ' || !p[i+5])
					break;
			memcpy(fontname, p+5, i);
			fontname[i] = 0;
		} else if (!strncmp(p, "noblink", 7))
			curblink = 0;
		while (*p && *p != ' ' && *p != ',') p++;
		if (*p != ',') break;
		p++;
	}
	return 0;
}

static int sbusfbcon_switch(int con, struct fb_info *info)
{
	int x_margin, y_margin;
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	int lastconsole;
    
	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, sbusfb_getcolreg, info);

	if (info->display_fg) {
		lastconsole = info->display_fg->vc_num;
		if (lastconsole != con && 
		    (fontwidth(&fb_display[lastconsole]) != fontwidth(&fb_display[con]) ||
		     fontheight(&fb_display[lastconsole]) != fontheight(&fb_display[con])))
			fb->cursor.mode |= CURSOR_SHAPE;
	}
	x_margin = (fb_display[con].var.xres_virtual - fb_display[con].var.xres) / 2;
	y_margin = (fb_display[con].var.yres_virtual - fb_display[con].var.yres) / 2;
	if (fb->margins)
		fb->margins(fb, &fb_display[con], x_margin, y_margin);
	if (fb->graphmode || fb->x_margin != x_margin || fb->y_margin != y_margin) {
		fb->x_margin = x_margin; fb->y_margin = y_margin;
		sbusfb_clear_margin(&fb_display[con], 0);
	}
	currcon = con;
	/* Install new colormap */
	do_install_cmap(con, info);
	return 0;
}

    /*
     *  Update the `var' structure (called by fbcon.c)
     */

static int sbusfbcon_updatevar(int con, struct fb_info *info)
{
	/* Nothing */
	return 0;
}

    /*
     *  Blank the display.
     */

static void sbusfbcon_blank(int blank, struct fb_info *info)
{
    struct fb_info_sbusfb *fb = sbusfbinfo(info);
    
    if (blank && fb->blank)
    	return fb->blank(fb);
    else if (!blank && fb->unblank)
    	return fb->unblank(fb);
}

    /*
     *  Read a single color register and split it into
     *  colors/transparent. Return != 0 for invalid regno.
     */

static int sbusfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			  u_int *transp, struct fb_info *info)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);

	if (!fb->color_map || regno > 255)
		return 1;
	*red = (fb->color_map CM(regno, 0)<<8) | fb->color_map CM(regno, 0);
	*green = (fb->color_map CM(regno, 1)<<8) | fb->color_map CM(regno, 1);
	*blue = (fb->color_map CM(regno, 2)<<8) | fb->color_map CM(regno, 2);
	*transp = 0;
	return 0;
}


    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int sbusfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			    u_int transp, struct fb_info *info)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);

	if (!fb->color_map || regno > 255)
		return 1;
	red >>= 8;
	green >>= 8;
	blue >>= 8;
	fb->color_map CM(regno, 0) = red;
	fb->color_map CM(regno, 1) = green;
	fb->color_map CM(regno, 2) = blue;
	return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
	struct fb_info_sbusfb *fb = sbusfbinfo(info);
	
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, sbusfb_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
			    1, sbusfb_setcolreg, info);
	if (fb->loadcmap)
		(*fb->loadcmap)(fb, &fb_display[con], 0, 256);
}

static int sbusfb_set_font(struct display *p, int width, int height)
{
	int margin;
	int w = p->var.xres_virtual, h = p->var.yres_virtual;
	int depth = p->var.bits_per_pixel;
	struct fb_info_sbusfb *fb = sbusfbinfod(p);
	int x_margin, y_margin;
	
	if (depth > 8) depth = 8;
	x_margin = 0;
	y_margin = 0;
	if (defx_margin < 0 || defy_margin < 0) {
		for (margin = 0; def_margins[margin].depth; margin++)
			if (w == def_margins[margin].xres &&
			    h == def_margins[margin].yres &&
			    depth == def_margins[margin].depth) {
				x_margin = def_margins[margin].x_margin;
				y_margin = def_margins[margin].y_margin;
				break;
			}
	} else {
		x_margin = defx_margin;
		y_margin = defy_margin;
	}
	x_margin += ((w - 2*x_margin) % width) / 2;
	y_margin += ((h - 2*y_margin) % height) / 2;

	p->var.xres = w - 2*x_margin;
	p->var.yres = h - 2*y_margin;
	
	fb->cursor.mode |= CURSOR_SHAPE;
	
	if (fb->margins)
		fb->margins(fb, p, x_margin, y_margin);
	if (fb->x_margin != x_margin || fb->y_margin != y_margin) {
		fb->x_margin = x_margin; fb->y_margin = y_margin;
		sbusfb_clear_margin(p, 0);
	}

	return 1;
}

void sbusfb_palette(int enter)
{
	int i;
	struct display *p;
	
	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		p = &fb_display[i];
		if (p->dispsw && p->dispsw->setup == sbusfb_disp_setup &&
		    p->fb_info->display_fg &&
		    p->fb_info->display_fg->vc_num == i) {
			struct fb_info_sbusfb *fb = sbusfbinfod(p);

			if (fb->restore_palette) {
				if (enter)
					fb->restore_palette(fb);
				else if (vt_cons[i]->vc_mode != KD_GRAPHICS)
				         vc_cons[i].d->vc_sw->con_set_palette(vc_cons[i].d, color_table);
			}
		}
	}
}

    /*
     *  Initialisation
     */
     
extern void (*prom_palette)(int);

static void __init sbusfb_init_fb(int node, int parent, int fbtype,
				  struct sbus_dev *sbdp)
{
	struct fb_fix_screeninfo *fix;
	struct fb_var_screeninfo *var;
	struct display *disp;
	struct fb_info_sbusfb *fb;
	struct fbtype *type;
	int linebytes, w, h, depth;
	char *p = NULL;
	int margin;

	fb = kmalloc(sizeof(struct fb_info_sbusfb), GFP_ATOMIC);
	if (!fb) {
		prom_printf("Could not allocate sbusfb structure\n");
		return;
	}
	
	if (!prom_palette)
		prom_palette = sbusfb_palette;
	
	memset(fb, 0, sizeof(struct fb_info_sbusfb));
	fix = &fb->fix;
	var = &fb->var;
	disp = &fb->disp;
	type = &fb->type;
	
	spin_lock_init(&fb->lock);
	fb->prom_node = node;
	fb->prom_parent = parent;
	fb->sbdp = sbdp;
	if (sbdp)
		fb->iospace = sbdp->reg_addrs[0].which_io;

	type->fb_type = fbtype;
	memset(&fb->emulations, 0xff, sizeof(fb->emulations));
	fb->emulations[0] = fbtype;
	
#ifndef __sparc_v9__
	disp->screen_base = (unsigned char *)prom_getintdefault(node, "address", 0);
#endif
	
	type->fb_height = h = prom_getintdefault(node, "height", 900);
	type->fb_width  = w = prom_getintdefault(node, "width", 1152);
sizechange:
	type->fb_depth  = depth = (fbtype == FBTYPE_SUN2BW) ? 1 : 8;
	linebytes = prom_getintdefault(node, "linebytes", w * depth / 8);
	type->fb_size   = PAGE_ALIGN((linebytes) * h);
	
	if (defx_margin < 0 || defy_margin < 0) {
		for (margin = 0; def_margins[margin].depth; margin++)
			if (w == def_margins[margin].xres &&
			    h == def_margins[margin].yres &&
			    depth == def_margins[margin].depth) {
				fb->x_margin = def_margins[margin].x_margin;
				fb->y_margin = def_margins[margin].y_margin;
				break;
			}
	} else {
		fb->x_margin = defx_margin;
		fb->y_margin = defy_margin;
	}
	fb->x_margin += ((w - 2*fb->x_margin) & 7) / 2;
	fb->y_margin += ((h - 2*fb->y_margin) & 15) / 2;

	var->xres_virtual = w;
	var->yres_virtual = h;
	var->xres = w - 2*fb->x_margin;
	var->yres = h - 2*fb->y_margin;
	
	var->bits_per_pixel = depth;
	var->height = var->width = -1;
	var->pixclock = 10000;
	var->vmode = FB_VMODE_NONINTERLACED;
	var->red.length = var->green.length = var->blue.length = 8;

	fix->line_length = linebytes;
	fix->smem_len = type->fb_size;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	
	fb->info.node = -1;
	fb->info.fbops = &sbusfb_ops;
	fb->info.disp = disp;
	strcpy(fb->info.fontname, fontname);
	fb->info.changevar = NULL;
	fb->info.switch_con = &sbusfbcon_switch;
	fb->info.updatevar = &sbusfbcon_updatevar;
	fb->info.blank = &sbusfbcon_blank;
	fb->info.flags = FBINFO_FLAG_DEFAULT;
	
	fb->cursor.hwsize.fbx = 32;
	fb->cursor.hwsize.fby = 32;
	
	if (depth > 1 && !fb->color_map)
		fb->color_map = kmalloc(256 * 3, GFP_ATOMIC);
		
	switch(fbtype) {
#ifdef CONFIG_FB_CREATOR
	case FBTYPE_CREATOR:
		p = creatorfb_init(fb); break;
#endif
#ifdef CONFIG_FB_CGSIX
	case FBTYPE_SUNFAST_COLOR:
		p = cgsixfb_init(fb); break;
#endif
#ifdef CONFIG_FB_CGTHREE
	case FBTYPE_SUN3COLOR:
		p = cgthreefb_init(fb); break;
#endif
#ifdef CONFIG_FB_TCX
	case FBTYPE_TCXCOLOR:
		p = tcxfb_init(fb); break;
#endif
#ifdef CONFIG_FB_LEO
	case FBTYPE_SUNLEO:
		p = leofb_init(fb); break;
#endif
#ifdef CONFIG_FB_BWTWO
	case FBTYPE_SUN2BW:
		p = bwtwofb_init(fb); break;
#endif
#ifdef CONFIG_FB_CGFOURTEEN
	case FBTYPE_MDICOLOR:
		p = cgfourteenfb_init(fb); break;
#endif
#ifdef CONFIG_FB_P9100
	case FBTYPE_P9100COLOR:
		/* Temporary crock. For now we are a cg3 */
		p = p9100fb_init(fb); type->fb_type = FBTYPE_SUN3COLOR; break;
#endif
	}
	
	if (!p) {
		if (fb->color_map)
			kfree(fb->color_map);
		kfree(fb);
		return;
	}
	
	if (p == SBUSFBINIT_SIZECHANGE)
		goto sizechange;

	disp->dispsw = &fb->dispsw;
	if (fb->setcursor) {
		fb->dispsw.cursor = sbusfb_cursor;
		if (curblink) {
			fb->cursor.blink_rate = DEFAULT_CURSOR_BLINK_RATE;
			init_timer(&fb->cursor.timer);
			fb->cursor.timer.expires = jiffies + fb->cursor.blink_rate;
			fb->cursor.timer.data = (unsigned long)fb;
			fb->cursor.timer.function = sbusfb_cursor_timer_handler;
			add_timer(&fb->cursor.timer);
		}
	}
	fb->cursor.mode = CURSOR_SHAPE;
	fb->dispsw.set_font = sbusfb_set_font;
	fb->setup = fb->dispsw.setup;
	fb->dispsw.setup = sbusfb_disp_setup;
	fb->dispsw.clear_margins = NULL;

	disp->var = *var;
	disp->visual = fix->visual;
	disp->type = fix->type;
	disp->type_aux = fix->type_aux;
	disp->line_length = fix->line_length;
	
	if (fb->blank)
		disp->can_soft_blank = 1;

	sbusfb_set_var(var, -1, &fb->info);

	if (register_framebuffer(&fb->info) < 0) {
		if (fb->color_map)
			kfree(fb->color_map);
		kfree(fb);
		return;
	}
	printk(KERN_INFO "fb%d: %s\n", GET_FB_IDX(fb->info.node), p);
}

static inline int known_card(char *name)
{
	char *p;
	for (p = name; *p && *p != ','; p++);
	if (*p == ',') name = p + 1;
	if (!strcmp(name, "cgsix") || !strcmp(name, "cgthree+"))
		return FBTYPE_SUNFAST_COLOR;
	if (!strcmp(name, "cgthree") || !strcmp(name, "cgRDI"))
		return FBTYPE_SUN3COLOR;
	if (!strcmp(name, "cgfourteen"))
		return FBTYPE_MDICOLOR;
	if (!strcmp(name, "leo"))
		return FBTYPE_SUNLEO;
	if (!strcmp(name, "bwtwo"))
		return FBTYPE_SUN2BW;
	if (!strcmp(name, "tcx"))
		return FBTYPE_TCXCOLOR;
	if (!strcmp(name, "p9100"))
		return FBTYPE_P9100COLOR;
	return FBTYPE_NOTYPE;
}

#ifdef CONFIG_FB_CREATOR
static void creator_fb_scan_siblings(int root)
{
	int node, child;

	child = prom_getchild(root);
	for (node = prom_searchsiblings(child, "SUNW,ffb"); node;
	     node = prom_searchsiblings(prom_getsibling(node), "SUNW,ffb"))
		sbusfb_init_fb(node, root, FBTYPE_CREATOR, NULL);
	for (node = prom_searchsiblings(child, "SUNW,afb"); node;
	     node = prom_searchsiblings(prom_getsibling(node), "SUNW,afb"))
		sbusfb_init_fb(node, root, FBTYPE_CREATOR, NULL);
}

static void creator_fb_scan(void)
{
	int root;

	creator_fb_scan_siblings(prom_root_node);

	root = prom_getchild(prom_root_node);
	for (root = prom_searchsiblings(root, "upa"); root;
	     root = prom_searchsiblings(prom_getsibling(root), "upa"))
		creator_fb_scan_siblings(root);
}
#endif

int __init sbusfb_init(void)
{
	int type;
	struct sbus_dev *sbdp;
	struct sbus_bus *sbus;
	char prom_name[40];
	extern int con_is_present(void);
	
	if (!con_is_present()) return -ENXIO;
	
#ifdef CONFIG_FB_CREATOR
	creator_fb_scan();
#endif
#ifdef CONFIG_SUN4
	sbusfb_init_fb(0, 0, FBTYPE_SUN2BW, NULL);
#endif
#if defined(CONFIG_FB_CGFOURTEEN) && !defined(__sparc_v9__)
	{
		int root, node;
		root = prom_getchild(prom_root_node);
		root = prom_searchsiblings(root, "obio");
		if (root && 
		    (node = prom_searchsiblings(prom_getchild(root), "cgfourteen"))) {
			sbusfb_init_fb(node, root, FBTYPE_MDICOLOR, NULL);
		}
	}
#endif
	if (sbus_root == NULL)
		return 0;
	for_all_sbusdev(sbdp, sbus) {
		type = known_card(sbdp->prom_name);
		if (type == FBTYPE_NOTYPE)
			continue;
		if (prom_getproperty(sbdp->prom_node, "emulation",
				     prom_name, sizeof(prom_name)) > 0) {
			type = known_card(prom_name);
			if (type == FBTYPE_NOTYPE)
				type = known_card(sbdp->prom_name);
		}
		sbusfb_init_fb(sbdp->prom_node, sbdp->bus->prom_node, type, sbdp);
	}
	return 0;
}

MODULE_LICENSE("GPL");	
