/*
 *  linux/drivers/video/iplan2p8.c -- Low level frame buffer operations for
 *				      interleaved bitplanes à la Atari (8
 *				      planes, 2 bytes interleave)
 *
 *	Created 5 Apr 1997 by Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>

#include <asm/byteorder.h>

#ifdef __mc68000__
#include <asm/setup.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-iplan2p8.h>


    /*
     *  Interleaved bitplanes à la Atari (8 planes, 2 bytes interleave)
     *
     *  In 8 plane mode, 256 colors would be possible, but only the first
     *  16 are used by the console code (the upper 4 bits are
     *  background/unused). For that, the following functions mask off the
     *  higher 4 bits of each color.
     */

/* Increment/decrement 8 plane addresses */

#define	INC_8P(p)	do { if (!((long)(++(p)) & 1)) (p) += 14; } while(0)
#define	DEC_8P(p)	do { if ((long)(--(p)) & 1) (p) -= 14; } while(0)

/* Perform the m68k movepl operation extended to 64 bits.  */
static inline void movepl2(u8 *d, u32 val1, u32 val2)
{
#if defined __mc68000__ && !defined CPU_M68060_ONLY
    asm volatile ("movepl %1,%0@(0); movepl %2,%0@(8)"
		  : : "a" (d), "d" (val1), "d" (val2));
#else
    d[0] = (val1 >> 24) & 0xff;
    d[2] = (val1 >> 16) & 0xff;
    d[4] = (val1 >> 8) & 0xff;
    d[6] = val1 & 0xff;
    d[8] = (val2 >> 24) & 0xff;
    d[10] = (val2 >> 16) & 0xff;
    d[12] = (val2 >> 8) & 0xff;
    d[14] = val2 & 0xff;
#endif
}

/* Sets the bytes in the visible column at d, height h, to the value
 * val1,val2 for a 8 plane screen. The bits of the color in 'color' are
 * moved (8 times) to the respective bytes. This means:
 *
 * for(h times; d += bpr)
 *   *d      = (color & 1) ? 0xff : 0;
 *   *(d+2)  = (color & 2) ? 0xff : 0;
 *   *(d+4)  = (color & 4) ? 0xff : 0;
 *   *(d+6)  = (color & 8) ? 0xff : 0;
 *   *(d+8)  = (color & 16) ? 0xff : 0;
 *   *(d+10) = (color & 32) ? 0xff : 0;
 *   *(d+12) = (color & 64) ? 0xff : 0;
 *   *(d+14) = (color & 128) ? 0xff : 0;
 */

static __inline__ void memclear_8p_col(void *d, size_t h, u32 val1,
                                       u32 val2, int bpr)
{
    u8 *dd = d;
    do {
	movepl2(dd, val1, val2);
	dd += bpr;
    } while (--h);
}

/* Sets a 8 plane region from 'd', length 'count' bytes, to the color
 * val1..val4. 'd' has to be an even address and count must be divisible
 * by 16, because only whole words and all planes are accessed. I.e.:
 *
 * for(count/16 times)
 *   *d      = *(d+1)  = (color & 1) ? 0xff : 0;
 *   *(d+2)  = *(d+3)  = (color & 2) ? 0xff : 0;
 *   *(d+4)  = *(d+5)  = (color & 4) ? 0xff : 0;
 *   *(d+6)  = *(d+7)  = (color & 8) ? 0xff : 0;
 *   *(d+8)  = *(d+9)  = (color & 16) ? 0xff : 0;
 *   *(d+10) = *(d+11) = (color & 32) ? 0xff : 0;
 *   *(d+12) = *(d+13) = (color & 64) ? 0xff : 0;
 *   *(d+14) = *(d+15) = (color & 128) ? 0xff : 0;
 */

static __inline__ void memset_even_8p(void *d, size_t count, u32 val1,
                                      u32 val2, u32 val3, u32 val4)
{
    u32 *dd = d;

    count /= 16;
    while (count--) {
	*dd++ = val1;
	*dd++ = val2;
	*dd++ = val3;
	*dd++ = val4;
    }
}

/* Copies a 8 plane column from 's', height 'h', to 'd'. */

static __inline__ void memmove_8p_col (void *d, void *s, int h, int bpr)
{
    u8 *dd = d, *ss = s;

    while (h--) {
	dd[0] = ss[0];
	dd[2] = ss[2];
	dd[4] = ss[4];
	dd[6] = ss[6];
	dd[8] = ss[8];
	dd[10] = ss[10];
	dd[12] = ss[12];
	dd[14] = ss[14];
	dd += bpr;
	ss += bpr;
    }
}


/* This expands a 8 bit color into two longs for two movepl (8 plane)
 * operations.
 */

static const u32 four2long[] =
{
    0x00000000, 0xff000000, 0x00ff0000, 0xffff0000,
    0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
    0x000000ff, 0xff0000ff, 0x00ff00ff, 0xffff00ff,
    0x0000ffff, 0xff00ffff, 0x00ffffff, 0xffffffff,
};

static __inline__ void expand8dl(u8 c, u32 *ret1, u32 *ret2)
{
    *ret1 = four2long[c & 15];
    *ret2 = four2long[c >> 4];
}


/* This expands a 8 bit color into four longs for four movel operations
 * (8 planes).
 */

static const u32 two2word[] =
{
#ifndef __LITTLE_ENDIAN
    0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff
#else
    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff
#endif
};

static inline void expand8ql(u8 c, u32 *rv1, u32 *rv2, u32 *rv3, u32 *rv4)
{
    *rv1 = two2word[c & 4];
    *rv2 = two2word[(c >> 2) & 4];
    *rv3 = two2word[(c >> 4) & 4];
    *rv4 = two2word[c >> 6];
}


/* This duplicates a byte 4 times into a long. */

static __inline__ u32 dup4l(u8 c)
{
    u32 rv;

    rv = c;
    rv |= rv << 8;
    rv |= rv << 16;
    return rv;
}


void fbcon_iplan2p8_setup(struct display *p)
{
    p->next_line = p->var.xres_virtual;
    p->next_plane = 2;
}

void fbcon_iplan2p8_bmove(struct display *p, int sy, int sx, int dy, int dx,
			  int height, int width)
{
    /*  bmove() has to distinguish two major cases: If both, source and
     *  destination, start at even addresses or both are at odd
     *  addresses, just the first odd and last even column (if present)
     *  require special treatment (memmove_col()). The rest between
     *  then can be copied by normal operations, because all adjacent
     *  bytes are affected and are to be stored in the same order.
     *    The pathological case is when the move should go from an odd
     *  address to an even or vice versa. Since the bytes in the plane
     *  words must be assembled in new order, it seems wisest to make
     *  all movements by memmove_col().
     */

     if (sx == 0 && dx == 0 && width * 8 == p->next_line) {
	/*  Special (but often used) case: Moving whole lines can be
	 *  done with memmove()
	 */
	fast_memmove(p->screen_base + dy * p->next_line * fontheight(p),
		     p->screen_base + sy * p->next_line * fontheight(p),
		     p->next_line * height * fontheight(p));
     } else {
	int rows, cols;
	u8 *src;
	u8 *dst;
	int bytes = p->next_line;
	int linesize;
	u_int colsize;
	u_int upwards = (dy < sy) || (dy == sy && dx < sx);

	if (fontheightlog(p)) {
	    linesize = bytes << fontheightlog(p);
	    colsize = height << fontheightlog(p);
	} else {
	    linesize = bytes * fontheight(p);
	    colsize = height * fontheight(p);
	}
	if ((sx & 1) == (dx & 1)) {
	    /* odd->odd or even->even */

	    if (upwards) {
		src = p->screen_base + sy * linesize + (sx>>1)*16 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*16 + (dx & 1);
		if (sx & 1) {
		    memmove_8p_col(dst, src, colsize, bytes);
		    src += 15;
		    dst += 15;
		    --width;
		}
		if (width > 1) {
		    for(rows = colsize; rows > 0; --rows) {
			fast_memmove (dst, src, (width >> 1) * 16);
			src += bytes;
			dst += bytes;
		    }
		}

		if (width & 1) {
		    src -= colsize * bytes;
		    dst -= colsize * bytes;
		    memmove_8p_col(dst + (width>>1)*16, src + (width>>1)*16,
		    colsize, bytes);
		}
	    } else {
		if (!((sx+width-1) & 1)) {
		    src = p->screen_base + sy * linesize + ((sx+width-1)>>1)*16;
		    dst = p->screen_base + dy * linesize + ((dx+width-1)>>1)*16;
		    memmove_8p_col(dst, src, colsize, bytes);
		    --width;
		}
		src = p->screen_base + sy * linesize + (sx>>1)*16 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*16 + (dx & 1);
		if (width > 1) {
		    src += colsize * bytes + (sx & 1)*15;
		    dst += colsize * bytes + (sx & 1)*15;
		    for(rows = colsize; rows > 0; --rows) {
			src -= bytes;
			dst -= bytes;
			fast_memmove (dst, src, (width>>1)*16);
		    }
		}
		if (width & 1)
		    memmove_8p_col(dst-15, src-15, colsize, bytes);
	    }
	} else {
	/* odd->even or even->odd */

	    if (upwards) {
		src = p->screen_base + sy * linesize + (sx>>1)*16 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*16 + (dx & 1);
		for(cols = width; cols > 0; --cols) {
		    memmove_8p_col(dst, src, colsize, bytes);
		    INC_8P(src);
		    INC_8P(dst);
		}
	    } else {
		sx += width-1;
		dx += width-1;
		src = p->screen_base + sy * linesize + (sx>>1)*16 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*16 + (dx & 1);
		for(cols = width; cols > 0; --cols) {
		    memmove_8p_col(dst, src, colsize, bytes);
		    DEC_8P(src);
		    DEC_8P(dst);
		}
	    }
	}
    }
}

void fbcon_iplan2p8_clear(struct vc_data *conp, struct display *p, int sy,
			  int sx, int height, int width)
{
    u32 offset;
    u8 *start;
    int rows;
    int bytes = p->next_line;
    int lines;
    u32 size;
    u32 cval1, cval2, cval3, cval4, pcval1, pcval2;

    expand8ql(attr_bgcol_ec(p,conp), &cval1, &cval2, &cval3, &cval4);

    if (fontheightlog(p))
	lines = height << fontheightlog(p);
    else
	lines = height * fontheight(p);

    if (sx == 0 && width * 8 == bytes) {
	if (fontheightlog(p))
	    offset = (sy * bytes) << fontheightlog(p);
	else
	    offset = sy * bytes * fontheight(p);
	size    = lines * bytes;
	memset_even_8p(p->screen_base+offset, size, cval1, cval2, cval3, cval4);
    } else {
	if (fontheightlog(p))
	    offset = ((sy * bytes) << fontheightlog(p)) + (sx>>1)*16 + (sx & 1);
	else
	    offset = sy * bytes * fontheight(p) + (sx>>1)*16 + (sx & 1);
	start = p->screen_base + offset;
	expand8dl(attr_bgcol_ec(p,conp), &pcval1, &pcval2);

	/* Clears are split if the region starts at an odd column or
	* end at an even column. These extra columns are spread
	* across the interleaved planes. All in between can be
	* cleared by normal fb_memclear_small(), because both bytes of
	* the single plane words are affected.
	*/

	if (sx & 1) {
	    memclear_8p_col(start, lines, pcval1, pcval2, bytes);
	    start += 7;
	    width--;
	}
	if (width & 1) {
	    memclear_8p_col(start + (width>>1)*16, lines, pcval1,
	    pcval2, bytes);
	    width--;
	}
	if (width)
	    for(rows = lines; rows-- ; start += bytes)
		memset_even_8p(start, width*8, cval1, cval2, cval3, cval4);
	}
}

void fbcon_iplan2p8_putc(struct vc_data *conp, struct display *p, int c,
			 int yy, int xx)
{
    u8 *dest;
    u8 *cdat;
    int rows;
    int bytes = p->next_line;
    u32 eorx1, eorx2, fgx1, fgx2, bgx1, bgx2, fdx;

    if (fontheightlog(p)) {
	dest = (p->screen_base + ((yy * bytes) << fontheightlog(p)) +
		(xx>>1)*16 + (xx & 1));
	cdat = p->fontdata + ((c & p->charmask) << fontheightlog(p));
    } else {
	dest = (p->screen_base + yy * bytes * fontheight(p) +
		(xx>>1)*16 + (xx & 1));
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
    }

    expand8dl(attr_fgcol(p,c), &fgx1, &fgx2);
    expand8dl(attr_bgcol(p,c), &bgx1, &bgx2);
    eorx1 = fgx1 ^ bgx1; eorx2  = fgx2 ^ bgx2;

    for(rows = fontheight(p) ; rows-- ; dest += bytes) {
	fdx = dup4l(*cdat++);
	movepl2(dest, (fdx & eorx1) ^ bgx1, (fdx & eorx2) ^ bgx2);
    }
}

void fbcon_iplan2p8_putcs(struct vc_data *conp, struct display *p,
			  const unsigned short *s, int count, int yy, int xx)
{
    u8 *dest, *dest0;
    u8 *cdat;
    u16 c;
    int rows;
    int bytes;
    u32 eorx1, eorx2, fgx1, fgx2, bgx1, bgx2, fdx;

    bytes = p->next_line;
    if (fontheightlog(p))
	dest0 = (p->screen_base + ((yy * bytes) << fontheightlog(p)) +
		 (xx>>1)*16 + (xx & 1));
    else
	dest0 = (p->screen_base + yy * bytes * fontheight(p) +
		 (xx>>1)*16 + (xx & 1));

    c = scr_readw(s);
    expand8dl(attr_fgcol(p, c), &fgx1, &fgx2);
    expand8dl(attr_bgcol(p, c), &bgx1, &bgx2);
    eorx1 = fgx1 ^ bgx1; eorx2  = fgx2 ^ bgx2;

    while (count--) {

	/* I think, unrolling the loops like in the 1 plane case isn't
	* practicable here, because the body is much longer for 4
	* planes (mostly the dup4l()). I guess, unrolling this would
	* need more than 256 bytes and so exceed the instruction
	* cache :-(
	*/

	c = scr_readw(s++) & p->charmask;
	if (fontheightlog(p))
	    cdat = p->fontdata + (c << fontheightlog(p));
	else
	    cdat = p->fontdata + c * fontheight(p);

	for(rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
	    fdx = dup4l(*cdat++);
	    movepl2(dest, (fdx & eorx1) ^ bgx1, (fdx & eorx2) ^ bgx2);
	}
	INC_8P(dest0);
    }
}

void fbcon_iplan2p8_revc(struct display *p, int xx, int yy)
{
    u8 *dest;
    int j;
    int bytes;

    if (fontheightlog(p))
	dest = (p->screen_base + ((yy * p->next_line) << fontheightlog(p)) +
		(xx>>1)*16 + (xx & 1));
    else
	dest = (p->screen_base + yy * p->next_line * fontheight(p) +
		(xx>>1)*16 + (xx & 1));
    j = fontheight(p);
    bytes = p->next_line;

    while (j--) {
	/*  This should really obey the individual character's
	 *  background and foreground colors instead of simply
	 *  inverting. For 8 plane mode, only the lower 4 bits of the
	 *  color are inverted, because only these color registers have
	 *  been set up.
	 */
	dest[0] = ~dest[0];
	dest[2] = ~dest[2];
	dest[4] = ~dest[4];
	dest[6] = ~dest[6];
	dest += bytes;
    }
}

void fbcon_iplan2p8_clear_margins(struct vc_data *conp, struct display *p,
				  int bottom_only)
{
    u32 offset;
    int bytes;
    int lines;
    u32 cval1, cval2, cval3, cval4;

/* No need to handle right margin, cannot occur with fontwidth == 8 */

    bytes = p->next_line;
    if (fontheightlog(p)) {
	lines = p->var.yres - (conp->vc_rows << fontheightlog(p));
	offset = ((p->yscroll + conp->vc_rows) * bytes) << fontheightlog(p);
    } else {
	lines = p->var.yres - conp->vc_rows * fontheight(p);
	offset = (p->yscroll + conp->vc_rows) * bytes * fontheight(p);
    }
    if (lines) {
	expand8ql(attr_bgcol_ec(p,conp), &cval1, &cval2, &cval3, &cval4);
	memset_even_8p(p->screen_base+offset, lines * bytes,
		       cval1, cval2, cval3, cval4);
    }
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_iplan2p8 = {
    setup:		fbcon_iplan2p8_setup,
    bmove:		fbcon_iplan2p8_bmove,
    clear:		fbcon_iplan2p8_clear,
    putc:		fbcon_iplan2p8_putc,
    putcs:		fbcon_iplan2p8_putcs,
    revc:		fbcon_iplan2p8_revc,
    clear_margins:	fbcon_iplan2p8_clear_margins,
    fontwidthmask:	FONTWIDTH(8)
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

EXPORT_SYMBOL(fbcon_iplan2p8);
EXPORT_SYMBOL(fbcon_iplan2p8_setup);
EXPORT_SYMBOL(fbcon_iplan2p8_bmove);
EXPORT_SYMBOL(fbcon_iplan2p8_clear);
EXPORT_SYMBOL(fbcon_iplan2p8_putc);
EXPORT_SYMBOL(fbcon_iplan2p8_putcs);
EXPORT_SYMBOL(fbcon_iplan2p8_revc);
EXPORT_SYMBOL(fbcon_iplan2p8_clear_margins);
