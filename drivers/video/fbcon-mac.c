/*
 *  linux/drivers/video/fbcon-mac.c -- Low level frame buffer operations for 
 *				       x bpp packed pixels, font width != 8
 *
 *	Created 26 Dec 1997 by Michael Schmitz
 *	Based on the old macfb.c 6x11 code by Randy Thelen
 *
 *	This driver is significantly slower than the 8bit font drivers 
 *	and would probably benefit from splitting into drivers for each depth.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <video/fbcon.h>
#include <video/fbcon-mac.h>


    /*
     *  variable bpp packed pixels
     */

static void plot_pixel_mac(struct display *p, int bw, int pixel_x,
			   int pixel_y);
static int get_pixel_mac(struct display *p, int pixel_x, int pixel_y);

void fbcon_mac_setup(struct display *p)
{
    if (p->line_length)
	p->next_line = p->line_length;
    else
    	p->next_line = p->var.xres_virtual>>3;
    p->next_plane = 0;
}


   /*
    *    Macintosh
    */
#define PIXEL_BLACK_MAC          0
#define PIXEL_WHITE_MAC          1
#define PIXEL_INVERT_MAC         2

void fbcon_mac_bmove(struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
   int i, j;
   u8 *dest, *src;
   int l,r,t,b,w,lo,s;
   int dl,dr,dt,db,dw,dlo;
   int move_up;

   src = (u8 *) (p->screen_base + sy * fontheight(p) * p->next_line);
   dest = (u8 *) (p->screen_base + dy * fontheight(p) * p->next_line);

   if( sx == 0 && width == p->conp->vc_cols) {
     s = height * fontheight(p) * p->next_line;
     fb_memmove(dest, src, s);
     return;
   }
   
   l = sx * fontwidth(p);
   r = l + width * fontwidth(p);
   t = sy * fontheight(p);
   b = t + height * fontheight(p);

   dl = dx * fontwidth(p);
   dr = dl + width * fontwidth(p);
   dt = dy * fontheight(p);
   db = dt + height * fontheight(p);

   /* w is the # pixels between two long-aligned points, left and right */
   w = (r&~31) - ((l+31)&~31);
   dw = (dr&~31) - ((dl+31)&~31);
   /* lo is the # pixels between the left edge and a long-aligned left pixel */
   lo = ((l+31)&~31) - l;
   dlo = ((dl+31)&~31) - dl;
   
   /* if dx != sx then, logic has to align the left and right edges for fast moves */
   if (lo != dlo) {
     lo = ((l+7)&~7) - l;
     dlo = ((dl+7)&~7) - dl;
     w = (r&~7) - ((l+7)&~7);
     dw = (dr&~7) - ((dl+7)&~7);
     if (lo != dlo) {
       unsigned char err_str[128];
       unsigned short err_buf[256];
       unsigned long cnt, len;
       sprintf( err_str, "ERROR: Shift algorithm: sx=%d,sy=%d,dx=%d,dy=%d,w=%d,h=%d,bpp=%d",
		sx,sy,dx,dy,width,height,p->var.bits_per_pixel);
       len = strlen(err_str);
       for (cnt = 0; cnt < len; cnt++)
         err_buf[cnt] = 0x700 | err_str[cnt];
       fbcon_mac_putcs(p->conp, p, err_buf, len, 0, 0);
       /* pause for the user */
       printk( "ERROR: shift algorithm...\n" );
       mdelay(5000);
       return;
     }
   }

   s = 0;
   switch (p->var.bits_per_pixel) {
   case 1:
     s = w >> 3;
     src += lo >> 3;
     dest += lo >> 3;
     break;
   case 2:
     s = w >> 2;
     src += lo >> 2;
     dest += lo >> 2;
     break;
   case 4:
     s = w >> 1;
     src += lo >> 1;
     dest += lo >> 1;
     break;
   case 8:
     s = w;
     src += lo;
     dest += lo;
     break;
   case 16:
     s = w << 1;
     src += lo << 1;
     dest += lo << 1;
     break;
   case 32:
     s = w << 2;
     src += lo << 2;
     dest += lo << 2;
     break;
   }

   if (sy <= sx) {
     i = b;
     move_up = 0;
     src += height * fontheight(p);
     dest += height * fontheight(p);
   } else {
     i = t;
     move_up = 1;
   }

   while (1) {
     for (i = t; i < b; i++) {
       j = l;

       for (; j & 31 && j < r; j++)
	 plot_pixel_mac(p, get_pixel_mac(p, j+(dx-sx), i+(dy-sy)), j, i);

       if (j < r) {
	 fb_memmove(dest, src, s);
	 if (move_up) {
	   dest += p->next_line;
	   src += p->next_line;
	 } else {
	   dest -= p->next_line;
	   src -= p->next_line;
	 }
	 j += w;
       }
     
       for (; j < r; j++)
	 plot_pixel_mac(p, get_pixel_mac(p, j+(dx-sx), i+(dy-sy)), j, i);
     }

     if (move_up) {
       i++;
       if (i >= b)
	 break;
     } else {
       i--;
       if (i < t)
	 break;
     }
   }
}


void fbcon_mac_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
   int pixel;
   int i, j;
   int inverse;
   u8 *dest;
   int l,r,t,b,w,lo,s;

   inverse = conp ? attr_reverse(p,conp->vc_attr) : 0;
   pixel = inverse ? PIXEL_WHITE_MAC : PIXEL_BLACK_MAC;
   dest = (u8 *) (p->screen_base + sy * fontheight(p) * p->next_line);

   if( sx == 0 && width == p->conp->vc_cols) {
     s = height * fontheight(p) * p->next_line;
     if (inverse)
       fb_memclear(dest, s);
     else
       fb_memset255(dest, s);
   }
   
   l = sx * fontwidth(p);
   r = l + width * fontwidth(p);
   t = sy * fontheight(p);
   b = t + height * fontheight(p);
   /* w is the # pixels between two long-aligned points, left and right */
   w = (r&~31) - ((l+31)&~31);
   /* lo is the # pixels between the left edge and a long-aligned left pixel */
   lo = ((l+31)&~31) - l;
   s = 0;
   switch (p->var.bits_per_pixel) {
   case 1:
     s = w >> 3;
     dest += lo >> 3;
     break;
   case 2:
     s = w >> 2;
     dest += lo >> 2;
     break;
   case 4:
     s = w >> 1;
     dest += lo >> 1;
     break;
   case 8:
     s = w;
     dest += lo;
     break;
   case 16:
     s = w << 1;
     dest += lo << 1;
     break;
   case 32:
     s = w << 2;
     dest += lo << 2;
     break;
   }

   for (i = t; i < b; i++) {
     j = l;

     for (; j & 31 && j < r; j++)
       plot_pixel_mac(p, pixel, j, i);

     if (j < r) {
       if (PIXEL_WHITE_MAC == pixel)
	 fb_memclear(dest, s);
       else
	 fb_memset255(dest, s);
       dest += p->next_line;
       j += w;
     }
     
     for (; j < r; j++)
       plot_pixel_mac(p, pixel, j, i);
   }
}


void fbcon_mac_putc(struct vc_data *conp, struct display *p, int c, int yy,
		    int xx)
{
   u8 *cdat;
   u_int rows, bold, ch_reverse, ch_underline;
   u8 d;
   int j;

   cdat = p->fontdata+(c&p->charmask)*fontheight(p);
   bold = attr_bold(p,c);
   ch_reverse = attr_reverse(p,c);
   ch_underline = attr_underline(p,c);

   for (rows = 0; rows < fontheight(p); rows++) {
      d = *cdat++;
      if (!conp->vc_can_do_color) {
	if (ch_underline && rows == (fontheight(p)-2))
	  d = 0xff;
	else if (bold)
	  d |= d>>1;
	if (ch_reverse)
	  d = ~d;
      }
      for (j = 0; j < fontwidth(p); j++) {
	plot_pixel_mac(p, (d & 0x80) >> 7, (xx*fontwidth(p)) + j, (yy*fontheight(p)) + rows);
	d <<= 1;
      }
   }
}


void fbcon_mac_putcs(struct vc_data *conp, struct display *p, 
		     const unsigned short *s, int count, int yy, int xx)
{
   u16 c;

   while (count--) {
      c = scr_readw(s++);
      fbcon_mac_putc(conp, p, c, yy, xx++);
   }
}


void fbcon_mac_revc(struct display *p, int xx, int yy)
{
   u_int rows, j;

   for (rows = 0; rows < fontheight(p); rows++) {
     for (j = 0; j < fontwidth(p); j++) {
       plot_pixel_mac (p, PIXEL_INVERT_MAC, (xx*fontwidth(p))+j, (yy*fontheight(p))+rows);
     }
   }
}

static inline void plot_helper(u8 *dest, u8 bit, int bw)
{
    switch (bw) {
    case PIXEL_BLACK_MAC:
      fb_writeb( fb_readb(dest) | bit, dest );
      break;
    case PIXEL_WHITE_MAC:
      fb_writeb( fb_readb(dest) & (~bit), dest );
      break;
    case PIXEL_INVERT_MAC:
      fb_writeb( fb_readb(dest) ^ bit, dest );
      break;
    default:
      printk( "ERROR: Unknown pixel value in plot_pixel_mac\n");
    }
}

/*
 * plot_pixel_mac
 */
static void plot_pixel_mac(struct display *p, int bw, int pixel_x, int pixel_y)
{
  u8 *dest, bit;
  u16 *dest16, pix16;
  u32 *dest32, pix32;

  /* There *are* 68k Macs that support more than 832x624, you know :-) */
  if (pixel_x < 0 || pixel_y < 0 || pixel_x >= p->var.xres || pixel_y >= p->var.yres) {
    printk ("ERROR: pixel_x == %d, pixel_y == %d", pixel_x, pixel_y);
    mdelay(1000);
    return;
  }

  switch (p->var.bits_per_pixel) {
  case 1:
    dest = (u8 *) ((pixel_x >> 3) + p->screen_base + pixel_y * p->next_line);
    bit = 0x80 >> (pixel_x & 7);
    plot_helper(dest, bit, bw);
    break;

  case 2:
    dest = (u8 *) ((pixel_x >> 2) + p->screen_base + pixel_y * p->next_line);
    bit = 0xC0 >> ((pixel_x & 3) << 1);
    plot_helper(dest, bit, bw);
    break;

  case 4:
    dest = (u8 *) ((pixel_x >> 1) + p->screen_base + pixel_y * p->next_line);
    bit = 0xF0 >> ((pixel_x & 1) << 2);
    plot_helper(dest, bit, bw);
    break;

  case 8:
    dest = (u8 *) (pixel_x + p->screen_base + pixel_y * p->next_line);
    bit = 0xFF;
    plot_helper(dest, bit, bw);
    break;

/* FIXME: You can't access framebuffer directly like this! */
  case 16:
    dest16 = (u16 *) ((pixel_x *2) + p->screen_base + pixel_y * p->next_line);
    pix16 = 0xFFFF;
    switch (bw) {
    case PIXEL_BLACK_MAC:  *dest16 = ~pix16; break;
    case PIXEL_WHITE_MAC:  *dest16 = pix16;  break;
    case PIXEL_INVERT_MAC: *dest16 ^= pix16; break;
    default: printk( "ERROR: Unknown pixel value in plot_pixel_mac\n");
    }
    break;

  case 32:
    dest32 = (u32 *) ((pixel_x *4) + p->screen_base + pixel_y * p->next_line);
    pix32 = 0xFFFFFFFF;
    switch (bw) {
    case PIXEL_BLACK_MAC:  *dest32 = ~pix32; break;
    case PIXEL_WHITE_MAC:  *dest32 = pix32;  break;
    case PIXEL_INVERT_MAC: *dest32 ^= pix32; break;
    default: printk( "ERROR: Unknown pixel value in plot_pixel_mac\n");
    }
    break;
  }
}

static int get_pixel_mac(struct display *p, int pixel_x, int pixel_y)
{
  u8 *dest, bit;
  u16 *dest16;
  u32 *dest32;
  u8 pixel=0;

  switch (p->var.bits_per_pixel) {
  case 1:
    dest = (u8 *) ((pixel_x / 8) + p->screen_base + pixel_y * p->next_line);
    bit = 0x80 >> (pixel_x & 7);
    pixel = *dest & bit;
    break;
  case 2:
    dest = (u8 *) ((pixel_x / 4) + p->screen_base + pixel_y * p->next_line);
    bit = 0xC0 >> (pixel_x & 3);
    pixel = *dest & bit;
    break;
  case 4:
    dest = (u8 *) ((pixel_x / 2) + p->screen_base + pixel_y * p->next_line);
    bit = 0xF0 >> (pixel_x & 1);
    pixel = *dest & bit;
    break;
  case 8:
    dest = (u8 *) (pixel_x + p->screen_base + pixel_y * p->next_line);
    pixel = *dest;
    break;
  case 16:
    dest16 = (u16 *) ((pixel_x *2) + p->screen_base + pixel_y * p->next_line);
    pixel = *dest16 ? 1 : 0;
    break;
  case 32:
    dest32 = (u32 *) ((pixel_x *4) + p->screen_base + pixel_y * p->next_line);
    pixel = *dest32 ? 1 : 0;
    break;
  }

  return pixel ? PIXEL_BLACK_MAC : PIXEL_WHITE_MAC;
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_mac = {
    setup:		fbcon_mac_setup,
    bmove:		fbcon_redraw_bmove,
    clear:		fbcon_redraw_clear,
    putc:		fbcon_mac_putc,
    putcs:		fbcon_mac_putcs,
    revc:		fbcon_mac_revc,
    fontwidthmask:	FONTWIDTHRANGE(1,8)
};


#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{}
#endif /* MODULE */


    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(fbcon_mac);
EXPORT_SYMBOL(fbcon_mac_setup);
EXPORT_SYMBOL(fbcon_mac_bmove);
EXPORT_SYMBOL(fbcon_mac_clear);
EXPORT_SYMBOL(fbcon_mac_putc);
EXPORT_SYMBOL(fbcon_mac_putcs);
EXPORT_SYMBOL(fbcon_mac_revc);
