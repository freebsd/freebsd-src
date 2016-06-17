/*
 *  linux/drivers/video/fbcon.c -- Low level frame buffer based console driver
 *
 *	Copyright (C) 1995 Geert Uytterhoeven
 *
 *
 *  This file is based on the original Amiga console driver (amicon.c):
 *
 *	Copyright (C) 1993 Hamish Macdonald
 *			   Greg Harp
 *	Copyright (C) 1994 David Carter [carter@compsci.bristol.ac.uk]
 *
 *	      with work by William Rucklidge (wjr@cs.cornell.edu)
 *			   Geert Uytterhoeven
 *			   Jes Sorensen (jds@kom.auc.dk)
 *			   Martin Apel
 *
 *  and on the original Atari console driver (atacon.c):
 *
 *	Copyright (C) 1993 Bjoern Brauel
 *			   Roman Hodek
 *
 *	      with work by Guenther Kelleter
 *			   Martin Schaller
 *			   Andreas Schwab
 *
 *  Hardware cursor support added by Emmanuel Marty (core@ggi-project.org)
 *  Smart redraw scrolling, arbitrary font width support, 512char font support
 *  and software scrollback added by 
 *                         Jakub Jelinek (jj@ultra.linux.cz)
 *
 *  Random hacking by Martin Mares <mj@ucw.cz>
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 *  The low level operations for the various display memory organizations are
 *  now in separate source files.
 *
 *  Currently the following organizations are supported:
 *
 *    o afb			Amiga bitplanes
 *    o cfb{2,4,8,16,24,32}	Packed pixels
 *    o ilbm			Amiga interleaved bitplanes
 *    o iplan2p[248]		Atari interleaved bitplanes
 *    o mfb			Monochrome
 *    o vga			VGA characters/attributes
 *
 *  To do:
 *
 *    - Implement 16 plane mode (iplan2p16)
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#undef FBCONDEBUG

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>	/* MSch: for IRQ probe */
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/kd.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/pm.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#ifdef CONFIG_AMIGA
#include <asm/amigahw.h>
#include <asm/amigaints.h>
#endif /* CONFIG_AMIGA */
#ifdef CONFIG_ATARI
#include <asm/atariints.h>
#endif
#ifdef CONFIG_MAC
#include <asm/macints.h>
#endif
#if defined(__mc68000__) || defined(CONFIG_APUS)
#include <asm/machdep.h>
#include <asm/setup.h>
#endif
#ifdef CONFIG_FBCON_VGA_PLANES
#include <asm/io.h>
#endif
#define INCLUDE_LINUX_LOGO_DATA
#include <asm/linux_logo.h>

#include <video/fbcon.h>
#include <video/fbcon-mac.h>	/* for 6x11 font on mac */
#include <video/font.h>

#ifdef FBCONDEBUG
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#define LOGO_H			80
#define LOGO_W			80
#define LOGO_LINE	(LOGO_W/8)

struct display fb_display[MAX_NR_CONSOLES];
char con2fb_map[MAX_NR_CONSOLES];
static int logo_lines;
static int logo_shown = -1;
/* Software scrollback */
int fbcon_softback_size = 32768;
static unsigned long softback_buf, softback_curr;
static unsigned long softback_in;
static unsigned long softback_top, softback_end;
static int softback_lines;

#define REFCOUNT(fd)	(((int *)(fd))[-1])
#define FNTSIZE(fd)	(((int *)(fd))[-2])
#define FNTCHARCNT(fd)	(((int *)(fd))[-3])
#define FNTSUM(fd)	(((int *)(fd))[-4])
#define FONT_EXTRA_WORDS 4

#define CM_SOFTBACK	(8)

#define advance_row(p, delta) (unsigned short *)((unsigned long)(p) + (delta) * conp->vc_size_row)

static void fbcon_free_font(struct display *);
static int fbcon_set_origin(struct vc_data *);

#ifdef CONFIG_PM
static int pm_fbcon_request(struct pm_dev *dev, pm_request_t rqst, void *data);
static struct pm_dev *pm_fbcon;
static int fbcon_sleeping;
#endif

/*
 * Emmanuel: fbcon will now use a hardware cursor if the
 * low-level driver provides a non-NULL dispsw->cursor pointer,
 * in which case the hardware should do blinking, etc.
 *
 * if dispsw->cursor is NULL, use Atari alike software cursor
 */

static int cursor_drawn;

#define CURSOR_DRAW_DELAY		(1)

/* # VBL ints between cursor state changes */
#define ARM_CURSOR_BLINK_RATE		(10)
#define AMIGA_CURSOR_BLINK_RATE		(20)
#define ATARI_CURSOR_BLINK_RATE		(42)
#define MAC_CURSOR_BLINK_RATE		(32)
#define DEFAULT_CURSOR_BLINK_RATE	(20)

static int vbl_cursor_cnt;
static int cursor_on;
static int cursor_blink_rate;

static inline void cursor_undrawn(void)
{
    vbl_cursor_cnt = 0;
    cursor_drawn = 0;
}


#define divides(a, b)	((!(a) || (b)%(a)) ? 0 : 1)


/*
 *  Interface used by the world
 */

static const char *fbcon_startup(void);
static void fbcon_init(struct vc_data *conp, int init);
static void fbcon_deinit(struct vc_data *conp);
static int fbcon_changevar(int con);
static void fbcon_clear(struct vc_data *conp, int sy, int sx, int height,
		       int width);
static void fbcon_putc(struct vc_data *conp, int c, int ypos, int xpos);
static void fbcon_putcs(struct vc_data *conp, const unsigned short *s, int count,
			int ypos, int xpos);
static void fbcon_cursor(struct vc_data *conp, int mode);
static int fbcon_scroll(struct vc_data *conp, int t, int b, int dir,
			 int count);
static void fbcon_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			int height, int width);
static int fbcon_switch(struct vc_data *conp);
static int fbcon_blank(struct vc_data *conp, int blank);
static int fbcon_font_op(struct vc_data *conp, struct console_font_op *op);
static int fbcon_set_palette(struct vc_data *conp, unsigned char *table);
static int fbcon_scrolldelta(struct vc_data *conp, int lines);


/*
 *  Internal routines
 */

static void fbcon_setup(int con, int init, int logo);
static __inline__ int real_y(struct display *p, int ypos);
static void fbcon_vbl_handler(int irq, void *dummy, struct pt_regs *fp);
static __inline__ void updatescrollmode(struct display *p);
static __inline__ void ywrap_up(int unit, struct vc_data *conp,
				struct display *p, int count);
static __inline__ void ywrap_down(int unit, struct vc_data *conp,
				  struct display *p, int count);
static __inline__ void ypan_up(int unit, struct vc_data *conp,
			       struct display *p, int count);
static __inline__ void ypan_down(int unit, struct vc_data *conp,
				 struct display *p, int count);
static void fbcon_bmove_rec(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width, u_int y_break);

static int fbcon_show_logo(void);

#ifdef CONFIG_MAC
/*
 * On the Macintoy, there may or may not be a working VBL int. We need to probe
 */
static int vbl_detected;

static void fbcon_vbl_detect(int irq, void *dummy, struct pt_regs *fp)
{
      vbl_detected++;
}
#endif

static void cursor_timer_handler(unsigned long dev_addr);

static struct timer_list cursor_timer = {
    function: cursor_timer_handler
};
static int use_timer_cursor;

static void cursor_timer_handler(unsigned long dev_addr)
{
      fbcon_vbl_handler(0, NULL, NULL);
      cursor_timer.expires = jiffies+HZ/50;
      add_timer(&cursor_timer);
}


/**
 *	PROC_CONSOLE - find the attached tty or visible console
 *	@info: frame buffer info structure
 *
 *	Finds the tty attached to the process or visible console if
 *	the process is not directly attached to a tty (e.g. remote
 *	user) for device @info.
 *
 *	Returns -1 errno on error, or tty/visible console number
 *	on success.
 *
 */

int PROC_CONSOLE(const struct fb_info *info)
{
        int fgc;

	if (info->display_fg == NULL)
		return -1;

        if (!current->tty ||
	    current->tty->driver.type != TTY_DRIVER_TYPE_CONSOLE ||
	    MINOR(current->tty->device) < 1)
		fgc = info->display_fg->vc_num;
	else
		fgc = MINOR(current->tty->device)-1;

	/* Does this virtual console belong to the specified fbdev? */
	if (fb_display[fgc].fb_info != info)
		return -1;

	return fgc;
}


/**
 *	set_all_vcs - set all virtual consoles to match
 *	@fbidx: frame buffer index (e.g. fb0, fb1, ...)
 *	@fb: frame buffer ops structure
 *	@var: frame buffer screen structure to set
 *	@info: frame buffer info structure
 *
 *	Set all virtual consoles to match screen info set in @var
 *	for device @info.
 *
 *	Returns negative errno on error, or zero on success.
 *
 */

int set_all_vcs(int fbidx, struct fb_ops *fb, struct fb_var_screeninfo *var,
                struct fb_info *info)
{
    int unit, err;

    var->activate |= FB_ACTIVATE_TEST;
    err = fb->fb_set_var(var, PROC_CONSOLE(info), info);
    var->activate &= ~FB_ACTIVATE_TEST;
    if (err)
            return err;
    for (unit = 0; unit < MAX_NR_CONSOLES; unit++)
            if (fb_display[unit].conp && con2fb_map[unit] == fbidx)
                    fb->fb_set_var(var, unit, info);
    return 0;
}


/**
 *	set_con2fb_map - map console to frame buffer device
 *	@unit: virtual console number to map
 *	@newidx: frame buffer index to map virtual console to
 *
 *	Maps a virtual console @unit to a frame buffer device
 *	@newidx.
 *
 */

void set_con2fb_map(int unit, int newidx)
{
    int oldidx = con2fb_map[unit];
    struct fb_info *oldfb, *newfb;
    struct vc_data *conp;
    char *fontdata;
    unsigned short fontwidth, fontheight, fontwidthlog, fontheightlog;
    int userfont;

    if (newidx != con2fb_map[unit]) {
       oldfb = registered_fb[oldidx];
       newfb = registered_fb[newidx];
	if (newfb->fbops->owner)
		__MOD_INC_USE_COUNT(newfb->fbops->owner);
	if (newfb->fbops->fb_open && newfb->fbops->fb_open(newfb,0)) {
		if (newfb->fbops->owner)
			__MOD_DEC_USE_COUNT(newfb->fbops->owner);
		return;
	}
	if (oldfb->fbops->fb_release)
		oldfb->fbops->fb_release(oldfb,0);
	if (oldfb->fbops->owner)
		__MOD_DEC_USE_COUNT(oldfb->fbops->owner);
       conp = fb_display[unit].conp;
       fontdata = fb_display[unit].fontdata;
       fontwidth = fb_display[unit]._fontwidth;
       fontheight = fb_display[unit]._fontheight;
       fontwidthlog = fb_display[unit]._fontwidthlog;
       fontheightlog = fb_display[unit]._fontheightlog;
       userfont = fb_display[unit].userfont;
       con2fb_map[unit] = newidx;
       fb_display[unit] = *(newfb->disp);
       fb_display[unit].conp = conp;
       fb_display[unit].fontdata = fontdata;
       fb_display[unit]._fontwidth = fontwidth;
       fb_display[unit]._fontheight = fontheight;
       fb_display[unit]._fontwidthlog = fontwidthlog;
       fb_display[unit]._fontheightlog = fontheightlog;
       fb_display[unit].userfont = userfont;
       fb_display[unit].fb_info = newfb;
       if (conp)
	   conp->vc_display_fg = &newfb->display_fg;
       if (!newfb->display_fg)
	   newfb->display_fg = conp;
       if (!newfb->changevar)
           newfb->changevar = oldfb->changevar;
       /* tell console var has changed */
       if (newfb->changevar)
           newfb->changevar(unit);
   }
}

/*
 *  Low Level Operations
 */

struct display_switch fbcon_dummy;

/* NOTE: fbcon cannot be __init: it may be called from take_over_console later */

static const char *fbcon_startup(void)
{
    const char *display_desc = "frame buffer device";
    int irqres = 1;
    static int done = 0;

    /*
     *  If num_registered_fb is zero, this is a call for the dummy part.
     *  The frame buffer devices weren't initialized yet.
     */
    if (!num_registered_fb || done)
	return display_desc;
    done = 1;

#ifdef CONFIG_AMIGA
    if (MACH_IS_AMIGA) {
	cursor_blink_rate = AMIGA_CURSOR_BLINK_RATE;
	irqres = request_irq(IRQ_AMIGA_VERTB, fbcon_vbl_handler, 0,
			     "console/cursor", fbcon_vbl_handler);
    }
#endif /* CONFIG_AMIGA */
#ifdef CONFIG_ATARI
    if (MACH_IS_ATARI) {
	cursor_blink_rate = ATARI_CURSOR_BLINK_RATE;
	irqres = request_irq(IRQ_AUTO_4, fbcon_vbl_handler, IRQ_TYPE_PRIO,
			     "console/cursor", fbcon_vbl_handler);
    }
#endif /* CONFIG_ATARI */

#ifdef CONFIG_MAC
    /*
     * On a Macintoy, the VBL interrupt may or may not be active. 
     * As interrupt based cursor is more reliable and race free, we 
     * probe for VBL interrupts.
     */
    if (MACH_IS_MAC) {
	int ct = 0;
	/*
	 * Probe for VBL: set temp. handler ...
	 */
	irqres = request_irq(IRQ_MAC_VBL, fbcon_vbl_detect, 0,
			     "console/cursor", fbcon_vbl_detect);
	vbl_detected = 0;

	/*
	 * ... and spin for 20 ms ...
	 */
	while (!vbl_detected && ++ct<1000)
	    udelay(20);
 
	if(ct==1000)
	    printk("fbcon_startup: No VBL detected, using timer based cursor.\n");
 
	free_irq(IRQ_MAC_VBL, fbcon_vbl_detect);

	if (vbl_detected) {
	    /*
	     * interrupt based cursor ok
	     */
	    cursor_blink_rate = MAC_CURSOR_BLINK_RATE;
	    irqres = request_irq(IRQ_MAC_VBL, fbcon_vbl_handler, 0,
				 "console/cursor", fbcon_vbl_handler);
	} else {
	    /*
	     * VBL not detected: fall through, use timer based cursor
	     */
	    irqres = 1;
	}
    }
#endif /* CONFIG_MAC */

#if defined(__arm__) && defined(IRQ_VSYNCPULSE)
    cursor_blink_rate = ARM_CURSOR_BLINK_RATE;
    irqres = request_irq(IRQ_VSYNCPULSE, fbcon_vbl_handler, SA_SHIRQ,
			 "console/cursor", fbcon_vbl_handler);
#endif

    if (irqres) {
	use_timer_cursor = 1;
	cursor_blink_rate = DEFAULT_CURSOR_BLINK_RATE;
	cursor_timer.expires = jiffies+HZ/50;
	add_timer(&cursor_timer);
    }

#ifdef CONFIG_PM
    pm_fbcon = pm_register(PM_SYS_DEV, PM_SYS_VGA, pm_fbcon_request);
#endif

    return display_desc;
}


static void fbcon_init(struct vc_data *conp, int init)
{
    int unit = conp->vc_num;
    struct fb_info *info;

    /* on which frame buffer will we open this console? */
    info = registered_fb[(int)con2fb_map[unit]];

    info->changevar = &fbcon_changevar;
    fb_display[unit] = *(info->disp);	/* copy from default */
    DPRINTK("mode:   %s\n",info->modename);
    DPRINTK("visual: %d\n",fb_display[unit].visual);
    DPRINTK("res:    %dx%d-%d\n",fb_display[unit].var.xres,
	                     fb_display[unit].var.yres,
	                     fb_display[unit].var.bits_per_pixel);
    fb_display[unit].conp = conp;
    fb_display[unit].fb_info = info;
    /* clear out the cmap so we don't have dangling pointers */
    fb_display[unit].cmap.len = 0;
    fb_display[unit].cmap.red = 0;
    fb_display[unit].cmap.green = 0;
    fb_display[unit].cmap.blue = 0;
    fb_display[unit].cmap.transp = 0;
    fbcon_setup(unit, init, !init);
    /* Must be done after fbcon_setup to prevent excess updates */
    conp->vc_display_fg = &info->display_fg;
    if (!info->display_fg)
        info->display_fg = conp;
}


static void fbcon_deinit(struct vc_data *conp)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];

    fbcon_free_font(p);
    p->dispsw = &fbcon_dummy;
    p->conp = 0;
}


static int fbcon_changevar(int con)
{
    if (fb_display[con].conp)
	    fbcon_setup(con, 0, 0);
    return 0;
}


static __inline__ void updatescrollmode(struct display *p)
{
    int m;
    if (p->scrollmode & __SCROLL_YFIXED)
    	return;
    if (divides(p->ywrapstep, fontheight(p)) &&
	divides(fontheight(p), p->var.yres_virtual))
	m = __SCROLL_YWRAP;
    else if (divides(p->ypanstep, fontheight(p)) &&
	     p->var.yres_virtual >= p->var.yres+fontheight(p))
	m = __SCROLL_YPAN;
    else if (p->scrollmode & __SCROLL_YNOMOVE)
    	m = __SCROLL_YREDRAW;
    else
	m = __SCROLL_YMOVE;
    p->scrollmode = (p->scrollmode & ~__SCROLL_YMASK) | m;
}

static void fbcon_font_widths(struct display *p)
{
    int i;
    
    p->_fontwidthlog = 0;
    for (i = 2; i <= 6; i++)
    	if (fontwidth(p) == (1 << i))
	    p->_fontwidthlog = i;
    p->_fontheightlog = 0;
    for (i = 2; i <= 6; i++)
    	if (fontheight(p) == (1 << i))
	    p->_fontheightlog = i;
}

#define fontwidthvalid(p,w) ((p)->dispsw->fontwidthmask & FONTWIDTH(w))

static void fbcon_setup(int con, int init, int logo)
{
    struct display *p = &fb_display[con];
    struct vc_data *conp = p->conp;
    int nr_rows, nr_cols;
    int old_rows, old_cols;
    unsigned short *save = NULL, *r, *q;
    int i, charcnt = 256;
    struct fbcon_font_desc *font;
    
    if (con != fg_console || (p->fb_info->flags & FBINFO_FLAG_MODULE) ||
        p->type == FB_TYPE_TEXT)
    	logo = 0;

    p->var.xoffset = p->var.yoffset = p->yscroll = 0;  /* reset wrap/pan */

    if (con == fg_console && p->type != FB_TYPE_TEXT) {   
	if (fbcon_softback_size) {
	    if (!softback_buf) {
		softback_buf = (unsigned long)kmalloc(fbcon_softback_size, GFP_KERNEL);
		if (!softback_buf) {
    	            fbcon_softback_size = 0;
    	            softback_top = 0;
    		}
    	    }
	} else {
	    if (softback_buf) {
		kfree((void *)softback_buf);
		softback_buf = 0;
		softback_top = 0;
	    }
	}
	if (softback_buf)
	    softback_in = softback_top = softback_curr = softback_buf;
	softback_lines = 0;
    }
    
    for (i = 0; i < MAX_NR_CONSOLES; i++)
    	if (i != con && fb_display[i].fb_info == p->fb_info &&
    	    fb_display[i].conp && fb_display[i].fontdata)
    		break;

    fbcon_free_font(p);    
    if (i < MAX_NR_CONSOLES) {
    	struct display *q = &fb_display[i];

        if (fontwidthvalid(p,fontwidth(q))) {
            /* If we are not the first console on this
               fb, copy the font from that console */
	    p->_fontwidth = q->_fontwidth;
	    p->_fontheight = q->_fontheight;
    	    p->_fontwidthlog = q->_fontwidthlog;
    	    p->_fontheightlog = q->_fontheightlog;
    	    p->fontdata = q->fontdata;
    	    p->userfont = q->userfont; 
    	    if (p->userfont) {
    		REFCOUNT(p->fontdata)++;
    		charcnt = FNTCHARCNT(p->fontdata);
    	    }
    	    con_copy_unimap(con, i);
    	}
    }

    if (!p->fontdata) {
        if (!p->fb_info->fontname[0] ||
	    !(font = fbcon_find_font(p->fb_info->fontname)))
	        font = fbcon_get_default_font(p->var.xres, p->var.yres);
        p->_fontwidth = font->width;
        p->_fontheight = font->height;
        p->fontdata = font->data;
        fbcon_font_widths(p);
    }
    
    if (!fontwidthvalid(p,fontwidth(p))) {
#if defined(CONFIG_FBCON_MAC) && defined(CONFIG_MAC)
	if (MACH_IS_MAC)
	    /* ++Geert: hack to make 6x11 fonts work on mac */
	    p->dispsw = &fbcon_mac;
	else
#endif
	{
	    /* ++Geert: changed from panic() to `correct and continue' */
	    printk(KERN_ERR "fbcon_setup: No support for fontwidth %d\n", fontwidth(p));
	    p->dispsw = &fbcon_dummy;
	}
    }
    if (p->dispsw->set_font)
    	p->dispsw->set_font(p, fontwidth(p), fontheight(p));
    updatescrollmode(p);
    
    old_cols = conp->vc_cols;
    old_rows = conp->vc_rows;
    
    nr_cols = p->var.xres/fontwidth(p);
    nr_rows = p->var.yres/fontheight(p);
    
    if (logo) {
    	/* Need to make room for the logo */
	int cnt;
	int step;
    
    	logo_lines = (LOGO_H + fontheight(p) - 1) / fontheight(p);
    	q = (unsigned short *)(conp->vc_origin + conp->vc_size_row * old_rows);
    	step = logo_lines * old_cols;
    	for (r = q - logo_lines * old_cols; r < q; r++)
    	    if (scr_readw(r) != conp->vc_video_erase_char)
    	    	break;
	if (r != q && nr_rows >= old_rows + logo_lines) {
    	    save = kmalloc(logo_lines * nr_cols * 2, GFP_KERNEL);
    	    if (save) {
    	        int i = old_cols < nr_cols ? old_cols : nr_cols;
    	    	scr_memsetw(save, conp->vc_video_erase_char, logo_lines * nr_cols * 2);
    	    	r = q - step;
    	    	for (cnt = 0; cnt < logo_lines; cnt++, r += i)
    	    		scr_memcpyw(save + cnt * nr_cols, r, 2 * i);
    	    	r = q;
    	    }
    	}
    	if (r == q) {
    	    /* We can scroll screen down */
	    r = q - step - old_cols;
    	    for (cnt = old_rows - logo_lines; cnt > 0; cnt--) {
    	    	scr_memcpyw(r + step, r, conp->vc_size_row);
    	    	r -= old_cols;
    	    }
    	    if (!save) {
	    	conp->vc_y += logo_lines;
    		conp->vc_pos += logo_lines * conp->vc_size_row;
    	    }
    	}
    	scr_memsetw((unsigned short *)conp->vc_origin,
		    conp->vc_video_erase_char, 
		    conp->vc_size_row * logo_lines);
    }
    
    /*
     *  ++guenther: console.c:vc_allocate() relies on initializing
     *  vc_{cols,rows}, but we must not set those if we are only
     *  resizing the console.
     */
    if (init) {
	conp->vc_cols = nr_cols;
	conp->vc_rows = nr_rows;
    }
    p->vrows = p->var.yres_virtual/fontheight(p);
    if ((p->var.yres % fontheight(p)) &&
	(p->var.yres_virtual % fontheight(p) < p->var.yres % fontheight(p)))
	p->vrows--;
    conp->vc_can_do_color = p->var.bits_per_pixel != 1;
    conp->vc_complement_mask = conp->vc_can_do_color ? 0x7700 : 0x0800;
    if (charcnt == 256) {
    	conp->vc_hi_font_mask = 0;
    	p->fgshift = 8;
    	p->bgshift = 12;
    	p->charmask = 0xff;
    } else {
    	conp->vc_hi_font_mask = 0x100;
    	if (conp->vc_can_do_color)
	    conp->vc_complement_mask <<= 1;
    	p->fgshift = 9;
    	p->bgshift = 13;
    	p->charmask = 0x1ff;
    }

    if (p->dispsw == &fbcon_dummy)
	printk(KERN_WARNING "fbcon_setup: type %d (aux %d, depth %d) not "
	       "supported\n", p->type, p->type_aux, p->var.bits_per_pixel);
    p->dispsw->setup(p);

    p->fgcol = p->var.bits_per_pixel > 2 ? 7 : (1<<p->var.bits_per_pixel)-1;
    p->bgcol = 0;

    if (!init) {
	if (conp->vc_cols != nr_cols || conp->vc_rows != nr_rows)
	    vc_resize_con(nr_rows, nr_cols, con);
	else if (CON_IS_VISIBLE(conp) &&
		 vt_cons[conp->vc_num]->vc_mode == KD_TEXT) {
	    if (p->dispsw->clear_margins)
		p->dispsw->clear_margins(conp, p, 0);
	    update_screen(con);
	}
	if (save) {
    	    q = (unsigned short *)(conp->vc_origin + conp->vc_size_row * old_rows);
	    scr_memcpyw(q, save, logo_lines * nr_cols * 2);
	    conp->vc_y += logo_lines;
    	    conp->vc_pos += logo_lines * conp->vc_size_row;
    	    kfree(save);
	}
    }
	
    if (logo) {
	logo_shown = -2;
    	conp->vc_top = logo_lines;
    }
    
    if (con == fg_console && softback_buf) {
    	int l = fbcon_softback_size / conp->vc_size_row;
    	if (l > 5)
    	    softback_end = softback_buf + l * conp->vc_size_row;
    	else {
    	    /* Smaller scrollback makes no sense, and 0 would screw
    	       the operation totally */
    	    softback_top = 0;
    	}
    }
}


/* ====================================================================== */

/*  fbcon_XXX routines - interface used by the world
 *
 *  This system is now divided into two levels because of complications
 *  caused by hardware scrolling. Top level functions:
 *
 *	fbcon_bmove(), fbcon_clear(), fbcon_putc()
 *
 *  handles y values in range [0, scr_height-1] that correspond to real
 *  screen positions. y_wrap shift means that first line of bitmap may be
 *  anywhere on this display. These functions convert lineoffsets to
 *  bitmap offsets and deal with the wrap-around case by splitting blits.
 *
 *	fbcon_bmove_physical_8()    -- These functions fast implementations
 *	fbcon_clear_physical_8()    -- of original fbcon_XXX fns.
 *	fbcon_putc_physical_8()	    -- (fontwidth != 8) may be added later
 *
 *  WARNING:
 *
 *  At the moment fbcon_putc() cannot blit across vertical wrap boundary
 *  Implies should only really hardware scroll in rows. Only reason for
 *  restriction is simplicity & efficiency at the moment.
 */

static __inline__ int real_y(struct display *p, int ypos)
{
    int rows = p->vrows;

    ypos += p->yscroll;
    return ypos < rows ? ypos : ypos-rows;
}


static void fbcon_clear(struct vc_data *conp, int sy, int sx, int height,
			int width)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    u_int y_break;
    int redraw_cursor = 0;

    if (!p->can_soft_blank && console_blanked)
	return;

    if (!height || !width)
	return;

    if ((sy <= p->cursor_y) && (p->cursor_y < sy+height) &&
	(sx <= p->cursor_x) && (p->cursor_x < sx+width)) {
	cursor_undrawn();
	redraw_cursor = 1;
    }

    /* Split blits that cross physical y_wrap boundary */

    y_break = p->vrows-p->yscroll;
    if (sy < y_break && sy+height-1 >= y_break) {
	u_int b = y_break-sy;
	p->dispsw->clear(conp, p, real_y(p, sy), sx, b, width);
	p->dispsw->clear(conp, p, real_y(p, sy+b), sx, height-b, width);
    } else
	p->dispsw->clear(conp, p, real_y(p, sy), sx, height, width);

    if (redraw_cursor)
	vbl_cursor_cnt = CURSOR_DRAW_DELAY;
}


static void fbcon_putc(struct vc_data *conp, int c, int ypos, int xpos)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    int redraw_cursor = 0;

    if (!p->can_soft_blank && console_blanked)
	    return;
	    
    if (vt_cons[unit]->vc_mode != KD_TEXT)
    	    return;

    if ((p->cursor_x == xpos) && (p->cursor_y == ypos)) {
	    cursor_undrawn();
	    redraw_cursor = 1;
    }

    p->dispsw->putc(conp, p, c, real_y(p, ypos), xpos);

    if (redraw_cursor)
	    vbl_cursor_cnt = CURSOR_DRAW_DELAY;
}


static void fbcon_putcs(struct vc_data *conp, const unsigned short *s, int count,
		       int ypos, int xpos)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    int redraw_cursor = 0;

    if (!p->can_soft_blank && console_blanked)
	    return;

    if (vt_cons[unit]->vc_mode != KD_TEXT)
    	    return;

    if ((p->cursor_y == ypos) && (xpos <= p->cursor_x) &&
	(p->cursor_x < (xpos + count))) {
	    cursor_undrawn();
	    redraw_cursor = 1;
    }
    p->dispsw->putcs(conp, p, s, count, real_y(p, ypos), xpos);
    if (redraw_cursor)
	    vbl_cursor_cnt = CURSOR_DRAW_DELAY;
}


static void fbcon_cursor(struct vc_data *conp, int mode)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    int y = conp->vc_y;
    
    if (mode & CM_SOFTBACK) {
    	mode &= ~CM_SOFTBACK;
    	if (softback_lines) {
    	    if (y + softback_lines >= conp->vc_rows)
    		mode = CM_ERASE;
    	    else
    	        y += softback_lines;
    	}
    } else if (softback_lines)
        fbcon_set_origin(conp);

    /* do we have a hardware cursor ? */
    if (p->dispsw->cursor) {
	p->cursor_x = conp->vc_x;
	p->cursor_y = y;
	p->dispsw->cursor(p, mode, p->cursor_x, real_y(p, p->cursor_y));
	return;
    }

    /* Avoid flickering if there's no real change. */
    if (p->cursor_x == conp->vc_x && p->cursor_y == y &&
	(mode == CM_ERASE) == !cursor_on)
	return;

    cursor_on = 0;
    if (cursor_drawn && p->cursor_x < conp->vc_cols &&
	p->cursor_y < conp->vc_rows)
	p->dispsw->revc(p, p->cursor_x, real_y(p, p->cursor_y));

    p->cursor_x = conp->vc_x;
    p->cursor_y = y;

    switch (mode) {
        case CM_ERASE:
            cursor_drawn = 0;
            break;
        case CM_MOVE:
        case CM_DRAW:
            if (cursor_drawn)
	        p->dispsw->revc(p, p->cursor_x, real_y(p, p->cursor_y));
            vbl_cursor_cnt = CURSOR_DRAW_DELAY;
            cursor_on = 1;
            break;
        }
}


static void fbcon_vbl_handler(int irq, void *dummy, struct pt_regs *fp)
{
    struct display *p;

    if (!cursor_on)
	return;

    if (vbl_cursor_cnt && --vbl_cursor_cnt == 0) {
	p = &fb_display[fg_console];
	if (p->dispsw->revc)
		p->dispsw->revc(p, p->cursor_x, real_y(p, p->cursor_y));
	cursor_drawn ^= 1;
	vbl_cursor_cnt = cursor_blink_rate;
    }
}

static int scrollback_phys_max = 0;
static int scrollback_max = 0;
static int scrollback_current = 0;

static __inline__ void ywrap_up(int unit, struct vc_data *conp,
				struct display *p, int count)
{
    p->yscroll += count;
    if (p->yscroll >= p->vrows)	/* Deal with wrap */
	p->yscroll -= p->vrows;
    p->var.xoffset = 0;
    p->var.yoffset = p->yscroll*fontheight(p);
    p->var.vmode |= FB_VMODE_YWRAP;
    p->fb_info->updatevar(unit, p->fb_info);
    scrollback_max += count;
    if (scrollback_max > scrollback_phys_max)
	scrollback_max = scrollback_phys_max;
    scrollback_current = 0;
}


static __inline__ void ywrap_down(int unit, struct vc_data *conp,
				  struct display *p, int count)
{
    p->yscroll -= count;
    if (p->yscroll < 0)		/* Deal with wrap */
	p->yscroll += p->vrows;
    p->var.xoffset = 0;
    p->var.yoffset = p->yscroll*fontheight(p);
    p->var.vmode |= FB_VMODE_YWRAP;
    p->fb_info->updatevar(unit, p->fb_info);
    scrollback_max -= count;
    if (scrollback_max < 0)
	scrollback_max = 0;
    scrollback_current = 0;
}


static __inline__ void ypan_up(int unit, struct vc_data *conp,
			       struct display *p, int count)
{
    p->yscroll += count;
    if (p->yscroll > p->vrows-conp->vc_rows) {
	p->dispsw->bmove(p, p->vrows-conp->vc_rows, 0, 0, 0,
			 conp->vc_rows, conp->vc_cols);
	p->yscroll -= p->vrows-conp->vc_rows;
    }
    p->var.xoffset = 0;
    p->var.yoffset = p->yscroll*fontheight(p);
    p->var.vmode &= ~FB_VMODE_YWRAP;
    p->fb_info->updatevar(unit, p->fb_info);
    if (p->dispsw->clear_margins)
	p->dispsw->clear_margins(conp, p, 1);
    scrollback_max += count;
    if (scrollback_max > scrollback_phys_max)
	scrollback_max = scrollback_phys_max;
    scrollback_current = 0;
}


static __inline__ void ypan_down(int unit, struct vc_data *conp,
				 struct display *p, int count)
{
    p->yscroll -= count;
    if (p->yscroll < 0) {
	p->dispsw->bmove(p, 0, 0, p->vrows-conp->vc_rows, 0,
			 conp->vc_rows, conp->vc_cols);
	p->yscroll += p->vrows-conp->vc_rows;
    }
    p->var.xoffset = 0;
    p->var.yoffset = p->yscroll*fontheight(p);
    p->var.vmode &= ~FB_VMODE_YWRAP;
    p->fb_info->updatevar(unit, p->fb_info);
    if (p->dispsw->clear_margins)
	p->dispsw->clear_margins(conp, p, 1);
    scrollback_max -= count;
    if (scrollback_max < 0)
	scrollback_max = 0;
    scrollback_current = 0;
}

static void fbcon_redraw_softback(struct vc_data *conp, struct display *p, long delta)
{
    unsigned short *d, *s;
    unsigned long n;
    int line = 0;
    int count = conp->vc_rows;
    
    d = (u16 *)softback_curr;
    if (d == (u16 *)softback_in)
	d = (u16 *)conp->vc_origin;
    n = softback_curr + delta * conp->vc_size_row;
    softback_lines -= delta;
    if (delta < 0) {
        if (softback_curr < softback_top && n < softback_buf) {
            n += softback_end - softback_buf;
	    if (n < softback_top) {
		softback_lines -= (softback_top - n) / conp->vc_size_row;
		n = softback_top;
	    }
        } else if (softback_curr >= softback_top && n < softback_top) {
	    softback_lines -= (softback_top - n) / conp->vc_size_row;
	    n = softback_top;
        }
    } else {
    	if (softback_curr > softback_in && n >= softback_end) {
    	    n += softback_buf - softback_end;
	    if (n > softback_in) {
		n = softback_in;
		softback_lines = 0;
	    }
	} else if (softback_curr <= softback_in && n > softback_in) {
	    n = softback_in;
	    softback_lines = 0;
	}
    }
    if (n == softback_curr)
    	return;
    softback_curr = n;
    s = (u16 *)softback_curr;
    if (s == (u16 *)softback_in)
	s = (u16 *)conp->vc_origin;
    while (count--) {
	unsigned short *start;
	unsigned short *le;
	unsigned short c;
	int x = 0;
	unsigned short attr = 1;

	start = s;
	le = advance_row(s, 1);
	do {
	    c = scr_readw(s);
	    if (attr != (c & 0xff00)) {
		attr = c & 0xff00;
		if (s > start) {
		    p->dispsw->putcs(conp, p, start, s - start,
				     real_y(p, line), x);
		    x += s - start;
		    start = s;
		}
	    }
	    if (c == scr_readw(d)) {
	    	if (s > start) {
	    	    p->dispsw->putcs(conp, p, start, s - start,
				     real_y(p, line), x);
		    x += s - start + 1;
		    start = s + 1;
	    	} else {
	    	    x++;
	    	    start++;
	    	}
	    }
	    s++;
	    d++;
	} while (s < le);
	if (s > start)
	    p->dispsw->putcs(conp, p, start, s - start, real_y(p, line), x);
	line++;
	if (d == (u16 *)softback_end)
	    d = (u16 *)softback_buf;
	if (d == (u16 *)softback_in)
	    d = (u16 *)conp->vc_origin;
	if (s == (u16 *)softback_end)
	    s = (u16 *)softback_buf;
	if (s == (u16 *)softback_in)
	    s = (u16 *)conp->vc_origin;
    }
}

static void fbcon_redraw(struct vc_data *conp, struct display *p, 
			 int line, int count, int offset)
{
    unsigned short *d = (unsigned short *)
	(conp->vc_origin + conp->vc_size_row * line);
    unsigned short *s = d + offset;

    while (count--) {
	unsigned short *start = s;
	unsigned short *le = advance_row(s, 1);
	unsigned short c;
	int x = 0;
	unsigned short attr = 1;

	do {
	    c = scr_readw(s);
	    if (attr != (c & 0xff00)) {
		attr = c & 0xff00;
		if (s > start) {
		    p->dispsw->putcs(conp, p, start, s - start,
				     real_y(p, line), x);
		    x += s - start;
		    start = s;
		}
	    }
	    if (c == scr_readw(d)) {
	    	if (s > start) {
	    	    p->dispsw->putcs(conp, p, start, s - start,
				     real_y(p, line), x);
		    x += s - start + 1;
		    start = s + 1;
	    	} else {
	    	    x++;
	    	    start++;
	    	}
	    }
	    scr_writew(c, d);
	    console_conditional_schedule();
	    s++;
	    d++;
	} while (s < le);
	if (s > start)
	    p->dispsw->putcs(conp, p, start, s - start, real_y(p, line), x);
	console_conditional_schedule();
	if (offset > 0)
		line++;
	else {
		line--;
		/* NOTE: We subtract two lines from these pointers */
		s -= conp->vc_size_row;
		d -= conp->vc_size_row;
	}
    }
}

/**
 *	fbcon_redraw_clear - clear area of the screen
 *	@conp: stucture pointing to current active virtual console
 *	@p: display structure
 *	@sy: starting Y coordinate
 *	@sx: starting X coordinate
 *	@height: height of area to clear
 *	@width: width of area to clear
 *
 *	Clears a specified area of the screen.  All dimensions are in
 *	pixels.
 *
 */

void fbcon_redraw_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
    int x, y;
    for (y=0; y<height; y++)
	for (x=0; x<width; x++)
	    fbcon_putc(conp, ' ', sy+y, sx+x);
}


/**
 *	fbcon_redraw_bmove - copy area of screen to another area
 *	@p: display structure
 *	@sy: origin Y coordinate
 *	@sx: origin X coordinate
 *	@dy: destination Y coordinate
 *	@dx: destination X coordinate
 *	@h: height of area to copy
 *	@w: width of area to copy
 *
 *	Copies an area of the screen to another area of the same screen.
 *	All dimensions are in pixels.
 *
 *	Note that this function cannot be used together with ypan or
 *	ywrap.
 *
 */

void fbcon_redraw_bmove(struct display *p, int sy, int sx, int dy, int dx, int h, int w)
{
    if (sy != dy)
    	panic("fbcon_redraw_bmove width sy != dy");
    /* h will be always 1, but it does not matter if we are more generic */

    while (h-- > 0) {
	struct vc_data *conp = p->conp;
	unsigned short *d = (unsigned short *)
		(conp->vc_origin + conp->vc_size_row * dy + dx * 2);
	unsigned short *s = d + (dx - sx);
	unsigned short *start = d;
	unsigned short *ls = d;
	unsigned short *le = d + w;
	unsigned short c;
	int x = dx;
	unsigned short attr = 1;

	do {
	    c = scr_readw(d);
	    if (attr != (c & 0xff00)) {
		attr = c & 0xff00;
		if (d > start) {
		    p->dispsw->putcs(conp, p, start, d - start, dy, x);
		    x += d - start;
		    start = d;
		}
	    }
	    if (s >= ls && s < le && c == scr_readw(s)) {
		if (d > start) {
		    p->dispsw->putcs(conp, p, start, d - start, dy, x);
		    x += d - start + 1;
		    start = d + 1;
		} else {
		    x++;
		    start++;
		}
	    }
	    s++;
	    d++;
	} while (d < le);
	if (d > start)
	    p->dispsw->putcs(conp, p, start, d - start, dy, x);
	sy++;
	dy++;
    }
}

static inline void fbcon_softback_note(struct vc_data *conp, int t, int count)
{
    unsigned short *p;

    if (conp->vc_num != fg_console)
	return;
    p = (unsigned short *)(conp->vc_origin + t * conp->vc_size_row);

    while (count) {
    	scr_memcpyw((u16 *)softback_in, p, conp->vc_size_row);
    	count--;
    	p = advance_row(p, 1);
    	softback_in += conp->vc_size_row;
    	if (softback_in == softback_end)
    	    softback_in = softback_buf;
    	if (softback_in == softback_top) {
    	    softback_top += conp->vc_size_row;
    	    if (softback_top == softback_end)
    	    	softback_top = softback_buf;
    	}
    }
    softback_curr = softback_in;
}

static int fbcon_scroll(struct vc_data *conp, int t, int b, int dir,
			int count)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    int scroll_partial = !(p->scrollmode & __SCROLL_YNOPARTIAL);

    if (!p->can_soft_blank && console_blanked)
	return 0;

    if (!count || vt_cons[unit]->vc_mode != KD_TEXT)
	return 0;

    fbcon_cursor(conp, CM_ERASE);
    
    /*
     * ++Geert: Only use ywrap/ypan if the console is in text mode
     * ++Andrew: Only use ypan on hardware text mode when scrolling the
     *           whole screen (prevents flicker).
     */

    switch (dir) {
	case SM_UP:
	    if (count > conp->vc_rows)	/* Maximum realistic size */
		count = conp->vc_rows;
	    if (softback_top)
	        fbcon_softback_note(conp, t, count);
	    if (logo_shown >= 0) goto redraw_up;
	    switch (p->scrollmode & __SCROLL_YMASK) {
	    case __SCROLL_YMOVE:
		p->dispsw->bmove(p, t+count, 0, t, 0, b-t-count,
				 conp->vc_cols);
		p->dispsw->clear(conp, p, b-count, 0, count,
				 conp->vc_cols);
		break;

	    case __SCROLL_YWRAP:
		if (b-t-count > 3*conp->vc_rows>>2) {
		    if (t > 0)
			fbcon_bmove(conp, 0, 0, count, 0, t,
				    conp->vc_cols);
			ywrap_up(unit, conp, p, count);
		    if (conp->vc_rows-b > 0)
			fbcon_bmove(conp, b-count, 0, b, 0,
				    conp->vc_rows-b, conp->vc_cols);
		} else if (p->scrollmode & __SCROLL_YPANREDRAW)
		    goto redraw_up;
		else
		    fbcon_bmove(conp, t+count, 0, t, 0, b-t-count,
				conp->vc_cols);
		fbcon_clear(conp, b-count, 0, count, conp->vc_cols);
		break;

	    case __SCROLL_YPAN:
		if (( p->yscroll + count <= 2 * (p->vrows - conp->vc_rows)) &&
		    (( !scroll_partial && (b-t == conp->vc_rows)) ||
		     ( scroll_partial  && (b-t-count > 3*conp->vc_rows>>2)))) {
		    if (t > 0)
			fbcon_bmove(conp, 0, 0, count, 0, t,
				    conp->vc_cols);
		    ypan_up(unit, conp, p, count);
		    if (conp->vc_rows-b > 0)
			fbcon_bmove(conp, b-count, 0, b, 0,
				    conp->vc_rows-b, conp->vc_cols);
		} else if (p->scrollmode & __SCROLL_YPANREDRAW)
		    goto redraw_up;
		else
		    fbcon_bmove(conp, t+count, 0, t, 0, b-t-count,
				conp->vc_cols);
		fbcon_clear(conp, b-count, 0, count, conp->vc_cols);
		break;

	    case __SCROLL_YREDRAW:
	    redraw_up:
		fbcon_redraw(conp, p, t, b-t-count, count*conp->vc_cols);
		p->dispsw->clear(conp, p, real_y(p, b-count), 0,
				 count, conp->vc_cols);
		scr_memsetw((unsigned short *)(conp->vc_origin + 
		    	    conp->vc_size_row * (b-count)), 
		    	    conp->vc_video_erase_char,
		    	    conp->vc_size_row * count);
		return 1;
	    }
	    break;

	case SM_DOWN:
	    if (count > conp->vc_rows)	/* Maximum realistic size */
		count = conp->vc_rows;
	    switch (p->scrollmode & __SCROLL_YMASK) {
	    case __SCROLL_YMOVE:
		p->dispsw->bmove(p, t, 0, t+count, 0, b-t-count,
				 conp->vc_cols);
		p->dispsw->clear(conp, p, t, 0,
				 count, conp->vc_cols);
		break;

	    case __SCROLL_YWRAP:
		if (b-t-count > 3*conp->vc_rows>>2) {
		    if (conp->vc_rows-b > 0)
			fbcon_bmove(conp, b, 0, b-count, 0,
				    conp->vc_rows-b, conp->vc_cols);
		    ywrap_down(unit, conp, p, count);
		    if (t > 0)
			fbcon_bmove(conp, count, 0, 0, 0, t,
				    conp->vc_cols);
		} else if (p->scrollmode & __SCROLL_YPANREDRAW)
		    goto redraw_down;
		else
		    fbcon_bmove(conp, t, 0, t+count, 0, b-t-count,
				conp->vc_cols);
		fbcon_clear(conp, t, 0, count, conp->vc_cols);
		break;

	    case __SCROLL_YPAN:
		if (( count-p->yscroll <= p->vrows-conp->vc_rows) &&
		    (( !scroll_partial && (b-t == conp->vc_rows)) ||
		     ( scroll_partial  && (b-t-count > 3*conp->vc_rows>>2)))) {
		    if (conp->vc_rows-b > 0)
			fbcon_bmove(conp, b, 0, b-count, 0,
				    conp->vc_rows-b, conp->vc_cols);
		    ypan_down(unit, conp, p, count);
		    if (t > 0)
			fbcon_bmove(conp, count, 0, 0, 0, t,
				    conp->vc_cols);
		} else if (p->scrollmode & __SCROLL_YPANREDRAW)
		    goto redraw_down;
		else
		    fbcon_bmove(conp, t, 0, t+count, 0, b-t-count,
				conp->vc_cols);
		fbcon_clear(conp, t, 0, count, conp->vc_cols);
		break;

	    case __SCROLL_YREDRAW:
	    redraw_down:
		fbcon_redraw(conp, p, b - 1, b-t-count, -count*conp->vc_cols);
		p->dispsw->clear(conp, p, real_y(p, t), 0,
				 count, conp->vc_cols);
	    	scr_memsetw((unsigned short *)(conp->vc_origin + 
	    		    conp->vc_size_row * t), 
	    		    conp->vc_video_erase_char,
	    		    conp->vc_size_row * count);
	    	return 1;
	    }
    }
    return 0;
}


static void fbcon_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			int height, int width)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    
    if (!p->can_soft_blank && console_blanked)
	return;

    if (!width || !height)
	return;

    if (((sy <= p->cursor_y) && (p->cursor_y < sy+height) &&
	  (sx <= p->cursor_x) && (p->cursor_x < sx+width)) ||
	 ((dy <= p->cursor_y) && (p->cursor_y < dy+height) &&
	  (dx <= p->cursor_x) && (p->cursor_x < dx+width)))
	fbcon_cursor(conp, CM_ERASE|CM_SOFTBACK);

    /*  Split blits that cross physical y_wrap case.
     *  Pathological case involves 4 blits, better to use recursive
     *  code rather than unrolled case
     *
     *  Recursive invocations don't need to erase the cursor over and
     *  over again, so we use fbcon_bmove_rec()
     */
    fbcon_bmove_rec(p, sy, sx, dy, dx, height, width, p->vrows-p->yscroll);
}

static void fbcon_bmove_rec(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width, u_int y_break)
{
    u_int b;

    if (sy < y_break && sy+height > y_break) {
	b = y_break-sy;
	if (dy < sy) {	/* Avoid trashing self */
	    fbcon_bmove_rec(p, sy, sx, dy, dx, b, width, y_break);
	    fbcon_bmove_rec(p, sy+b, sx, dy+b, dx, height-b, width, y_break);
	} else {
	    fbcon_bmove_rec(p, sy+b, sx, dy+b, dx, height-b, width, y_break);
	    fbcon_bmove_rec(p, sy, sx, dy, dx, b, width, y_break);
	}
	return;
    }

    if (dy < y_break && dy+height > y_break) {
	b = y_break-dy;
	if (dy < sy) {	/* Avoid trashing self */
	    fbcon_bmove_rec(p, sy, sx, dy, dx, b, width, y_break);
	    fbcon_bmove_rec(p, sy+b, sx, dy+b, dx, height-b, width, y_break);
	} else {
	    fbcon_bmove_rec(p, sy+b, sx, dy+b, dx, height-b, width, y_break);
	    fbcon_bmove_rec(p, sy, sx, dy, dx, b, width, y_break);
	}
	return;
    }
    p->dispsw->bmove(p, real_y(p, sy), sx, real_y(p, dy), dx, height, width);
}


static int fbcon_switch(struct vc_data *conp)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    struct fb_info *info = p->fb_info;

    if (softback_top) {
    	int l = fbcon_softback_size / conp->vc_size_row;
	if (softback_lines)
	    fbcon_set_origin(conp);
        softback_top = softback_curr = softback_in = softback_buf;
        softback_lines = 0;

	if (l > 5)
	    softback_end = softback_buf + l * conp->vc_size_row;
	else {
	    /* Smaller scrollback makes no sense, and 0 would screw
	       the operation totally */
	    softback_top = 0;
	}
    }
    if (logo_shown >= 0) {
    	struct vc_data *conp2 = vc_cons[logo_shown].d;
    	
    	if (conp2->vc_top == logo_lines && conp2->vc_bottom == conp2->vc_rows)
    		conp2->vc_top = 0;
    	logo_shown = -1;
    }
    p->var.yoffset = p->yscroll = 0;
    switch (p->scrollmode & __SCROLL_YMASK) {
	case __SCROLL_YWRAP:
	    scrollback_phys_max = p->vrows-conp->vc_rows;
	    break;
	case __SCROLL_YPAN:
	    scrollback_phys_max = p->vrows-2*conp->vc_rows;
	    if (scrollback_phys_max < 0)
		scrollback_phys_max = 0;
	    break;
	default:
	    scrollback_phys_max = 0;
	    break;
    }
    scrollback_max = 0;
    scrollback_current = 0;

    if (info && info->switch_con)
	(*info->switch_con)(unit, info);
    if (p->dispsw->clear_margins && vt_cons[unit]->vc_mode == KD_TEXT)
	p->dispsw->clear_margins(conp, p, 0);
    if (logo_shown == -2) {
	logo_shown = fg_console;
	fbcon_show_logo(); /* This is protected above by initmem_freed */
	update_region(fg_console,
		      conp->vc_origin + conp->vc_size_row * conp->vc_top,
		      conp->vc_size_row * (conp->vc_bottom - conp->vc_top) / 2);
	return 0;
    }
    return 1;
}


static int fbcon_blank(struct vc_data *conp, int blank)
{
    struct display *p = &fb_display[conp->vc_num];
    struct fb_info *info = p->fb_info;

    if (blank < 0)	/* Entering graphics mode */
	return 0;

    fbcon_cursor(p->conp, blank ? CM_ERASE : CM_DRAW);

    if (!p->can_soft_blank) {
	if (blank) {
	    if (p->visual == FB_VISUAL_MONO01) {
		if (p->screen_base)
		    fb_memset255(p->screen_base,
				 p->var.xres_virtual*p->var.yres_virtual*
				 p->var.bits_per_pixel>>3);
	    } else {
	    	unsigned short oldc;
	    	u_int height;
	    	u_int y_break;

	    	oldc = conp->vc_video_erase_char;
	    	conp->vc_video_erase_char &= p->charmask;
	    	height = conp->vc_rows;
		y_break = p->vrows-p->yscroll;
		if (height > y_break) {
			p->dispsw->clear(conp, p, real_y(p, 0), 0, y_break, conp->vc_cols);
			p->dispsw->clear(conp, p, real_y(p, y_break), 0, height-y_break, conp->vc_cols);
		} else
			p->dispsw->clear(conp, p, real_y(p, 0), 0, height, conp->vc_cols);
		conp->vc_video_erase_char = oldc;
	    }
	    return 0;
	} else {
	    /* Tell console.c that it has to restore the screen itself */
	    return 1;
	}
    }
    (*info->blank)(blank, info);
    return 0;
}

static void fbcon_free_font(struct display *p)
{
    if (p->userfont && p->fontdata &&
        (--REFCOUNT(p->fontdata) == 0))
	kfree(p->fontdata - FONT_EXTRA_WORDS*sizeof(int));
    p->fontdata = NULL;
    p->userfont = 0;
}

static inline int fbcon_get_font(int unit, struct console_font_op *op)
{
    struct display *p = &fb_display[unit];
    u8 *data = op->data;
    u8 *fontdata = p->fontdata;
    int i, j;

#ifdef CONFIG_FBCON_FONTWIDTH8_ONLY
    if (fontwidth(p) != 8) return -EINVAL;
#endif
    op->width = fontwidth(p);
    op->height = fontheight(p);
    op->charcount = (p->charmask == 0x1ff) ? 512 : 256;
    if (!op->data) return 0;
    
    if (op->width <= 8) {
	j = fontheight(p);
    	for (i = 0; i < op->charcount; i++) {
	    memcpy(data, fontdata, j);
	    memset(data+j, 0, 32-j);
	    data += 32;
	    fontdata += j;
	}
    }
#ifndef CONFIG_FBCON_FONTWIDTH8_ONLY
    else if (op->width <= 16) {
	j = fontheight(p) * 2;
	for (i = 0; i < op->charcount; i++) {
	    memcpy(data, fontdata, j);
	    memset(data+j, 0, 64-j);
	    data += 64;
	    fontdata += j;
	}
    } else if (op->width <= 24) {
	for (i = 0; i < op->charcount; i++) {
	    for (j = 0; j < fontheight(p); j++) {
		*data++ = fontdata[0];
		*data++ = fontdata[1];
		*data++ = fontdata[2];
		fontdata += sizeof(u32);
	    }
	    memset(data, 0, 3*(32-j));
	    data += 3 * (32 - j);
	}
    } else {
	j = fontheight(p) * 4;
	for (i = 0; i < op->charcount; i++) {
	    memcpy(data, fontdata, j);
	    memset(data+j, 0, 128-j);
	    data += 128;
	    fontdata += j;
	}
    }
#endif
    return 0;
}

static int fbcon_do_set_font(int unit, struct console_font_op *op, u8 *data, int userfont)
{
    struct display *p = &fb_display[unit];
    int resize;
    int w = op->width;
    int h = op->height;
    int cnt;
    char *old_data = NULL;

    if (!fontwidthvalid(p,w)) {
        if (userfont && op->op != KD_FONT_OP_COPY)
	    kfree(data - FONT_EXTRA_WORDS*sizeof(int));
	return -ENXIO;
    }

    if (CON_IS_VISIBLE(p->conp) && softback_lines)
	fbcon_set_origin(p->conp);
	
    resize = (w != fontwidth(p)) || (h != fontheight(p));
    if (p->userfont)
        old_data = p->fontdata;
    if (userfont)
        cnt = FNTCHARCNT(data);
    else
    	cnt = 256;
    p->fontdata = data;
    if ((p->userfont = userfont))
        REFCOUNT(data)++;
    p->_fontwidth = w;
    p->_fontheight = h;
    if (p->conp->vc_hi_font_mask && cnt == 256) {
    	p->conp->vc_hi_font_mask = 0;
    	if (p->conp->vc_can_do_color)
	    p->conp->vc_complement_mask >>= 1;
    	p->fgshift--;
    	p->bgshift--;
    	p->charmask = 0xff;

	/* ++Edmund: reorder the attribute bits */
	if (p->conp->vc_can_do_color) {
	    struct vc_data *conp = p->conp;
	    unsigned short *cp = (unsigned short *) conp->vc_origin;
	    int count = conp->vc_screenbuf_size/2;
	    unsigned short c;
	    for (; count > 0; count--, cp++) {
	        c = scr_readw(cp);
		scr_writew(((c & 0xfe00) >> 1) | (c & 0xff), cp);
	    }
	    c = conp->vc_video_erase_char;
	    conp->vc_video_erase_char = ((c & 0xfe00) >> 1) | (c & 0xff);
	    conp->vc_attr >>= 1;
	}

    } else if (!p->conp->vc_hi_font_mask && cnt == 512) {
    	p->conp->vc_hi_font_mask = 0x100;
    	if (p->conp->vc_can_do_color)
	    p->conp->vc_complement_mask <<= 1;
    	p->fgshift++;
    	p->bgshift++;
    	p->charmask = 0x1ff;

	/* ++Edmund: reorder the attribute bits */
	{
	    struct vc_data *conp = p->conp;
	    unsigned short *cp = (unsigned short *) conp->vc_origin;
	    int count = conp->vc_screenbuf_size/2;
	    unsigned short c;
	    for (; count > 0; count--, cp++) {
	        unsigned short newc;
	        c = scr_readw(cp);
		if (conp->vc_can_do_color)
		    newc = ((c & 0xff00) << 1) | (c & 0xff);
		else
		    newc = c & ~0x100;
		scr_writew(newc, cp);
	    }
	    c = conp->vc_video_erase_char;
	    if (conp->vc_can_do_color) {
		conp->vc_video_erase_char = ((c & 0xff00) << 1) | (c & 0xff);
		conp->vc_attr <<= 1;
	    } else
	        conp->vc_video_erase_char = c & ~0x100;
	}

    }
    fbcon_font_widths(p);

    if (resize) {
    	struct vc_data *conp = p->conp;
	/* reset wrap/pan */
	p->var.xoffset = p->var.yoffset = p->yscroll = 0;
	p->vrows = p->var.yres_virtual/h;
	if ((p->var.yres % h) && (p->var.yres_virtual % h < p->var.yres % h))
	    p->vrows--;
	updatescrollmode(p);
	vc_resize_con( p->var.yres/h, p->var.xres/w, unit );
        if (CON_IS_VISIBLE(conp) && softback_buf) {
	    int l = fbcon_softback_size / conp->vc_size_row;
	    if (l > 5)
		softback_end = softback_buf + l * conp->vc_size_row;
	    else {
		/* Smaller scrollback makes no sense, and 0 would screw
		   the operation totally */
		softback_top = 0;
    	    }
    	}
    } else if (CON_IS_VISIBLE(p->conp) && vt_cons[unit]->vc_mode == KD_TEXT) {
	if (p->dispsw->clear_margins)
	    p->dispsw->clear_margins(p->conp, p, 0);
	update_screen(unit);
    }

    if (old_data && (--REFCOUNT(old_data) == 0))
	kfree(old_data - FONT_EXTRA_WORDS*sizeof(int));

    return 0;
}

static inline int fbcon_copy_font(int unit, struct console_font_op *op)
{
    struct display *od, *p = &fb_display[unit];
    int h = op->height;

    if (h < 0 || !vc_cons_allocated( h ))
        return -ENOTTY;
    if (h == unit)
        return 0; /* nothing to do */
    od = &fb_display[h];
    if (od->fontdata == p->fontdata)
        return 0; /* already the same font... */
    op->width = fontwidth(od);
    op->height = fontheight(od);
    return fbcon_do_set_font(unit, op, od->fontdata, od->userfont);
}

static inline int fbcon_set_font(int unit, struct console_font_op *op)
{
    int w = op->width;
    int h = op->height;
    int size = h;
    int i, k;
    u8 *new_data, *data = op->data, *p;

#ifdef CONFIG_FBCON_FONTWIDTH8_ONLY
    if (w != 8)
    	return -EINVAL;
#endif
    if ((w <= 0) || (w > 32) || (op->charcount != 256 && op->charcount != 512))
        return -EINVAL;
    
    if (w > 8) { 
    	if (w <= 16)
    		size *= 2;
    	else
    		size *= 4;
    }
    size *= op->charcount;
       
    if (!(new_data = kmalloc(FONT_EXTRA_WORDS*sizeof(int)+size, GFP_USER)))
        return -ENOMEM;
    new_data += FONT_EXTRA_WORDS*sizeof(int);
    FNTSIZE(new_data) = size;
    FNTCHARCNT(new_data) = op->charcount;
    REFCOUNT(new_data) = 0; /* usage counter */
    p = new_data;
    if (w <= 8) {
	for (i = 0; i < op->charcount; i++) {
	    memcpy(p, data, h);
	    data += 32;
	    p += h;
	}
    }
#ifndef CONFIG_FBCON_FONTWIDTH8_ONLY
    else if (w <= 16) {
	h *= 2;
	for (i = 0; i < op->charcount; i++) {
	    memcpy(p, data, h);
	    data += 64;
	    p += h;
	}
    } else if (w <= 24) {
	for (i = 0; i < op->charcount; i++) {
	    int j;
	    for (j = 0; j < h; j++) {
	        memcpy(p, data, 3);
		p[3] = 0;
		data += 3;
		p += sizeof(u32);
	    }
	    data += 3*(32 - h);
	}
    } else {
	h *= 4;
	for (i = 0; i < op->charcount; i++) {
	    memcpy(p, data, h);
	    data += 128;
	    p += h;
	}
    }
#endif
    /* we can do it in u32 chunks because of charcount is 256 or 512, so
       font length must be multiple of 256, at least. And 256 is multiple
       of 4 */
    k = 0;
    while (p > new_data) k += *--(u32 *)p;
    FNTSUM(new_data) = k;
    /* Check if the same font is on some other console already */
    for (i = 0; i < MAX_NR_CONSOLES; i++) {
    	if (fb_display[i].userfont &&
    	    fb_display[i].fontdata &&
    	    FNTSUM(fb_display[i].fontdata) == k &&
    	    FNTSIZE(fb_display[i].fontdata) == size &&
    	    fontwidth(&fb_display[i]) == w &&
	    !memcmp(fb_display[i].fontdata, new_data, size)) {
	    kfree(new_data - FONT_EXTRA_WORDS*sizeof(int));
	    new_data = fb_display[i].fontdata;
	    break;
    	}
    }
    return fbcon_do_set_font(unit, op, new_data, 1);
}

static inline int fbcon_set_def_font(int unit, struct console_font_op *op)
{
    char name[MAX_FONT_NAME];
    struct fbcon_font_desc *f;
    struct display *p = &fb_display[unit];

    if (!op->data)
	f = fbcon_get_default_font(p->var.xres, p->var.yres);
    else if (strncpy_from_user(name, op->data, MAX_FONT_NAME-1) < 0)
	return -EFAULT;
    else {
	name[MAX_FONT_NAME-1] = 0;
	if (!(f = fbcon_find_font(name)))
	    return -ENOENT;
    }
    op->width = f->width;
    op->height = f->height;
    return fbcon_do_set_font(unit, op, f->data, 0);
}

static int fbcon_font_op(struct vc_data *conp, struct console_font_op *op)
{
    int unit = conp->vc_num;

    switch (op->op) {
	case KD_FONT_OP_SET:
	    return fbcon_set_font(unit, op);
	case KD_FONT_OP_GET:
	    return fbcon_get_font(unit, op);
	case KD_FONT_OP_SET_DEFAULT:
	    return fbcon_set_def_font(unit, op);
	case KD_FONT_OP_COPY:
	    return fbcon_copy_font(unit, op);
	default:
	    return -ENOSYS;
    }
}

static u16 palette_red[16];
static u16 palette_green[16];
static u16 palette_blue[16];

static struct fb_cmap palette_cmap  = {
    0, 16, palette_red, palette_green, palette_blue, NULL
};

static int fbcon_set_palette(struct vc_data *conp, unsigned char *table)
{
    int unit = conp->vc_num;
    struct display *p = &fb_display[unit];
    int i, j, k;
    u8 val;

    if (!conp->vc_can_do_color || (!p->can_soft_blank && console_blanked))
	return -EINVAL;
    for (i = j = 0; i < 16; i++) {
	k = table[i];
	val = conp->vc_palette[j++];
	palette_red[k] = (val<<8)|val;
	val = conp->vc_palette[j++];
	palette_green[k] = (val<<8)|val;
	val = conp->vc_palette[j++];
	palette_blue[k] = (val<<8)|val;
    }
    if (p->var.bits_per_pixel <= 4)
	palette_cmap.len = 1<<p->var.bits_per_pixel;
    else
	palette_cmap.len = 16;
    palette_cmap.start = 0;
    return p->fb_info->fbops->fb_set_cmap(&palette_cmap, 1, unit, p->fb_info);
}

static u16 *fbcon_screen_pos(struct vc_data *conp, int offset)
{
    int line;
    unsigned long p;

    if (conp->vc_num != fg_console || !softback_lines)
    	return (u16 *)(conp->vc_origin + offset);
    line = offset / conp->vc_size_row;
    if (line >= softback_lines)
    	return (u16 *)(conp->vc_origin + offset - softback_lines * conp->vc_size_row);
    p = softback_curr + offset;
    if (p >= softback_end)
    	p += softback_buf - softback_end;
    return (u16 *)p;
}

static unsigned long fbcon_getxy(struct vc_data *conp, unsigned long pos, int *px, int *py)
{
    int x, y;
    unsigned long ret;
    if (pos >= conp->vc_origin && pos < conp->vc_scr_end) {
    	unsigned long offset = (pos - conp->vc_origin) / 2;
    	
    	x = offset % conp->vc_cols;
    	y = offset / conp->vc_cols;
    	if (conp->vc_num == fg_console)
    	    y += softback_lines;
    	ret = pos + (conp->vc_cols - x) * 2;
    } else if (conp->vc_num == fg_console && softback_lines) {
    	unsigned long offset = pos - softback_curr;
    	
    	if (pos < softback_curr)
    	    offset += softback_end - softback_buf;
    	offset /= 2;
    	x = offset % conp->vc_cols;
    	y = offset / conp->vc_cols;
	ret = pos + (conp->vc_cols - x) * 2;
	if (ret == softback_end)
	    ret = softback_buf;
	if (ret == softback_in)
	    ret = conp->vc_origin;
    } else {
    	/* Should not happen */
    	x = y = 0;
    	ret = conp->vc_origin;
    }
    if (px) *px = x;
    if (py) *py = y;
    return ret;
}

/* As we might be inside of softback, we may work with non-contiguous buffer,
   that's why we have to use a separate routine. */
static void fbcon_invert_region(struct vc_data *conp, u16 *p, int cnt)
{
    while (cnt--) {
	u16 a = scr_readw(p);
	if (!conp->vc_can_do_color)
	    a ^= 0x0800;
	else if (conp->vc_hi_font_mask == 0x100)
	    a = ((a) & 0x11ff) | (((a) & 0xe000) >> 4) | (((a) & 0x0e00) << 4);
	else
	    a = ((a) & 0x88ff) | (((a) & 0x7000) >> 4) | (((a) & 0x0700) << 4);
	scr_writew(a, p++);
	if (p == (u16 *)softback_end)
	    p = (u16 *)softback_buf;
	if (p == (u16 *)softback_in)
	    p = (u16 *)conp->vc_origin;
    }
}

static int fbcon_scrolldelta(struct vc_data *conp, int lines)
{
    int unit, offset, limit, scrollback_old;
    struct display *p;
    
    unit = fg_console;
    p = &fb_display[unit];
    if (softback_top) {
    	if (conp->vc_num != unit)
    	    return 0;
    	if (vt_cons[unit]->vc_mode != KD_TEXT || !lines)
    	    return 0;
    	if (logo_shown >= 0) {
		struct vc_data *conp2 = vc_cons[logo_shown].d;
    	
		if (conp2->vc_top == logo_lines && conp2->vc_bottom == conp2->vc_rows)
    		    conp2->vc_top = 0;
    		if (logo_shown == unit) {
    		    unsigned long p, q;
    		    int i;
    		    
    		    p = softback_in;
    		    q = conp->vc_origin + logo_lines * conp->vc_size_row;
    		    for (i = 0; i < logo_lines; i++) {
    		    	if (p == softback_top) break;
    		    	if (p == softback_buf) p = softback_end;
    		    	p -= conp->vc_size_row;
    		    	q -= conp->vc_size_row;
    		    	scr_memcpyw((u16 *)q, (u16 *)p, conp->vc_size_row);
    		    }
    		    softback_in = p;
    		    update_region(unit, conp->vc_origin, logo_lines * conp->vc_cols);
    		}
		logo_shown = -1;
	}
    	fbcon_cursor(conp, CM_ERASE|CM_SOFTBACK);
    	fbcon_redraw_softback(conp, p, lines);
    	fbcon_cursor(conp, CM_DRAW|CM_SOFTBACK);
    	return 0;
    }

    if (!scrollback_phys_max)
	return -ENOSYS;

    scrollback_old = scrollback_current;
    scrollback_current -= lines;
    if (scrollback_current < 0)
	scrollback_current = 0;
    else if (scrollback_current > scrollback_max)
	scrollback_current = scrollback_max;
    if (scrollback_current == scrollback_old)
	return 0;

    if (!p->can_soft_blank &&
	(console_blanked || vt_cons[unit]->vc_mode != KD_TEXT || !lines))
	return 0;
    fbcon_cursor(conp, CM_ERASE);

    offset = p->yscroll-scrollback_current;
    limit = p->vrows;
    switch (p->scrollmode && __SCROLL_YMASK) {
	case __SCROLL_YWRAP:
	    p->var.vmode |= FB_VMODE_YWRAP;
	    break;
	case __SCROLL_YPAN:
	    limit -= conp->vc_rows;
	    p->var.vmode &= ~FB_VMODE_YWRAP;
	    break;
    }
    if (offset < 0)
	offset += limit;
    else if (offset >= limit)
	offset -= limit;
    p->var.xoffset = 0;
    p->var.yoffset = offset*fontheight(p);
    p->fb_info->updatevar(unit, p->fb_info);
    if (!scrollback_current)
	fbcon_cursor(conp, CM_DRAW);
    return 0;
}

static int fbcon_set_origin(struct vc_data *conp)
{
    if (softback_lines && !console_blanked)
        fbcon_scrolldelta(conp, softback_lines);
    return 0;
}

static inline unsigned safe_shift(unsigned d,int n)
{
    return n<0 ? d>>-n : d<<n;
}

static int __init fbcon_show_logo( void )
{
    struct display *p = &fb_display[fg_console]; /* draw to vt in foreground */
    int depth = p->var.bits_per_pixel;
    int line = p->next_line;
    unsigned char *fb = p->screen_base;
    unsigned char *logo;
    unsigned char *dst, *src;
    int i, j, n, x1, y1, x;
    int logo_depth, done = 0;

    /* Return if the frame buffer is not mapped */
    if (!fb)
	return 0;
	
    /*
     * Set colors if visual is PSEUDOCOLOR and we have enough colors, or for
     * DIRECTCOLOR
     * We don't have to set the colors for the 16-color logo, since that logo
     * uses the standard VGA text console palette
     */
    if ((p->visual == FB_VISUAL_PSEUDOCOLOR && depth >= 8) ||
	(p->visual == FB_VISUAL_DIRECTCOLOR && depth >= 24))
	for (i = 0; i < LINUX_LOGO_COLORS; i += n) {
	    n = LINUX_LOGO_COLORS - i;
	    if (n > 16)
		/* palette_cmap provides space for only 16 colors at once */
		n = 16;
	    palette_cmap.start = 32 + i;
	    palette_cmap.len   = n;
	    for( j = 0; j < n; ++j ) {
		palette_cmap.red[j]   = (linux_logo_red[i+j] << 8) |
					linux_logo_red[i+j];
		palette_cmap.green[j] = (linux_logo_green[i+j] << 8) |
					linux_logo_green[i+j];
		palette_cmap.blue[j]  = (linux_logo_blue[i+j] << 8) |
					linux_logo_blue[i+j];
	    }
	    p->fb_info->fbops->fb_set_cmap(&palette_cmap, 1, fg_console,
					   p->fb_info);
	}
	
    if (depth >= 8) {
	logo = linux_logo;
	logo_depth = 8;
    }
    else if (depth >= 4) {
	logo = linux_logo16;
	logo_depth = 4;
    }
    else {
	logo = linux_logo_bw;
	logo_depth = 1;
    }
    
    if (p->fb_info->fbops->fb_rasterimg)
    	p->fb_info->fbops->fb_rasterimg(p->fb_info, 1);

    for (x = 0; x < smp_num_cpus * (LOGO_W + 8) &&
    	 x < p->var.xres - (LOGO_W + 8); x += (LOGO_W + 8)) {
    	 
#if defined(CONFIG_FBCON_CFB16) || defined(CONFIG_FBCON_CFB24) || \
    defined(CONFIG_FBCON_CFB32) || defined(CONFIG_FB_SBUS)
        if (p->visual == FB_VISUAL_DIRECTCOLOR) {
	    unsigned int val;		/* max. depth 32! */
	    int bdepth;
	    int redshift, greenshift, blueshift;
		
	    /* Bug: Doesn't obey msb_right ... (who needs that?) */
	    redshift   = p->var.red.offset;
	    greenshift = p->var.green.offset;
	    blueshift  = p->var.blue.offset;

	    if (depth >= 24 && (depth % 8) == 0) {
		/* have at least 8 bits per color */
		src = logo;
		bdepth = depth/8;
		for( y1 = 0; y1 < LOGO_H; y1++ ) {
		    dst = fb + y1*line + x*bdepth;
		    for( x1 = 0; x1 < LOGO_W; x1++, src++ ) {
			val = (*src << redshift) |
			      (*src << greenshift) |
			      (*src << blueshift);
			if (bdepth == 4 && !((long)dst & 3)) {
			    /* Some cards require 32bit access */
			    fb_writel (val, dst);
			    dst += 4;
			} else if (bdepth == 2 && !((long)dst & 1)) {
			    /* others require 16bit access */
			    fb_writew (val,dst);
			    dst +=2;
			} else {
#ifdef __LITTLE_ENDIAN
			    for( i = 0; i < bdepth; ++i )
#else
			    for( i = bdepth-1; i >= 0; --i )
#endif
			        fb_writeb (val >> (i*8), dst++);
			}
		    }
		}
	    }
	    else if (depth >= 12 && depth <= 23) {
	        /* have 4..7 bits per color, using 16 color image */
		unsigned int pix;
		src = linux_logo16;
		bdepth = (depth+7)/8;
		for( y1 = 0; y1 < LOGO_H; y1++ ) {
		    dst = fb + y1*line + x*bdepth;
		    for( x1 = 0; x1 < LOGO_W/2; x1++, src++ ) {
			pix = *src >> 4; /* upper nibble */
			val = (pix << redshift) |
			      (pix << greenshift) |
			      (pix << blueshift);
#ifdef __LITTLE_ENDIAN
			for( i = 0; i < bdepth; ++i )
#else
			for( i = bdepth-1; i >= 0; --i )
#endif
			    fb_writeb (val >> (i*8), dst++);
			pix = *src & 0x0f; /* lower nibble */
			val = (pix << redshift) |
			      (pix << greenshift) |
			      (pix << blueshift);
#ifdef __LITTLE_ENDIAN
			for( i = 0; i < bdepth; ++i )
#else
			for( i = bdepth-1; i >= 0; --i )
#endif
			    fb_writeb (val >> (i*8), dst++);
		    }
		}
	    }
	    done = 1;
        }
#endif
#if defined(CONFIG_FBCON_CFB16) || defined(CONFIG_FBCON_CFB24) || \
    defined(CONFIG_FBCON_CFB32) || defined(CONFIG_FB_SBUS)
	if ((depth % 8 == 0) && (p->visual == FB_VISUAL_TRUECOLOR)) {
	    /* Modes without color mapping, needs special data transformation... */
	    unsigned int val;		/* max. depth 32! */
	    int bdepth = depth/8;
	    unsigned char mask[9] = { 0,0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff };
	    unsigned char redmask, greenmask, bluemask;
	    int redshift, greenshift, blueshift;
		
	    /* Bug: Doesn't obey msb_right ... (who needs that?) */
	    redmask   = mask[p->var.red.length   < 8 ? p->var.red.length   : 8];
	    greenmask = mask[p->var.green.length < 8 ? p->var.green.length : 8];
	    bluemask  = mask[p->var.blue.length  < 8 ? p->var.blue.length  : 8];
	    redshift   = p->var.red.offset   - (8-p->var.red.length);
	    greenshift = p->var.green.offset - (8-p->var.green.length);
	    blueshift  = p->var.blue.offset  - (8-p->var.blue.length);

	    src = logo;
	    for( y1 = 0; y1 < LOGO_H; y1++ ) {
		dst = fb + y1*line + x*bdepth;
		for( x1 = 0; x1 < LOGO_W; x1++, src++ ) {
		    val = safe_shift((linux_logo_red[*src-32]   & redmask), redshift) |
		          safe_shift((linux_logo_green[*src-32] & greenmask), greenshift) |
		          safe_shift((linux_logo_blue[*src-32]  & bluemask), blueshift);
		    if (bdepth == 4 && !((long)dst & 3)) {
			/* Some cards require 32bit access */
			fb_writel (val, dst);
			dst += 4;
		    } else if (bdepth == 2 && !((long)dst & 1)) {
			/* others require 16bit access */
			fb_writew (val,dst);
			dst +=2;
		    } else {
#ifdef __LITTLE_ENDIAN
			for( i = 0; i < bdepth; ++i )
#else
			for( i = bdepth-1; i >= 0; --i )
#endif
			    fb_writeb (val >> (i*8), dst++);
		    }
		}
	    }
	    done = 1;
	}
#endif
#if defined(CONFIG_FBCON_CFB4)
	if (depth == 4 && p->type == FB_TYPE_PACKED_PIXELS) {
		src = logo;
		for( y1 = 0; y1 < LOGO_H; y1++) {
			dst = fb + y1*line + x/2;
			for( x1 = 0; x1 < LOGO_W/2; x1++) {
				u8 q = *src++;
				q = (q << 4) | (q >> 4);
				fb_writeb (q, dst++);
			}
		}
		done = 1;
	}
#endif
#if defined(CONFIG_FBCON_CFB8) || defined(CONFIG_FB_SBUS)
	if (depth == 8 && p->type == FB_TYPE_PACKED_PIXELS) {
	    /* depth 8 or more, packed, with color registers */
		
	    src = logo;
	    for( y1 = 0; y1 < LOGO_H; y1++ ) {
		dst = fb + y1*line + x;
		for( x1 = 0; x1 < LOGO_W; x1++ )
		    fb_writeb (*src++, dst++);
	    }
	    done = 1;
	}
#endif
#if defined(CONFIG_FBCON_AFB) || defined(CONFIG_FBCON_ILBM) || \
    defined(CONFIG_FBCON_IPLAN2P2) || defined(CONFIG_FBCON_IPLAN2P4) || \
    defined(CONFIG_FBCON_IPLAN2P8)
	if (depth >= 2 && (p->type == FB_TYPE_PLANES ||
			   p->type == FB_TYPE_INTERLEAVED_PLANES)) {
	    /* planes (normal or interleaved), with color registers */
	    int bit;
	    unsigned char val, mask;
	    int plane = p->next_plane;

#if defined(CONFIG_FBCON_IPLAN2P2) || defined(CONFIG_FBCON_IPLAN2P4) || \
    defined(CONFIG_FBCON_IPLAN2P8)
	    int line_length = p->line_length;

	    /* for support of Atari interleaved planes */
#define MAP_X(x)	(line_length ? (x) : ((x) & ~1)*depth + ((x) & 1))
#else
#define MAP_X(x)	(x)
#endif
	    /* extract a bit from the source image */
#define	BIT(p,pix,bit)	(p[pix*logo_depth/8] & \
			 (1 << ((8-((pix*logo_depth)&7)-logo_depth) + bit)))
		
	    src = logo;
	    for( y1 = 0; y1 < LOGO_H; y1++ ) {
		for( x1 = 0; x1 < LOGO_LINE; x1++, src += logo_depth ) {
		    dst = fb + y1*line + MAP_X(x/8+x1);
		    for( bit = 0; bit < logo_depth; bit++ ) {
			val = 0;
			for( mask = 0x80, i = 0; i < 8; mask >>= 1, i++ ) {
			    if (BIT( src, i, bit ))
				val |= mask;
			}
			*dst = val;
			dst += plane;
		    }
		}
	    }
	
	    /* fill remaining planes */
	    if (depth > logo_depth) {
		for( y1 = 0; y1 < LOGO_H; y1++ ) {
		    for( x1 = 0; x1 < LOGO_LINE; x1++ ) {
			dst = fb + y1*line + MAP_X(x/8+x1) + logo_depth*plane;
			for( i = logo_depth; i < depth; i++, dst += plane )
			    *dst = 0x00;
		    }
		}
	    }
	    done = 1;
	    break;
	}
#endif
#if defined(CONFIG_FBCON_MFB) || defined(CONFIG_FBCON_AFB) || \
    defined(CONFIG_FBCON_ILBM) || defined(CONFIG_FBCON_HGA)

	if (depth == 1 && (p->type == FB_TYPE_PACKED_PIXELS ||
			   p->type == FB_TYPE_PLANES ||
			   p->type == FB_TYPE_INTERLEAVED_PLANES)) {

	    /* monochrome */
	    unsigned char inverse = p->inverse || p->visual == FB_VISUAL_MONO01
		? 0x00 : 0xff;

	    int is_hga = !strncmp(p->fb_info->modename, "HGA", 3);
	    /* can't use simply memcpy because need to apply inverse */
	    for( y1 = 0; y1 < LOGO_H; y1++ ) {
		src = logo + y1*LOGO_LINE;
		if (is_hga)
		    dst = fb + (y1%4)*8192 + (y1>>2)*line + x/8;
		else
		    dst = fb + y1*line + x/8;
		for( x1 = 0; x1 < LOGO_LINE; ++x1 )
		    fb_writeb(*src++ ^ inverse, dst++);
	    }
	    done = 1;
	}
#endif
#if defined(CONFIG_FBCON_VGA_PLANES)
	if (depth == 4 && p->type == FB_TYPE_VGA_PLANES) {
		outb_p(1,0x3ce); outb_p(0xf,0x3cf);
		outb_p(3,0x3ce); outb_p(0,0x3cf);
		outb_p(5,0x3ce); outb_p(0,0x3cf);

		src = logo;
		for (y1 = 0; y1 < LOGO_H; y1++) {
			for (x1 = 0; x1 < LOGO_W / 2; x1++) {
				dst = fb + y1*line + x1/4 + x/8;

				outb_p(0,0x3ce);
				outb_p(*src >> 4,0x3cf);
				outb_p(8,0x3ce);
				outb_p(1 << (7 - x1 % 4 * 2),0x3cf);
				fb_readb (dst);
				fb_writeb (0, dst);

				outb_p(0,0x3ce);
				outb_p(*src & 0xf,0x3cf);
				outb_p(8,0x3ce);
				outb_p(1 << (7 - (1 + x1 % 4 * 2)),0x3cf);
				fb_readb (dst);
				fb_writeb (0, dst);

				src++;
			}
		}
		done = 1;
	}
#endif			
    }
    
    if (p->fb_info->fbops->fb_rasterimg)
    	p->fb_info->fbops->fb_rasterimg(p->fb_info, 0);

    /* Modes not yet supported: packed pixels with depth != 8 (does such a
     * thing exist in reality?) */

    return done ? (LOGO_H + fontheight(p) - 1) / fontheight(p) : 0 ;
}

#ifdef CONFIG_PM
/* console.c doesn't do enough here */
static int
pm_fbcon_request(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	unsigned long flags;
	
	switch (rqst)
	{
	case PM_RESUME:
		acquire_console_sem();
		fbcon_sleeping = 0;
		if (use_timer_cursor) {
			cursor_timer.expires = jiffies+HZ/50;
			add_timer(&cursor_timer);
		}
		release_console_sem();
		break;
	case PM_SUSPEND:
		acquire_console_sem();
		save_flags(flags);
		cli();
		if (use_timer_cursor)
			del_timer(&cursor_timer);
		fbcon_sleeping = 1;
		restore_flags(flags);
		release_console_sem();
		break;
	}
	return 0;
}
#endif /* CONFIG_PM */

/*
 *  The console `switch' structure for the frame buffer based console
 */
 
const struct consw fb_con = {
    con_startup: 	fbcon_startup, 
    con_init: 		fbcon_init,
    con_deinit: 	fbcon_deinit,
    con_clear: 		fbcon_clear,
    con_putc: 		fbcon_putc,
    con_putcs: 		fbcon_putcs,
    con_cursor: 	fbcon_cursor,
    con_scroll: 	fbcon_scroll,
    con_bmove: 		fbcon_bmove,
    con_switch: 	fbcon_switch,
    con_blank: 		fbcon_blank,
    con_font_op:	fbcon_font_op,
    con_set_palette: 	fbcon_set_palette,
    con_scrolldelta: 	fbcon_scrolldelta,
    con_set_origin: 	fbcon_set_origin,
    con_invert_region:	fbcon_invert_region,
    con_screen_pos:	fbcon_screen_pos,
    con_getxy:		fbcon_getxy,
};


/*
 *  Dummy Low Level Operations
 */

static void fbcon_dummy_op(void) {}

#define DUMMY	(void *)fbcon_dummy_op

struct display_switch fbcon_dummy = {
    setup:	DUMMY,
    bmove:	DUMMY,
    clear:	DUMMY,
    putc:	DUMMY,
    putcs:	DUMMY,
    revc:	DUMMY,
};


/*
 *  Visible symbols for modules
 */

EXPORT_SYMBOL(fb_display);
EXPORT_SYMBOL(fbcon_redraw_bmove);
EXPORT_SYMBOL(fbcon_redraw_clear);
EXPORT_SYMBOL(fbcon_dummy);
EXPORT_SYMBOL(fb_con);

MODULE_LICENSE("GPL");
