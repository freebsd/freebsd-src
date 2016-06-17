/*
 *  linux/drivers/video/fbcon.h -- Low level frame buffer based console driver
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#ifndef _VIDEO_FBCON_H
#define _VIDEO_FBCON_H

#include <linux/config.h>
#include <linux/types.h>
#include <linux/console_struct.h>
#include <linux/vt_buffer.h>

#include <asm/io.h>


    /*                                  
     *  `switch' for the Low Level Operations
     */
 
struct display_switch {                                                
    void (*setup)(struct display *p);
    void (*bmove)(struct display *p, int sy, int sx, int dy, int dx,
		  int height, int width);
    /* for clear, conp may be NULL, which means use a blanking (black) color */
    void (*clear)(struct vc_data *conp, struct display *p, int sy, int sx,
		  int height, int width);
    void (*putc)(struct vc_data *conp, struct display *p, int c, int yy,
    		 int xx);
    void (*putcs)(struct vc_data *conp, struct display *p, const unsigned short *s,
		  int count, int yy, int xx);     
    void (*revc)(struct display *p, int xx, int yy);
    void (*cursor)(struct display *p, int mode, int xx, int yy);
    int  (*set_font)(struct display *p, int width, int height);
    void (*clear_margins)(struct vc_data *conp, struct display *p,
			  int bottom_only);
    unsigned int fontwidthmask;      /* 1 at (1 << (width - 1)) if width is supported */
}; 

extern struct display_switch fbcon_dummy;

   /*
    *    This is the interface between the low-level console driver and the
    *    low-level frame buffer device
    */

struct display {
    /* Filled in by the frame buffer device */

    struct fb_var_screeninfo var;   /* variable infos. yoffset and vmode */
                                    /* are updated by fbcon.c */
    struct fb_cmap cmap;            /* colormap */
    char *screen_base;              /* pointer to top of virtual screen */    
                                    /* (virtual address) */
    int visual;
    int type;                       /* see FB_TYPE_* */
    int type_aux;                   /* Interleave for interleaved Planes */
    u_short ypanstep;               /* zero if no hardware ypan */
    u_short ywrapstep;              /* zero if no hardware ywrap */
    u_long line_length;             /* length of a line in bytes */
    u_short can_soft_blank;         /* zero if no hardware blanking */
    u_short inverse;                /* != 0 text black on white as default */
    struct display_switch *dispsw;  /* low level operations */
    void *dispsw_data;              /* optional dispsw helper data */

#if 0
    struct fb_fix_cursorinfo fcrsr;
    struct fb_var_cursorinfo *vcrsr;
    struct fb_cursorstate crsrstate;
#endif

    /* Filled in by the low-level console driver */

    struct vc_data *conp;           /* pointer to console data */
    struct fb_info *fb_info;        /* frame buffer for this console */
    int vrows;                      /* number of virtual rows */
    unsigned short cursor_x;        /* current cursor position */
    unsigned short cursor_y;
    int fgcol;                      /* text colors */
    int bgcol;
    u_long next_line;               /* offset to one line below */
    u_long next_plane;              /* offset to next plane */
    u_char *fontdata;               /* Font associated to this display */
    unsigned short _fontheightlog;
    unsigned short _fontwidthlog;
    unsigned short _fontheight;
    unsigned short _fontwidth;
    int userfont;                   /* != 0 if fontdata kmalloc()ed */
    u_short scrollmode;             /* Scroll Method */
    short yscroll;                  /* Hardware scrolling */
    unsigned char fgshift, bgshift;
    unsigned short charmask;        /* 0xff or 0x1ff */
};

/* drivers/video/fbcon.c */
extern struct display fb_display[MAX_NR_CONSOLES];
extern char con2fb_map[MAX_NR_CONSOLES];
extern int PROC_CONSOLE(const struct fb_info *info);
extern void set_con2fb_map(int unit, int newidx);
extern int set_all_vcs(int fbidx, struct fb_ops *fb,
		       struct fb_var_screeninfo *var, struct fb_info *info);

#define fontheight(p) ((p)->_fontheight)
#define fontheightlog(p) ((p)->_fontheightlog)

#ifdef CONFIG_FBCON_FONTWIDTH8_ONLY

/* fontwidth w is supported by dispsw */
#define FONTWIDTH(w)	(1 << ((8) - 1))
/* fontwidths w1-w2 inclusive are supported by dispsw */
#define FONTWIDTHRANGE(w1,w2)	FONTWIDTH(8)

#define fontwidth(p) (8)
#define fontwidthlog(p) (0)

#else

/* fontwidth w is supported by dispsw */
#define FONTWIDTH(w)	(1 << ((w) - 1))
/* fontwidths w1-w2 inclusive are supported by dispsw */
#define FONTWIDTHRANGE(w1,w2)	(FONTWIDTH(w2+1) - FONTWIDTH(w1))

#define fontwidth(p) ((p)->_fontwidth)
#define fontwidthlog(p) ((p)->_fontwidthlog)

#endif

    /*
     *  Attribute Decoding
     */

/* Color */
#define attr_fgcol(p,s)    \
	(((s) >> ((p)->fgshift)) & 0x0f)
#define attr_bgcol(p,s)    \
	(((s) >> ((p)->bgshift)) & 0x0f)
#define	attr_bgcol_ec(p,conp) \
	((conp) ? (((conp)->vc_video_erase_char >> ((p)->bgshift)) & 0x0f) : 0)

/* Monochrome */
#define attr_bold(p,s) \
	((s) & 0x200)
#define attr_reverse(p,s) \
	(((s) & 0x800) ^ ((p)->inverse ? 0x800 : 0))
#define attr_underline(p,s) \
	((s) & 0x400)
#define attr_blink(p,s) \
	((s) & 0x8000)
	
    /*
     *  Scroll Method
     */
     
/* Internal flags */
#define __SCROLL_YPAN		0x001
#define __SCROLL_YWRAP		0x002
#define __SCROLL_YMOVE		0x003
#define __SCROLL_YREDRAW	0x004
#define __SCROLL_YMASK		0x00f
#define __SCROLL_YFIXED		0x010
#define __SCROLL_YNOMOVE	0x020
#define __SCROLL_YPANREDRAW	0x040
#define __SCROLL_YNOPARTIAL	0x080

/* Only these should be used by the drivers */
/* Which one should you use? If you have a fast card and slow bus,
   then probably just 0 to indicate fbcon should choose between
   YWRAP/YPAN+MOVE/YMOVE. On the other side, if you have a fast bus
   and even better if your card can do fonting (1->8/32bit painting),
   you should consider either SCROLL_YREDRAW (if your card is
   able to do neither YPAN/YWRAP), or SCROLL_YNOMOVE.
   The best is to test it with some real life scrolling (usually, not
   all lines on the screen are filled completely with non-space characters,
   and REDRAW performs much better on such lines, so don't cat a file
   with every line covering all screen columns, it would not be the right
   benchmark).
 */
#define SCROLL_YREDRAW		(__SCROLL_YFIXED|__SCROLL_YREDRAW)
#define SCROLL_YNOMOVE		(__SCROLL_YNOMOVE|__SCROLL_YPANREDRAW)

/* SCROLL_YNOPARTIAL, used in combination with the above, is for video
   cards which can not handle using panning to scroll a portion of the
   screen without excessive flicker.  Panning will only be used for
   whole screens.
 */
/* Namespace consistency */
#define SCROLL_YNOPARTIAL	__SCROLL_YNOPARTIAL


#if defined(__sparc__)

/* We map all of our framebuffers such that big-endian accesses
 * are what we want, so the following is sufficient.
 */

#define fb_readb sbus_readb
#define fb_readw sbus_readw
#define fb_readl sbus_readl
#define fb_writeb sbus_writeb
#define fb_writew sbus_writew
#define fb_writel sbus_writel
#define fb_memset sbus_memset_io

#elif defined(__i386__) || defined(__alpha__) || \
      defined(__x86_64__) || defined(__hppa__) || \
      defined(__powerpc64__)

#define fb_readb __raw_readb
#define fb_readw __raw_readw
#define fb_readl __raw_readl
#define fb_writeb __raw_writeb
#define fb_writew __raw_writew
#define fb_writel __raw_writel
#define fb_memset memset_io

#else

#define fb_readb(addr) (*(volatile u8 *) (addr))
#define fb_readw(addr) (*(volatile u16 *) (addr))
#define fb_readl(addr) (*(volatile u32 *) (addr))
#define fb_writeb(b,addr) (*(volatile u8 *) (addr) = (b))
#define fb_writew(b,addr) (*(volatile u16 *) (addr) = (b))
#define fb_writel(b,addr) (*(volatile u32 *) (addr) = (b))
#define fb_memset memset

#endif


extern void fbcon_redraw_clear(struct vc_data *, struct display *, int, int, int, int);
extern void fbcon_redraw_bmove(struct display *, int, int, int, int, int, int);


/* ================================================================= */
/*                      Utility Assembler Functions                  */
/* ================================================================= */


#if defined(__mc68000__)

/* ====================================================================== */

/* Those of a delicate disposition might like to skip the next couple of
 * pages.
 *
 * These functions are drop in replacements for memmove and
 * memset(_, 0, _). However their five instances add at least a kilobyte
 * to the object file. You have been warned.
 *
 * Not a great fan of assembler for the sake of it, but I think
 * that these routines are at least 10 times faster than their C
 * equivalents for large blits, and that's important to the lowest level of
 * a graphics driver. Question is whether some scheme with the blitter
 * would be faster. I suspect not for simple text system - not much
 * asynchrony.
 *
 * Code is very simple, just gruesome expansion. Basic strategy is to
 * increase data moved/cleared at each step to 16 bytes to reduce
 * instruction per data move overhead. movem might be faster still
 * For more than 15 bytes, we try to align the write direction on a
 * longword boundary to get maximum speed. This is even more gruesome.
 * Unaligned read/write used requires 68020+ - think this is a problem?
 *
 * Sorry!
 */


/* ++roman: I've optimized Robert's original versions in some minor
 * aspects, e.g. moveq instead of movel, let gcc choose the registers,
 * use movem in some places...
 * For other modes than 1 plane, lots of more such assembler functions
 * were needed (e.g. the ones using movep or expanding color values).
 */

/* ++andreas: more optimizations:
   subl #65536,d0 replaced by clrw d0; subql #1,d0 for dbcc
   addal is faster than addaw
   movep is rather expensive compared to ordinary move's
   some functions rewritten in C for clarity, no speed loss */

static __inline__ void *fb_memclear_small(void *s, size_t count)
{
   if (!count)
      return(0);

   __asm__ __volatile__(
         "lsrl   #1,%1 ; jcc 1f ; moveb %2,%0@-\n\t"
      "1: lsrl   #1,%1 ; jcc 1f ; movew %2,%0@-\n\t"
      "1: lsrl   #1,%1 ; jcc 1f ; movel %2,%0@-\n\t"
      "1: lsrl   #1,%1 ; jcc 1f ; movel %2,%0@- ; movel %2,%0@-\n\t"
      "1:"
         : "=a" (s), "=d" (count)
         : "d" (0), "0" ((char *)s+count), "1" (count)
   );
   __asm__ __volatile__(
         "subql  #1,%1 ; jcs 3f\n\t"
	 "movel %2,%%d4; movel %2,%%d5; movel %2,%%d6\n\t"
      "2: moveml %2/%%d4/%%d5/%%d6,%0@-\n\t"
         "dbra %1,2b\n\t"
      "3:"
         : "=a" (s), "=d" (count)
         : "d" (0), "0" (s), "1" (count)
	 : "d4", "d5", "d6"
  );

   return(0);
}


static __inline__ void *fb_memclear(void *s, size_t count)
{
   if (!count)
      return(0);

   if (count < 16) {
      __asm__ __volatile__(
            "lsrl   #1,%1 ; jcc 1f ; clrb %0@+\n\t"
         "1: lsrl   #1,%1 ; jcc 1f ; clrw %0@+\n\t"
         "1: lsrl   #1,%1 ; jcc 1f ; clrl %0@+\n\t"
         "1: lsrl   #1,%1 ; jcc 1f ; clrl %0@+ ; clrl %0@+\n\t"
         "1:"
            : "=a" (s), "=d" (count)
            : "0" (s), "1" (count)
     );
   } else {
      long tmp;
      __asm__ __volatile__(
            "movel %1,%2\n\t"
            "lsrl   #1,%2 ; jcc 1f ; clrb %0@+ ; subqw #1,%1\n\t"
            "lsrl   #1,%2 ; jcs 2f\n\t"  /* %0 increased=>bit 2 switched*/
            "clrw   %0@+  ; subqw  #2,%1 ; jra 2f\n\t"
         "1: lsrl   #1,%2 ; jcc 2f\n\t"
            "clrw   %0@+  ; subqw  #2,%1\n\t"
         "2: movew %1,%2; lsrl #2,%1 ; jeq 6f\n\t"
            "lsrl   #1,%1 ; jcc 3f ; clrl %0@+\n\t"
         "3: lsrl   #1,%1 ; jcc 4f ; clrl %0@+ ; clrl %0@+\n\t"
         "4: subql  #1,%1 ; jcs 6f\n\t"
         "5: clrl %0@+; clrl %0@+ ; clrl %0@+ ; clrl %0@+\n\t"
            "dbra %1,5b   ; clrw %1; subql #1,%1; jcc 5b\n\t"
         "6: movew %2,%1; btst #1,%1 ; jeq 7f ; clrw %0@+\n\t"
         "7:            ; btst #0,%1 ; jeq 8f ; clrb %0@+\n\t"
         "8:"
            : "=a" (s), "=d" (count), "=d" (tmp)
            : "0" (s), "1" (count)
     );
   }

   return(0);
}


static __inline__ void *fb_memset255(void *s, size_t count)
{
   if (!count)
      return(0);

   __asm__ __volatile__(
         "lsrl   #1,%1 ; jcc 1f ; moveb %2,%0@-\n\t"
      "1: lsrl   #1,%1 ; jcc 1f ; movew %2,%0@-\n\t"
      "1: lsrl   #1,%1 ; jcc 1f ; movel %2,%0@-\n\t"
      "1: lsrl   #1,%1 ; jcc 1f ; movel %2,%0@- ; movel %2,%0@-\n\t"
      "1:"
         : "=a" (s), "=d" (count)
         : "d" (-1), "0" ((char *)s+count), "1" (count)
   );
   __asm__ __volatile__(
         "subql  #1,%1 ; jcs 3f\n\t"
	 "movel %2,%%d4; movel %2,%%d5; movel %2,%%d6\n\t"
      "2: moveml %2/%%d4/%%d5/%%d6,%0@-\n\t"
         "dbra %1,2b\n\t"
      "3:"
         : "=a" (s), "=d" (count)
         : "d" (-1), "0" (s), "1" (count)
	 : "d4", "d5", "d6"
  );

   return(0);
}


static __inline__ void *fb_memmove(void *d, const void *s, size_t count)
{
   if (d < s) {
      if (count < 16) {
         __asm__ __volatile__(
               "lsrl   #1,%2 ; jcc 1f ; moveb %1@+,%0@+\n\t"
            "1: lsrl   #1,%2 ; jcc 1f ; movew %1@+,%0@+\n\t"
            "1: lsrl   #1,%2 ; jcc 1f ; movel %1@+,%0@+\n\t"
            "1: lsrl   #1,%2 ; jcc 1f ; movel %1@+,%0@+ ; movel %1@+,%0@+\n\t"
            "1:"
               : "=a" (d), "=a" (s), "=d" (count)
               : "0" (d), "1" (s), "2" (count)
        );
      } else {
         long tmp;
         __asm__ __volatile__(
               "movel  %0,%3\n\t"
               "lsrl   #1,%3 ; jcc 1f ; moveb %1@+,%0@+ ; subqw #1,%2\n\t"
               "lsrl   #1,%3 ; jcs 2f\n\t"  /* %0 increased=>bit 2 switched*/
               "movew  %1@+,%0@+  ; subqw  #2,%2 ; jra 2f\n\t"
            "1: lsrl   #1,%3 ; jcc 2f\n\t"
               "movew  %1@+,%0@+  ; subqw  #2,%2\n\t"
            "2: movew  %2,%-; lsrl #2,%2 ; jeq 6f\n\t"
               "lsrl   #1,%2 ; jcc 3f ; movel %1@+,%0@+\n\t"
            "3: lsrl   #1,%2 ; jcc 4f ; movel %1@+,%0@+ ; movel %1@+,%0@+\n\t"
            "4: subql  #1,%2 ; jcs 6f\n\t"
            "5: movel  %1@+,%0@+;movel %1@+,%0@+\n\t"
               "movel  %1@+,%0@+;movel %1@+,%0@+\n\t"
               "dbra   %2,5b ; clrw %2; subql #1,%2; jcc 5b\n\t"
            "6: movew  %+,%2; btst #1,%2 ; jeq 7f ; movew %1@+,%0@+\n\t"
            "7:              ; btst #0,%2 ; jeq 8f ; moveb %1@+,%0@+\n\t"
            "8:"
               : "=a" (d), "=a" (s), "=d" (count), "=d" (tmp)
               : "0" (d), "1" (s), "2" (count)
        );
      }
   } else {
      if (count < 16) {
         __asm__ __volatile__(
               "lsrl   #1,%2 ; jcc 1f ; moveb %1@-,%0@-\n\t"
            "1: lsrl   #1,%2 ; jcc 1f ; movew %1@-,%0@-\n\t"
            "1: lsrl   #1,%2 ; jcc 1f ; movel %1@-,%0@-\n\t"
            "1: lsrl   #1,%2 ; jcc 1f ; movel %1@-,%0@- ; movel %1@-,%0@-\n\t"
            "1:"
               : "=a" (d), "=a" (s), "=d" (count)
               : "0" ((char *) d + count), "1" ((char *) s + count), "2" (count)
        );
      } else {
         long tmp;
         __asm__ __volatile__(
               "movel %0,%3\n\t"
               "lsrl   #1,%3 ; jcc 1f ; moveb %1@-,%0@- ; subqw #1,%2\n\t"
               "lsrl   #1,%3 ; jcs 2f\n\t"  /* %0 increased=>bit 2 switched*/
               "movew  %1@-,%0@-  ; subqw  #2,%2 ; jra 2f\n\t"
            "1: lsrl   #1,%3 ; jcc 2f\n\t"
               "movew  %1@-,%0@-  ; subqw  #2,%2\n\t"
            "2: movew %2,%-; lsrl #2,%2 ; jeq 6f\n\t"
               "lsrl   #1,%2 ; jcc 3f ; movel %1@-,%0@-\n\t"
            "3: lsrl   #1,%2 ; jcc 4f ; movel %1@-,%0@- ; movel %1@-,%0@-\n\t"
            "4: subql  #1,%2 ; jcs 6f\n\t"
            "5: movel %1@-,%0@-;movel %1@-,%0@-\n\t"
               "movel %1@-,%0@-;movel %1@-,%0@-\n\t"
               "dbra %2,5b ; clrw %2; subql #1,%2; jcc 5b\n\t"
            "6: movew %+,%2; btst #1,%2 ; jeq 7f ; movew %1@-,%0@-\n\t"
            "7:              ; btst #0,%2 ; jeq 8f ; moveb %1@-,%0@-\n\t"
            "8:"
               : "=a" (d), "=a" (s), "=d" (count), "=d" (tmp)
               : "0" ((char *) d + count), "1" ((char *) s + count), "2" (count)
        );
      }
   }

   return(0);
}


/* ++andreas: Simple and fast version of memmove, assumes size is
   divisible by 16, suitable for moving the whole screen bitplane */
static __inline__ void fast_memmove(char *dst, const char *src, size_t size)
{
  if (!size)
    return;
  if (dst < src)
    __asm__ __volatile__
      ("1:"
       "  moveml %0@+,%/d0/%/d1/%/a0/%/a1\n"
       "  moveml %/d0/%/d1/%/a0/%/a1,%1@\n"
       "  addql #8,%1; addql #8,%1\n"
       "  dbra %2,1b\n"
       "  clrw %2; subql #1,%2\n"
       "  jcc 1b"
       : "=a" (src), "=a" (dst), "=d" (size)
       : "0" (src), "1" (dst), "2" (size / 16 - 1)
       : "d0", "d1", "a0", "a1", "memory");
  else
    __asm__ __volatile__
      ("1:"
       "  subql #8,%0; subql #8,%0\n"
       "  moveml %0@,%/d0/%/d1/%/a0/%/a1\n"
       "  moveml %/d0/%/d1/%/a0/%/a1,%1@-\n"
       "  dbra %2,1b\n"
       "  clrw %2; subql #1,%2\n"
       "  jcc 1b"
       : "=a" (src), "=a" (dst), "=d" (size)
       : "0" (src + size), "1" (dst + size), "2" (size / 16 - 1)
       : "d0", "d1", "a0", "a1", "memory");
}

#elif defined(CONFIG_SUN4)

/* You may think that I'm crazy and that I should use generic
   routines.  No, I'm not: sun4's framebuffer crashes if we std
   into it, so we cannot use memset.  */

static __inline__ void *sun4_memset(void *s, char val, size_t count)
{
    int i;
    for(i=0; i<count;i++)
        ((char *) s) [i] = val;
    return s;
}

static __inline__ void *fb_memset255(void *s, size_t count)
{
    return sun4_memset(s, 255, count);
}

static __inline__ void *fb_memclear(void *s, size_t count)
{
    return sun4_memset(s, 0, count);
}

static __inline__ void *fb_memclear_small(void *s, size_t count)
{
    return sun4_memset(s, 0, count);
}

/* To be honest, this is slow_memmove :). But sun4 is crappy, so what we can do. */
static __inline__ void fast_memmove(void *d, const void *s, size_t count)
{
    int i;
    if (d<s) {
	for (i=0; i<count; i++)
	    ((char *) d)[i] = ((char *) s)[i];
    } else
	for (i=0; i<count; i++)
	    ((char *) d)[count-i-1] = ((char *) s)[count-i-1];
}

static __inline__ void *fb_memmove(char *dst, const char *src, size_t size)
{
    fast_memmove(dst, src, size);
    return dst;
}

#else

static __inline__ void *fb_memclear_small(void *s, size_t count)
{
    char *xs = (char *) s;

    while (count--)
	fb_writeb(0, xs++);

    return s;
}

static __inline__ void *fb_memclear(void *s, size_t count)
{
    unsigned long xs = (unsigned long) s;

    if (count < 8)
	goto rest;

    if (xs & 1) {
	fb_writeb(0, xs++);
	count--;
    }
    if (xs & 2) {
	fb_writew(0, xs);
	xs += 2;
	count -= 2;
    }
    while (count > 3) {
	fb_writel(0, xs);
	xs += 4;
	count -= 4;
    }
rest:
    while (count--)
	fb_writeb(0, xs++);

    return s;
}

static __inline__ void *fb_memset255(void *s, size_t count)
{
    unsigned long xs = (unsigned long) s;

    if (count < 8)
	goto rest;

    if (xs & 1) {
	fb_writeb(0xff, xs++);
	count--;
    }
    if (xs & 2) {
	fb_writew(0xffff, xs);
	xs += 2;
	count -= 2;
    }
    while (count > 3) {
	fb_writel(0xffffffff, xs);
	xs += 4;
	count -= 4;
    }
rest:
    while (count--)
	fb_writeb(0xff, xs++);

    return s;
}

#if defined(__i386__)

static __inline__ void fast_memmove(void *d, const void *s, size_t count)
{
  int d0, d1, d2, d3;
    if (d < s) {
__asm__ __volatile__ (
	"cld\n\t"
	"shrl $1,%%ecx\n\t"
	"jnc 1f\n\t"
	"movsb\n"
	"1:\tshrl $1,%%ecx\n\t"
	"jnc 2f\n\t"
	"movsw\n"
	"2:\trep\n\t"
	"movsl"
	: "=&c" (d0), "=&D" (d1), "=&S" (d2)
	:"0"(count),"1"((long)d),"2"((long)s)
	:"memory");
    } else {
__asm__ __volatile__ (
	"std\n\t"
	"shrl $1,%%ecx\n\t"
	"jnc 1f\n\t"
	"movb 3(%%esi),%%al\n\t"
	"movb %%al,3(%%edi)\n\t"
	"decl %%esi\n\t"
	"decl %%edi\n"
	"1:\tshrl $1,%%ecx\n\t"
	"jnc 2f\n\t"
	"movw 2(%%esi),%%ax\n\t"
	"movw %%ax,2(%%edi)\n\t"
	"decl %%esi\n\t"
	"decl %%edi\n\t"
	"decl %%esi\n\t"
	"decl %%edi\n"
	"2:\trep\n\t"
	"movsl\n\t"
	"cld"
	: "=&c" (d0), "=&D" (d1), "=&S" (d2), "=&a" (d3)
	:"0"(count),"1"(count-4+(long)d),"2"(count-4+(long)s)
	:"memory");
    }
}

static __inline__ void *fb_memmove(char *dst, const char *src, size_t size)
{
    fast_memmove(dst, src, size);
    return dst;
}

#else /* !__i386__ */

    /*
     *  Anyone who'd like to write asm functions for other CPUs?
     *   (Why are these functions better than those from include/asm/string.h?)
     */

static __inline__ void *fb_memmove(void *d, const void *s, size_t count)
{
    unsigned long dst, src;

    if (d < s) {
	dst = (unsigned long) d;
	src = (unsigned long) s;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restup;

	if (dst & 1) {
	    fb_writeb(fb_readb(src++), dst++);
	    count--;
	}
	if (dst & 2) {
	    fb_writew(fb_readw(src), dst);
	    src += 2;
	    dst += 2;
	    count -= 2;
	}
	while (count > 3) {
	    fb_writel(fb_readl(src), dst);
	    src += 4;
	    dst += 4;
	    count -= 4;
	}

    restup:
	while (count--)
	    fb_writeb(fb_readb(src++), dst++);
    } else {
	dst = (unsigned long) d + count;
	src = (unsigned long) s + count;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restdown;

	if (dst & 1) {
	    src--;
	    dst--;
	    count--;
	    fb_writeb(fb_readb(src), dst);
	}
	if (dst & 2) {
	    src -= 2;
	    dst -= 2;
	    count -= 2;
	    fb_writew(fb_readw(src), dst);
	}
	while (count > 3) {
	    src -= 4;
	    dst -= 4;
	    count -= 4;
	    fb_writel(fb_readl(src), dst);
	}

    restdown:
	while (count--) {
	    src--;
	    dst--;
	    fb_writeb(fb_readb(src), dst);
	}
    }

    return d;
}

static __inline__ void fast_memmove(char *d, const char *s, size_t count)
{
    unsigned long dst, src;

    if (d < s) {
	dst = (unsigned long) d;
	src = (unsigned long) s;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restup;

	if (dst & 1) {
	    fb_writeb(fb_readb(src++), dst++);
	    count--;
	}
	if (dst & 2) {
	    fb_writew(fb_readw(src), dst);
	    src += 2;
	    dst += 2;
	    count -= 2;
	}
	while (count > 3) {
	    fb_writel(fb_readl(src), dst);
	    src += 4;
	    dst += 4;
	    count -= 4;
	}

    restup:
	while (count--)
	    fb_writeb(fb_readb(src++), dst++);
    } else {
	dst = (unsigned long) d + count;
	src = (unsigned long) s + count;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restdown;

	if (dst & 1) {
	    src--;
	    dst--;
	    count--;
	    fb_writeb(fb_readb(src), dst);
	}
	if (dst & 2) {
	    src -= 2;
	    dst -= 2;
	    count -= 2;
	    fb_writew(fb_readw(src), dst);
	}
	while (count > 3) {
	    src -= 4;
	    dst -= 4;
	    count -= 4;
	    fb_writel(fb_readl(src), dst);
	}

    restdown:
	while (count--) {
	    src--;
	    dst--;
	    fb_writeb(fb_readb(src), dst);
	}
    }
}

#endif /* !__i386__ */

#endif /* !__mc68000__ */

#endif /* _VIDEO_FBCON_H */
