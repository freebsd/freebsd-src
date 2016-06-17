/*
 *  linux/drivers/video/fbcon-iplan2p2.c -- Low level frame buffer operations
 *				  for interleaved bitplanes à la Atari (2
 *				  planes, 2 bytes interleave)
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
#include <video/fbcon-iplan2p2.h>


    /*
     *  Interleaved bitplanes à la Atari (2 planes, 2 bytes interleave)
     */

/* Increment/decrement 2 plane addresses */

#define	INC_2P(p)	do { if (!((long)(++(p)) & 1)) (p) += 2; } while(0)
#define	DEC_2P(p)	do { if ((long)(--(p)) & 1) (p) -= 2; } while(0)

    /*  Convert a standard 4 bit color to our 2 bit color assignment:
     *  If at least two RGB channels are active, the low bit is turned on;
     *  The intensity bit (b3) is shifted into b1.
     */

static const u8 color_2p[] = { 0, 0, 0, 1, 0, 1, 1, 1, 2, 2, 2, 3, 2, 3, 3, 3 };
#define	COLOR_2P(c)	color_2p[c]

/* Perform the m68k movepw operation.  */
static inline void movepw(u8 *d, u16 val)
{
#if defined __mc68000__ && !defined CPU_M68060_ONLY
    asm volatile ("movepw %1,%0@(0)" : : "a" (d), "d" (val));
#else
    d[0] = (val >> 16) & 0xff;
    d[2] = val & 0xff;
#endif
}

/* Sets the bytes in the visible column at d, height h, to the value
 * val for a 2 plane screen. The bits of the color in 'color' are
 * moved (8 times) to the respective bytes. This means:
 *
 * for(h times; d += bpr)
 *   *d     = (color & 1) ? 0xff : 0;
 *   *(d+2) = (color & 2) ? 0xff : 0;
 */

static __inline__ void memclear_2p_col(void *d, size_t h, u16 val, int bpr)
{
    u8 *dd = d;
    do {
	movepw(dd, val);
	dd += bpr;
    } while (--h);
}

/* Sets a 2 plane region from 'd', length 'count' bytes, to the color
 * in val1. 'd' has to be an even address and count must be divisible
 * by 8, because only whole words and all planes are accessed. I.e.:
 *
 * for(count/4 times)
 *   *d     = *(d+1) = (color & 1) ? 0xff : 0;
 *   *(d+2) = *(d+3) = (color & 2) ? 0xff : 0;
 */

static __inline__ void memset_even_2p(void *d, size_t count, u32 val)
{
    u32 *dd = d;

    count /= 4;
    while (count--)
	*dd++ = val;
}

/* Copies a 2 plane column from 's', height 'h', to 'd'. */

static __inline__ void memmove_2p_col (void *d, void *s, int h, int bpr)
{
    u8 *dd = d, *ss = s;

    while (h--) {
	dd[0] = ss[0];
	dd[2] = ss[2];
	dd += bpr;
	ss += bpr;
    }
}


/* This expands a 2 bit color into a short for movepw (2 plane) operations. */

static const u16 two2byte[] = {
    0x0000, 0xff00, 0x00ff, 0xffff
};

static __inline__ u16 expand2w(u8 c)
{
    return two2byte[c];
}


/* This expands a 2 bit color into one long for a movel operation
 * (2 planes).
 */

static const u32 two2word[] = {
#ifndef __LITTLE_ENDIAN
    0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff
#else
    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff
#endif
};

static __inline__ u32 expand2l(u8 c)
{
    return two2word[c];
}


/* This duplicates a byte 2 times into a short. */

static __inline__ u16 dup2w(u8 c)
{
    u16 rv;

    rv = c;
    rv |= c << 8;
    return rv;
}


void fbcon_iplan2p2_setup(struct display *p)
{
    p->next_line = p->var.xres_virtual>>2;
    p->next_plane = 2;
}

void fbcon_iplan2p2_bmove(struct display *p, int sy, int sx, int dy, int dx,
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

    if (sx == 0 && dx == 0 && width * 2 == p->next_line) {
	/*  Special (but often used) case: Moving whole lines can be
	 *  done with memmove()
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
		src = p->screen_base + sy * linesize + (sx>>1)*4 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*4 + (dx & 1);
		if (sx & 1) {
		    memmove_2p_col(dst, src, colsize, bytes);
		    src += 3;
		    dst += 3;
		    --width;
		}
		if (width > 1) {
		    for (rows = colsize; rows > 0; --rows) {
			fb_memmove(dst, src, (width>>1)*4);
			src += bytes;
			dst += bytes;
		    }
		}
		if (width & 1) {
		    src -= colsize * bytes;
		    dst -= colsize * bytes;
		    memmove_2p_col(dst + (width>>1)*4, src + (width>>1)*4,
				   colsize, bytes);
		}
	    } else {
		if (!((sx+width-1) & 1)) {
		    src = p->screen_base + sy * linesize + ((sx+width-1)>>1)*4;
		    dst = p->screen_base + dy * linesize + ((dx+width-1)>>1)*4;
		    memmove_2p_col(dst, src, colsize, bytes);
		    --width;
		}
		src = p->screen_base + sy * linesize + (sx>>1)*4 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*4 + (dx & 1);
		if (width > 1) {
		    src += colsize * bytes + (sx & 1)*3;
		    dst += colsize * bytes + (sx & 1)*3;
		    for(rows = colsize; rows > 0; --rows) {
			src -= bytes;
			dst -= bytes;
			fb_memmove(dst, src, (width>>1)*4);
		    }
		}
		if (width & 1)
		    memmove_2p_col(dst-3, src-3, colsize, bytes);
	    }
	} else {
	    /* odd->even or even->odd */
	    if (upwards) {
		src = p->screen_base + sy * linesize + (sx>>1)*4 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*4 + (dx & 1);
		for (cols = width; cols > 0; --cols) {
		    memmove_2p_col(dst, src, colsize, bytes);
		    INC_2P(src);
		    INC_2P(dst);
		}
	    } else {
		sx += width-1;
		dx += width-1;
		src = p->screen_base + sy * linesize + (sx>>1)*4 + (sx & 1);
		dst = p->screen_base + dy * linesize + (dx>>1)*4 + (dx & 1);
		for(cols = width; cols > 0; --cols) {
		    memmove_2p_col(dst, src, colsize, bytes);
		    DEC_2P(src);
		    DEC_2P(dst);
		}
	    }
	}
    }
}

void fbcon_iplan2p2_clear(struct vc_data *conp, struct display *p, int sy,
			  int sx, int height, int width)
{
    u32 offset;
    u8 *start;
    int rows;
    int bytes = p->next_line;
    int lines;
    u32 size;
    u32 cval;
    u16 pcval;

    cval = expand2l (COLOR_2P (attr_bgcol_ec(p,conp)));

    if (fontheightlog(p))
	lines = height << fontheightlog(p);
    else
	lines = height * fontheight(p);

    if (sx == 0 && width * 2 == bytes) {
	if (fontheightlog(p))
	    offset = (sy * bytes) << fontheightlog(p);
	else
	    offset = sy * bytes * fontheight(p);
	size = lines * bytes;
	memset_even_2p(p->screen_base+offset, size, cval);
    } else {
	if (fontheightlog(p))
	    offset = ((sy * bytes) << fontheightlog(p)) + (sx>>1)*4 + (sx & 1);
	else
	    offset = sy * bytes * fontheight(p) + (sx>>1)*4 + (sx & 1);
	start = p->screen_base + offset;
	pcval = expand2w(COLOR_2P(attr_bgcol_ec(p,conp)));

	/*  Clears are split if the region starts at an odd column or
	 *  end at an even column. These extra columns are spread
	 *  across the interleaved planes. All in between can be
	 *  cleared by normal fb_memclear_small(), because both bytes of
	 *  the single plane words are affected.
	 */

	if (sx & 1) {
	    memclear_2p_col(start, lines, pcval, bytes);
	    start += 3;
	    width--;
	}
	if (width & 1) {
	    memclear_2p_col(start + (width>>1)*4, lines, pcval, bytes);
	    width--;
	}
	if (width) {
	    for (rows = lines; rows-- ; start += bytes)
	    memset_even_2p(start, width*2, cval);
	}
    }
}

void fbcon_iplan2p2_putc(struct vc_data *conp, struct display *p, int c,
			 int yy, int xx)
{
    u8 *dest;
    u8 *cdat;
    int rows;
    int bytes = p->next_line;
    u16 eorx, fgx, bgx, fdx;

    if (fontheightlog(p)) {
	dest = (p->screen_base + ((yy * bytes) << fontheightlog(p)) +
		(xx>>1)*4 + (xx & 1));
	cdat = p->fontdata + ((c & p->charmask) << fontheightlog(p));
    } else {
	dest = (p->screen_base + yy * bytes * fontheight(p) +
		(xx>>1)*4 + (xx & 1));
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
    }

    fgx = expand2w(COLOR_2P(attr_fgcol(p,c)));
    bgx = expand2w(COLOR_2P(attr_bgcol(p,c)));
    eorx = fgx ^ bgx;

    for (rows = fontheight(p) ; rows-- ; dest += bytes) {
	fdx = dup2w(*cdat++);
	movepw(dest, (fdx & eorx) ^ bgx);
    }
}

void fbcon_iplan2p2_putcs(struct vc_data *conp, struct display *p,
			  const unsigned short *s, int count, int yy, int xx)
{
    u8 *dest, *dest0;
    u8 *cdat;
    u16 c;
    int rows;
    int bytes;
    u16 eorx, fgx, bgx, fdx;

    bytes = p->next_line;
    if (fontheightlog(p))
	dest0 = (p->screen_base + ((yy * bytes) << fontheightlog(p)) +
		 (xx>>1)*4 + (xx & 1));
    else
	dest0 = (p->screen_base + yy * bytes * fontheight(p) +
		 (xx>>1)*4 + (xx & 1));
    c = scr_readw(s);
    fgx = expand2w(COLOR_2P(attr_fgcol(p, c)));
    bgx = expand2w(COLOR_2P(attr_bgcol(p, c)));
    eorx = fgx ^ bgx;

    while (count--) {
	c = scr_readw(s++) & p->charmask;
	if (fontheightlog(p))
	    cdat = p->fontdata + (c << fontheightlog(p));
	else
	    cdat = p->fontdata + c * fontheight(p);

	for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
	    fdx = dup2w(*cdat++);
	    movepw(dest, (fdx & eorx) ^ bgx);
	}
	INC_2P(dest0);
    }
}

void fbcon_iplan2p2_revc(struct display *p, int xx, int yy)
{
    u8 *dest;
    int j;
    int bytes;

    if (fontheightlog(p))
	dest = (p->screen_base + ((yy * p->next_line) << fontheightlog(p)) +
		(xx>>1)*4 + (xx & 1));
    else
	dest = (p->screen_base + yy * p->next_line * fontheight(p) +
		(xx>>1)*4 + (xx & 1));
    j = fontheight(p);
    bytes = p->next_line;
    while (j--) {
	/*  This should really obey the individual character's
	 *  background and foreground colors instead of simply
	 *  inverting.
	 */
	dest[0] = ~dest[0];
	dest[2] = ~dest[2];
	dest += bytes;
    }
}

void fbcon_iplan2p2_clear_margins(struct vc_data *conp, struct display *p,
				  int bottom_only)
{
    u32 offset;
    int bytes;
    int lines;
    u32 cval;

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
	cval = expand2l(COLOR_2P(attr_bgcol_ec(p,conp)));
	memset_even_2p(p->screen_base+offset, lines * bytes, cval);
    }
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_iplan2p2 = {
    setup:		fbcon_iplan2p2_setup,
    bmove:		fbcon_iplan2p2_bmove,
    clear:		fbcon_iplan2p2_clear,
    putc:		fbcon_iplan2p2_putc,
    putcs:		fbcon_iplan2p2_putcs,
    revc:		fbcon_iplan2p2_revc,
    clear_margins:	fbcon_iplan2p2_clear_margins,
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

EXPORT_SYMBOL(fbcon_iplan2p2);
EXPORT_SYMBOL(fbcon_iplan2p2_setup);
EXPORT_SYMBOL(fbcon_iplan2p2_bmove);
EXPORT_SYMBOL(fbcon_iplan2p2_clear);
EXPORT_SYMBOL(fbcon_iplan2p2_putc);
EXPORT_SYMBOL(fbcon_iplan2p2_putcs);
EXPORT_SYMBOL(fbcon_iplan2p2_revc);
EXPORT_SYMBOL(fbcon_iplan2p2_clear_margins);
