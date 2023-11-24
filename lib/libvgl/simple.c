/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991-1997 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <signal.h>
#include <sys/fbio.h>
#include <sys/kbio.h>
#include <sys/endian.h>
#include "vgl.h"

static int VGLBlank;
static byte VGLBorderColor;
static byte VGLSavePaletteRed[256];
static byte VGLSavePaletteGreen[256];
static byte VGLSavePaletteBlue[256];

#define ABS(a)		(((a)<0) ? -(a) : (a))
#define SGN(a)		(((a)<0) ? -1 : 1)
#define min(x, y)	(((x) < (y)) ? (x) : (y))
#define max(x, y)	(((x) > (y)) ? (x) : (y))

void
VGLSetXY(VGLBitmap *object, int x, int y, u_long color)
{
  int offset, soffset, undermouse;

  VGLCheckSwitch();
  if (x>=0 && x<object->VXsize && y>=0 && y<object->VYsize) {
    if (object == VGLDisplay) {
      undermouse = VGLMouseFreezeXY(x, y);
      VGLSetXY(&VGLVDisplay, x, y, color);
    } else if (object->Type != MEMBUF)
      return;		/* invalid */
    else
      undermouse = 0;
    if (!undermouse) {
      offset = (y * object->VXsize + x) * object->PixelBytes;
      switch (object->Type) {
      case VIDBUF8S:
      case VIDBUF16S:
      case VIDBUF32S:
        offset = VGLSetSegment(offset);
        /* FALLTHROUGH */
      case MEMBUF:
      case VIDBUF8:
      case VIDBUF16:
      case VIDBUF24:
      case VIDBUF32:
        color = htole32(color);
        switch (object->PixelBytes) {
        case 1:
          memcpy(&object->Bitmap[offset], &color, 1);
          break;
        case 2:
          memcpy(&object->Bitmap[offset], &color, 2);
          break;
        case 3:
          memcpy(&object->Bitmap[offset], &color, 3);
          break;
        case 4:
          memcpy(&object->Bitmap[offset], &color, 4);
          break;
        }
        break;
      case VIDBUF24S:
        soffset = VGLSetSegment(offset);
        color = htole32(color);
        switch (VGLAdpInfo.va_window_size - soffset) {
        case 1:
          memcpy(&object->Bitmap[soffset], &color, 1);
          soffset = VGLSetSegment(offset + 1);
          memcpy(&object->Bitmap[soffset], (byte *)&color + 1, 2);
          break;
        case 2:
          memcpy(&object->Bitmap[soffset], &color, 2);
          soffset = VGLSetSegment(offset + 2);
          memcpy(&object->Bitmap[soffset], (byte *)&color + 2, 1);
          break;
        default:
          memcpy(&object->Bitmap[soffset], &color, 3);
          break;
        }
        break;
      case VIDBUF8X:
        outb(0x3c4, 0x02);
        outb(0x3c5, 0x01 << (x&0x3));
	object->Bitmap[(unsigned)(VGLAdpInfo.va_line_width*y)+(x/4)] = ((byte)color);
	break;
      case VIDBUF4S:
	offset = VGLSetSegment(y*VGLAdpInfo.va_line_width + x/8);
	goto set_planar;
      case VIDBUF4:
	offset = y*VGLAdpInfo.va_line_width + x/8;
set_planar:
        outb(0x3c4, 0x02); outb(0x3c5, 0x0f);
        outb(0x3ce, 0x00); outb(0x3cf, (byte)color & 0x0f);	/* set/reset */
        outb(0x3ce, 0x01); outb(0x3cf, 0x0f);		/* set/reset enable */
        outb(0x3ce, 0x08); outb(0x3cf, 0x80 >> (x%8));	/* bit mask */
	object->Bitmap[offset] |= (byte)color;
      }
    }
    if (object == VGLDisplay)
      VGLMouseUnFreeze();
  }
}

u_long
VGLGetXY(VGLBitmap *object, int x, int y)
{
  u_long color;
  int offset;

  VGLCheckSwitch();
  if (x<0 || x>=object->VXsize || y<0 || y>=object->VYsize)
    return 0;
  if (object == VGLDisplay)
    object = &VGLVDisplay;
  else if (object->Type != MEMBUF)
    return 0;		/* invalid */
  offset = (y * object->VXsize + x) * object->PixelBytes;
  switch (object->PixelBytes) {
  case 1:
    memcpy(&color, &object->Bitmap[offset], 1);
    return le32toh(color) & 0xff;
  case 2:
    memcpy(&color, &object->Bitmap[offset], 2);
    return le32toh(color) & 0xffff;
  case 3:
    memcpy(&color, &object->Bitmap[offset], 3);
    return le32toh(color) & 0xffffff;
  case 4:
    memcpy(&color, &object->Bitmap[offset], 4);
    return le32toh(color);
  }
  return 0;		/* invalid */
}

 /*
  * Symmetric Double Step Line Algorithm by Brian Wyvill from
  * "Graphics Gems", Academic Press, 1990.
  */

#define SL_SWAP(a,b)           {a^=b; b^=a; a^=b;}
#define SL_ABSOLUTE(i,j,k)     ( (i-j)*(k = ( (i-j)<0 ? -1 : 1)))

void
plot(VGLBitmap * object, int x, int y, int flag, u_long color)
{
  /* non-zero flag indicates the pixels need swapping back. */
  if (flag)
    VGLSetXY(object, y, x, color);
  else
    VGLSetXY(object, x, y, color);
}


void
VGLLine(VGLBitmap *object, int x1, int y1, int x2, int y2, u_long color)
{
  int dx, dy, incr1, incr2, D, x, y, xend, c, pixels_left;
  int sign_x, sign_y, step, reverse, i;

  dx = SL_ABSOLUTE(x2, x1, sign_x);
  dy = SL_ABSOLUTE(y2, y1, sign_y);
  /* decide increment sign by the slope sign */
  if (sign_x == sign_y)
    step = 1;
  else
    step = -1;

  if (dy > dx) {	/* chooses axis of greatest movement (make dx) */
    SL_SWAP(x1, y1);
    SL_SWAP(x2, y2);
    SL_SWAP(dx, dy);
    reverse = 1;
  } else
    reverse = 0;
  /* note error check for dx==0 should be included here */
  if (x1 > x2) {      /* start from the smaller coordinate */
    x = x2;
    y = y2;
/*  x1 = x1;
    y1 = y1; */
  } else {
    x = x1;
    y = y1;
    x1 = x2;
    y1 = y2;
  }


  /* Note dx=n implies 0 - n or (dx+1) pixels to be set */
  /* Go round loop dx/4 times then plot last 0,1,2 or 3 pixels */
  /* In fact (dx-1)/4 as 2 pixels are already plotted */
  xend = (dx - 1) / 4;
  pixels_left = (dx - 1) % 4;  /* number of pixels left over at the
           * end */
  plot(object, x, y, reverse, color);
  if (pixels_left < 0)
    return;      /* plot only one pixel for zero length
           * vectors */
  plot(object, x1, y1, reverse, color);  /* plot first two points */
  incr2 = 4 * dy - 2 * dx;
  if (incr2 < 0) {    /* slope less than 1/2 */
    c = 2 * dy;
    incr1 = 2 * c;
    D = incr1 - dx;

    for (i = 0; i < xend; i++) {  /* plotting loop */
      ++x;
      --x1;
      if (D < 0) {
        /* pattern 1 forwards */
        plot(object, x, y, reverse, color);
        plot(object, ++x, y, reverse, color);
        /* pattern 1 backwards */
        plot(object, x1, y1, reverse, color);
        plot(object, --x1, y1, reverse, color);
        D += incr1;
      } else {
        if (D < c) {
          /* pattern 2 forwards */
          plot(object, x, y, reverse, color);
          plot(object, ++x, y += step, reverse,
              color);
          /* pattern 2 backwards */
          plot(object, x1, y1, reverse, color);
          plot(object, --x1, y1 -= step, reverse,
              color);
        } else {
          /* pattern 3 forwards */
          plot(object, x, y += step, reverse, color);
          plot(object, ++x, y, reverse, color);
          /* pattern 3 backwards */
          plot(object, x1, y1 -= step, reverse,
              color);
          plot(object, --x1, y1, reverse, color);
        }
        D += incr2;
      }
    }      /* end for */

    /* plot last pattern */
    if (pixels_left) {
      if (D < 0) {
        plot(object, ++x, y, reverse, color);  /* pattern 1 */
        if (pixels_left > 1)
          plot(object, ++x, y, reverse, color);
        if (pixels_left > 2)
          plot(object, --x1, y1, reverse, color);
      } else {
        if (D < c) {
          plot(object, ++x, y, reverse, color);  /* pattern 2  */
          if (pixels_left > 1)
            plot(object, ++x, y += step, reverse, color);
          if (pixels_left > 2)
            plot(object, --x1, y1, reverse, color);
        } else {
          /* pattern 3 */
          plot(object, ++x, y += step, reverse, color);
          if (pixels_left > 1)
            plot(object, ++x, y, reverse, color);
          if (pixels_left > 2)
            plot(object, --x1, y1 -= step, reverse, color);
        }
      }
    }      /* end if pixels_left */
  }
  /* end slope < 1/2 */
  else {        /* slope greater than 1/2 */
    c = 2 * (dy - dx);
    incr1 = 2 * c;
    D = incr1 + dx;
    for (i = 0; i < xend; i++) {
      ++x;
      --x1;
      if (D > 0) {
        /* pattern 4 forwards */
        plot(object, x, y += step, reverse, color);
        plot(object, ++x, y += step, reverse, color);
        /* pattern 4 backwards */
        plot(object, x1, y1 -= step, reverse, color);
        plot(object, --x1, y1 -= step, reverse, color);
        D += incr1;
      } else {
        if (D < c) {
          /* pattern 2 forwards */
          plot(object, x, y, reverse, color);
          plot(object, ++x, y += step, reverse,
              color);

          /* pattern 2 backwards */
          plot(object, x1, y1, reverse, color);
          plot(object, --x1, y1 -= step, reverse,
              color);
        } else {
          /* pattern 3 forwards */
          plot(object, x, y += step, reverse, color);
          plot(object, ++x, y, reverse, color);
          /* pattern 3 backwards */
          plot(object, x1, y1 -= step, reverse, color);
          plot(object, --x1, y1, reverse, color);
        }
        D += incr2;
      }
    }      /* end for */
    /* plot last pattern */
    if (pixels_left) {
      if (D > 0) {
        plot(object, ++x, y += step, reverse, color);  /* pattern 4 */
        if (pixels_left > 1)
          plot(object, ++x, y += step, reverse,
              color);
        if (pixels_left > 2)
          plot(object, --x1, y1 -= step, reverse,
              color);
      } else {
        if (D < c) {
          plot(object, ++x, y, reverse, color);  /* pattern 2  */
          if (pixels_left > 1)
            plot(object, ++x, y += step, reverse, color);
          if (pixels_left > 2)
            plot(object, --x1, y1, reverse, color);
        } else {
          /* pattern 3 */
          plot(object, ++x, y += step, reverse, color);
          if (pixels_left > 1)
            plot(object, ++x, y, reverse, color);
          if (pixels_left > 2) {
            if (D > c)  /* step 3 */
              plot(object, --x1, y1 -= step, reverse, color);
            else  /* step 2 */
              plot(object, --x1, y1, reverse, color);
          }
        }
      }
    }
  }
}

void
VGLBox(VGLBitmap *object, int x1, int y1, int x2, int y2, u_long color)
{
  VGLLine(object, x1, y1, x2, y1, color);
  VGLLine(object, x2, y1, x2, y2, color);
  VGLLine(object, x2, y2, x1, y2, color);
  VGLLine(object, x1, y2, x1, y1, color);
}

void
VGLFilledBox(VGLBitmap *object, int x1, int y1, int x2, int y2, u_long color)
{
  int y;

  for (y=y1; y<=y2; y++) VGLLine(object, x1, y, x2, y, color);
}

static inline void
set4pixels(VGLBitmap *object, int x, int y, int xc, int yc, u_long color)
{
  if (x!=0) { 
    VGLSetXY(object, xc+x, yc+y, color); 
    VGLSetXY(object, xc-x, yc+y, color); 
    if (y!=0) { 
      VGLSetXY(object, xc+x, yc-y, color); 
      VGLSetXY(object, xc-x, yc-y, color); 
    } 
  } 
  else { 
    VGLSetXY(object, xc, yc+y, color); 
    if (y!=0) 
      VGLSetXY(object, xc, yc-y, color); 
  } 
}

void
VGLEllipse(VGLBitmap *object, int xc, int yc, int a, int b, u_long color)
{
  int x = 0, y = b, asq = a*a, asq2 = a*a*2, bsq = b*b;
  int bsq2 = b*b*2, d = bsq-asq*b+asq/4, dx = 0, dy = asq2*b;

  while (dx<dy) {
    set4pixels(object, x, y, xc, yc, color);
    if (d>0) {
      y--; dy-=asq2; d-=dy;
    }
    x++; dx+=bsq2; d+=bsq+dx;
  }
  d+=(3*(asq-bsq)/2-(dx+dy))/2;
  while (y>=0) {
    set4pixels(object, x, y, xc, yc, color);
    if (d<0) {
      x++; dx+=bsq2; d+=dx;
    }
    y--; dy-=asq2; d+=asq-dy;
  }
}

static inline void
set2lines(VGLBitmap *object, int x, int y, int xc, int yc, u_long color)
{
  if (x!=0) { 
    VGLLine(object, xc+x, yc+y, xc-x, yc+y, color); 
    if (y!=0) 
      VGLLine(object, xc+x, yc-y, xc-x, yc-y, color); 
  } 
  else { 
    VGLLine(object, xc, yc+y, xc, yc-y, color); 
  } 
}

void
VGLFilledEllipse(VGLBitmap *object, int xc, int yc, int a, int b, u_long color)
{
  int x = 0, y = b, asq = a*a, asq2 = a*a*2, bsq = b*b;
  int bsq2 = b*b*2, d = bsq-asq*b+asq/4, dx = 0, dy = asq2*b;

  while (dx<dy) {
    set2lines(object, x, y, xc, yc, color);
    if (d>0) {
      y--; dy-=asq2; d-=dy;
    }
    x++; dx+=bsq2; d+=bsq+dx;
  }
  d+=(3*(asq-bsq)/2-(dx+dy))/2;
  while (y>=0) {
    set2lines(object, x, y, xc, yc, color);
    if (d<0) {
      x++; dx+=bsq2; d+=dx;
    }
    y--; dy-=asq2; d+=asq-dy;
  }
}

void
VGLClear(VGLBitmap *object, u_long color)
{
  VGLBitmap src;
  int i, len, mousemode, offset;

  VGLCheckSwitch();
  if (object == VGLDisplay) {
    VGLMouseFreeze();
    VGLClear(&VGLVDisplay, color);
  } else if (object->Type != MEMBUF)
    return;		/* invalid */
  switch (object->Type) {
  case MEMBUF:
  case VIDBUF8:
  case VIDBUF8S:
  case VIDBUF16:
  case VIDBUF16S:
  case VIDBUF24:
  case VIDBUF24S:
  case VIDBUF32:
  case VIDBUF32S:
    src.Type = MEMBUF;
    src.Xsize = object->Xsize;
    src.VXsize = object->VXsize;
    src.Ysize = 1;
    src.VYsize = 1;
    src.Xorigin = 0;
    src.Yorigin = 0;
    src.Bitmap = alloca(object->VXsize * object->PixelBytes);
    src.PixelBytes = object->PixelBytes;
    color = htole32(color);
    for (i = 0; i < object->VXsize; i++)
      bcopy(&color, src.Bitmap + i * object->PixelBytes, object->PixelBytes);
    for (i = 0; i < object->VYsize; i++)
      __VGLBitmapCopy(&src, 0, 0, object, 0, i, object->VXsize, -1);
    break;

  case VIDBUF8X:
    mousemode = __VGLMouseMode(VGL_MOUSEHIDE);
    /* XXX works only for Xsize % 4 = 0 */
    outb(0x3c6, 0xff);
    outb(0x3c4, 0x02); outb(0x3c5, 0x0f);
    memset(object->Bitmap, (byte)color, VGLAdpInfo.va_line_width*object->VYsize);
    __VGLMouseMode(mousemode);
    break;

  case VIDBUF4:
  case VIDBUF4S:
    mousemode = __VGLMouseMode(VGL_MOUSEHIDE);
    /* XXX works only for Xsize % 8 = 0 */
    outb(0x3c4, 0x02); outb(0x3c5, 0x0f);
    outb(0x3ce, 0x05); outb(0x3cf, 0x02);		/* mode 2 */
    outb(0x3ce, 0x01); outb(0x3cf, 0x00);		/* set/reset enable */
    outb(0x3ce, 0x08); outb(0x3cf, 0xff);		/* bit mask */
    for (offset = 0; offset < VGLAdpInfo.va_line_width*object->VYsize; ) {
      VGLSetSegment(offset);
      len = min(object->VXsize*object->VYsize - offset,
		VGLAdpInfo.va_window_size);
      memset(object->Bitmap, (byte)color, len);
      offset += len;
    }
    outb(0x3ce, 0x05); outb(0x3cf, 0x00);
    __VGLMouseMode(mousemode);
    break;
  }
  if (object == VGLDisplay)
    VGLMouseUnFreeze();
}

static inline u_long
VGLrgbToNative(uint16_t r, uint16_t g, uint16_t b)
{
  int nr, ng, nb;

  nr = VGLModeInfo.vi_pixel_fsizes[2];
  ng = VGLModeInfo.vi_pixel_fsizes[1];
  nb = VGLModeInfo.vi_pixel_fsizes[0];
  return (r >> (16 - nr) << (ng + nb)) | (g >> (16 - ng) << nb) |
	 (b >> (16 - nb) << 0);
}

u_long
VGLrgb332ToNative(byte c)
{
  uint16_t r, g, b;

  /* 3:3:2 to 16:16:16 */
  r = ((c & 0xe0) >> 5) * 0xffff / 7;
  g = ((c & 0x1c) >> 2) * 0xffff / 7;
  b = ((c & 0x03) >> 0) * 0xffff / 3;

  return VGLrgbToNative(r, g, b);
}

void
VGLRestorePalette()
{
  int i;

  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT)
    return;
  outb(0x3C6, 0xFF);
  inb(0x3DA); 
  outb(0x3C8, 0x00);
  for (i=0; i<256; i++) {
    outb(0x3C9, VGLSavePaletteRed[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteGreen[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteBlue[i]);
    inb(0x84);
  }
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSavePalette()
{
  int i;

  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT)
    return;
  outb(0x3C6, 0xFF);
  inb(0x3DA);
  outb(0x3C7, 0x00);
  for (i=0; i<256; i++) {
    VGLSavePaletteRed[i] = inb(0x3C9);
    inb(0x84);
    VGLSavePaletteGreen[i] = inb(0x3C9);
    inb(0x84);
    VGLSavePaletteBlue[i] = inb(0x3C9);
    inb(0x84);
  }
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSetPalette(byte *red, byte *green, byte *blue)
{
  int i;
  
  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT)
    return;
  for (i=0; i<256; i++) {
    VGLSavePaletteRed[i] = red[i];
    VGLSavePaletteGreen[i] = green[i];
    VGLSavePaletteBlue[i] = blue[i];
  }
  VGLCheckSwitch();
  outb(0x3C6, 0xFF);
  inb(0x3DA); 
  outb(0x3C8, 0x00);
  for (i=0; i<256; i++) {
    outb(0x3C9, VGLSavePaletteRed[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteGreen[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteBlue[i]);
    inb(0x84);
  }
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSetPaletteIndex(byte color, byte red, byte green, byte blue)
{
  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT)
    return;
  VGLSavePaletteRed[color] = red;
  VGLSavePaletteGreen[color] = green;
  VGLSavePaletteBlue[color] = blue;
  VGLCheckSwitch();
  outb(0x3C6, 0xFF);
  inb(0x3DA);
  outb(0x3C8, color); 
  outb(0x3C9, red); outb(0x3C9, green); outb(0x3C9, blue);
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLRestoreBorder(void)
{
  VGLSetBorder(VGLBorderColor);
}

void
VGLSetBorder(byte color)
{
  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT && ioctl(0, KDENABIO, 0))
    return;
  VGLCheckSwitch();
  inb(0x3DA);
  outb(0x3C0,0x11); outb(0x3C0, color); 
  inb(0x3DA);
  outb(0x3C0, 0x20);
  VGLBorderColor = color;
  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT)
    ioctl(0, KDDISABIO, 0);
}

void
VGLRestoreBlank(void)
{
  VGLBlankDisplay(VGLBlank);
}

void
VGLBlankDisplay(int blank)
{
  byte val;

  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT && ioctl(0, KDENABIO, 0))
    return;
  VGLCheckSwitch();
  outb(0x3C4, 0x01); val = inb(0x3C5); outb(0x3C4, 0x01);
  outb(0x3C5, ((blank) ? (val |= 0x20) : (val &= 0xDF)));
  VGLBlank = blank;
  if (VGLModeInfo.vi_mem_model == V_INFO_MM_DIRECT)
    ioctl(0, KDDISABIO, 0);
}
