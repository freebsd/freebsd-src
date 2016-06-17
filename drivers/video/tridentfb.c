/*
 * Frame buffer driver for Trident Blade and Image series
 *
 * Copyright 2001,2002 - Jani Monoses   <jani@iv.ro>
 *
 *
 * CREDITS:(in order of appearance)
 * 	skeletonfb.c by Geert Uytterhoeven and other fb code in drivers/video
 * 	Special thanks ;) to Mattia Crivellini <tia@mclink.it>
 * 	much inspired by the XFree86 4.x Trident driver sources by Alan Hourihane
 * 	the FreeVGA project
 *	Francesco Salvestrini <salvestrini@users.sf.net> XP support,code,suggestions
 * TODO:
 * 	timing value tweaking so it looks good on every monitor in every mode
 *	TGUI acceleration	
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "tridentfb.h"

#define VERSION		"0.7.5"

struct tridentfb_par {
	struct fb_var_screeninfo var;
	int bpp;
	int hres;
	int vres;
	int linelength;
	int vclk;		//in MHz

	int vtotal;
	int vdispend;
	int vsyncstart;
	int vsyncend;
	int vblankstart;
	int vblankend;

	int htotal;
	int hdispend;
	int hsyncstart;
	int hsyncend;
	int hblankstart;
	int hblankend;
};

struct tridentfb_info {
	struct fb_info_gen gen;
	unsigned long fbmem_virt;	//framebuffer virtual memory address
	unsigned long fbmem;		//framebuffer physical memory address
	unsigned int memsize;		//size of fbmem
	unsigned long io;		//io space address
	unsigned long io_virt;		//iospace virtual memory address
	unsigned int nativex;		//flat panel xres
	struct tridentfb_par currentmode;
	unsigned char eng_oper;		//engine operation...
};

static struct fb_ops tridentfb_ops;

static struct tridentfb_info fb_info;
static struct display disp;

static struct { unsigned char red,green,blue,transp; } palette[256];

static struct fb_var_screeninfo default_var;

static char * tridentfb_name = "Trident";

static int chip_id;

static int defaultaccel;
static int displaytype;

static int pseudo_pal[16];

/* defaults which are normally overriden by user values */

/* video mode */
static char * mode = "640x480";
static int bpp = 8;

static int noaccel;

static int center;
static int stretch;

static int fp;
static int crt;

static int memsize;
static int memdiff;
static int nativex;


MODULE_PARM(mode,"s");
MODULE_PARM(bpp,"i");
MODULE_PARM(center,"i");
MODULE_PARM(stretch,"i");
MODULE_PARM(noaccel,"i");
MODULE_PARM(memsize,"i");
MODULE_PARM(memdiff,"i");
MODULE_PARM(nativex,"i");
MODULE_PARM(fp,"i");
MODULE_PARM(crt,"i");

static int chip3D;
int is3Dchip(int id)
{
	return 	((id == BLADE3D) || (id == CYBERBLADEE4) ||
		 (id == CYBERBLADEi7) || (id == CYBERBLADEi7D) ||
		 (id == CYBER9397) || (id == CYBER9397DVD) ||
		 (id == CYBER9520) || (id == CYBER9525DVD) ||
		 (id == IMAGE975) || (id == IMAGE985) ||
		 (id == CYBERBLADEi1) || (id == CYBERBLADEi1D) ||
		 (id ==	CYBERBLADEAi1) || (id == CYBERBLADEAi1D) ||
		 (id ==	CYBERBLADEXPm8) || (id == CYBERBLADEXPm16) ||
		 (id ==	CYBERBLADEXPAi1));
}

#define CRT 0x3D0		//CRTC registers offset for color display

#ifndef TRIDENT_MMIO
	#define TRIDENT_MMIO 1
#endif

#if TRIDENT_MMIO
	#define t_outb(val,reg)	writeb(val,fb_info.io_virt + reg)
	#define t_inb(reg)	readb(fb_info.io_virt + reg)
#else
	#define t_outb(val,reg) outb(val,reg)
	#define t_inb(reg) inb(reg)
#endif


static struct accel_switch {
	void (*init_accel)(int,int);
	void (*wait_engine)(void);
	void (*fill_rect)(int,int,int,int,int);
	void (*copy_rect)(int,int,int,int,int,int);
} *acc;

#define writemmr(r,v)	writel(v, fb_info.io_virt + r)
#define readmmr(r)	readl(fb_info.io_virt + r)



/*
 * Blade specific acceleration.
 */

#define point(x,y) ((y)<<16|(x))
#define STA	0x2120
#define CMD	0x2144
#define ROP	0x2148
#define CLR	0x2160
#define SR1	0x2100
#define SR2	0x2104
#define DR1	0x2108
#define DR2	0x210C

#define REPL(x)	x = x | x<<16
#define ROP_S	0xCC

static void blade_init_accel(int pitch,int bpp)
{
	int v1 = (pitch>>3)<<20;
	int tmp = 0,v2;
	switch (bpp) {
		case 8:tmp = 0;break;
		case 15:tmp = 5;break;
		case 16:tmp = 1;break;
		case 24:
		case 32:tmp = 2;break;
	}
	v2 = v1 | (tmp<<29);
	writemmr(0x21C0,v2);
	writemmr(0x21C4,v2);
	writemmr(0x21B8,v2);
	writemmr(0x21BC,v2);
	writemmr(0x21D0,v1);
	writemmr(0x21D4,v1);
	writemmr(0x21C8,v1);
	writemmr(0x21CC,v1);
	writemmr(0x216C,0);
}

static void blade_wait_engine(void)
{
	while(readmmr(STA) & 0xFA800000);
}

static void blade_fill_rect(int x,int y,int w,int h,int c)
{
	writemmr(CLR,c);
	writemmr(ROP,ROP_S);
	writemmr(CMD,0x20000000|1<<19|1<<4|2<<2);

	writemmr(DR1,point(x,y));
	writemmr(DR2,point(x+w-1,y+h-1));
}

static void blade_copy_rect(int x1,int y1,int x2,int y2,int w,int h)
{
	int s1,s2,d1,d2;
	int direction = 2;
	s1 = point(x1,y1);
	s2 = point(x1+w-1,y1+h-1);
	d1 = point(x2,y2);
	d2 = point(x2+w-1,y2+h-1);

	if ((y1 > y2) || ((y1 == y2) && (x1 > x2)))
			direction = 0;


	writemmr(ROP,ROP_S);
	writemmr(CMD,0xE0000000|1<<19|1<<4|1<<2|direction);

	writemmr(SR1,direction?s2:s1);
	writemmr(SR2,direction?s1:s2);
	writemmr(DR1,direction?d2:d1);
	writemmr(DR2,direction?d1:d2);
}

static struct accel_switch accel_blade = {
	blade_init_accel,
	blade_wait_engine,
	blade_fill_rect,
	blade_copy_rect,
};


/*
 * BladeXP specific acceleration functions
 */

#define ROP_P 0xF0
#define masked_point(x,y) ((y & 0xffff)<<16|(x & 0xffff))

static void xp_init_accel(int pitch,int bpp)
{
	int tmp = 0,v1;
	unsigned char x = 0;

	switch (bpp) {
		case 8:  x = 0; break;
		case 16: x = 1; break;
		case 24: x = 3; break;
		case 32: x = 2; break;
	}

	switch (pitch << (bpp >> 3)) {
		case 8192:
		case 512:  x |= 0x00; break;
		case 1024: x |= 0x04; break;
		case 2048: x |= 0x08; break;
		case 4096: x |= 0x0C; break;
	}

	t_outb(x,0x2125);

	fb_info.eng_oper = x | 0x40;

	switch (bpp) {
		case 8:  tmp = 18; break;
		case 15:
		case 16: tmp = 19; break;
		case 24:
		case 32: tmp = 20; break;
	}

	v1 = pitch << tmp;

	writemmr(0x2154,v1);
	writemmr(0x2150,v1);
	t_outb(3,0x2126);
}

static void xp_wait_engine(void)
{
	int busy;
	int count, timeout;

	count = 0;
	timeout = 0;
	for (;;) {
		busy = t_inb(STA) & 0x80;
		if (busy != 0x80)
			return;
		count++;
		if (count == 10000000) {
			/* Timeout */
			count = 9990000;
			timeout++;
			if (timeout == 8) {
				/* Reset engine */
				t_outb(0x00, 0x2120);
				return;
			}
		}
	}
}

static void xp_fill_rect(int x,int y,int w,int h,int c)
{
	writemmr(0x2127,ROP_P);
	writemmr(0x2158,c);
	writemmr(0x2128,0x4000);
	writemmr(0x2140,masked_point(h,w));
	writemmr(0x2138,masked_point(y,x));
	t_outb(0x01,0x2124);
        t_outb(fb_info.eng_oper,0x2125);
}

static void xp_copy_rect(int x1,int y1,int x2,int y2,int w,int h)
{
	int direction;
	int x1_tmp, x2_tmp, y1_tmp, y2_tmp;

	direction = 0x0004;
	
	if ((x1 < x2) && (y1 == y2)) {
		direction |= 0x0200;
		x1_tmp = x1 + w - 1;
		x2_tmp = x2 + w - 1;
	} else {
		x1_tmp = x1;
		x2_tmp = x2;
	}
  
	if (y1 < y2) {
		direction |= 0x0100;
		y1_tmp = y1 + h - 1;
		y2_tmp = y2 + h - 1;
  	} else {
		y1_tmp = y1;
		y2_tmp = y2;
	}

	writemmr(0x2128,direction);	
	t_outb(ROP_S,0x2127);
	writemmr(0x213C,masked_point(y1_tmp,x1_tmp));
	writemmr(0x2138,masked_point(y2_tmp,x2_tmp));
	writemmr(0x2140,masked_point(h,w));
	t_outb(0x01,0x2124);
}

static struct accel_switch accel_xp = {
  	xp_init_accel,
	xp_wait_engine,
	xp_fill_rect,
	xp_copy_rect,
};


/*
 * Image specific acceleration functions
 */
static void image_init_accel(int pitch,int bpp)
{
	int tmp = 0;
   	switch (bpp) {
		case 8:tmp = 0;break;
		case 15:tmp = 5;break;
		case 16:tmp = 1;break;
		case 24:
		case 32:tmp = 2;break;
	}
	writemmr(0x2120, 0xF0000000);
	writemmr(0x2120, 0x40000000|tmp);
	writemmr(0x2120, 0x80000000);
	writemmr(0x2144, 0x00000000);
	writemmr(0x2148, 0x00000000);
	writemmr(0x2150, 0x00000000);
	writemmr(0x2154, 0x00000000);
	writemmr(0x2120, 0x60000000|(pitch<<16) |pitch);
	writemmr(0x216C, 0x00000000);
	writemmr(0x2170, 0x00000000);
	writemmr(0x217C, 0x00000000);
	writemmr(0x2120, 0x10000000);
	writemmr(0x2130, (2047 << 16) | 2047);
}

static void image_wait_engine(void)
{
	while(readmmr(0x2164) & 0xF0000000);
}

static void image_fill_rect(int x,int y,int w,int h,int c)
{
	writemmr(0x2120,0x80000000);
	writemmr(0x2120,0x90000000|ROP_S);

	writemmr(0x2144,c);

	writemmr(DR1,point(x,y));
	writemmr(DR2,point(x+w-1,y+h-1));

	writemmr(0x2124,0x80000000|3<<22|1<<10|1<<9);
}

static void image_copy_rect(int x1,int y1,int x2,int y2,int w,int h)
{
	int s1,s2,d1,d2;
	int direction = 2;
	s1 = point(x1,y1);
	s2 = point(x1+w-1,y1+h-1);
	d1 = point(x2,y2);
	d2 = point(x2+w-1,y2+h-1);

	if ((y1 > y2) || ((y1 == y2) && (x1 >x2)))
			direction = 0;

	writemmr(0x2120,0x80000000);
	writemmr(0x2120,0x90000000|ROP_S);

	writemmr(SR1,direction?s2:s1);
	writemmr(SR2,direction?s1:s2);
	writemmr(DR1,direction?d2:d1);
	writemmr(DR2,direction?d1:d2);
	writemmr(0x2124,0x80000000|1<<22|1<<10|1<<7|direction);
}


static struct accel_switch accel_image = {
	image_init_accel,
	image_wait_engine,
	image_fill_rect,
	image_copy_rect,
};

/*
 * Accel functions called by the upper layers
 */

static void trident_bmove (struct display *p, int sy, int sx,
				int dy, int dx, int height, int width)
{
	sx *= fontwidth(p);
	dx *= fontwidth(p);
	width *= fontwidth(p);
	sy *= fontheight(p);
	dy *= fontheight(p);
	height *= fontheight(p);
	acc->copy_rect(sx,sy,dx,dy,width,height);
	acc->wait_engine();
}
static void trident_clear_helper (int c, struct display *p,
				int sy, int sx, int height, int width)
{
	sx *= fontwidth(p);
	sy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);
	acc->fill_rect(sx,sy,width,height,c);
	acc->wait_engine();
}


#ifdef FBCON_HAS_CFB8
static void trident_8bpp_clear (struct vc_data *conp, struct display *p,
				int sy, int sx, int height, int width)
{
	int c;
	c = attr_bgcol_ec(p,conp) & 0xFF;
	c |= c<<8;
	c |= c<<16;
	trident_clear_helper(c,p,sy,sx,height,width);
}

static struct display_switch trident_8bpp = {
	setup:		fbcon_cfb8_setup,
	bmove:		trident_bmove,
	clear:		trident_8bpp_clear,
	putc:		fbcon_cfb8_putc,
	putcs:		fbcon_cfb8_putcs,
	revc:		fbcon_cfb8_revc,
	clear_margins:	fbcon_cfb8_clear_margins,
	fontwidthmask:	FONTWIDTH (4) | FONTWIDTH (8) | FONTWIDTH (12) | FONTWIDTH (16)
};
#endif
#ifdef FBCON_HAS_CFB16
static void trident_16bpp_clear (struct vc_data *conp, struct display *p,
				int sy, int sx, int height, int width)
{
	int c;
	c = ((u16*)p->dispsw_data)[attr_bgcol_ec(p,conp)];
	c = c | c<<16;
	trident_clear_helper(c,p,sy,sx,height,width);
}

static struct display_switch trident_16bpp = {
	setup:		fbcon_cfb16_setup,
	bmove:		trident_bmove,
	clear:		trident_16bpp_clear,
	putc:		fbcon_cfb16_putc,
	putcs:		fbcon_cfb16_putcs,
	revc:		fbcon_cfb16_revc,
	clear_margins:	fbcon_cfb16_clear_margins,
	fontwidthmask:	FONTWIDTH (4) | FONTWIDTH (8) | FONTWIDTH (12) | FONTWIDTH (16)
};
#endif
#ifdef FBCON_HAS_CFB32
static void trident_32bpp_clear (struct vc_data *conp, struct display *p,
				int sy, int sx, int height, int width)
{
	int c;
	c = ((u32*)p->dispsw_data)[attr_bgcol_ec(p,conp)];
	trident_clear_helper(c,p,sy,sx,height,width);
}

static struct display_switch trident_32bpp = {
	setup:		fbcon_cfb32_setup,
	bmove:		trident_bmove,
	clear:		trident_32bpp_clear,
	putc:		fbcon_cfb32_putc,
	putcs:		fbcon_cfb32_putcs,
	revc:		fbcon_cfb32_revc,
	clear_margins:	fbcon_cfb32_clear_margins,
	fontwidthmask:	FONTWIDTH (4) | FONTWIDTH (8) | FONTWIDTH (12) | FONTWIDTH (16)
};
#endif

/*
 * Hardware access functions
 */

static inline unsigned char read3X4(int reg)
{
	writeb(reg, fb_info.io_virt + CRT + 4);
	return readb(fb_info.io_virt + CRT + 5);
}

static inline void write3X4(int reg, unsigned char val)
{
	writeb(reg, fb_info.io_virt + CRT + 4);
	writeb(val, fb_info.io_virt + CRT + 5);
}

static inline unsigned char read3C4(int reg)
{
	t_outb(reg, 0x3C4);
	return t_inb(0x3C5);
}

static inline void write3C4(int reg, unsigned char val)
{
	t_outb(reg, 0x3C4);
	t_outb(val, 0x3C5);
}

static inline unsigned char read3CE(int reg)
{
	t_outb(reg, 0x3CE);
	return t_inb(0x3CF);
}

static inline void writeAttr(int reg, unsigned char val)
{
	readb(fb_info.io_virt + CRT + 0x0A);	//flip-flop to index
	t_outb(reg, 0x3C0);
	t_outb(val, 0x3C0);
}

static inline unsigned char readAttr(int reg)
{
	readb(fb_info.io_virt + CRT + 0x0A);	//flip-flop to index
	t_outb(reg, 0x3C0);
	return t_inb(0x3C1);
}

static inline void write3CE(int reg, unsigned char val)
{
	t_outb(reg, 0x3CE);
	t_outb(val, 0x3CF);
}

#define bios_reg(reg) 	write3CE(BiosReg, reg)
#if 0
static inline void unprotect_all(void)
{
	outb(Protection, 0x3C4);
	outb(0x92, 0x3C5);
}
#endif
static inline void enable_mmio(void)
{
	/* Goto New Mode */
	outb(0x0B, 0x3C4);
	inb(0x3C5);

	/* Unprotect registers */
	outb(NewMode1, 0x3C4);
	outb(0x80, 0x3C5);
  
	/* Enable MMIO */
	outb(PCIReg, 0x3D4); 
	outb(inb(0x3D5) | 0x01, 0x3D5);
}


#define crtc_unlock()	write3X4(CRTVSyncEnd, read3X4(CRTVSyncEnd) & 0x7F)

/*  Return flat panel's maximum x resolution */
static int __init get_nativex(void)
{
	int x,y,tmp;

	if (nativex)
		return nativex;

       	tmp = (read3CE(VertStretch) >> 4) & 3;

	switch (tmp) {
		case 0: x = 1280; y = 1024; break;
		case 2: x = 1024; y = 768;  break;
		case 3: x = 800;  y = 600;  break; 
		case 4: x = 1400; y = 1050; break;
		case 1: 
		default:x = 640;  y = 480;  break;
	}

	output("%dx%d flat panel found\n", x, y);
	return x;
}

/* Set pitch */
static void set_lwidth(int width)
{
	write3X4(Offset, width & 0xFF);
	write3X4(AddColReg, (read3X4(AddColReg) & 0xCF) | ((width & 0x300) >>4));
}

/* For resolutions smaller than FP resolution stretch */
static void screen_stretch(void)
{
	write3CE(VertStretch,(read3CE(VertStretch) & 0x7C) | 1);
	write3CE(HorStretch,(read3CE(HorStretch) & 0x7C) | 1);
}

/* For resolutions smaller than FP resolution center */
static void screen_center(void)
{
	bios_reg(0);		// no stretch
	write3CE(VertStretch,(read3CE(VertStretch) & 0x7C) | 0x80);
	write3CE(HorStretch,(read3CE(HorStretch) & 0x7C) | 0x80);
}

/* Address of first shown pixel in display memory */
static void set_screen_start(int base)
{
	write3X4(StartAddrLow, base & 0xFF);
	write3X4(StartAddrHigh, (base & 0xFF00) >> 8);
	write3X4(CRTCModuleTest, (read3X4(CRTCModuleTest) & 0xDF) | ((base & 0x10000) >> 11));
	write3X4(CRTHiOrd, (read3X4(CRTHiOrd) & 0xF8) | ((base & 0xE0000) >> 17));
}

/* Use 20.12 fixed-point for NTSC value and frequency calculation */
#define calc_freq(n,m,k)  ( ((unsigned long)0xE517 * (n+8) / ((m+2)*(1<<k))) >> 12 )

/* Set dotclock frequency */
static void set_vclk(int freq)
{
	int m,n,k;
	int f,fi,d,di;
	unsigned char lo=0,hi=0;

	d = 20;
	for(k = 2;k>=0;k--)
	for(m = 0;m<63;m++)
	for(n = 0;n<128;n++) {
		fi = calc_freq(n,m,k);
		if ((di = abs(fi - freq)) < d) {
			d = di;
			f = fi;
			lo = n;
			hi = (k<<6) | m;
		}
	}
	if (chip3D) {
		write3C4(ClockHigh,hi);
		write3C4(ClockLow,lo);
	} else {
		outb(lo,0x43C8);
		outb(hi,0x43C9);
	}
	debug("VCLK = %X %X\n",hi,lo);
}

/* Set number of lines for flat panels*/
static void set_number_of_lines(int lines)
{
	int tmp = read3CE(CyberEnhance) & 0x8F;
	if (lines > 768)
		tmp |= 0x30;
	else if (lines > 600)
		tmp |= 0x20;
	else if (lines > 480)
		tmp |= 0x10;
	write3CE(CyberEnhance, tmp);
}

/*
 * If we see that FP is active we assume we have one.
 * Otherwise we have a CRT display.User can override.
 */
static unsigned int __init get_displaytype(void)
{
	if (fp)
		return DISPLAY_FP;
	if (crt)
		return DISPLAY_CRT;
	return (read3CE(FPConfig) & 0x10)?DISPLAY_FP:DISPLAY_CRT;
}

/* Try detecting the video memory size */
static unsigned int __init get_memsize(void)
{
	unsigned char tmp, tmp2;
	unsigned int k;

	/* If memory size provided by user */
	if (memsize)
		k = memsize * Kb;
	else
	switch (chip_id) {
		case CYBER9525DVD:    k = 2560 * Kb; break;
		default:
			tmp = read3X4(SPR) & 0x0F;
			switch (tmp) {

				case 0x01: k = 512;     break;
				case 0x02: k = 6 * Mb;  break; /* XP */
				case 0x03: k = 1 * Mb;  break;
				case 0x04: k = 8 * Mb;  break;
				case 0x06: k = 10 * Mb; break; /* XP */
				case 0x07: k = 2 * Mb;  break;
				case 0x08: k = 12 * Mb; break; /* XP */
				case 0x0A: k = 14 * Mb; break; /* XP */
				case 0x0C: k = 16 * Mb; break; /* XP */
				case 0x0E:                     /* XP */
  
					tmp2 = read3C4(0xC1);
					switch (tmp2) {
						case 0x00: k = 20 * Mb; break;
						case 0x01: k = 24 * Mb; break;
						case 0x10: k = 28 * Mb; break;
						case 0x11: k = 32 * Mb; break;
						default:   k = 1 * Mb;  break;
					}
				break;
	
				case 0x0F: k = 4 * Mb; break;
				default:   k = 1 * Mb;
			}
	}

	k -= memdiff * Kb;
	output("framebuffer size = %d Kb\n", k/Kb);
	return k;
}

/* Fill in fix */
static int trident_encode_fix(struct fb_fix_screeninfo *fix,
				  const void *par,
				  struct fb_info_gen *info)
{
	struct tridentfb_info * i = (struct tridentfb_info *)info;
	struct tridentfb_par * p = (struct tridentfb_par *)par;

	debug("enter\n");

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id,tridentfb_name);

	fix->smem_start = i->fbmem;
	fix->smem_len = i->memsize;

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;

	fix->visual = p->bpp==8 ? FB_VISUAL_PSEUDOCOLOR:FB_VISUAL_TRUECOLOR;

	fix->xpanstep = fix->ywrapstep = 0;
	fix->ypanstep = 1;
	fix->line_length = p->linelength;
	fix->mmio_start = 0;
	fix->mmio_len = 0;

	fix->accel = FB_ACCEL_NONE;

	debug("exit\n");
	return 0;
}

/* Fill in par from var */
static int trident_decode_var(const struct fb_var_screeninfo *var,
				  void *par,
				  struct fb_info_gen *info)
{
	struct tridentfb_par * p = (struct tridentfb_par *)par;
	struct tridentfb_info * i = (struct tridentfb_info *)info;
	int vres,vfront,vback,vsync;
	debug("enter\n");
	p->var = *var;
	p->bpp = var->bits_per_pixel;

	if (p->bpp == 24 )
		p->bpp = 32;

	p->linelength = var->xres_virtual * p->bpp/8;

	switch (p->bpp) {
		case 8:
			p->var.red.offset = 0;
			p->var.green.offset = 0;
			p->var.blue.offset = 0;
			p->var.red.length = 6;
			p->var.green.length = 6;
			p->var.blue.length = 6;
			break;
		case 16:
			p->var.red.offset = 11;
			p->var.green.offset = 5;
			p->var.blue.offset = 0;
			p->var.red.length = 5;
			p->var.green.length = 6;
			p->var.blue.length = 5;
			break;
		case 32:
			p->var.red.offset = 16;
			p->var.green.offset = 8;
			p->var.blue.offset = 0;
			p->var.red.length = 8;
			p->var.green.length = 8;
			p->var.blue.length = 8;
			break;
		default:
			return -EINVAL;
	}

	/* convert from picoseconds to MHz */
	p->vclk = 1000000/var->pixclock;

	if (p->bpp == 32)
		p->vclk *=2;

	p->hres = var->xres;
	vres = p->vres = var->yres;

	/* See if requested resolution is larger than flat panel */
	if (p->hres > i->nativex && flatpanel) {
		return -EINVAL;
	}

	/* See if requested resolution fits in available memory */
	if (p->hres * p->vres * p->bpp/8 > i->memsize) {
		return -EINVAL;
	}

	vfront = var->upper_margin;
	vback =	var->lower_margin;
	vsync =	var->vsync_len;

	/* Compute horizontal and vertical VGA CRTC timing values */
	if (var->vmode & FB_VMODE_INTERLACED) {
		vres /= 2;
		vfront /=2;
		vback /=2;
		vsync /=2;
	}

	if (var->vmode & FB_VMODE_DOUBLE) {
		vres *= 2;
		vfront *=2;
		vback *=2;
		vsync *=2;
	}

	p->htotal = (p->hres + var->left_margin + var->right_margin + var->hsync_len)/8 - 10;
	p->hdispend = p->hres/8 - 1;
	p->hsyncstart = (p->hres + var->right_margin)/8;
	p->hsyncend = var->hsync_len/8;
	p->hblankstart = p->hdispend + 1;
	p->hblankend = p->htotal + 5;

	p->vtotal = vres + vfront + vback + vsync - 2;
	p->vdispend = vres - 1;
	p->vsyncstart = vres + vback;
	p->vsyncend = vsync;
	p->vblankstart = vres;
	p->vblankend = p->vtotal + 2;

	debug("exit\n");

	return 0;
}


/* Fill in var from info */
static int trident_encode_var(struct fb_var_screeninfo *var,
				  const void *par,
				  struct fb_info_gen *info)
{
	struct tridentfb_par * p = (struct tridentfb_par *)par;
	debug("enter\n");
	*var = p->var;
	var->bits_per_pixel = p->bpp;
	debug("exit\n");
	return 0;
}

/* Fill in par from hardware */
static void trident_get_par(void *par, struct fb_info_gen *info)
{
	struct tridentfb_par * p = (struct tridentfb_par *)par;
	struct tridentfb_info * i = (struct tridentfb_info *)info;

	debug("enter\n");
	*p = i->currentmode;
	debug("exit\n");
}

/* Pan the display */
static int trident_pan_display(const struct fb_var_screeninfo *var,
				   struct fb_info_gen *info)
{
	unsigned int offset;
	struct tridentfb_info * i = (struct tridentfb_info *)info;

	debug("enter\n");

	offset = (var->xoffset + (var->yoffset * var->xres))
			* var->bits_per_pixel/32;
	i->currentmode.var.xoffset = var->xoffset;
	i->currentmode.var.yoffset = var->yoffset;
	set_screen_start(offset);
	debug("exit\n");
	return 0;
}

/* Set the hardware from par */
static void trident_set_par(const void *par, struct fb_info_gen *info)
{
	struct tridentfb_par * p = (struct tridentfb_par *)par;
	struct tridentfb_info * i = (struct tridentfb_info *)info;
	unsigned char tmp;
	debug("enter\n");

	i->currentmode = *p;
	enable_mmio();
	crtc_unlock();

	write3CE(CyberControl,8);
	if (flatpanel && p->hres < i->nativex) {
		/*
		 * on flat panels with native size larger
		 * than requested resolution decide whether
		 * we stretch or center
		 */
		t_outb(0xEB,0x3C2);
		write3CE(CyberControl,0x81);
		if (center) 
			screen_center();
		else if (stretch)
			screen_stretch();

	} else {
		t_outb(0x2B,0x3C2);
		write3CE(CyberControl,8);
	}

	/* vertical timing values */
	write3X4(CRTVTotal, p->vtotal & 0xFF);
	write3X4(CRTVDispEnd, p->vdispend & 0xFF);
	write3X4(CRTVSyncStart, p->vsyncstart & 0xFF);
	write3X4(CRTVSyncEnd, (p->vsyncend & 0x0F));
	write3X4(CRTVBlankStart, p->vblankstart & 0xFF);
	write3X4(CRTVBlankEnd, 0/*p->vblankend & 0xFF*/);

	/* horizontal timing values */
	write3X4(CRTHTotal, p->htotal & 0xFF);
	write3X4(CRTHDispEnd, p->hdispend & 0xFF);
	write3X4(CRTHSyncStart, p->hsyncstart & 0xFF);
	write3X4(CRTHSyncEnd, (p->hsyncend & 0x1F) | ((p->hblankend & 0x20)<<2));
	write3X4(CRTHBlankStart, p->hblankstart & 0xFF);
	write3X4(CRTHBlankEnd, 0/*(p->hblankend & 0x1F)*/);

	/* higher bits of vertical timing values */
	tmp = 0x10;
	if (p->vtotal & 0x100) tmp |= 0x01;
	if (p->vdispend & 0x100) tmp |= 0x02;
	if (p->vsyncstart & 0x100) tmp |= 0x04;
	if (p->vblankstart & 0x100) tmp |= 0x08;

	if (p->vtotal & 0x200) tmp |= 0x20;
	if (p->vdispend & 0x200) tmp |= 0x40;
	if (p->vsyncstart & 0x200) tmp |= 0x80;

	write3X4(CRTOverflow, tmp);


	tmp = read3X4(CRTHiOrd) | 0x08;	//line compare bit 10

	if (p->vtotal & 0x400) tmp |= 0x80;
	if (p->vblankstart & 0x400) tmp |= 0x40;
	if (p->vsyncstart & 0x400) tmp |= 0x20;
	if (p->vdispend & 0x400) tmp |= 0x10;
	write3X4(CRTHiOrd, tmp);

	write3X4(HorizOverflow, 0);

	tmp = 0x40;
	if (p->vblankstart & 0x200) tmp |= 0x20;
	if (p->var.vmode & FB_VMODE_DOUBLE) tmp |= 0x80;  //double scan for 200 line modes
	write3X4(CRTMaxScanLine, tmp);
	write3X4(CRTLineCompare,0xFF);
	write3X4(CRTPRowScan,0);
	write3X4(CRTModeControl,0xC3);
	write3X4(LinearAddReg,0x20);	//enable linear addressing
	tmp = (p->var.vmode & FB_VMODE_INTERLACED) ? 0x84:0x80;
	write3X4(CRTCModuleTest,tmp);	//enable access extended memory
	write3X4(GraphEngReg, 0x80);	//enable GE for text acceleration

	if (p->var.accel_flags & FB_ACCELF_TEXT)
		acc->init_accel(p->hres,p->bpp);

	switch (p->bpp) {
		case 8:tmp=0;break;
		case 16:tmp=5;break;
		case 24:
			/* tmp=0x29;break; */
			/* seems like 24bpp is same as 32bpp when using vesafb */
		case 32:tmp=9;break;
	}
	write3X4(PixelBusReg, tmp);

	write3X4(InterfaceSel, 0x5B);	//32bit internal data path
	write3X4(DRAMControl, 0x30);	//both IO,linear enable
	write3X4(Performance, 0xBF);
	write3X4(PCIReg,0x07);		//MMIO & PCI read and write burst enable

	set_vclk(p->vclk);

	write3C4(0,3);
	write3C4(1,1);		//set char clock 8 dots wide
	write3C4(2,0x0F);	//enable 4 maps because needed in chain4 mode
	write3C4(3,0);
	write3C4(4,0x0E);	//memory mode enable bitmaps ??

	write3CE(MiscExtFunc,(p->bpp==32)?0x1A:0x12);	//divide clock by 2 if 32bpp
							//chain4 mode display and CPU path
	write3CE(0x5,0x40);	//no CGA compat,allow 256 col
	write3CE(0x6,0x05);	//graphics mode
	write3CE(0x7,0x0F);	//planes?

	writeAttr(0x10,0x41);	//graphics mode and support 256 color modes
	writeAttr(0x12,0x0F);	//planes
	writeAttr(0x13,0);	//horizontal pel panning

	//colors
	for(tmp = 0;tmp < 0x10;tmp++)
		writeAttr(tmp,tmp);
	readb(fb_info.io_virt + CRT + 0x0A);	//flip-flop to index
	t_outb(0x20, 0x3C0);			//enable attr

	switch (p->bpp) {
		case 8:	tmp = 0;break;		//256 colors
		case 15: tmp = 0x10;break;
		case 16: tmp = 0x30;break;	//hicolor
		case 24: 			//truecolor
		case 32: tmp = 0xD0;break;
	}

	t_inb(0x3C8);
	t_inb(0x3C6);
	t_inb(0x3C6);
	t_inb(0x3C6);
	t_inb(0x3C6);
	t_outb(tmp,0x3C6);
	t_inb(0x3C8);

	if (flatpanel)
		set_number_of_lines(p->vres);
	set_lwidth(p->hres*p->bpp/(4*16));

	trident_pan_display(&p->var,info);
	debug("exit\n");
}

/* Get value of one color register */
static int trident_getcolreg(unsigned regno, unsigned *red,
				 unsigned *green, unsigned *blue,
				 unsigned *transp, struct fb_info *info)
{
	struct tridentfb_info * i = (struct tridentfb_info *)info;
	int m = i->currentmode.bpp==8?256:16;

	debug("enter %d\n",regno);

	if (regno >= m)
		return 1;

	*red = palette[regno].red;
	*green = palette[regno].green;
	*blue = palette[regno].blue;
	*transp = palette[regno].transp;

	debug("exit\n");
	return 0;
}

/* Set one color register */
static int trident_setcolreg(unsigned regno, unsigned red, unsigned green,
				 unsigned blue, unsigned transp,
				 struct fb_info *info)
{
	struct tridentfb_info * i = (struct tridentfb_info *)info;
	int bpp = i->currentmode.bpp;
	int m = bpp==8?256:16;

	debug("enter %d\n",regno);

	if (regno >= m)
		return 1;

	palette[regno].red = red;
	palette[regno].green = green;
	palette[regno].blue = blue;
	palette[regno].transp = transp;

	if (bpp==8) {
		t_outb(0xFF,0x3C6);
		t_outb(regno,0x3C8);

		t_outb(red>>10,0x3C9);
		t_outb(green>>10,0x3C9);
		t_outb(blue>>10,0x3C9);

	} else
	if (bpp == 16) 			/* RGB 565 */
			((u16*)info->pseudo_palette)[regno] = (red & 0xF800) |
			((green & 0xFC00) >> 5) | ((blue & 0xF800) >> 11);
	else
	if (bpp == 32)		/* ARGB 8888 */
		((u32*)info->pseudo_palette)[regno] =
			((transp & 0xFF00) <<16) 	|
			((red & 0xFF00) << 8) 		|
			((green & 0xFF00))		|
			((blue & 0xFF00)>>8);

	debug("exit\n");
	return 0;
}

/* Try blanking the screen.For flat panels it does nothing */
static int trident_blank(int blank_mode, struct fb_info_gen *info)
{
	unsigned char PMCont,DPMSCont;

	debug("enter\n");
	if (flatpanel)
		return 0;
	t_outb(0x04,0x83C8); /* Read DPMS Control */
	PMCont = t_inb(0x83C6) & 0xFC;
	DPMSCont = read3CE(PowerStatus) & 0xFC;
	switch (blank_mode)
	{
	case VESA_NO_BLANKING:
		/* Screen: On, HSync: On, VSync: On */
		PMCont |= 0x03;
		DPMSCont |= 0x00;
		break;
	case VESA_HSYNC_SUSPEND:
		/* Screen: Off, HSync: Off, VSync: On */
		PMCont |= 0x02;
		DPMSCont |= 0x01;
		break;
	case VESA_VSYNC_SUSPEND:
		/* Screen: Off, HSync: On, VSync: Off */
		PMCont |= 0x02;
		DPMSCont |= 0x02;
		break;
	case VESA_POWERDOWN:
		/* Screen: Off, HSync: Off, VSync: Off */
		PMCont |= 0x00;
		DPMSCont |= 0x03;
		break;
    	}

	write3CE(PowerStatus,DPMSCont);
	t_outb(4,0x83C8);
	t_outb(PMCont,0x83C6);

	debug("exit\n");
	return 0;
}

/* Set display switch used by console */
static void trident_set_disp(const void *par, struct display *disp,
				 struct fb_info_gen *info)
{
	struct tridentfb_info * i = (struct tridentfb_info *)info;
	struct fb_info * ii = (struct fb_info *)info;
	struct tridentfb_par * p = (struct tridentfb_par *)par;
	int isaccel = p->var.accel_flags & FB_ACCELF_TEXT;

	disp->screen_base = (char *)i->fbmem_virt;
	debug("enter\n");
#ifdef FBCON_HAS_CFB8
	if (p->bpp == 8 ) {
		if (isaccel)
			disp->dispsw = &trident_8bpp;
		else
			disp->dispsw = &fbcon_cfb8;
	} else
#endif
#ifdef FBCON_HAS_CFB16
	if (p->bpp == 16) {
		if (isaccel)
			disp->dispsw = &trident_16bpp;
		else
			disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data =ii->pseudo_palette;	/* console palette */
	} else
#endif
#ifdef FBCON_HAS_CFB32
	if (p->bpp == 32) {
		if (isaccel)
			disp->dispsw = &trident_32bpp;
		else
			disp->dispsw = &fbcon_cfb32;
		disp->dispsw_data =ii->pseudo_palette;	/* console palette */
	} else
#endif
	disp->dispsw = &fbcon_dummy;
	debug("exit\n");
}

static struct fbgen_hwswitch trident_hwswitch = {
	NULL, /* detect not needed */
	trident_encode_fix,
	trident_decode_var,
	trident_encode_var,
	trident_get_par,
	trident_set_par,
	trident_getcolreg,
	trident_setcolreg,
	trident_pan_display,
	trident_blank,
	trident_set_disp
};

static int trident_iosize;

static int __devinit trident_pci_probe(struct pci_dev * dev, const struct pci_device_id * id)
{
	int err;
	unsigned char revision;

	err = pci_enable_device(dev);
	if (err)
		return err;

	chip_id = id->device;

	/* If PCI id is 0x9660 then further detect chip type */
	
	if (chip_id == TGUI9660) {
		outb(RevisionID,0x3C4);
		revision = inb(0x3C5);	
	
		switch (revision) {
			case 0x22:
			case 0x23: chip_id = CYBER9397;break;
			case 0x2A: chip_id = CYBER9397DVD;break;
			case 0x30:
			case 0x33:
			case 0x34:
			case 0x35:
			case 0x38:
			case 0x3A:
			case 0xB3: chip_id = CYBER9385;break;
			case 0x40 ... 0x43: chip_id = CYBER9382;break;
			case 0x4A: chip_id = CYBER9388;break;
			default:break;	
		}
	}

	chip3D = is3Dchip(chip_id);

	if (is_xp(chip_id)) {
		acc = &accel_xp;
	} else 
	if (is_blade(chip_id)) {
		acc = &accel_blade;
	} else {
		acc = &accel_image;
	}

	/* acceleration is on by default for 3D chips */
	defaultaccel = chip3D && !noaccel;

	fb_info.io = pci_resource_start(dev,1);
	fb_info.fbmem = pci_resource_start(dev,0);

	trident_iosize = chip3D ? 0x20000:0x10000;

	if (!request_mem_region(fb_info.io, trident_iosize, "tridentfb")) {
		debug("request_region failed!\n");
		return -1;
	}

	fb_info.io_virt = (unsigned long)ioremap_nocache(fb_info.io, trident_iosize);

	if (!fb_info.io_virt) {
		release_region(fb_info.io, trident_iosize);
		debug("ioremap failed\n");
		return -1;
	}

	enable_mmio();

	fb_info.memsize = get_memsize();
	if (!request_mem_region(fb_info.fbmem, fb_info.memsize, "tridentfb")) {
		debug("request_mem_region failed!\n");
		return -1;
	}

	fb_info.fbmem_virt = (unsigned long)ioremap_nocache(fb_info.fbmem, fb_info.memsize);

	if (!fb_info.fbmem_virt) {
		release_mem_region(fb_info.fbmem, fb_info.memsize);
		debug("ioremap failed\n");
		return -1;
	}

	output("%s board found\n", dev->name);
	debug("Trident board found : mem = %X,io = %X, mem_v = %X, io_v = %X\n",
		fb_info.fbmem, fb_info.io, fb_info.fbmem_virt, fb_info.io_virt);

	fb_info.gen.parsize = sizeof (struct tridentfb_par);
	fb_info.gen.fbhw = &trident_hwswitch;

	strcpy(fb_info.gen.info.modename, tridentfb_name);
	displaytype = get_displaytype();

	if(flatpanel)
		fb_info.nativex = get_nativex();

	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = NODEV;
	fb_info.gen.info.fbops = &tridentfb_ops;
	fb_info.gen.info.disp = &disp;

	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;

	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.gen.info.fontname[0] = '\0';
	fb_info.gen.info.pseudo_palette = pseudo_pal;

	/* This should give a reasonable default video mode */
	fb_find_mode(&default_var,&fb_info.gen.info,mode,NULL,0,NULL,bpp);

	if (defaultaccel && acc)
		default_var.accel_flags |= FB_ACCELF_TEXT;
	else
		default_var.accel_flags &= ~FB_ACCELF_TEXT;

	trident_decode_var(&default_var, &fb_info.currentmode, &fb_info.gen);
	fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
	default_var.activate |= FB_ACTIVATE_NOW;
	fbgen_set_disp(-1, &fb_info.gen);

	if (register_framebuffer(&fb_info.gen.info) < 0) {
		printk("Could not register Trident framebuffer\n");
		return -EINVAL;
	}

	output("fb%d: %s frame buffer device %dx%d-%dbpp\n",
	   GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename,default_var.xres,
	   default_var.yres,default_var.bits_per_pixel);
	return 0;
}

static void __devexit trident_pci_remove(struct pci_dev * dev)
{
	unregister_framebuffer(&fb_info.gen.info);
	iounmap((void *)fb_info.io_virt);
	iounmap((void *)fb_info.fbmem_virt);
	release_mem_region(fb_info.fbmem, fb_info.memsize);
	release_region(fb_info.io, trident_iosize);
}

/* List of boards that we are trying to support */
static struct pci_device_id trident_devices[] __devinitdata = {
	{PCI_VENDOR_ID_TRIDENT,	BLADE3D, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEi7, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEi7D, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEi1, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEi1D, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEAi1, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEAi1D, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEE4, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	TGUI9660, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	IMAGE975, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	IMAGE985, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBER9320, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBER9388, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBER9520, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBER9525DVD, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBER9397, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBER9397DVD, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEXPAi1, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEXPm8, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{PCI_VENDOR_ID_TRIDENT,	CYBERBLADEXPm16, PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{0,}
};	
	
MODULE_DEVICE_TABLE(pci,trident_devices); 

static struct pci_driver tridentfb_pci_driver = {
	name:"tridentfb",
	id_table:trident_devices,
	probe:trident_pci_probe,
	remove:__devexit_p(trident_pci_remove),
};

int __init tridentfb_init(void)
{
	output("Trident framebuffer %s initializing\n", VERSION);
	return pci_module_init(&tridentfb_pci_driver);
}

void __exit tridentfb_exit(void)
{
	pci_unregister_driver(&tridentfb_pci_driver);
}


/*
 * Parse user specified options (`video=trident:')
 * example:
 * 	video=trident:800x600,bpp=16,noaccel
 */
int tridentfb_setup(char *options)
{
	char * opt;
	if (!options || !*options)
		return 0;
	while((opt = strsep(&options,",")) != NULL ) {
		if (!*opt) continue;
		if (!strncmp(opt,"noaccel",7))
			noaccel = 1;
		else if (!strncmp(opt,"fp",2))
			displaytype = DISPLAY_FP;
		else if (!strncmp(opt,"crt",3))
			displaytype = DISPLAY_CRT;
		else if (!strncmp(opt,"bpp=",4))
			bpp = simple_strtoul(opt+4,NULL,0);
		else if (!strncmp(opt,"center",6))
			center = 1;
		else if (!strncmp(opt,"stretch",7))
			stretch = 1;
		else if (!strncmp(opt,"memsize=",8))
			memsize = simple_strtoul(opt+8,NULL,0);
		else if (!strncmp(opt,"memdiff=",8))
			memdiff = simple_strtoul(opt+8,NULL,0);
		else if (!strncmp(opt,"nativex=",8))
			nativex = simple_strtoul(opt+8,NULL,0);
		else
			mode = opt;
	}
	return 0;
}

static struct fb_ops tridentfb_ops = {
	fb_get_fix:fbgen_get_fix,
	fb_get_var:fbgen_get_var,
	fb_set_var:fbgen_set_var,
	fb_get_cmap:fbgen_get_cmap,
	fb_set_cmap:fbgen_set_cmap,
	fb_pan_display:fbgen_pan_display,
};

#ifdef MODULE
module_init(tridentfb_init);
#endif
module_exit(tridentfb_exit);

MODULE_AUTHOR("Jani Monoses <jani@iv.ro>");
MODULE_DESCRIPTION("Framebuffer driver for Trident cards");
MODULE_LICENSE("GPL");

