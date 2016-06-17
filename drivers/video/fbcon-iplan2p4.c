/*
 *  linux/drivers/video/fbcon-iplan2p4.c -- Low level frame buffer operations
 *				   for interleaved bitplanes à la Atari (4
 *				   planes, 2 bytes interleave)
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
#include <video/fbcon-iplan2p4.h>


    /*
     *  Interleaved bitplanes à la Atari (4 planes, 2 bytes interleave)
     */

/* Increment/decrement 4 plane addresses */

#define	INC_4P(p)	do { if (!((long)(++(p)) & 1)) (p) += 6; } while(0)
#define	DEC_4P(p)	do { if ((long)(--(p)) & 1) (p) -= 6; } while(0)

/* Perform the m68k movepl operation.  */
static inline void movepl(u8 *d, u32 val)
{
#if defined __mc68000__ && !defined CPU_M68060_ONLY
    asm volatile ("movepl %1,%0@(0)" : : "a" (d), "d" (val));
#else
    d[0] = (val >> 24) & 0xff;
    d[2] = (val >> 16) & 0xff;
    d[4] = (val >> 8) & 0xff;
    d[6] = val & 0xff;
#endif
}

/* Sets the bytes in the visible column at d, height h, to the value
 * val for a 4 plane screen. The bits of the color in 'color' are
 * moved (8 times) to the respective bytes. This means:
 *
 * for(h times; d += bpr)
 *   *d     = (color & 1) ? 0xff : 0;
 *   *(d+2) = (color & 2) ? 0xff : 0;
 *   *(d+4) = (color & 4) ? 0xff : 0;
 *   *(d+6) = (color & 8) ? 0xff : 0;
 */

static __inline__ void memclear_4p_col(void *d, size_t h, u32 val, int bpr)
{
    u8 *dd = d;
    do {
	movepl(dd, val);
	dd += bpr;
    } while (--h);
}

/* Sets a 4 plane region from 'd', length 'count' bytes, to the color
 * in val1/val2. 'd' has to be an even address and count must be divisible
 * by 8, because only whole words and all planes are accessed. I.e.:
 *
 * for(count/8 times)
 *   *d     = *(d+1) = (color & 1) ? 0xff : 0;
 *   *(d+2) = *(d+3) = (color & 2) ? 0xff : 0;
 *   *(d+4) = *(d+5) = (color & 4) ? 0xff : 0;
 *   *(d+6) = *(d+7) = (color & 8) ? 0xff : 0;
 */

static __inline__ void memset_even_4p(void *d, size_t count, u32 val1,
                                      u32 val2)
{
    u32 *dd = d;

    count /= 8;
    while (count--) {
	*dd++ = val1;
	*dd++ = val2;
    }
}

/* Copies a 4 plane column from 's', height 'h', to 'd'. */

static __inline__ void memmove_4p_col (void *d, void *s, int h, int bpr)
{
    u8 *dd = d, *ss = s;

    while (h--) {
	dd[0] = ss[0];
	dd[2] = ss[2];
	dd[4] = ss[4];
	dd[6] = ss[6];
	dd += bpr;
	ss += bpr;
    }
}


/* This expands a 4 bit color into a long for movepl (4 plane) operations. */

static const u32 four2byte[] = {
    0x00000000, 0xff000000, 0x00ff0000, 0xffff0000,
    0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
    0x000000ff, 0xff0000ff, 0x00ff00ff, 0xffff00ff,
    0x0000ffff, 0xff00ffff, 0x00ffffff, 0xffffffff
};

static __inline__ u32 expand4l(u8 c)
{
    return four2byte[c];
}


/* This expands a 4 bit color into two longs for two movel operations
 * (4 planes).
 */

static const u32 two2word[] = {
#ifndef __LITTLE_ENDIAN
    0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff,
#else
    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff,
#endif
};

static __inline__ void expand4dl(u8 c, u32 *ret1, u32 *ret2)
{
    *ret1 = two2word[c & 3];
    *ret2 = two2word[c >> 2];
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


void fbcon_iplan2p4_setup(struct display *p)
{
    p->next_line = p->var.xres_virtual>>1;
    p->next_plane = 2;
}

void fbcon_iplan2p4_bmove(struct display *p, int sy, int sx, int dy, int dx,
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

    if (sx == 0 && dx == 0 && width * 4 == p->next_line) {
	/*  Special (but often used) case: Moving whole lines can be
	 *done with memmove()
	 */
	fb_memmove(p->screen_base + dy * p->next_line * fontheight(p),
		  p->screen_base + sy * p->next_line * fontheight(p),
		  p->next_line * height * fontheight(p));
    } else {
	int rows, cols;
	u8 *src;
	u8 *dst;
	int bytes = p->next_line;
	int linesize;
	u_int colsize;
	u_int upwards  = (dy < sy) || (dy == sy && dx < sx);

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
		src = p->screen_base + sy * linesize + (sx>>1)*8 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*8 + (dx & 1);
		if (sx & 1) {
		    memmove_4p_col(dst, src, colsize, bytes);
		    src += 7;
		    dst += 7;
		    --width;
		}
		if (width > 1) {
		    for(rows = colsize; rows > 0; --rows) {
			fb_memmove(dst, src, (width>>1)*8);
			src += bytes;
			dst += bytes;
		    }
		}
		if (width & 1) {
		    src -= colsize * bytes;
		    dst -= colsize * bytes;
		    memmove_4p_col(dst + (width>>1)*8, src + (width>>1)*8,
		    colsize, bytes);
		}
	    } else {
		if (!((sx+width-1) & 1)) {
		    src = p->screen_base + sy * linesize + ((sx+width-1)>>1)*8;
		    dst = p->screen_base + dy * linesize + ((dx+width-1)>>1)*8;
		    memmove_4p_col(dst, src, colsize, bytes);
		    --width;
		}
		src = p->screen_base + sy * linesize + (sx>>1)*8 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*8 + (dx & 1);
		if (width > 1) {
		    src += colsize * bytes + (sx & 1)*7;
		    dst += colsize * bytes + (sx & 1)*7;
		    for(rows = colsize; rows > 0; --rows) {
			src -= bytes;
			dst -= bytes;
			fb_memmove(dst, src, (width>>1)*8);
		    }
		}
		if (width & 1) {
		memmove_4p_col(dst-7, src-7, colsize, bytes);
		}
	    }
	} else {
	/* odd->even or even->odd */

	    if (upwards) {
		src = p->screen_base + sy * linesize + (sx>>1)*8 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*8 + (dx & 1);
		for(cols = width; cols > 0; --cols) {
		    memmove_4p_col(dst, src, colsize, bytes);
		    INC_4P(src);
		    INC_4P(dst);
		}
	    } else {
		sx += width-1;
		dx += width-1;
		src = p->screen_base + sy * linesize + (sx>>1)*8 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*8 + (dx & 1);
		for(cols = width; cols > 0; --cols) {
		    memmove_4p_col(dst, src, colsize, bytes);
		    DEC_4P(src);
		    DEC_4P(dst);
		}
	    }
	}
    }
}

void fbcon_iplan2p4_clear(struct vc_data *conp, struct display *p, int sy,
			  int sx, int height, int width)
{
    u32 offset;
    u8 *start;
    int rows;
    int bytes = p->next_line;
    int lines;
    u32 size;
    u32 cval1, cval2, pcval;

    expand4dl(attr_bgcol_ec(p,conp), &cval1, &cval2);

    if (fontheightlog(p))
	lines = height << fontheightlog(p);
    else
	lines = height * fontheight(p);

    if (sx == 0 && width * 4 == bytes) {
	if (fontheightlog(p))
	    offset = (sy * bytes) << fontheightlog(p);
	else
	    offset = sy * bytes * fontheight(p);
	size = lines * bytes;
	memset_even_4p(p->screen_base+offset, size, cval1, cval2);
    } else {
	if (fontheightlog(p))
	    offset = ((sy * bytes) << fontheightlog(p)) + (sx>>1)*8 + (sx & 1);
	else
	    offset = sy * bytes * fontheight(p) + (sx>>1)*8 + (sx & 1);
	start = p->screen_base + offset;
	pcval = expand4l(attr_bgcol_ec(p,conp));

	/*  Clears are split if the region starts at an odd column or
	 *  end at an even column. These extra columns are spread
	 *  across the interleaved planes. All in between can be
	 *  cleared by normal fb_memclear_small(), because both bytes of
	 *  the single plane words are affected.
	 */

	if (sx & 1) {
	    memclear_4p_col(start, lines, pcval, bytes);
	    start += 7;
	    width--;
	}
	if (width & 1) {
	    memclear_4p_col(start + (width>>1)*8, lines, pcval, bytes);
	    width--;
	}
	if (width) {
	    for(rows = lines; rows-- ; start += bytes)
		memset_even_4p(start, width*4, cval1, cval2);
	}
    }
}

void fbcon_iplan2p4_putc(struct vc_data *conp, struct display *p, int c,
			 int yy, int xx)
{
    u8 *dest;
    u8 *cdat;
    int rows;
    int bytes = p->next_line;
    u32 eorx, fgx, bgx, fdx;

    if (fontheightlog(p)) {
	dest = (p->screen_base + ((yy * bytes) << fontheightlog(p)) +
		(xx>>1)*8 + (xx & 1));
	cdat = p->fontdata + ((c & p->charmask) << fontheightlog(p));
    } else {
	dest = (p->screen_base + yy * bytes * fontheight(p) +
		(xx>>1)*8 + (xx & 1));
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
    }

    fgx = expand4l(attr_fgcol(p,c));
    bgx = expand4l(attr_bgcol(p,c));
    eorx = fgx ^ bgx;

    for(rows = fontheight(p) ; rows-- ; dest += bytes) {
	fdx = dup4l(*cdat++);
	movepl(dest, (fdx & eorx) ^ bgx);
    }
}

void fbcon_iplan2p4_putcs(struct vc_data *conp, struct display *p,
			  const unsigned short *s, int count, int yy, int xx)
{
    u8 *dest, *dest0;
    u8 *cdat;
    u16 c;
    int rows;
    int bytes;
    u32 eorx, fgx, bgx, fdx;

    bytes = p->next_line;
    if (fontheightlog(p))
	dest0 = (p->screen_base + ((yy * bytes) << fontheightlog(p)) +
		 (xx>>1)*8 + (xx & 1));
    else
	dest0 = (p->screen_base + yy * bytes * fontheight(p) +
		 (xx>>1)*8 + (xx & 1));
    c = scr_readw(s);
    fgx = expand4l(attr_fgcol(p, c));
    bgx = expand4l(attr_bgcol(p, c));
    eorx = fgx ^ bgx;

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
	    movepl(dest, (fdx & eorx) ^ bgx);
	}
	INC_4P(dest0);
    }
}

void fbcon_iplan2p4_revc(struct display *p, int xx, int yy)
{
    u8 *dest;
    int j;
    int bytes;

    if (fontheightlog(p))
	dest = (p->screen_base + ((yy * p->next_line) << fontheightlog(p)) +
		(xx>>1)*8 + (xx & 1));
    else
	dest = (p->screen_base + yy * p->next_line * fontheight(p) +
		(xx>>1)*8 + (xx & 1));
    j = fontheight(p);
    bytes = p->next_line;

    while (j--) {
	/*  This should really obey the individual character's
	 *  background and foreground colors instead of simply
	 *  inverting.
	 */
	dest[0] = ~dest[0];
	dest[2] = ~dest[2];
	dest[4] = ~dest[4];
	dest[6] = ~dest[6];
	dest += bytes;
    }
}

void fbcon_iplan2p4_clear_margins(struct vc_data *conp, struct display *p,
				  int bottom_only)
{
    u32 offset;
    int bytes;
    int lines;
    u32 cval1, cval2;

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
	expand4dl(attr_bgcol_ec(p,conp), &cval1, &cval2);
	memset_even_4p(p->screen_base+offset, lines * bytes, cval1, cval2);
    }
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_iplan2p4 = {
    setup:		fbcon_iplan2p4_setup,
    bmove:		fbcon_iplan2p4_bmove,
    clear:		fbcon_iplan2p4_clear,
    putc:		fbcon_iplan2p4_putc,
    putcs:		fbcon_iplan2p4_putcs,
    revc:		fbcon_iplan2p4_revc,
    clear_margins:	fbcon_iplan2p4_clear_margins,
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

EXPORT_SYMBOL(fbcon_iplan2p4);
EXPORT_SYMBOL(fbcon_iplan2p4_setup);
EXPORT_SYMBOL(fbcon_iplan2p4_bmove);
EXPORT_SYMBOL(fbcon_iplan2p4_clear);
EXPORT_SYMBOL(fbcon_iplan2p4_putc);
EXPORT_SYMBOL(fbcon_iplan2p4_putcs);
EXPORT_SYMBOL(fbcon_iplan2p4_revc);
EXPORT_SYMBOL(fbcon_iplan2p4_clear_margins);
