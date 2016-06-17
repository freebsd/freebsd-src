/*
 *  linux/drivers/video/fm2fb.c -- BSC FrameMaster II/Rainbow II frame buffer
 *				   device
 *
 *	Copyright (C) 1998 Steffen A. Mork (mork@ls7.cs.uni-dortmund.de)
 *	Copyright (C) 1999 Geert Uytterhoeven
 *
 *  Written for 2.0.x by Steffen A. Mork
 *  Ported to 2.1.x by Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/zorro.h>

#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb32.h>


/*
 *	Some technical notes:
 *
 *	The BSC FrameMaster II (or Rainbow II) is a simple very dumb
 *	frame buffer which allows to display 24 bit true color images.
 *	Each pixel is 32 bit width so it's very easy to maintain the
 *	frame buffer. One long word has the following layout:
 *	AARRGGBB which means: AA the alpha channel byte, RR the red
 *	channel, GG the green channel and BB the blue channel.
 *
 *	The FrameMaster II supports the following video modes.
 *	- PAL/NTSC
 *	- interlaced/non interlaced
 *	- composite sync/sync/sync over green
 *
 *	The resolution is to the following both ones:
 *	- 768x576 (PAL)
 *	- 768x480 (NTSC)
 *
 *	This means that pixel access per line is fixed due to the
 *	fixed line width. In case of maximal resolution the frame
 *	buffer needs an amount of memory of 1.769.472 bytes which
 *	is near to 2 MByte (the allocated address space of Zorro2).
 *	The memory is channel interleaved. That means every channel
 *	owns four VRAMs. Unfortunatly most FrameMasters II are
 *	not assembled with memory for the alpha channel. In this
 *	case it could be possible to add the frame buffer into the
 *	normal memory pool.
 *	
 *	At relative address 0x1ffff8 of the frame buffers base address
 *	there exists a control register with the number of
 *	four control bits. They have the following meaning:
 *	bit value meaning
 *
 *	 0    1   0=interlaced/1=non interlaced
 *	 1    2   0=video out disabled/1=video out enabled
 *	 2    4   0=normal mode as jumpered via JP8/1=complement mode
 *	 3    8   0=read  onboard ROM/1 normal operation (required)
 *
 *	As mentioned above there are several jumper. I think there
 *	is not very much information about the FrameMaster II in
 *	the world so I add these information for completeness.
 *
 *	JP1  interlace selection (1-2 non interlaced/2-3 interlaced) 
 *	JP2  wait state creation (leave as is!)
 *	JP3  wait state creation (leave as is!)
 *	JP4  modulate composite sync on green output (1-2 composite
 *	     sync on green channel/2-3 normal composite sync)
 *	JP5  create test signal, shorting this jumper will create
 *	     a white screen
 *	JP6  sync creation (1-2 composite sync/2-3 H-sync output)
 *	JP8  video mode (1-2 PAL/2-3 NTSC)
 *
 *	With the following jumpering table you can connect the
 *	FrameMaster II to a normal TV via SCART connector:
 *	JP1:  2-3
 *	JP4:  2-3
 *	JP6:  2-3
 *	JP8:  1-2 (means PAL for Europe)
 *
 *	NOTE:
 *	There is no other possibility to change the video timings
 *	except the interlaced/non interlaced, sync control and the
 *	video mode PAL (50 Hz)/NTSC (60 Hz). Inside this
 *	FrameMaster II driver are assumed values to avoid anomalies
 *	to a future X server. Except the pixel clock is really
 *	constant at 30 MHz.
 *
 *	9 pin female video connector:
 *
 *	1  analog red 0.7 Vss
 *	2  analog green 0.7 Vss
 *	3  analog blue 0.7 Vss
 *	4  H-sync TTL
 *	5  V-sync TTL
 *	6  ground
 *	7  ground
 *	8  ground
 *	9  ground
 *
 *	Some performance notes:
 *	The FrameMaster II was not designed to display a console
 *	this driver would do! It was designed to display still true
 *	color images. Imagine: When scroll up a text line there
 *	must copied ca. 1.7 MBytes to another place inside this
 *	frame buffer. This means 1.7 MByte read and 1.7 MByte write
 *	over the slow 16 bit wide Zorro2 bus! A scroll of one
 *	line needs 1 second so do not expect to much from this
 *	driver - he is at the limit!
 *
 */


/*
 *	definitions
 */

#define FRAMEMASTER_SIZE	0x200000
#define FRAMEMASTER_REG		0x1ffff8

#define FRAMEMASTER_NOLACE	1
#define FRAMEMASTER_ENABLE	2
#define FRAMEMASTER_COMPL	4
#define FRAMEMASTER_ROM		8


struct FrameMaster_fb_par
{
	int xres;
	int yres;
	int bpp;
	int pixclock;
};

static unsigned long fm2fb_mem_phys;
static void *fm2fb_mem;
static unsigned long fm2fb_reg_phys;
static volatile unsigned char *fm2fb_reg;

static int currcon = 0;
static struct display disp;
static struct fb_info fb_info;
static struct { u_char red, green, blue, pad; } palette[16];
#ifdef FBCON_HAS_CFB32
static u32 fbcon_cfb32_cmap[16];
#endif

static struct fb_fix_screeninfo fb_fix;
static struct fb_var_screeninfo fb_var;

static int fm2fb_mode __initdata = -1;

#define FM2FB_MODE_PAL	0
#define FM2FB_MODE_NTSC	1

static struct fb_var_screeninfo fb_var_modes[] __initdata = {
    {
	/* 768 x 576, 32 bpp (PAL) */
	768, 576, 768, 576, 0, 0, 32, 0,
	{ 16, 8, 0 }, { 8, 8, 0 }, { 0, 8, 0 }, { 24, 8, 0 },
	0, FB_ACTIVATE_NOW, -1, -1, FB_ACCEL_NONE,
	33333, 10, 102, 10, 5, 80, 34, FB_SYNC_COMP_HIGH_ACT, 0
    }, {
	/* 768 x 480, 32 bpp (NTSC - not supported yet */
	768, 480, 768, 480, 0, 0, 32, 0,
	{ 16, 8, 0 }, { 8, 8, 0 }, { 0, 8, 0 }, { 24, 8, 0 },
	0, FB_ACTIVATE_NOW, -1, -1, FB_ACCEL_NONE,
	33333, 10, 102, 10, 5, 80, 34, FB_SYNC_COMP_HIGH_ACT, 0
    }
};


    /*
     *  Interface used by the world
     */

static int fm2fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info);
static int fm2fb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
static int fm2fb_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
static int fm2fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);
static int fm2fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info);


    /*
     *  Interface to the low level console driver
     */

int fm2fb_init(void);
static int fm2fbcon_switch(int con, struct fb_info *info);
static int fm2fbcon_updatevar(int con, struct fb_info *info);
static void fm2fbcon_blank(int blank, struct fb_info *info);


    /*
     *  Internal routines
     */

static int fm2fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			   u_int *transp, struct fb_info *info);
static int fm2fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			   u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);


static struct fb_ops fm2fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	fm2fb_get_fix,
	fb_get_var:	fm2fb_get_var,
	fb_set_var:	fm2fb_set_var,
	fb_get_cmap:	fm2fb_get_cmap,
	fb_set_cmap:	fm2fb_set_cmap,
};

    /*
     *  Get the Fixed Part of the Display
     */

static int fm2fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
    memcpy(fix, &fb_fix, sizeof(fb_fix));
    return 0;
}


    /*
     *  Get the User Defined Part of the Display
     */

static int fm2fb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
    memcpy(var, &fb_var, sizeof(fb_var));
    return 0;
}


    /*
     *  Set the User Defined Part of the Display
     */

static int fm2fb_set_var(struct fb_var_screeninfo *var, int con,
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
    }
    if (oldbpp != var->bits_per_pixel) {
	if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
	    return err;
	do_install_cmap(con, info);
    }
    return 0;
}


    /*
     *  Get the Colormap
     */

static int fm2fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
    if (con == currcon) /* current console? */
	return fb_get_cmap(cmap, kspc, fm2fb_getcolreg, info);
    else if (fb_display[con].cmap.len) /* non default colormap? */
	fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
    else
	fb_copy_cmap(fb_default_cmap(256), cmap, kspc ? 0 : 2);
    return 0;
}

    /*
     *  Set the Colormap
     */

static int fm2fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
    int err;

    if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
	if ((err = fb_alloc_cmap(&fb_display[con].cmap, 256, 0)))
	    return err;
    }
    if (con == currcon) {		/* current console? */
	err = fb_set_cmap(cmap, kspc, fm2fb_setcolreg, info);
	return err;
    } else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}


    /*
     *  Initialisation
     */

int __init fm2fb_init(void)
{
    int is_fm;
    struct zorro_dev *z = NULL;
    unsigned long *ptr;
    int x, y;

    while ((z = zorro_find_device(ZORRO_WILDCARD, z))) {
	if (z->id == ZORRO_PROD_BSC_FRAMEMASTER_II)
	    is_fm = 1;
	else if (z->id == ZORRO_PROD_HELFRICH_RAINBOW_II)
	    is_fm = 0;
	else
	    continue;
	if (!request_mem_region(z->resource.start, FRAMEMASTER_SIZE, "fm2fb"))
	    continue;

	/* assigning memory to kernel space */
	fm2fb_mem_phys = z->resource.start;
	fm2fb_mem  = ioremap(fm2fb_mem_phys, FRAMEMASTER_SIZE);
	fm2fb_reg_phys = fm2fb_mem_phys+FRAMEMASTER_REG;
	fm2fb_reg  = (unsigned char *)(fm2fb_mem+FRAMEMASTER_REG);

	/* make EBU color bars on display */
	ptr = (unsigned long *)fm2fb_mem;
	for (y = 0; y < 576; y++) {
	    for (x = 0; x < 96; x++) *ptr++ = 0xffffff;	/* white */
	    for (x = 0; x < 96; x++) *ptr++ = 0xffff00;	/* yellow */
	    for (x = 0; x < 96; x++) *ptr++ = 0x00ffff;	/* cyan */
	    for (x = 0; x < 96; x++) *ptr++ = 0x00ff00;	/* green */
	    for (x = 0; x < 96; x++) *ptr++ = 0xff00ff;	/* magenta */
	    for (x = 0; x < 96; x++) *ptr++ = 0xff0000;	/* red */
	    for (x = 0; x < 96; x++) *ptr++ = 0x0000ff;	/* blue */
	    for (x = 0; x < 96; x++) *ptr++ = 0x000000;	/* black */
	}
	fm2fbcon_blank(0, NULL);

	if (fm2fb_mode == -1)
	    fm2fb_mode = FM2FB_MODE_PAL;

	fb_var = fb_var_modes[fm2fb_mode];

	strcpy(fb_fix.id, is_fm ? "FrameMaster II" : "Rainbow II");
	fb_fix.smem_start = fm2fb_mem_phys;
	fb_fix.smem_len = FRAMEMASTER_REG;
	fb_fix.type = FB_TYPE_PACKED_PIXELS;
	fb_fix.type_aux = 0;
	fb_fix.visual = FB_VISUAL_TRUECOLOR;
	fb_fix.line_length = 768<<2;
	fb_fix.mmio_start = fm2fb_reg_phys;
	fb_fix.mmio_len = 8;
	fb_fix.accel = FB_ACCEL_NONE;

	disp.var = fb_var;
	disp.cmap.start = 0;
	disp.cmap.len = 0;
	disp.cmap.red = disp.cmap.green = disp.cmap.blue = disp.cmap.transp = NULL;
	disp.screen_base = (char *)fm2fb_mem;
	disp.visual = fb_fix.visual;
	disp.type = fb_fix.type;
	disp.type_aux = fb_fix.type_aux;
	disp.ypanstep = 0;
	disp.ywrapstep = 0;
	disp.line_length = fb_fix.line_length;
	disp.can_soft_blank = 1;
	disp.inverse = 0;
    #ifdef FBCON_HAS_CFB32
	disp.dispsw = &fbcon_cfb32;
	disp.dispsw_data = &fbcon_cfb32_cmap;
    #else
	disp.dispsw = &fbcon_dummy;
    #endif
	disp.scrollmode = SCROLL_YREDRAW;

	strcpy(fb_info.modename, fb_fix.id);
	fb_info.node = -1;
	fb_info.fbops = &fm2fb_ops;
	fb_info.disp = &disp;
	fb_info.fontname[0] = '\0';
	fb_info.changevar = NULL;
	fb_info.switch_con = &fm2fbcon_switch;
	fb_info.updatevar = &fm2fbcon_updatevar;
	fb_info.blank = &fm2fbcon_blank;
	fb_info.flags = FBINFO_FLAG_DEFAULT;

	fm2fb_set_var(&fb_var, -1, &fb_info);

	if (register_framebuffer(&fb_info) < 0)
	    return -EINVAL;

	printk("fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.node),
	       fb_fix.id);
	return 0;
    }
    return -ENXIO;
}

int __init fm2fb_setup(char *options)
{
    char *this_opt;

    if (!options || !*options)
	return 0;

    while ((this_opt = strsep(&options, ",")) != NULL) {
	if (!strncmp(this_opt, "pal", 3))
	    fm2fb_mode = FM2FB_MODE_PAL;
	else if (!strncmp(this_opt, "ntsc", 4))
	    fm2fb_mode = FM2FB_MODE_NTSC;
    }
    return 0;
}


static int fm2fbcon_switch(int con, struct fb_info *info)
{
    /* Do we have to save the colormap? */
    if (fb_display[currcon].cmap.len)
	fb_get_cmap(&fb_display[currcon].cmap, 1, fm2fb_getcolreg, info);

    currcon = con;
    /* Install new colormap */
    do_install_cmap(con, info);
    return 0;
}

    /*
     *  Update the `var' structure (called by fbcon.c)
     */

static int fm2fbcon_updatevar(int con, struct fb_info *info)
{
    /* Nothing */
    return 0;
}

    /*
     *  Blank the display.
     */

static void fm2fbcon_blank(int blank, struct fb_info *info)
{
    unsigned char t = FRAMEMASTER_ROM;

    if (!blank)
	t |= FRAMEMASTER_ENABLE | FRAMEMASTER_NOLACE;
    fm2fb_reg[0] = t;
}

    /*
     *  Read a single color register and split it into
     *  colors/transparent. Return != 0 for invalid regno.
     */

static int fm2fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
    if (regno > 15)
	return 1;
    *red = (palette[regno].red<<8) | palette[regno].red;
    *green = (palette[regno].green<<8) | palette[regno].green;
    *blue = (palette[regno].blue<<8) | palette[regno].blue;
    *transp = 0;
    return 0;
}


    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int fm2fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
    if (regno > 15)
	return 1;
    red >>= 8;
    green >>= 8;
    blue >>= 8;
    palette[regno].red = red;
    palette[regno].green = green;
    palette[regno].blue = blue;

#ifdef FBCON_HAS_CFB32
    fbcon_cfb32_cmap[regno] = (red << 16) | (green << 8) | blue;
#endif
    return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
    if (con != currcon)
	return;
    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, fm2fb_setcolreg, info);
    else
	fb_set_cmap(fb_default_cmap(256), 1, fm2fb_setcolreg, info);
}

MODULE_LICENSE("GPL");
