#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/amigahw.h>
#include <asm/amigaints.h>
#include <asm/apollohw.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include "dn_accel.h"

/* apollo video HW definitions */

/*
 * Control Registers.   IOBASE + $x
 *
 * Note: these are the Memory/IO BASE definitions for a mono card set to the
 * alternate address
 *
 * Control 3A and 3B serve identical functions except that 3A
 * deals with control 1 and 3b deals with Color LUT reg.
 */

#define AP_IOBASE       0x3d0          /* Base address of 1 plane board. */
#define AP_STATUS       isaIO2mem(AP_IOBASE+0) /* Status register.  Read */
#define AP_WRITE_ENABLE isaIO2mem(AP_IOBASE+0) /* Write Enable Register Write */
#define AP_DEVICE_ID    isaIO2mem(AP_IOBASE+1) /* Device ID Register. Read */
#define AP_ROP_1        isaIO2mem(AP_IOBASE+2) /* Raster Operation reg. Write Word */
#define AP_DIAG_MEM_REQ isaIO2mem(AP_IOBASE+4) /* Diagnostic Memory Request. Write Word */
#define AP_CONTROL_0    isaIO2mem(AP_IOBASE+8) /* Control Register 0.  Read/Write */
#define AP_CONTROL_1    isaIO2mem(AP_IOBASE+0xa) /* Control Register 1.  Read/Write */
#define AP_CONTROL_2    isaIO2mem(AP_IOBASE+0xc) /* Control Register 2. Read/Write */
#define AP_CONTROL_3A   isaIO2mem(AP_IOBASE+0xe) /* Control Register 3a. Read/Write */
#define AP_LUT_RED     isaIO2mem(AP_IOBASE+9) /* Red Lookup Table register. Write */
#define AP_LUT_GREEN  isaIO2mem(AP_IOBASE+0xb) /* Green Lookup Table register. Write */
#define AP_LUT_BLUE   isaIO2mem(AP_IOBASE+0xd) /* Blue Lookup Table register. Write */
#define AP_AD_CHANNEL   isaIO2mem(AP_IOBASE+0xf) /* A/D Result/Channel register. Read/Write */


#define FRAME_BUFFER_START 0x0A0000
#define FRAME_BUFFER_LEN 0x20000

/* CREG 0 */
#define VECTOR_MODE 0x40 /* 010x.xxxx */
#define DBLT_MODE   0x80 /* 100x.xxxx */
#define NORMAL_MODE 0xE0 /* 111x.xxxx */
#define SHIFT_BITS  0x1F /* xxx1.1111 */
        /* other bits are Shift value */

/* CREG 1 */
#define AD_BLT      0x80 /* 1xxx.xxxx */

#define ROP_EN          0x10 /* xxx1.xxxx */
#define DST_EQ_SRC      0x00 /* xxx0.xxxx */
#define nRESET_SYNC     0x08 /* xxxx.1xxx */
#define SYNC_ENAB       0x02 /* xxxx.xx1x */

#define BLANK_DISP      0x00 /* xxxx.xxx0 */
#define ENAB_DISP       0x01 /* xxxx.xxx1 */

#define NORM_CREG1      (nRESET_SYNC | SYNC_ENAB | ENAB_DISP) /* no reset sync */

/* CREG 2B */

/*
 * Following 3 defines are common to 1, 4 and 8 plane.
 */

#define S_DATA_1s   0x00 /* 00xx.xxxx */ /* set source to all 1's -- vector drawing */
#define S_DATA_PIX  0x40 /* 01xx.xxxx */ /* takes source from ls-bits and replicates over 16 bits */
#define S_DATA_PLN  0xC0 /* 11xx.xxxx */ /* normal, each data access =16-bits in
 one plane of image mem */

/* CREG 3A/CREG 3B */
#       define RESET_CREG 0x80 /* 1000.0000 */

/* ROP REG  -  all one nibble */
/*      ********* NOTE : this is used r0,r1,r2,r3 *********** */
#define ROP(r2,r3,r0,r1) ( (U_SHORT)((r0)|((r1)<<4)|((r2)<<8)|((r3)<<12)) )
#define DEST_ZERO               0x0
#define SRC_AND_DEST    0x1
#define SRC_AND_nDEST   0x2
#define SRC                             0x3
#define nSRC_AND_DEST   0x4
#define DEST                    0x5
#define SRC_XOR_DEST    0x6
#define SRC_OR_DEST             0x7
#define SRC_NOR_DEST    0x8
#define SRC_XNOR_DEST   0x9
#define nDEST                   0xA
#define SRC_OR_nDEST    0xB
#define nSRC                    0xC
#define nSRC_OR_DEST    0xD
#define SRC_NAND_DEST   0xE
#define DEST_ONE                0xF

#define SWAP(A) ((A>>8) | ((A&0xff) <<8))


/* frame buffer operations */

static int dn_fb_get_fix(struct fb_fix_screeninfo *fix, int con, 
			 struct fb_info *info);
static int dn_fb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info);
static int dn_fb_set_var(struct fb_var_screeninfo *var, int isactive,
			 struct fb_info *info);
static int dn_fb_get_cmap(struct fb_cmap *cmap,int kspc,int con,
			  struct fb_info *info);
static int dn_fb_set_cmap(struct fb_cmap *cmap,int kspc,int con,
			  struct fb_info *info);

static int dnfbcon_switch(int con,struct fb_info *info);
static int dnfbcon_updatevar(int con,struct fb_info *info);
static void dnfbcon_blank(int blank,struct fb_info *info);

static void dn_fb_set_disp(int con,struct fb_info *info);

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;
static struct fb_ops dn_fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	dn_fb_get_fix,
	fb_get_var:	dn_fb_get_var,
	fb_set_var:	dn_fb_set_var,
	fb_get_cmap:	dn_fb_get_cmap,
	fb_set_cmap:	dn_fb_set_cmap,
};

static int currcon=0;

#define NUM_TOTAL_MODES 1
struct fb_var_screeninfo dn_fb_predefined[] = {

	{ 0, },

};

static char dn_fb_name[]="Apollo ";

/* accel stuff */
#define USE_DN_ACCEL

static struct display_switch dispsw_apollofb;

static int dn_fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info) {

	strcpy(fix->id,"Apollo Color4");
	fix->smem_start=(FRAME_BUFFER_START+IO_BASE);
	fix->smem_len=FRAME_BUFFER_LEN;
	fix->type=FB_TYPE_PACKED_PIXELS;
	fix->type_aux=0;
	fix->visual=FB_VISUAL_MONO10;
	fix->xpanstep=0;
	fix->ypanstep=0;
	fix->ywrapstep=0;
        fix->line_length=128;

	return 0;

}
        
static int dn_fb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info) {
		
	var->xres=1024;
	var->yres=800;
	var->xres_virtual=1024;
	var->yres_virtual=1024;
	var->xoffset=0;
	var->yoffset=0;
	var->bits_per_pixel=1;
	var->grayscale=0;
	var->nonstd=0;
	var->activate=0;
	var->height=-1;
	var->width=-1;
	var->pixclock=0;
	var->left_margin=0;
	var->right_margin=0;
	var->hsync_len=0;
	var->vsync_len=0;
	var->sync=0;
	var->vmode=FB_VMODE_NONINTERLACED;

	return 0;

}

static int dn_fb_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info) {

        printk("fb_set_var\n");
	if(var->xres!=1024) 
		return -EINVAL;
	if(var->yres!=800)
		return -EINVAL;
	if(var->xres_virtual!=1024)
		return -EINVAL;
	if(var->yres_virtual!=1024)
		return -EINVAL;
	if(var->xoffset!=0)
		return -EINVAL;
	if(var->yoffset!=0)
		return -EINVAL;
	if(var->bits_per_pixel!=1)
		return -EINVAL;
	if(var->grayscale!=0)
		return -EINVAL;
	if(var->nonstd!=0)
		return -EINVAL;
	if(var->activate!=0)
		return -EINVAL;
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

	return 0;

}

static int dn_fb_get_cmap(struct fb_cmap *cmap,int kspc,int con,
			  struct fb_info *info) {

	printk("get cmap not supported\n");

	return -EINVAL;
}

static int dn_fb_set_cmap(struct fb_cmap *cmap,int kspc,int con,
			  struct fb_info *info) {

	printk("set cmap not supported\n");

	return -EINVAL;

}

static void dn_fb_set_disp(int con, struct fb_info *info) {

  struct fb_fix_screeninfo fix;
  struct display *display;


  dn_fb_get_fix(&fix,con, info);

  if (con>=0)
	display=&fb_display[con];
  else
	display=&disp[0];

  if(con==-1) 
    con=0;

   display->screen_base = (u_char *)fix.smem_start;
printk("screenbase: %lx\n",fix.smem_start);
   display->visual = fix.visual;
   display->type = fix.type;
   display->type_aux = fix.type_aux;
   display->ypanstep = fix.ypanstep;
   display->ywrapstep = fix.ywrapstep;
   display->can_soft_blank = 1;
   display->inverse = 0;
   display->line_length = fix.line_length;
   display->scrollmode = SCROLL_YREDRAW;
#ifdef FBCON_HAS_MFB
   display->dispsw = &fbcon_mfb;
#else
   display->dispsw=&fbcon_dummy;
#endif

}
  
unsigned long dnfb_init(unsigned long mem_start) {

	int err;

printk("dn_fb_init\n");

	fb_info.changevar=NULL;
	strcpy(&fb_info.modename[0],dn_fb_name);
	fb_info.fontname[0]=0;
	fb_info.disp=disp;
	fb_info.switch_con=&dnfbcon_switch;
	fb_info.updatevar=&dnfbcon_updatevar;
	fb_info.blank=&dnfbcon_blank;	
	fb_info.node = -1;
	fb_info.fbops = &dn_fb_ops;
	fb_info.flags = FBINFO_FLAG_DEFAULT;	

        dn_fb_get_var(&disp[0].var,0, &fb_info);
	dn_fb_set_disp(-1, &fb_info);

printk("dn_fb_init: register\n");
	err=register_framebuffer(&fb_info);
	if(err < 0) {
		panic("unable to register apollo frame buffer\n");
	}
 
	/* now we have registered we can safely setup the hardware */

        outb(RESET_CREG,  AP_CONTROL_3A);
        outb(NORMAL_MODE, AP_CONTROL_0); 
        outb((AD_BLT | DST_EQ_SRC | NORM_CREG1),  AP_CONTROL_1);
        outb(S_DATA_PLN,  AP_CONTROL_2);
        outw(SWAP(0x3), AP_ROP_1);

        printk("apollo frame buffer alive and kicking !\n");


	return mem_start;

}	

	
static int dnfbcon_switch(int con,  struct fb_info *info) { 

	currcon=con;
	
	return 0;

}

static int dnfbcon_updatevar(int con,  struct fb_info *info) {

	return 0;

}

static void dnfbcon_blank(int blank,  struct fb_info *info) {

	if(blank)  {
        	outb(0x0,  AP_CONTROL_3A);
	}
	else {
	        outb(0x1,  AP_CONTROL_3A);
	}

	return ;

}

void dn_bitblt(struct display *p,int x_src,int y_src, int x_dest, int y_dest,
               int x_count, int y_count) {

	int incr,y_delta,pre_read=0,x_end,x_word_count;
	ushort *src,dummy;
	uint start_mask,end_mask,dest;
	short i,j;

	incr=(y_dest<=y_src) ? 1 : -1 ;

	src=(ushort *)(p->screen_base+ y_src*p->next_line+(x_src >> 4));
	dest=y_dest*(p->next_line >> 1)+(x_dest >> 4);
	
	if(incr>0) {
		y_delta=(p->next_line*8)-x_src-x_count;
		x_end=x_dest+x_count-1;
		x_word_count=(x_end>>4) - (x_dest >> 4) + 1;
		start_mask=0xffff0000 >> (x_dest & 0xf);
		end_mask=0x7ffff >> (x_end & 0xf);
		outb((((x_dest & 0xf) - (x_src &0xf))  % 16)|(0x4 << 5),AP_CONTROL_0);
		if((x_dest & 0xf) < (x_src & 0xf))
			pre_read=1;
	}
	else {
		y_delta=-((p->next_line*8)-x_src-x_count);
		x_end=x_dest-x_count+1;
		x_word_count=(x_dest>>4) - (x_end >> 4) + 1;
		start_mask=0x7ffff >> (x_dest & 0xf);
		end_mask=0xffff0000 >> (x_end & 0xf);
		outb(((-((x_src & 0xf) - (x_dest &0xf))) % 16)|(0x4 << 5),AP_CONTROL_0);
		if((x_dest & 0xf) > (x_src & 0xf))
			pre_read=1;
	}

	for(i=0;i<y_count;i++) {
			
		if(pre_read) {
			dummy=*src;
			src+=incr;
		}

		if(x_word_count) {
			outb(start_mask,AP_WRITE_ENABLE);
			*src=dest;
			src+=incr;
			dest+=incr;
			outb(0,AP_WRITE_ENABLE);

			for(j=1;j<(x_word_count-1);j++) {
				*src=dest;
				src+=incr;	
				dest+=incr;
			}

			outb(start_mask,AP_WRITE_ENABLE);
			*src=dest;
			dest+=incr;
			src+=incr;
		}
		else {
			outb(start_mask | end_mask, AP_WRITE_ENABLE);
			*src=dest;
			dest+=incr;
			src+=incr;
		}
		src+=(y_delta/16);
		dest+=(y_delta/16);
	}
	outb(NORMAL_MODE,AP_CONTROL_0);
}

static void bmove_apollofb(struct display *p, int sy, int sx, int dy, int dx,
		      int height, int width)
{

   int fontheight,fontwidth;

    fontheight=fontheight(p);
    fontwidth=fontwidth(p);

#ifdef USE_DN_ACCEL
    dn_bitblt(p,sx,sy*fontheight,dx,dy*fontheight,width*fontwidth,
	      height*fontheight);
#else
    u_char *src, *dest;
    u_int rows;

    if (sx == 0 && dx == 0 && width == p->next_line) {
	src = p->screen_base+sy*fontheight*width;
	dest = p->screen_base+dy*fontheight*width;
	mymemmove(dest, src, height*fontheight*width);
    } else if (dy <= sy) {
	src = p->screen_base+sy*fontheight*p->next_line+sx;
	dest = p->screen_base+dy*fontheight*p->next_line+dx;
	for (rows = height*fontheight; rows--;) {
	    mymemmove(dest, src, width);
	    src += p->next_line;
	    dest += p->next_line;
	}
    } else {
	src = p->screen_base+((sy+height)*fontheight-1)*p->next_line+sx;
	dest = p->screen_base+((dy+height)*fontheight-1)*p->next_line+dx;
	for (rows = height*fontheight; rows--;) {
	    mymemmove(dest, src, width);
	    src -= p->next_line;
	    dest -= p->next_line;
	}
    }
#endif
}

static void clear_apollofb(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{
	fbcon_mfb_clear(conp,p,sy,sx,height,width);
}

static void putc_apollofb(struct vc_data *conp, struct display *p, int c, int yy,
		     int xx)
{
	fbcon_mfb_putc(conp,p,c,yy,xx);
}

static void putcs_apollofb(struct vc_data *conp, struct display *p, const unsigned short *s,
		      int count, int yy, int xx)
{
	fbcon_mfb_putcs(conp,p,s,count,yy,xx);
}

static void rev_char_apollofb(struct display *p, int xx, int yy)
{
	fbcon_mfb_revc(p,xx,yy);
}

static struct display_switch dispsw_apollofb = {
    setup:		fbcon_mfb_setup,
    bmove:		bmove_apollofb,
    clear:		clear_apollofb,
    putc:		putc_apollofb,
    putcs:		putcs_apollofb,
    revc:		rev_char_apollofb,
    fontwidthmask:	FONTWIDTH(8)
};

MODULE_LICENSE("GPL");
