/*-
 * Copyright (c) 2012 Simon W. Moore
 * Copyright (c) 2012-2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <stdbool.h>
// endian.h not available in Linux?
// #include <sys/endian.h>
#include <sys/mman.h>
#define PNG_DEBUG 3
#include <png.h>

#include "terasic_mtl.h"

// file descriptors for MTL control and display regions
static int ctrlfd;
static int dispfd;
static int textfd;
static int fademode=0;
volatile u_int32_t *pfbp;
static volatile u_int16_t *tfbp;
volatile u_int32_t *mtlctrl;
static struct tsstate *sp = NULL;

// fade timing (for crude timing loop)
static const int fb_cross_fade_time = 500;

// number of lines in the line pattern
static const int num_lines_pattern = 600;

const int fb_height = 480;
const int fb_width = 800;


/*****************************************************************************
 * hack around endian issue
 * TODO: replace with endian library call (but not present in Linux?)
 *****************************************************************************/

static u_int32_t
endian_swap(u_int32_t lend)
{
  u_int32_t bend;
  bend = lend & 0xff;
  bend = bend<<8;
  lend = lend>>8;

  bend |= lend & 0xff;
  bend = bend<<8;
  lend = lend>>8;

  bend |= lend & 0xff;
  bend = bend<<8;
  lend = lend>>8;

  bend |= lend & 0xff;
  return bend;
}



/*****************************************************************************
 * sample touch input
 *****************************************************************************/

int touch_x0=0;
int touch_y0=0;
int touch_x1=0;
int touch_y1=0;
int touch_gesture=0;
int touch_count=0;

void
multitouch_pole(void)
{
  int t_x0 = endian_swap(mtlctrl[3]);
  if(t_x0>=0) { // new touch info available
    touch_x0 = t_x0;
    touch_y0 = endian_swap(mtlctrl[4]);
    touch_x1 = endian_swap(mtlctrl[5]);
    touch_y1 = endian_swap(mtlctrl[6]);
    // note that this final read dequeues
    touch_gesture = endian_swap(mtlctrl[7]);
    touch_count = touch_gesture>>8;
    if(touch_count<0) touch_count=0; // hack
    touch_gesture &= 0xff;
  }
  // else
  //  t_x0 = mtlctrl[7]; // clear any -1s from FIFO?
}


// filter out short lived touch releases
void
multitouch_filter(void)
{
  int j;
  multitouch_pole();
  for(j=30000; (j>0) && (touch_count==0); j--)
    multitouch_pole();
}

// wait for touch release
void
multitouch_release_event(void)
{
  do {
    // multitouch_filter();
    multitouch_pole();
  } while(touch_count!=0);
}

/*****************************************************************************
 * Revised touch screen polling interface
 *****************************************************************************/

int
tsg2tsgf(int g)
{

	switch (g) {
	case TSG_NONE:		return (TSGF_NONE);
	case TSG_NORTH:		return (TSGF_NORTH);
	case TSG_NORTHEAST:	return (TSGF_NORTHEAST);
	case TSG_EAST:		return (TSGF_EAST);
	case TSG_SOUTHEAST:	return (TSGF_SOUTHEAST);
	case TSG_SOUTH:		return (TSGF_SOUTH);
	case TSG_SOUTHWEST:	return (TSGF_SOUTHWEST);
	case TSG_WEST:		return (TSGF_WEST);
	case TSG_NORTHWEST:	return (TSGF_NORTHWEST);
	case TSG_ROTATE_CW:	return (TSGF_ROTATE_CW);
	case TSG_ROTATE_CCW:	return (TSGF_ROTATE_CCW);
	case TSG_CLICK:		return (TSGF_CLICK);
	case TSG_DCLICK:	return (TSGF_DCLICK);
	case TSG2_NORTH:	return (TSGF_NORTH);
	case TSG2_NORTHEAST:	return (TSGF_NORTHEAST);
	case TSG2_EAST:		return (TSGF_EAST);
	case TSG2_SOUTHEAST:	return (TSGF_SOUTHEAST);
	case TSG2_SOUTH:	return (TSGF_SOUTH);
	case TSG2_SOUTHWEST:	return (TSGF_SOUTHWEST);
	case TSG2_WEST:		return (TSGF_WEST);
	case TSG2_NORTHWEST:	return (TSGF_NORTHWEST);
	case TSG2_CLICK:	return (TSGF_CLICK);
	case TSG2_ZOOM_IN:	return (TSGF_ZOOM_IN);
	case TSG2_ZOOM_OUT:	return (TSGF_ZOOM_OUT);
	}
	errx(1, "tsg2tsgf called with invalid gesture 0x%x", g);
}

int
tsgf2tsg(int f)
{

	switch (f) {
	case TSGF_NONE:		return (TSG_NONE);
	case TSGF_NORTH:	return (TSG_NORTH);
	case TSGF_NORTHEAST:	return (TSG_NORTHEAST);
	case TSGF_EAST:		return (TSG_EAST);
	case TSGF_SOUTHEAST:	return (TSG_SOUTHEAST);
	case TSGF_SOUTH:	return (TSG_SOUTH);
	case TSGF_SOUTHWEST:	return (TSG_SOUTHWEST);
	case TSGF_WEST:		return (TSG_WEST);
	case TSGF_NORTHWEST:	return (TSG_NORTHWEST);
	case TSGF_ROTATE_CW:	return (TSG_ROTATE_CW);
	case TSGF_ROTATE_CCW:	return (TSG_ROTATE_CCW);
	case TSGF_CLICK:	return (TSG_CLICK);
	case TSGF_DCLICK:	return (TSG_DCLICK);
	case TSGF_2NORTH:	return (TSG2_NORTH);
	case TSGF_2NORTHEAST:	return (TSG2_NORTHEAST);
	case TSGF_2EAST:	return (TSG2_EAST);
	case TSGF_2SOUTHEAST:	return (TSG2_SOUTHEAST);
	case TSGF_2SOUTH:	return (TSG2_SOUTH);
	case TSGF_2SOUTHWEST:	return (TSG2_SOUTHWEST);
	case TSGF_2WEST:	return (TSG2_WEST);
	case TSGF_2NORTHWEST:	return (TSG2_NORTHWEST);
	case TSGF_2CLICK:	return (TSG2_CLICK);
	case TSGF_ZOOM_IN:	return (TSG2_ZOOM_IN);
	case TSGF_ZOOM_OUT:	return (TSG2_ZOOM_OUT);
	}
	errx(1, "tsg2tsgf called with invalid flag 0x%x", f);
}

struct tsstate*
ts_poll(int timeout)
{
        struct timespec stime = {0, 1000000};
        int init = 0, first_pass = 1;
        int check_release = 0;
        struct tsstate tmp_s, rel_s;
	int loops = 0;

        if (sp == NULL) {
                sp = malloc(sizeof(struct tsstate));
                if (sp == NULL)
                        err(1, "malloc of tstate");
                init = 1;
        }

        for (;;) {
		if (timeout != 0 && !check_release && loops > timeout) {
			/*
			 * If we have timed out and aren't waiting for a
			 * release then return an empty gesture.
			 */
			sp->ts_count = 0;
			sp->ts_gesture = 0;
			return (sp);
		}
                tmp_s.ts_x1 = le32toh(mtlctrl[3]);
                tmp_s.ts_y1 = le32toh(mtlctrl[4]);
                tmp_s.ts_x2 = le32toh(mtlctrl[5]);
                tmp_s.ts_y2 = le32toh(mtlctrl[6]);
                tmp_s.ts_gesture = le32toh(mtlctrl[7]);
                if (tmp_s.ts_gesture < 0) {
			if (check_release) {
				check_release = 0;
				*sp = rel_s;
				return (sp);
			}
			if (first_pass && !init && sp->ts_count > 0) {
				/*
				 * If we returned a touch last time around
				 * then fake up a release now.
				 * XXX: we should probably have a timelimit
				 */
				sp->ts_count = 0;
				sp->ts_gesture = 0;
				return(sp);
			}
			first_pass = 0;
                        nanosleep(&stime, NULL);
			loops++;
                        continue;
                }
                tmp_s.ts_count = tmp_s.ts_gesture >> 8;
                tmp_s.ts_gesture &= 0xFF;
 
               if (init ||
                    tmp_s.ts_x1 != sp->ts_x1 || tmp_s.ts_y1 != sp->ts_y1 ||
                    tmp_s.ts_x2 != sp->ts_x2 || tmp_s.ts_y2 != sp->ts_y2 ||
                    tmp_s.ts_count != sp->ts_count ||
                    tmp_s.ts_gesture != sp->ts_gesture) {
			/*
			 * If we get an release event, differ returning
			 * it until we sleep and get a non-event.
			 */
			if (tmp_s.ts_count == 0) {
				check_release = 1;
				rel_s = tmp_s;
			} else {
				*sp = tmp_s;
				return (sp);
			}
                }
		first_pass = 0;
                nanosleep(&stime, NULL);
		loops++;
        }
}


void
ts_drain(void)
{
        struct timespec stime = {0, 1000000};
	int noprevtouch = 0;

	if (sp == NULL || sp->ts_count == 0)
		noprevtouch = 1;

	for (;;) {
                nanosleep(&stime, NULL);
		if ((int32_t)le32toh(mtlctrl[7]) < 0) {
			if (noprevtouch)
				return;
			else
				noprevtouch++;
		}
	}
}


/*****************************************************************************
 * frame buffer routines
 * TODO: put in seperate library
 *****************************************************************************/

void
fb_init(void)
{
  ctrlfd = open("/dev/mtl_reg0", O_RDWR | O_NONBLOCK);
  if(ctrlfd < 0)
    err(1, "open mtl_reg0");

  mtlctrl = mmap(NULL, 0x20, PROT_READ | PROT_WRITE, MAP_SHARED, ctrlfd, 0);
  if (mtlctrl == MAP_FAILED)
    err(1, "mmap mtl_reg0");

  dispfd = open("/dev/mtl_pixel0", O_RDWR | O_NONBLOCK);
  if(dispfd < 0)
    err(1, "open mtl_pixel0");
  pfbp = mmap(NULL, 0x177000, PROT_READ | PROT_WRITE, MAP_SHARED, dispfd, 0);
  if (pfbp == MAP_FAILED)
    err(1, "mmap mtl_pixel0");

  textfd = open("/dev/mtl_text0", O_RDWR | O_NONBLOCK);
  if(textfd < 0)
    err(1, "open mtl_text0");
  tfbp = mmap(NULL, 100*40*2, PROT_READ | PROT_WRITE, MAP_SHARED, textfd, 0);
  if (tfbp == MAP_FAILED)
    err(1, "mmap mtl_text0");
}


void
fb_fini(void)
{
  close(ctrlfd);
  close(dispfd);
  close(textfd);
}



inline u_int32_t
fb_colour(int r, int g, int b)
{
  return ((r&0xff)<<8) | ((g&0xff)<<16) | ((b&0xff)<<24);
}


inline void
fb_putpixel(int px, int py, int colour)
{
  pfbp[px+py*fb_width] = colour;
}


void
fb_fill(int col)
{
  int addr;
  for(addr=0; addr<(fb_height*fb_width); addr++)
    pfbp[addr] = col;
}

int
fb_composite(u_int32_t *dbuf, int dwidth, int dheight, int x, int y,
    const u_int32_t *sbuf, int swidth, int sheight)
{
  int col, row;

  if (dwidth - x < swidth || dheight - y < sheight)
    return (-1);

  for (row = 0; row < sheight; row++)
    for (col = 0; col < swidth; col++)
      dbuf[(y + row) * dwidth + (x + col)] = sbuf[row * swidth + col];

  return (0);
}

void
fb_rectangle(u_int32_t color, int thickness, int x, int y, int w, int h)
{
  int col, row;

  for (row = 0; row < h; row++) {
    if (row < thickness || row >= h - thickness)
      for (col = 0; col < w; col++)
        pfbp[(y + row) * fb_width + (x + col)] = color;
    else {
      for (col = 0; col < thickness; col++) {
        pfbp[(y + row) * fb_width + (x + col)] = color;
        pfbp[(y + row) * fb_width + (x + col + w - thickness)] = color;
      }
    }
  }
}

void
fb_fill_region(u_int32_t colour, int x, int y, int w, int h)
{
  int col, row;

  for (row = 0; row < h; row++)
    for (col = 0; col < w; col++)
      pfbp[(y + row) * fb_width + (x + col)] = colour;
}


void
fb_fill_buf(u_int32_t *buf, u_int32_t color, int width, int height)
{
  int i;

  for (i = 0; i < width * height; i++)
    buf[i] = color;
}


void
fb_post(u_int32_t *buf)
{
  int addr;
  for(addr=0; addr<(fb_height*fb_width); addr++)
    pfbp[addr] = buf[addr];
}


void
fb_post_region(u_int32_t *buf, int x, int y, int w, int h)
{
  int col, row;

  for (row = 0; row < h; row++)
    for (col = 0; col < w; col++)
      pfbp[(y + row) * fb_width + (x + col)] = buf[row * w + col];
}


void
fb_save(u_int32_t *buf)
{
  int i;

  for (i = 0; i < fb_height * fb_width; i++)
    buf[i] = pfbp[i];
}


void
fb_blend(int blend_text_bg, int blend_text_fg, int blend_pixel, int wash __unused)
{
  mtlctrl[0] =
    ((blend_text_bg & 0xff)<<24) |
    ((blend_text_fg & 0xff)<<16) |
    ((blend_pixel   & 0xff)<<8)  |
    // to avoid a red screen colour wash "attack" indicator from being
    // removed, preserve the "wash" value:
    //wash;
    (mtlctrl[0] & 0xef); // clear TERASIC_MTL_BLEND_PIXEL_ENDIAN_SWAP bit
    // to try the dark red "attack" indicator:
    //4;
}


void
fb_text_cursor(int x, int y)
{
  mtlctrl[1] = ((y&0xff)<<24) | ((x)<<16);
}


// fade the pixel framebuffer to black using the MTL hardware alpha blending
void
fb_fade2off(void)
{
  int b,t;
  if(fademode==0)
    fb_blend(255,255,255,0);
  else
    for(b=0; b<256; b++)
      for(t=fb_cross_fade_time; t>0; t--) 
	fb_blend(b,b,255,0);
  fademode=0;
}


// fade the pixel framebuffer from black using the MTL hardware alpha blending
void
fb_fade2on(void)
{
  int b,t;
  if(fademode==1)
    fb_blend(0,0,255,0);
  else
    for(b=0; b<256; b++)
      for(t=fb_cross_fade_time; t>0; t--)
	fb_blend(255-b,255-b,255,0);
  fademode=1;
}


void
fb_fade2text(int textbg_alpha)
{
  int b, t;
  if(fademode==2)
    fb_blend(255,255,0,0);
  else
    for(b=0; b<256; b++)
      for(t=fb_cross_fade_time; t>0; t--)
	fb_blend((b<textbg_alpha) ? b : textbg_alpha,b,0,0);
  fademode=2;
}


/*****************************************************************************
 *  plot_line
 *    draws a line using Bresenham's line-drawing algorithm, which uses
 *    no multiplication or division.
 *****************************************************************************/

static inline int
sgn(int j)
{
  return j==0 ? 0 : ((j<0) ? -1 : 1);
}

void
plot_line(int x1, int y1, int x2, int y2, unsigned int colour)
{
  int i,dx,dy,sdx,sdy,dxabs,dyabs,x,y,px,py;
  dx=x2-x1;      /* the horizontal distance of the line */
  dy=y2-y1;      /* the vertical distance of the line */
  dxabs=abs(dx);
  dyabs=abs(dy);
  sdx=sgn(dx);
  sdy=sgn(dy);
  x=dyabs>>1;
  y=dxabs>>1;
  px=x1;
  py=y1;

  if((x1==x2) && (y1==y2))
    fb_putpixel(x1,y1,colour);
  else if (dxabs>=dyabs) { /* the line is more horizontal than vertical */
    for(i=0;i<dxabs;i++) {
      y+=dyabs;
      if (y>=dxabs) {
        y-=dxabs;
        py+=sdy;
      }
      px+=sdx;
      fb_putpixel(px,py,colour);
    }
  } else { /* the line is more vertical than horizontal */
    for(i=0;i<dyabs;i++) {
      x+=dxabs;
      if (x>=dyabs) {
        x-=dyabs;
        px+=sdx;
      }
      py+=sdy;
      fb_putpixel(px,py,colour);
    }
  }
}


/*****************************************************************************
 * PNG image loader
 *****************************************************************************/

int
read_png_file(const char* file_name, u_int32_t* imgbuf, int maxwidth, int maxheight)
{
	int fd, ret;

	fd = open(file_name, O_RDONLY);
	if (fd < 0)
		return(-1);
	ret = read_png_fd(fd, imgbuf, maxwidth, maxheight);
	/* read_png_fd() closes the file */
	return (ret);
}

int
read_png_fd(int fd, u_int32_t* imgbuf, int maxwidth, int maxheight)
{
  unsigned char header[8];    // 8 is the maximum size that can be checked
  size_t tmp;
  int x,y;

  int width, height, rowbytes;
  png_byte colour_type;
  png_byte bit_depth;

  png_structp png_ptr;
  png_infop info_ptr;
  int number_of_passes;
  png_bytep * row_pointers;
  int bppx; // bytes per pixel

  /* open file and test for it being a png */
  FILE *fp = fdopen(fd, "rb");
  if (!fp)
    return (-1);
  tmp=fread(header, 1, 8, fp);
  if (png_sig_cmp(header, 0, 8))
    return (-1);
  
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    return (-1);
  
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    return (-1);
  
  if (setjmp(png_jmpbuf(png_ptr)))
    return (-1);
  
  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  
  png_read_info(png_ptr, info_ptr);
  
  width = png_get_image_width(png_ptr, info_ptr);
  height = png_get_image_height(png_ptr, info_ptr);
  colour_type = png_get_color_type(png_ptr, info_ptr);
  bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  if((colour_type != PNG_COLOR_TYPE_RGB) && (colour_type != 6))
    return (-1);
  if(bit_depth != 8)
    return (-1);
  
  number_of_passes = png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);

  /* read file */
  if (setjmp(png_jmpbuf(png_ptr)))
    return (-1);
  
  row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
  
  if (bit_depth == 16)
    rowbytes = width*8;
  else
    rowbytes = width*4;
  
  for (y=0; y<height; y++)
    row_pointers[y] = (png_byte*) malloc(rowbytes);
  
  png_read_image(png_ptr, row_pointers);
  
  fclose(fp);

  // check that the image isn't too big
  if(height>maxheight) height=maxheight;
  if(width>maxwidth) width=maxwidth;
  
  bppx = 3;
  if(colour_type==6)
    bppx = 4;
  for (y=0; y<height; y++) {
    png_byte* row = row_pointers[y];
    for (x=0; x<width; x++) {
      //png_byte* ptr = &(row[x*4]);
      //fb_putpixel(x,y,fb_colour(ptr[0],ptr[1],ptr[2]));
      png_byte r = row[x*bppx+0];
      png_byte g = row[x*bppx+1];
      png_byte b = row[x*bppx+2];
      imgbuf[x+y*maxwidth] = fb_colour(r,g,b);
    }
    // if the image is too small, fill with black
    for(x=width; x<maxwidth; x++)
      imgbuf[x+y*maxwidth] = 0;
  }
  // if the image is too small, fill with black
  for(y=height; y<maxheight; y++)
    for(x=0; x<maxwidth; x++)
      imgbuf[x+y*maxwidth] = 0;

  return (0);
}


/*****************************************************************************
 * Busy indicator for startup sequence, etc
 *****************************************************************************/

static int busy_indicator_state = -1;
static u_int32_t* busy_indicator_imgs[2];

void
busy_indicator(void)
{
  int x0 = (fb_width-64)/2;
  int y0 = (fb_height-64)/2;

  if(busy_indicator_state<0) { // initialisation phase
    fb_fill(fb_colour(0,0,0));
    busy_indicator_imgs[0] = malloc(sizeof(u_int32_t) * 64 * 64);
    busy_indicator_imgs[1] = malloc(sizeof(u_int32_t) * 64 * 64);
    read_png_file("/usr/share/images/busy0.png", busy_indicator_imgs[0], 64, 64);
    read_png_file("/usr/share/images/busy1.png", busy_indicator_imgs[1], 64, 64);
    busy_indicator_state = 0;
  }
  busy_indicator_state = (busy_indicator_state+1) & 0x1;
  fb_post_region(busy_indicator_imgs[busy_indicator_state], x0, y0, 64, 64);
}

void
fb_progress_bar(int x, int y, int w, int h, int fill, int border_width, 
    u_int32_t fill_color, u_int32_t empty_color, u_int32_t border_color)
{

	assert(x >= 0);
	assert(y >= 0);
	assert(fill >= 0);
	assert(fill <= w - border_width * 2);
	assert(x + w <= fb_width);
	assert(y + h <= fb_height);
	assert(w > border_width * 2);
	assert(h > border_width * 2);

	if (border_width > 0) {
		fb_fill_region(border_color, x, y, w, border_width);
		fb_fill_region(border_color, x, y + (h - border_width), w,
		    border_width);
		fb_fill_region(border_color, x, y + border_width,
		    border_width, h - border_width * 2);
		fb_fill_region(border_color, x + (w - border_width),
		    y + border_width, border_width, h - border_width * 2);
	}
	if (fill > 0)
		fb_fill_region(fill_color, x + border_width, y + border_width,
		    fill, h - border_width * 2);
	if (fill < w - border_width * 2)
		fb_fill_region(empty_color, x + border_width + fill,
		    y + border_width, (w - border_width * 2) - fill,
		    h - border_width * 2);
}

#define	FBD_BORDER_LWIDTH	2
#define FBD_BORDER_SPACE	3
#define	FBD_BORDER_WIDTH	(FBD_BORDER_LWIDTH + FBD_BORDER_SPACE * 2)
int
fb_dialog_gestures(int cgestures, u_int32_t bcolor, u_int32_t bgcolor,
    u_int32_t tcolor, const char *title, const char *text)
{
	int dheight, dwidth, gesture, x0, y0, x, y;
	int i, textlines, linewidth, maxwidth;
	int textheight, textwidth, titleheight, titlewidth;
	char **lines;
	char *textdup;
	u_int32_t *textbuf, *titlebuf;
	struct tsstate *ts;
        struct timespec stime;

	titlewidth = strlen(title) * fb_get_font_width() * 2;
	if (titlewidth + FBD_BORDER_WIDTH * 2 > fb_width)
		titlewidth = fb_width - FBD_BORDER_WIDTH * 2;
	titleheight = fb_get_font_height() * 2;

	textlines = 0;
	linewidth = 0;
	textwidth = 0;
	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == '\n') {
			textlines++;
			textwidth = (linewidth > textwidth) ? linewidth :
			    textwidth;
			linewidth = 0;
		} else
			linewidth++;
	}
	textlines++;
	textwidth = (linewidth > textwidth) ? linewidth : textwidth;
	textwidth *= fb_get_font_width() * 2;
	if (textwidth + FBD_BORDER_WIDTH * 2 > fb_width)
		textwidth = fb_width - FBD_BORDER_WIDTH * 2;
	textheight = fb_get_font_height() * 2;
	if (textheight + FBD_BORDER_WIDTH * 2 + titleheight > fb_height)
		textheight = fb_height - FBD_BORDER_WIDTH * 2 + titleheight;

	maxwidth = (textwidth > titlewidth) ? textwidth : titlewidth;
	dwidth = FBD_BORDER_WIDTH + maxwidth + FBD_BORDER_WIDTH;

	dheight = FBD_BORDER_WIDTH + titleheight + FBD_BORDER_WIDTH +
	    textheight * textlines + FBD_BORDER_WIDTH;

	x0 = (fb_width - dwidth) / 2;
	y0 = (fb_height - dheight) / 2;

	lines = malloc(textlines * sizeof(char *));
	if (lines == NULL)
		err(1, "malloc");
	textdup = strdup(text);
	if (textdup == NULL)
		err(1, "strdup");
	textlines = 0;
	lines[textlines] = textdup;
	for (i = 0; textdup[i] != '\0'; i++) {
		if (textdup[i] == '\n') {
			textdup[i] = '\0';
			lines[++textlines] = &textdup[i+1];
		}
	}
	textlines++;

	fb_fill_region(bgcolor, x0, y0, dwidth, dheight);
	for (x = x0 + FBD_BORDER_SPACE; x < x0 + dwidth - FBD_BORDER_SPACE;
	    x++) {
		for (y = 0; y < FBD_BORDER_LWIDTH; y++) {
			fb_putpixel(x, y0 + FBD_BORDER_SPACE + y, bcolor);
			fb_putpixel(x, y0 + FBD_BORDER_SPACE + y + 
			    FBD_BORDER_WIDTH + titleheight, bcolor);
			fb_putpixel(x, y0 + FBD_BORDER_SPACE + y + dheight -
			    FBD_BORDER_WIDTH, bcolor);
		}
	}
	for (y = y0 + FBD_BORDER_SPACE; y < y0 + dheight - FBD_BORDER_SPACE;
	    y++) {
		for (x = 0; x < FBD_BORDER_LWIDTH; x++) {
			fb_putpixel(x0 + FBD_BORDER_SPACE + x, y, bcolor);
			fb_putpixel(x0 + dwidth + FBD_BORDER_SPACE + x -
			    FBD_BORDER_WIDTH, y, bcolor);
		}
	}

	titlebuf = malloc(sizeof(u_int32_t) * titlewidth * titleheight);
	if (titlebuf == NULL)
		err(1, "malloc");
	fb_render_text(title, 2, tcolor, bgcolor, titlebuf,
	    titlewidth, titleheight);
	fb_post_region(titlebuf,
	    x0 + (dwidth - titlewidth) / 2, y0 + FBD_BORDER_WIDTH,
	    titlewidth, titleheight);
	free(titlebuf);

	textbuf = malloc(sizeof(u_int32_t) * textwidth * textheight);
	if (textbuf == NULL)
		err(1, "malloc");
	for(i = 0; i < textlines; i++) {
		fb_fill_buf(textbuf, bgcolor, textwidth, textheight);
		fb_render_text(lines[i], 2, tcolor, bgcolor, textbuf,
		    textwidth, textheight);
		fb_post_region(textbuf, x0 + FBD_BORDER_WIDTH,
		    y0 + 2 * FBD_BORDER_WIDTH + titleheight + i * textheight,
		    textwidth, textheight);
	}
	free(textbuf);
	free(lines);

	/* Ignore all input for a quarter second to let the display settle. */
	stime.tv_sec = 0;
	stime.tv_nsec =  250 * 1000 * 1000;
	nanosleep(&stime, NULL);
	ts_drain();

	for (;;) {
		ts = ts_poll(0);
		gesture = tsg2tsgf(ts->ts_gesture);
		if (cgestures & gesture)
			return (gesture);
	}
}

fb_dialog_action
fb_dialog(fb_dialog_type type, u_int32_t bcolor, u_int32_t bgcolor,
    u_int32_t tcolor, const char *title, const char *text)
{
	int gesture;

	switch (type) {
	case FBDT_EAST2CLOSE:
		fb_dialog_gestures(TSGF_EAST, bcolor, bgcolor, tcolor,
		    title, text);
		return(FBDA_OK);
	case FBDT_PINCH2CLOSE:
		fb_dialog_gestures(TSGF_ZOOM_OUT, bcolor, bgcolor, tcolor,
		    title, text);
		return(FBDA_OK);
	case FBDT_PINCH_OR_VSCROLL:
		gesture = fb_dialog_gestures(TSGF_ZOOM_OUT | TSGF_NORTH |
		    TSGF_2NORTH | TSGF_SOUTH | TSGF_2SOUTH, bcolor,
		    bgcolor, tcolor, title, text);
		switch (gesture) {
		case TSGF_ZOOM_OUT:
			return(FBDA_OK);
		case TSGF_NORTH:
		case TSGF_2NORTH:
			return(FBDA_DOWN);
		case TSGF_SOUTH:
		case TSGF_2SOUTH:
			return(FBDA_UP);
		}
	default:
		err(1, "Unhandled dialog type");
	}
}
