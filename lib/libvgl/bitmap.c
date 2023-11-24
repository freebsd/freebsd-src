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
 *    notice, this list of conditions and the following disclaimer,
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/types.h>
#include <signal.h>
#include <sys/fbio.h>
#include "vgl.h"

#define min(x, y)	(((x) < (y)) ? (x) : (y))

static byte mask[8] = {0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01};
static int color2bit[16] = {0x00000000, 0x00000001, 0x00000100, 0x00000101,
			    0x00010000, 0x00010001, 0x00010100, 0x00010101,
			    0x01000000, 0x01000001, 0x01000100, 0x01000101,
			    0x01010000, 0x01010001, 0x01010100, 0x01010101};

static void
WriteVerticalLine(VGLBitmap *dst, int x, int y, int width, byte *line)
{
  int bwidth, i, pos, last, planepos, start_offset, end_offset, offset;
  int len;
  unsigned int word = 0;
  byte *address;
  byte *VGLPlane[4];

  switch (dst->Type) {
  case VIDBUF4:
  case VIDBUF4S:
    start_offset = (x & 0x07);
    end_offset = (x + width) & 0x07;
    bwidth = (width + start_offset) / 8;
    if (end_offset)
	bwidth++;
    VGLPlane[0] = VGLBuf;
    VGLPlane[1] = VGLPlane[0] + bwidth;
    VGLPlane[2] = VGLPlane[1] + bwidth;
    VGLPlane[3] = VGLPlane[2] + bwidth;
    pos = 0;
    planepos = 0;
    last = 8 - start_offset;
    while (pos < width) {
      word = 0;
      while (pos < last && pos < width)
	word = (word<<1) | color2bit[line[pos++]&0x0f];
      VGLPlane[0][planepos] = word;
      VGLPlane[1][planepos] = word>>8;
      VGLPlane[2][planepos] = word>>16;
      VGLPlane[3][planepos] = word>>24;
      planepos++;
      last += 8;
    }
    planepos--;
    if (end_offset) {
      word <<= (8 - end_offset);
      VGLPlane[0][planepos] = word;
      VGLPlane[1][planepos] = word>>8;
      VGLPlane[2][planepos] = word>>16;
      VGLPlane[3][planepos] = word>>24;
    }
    outb(0x3ce, 0x01); outb(0x3cf, 0x00);		/* set/reset enable */
    outb(0x3ce, 0x08); outb(0x3cf, 0xff);		/* bit mask */
    for (i=0; i<4; i++) {
      outb(0x3c4, 0x02);
      outb(0x3c5, 0x01<<i);
      outb(0x3ce, 0x04);
      outb(0x3cf, i);
      pos = VGLAdpInfo.va_line_width*y + x/8;
      if (dst->Type == VIDBUF4) {
	if (end_offset)
	  VGLPlane[i][planepos] |= dst->Bitmap[pos+planepos] & mask[end_offset];
	if (start_offset)
	  VGLPlane[i][0] |= dst->Bitmap[pos] & ~mask[start_offset];
	bcopy(&VGLPlane[i][0], dst->Bitmap + pos, bwidth);
      } else {	/* VIDBUF4S */
	if (end_offset) {
	  offset = VGLSetSegment(pos + planepos);
	  VGLPlane[i][planepos] |= dst->Bitmap[offset] & mask[end_offset];
	}
	offset = VGLSetSegment(pos);
	if (start_offset)
	  VGLPlane[i][0] |= dst->Bitmap[offset] & ~mask[start_offset];
	for (last = bwidth; ; ) { 
	  len = min(VGLAdpInfo.va_window_size - offset, last);
	  bcopy(&VGLPlane[i][bwidth - last], dst->Bitmap + offset, len);
	  pos += len;
	  last -= len;
	  if (last <= 0)
	    break;
	  offset = VGLSetSegment(pos);
	}
      }
    }
    break;
  case VIDBUF8X:
    address = dst->Bitmap + VGLAdpInfo.va_line_width * y + x/4;
    for (i=0; i<4; i++) {
      outb(0x3c4, 0x02);
      outb(0x3c5, 0x01 << ((x + i)%4));
      for (planepos=0, pos=i; pos<width; planepos++, pos+=4)
        address[planepos] = line[pos];
      if ((x + i)%4 == 3)
	++address;
    }
    break;
  case VIDBUF8S:
  case VIDBUF16S:
  case VIDBUF24S:
  case VIDBUF32S:
    width = width * dst->PixelBytes;
    pos = (dst->VXsize * y + x) * dst->PixelBytes;
    while (width > 0) {
      offset = VGLSetSegment(pos);
      i = min(VGLAdpInfo.va_window_size - offset, width);
      bcopy(line, dst->Bitmap + offset, i);
      line += i;
      pos += i;
      width -= i;
    }
    break;
  case MEMBUF:
  case VIDBUF8:
  case VIDBUF16:
  case VIDBUF24:
  case VIDBUF32:
    address = dst->Bitmap + (dst->VXsize * y + x) * dst->PixelBytes;
    bcopy(line, address, width * dst->PixelBytes);
    break;
  default:
    ;
  }
}

int
__VGLBitmapCopy(VGLBitmap *src, int srcx, int srcy,
	      VGLBitmap *dst, int dstx, int dsty, int width, int hight)
{
  byte *buffer, *p;
  int mousemerge, srcline, dstline, yend, yextra, ystep;
  
  mousemerge = 0;
  if (hight < 0) {
    hight = -hight;
    mousemerge = (dst == VGLDisplay &&
		  VGLMouseOverlap(dstx, dsty, width, hight));
    if (mousemerge)
      buffer = alloca(width*src->PixelBytes);
  }
  if (srcx>src->VXsize || srcy>src->VYsize
	|| dstx>dst->VXsize || dsty>dst->VYsize)
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
  if (srcx+width > src->VXsize)
     width=src->VXsize-srcx;
  if (srcy+hight > src->VYsize)
     hight=src->VYsize-srcy;
  if (dstx+width > dst->VXsize)
     width=dst->VXsize-dstx;
  if (dsty+hight > dst->VYsize)
     hight=dst->VYsize-dsty;
  if (width < 0 || hight < 0)
     return -1;
  yend = srcy + hight;
  yextra = 0;
  ystep = 1;
  if (src->Bitmap == dst->Bitmap && srcy < dsty) {
    yend = srcy - 1;
    yextra = hight - 1;
    ystep = -1;
  }
  for (srcline = srcy + yextra, dstline = dsty + yextra; srcline != yend;
       srcline += ystep, dstline += ystep) {
    p = src->Bitmap+(srcline*src->VXsize+srcx)*dst->PixelBytes;
    if (mousemerge && VGLMouseOverlap(dstx, dstline, width, 1)) {
      bcopy(p, buffer, width*src->PixelBytes);
      p = buffer;
      VGLMouseMerge(dstx, dstline, width, p);
    }
    WriteVerticalLine(dst, dstx, dstline, width, p);
  }
  return 0;
}

int
VGLBitmapCopy(VGLBitmap *src, int srcx, int srcy,
	      VGLBitmap *dst, int dstx, int dsty, int width, int hight)
{
  int error;

  if (hight < 0)
    return -1;
  if (src == VGLDisplay)
    src = &VGLVDisplay;
  if (src->Type != MEMBUF)
    return -1;		/* invalid */
  if (dst == VGLDisplay) {
    VGLMouseFreeze();
    __VGLBitmapCopy(src, srcx, srcy, &VGLVDisplay, dstx, dsty, width, hight);
    error = __VGLBitmapCopy(src, srcx, srcy, &VGLVDisplay, dstx, dsty,
                            width, hight);
    if (error != 0)
      return error;
    src = &VGLVDisplay;
    srcx = dstx;
    srcy = dsty;
  } else if (dst->Type != MEMBUF)
    return -1;		/* invalid */
  error = __VGLBitmapCopy(src, srcx, srcy, dst, dstx, dsty, width, -hight);
  if (dst == VGLDisplay)
    VGLMouseUnFreeze();
  return error;
}

VGLBitmap
*VGLBitmapCreate(int type, int xsize, int ysize, byte *bits)
{
  VGLBitmap *object;

  if (type != MEMBUF)
    return NULL;
  if (xsize < 0 || ysize < 0)
    return NULL;
  object = (VGLBitmap *)malloc(sizeof(*object));
  if (object == NULL)
    return NULL;
  object->Type = type;
  object->Xsize = xsize;
  object->Ysize = ysize;
  object->VXsize = xsize;
  object->VYsize = ysize;
  object->Xorigin = 0;
  object->Yorigin = 0;
  object->Bitmap = bits;
  object->PixelBytes = VGLDisplay->PixelBytes;
  return object;
}

void
VGLBitmapDestroy(VGLBitmap *object)
{
  if (object->Bitmap)
    free(object->Bitmap);
  free(object);
}

int
VGLBitmapAllocateBits(VGLBitmap *object)
{
  object->Bitmap = malloc(object->VXsize*object->VYsize*object->PixelBytes);
  if (object->Bitmap == NULL)
    return -1;
  return 0;
}

void
VGLBitmapCvt(VGLBitmap *src, VGLBitmap *dst)
{
  u_long color;
  int dstpos, i, pb, size, srcpb, srcpos;

  size = src->VXsize * src->VYsize;
  srcpb = src->PixelBytes;
  if (srcpb <= 0)
    srcpb = 1;
  pb = dst->PixelBytes;
  if (pb == srcpb) {
    bcopy(src->Bitmap, dst->Bitmap, size * pb);
    return;
  }
  if (srcpb != 1)
    return;		/* not supported */
  for (srcpos = dstpos = 0; srcpos < size; srcpos++) {
    color = VGLrgb332ToNative(src->Bitmap[srcpos]);
    for (i = 0; i < pb; i++, color >>= 8)
        dst->Bitmap[dstpos++] = color;
  }
}
