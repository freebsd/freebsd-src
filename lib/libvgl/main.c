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
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/fbio.h>
#include <sys/kbio.h>
#include <sys/consio.h>
#include "vgl.h"

#define min(x, y)	(((x) < (y)) ? (x) : (y))
#define max(x, y)	(((x) > (y)) ? (x) : (y))

VGLBitmap *VGLDisplay;
VGLBitmap VGLVDisplay;
video_info_t VGLModeInfo;
video_adapter_info_t VGLAdpInfo;
byte *VGLBuf;

static int VGLMode;
static int VGLOldMode;
static size_t VGLBufSize;
static byte *VGLMem = MAP_FAILED;
static int VGLSwitchPending;
static int VGLAbortPending;
static int VGLOnDisplay;
static unsigned int VGLCurWindow;
static int VGLInitDone = 0;
static video_info_t VGLOldModeInfo;
static vid_info_t VGLOldVInfo;
static int VGLOldVXsize;

void
VGLEnd()
{
struct vt_mode smode;
  int size[3];

  if (!VGLInitDone)
    return;
  VGLInitDone = 0;
  signal(SIGUSR1, SIG_IGN);
  signal(SIGUSR2, SIG_IGN);
  VGLSwitchPending = 0;
  VGLAbortPending = 0;
  VGLMouseMode(VGL_MOUSEHIDE);

  if (VGLMem != MAP_FAILED) {
    VGLClear(VGLDisplay, 0);
    munmap(VGLMem, VGLAdpInfo.va_window_size);
  }

  ioctl(0, FBIO_SETLINEWIDTH, &VGLOldVXsize);

  if (VGLOldMode >= M_VESA_BASE)
    ioctl(0, _IO('V', VGLOldMode - M_VESA_BASE), 0);
  else
    ioctl(0, _IO('S', VGLOldMode), 0);
  if (VGLOldModeInfo.vi_flags & V_INFO_GRAPHICS) {
    size[0] = VGLOldVInfo.mv_csz;
    size[1] = VGLOldVInfo.mv_rsz;
    size[2] = VGLOldVInfo.font_size;
    ioctl(0, KDRASTER, size);
  }
  if (VGLModeInfo.vi_mem_model != V_INFO_MM_DIRECT)
    ioctl(0, KDDISABIO, 0);
  ioctl(0, KDSETMODE, KD_TEXT);
  smode.mode = VT_AUTO;
  ioctl(0, VT_SETMODE, &smode);
  if (VGLBuf)
    free(VGLBuf);
  VGLBuf = NULL;
  free(VGLDisplay);
  VGLDisplay = NULL;
  VGLKeyboardEnd();
}

static void 
VGLAbort(int arg)
{
  sigset_t mask;

  VGLAbortPending = 1;
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGUSR2, SIG_IGN);
  if (arg == SIGBUS || arg == SIGSEGV) {
    signal(arg, SIG_DFL);
    sigemptyset(&mask);
    sigaddset(&mask, arg);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    VGLEnd();
    kill(getpid(), arg);
  }
}

static void
VGLSwitch(int arg __unused)
{
  if (!VGLOnDisplay)
    VGLOnDisplay = 1;
  else
    VGLOnDisplay = 0;
  VGLSwitchPending = 1;
  signal(SIGUSR1, VGLSwitch);
}

int
VGLInit(int mode)
{
  struct vt_mode smode;
  int adptype, depth;

  if (VGLInitDone)
    return -1;

  signal(SIGUSR1, VGLSwitch);
  signal(SIGINT, VGLAbort);
  signal(SIGTERM, VGLAbort);
  signal(SIGSEGV, VGLAbort);
  signal(SIGBUS, VGLAbort);
  signal(SIGUSR2, SIG_IGN);

  VGLOnDisplay = 1;
  VGLSwitchPending = 0;
  VGLAbortPending = 0;

  if (ioctl(0, CONS_GET, &VGLOldMode) || ioctl(0, CONS_CURRENT, &adptype))
    return -1;
  if (IOCGROUP(mode) == 'V')	/* XXX: this is ugly */
    VGLModeInfo.vi_mode = (mode & 0x0ff) + M_VESA_BASE;
  else
    VGLModeInfo.vi_mode = mode & 0x0ff;
  if (ioctl(0, CONS_MODEINFO, &VGLModeInfo))	/* FBIO_MODEINFO */
    return -1;

  /* Save info for old mode to restore font size if old mode is graphics. */
  VGLOldModeInfo.vi_mode = VGLOldMode;
  if (ioctl(0, CONS_MODEINFO, &VGLOldModeInfo))
    return -1;
  VGLOldVInfo.size = sizeof(VGLOldVInfo);
  if (ioctl(0, CONS_GETINFO, &VGLOldVInfo))
    return -1;

  VGLDisplay = (VGLBitmap *)malloc(sizeof(VGLBitmap));
  if (VGLDisplay == NULL)
    return -2;

  if (VGLModeInfo.vi_mem_model != V_INFO_MM_DIRECT && ioctl(0, KDENABIO, 0)) {
    free(VGLDisplay);
    return -3;
  }

  VGLInitDone = 1;

  /*
   * vi_mem_model specifies the memory model of the current video mode
   * in -CURRENT.
   */
  switch (VGLModeInfo.vi_mem_model) {
  case V_INFO_MM_PLANAR:
    /* we can handle EGA/VGA planner modes only */
    if (VGLModeInfo.vi_depth != 4 || VGLModeInfo.vi_planes != 4
	|| (adptype != KD_EGA && adptype != KD_VGA)) {
      VGLEnd();
      return -4;
    }
    VGLDisplay->Type = VIDBUF4;
    VGLDisplay->PixelBytes = 1;
    break;
  case V_INFO_MM_PACKED:
    /* we can do only 256 color packed modes */
    if (VGLModeInfo.vi_depth != 8) {
      VGLEnd();
      return -4;
    }
    VGLDisplay->Type = VIDBUF8;
    VGLDisplay->PixelBytes = 1;
    break;
  case V_INFO_MM_VGAX:
    VGLDisplay->Type = VIDBUF8X;
    VGLDisplay->PixelBytes = 1;
    break;
  case V_INFO_MM_DIRECT:
    VGLDisplay->PixelBytes = VGLModeInfo.vi_pixel_size;
    switch (VGLDisplay->PixelBytes) {
    case 2:
      VGLDisplay->Type = VIDBUF16;
      break;
    case 3:
      VGLDisplay->Type = VIDBUF24;
      break;
    case 4:
      VGLDisplay->Type = VIDBUF32;
      break;
    default:
      VGLEnd();
      return -4;
    }
    break;
  default:
    VGLEnd();
    return -4;
  }

  ioctl(0, VT_WAITACTIVE, 0);
  ioctl(0, KDSETMODE, KD_GRAPHICS);
  if (ioctl(0, mode, 0)) {
    VGLEnd();
    return -5;
  }
  if (ioctl(0, CONS_ADPINFO, &VGLAdpInfo)) {	/* FBIO_ADPINFO */
    VGLEnd();
    return -6;
  }

  /*
   * Calculate the shadow screen buffer size.  In -CURRENT, va_buffer_size
   * always holds the entire frame buffer size, wheather it's in the linear
   * mode or windowed mode.  
   *     VGLBufSize = VGLAdpInfo.va_buffer_size;
   * In -STABLE, va_buffer_size holds the frame buffer size, only if
   * the linear frame buffer mode is supported. Otherwise the field is zero.
   * We shall calculate the minimal size in this case:
   *     VGLAdpInfo.va_line_width*VGLModeInfo.vi_height*VGLModeInfo.vi_planes
   * or
   *     VGLAdpInfo.va_window_size*VGLModeInfo.vi_planes;
   * Use whichever is larger.
   */
  if (VGLAdpInfo.va_buffer_size != 0)
    VGLBufSize = VGLAdpInfo.va_buffer_size;
  else
    VGLBufSize = max(VGLAdpInfo.va_line_width*VGLModeInfo.vi_height,
		     VGLAdpInfo.va_window_size)*VGLModeInfo.vi_planes;
  /*
   * The above is for old -CURRENT.  Current -CURRENT since r203535 and/or
   * r248799 restricts va_buffer_size to the displayed size in VESA modes to
   * avoid wasting kva for mapping unused parts of the frame buffer.  But all
   * parts were usable here.  Applying the same restriction to user mappings
   * makes our virtualization useless and breaks our panning, but large frame
   * buffers are also difficult for us to manage (clearing and switching may
   * be too slow, and malloc() may fail).  Restrict ourselves similarly to
   * get the same efficiency and bugs for all kernels.
   */
  if (VGLModeInfo.vi_mode >= M_VESA_BASE)
    VGLBufSize = VGLAdpInfo.va_line_width*VGLModeInfo.vi_height*
                 VGLModeInfo.vi_planes;
  VGLBuf = malloc(VGLBufSize);
  if (VGLBuf == NULL) {
    VGLEnd();
    return -7;
  }

#ifdef LIBVGL_DEBUG
  fprintf(stderr, "VGLBufSize:0x%x\n", VGLBufSize);
#endif

  /* see if we are in the windowed buffer mode or in the linear buffer mode */
  if (VGLBufSize/VGLModeInfo.vi_planes > VGLAdpInfo.va_window_size) {
    switch (VGLDisplay->Type) {
    case VIDBUF4:
      VGLDisplay->Type = VIDBUF4S;
      break;
    case VIDBUF8:
      VGLDisplay->Type = VIDBUF8S;
      break;
    case VIDBUF16:
      VGLDisplay->Type = VIDBUF16S;
      break;
    case VIDBUF24:
      VGLDisplay->Type = VIDBUF24S;
      break;
    case VIDBUF32:
      VGLDisplay->Type = VIDBUF32S;
      break;
    default:
      VGLEnd();
      return -8;
    }
  }

  VGLMode = mode;
  VGLCurWindow = 0;

  VGLDisplay->Xsize = VGLModeInfo.vi_width;
  VGLDisplay->Ysize = VGLModeInfo.vi_height;
  depth = VGLModeInfo.vi_depth;
  if (depth == 15)
    depth = 16;
  VGLOldVXsize =
  VGLDisplay->VXsize = VGLAdpInfo.va_line_width
			   *8/(depth/VGLModeInfo.vi_planes);
  VGLDisplay->VYsize = VGLBufSize/VGLModeInfo.vi_planes/VGLAdpInfo.va_line_width;
  VGLDisplay->Xorigin = 0;
  VGLDisplay->Yorigin = 0;

  VGLMem = (byte*)mmap(0, VGLAdpInfo.va_window_size, PROT_READ|PROT_WRITE,
		       MAP_FILE | MAP_SHARED, 0, 0);
  if (VGLMem == MAP_FAILED) {
    VGLEnd();
    return -7;
  }
  VGLDisplay->Bitmap = VGLMem;

  VGLVDisplay = *VGLDisplay;
  VGLVDisplay.Type = MEMBUF;
  if (VGLModeInfo.vi_depth < 8)
    VGLVDisplay.Bitmap = malloc(2 * VGLBufSize);
  else
    VGLVDisplay.Bitmap = VGLBuf;

  VGLSavePalette();

#ifdef LIBVGL_DEBUG
  fprintf(stderr, "va_line_width:%d\n", VGLAdpInfo.va_line_width);
  fprintf(stderr, "VGLXsize:%d, Ysize:%d, VXsize:%d, VYsize:%d\n",
	  VGLDisplay->Xsize, VGLDisplay->Ysize, 
	  VGLDisplay->VXsize, VGLDisplay->VYsize);
#endif

  smode.mode = VT_PROCESS;
  smode.waitv = 0;
  smode.relsig = SIGUSR1;
  smode.acqsig = SIGUSR1;
  smode.frsig  = SIGINT;	
  if (ioctl(0, VT_SETMODE, &smode)) {
    VGLEnd();
    return -9;
  }
  VGLTextSetFontFile((byte*)0);
  VGLClear(VGLDisplay, 0);
  return 0;
}

void
VGLCheckSwitch()
{
  if (VGLAbortPending) {
    VGLEnd();
    exit(0);
  }
  while (VGLSwitchPending) {
    VGLSwitchPending = 0;
    if (VGLOnDisplay) {
      if (VGLModeInfo.vi_mem_model != V_INFO_MM_DIRECT)
        ioctl(0, KDENABIO, 0);
      ioctl(0, KDSETMODE, KD_GRAPHICS);
      ioctl(0, VGLMode, 0);
      VGLCurWindow = 0;
      VGLMem = (byte*)mmap(0, VGLAdpInfo.va_window_size, PROT_READ|PROT_WRITE,
			   MAP_FILE | MAP_SHARED, 0, 0);

      /* XXX: what if mmap() has failed! */
      VGLDisplay->Type = VIDBUF8;	/* XXX */
      switch (VGLModeInfo.vi_mem_model) {
      case V_INFO_MM_PLANAR:
	if (VGLModeInfo.vi_depth == 4 && VGLModeInfo.vi_planes == 4) {
	  if (VGLBufSize/VGLModeInfo.vi_planes > VGLAdpInfo.va_window_size)
	    VGLDisplay->Type = VIDBUF4S;
	  else
	    VGLDisplay->Type = VIDBUF4;
	} else {
	  /* shouldn't be happening */
	}
        break;
      case V_INFO_MM_PACKED:
	if (VGLModeInfo.vi_depth == 8) {
	  if (VGLBufSize/VGLModeInfo.vi_planes > VGLAdpInfo.va_window_size)
	    VGLDisplay->Type = VIDBUF8S;
	  else
	    VGLDisplay->Type = VIDBUF8;
	}
        break;
      case V_INFO_MM_VGAX:
	VGLDisplay->Type = VIDBUF8X;
	break;
      case V_INFO_MM_DIRECT:
	switch (VGLModeInfo.vi_pixel_size) {
	  case 2:
	    if (VGLBufSize/VGLModeInfo.vi_planes > VGLAdpInfo.va_window_size)
	      VGLDisplay->Type = VIDBUF16S;
	    else
	      VGLDisplay->Type = VIDBUF16;
	    break;
	  case 3:
	    if (VGLBufSize/VGLModeInfo.vi_planes > VGLAdpInfo.va_window_size)
	      VGLDisplay->Type = VIDBUF24S;
	    else
	      VGLDisplay->Type = VIDBUF24;
	    break;
	  case 4:
	    if (VGLBufSize/VGLModeInfo.vi_planes > VGLAdpInfo.va_window_size)
	      VGLDisplay->Type = VIDBUF32S;
	    else
	      VGLDisplay->Type = VIDBUF32;
	    break;
	  default:
	  /* shouldn't be happening */
          break;
        }
      default:
	/* shouldn't be happening */
        break;
      }

      VGLDisplay->Bitmap = VGLMem;
      VGLDisplay->Xsize = VGLModeInfo.vi_width;
      VGLDisplay->Ysize = VGLModeInfo.vi_height;
      VGLSetVScreenSize(VGLDisplay, VGLDisplay->VXsize, VGLDisplay->VYsize);
      VGLRestoreBlank();
      VGLRestoreBorder();
      VGLMouseRestore();
      VGLPanScreen(VGLDisplay, VGLDisplay->Xorigin, VGLDisplay->Yorigin);
      VGLBitmapCopy(&VGLVDisplay, 0, 0, VGLDisplay, 0, 0, 
                    VGLDisplay->VXsize, VGLDisplay->VYsize);
      VGLRestorePalette();
      ioctl(0, VT_RELDISP, VT_ACKACQ);
    }
    else {
      VGLMem = MAP_FAILED;
      munmap(VGLDisplay->Bitmap, VGLAdpInfo.va_window_size);
      ioctl(0, VGLOldMode, 0);
      ioctl(0, KDSETMODE, KD_TEXT);
      if (VGLModeInfo.vi_mem_model != V_INFO_MM_DIRECT)
        ioctl(0, KDDISABIO, 0);
      ioctl(0, VT_RELDISP, VT_TRUE);
      VGLDisplay->Bitmap = VGLBuf;
      VGLDisplay->Type = MEMBUF;
      VGLDisplay->Xsize = VGLDisplay->VXsize;
      VGLDisplay->Ysize = VGLDisplay->VYsize;
      while (!VGLOnDisplay) pause();
    }
  }
}

int
VGLSetSegment(unsigned int offset)
{
  if (offset/VGLAdpInfo.va_window_size != VGLCurWindow) {
    ioctl(0, CONS_SETWINORG, offset);		/* FBIO_SETWINORG */
    VGLCurWindow = offset/VGLAdpInfo.va_window_size;
  }
  return (offset%VGLAdpInfo.va_window_size);
}

int
VGLSetVScreenSize(VGLBitmap *object, int VXsize, int VYsize)
{
  int depth;

  if (VXsize < object->Xsize || VYsize < object->Ysize)
    return -1;
  if (object->Type == MEMBUF)
    return -1;
  if (ioctl(0, FBIO_SETLINEWIDTH, &VXsize))
    return -1;
  ioctl(0, CONS_ADPINFO, &VGLAdpInfo);	/* FBIO_ADPINFO */
  depth = VGLModeInfo.vi_depth;
  if (depth == 15)
    depth = 16;
  object->VXsize = VGLAdpInfo.va_line_width
			   *8/(depth/VGLModeInfo.vi_planes);
  object->VYsize = VGLBufSize/VGLModeInfo.vi_planes/VGLAdpInfo.va_line_width;
  if (VYsize < object->VYsize)
    object->VYsize = VYsize;

#ifdef LIBVGL_DEBUG
  fprintf(stderr, "new size: VGLXsize:%d, Ysize:%d, VXsize:%d, VYsize:%d\n",
	  object->Xsize, object->Ysize, object->VXsize, object->VYsize);
#endif

  return 0;
}

int
VGLPanScreen(VGLBitmap *object, int x, int y)
{
  video_display_start_t origin;

  if (x < 0 || x + object->Xsize > object->VXsize
      || y < 0 || y + object->Ysize > object->VYsize)
    return -1;
  if (object->Type == MEMBUF)
    return 0;
  origin.x = x;
  origin.y = y;
  if (ioctl(0, FBIO_SETDISPSTART, &origin))
    return -1;
  object->Xorigin = x;
  object->Yorigin = y;

#ifdef LIBVGL_DEBUG
  fprintf(stderr, "new origin: (%d, %d)\n", x, y);
#endif

  return 0;
}
