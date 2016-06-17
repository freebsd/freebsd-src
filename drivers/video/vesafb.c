/*
 * framebuffer driver for VBE 2.0 compliant graphic boards
 *
 * switching to graphics mode happens at boot time (while
 * running in real mode, see arch/i386/boot/video.S).
 *
 * (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 */

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

#include <asm/io.h>
#include <asm/mtrr.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>
#include <video/fbcon-mac.h>

#define dac_reg	(0x3c8)
#define dac_val	(0x3c9)

/* --------------------------------------------------------------------- */

/*
 * card parameters
 */

/* card */
unsigned long video_base; /* physical addr */
int   video_size;
char *video_vbase;        /* mapped */

/* mode */
static int  video_bpp;
static int  video_width;
static int  video_height;
static int  video_height_virtual;
static int  video_type = FB_TYPE_PACKED_PIXELS;
static int  video_visual;
static int  video_linelength;
static int  video_cmap_len;

/* --------------------------------------------------------------------- */

static struct fb_var_screeninfo vesafb_defined = {
	0,0,0,0,	/* W,H, W, H (virtual) load xres,xres_virtual*/
	0,0,		/* virtual -> visible no offset */
	8,		/* depth -> load bits_per_pixel */
	0,		/* greyscale ? */
	{0,0,0},	/* R */
	{0,0,0},	/* G */
	{0,0,0},	/* B */
	{0,0,0},	/* transparency */
	0,		/* standard pixel format */
	FB_ACTIVATE_NOW,
	-1,-1,
	0,
	0L,0L,0L,0L,0L,
	0L,0L,0,	/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0,0,0,0,0,0}
};

static struct display disp;
static struct fb_info fb_info;
static struct { u_short blue, green, red, pad; } palette[256];
static union {
#ifdef FBCON_HAS_CFB16
    u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB24
    u32 cfb24[16];
#endif
#ifdef FBCON_HAS_CFB32
    u32 cfb32[16];
#endif
} fbcon_cmap;

static int             inverse   = 0;
#ifdef __x86_64__
static int             mtrr      = 1;
#else
static int             mtrr      = 0;
#endif
static int	 vram __initdata = 0;	/* needed for vram boot option */
static int             currcon   = 0;

static int             pmi_setpal = 0;	/* pmi for palette changes ??? */
static int             ypan       = 0;  /* 0..nothing, 1..ypan, 2..ywrap */
static unsigned short  *pmi_base  = 0;
static void            (*pmi_start)(void);
static void            (*pmi_pal)(void);

static struct display_switch vesafb_sw;

/* --------------------------------------------------------------------- */

static int vesafb_pan_display(struct fb_var_screeninfo *var, int con,
                              struct fb_info *info)
{
#ifdef __i386__
	int offset;

	if (!ypan)
		return -EINVAL;
	if (var->xoffset)
		return -EINVAL;
	if (var->yoffset > var->yres_virtual)
		return -EINVAL;
	if ((ypan==1) && var->yoffset+var->yres > var->yres_virtual)
		return -EINVAL;

	offset = (var->yoffset * video_linelength + var->xoffset) / 4;

        __asm__ __volatile__(
                "call *(%%edi)"
                : /* no return value */
                : "a" (0x4f07),         /* EAX */
                  "b" (0),              /* EBX */
                  "c" (offset),         /* ECX */
                  "d" (offset >> 16),   /* EDX */
                  "D" (&pmi_start));    /* EDI */
#endif
	return 0;
}

static int vesafb_update_var(int con, struct fb_info *info)
{
	if (con == currcon && ypan) {
		struct fb_var_screeninfo *var = &fb_display[currcon].var;
		return vesafb_pan_display(var,con,info);
	}
	return 0;
}

static int vesafb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id,"VESA VGA");

	fix->smem_start=video_base;
	fix->smem_len=video_size;
	fix->type = video_type;
	fix->visual = video_visual;
	fix->xpanstep  = 0;
	fix->ypanstep  = ypan     ? 1 : 0;
	fix->ywrapstep = (ypan>1) ? 1 : 0;
	fix->line_length=video_linelength;
	return 0;
}

static int vesafb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	if(con==-1)
		memcpy(var, &vesafb_defined, sizeof(struct fb_var_screeninfo));
	else
		*var=fb_display[con].var;
	return 0;
}

static void vesafb_set_disp(int con)
{
	struct fb_fix_screeninfo fix;
	struct display *display;
	struct display_switch *sw;
	
	if (con >= 0)
		display = &fb_display[con];
	else
		display = &disp;	/* used during initialization */

	vesafb_get_fix(&fix, con, 0);

	memset(display, 0, sizeof(struct display));
	display->screen_base = video_vbase;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 0;
	display->inverse = inverse;
	vesafb_get_var(&display->var, -1, &fb_info);

	switch (video_bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
		sw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
	case 16:
		sw = &fbcon_cfb16;
		display->dispsw_data = fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		sw = &fbcon_cfb24;
		display->dispsw_data = fbcon_cmap.cfb24;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		sw = &fbcon_cfb32;
		display->dispsw_data = fbcon_cmap.cfb32;
		break;
#endif
	default:
#ifdef FBCON_HAS_MAC
		sw = &fbcon_mac;
		break;
#else
		sw = &fbcon_dummy;
		return;
#endif
	}
	memcpy(&vesafb_sw, sw, sizeof(*sw));
	display->dispsw = &vesafb_sw;
	if (!ypan) {
		display->scrollmode = SCROLL_YREDRAW;
		vesafb_sw.bmove = fbcon_redraw_bmove;
	}
}

static int vesafb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
	static int first = 1;

	if (var->xres           != vesafb_defined.xres           ||
	    var->yres           != vesafb_defined.yres           ||
	    var->xres_virtual   != vesafb_defined.xres_virtual   ||
	    var->yres_virtual   >  video_height_virtual          ||
	    var->yres_virtual   <  video_height                  ||
	    var->xoffset                                         ||
	    var->bits_per_pixel != vesafb_defined.bits_per_pixel ||
	    var->nonstd) {
		if (first) {
			printk(KERN_ERR "Vesafb does not support changing the video mode\n");
			first = 0;
		}
		return -EINVAL;
	}

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;

	if (ypan) {
		if (vesafb_defined.yres_virtual != var->yres_virtual) {
			vesafb_defined.yres_virtual = var->yres_virtual;
			if (con != -1) {
				fb_display[con].var = vesafb_defined;
				info->changevar(con);
			}
		}

		if (var->yoffset != vesafb_defined.yoffset)
			return vesafb_pan_display(var,con,info);
		return 0;
	}

	if (var->yoffset)
		return -EINVAL;
	return 0;
}

static int vesa_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			  unsigned *blue, unsigned *transp,
			  struct fb_info *fb_info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  Return != 0 for invalid regno.
	 */

	if (regno >= video_cmap_len)
		return 1;

	*red   = palette[regno].red;
	*green = palette[regno].green;
	*blue  = palette[regno].blue;
	*transp = 0;
	return 0;
}

#ifdef FBCON_HAS_CFB8

static void vesa_setpalette(int regno, unsigned red, unsigned green, unsigned blue)
{
#ifdef i386
	struct { u_char blue, green, red, pad; } entry;

	if (pmi_setpal) {
		entry.red   = red   >> 10;
		entry.green = green >> 10;
		entry.blue  = blue  >> 10;
		entry.pad   = 0;
	        __asm__ __volatile__(
                "call *(%%esi)"
                : /* no return value */
                : "a" (0x4f09),         /* EAX */
                  "b" (0),              /* EBX */
                  "c" (1),              /* ECX */
                  "d" (regno),          /* EDX */
                  "D" (&entry),         /* EDI */
                  "S" (&pmi_pal));      /* ESI */
	} else {
		/* without protected mode interface, try VGA registers... */
		outb_p(regno,       dac_reg);
		outb_p(red   >> 10, dac_val);
		outb_p(green >> 10, dac_val);
		outb_p(blue  >> 10, dac_val);
	}
#endif

}

#endif

static int vesa_setcolreg(unsigned regno, unsigned red, unsigned green,
			  unsigned blue, unsigned transp,
			  struct fb_info *fb_info)
{
	/*
	 *  Set a single color register. The values supplied are
	 *  already rounded down to the hardware's capabilities
	 *  (according to the entries in the `var' structure). Return
	 *  != 0 for invalid regno.
	 */
	
	if (regno >= video_cmap_len)
		return 1;

	palette[regno].red   = red;
	palette[regno].green = green;
	palette[regno].blue  = blue;
	
	switch (video_bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
		vesa_setpalette(regno,red,green,blue);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
	case 16:
		if (vesafb_defined.red.offset == 10) {
			/* 1:5:5:5 */
			fbcon_cmap.cfb16[regno] =
				((red   & 0xf800) >>  1) |
				((green & 0xf800) >>  6) |
				((blue  & 0xf800) >> 11);
		} else {
			/* 0:5:6:5 */
			fbcon_cmap.cfb16[regno] =
				((red   & 0xf800)      ) |
				((green & 0xfc00) >>  5) |
				((blue  & 0xf800) >> 11);
		}
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		fbcon_cmap.cfb24[regno] =
			(red   << vesafb_defined.red.offset)   |
			(green << vesafb_defined.green.offset) |
			(blue  << vesafb_defined.blue.offset);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		fbcon_cmap.cfb32[regno] =
			(red   << vesafb_defined.red.offset)   |
			(green << vesafb_defined.green.offset) |
			(blue  << vesafb_defined.blue.offset);
		break;
#endif
    }
    return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, vesa_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(video_cmap_len), 1, vesa_setcolreg,
			    info);
}

static int vesafb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, vesa_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(video_cmap_len),
		     cmap, kspc ? 0 : 2);
	return 0;
}

static int vesafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		err = fb_alloc_cmap(&fb_display[con].cmap,video_cmap_len,0);
		if (err)
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, vesa_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

static struct fb_ops vesafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	vesafb_get_fix,
	fb_get_var:	vesafb_get_var,
	fb_set_var:	vesafb_set_var,
	fb_get_cmap:	vesafb_get_cmap,
	fb_set_cmap:	vesafb_set_cmap,
	fb_pan_display:	vesafb_pan_display,
};

int __init vesafb_setup(char *options)
{
	char *this_opt;
	
	fb_info.fontname[0] = '\0';
	
	if (!options || !*options)
		return 0;
	
	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt) continue;
		
		if (! strcmp(this_opt, "inverse"))
			inverse=1;
		else if (! strcmp(this_opt, "redraw"))
			ypan=0;
		else if (! strcmp(this_opt, "ypan"))
			ypan=1;
		else if (! strcmp(this_opt, "ywrap"))
			ypan=2;
		else if (! strcmp(this_opt, "vgapal"))
			pmi_setpal=0;
		else if (! strcmp(this_opt, "pmipal"))
			pmi_setpal=1;
		else if (! strcmp(this_opt, "mtrr"))
			mtrr=1;
		/* checks for vram boot option */
		else if (! strncmp(this_opt, "vram:", 5))
			vram = simple_strtoul(this_opt+5, NULL, 0);

		else if (!strncmp(this_opt, "font:", 5))
			strcpy(fb_info.fontname, this_opt+5);
	}
	return 0;
}

static int vesafb_switch(int con, struct fb_info *info)
{
	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, vesa_getcolreg,
			    info);
	
	currcon = con;
	/* Install new colormap */
	do_install_cmap(con, info);
	vesafb_update_var(con,info);
	return 1;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */

static void vesafb_blank(int blank, struct fb_info *info)
{
	/* Not supported */
}

int __init vesafb_init(void)
{
	int i,j;

	if (screen_info.orig_video_isVGA != VIDEO_TYPE_VLFB)
		return -ENXIO;

	video_base          = screen_info.lfb_base;
	video_bpp           = screen_info.lfb_depth;
	if (15 == video_bpp)
		video_bpp = 16;
	video_width         = screen_info.lfb_width;
	video_height        = screen_info.lfb_height;
	video_linelength    = screen_info.lfb_linelength;

	/* remap memory according to videomode, multiply by 2 to get space for doublebuffering */
	video_size          = screen_info.lfb_width *	screen_info.lfb_height * video_bpp / 8 * 2;

	/* check that we don't remap more memory than old cards have */
	if (video_size > screen_info.lfb_size * 65536)
		video_size = screen_info.lfb_size * 65536;
	
	/* FIXME: Should we clip against declared size for banked devices ? */
	
	/* sets video_size according to vram boot option */
	if (vram && vram * 1024 * 1024 != video_size)
		video_size = vram * 1024 * 1024;
		
	video_visual = (video_bpp == 8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;

#ifndef __i386__
	screen_info.vesapm_seg = 0;
#endif

	if (!request_mem_region(video_base, video_size, "vesafb")) {
		printk(KERN_WARNING
		       "vesafb: abort, cannot reserve video memory at 0x%lx\n",
			video_base);
		/* We cannot make this fatal. Sometimes this comes from magic
		   spaces our resource handlers simply don't know about */
	}

        video_vbase = ioremap(video_base, video_size);
	if (!video_vbase) {
		release_mem_region(video_base, video_size);
		printk(KERN_ERR
		       "vesafb: abort, cannot ioremap video memory 0x%x @ 0x%lx\n",
			video_size, video_base);
		return -EIO;
	}

	printk(KERN_INFO "vesafb: framebuffer at 0x%lx, mapped to 0x%p, size %dk\n",
	       video_base, video_vbase, video_size/1024);
	printk(KERN_INFO "vesafb: mode is %dx%dx%d, linelength=%d, pages=%d\n",
	       video_width, video_height, video_bpp, video_linelength, screen_info.pages);

	if (screen_info.vesapm_seg) {
		printk(KERN_INFO "vesafb: protected mode interface info at %04x:%04x\n",
		       screen_info.vesapm_seg,screen_info.vesapm_off);
	}

	if (screen_info.vesapm_seg < 0xc000)
		ypan = pmi_setpal = 0; /* not available or some DOS TSR ... */

	if (ypan || pmi_setpal) {
		pmi_base  = (unsigned short*)bus_to_virt(((unsigned long)screen_info.vesapm_seg << 4) + screen_info.vesapm_off);
		pmi_start = (void*)((char*)pmi_base + pmi_base[1]);
		pmi_pal   = (void*)((char*)pmi_base + pmi_base[2]);
		printk(KERN_INFO "vesafb: pmi: set display start = %p, set palette = %p\n",pmi_start,pmi_pal);
		if (pmi_base[3]) {
			printk(KERN_INFO "vesafb: pmi: ports = ");
				for (i = pmi_base[3]/2; pmi_base[i] != 0xffff; i++)
					printk("%x ",pmi_base[i]);
			printk("\n");
			if (pmi_base[i] != 0xffff) {
				/*
				 * memory areas not supported (yet?)
				 *
				 * Rules are: we have to set up a descriptor for the requested
				 * memory area and pass it in the ES register to the BIOS function.
				 */
				printk(KERN_INFO "vesafb: can't handle memory requests, pmi disabled\n");
				ypan = pmi_setpal = 0;
			}
		}
	}

	vesafb_defined.xres=video_width;
	vesafb_defined.yres=video_height;
	vesafb_defined.xres_virtual=video_width;
	vesafb_defined.yres_virtual=video_size / video_linelength;
	vesafb_defined.bits_per_pixel=video_bpp;

	if (ypan && vesafb_defined.yres_virtual > video_height) {
		printk(KERN_INFO "vesafb: scrolling: %s using protected mode interface, yres_virtual=%d\n",
		       (ypan > 1) ? "ywrap" : "ypan",vesafb_defined.yres_virtual);
	} else {
		printk(KERN_INFO "vesafb: scrolling: redraw\n");
		vesafb_defined.yres_virtual = video_height;
		ypan = 0;
	}
	video_height_virtual = vesafb_defined.yres_virtual;

	/* some dummy values for timing to make fbset happy */
	vesafb_defined.pixclock     = 10000000 / video_width * 1000 / video_height;
	vesafb_defined.left_margin  = (video_width / 8) & 0xf8;
	vesafb_defined.right_margin = 32;
	vesafb_defined.upper_margin = 16;
	vesafb_defined.lower_margin = 4;
	vesafb_defined.hsync_len    = (video_width / 8) & 0xf8;
	vesafb_defined.vsync_len    = 4;

	if (video_bpp > 8) {
		vesafb_defined.red.offset    = screen_info.red_pos;
		vesafb_defined.red.length    = screen_info.red_size;
		vesafb_defined.green.offset  = screen_info.green_pos;
		vesafb_defined.green.length  = screen_info.green_size;
		vesafb_defined.blue.offset   = screen_info.blue_pos;
		vesafb_defined.blue.length   = screen_info.blue_size;
		vesafb_defined.transp.offset = screen_info.rsvd_pos;
		vesafb_defined.transp.length = screen_info.rsvd_size;
		printk(KERN_INFO "vesafb: directcolor: "
		       "size=%d:%d:%d:%d, shift=%d:%d:%d:%d\n",
		       screen_info.rsvd_size,
		       screen_info.red_size,
		       screen_info.green_size,
		       screen_info.blue_size,
		       screen_info.rsvd_pos,
		       screen_info.red_pos,
		       screen_info.green_pos,
		       screen_info.blue_pos);
		video_cmap_len = 16;
	} else {
		vesafb_defined.red.length   = 6;
		vesafb_defined.green.length = 6;
		vesafb_defined.blue.length  = 6;
		for(i = 0; i < 16; i++) {
			j = color_table[i];
			palette[i].red   = default_red[j];
			palette[i].green = default_grn[j];
			palette[i].blue  = default_blu[j];
		}
		video_cmap_len = 256;
	}

	/* request failure does not faze us, as vgacon probably has this
	 * region already (FIXME) */
	request_region(0x3c0, 32, "vesafb");

	if (mtrr) {
		int temp_size = video_size;
		/* Find the largest power-of-two */
		while (temp_size & (temp_size - 1))
                	temp_size &= (temp_size - 1);
                        
                /* Try and find a power of two to add */
		while (temp_size && mtrr_add(video_base, temp_size, MTRR_TYPE_WRCOMB, 1)==-EINVAL) {
			temp_size >>= 1;
		}
	}
	
	strcpy(fb_info.modename, "VESA VGA");
	fb_info.changevar = NULL;
	fb_info.node = -1;
	fb_info.fbops = &vesafb_ops;
	fb_info.disp=&disp;
	fb_info.switch_con=&vesafb_switch;
	fb_info.updatevar=&vesafb_update_var;
	fb_info.blank=&vesafb_blank;
	fb_info.flags=FBINFO_FLAG_DEFAULT;
	vesafb_set_disp(-1);

	if (register_framebuffer(&fb_info)<0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	       GET_FB_IDX(fb_info.node), fb_info.modename);
	return 0;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

MODULE_LICENSE("GPL");
