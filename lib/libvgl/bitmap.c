/*-
 * Copyright (c) 1991-1997 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission.
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
 *
 *  $Id: bitmap.c,v 1.8 1997/08/15 12:32:59 sos Exp $
 */

#include <sys/types.h>
#include <signal.h>
#include "vgl.h"

static byte VGLPlane[4][128];
static byte mask[8] = {0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01};
static int color2bit[16] = {0x00000000, 0x00000001, 0x00000100, 0x00000101,
			    0x00010000, 0x00010001, 0x00010100, 0x00010101,
			    0x01000000, 0x01000001, 0x01000100, 0x01000101,
			    0x01010000, 0x01010001, 0x01010100, 0x01010101};

static void
WriteVerticalLine(VGLBitmap *dst, int x, int y, int width, byte *line)
{
  int i, pos, last, planepos, start_offset, end_offset, offset;
  unsigned int word = 0;
  byte *address;

  switch (dst->Type) {
  case VIDBUF4:
    address = dst->Bitmap + (dst->Xsize/8 * y) + x/8;
    start_offset = (x & 0x07);
    end_offset = (x + width) & 0x07;
    offset = start_offset;
    pos = 0;
    planepos = 0;
    while (pos < width) {
      word = 0;
      last = pos + 8 - offset;
      while (pos < last && pos < width)
	word = (word<<1) | color2bit[line[pos++]&0x0f];
      VGLPlane[0][planepos] = word;
      VGLPlane[1][planepos] = word>>8;
      VGLPlane[2][planepos] = word>>16;
      VGLPlane[3][planepos] = word>>24;
      planepos++;
      offset = 0;
    }
    planepos--;
    if (end_offset) {
      word <<= (8 - end_offset);
      VGLPlane[0][planepos] = word;
      VGLPlane[1][planepos] = word>>8;
      VGLPlane[2][planepos] = word>>16;
      VGLPlane[3][planepos] = word>>24;
    }
    if (start_offset || end_offset)
      width+=8;
    for (i=0; i<4; i++) {
      outb(0x3c4, 0x02);
      outb(0x3c5, 0x01<<i);
      outb(0x3ce, 0x04);
      outb(0x3cf, i);
      if (start_offset)
	VGLPlane[i][0] |= *address & ~mask[start_offset];
      if (end_offset)
	VGLPlane[i][planepos] |= *(address + planepos) & mask[end_offset];
      bcopy(&VGLPlane[i][0], address, width/8);
    }
    break;
  case VIDBUF8X:
    address = dst->Bitmap + ((dst->Xsize * y) + x)/2;
    for (i=0; i<4; i++) {
      outb(0x3c4, 0x02);
      outb(0x3c5, 0x01<<i);
      pos = i;
      for (planepos=0; planepos<width/4; planepos++, pos+=4)
        address[planepos] = line[pos];
    } 
    break;
  case VIDBUF8:
  case MEMBUF:
    address = dst->Bitmap + (dst->Xsize * y) + x;
    bcopy(line, address, width);
    break;

  default:
  }
}

static void
ReadVerticalLine(VGLBitmap *src, int x, int y, int width, byte *line)
{
  int i, bit, pos, count, planepos, start_offset, end_offset, offset;
  byte *address;

  switch (src->Type) {
  case VIDBUF4:
    address = src->Bitmap + (src->Xsize/8 * y) + x/8;
    start_offset = (x & 0x07);
    end_offset = (x + width) & 0x07;
    offset = start_offset;
    if (start_offset)
	count = (width - (8 - start_offset)) / 8 + 1;
    else
	count = width / 8;
    if (end_offset)
	count++;
    for (i=0; i<4; i++) {
      outb(0x3ce, 0x04);
      outb(0x3cf, i);
      bcopy(address, &VGLPlane[i][0], count);
    }
    pos = 0;
    planepos = 0;
    while (pos < width) {
      for (bit = (7-offset); bit >= 0 && pos < width; bit--, pos++) {
        line[pos] = (VGLPlane[0][planepos] & (1<<bit) ? 1 : 0) |
                    ((VGLPlane[1][planepos] & (1<<bit) ? 1 : 0) << 1) |
                    ((VGLPlane[2][planepos] & (1<<bit) ? 1 : 0) << 2) |
                    ((VGLPlane[3][planepos] & (1<<bit) ? 1 : 0) << 3);
      }
      planepos++;
      offset = 0;
    }
    break;
  case VIDBUF8X:
    address = src->Bitmap + ((src->Xsize * y) + x)/2;
    for (i=0; i<4; i++) {
      outb(0x3ce, 0x04);
      outb(0x3cf, i);
      pos = i;
      for (planepos=0; planepos<width/4; planepos++, pos+=4)
        line[pos] = address[planepos];
    } 
    break;
  case VIDBUF8:
  case MEMBUF:
    address = src->Bitmap + (src->Xsize * y) + x;
    bcopy(address, line, width);
    break;
  default:
  }
}

int
__VGLBitmapCopy(VGLBitmap *src, int srcx, int srcy,
	      VGLBitmap *dst, int dstx, int dsty, int width, int hight)
{
  int srcline, dstline;

  if (srcx>src->Xsize||srcy>src->Ysize||dsty>dst->Xsize||dsty>dst->Ysize)
    return -1;  
  if (srcx < 0) {
    width=width+srcx; dstx-=srcx; srcx=0;    
  }
  if (srcy < 0) {
    hight=hight+srcy; dsty-=srcy; srcy=0; 
  }
  if (dstx < 0) {    
    width=width+dstx; srcx-=dstx; dstx=0;
  }
  if (dsty < 0) {
    hight=hight+dsty; srcy-=dsty; dsty=0;
  }
  if (srcx+width > src->Xsize)
     width=src->Xsize-srcx;
  if (srcy+hight > src->Ysize)
     hight=src->Ysize-srcy;
  if (dstx+width > dst->Xsize)
     width=dst->Xsize-dstx;
  if (dsty+hight > dst->Ysize)
     hight=dst->Ysize-dsty;
  if (width < 0 || hight < 0)
     return -1;
  if (src->Type == MEMBUF) {
    for (srcline=srcy, dstline=dsty; srcline<srcy+hight; srcline++, dstline++) {
      WriteVerticalLine(dst, dstx, dstline, width, 
	(src->Bitmap+(srcline*src->Xsize)+srcx));
    }
  }
  else if (dst->Type == MEMBUF) {
    for (srcline=srcy, dstline=dsty; srcline<srcy+hight; srcline++, dstline++) {
      ReadVerticalLine(src, srcx, srcline, width,
	 (dst->Bitmap+(dstline*dst->Xsize)+dstx));
    }
  }
  else {
    byte buffer[1024];
    for (srcline=srcy, dstline=dsty; srcline<srcy+hight; srcline++, dstline++) {
      ReadVerticalLine(src, srcx, srcline, width, buffer);
      WriteVerticalLine(dst, dstx, dstline, width, buffer);
    }
  }
  return 0;
}

int
VGLBitmapCopy(VGLBitmap *src, int srcx, int srcy,
	      VGLBitmap *dst, int dstx, int dsty, int width, int hight)
{
  int error;

  VGLMouseFreeze(dstx, dsty, width, hight, 0);
  error = __VGLBitmapCopy(src, srcx, srcy, dst, dstx, dsty, width, hight);
  VGLMouseUnFreeze();
  return error;
}

