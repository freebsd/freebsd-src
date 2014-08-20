/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NS_ATARI_PLOT_H
#define NS_ATARI_PLOT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <mint/osbind.h>
#include <mint/cookie.h>
#include <Hermes/Hermes.h>

#include "desktop/plotters.h"
#include "desktop/plot_style.h"
#include "image/bitmap.h"

#include "atari/bitmap.h"
#include "atari/plot/eddi.h"
#include "atari/plot/fontplot.h"

/* how much memory should be kept allocated for temp. conversion bitmaps: */
#define CONV_KEEP_LIMIT 512000
/* how much memory to allocate if some is needed: */
#define CONV_BLOCK_SIZE 32000

/* Plotter Option Flags: */
#define PLOT_FLAG_DITHER        0x04    /* true if the plotter shall dither images */
#define PLOT_FLAG_TRANS         0x08    /* true if the plotter supports transparent operations */

/* Plotter "feature" flags */
#define PLOT_FLAG_HAS_DITHER    0x0400
#define PLOT_FLAG_HAS_ALPHA     0x0800
#define PLOT_FLAG_OFFSCREEN     0x1000  /* offsreen plotter should set this flag */

/* Plotter "internal" flags  */
#define PLOT_FLAG_LOCKED	0x08000 	/* plotter should set this flag during screen updates */

/* Font Plotter flags: */
#define FONTPLOT_FLAG_MONOGLYPH 0x01

/* Flags for init_mfdb function: */
#define MFDB_FLAG_STAND         0x01
#define MFDB_FLAG_ZEROMEM       0x02
#define MFDB_FLAG_NOALLOC       0x04

/* Flags for blit functions: */
#define BITMAPF_MONOGLYPH       4096	/* The bitmap is an character bitmap  */
#define BITMAPF_BUFFER_NATIVE   8192	/* Bitmap shall be kept converted     */

/* Error codes: */
#define ERR_BUFFERSIZE_EXCEEDS_SCREEN 1	/* The buffer allocated is larger than the screen */
#define ERR_NO_MEM 2					/* Not enough memory for requested operation */
#define ERR_PLOTTER_NOT_AVAILABLE 3		/* invalid plotter driver name passed */

struct s_vdi_sysinfo {
    short vdi_handle;               /* vdi handle 					*/
    short scr_w;					/* resolution horz.             */
    short scr_h;					/* resolution vert.             */
    short scr_bpp;					/* bits per pixel              	*/
    int colors;						/* 0=hiclor, 2=mono           	*/
    unsigned long hicolors;         /* if colors = 0              	*/
    short pixelsize;				/* bytes per pixel            	*/
    unsigned short pitch;			/* row pitch                  	*/
    unsigned short vdiformat;       /* pixel format               	*/
    unsigned short clut;			/* type of clut support       	*/
    void * screen;					/* pointer to screen, or NULL 	*/
    unsigned long  screensize;		/* size of screen (in bytes)  	*/
    unsigned long  mask_r;          /* color masks                	*/
    unsigned long  mask_g;
    unsigned long  mask_b;
    unsigned long  mask_a;
    short maxintin;					/* maximum pxy items            */
    short maxpolycoords;			/* max coords for p_line etc.   */
    unsigned long EdDiVersion;		/* EdDi Version or 0            */
    bool rasterscale;				/* raster scaling support       */
};

struct rect;

extern const struct plotter_table atari_plotters;

int plot_init(char *);
int plot_finalise(void);
/* translate an error number */
const char* plot_err_str(int i) ;

bool plot_lock(void);
bool plot_unlock(void);
bool plot_set_dimensions( int x, int y, int w, int h );
bool plot_get_dimensions(GRECT *dst);
float plot_get_scale(void);
float plot_set_scale(float);
void plot_set_abs_clipping(const GRECT *area);
void plot_get_abs_clipping(struct rect *dst);
void plot_get_abs_clipping_grect(GRECT *dst);
bool plot_get_clip(struct rect * out);
/* Get clipping for current framebuffer as GRECT */
void plot_get_clip_grect(GRECT * out);
bool plot_clip(const struct rect *clip);
VdiHdl plot_get_vdi_handle(void);
long plot_get_flags(void);
bool plot_rectangle( int x0, int y0, int x1, int y1,const plot_style_t *style );
bool plot_line( int x0, int y0, int x1, int y1, const plot_style_t *style );
bool plot_blit_bitmap(struct bitmap * bmp, int x, int y,
                      unsigned long bg, unsigned long flags);
bool plot_blit_mfdb(GRECT * loc, MFDB * insrc, short fgcolor,
						uint32_t flags);
bool plot_copy_rect(GRECT src, GRECT dst);

/* convert an vdi color to bgra */
void vdi1000_to_rgb( unsigned short * in, unsigned char * out );

/* convert an bgra color to vdi1000 color */
void rgb_to_vdi1000( unsigned char * in, RGB1000 *out);

/* convert an rgb color to an index into the web palette */
short rgb_to_666_index(unsigned char r, unsigned char g, unsigned char b);

/* assign vdi line style to dst ( netsurf type ) */
#define NSLT2VDI(dst, src) \
	dst = 0;\
	switch( src->stroke_type ) {\
		case PLOT_OP_TYPE_DOT: \
			dst = (0xAAAA00 | 7);\
		break;\
		case PLOT_OP_TYPE_DASH:\
			dst = 3;	\
		break;\
		case PLOT_OP_TYPE_SOLID:\
		case PLOT_OP_TYPE_NONE:\
		default:\
			dst = 1;\
		break;\
	}\



#ifdef WITH_8BPP_SUPPORT
/* some Well known indexes into the VDI palette */
/* common indexes into the VDI palette */
/* (only used when running with 256 colors or less ) */
#define OFFSET_WEB_PAL 16
#define OFFSET_CUST_PAL 232
#define RGB_TO_VDI(c) rgb_to_666_index( (c&0xFF),(c&0xFF00)>>8,(c&0xFF0000)>>16)+OFFSET_WEB_PAL
#endif

/*      the name of this macro is crap - it should be named bgr_to_rgba ... or so */
#define ABGR_TO_RGB(c)  ( ((c&0xFF)<<16) | (c&0xFF00) | ((c&0xFF0000)>>16) ) << 8
/* this index into the palette is used by the TC renderer to set current draw color: */
#define OFFSET_CUSTOM_COLOR 255

#endif
