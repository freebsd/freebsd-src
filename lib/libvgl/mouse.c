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
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/consio.h>
#include <sys/fbio.h>
#include "vgl.h"

static void VGLMouseAction(int dummy);

#define BORDER	0xff	/* default border -- light white in rgb 3:3:2 */
#define INTERIOR 0xa0	/* default interior -- red in rgb 3:3:2 */
#define LARGE_MOUSE_IMG_XSIZE	19
#define LARGE_MOUSE_IMG_YSIZE	32
#define SMALL_MOUSE_IMG_XSIZE	10
#define SMALL_MOUSE_IMG_YSIZE	16
#define X	0xff	/* any nonzero in And mask means part of cursor */
#define B	BORDER
#define I	INTERIOR
static byte LargeAndMask[] = {
  X,X,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,0,0,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,0,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,0,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
  X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
  X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,
  X,X,X,X,X,X,X,X,X,X,X,X,0,0,0,0,0,0,0,
  X,X,X,X,X,X,0,X,X,X,X,X,X,0,0,0,0,0,0,
  X,X,X,X,X,0,0,X,X,X,X,X,X,0,0,0,0,0,0,
  X,X,X,X,0,0,0,0,X,X,X,X,X,X,0,0,0,0,0,
  X,X,X,0,0,0,0,0,X,X,X,X,X,X,0,0,0,0,0,
  X,X,0,0,0,0,0,0,0,X,X,X,X,X,X,0,0,0,0,
  0,0,0,0,0,0,0,0,0,X,X,X,X,X,X,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,X,X,X,X,X,X,0,0,0,
  0,0,0,0,0,0,0,0,0,0,X,X,X,X,X,X,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,X,X,X,X,X,X,0,0,
  0,0,0,0,0,0,0,0,0,0,0,X,X,X,X,X,X,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,X,X,X,X,0,0,0,
};
static byte LargeOrMask[] = {
  B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  B,I,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  B,I,I,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  B,I,I,I,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  B,I,I,I,I,B,0,0,0,0,0,0,0,0,0,0,0,0,0,
  B,I,I,I,I,I,B,0,0,0,0,0,0,0,0,0,0,0,0,
  B,I,I,I,I,I,I,B,0,0,0,0,0,0,0,0,0,0,0,
  B,I,I,I,I,I,I,I,B,0,0,0,0,0,0,0,0,0,0,
  B,I,I,I,I,I,I,I,I,B,0,0,0,0,0,0,0,0,0,
  B,I,I,I,I,I,I,I,I,I,B,0,0,0,0,0,0,0,0,
  B,I,I,I,I,I,I,I,I,I,I,B,0,0,0,0,0,0,0,
  B,I,I,I,I,I,I,I,I,I,I,I,B,0,0,0,0,0,0,
  B,I,I,I,I,I,I,I,I,I,I,I,I,B,0,0,0,0,0,
  B,I,I,I,I,I,I,I,I,I,I,I,I,I,B,0,0,0,0,
  B,I,I,I,I,I,I,I,I,I,I,I,I,I,I,B,0,0,0,
  B,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,B,0,0,
  B,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,B,0,
  B,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,B,
  B,I,I,I,I,I,I,I,I,I,I,B,B,B,B,B,B,B,B,
  B,I,I,I,I,I,I,I,I,I,I,B,0,0,0,0,0,0,0,
  B,I,I,I,I,I,B,I,I,I,I,B,0,0,0,0,0,0,0,
  B,I,I,I,I,B,0,B,I,I,I,I,B,0,0,0,0,0,0,
  B,I,I,I,B,0,0,B,I,I,I,I,B,0,0,0,0,0,0,
  B,I,I,B,0,0,0,0,B,I,I,I,I,B,0,0,0,0,0,
  B,I,B,0,0,0,0,0,B,I,I,I,I,B,0,0,0,0,0,
  B,B,0,0,0,0,0,0,0,B,I,I,I,I,B,0,0,0,0,
  0,0,0,0,0,0,0,0,0,B,I,I,I,I,B,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,B,I,I,I,I,B,0,0,0,
  0,0,0,0,0,0,0,0,0,0,B,I,I,I,I,B,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,B,I,I,I,I,B,0,0,
  0,0,0,0,0,0,0,0,0,0,0,B,I,I,I,I,B,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,B,0,0,0,
};
static byte SmallAndMask[] = {
  X,X,0,0,0,0,0,0,0,0,
  X,X,X,0,0,0,0,0,0,0,
  X,X,X,X,0,0,0,0,0,0,
  X,X,X,X,X,0,0,0,0,0,
  X,X,X,X,X,X,0,0,0,0,
  X,X,X,X,X,X,X,0,0,0,
  X,X,X,X,X,X,X,X,0,0,
  X,X,X,X,X,X,X,X,X,0,
  X,X,X,X,X,X,X,X,X,X,
  X,X,X,X,X,X,X,X,X,X,
  X,X,X,X,X,X,X,0,0,0,
  X,X,X,0,X,X,X,X,0,0,
  X,X,0,0,X,X,X,X,0,0,
  0,0,0,0,0,X,X,X,X,0,
  0,0,0,0,0,X,X,X,X,0,
  0,0,0,0,0,0,X,X,0,0,
};
static byte SmallOrMask[] = {
  B,B,0,0,0,0,0,0,0,0,
  B,I,B,0,0,0,0,0,0,0,
  B,I,I,B,0,0,0,0,0,0,
  B,I,I,I,B,0,0,0,0,0,
  B,I,I,I,I,B,0,0,0,0,
  B,I,I,I,I,I,B,0,0,0,
  B,I,I,I,I,I,I,B,0,0,
  B,I,I,I,I,I,I,I,B,0,
  B,I,I,I,I,I,I,I,I,B,
  B,I,I,I,I,I,B,B,B,B,
  B,I,I,B,I,I,B,0,0,0,
  B,I,B,0,B,I,I,B,0,0,
  B,B,0,0,B,I,I,B,0,0,
  0,0,0,0,0,B,I,I,B,0,
  0,0,0,0,0,B,I,I,B,0,
  0,0,0,0,0,0,B,B,0,0,
};
#undef X
#undef B
#undef I
static VGLBitmap VGLMouseLargeAndMask = 
  VGLBITMAP_INITIALIZER(MEMBUF, LARGE_MOUSE_IMG_XSIZE, LARGE_MOUSE_IMG_YSIZE,
                        LargeAndMask);
static VGLBitmap VGLMouseLargeOrMask = 
  VGLBITMAP_INITIALIZER(MEMBUF, LARGE_MOUSE_IMG_XSIZE, LARGE_MOUSE_IMG_YSIZE,
                        LargeOrMask);
static VGLBitmap VGLMouseSmallAndMask = 
  VGLBITMAP_INITIALIZER(MEMBUF, SMALL_MOUSE_IMG_XSIZE, SMALL_MOUSE_IMG_YSIZE,
                        SmallAndMask);
static VGLBitmap VGLMouseSmallOrMask = 
  VGLBITMAP_INITIALIZER(MEMBUF, SMALL_MOUSE_IMG_XSIZE, SMALL_MOUSE_IMG_YSIZE,
                        SmallOrMask);
static VGLBitmap *VGLMouseAndMask, *VGLMouseOrMask;
static int VGLMouseShown = VGL_MOUSEHIDE;
static int VGLMouseXpos = 0;
static int VGLMouseYpos = 0;
static int VGLMouseButtons = 0;
static volatile sig_atomic_t VGLMintpending;
static volatile sig_atomic_t VGLMsuppressint;

#define	INTOFF()	(VGLMsuppressint++)
#define	INTON()		do { 						\
				if (--VGLMsuppressint == 0 && VGLMintpending) \
					VGLMouseAction(0);		\
			} while (0)

int
__VGLMouseMode(int mode)
{
  int oldmode;

  INTOFF();
  oldmode = VGLMouseShown;
  if (mode == VGL_MOUSESHOW) {
    if (VGLMouseShown == VGL_MOUSEHIDE) {
      VGLMouseShown = VGL_MOUSESHOW;
      __VGLBitmapCopy(&VGLVDisplay, VGLMouseXpos, VGLMouseYpos,
                      VGLDisplay, VGLMouseXpos, VGLMouseYpos,
                      VGLMouseAndMask->VXsize, -VGLMouseAndMask->VYsize);
    }
  }
  else {
    if (VGLMouseShown == VGL_MOUSESHOW) {
      VGLMouseShown = VGL_MOUSEHIDE;
      __VGLBitmapCopy(&VGLVDisplay, VGLMouseXpos, VGLMouseYpos,
                      VGLDisplay, VGLMouseXpos, VGLMouseYpos,
                      VGLMouseAndMask->VXsize, VGLMouseAndMask->VYsize);
    }
  }
  INTON();
  return oldmode;
}

void
VGLMouseMode(int mode)
{
  __VGLMouseMode(mode);
}

static void
VGLMouseAction(int dummy)	
{
  struct mouse_info mouseinfo;
  int mousemode;

  if (VGLMsuppressint) {
    VGLMintpending = 1;
    return;
  }
again:
  INTOFF();
  VGLMintpending = 0;
  mouseinfo.operation = MOUSE_GETINFO;
  ioctl(0, CONS_MOUSECTL, &mouseinfo);
  if (VGLMouseXpos != mouseinfo.u.data.x ||
      VGLMouseYpos != mouseinfo.u.data.y) {
    mousemode = __VGLMouseMode(VGL_MOUSEHIDE);
    VGLMouseXpos = mouseinfo.u.data.x;
    VGLMouseYpos = mouseinfo.u.data.y;
    __VGLMouseMode(mousemode);
  }
  VGLMouseButtons = mouseinfo.u.data.buttons;

  /* 
   * Loop to handle any new (suppressed) signals.  This is INTON() without
   * recursion.  !SA_RESTART prevents recursion in signal handling.  So the
   * maximum recursion is 2 levels.
   */
  VGLMsuppressint = 0;
  if (VGLMintpending)
    goto again;
}

void
VGLMouseSetImage(VGLBitmap *AndMask, VGLBitmap *OrMask)
{
  int mousemode;

  mousemode = __VGLMouseMode(VGL_MOUSEHIDE);

  VGLMouseAndMask = AndMask;

  if (VGLMouseOrMask != NULL) {
    free(VGLMouseOrMask->Bitmap);
    free(VGLMouseOrMask);
  }
  VGLMouseOrMask = VGLBitmapCreate(MEMBUF, OrMask->VXsize, OrMask->VYsize, 0);
  VGLBitmapAllocateBits(VGLMouseOrMask);
  VGLBitmapCvt(OrMask, VGLMouseOrMask);

  __VGLMouseMode(mousemode);
}

void
VGLMouseSetStdImage()
{
  if (VGLDisplay->VXsize > 800)
    VGLMouseSetImage(&VGLMouseLargeAndMask, &VGLMouseLargeOrMask);
  else
    VGLMouseSetImage(&VGLMouseSmallAndMask, &VGLMouseSmallOrMask);
}

int
VGLMouseInit(int mode)
{
  struct mouse_info mouseinfo;
  VGLBitmap *ormask;
  int border, error, i, interior;

  switch (VGLModeInfo.vi_mem_model) {
  case V_INFO_MM_PACKED:
  case V_INFO_MM_PLANAR:
    border = 0x0f;
    interior = 0x04;
    break;
  case V_INFO_MM_VGAX:
    border = 0x3f;
    interior = 0x24;
    break;
  default:
    border = BORDER;
    interior = INTERIOR;
    break;
  }
  if (VGLModeInfo.vi_mode == M_BG640x480)
    border = 0;		/* XXX (palette makes 0x04 look like 0x0f) */
  if (getenv("VGLMOUSEBORDERCOLOR") != NULL)
    border = strtoul(getenv("VGLMOUSEBORDERCOLOR"), NULL, 0);
  if (getenv("VGLMOUSEINTERIORCOLOR") != NULL)
    interior = strtoul(getenv("VGLMOUSEINTERIORCOLOR"), NULL, 0);
  ormask = &VGLMouseLargeOrMask;
  for (i = 0; i < ormask->VXsize * ormask->VYsize; i++)
    ormask->Bitmap[i] = ormask->Bitmap[i] == BORDER ?  border :
                        ormask->Bitmap[i] == INTERIOR ? interior : 0;
  ormask = &VGLMouseSmallOrMask;
  for (i = 0; i < ormask->VXsize * ormask->VYsize; i++)
    ormask->Bitmap[i] = ormask->Bitmap[i] == BORDER ?  border :
                        ormask->Bitmap[i] == INTERIOR ? interior : 0;
  VGLMouseSetStdImage();
  mouseinfo.operation = MOUSE_MODE;
  mouseinfo.u.mode.signal = SIGUSR2;
  if ((error = ioctl(0, CONS_MOUSECTL, &mouseinfo)))
    return error;
  signal(SIGUSR2, VGLMouseAction);
  mouseinfo.operation = MOUSE_GETINFO;
  ioctl(0, CONS_MOUSECTL, &mouseinfo);
  VGLMouseXpos = mouseinfo.u.data.x;
  VGLMouseYpos = mouseinfo.u.data.y;
  VGLMouseButtons = mouseinfo.u.data.buttons;
  VGLMouseMode(mode);
  return 0;
}

void
VGLMouseRestore(void)
{
  struct mouse_info mouseinfo;

  INTOFF();
  mouseinfo.operation = MOUSE_GETINFO;
  if (ioctl(0, CONS_MOUSECTL, &mouseinfo) == 0) {
    mouseinfo.operation = MOUSE_MOVEABS;
    mouseinfo.u.data.x = VGLMouseXpos;
    mouseinfo.u.data.y = VGLMouseYpos;
    ioctl(0, CONS_MOUSECTL, &mouseinfo);
  }
  INTON();
}

int
VGLMouseStatus(int *x, int *y, char *buttons)
{
  INTOFF();
  *x =  VGLMouseXpos;
  *y =  VGLMouseYpos;
  *buttons =  VGLMouseButtons;
  INTON();
  return VGLMouseShown;
}

void
VGLMouseFreeze(void)
{
  INTOFF();
}

int
VGLMouseFreezeXY(int x, int y)
{
  INTOFF();
  if (VGLMouseShown != VGL_MOUSESHOW)
    return 0;
  if (x >= VGLMouseXpos && x < VGLMouseXpos + VGLMouseAndMask->VXsize &&
      y >= VGLMouseYpos && y < VGLMouseYpos + VGLMouseAndMask->VYsize &&
      VGLMouseAndMask->Bitmap[(y-VGLMouseYpos)*VGLMouseAndMask->VXsize+
                              (x-VGLMouseXpos)])
    return 1;
  return 0;
}

int
VGLMouseOverlap(int x, int y, int width, int hight)
{
  int overlap;

  if (VGLMouseShown != VGL_MOUSESHOW)
    return 0;
  if (x > VGLMouseXpos)
    overlap = (VGLMouseXpos + VGLMouseAndMask->VXsize) - x;
  else
    overlap = (x + width) - VGLMouseXpos;
  if (overlap <= 0)
    return 0;
  if (y > VGLMouseYpos)
    overlap = (VGLMouseYpos + VGLMouseAndMask->VYsize) - y;
  else
    overlap = (y + hight) - VGLMouseYpos;
  return overlap > 0;
}

void
VGLMouseMerge(int x, int y, int width, byte *line)
{
  int pos, x1, xend, xstart;

  xstart = x;
  if (xstart < VGLMouseXpos)
    xstart = VGLMouseXpos;
  xend = x + width;
  if (xend > VGLMouseXpos + VGLMouseAndMask->VXsize)
    xend = VGLMouseXpos + VGLMouseAndMask->VXsize;
  for (x1 = xstart; x1 < xend; x1++) {
    pos = (y - VGLMouseYpos) * VGLMouseAndMask->VXsize + x1 - VGLMouseXpos;
    if (VGLMouseAndMask->Bitmap[pos])
      bcopy(&VGLMouseOrMask->Bitmap[pos * VGLDisplay->PixelBytes],
            &line[(x1 - x) * VGLDisplay->PixelBytes], VGLDisplay->PixelBytes);
  }
}

void
VGLMouseUnFreeze()
{
  INTON();
}
