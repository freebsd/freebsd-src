/*-
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
 *    derived from this software withough specific prior written permission
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
 *  $Id: main.c,v 1.14 1997/08/15 12:32:59 sos Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <machine/console.h>
#include "vgl.h"

VGLBitmap *VGLDisplay;

static int VGLMode;
static int VGLOldMode;
static byte *VGLBuf;
static byte *VGLMem;
static int VGLSwitchPending;
static int VGLOnDisplay;

void
VGLEnd()
{
struct vt_mode smode;

/*
  while (!VGLOnDisplay) pause(); 
  VGLCheckSwitch();;
*/
  outb(0x3c4, 0x02);
  outb(0x3c5, 0x0f);
  bzero(VGLMem, 64*1024);
  ioctl(0, _IO('S', VGLOldMode), 0);
  ioctl(0, KDDISABIO, 0);
  ioctl(0, KDSETMODE, KD_TEXT);
  smode.mode = VT_AUTO;
  ioctl(0, VT_SETMODE, &smode);
  free(VGLBuf);
  free(VGLDisplay);
}

static void 
VGLAbort()
{
  VGLEnd();
  exit(0);
}

static void
VGLSwitch()
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
  struct winsize winsz;
  int error;

  signal(SIGUSR1, VGLSwitch);
  signal(SIGINT, VGLAbort);
  signal(SIGSEGV, VGLAbort);
  signal(SIGBUS, VGLAbort);

  VGLOnDisplay = 1;
  VGLSwitchPending = 0;

  ioctl(0, CONS_GET, &VGLOldMode);

  VGLMem = (byte*)mmap(0, 0x10000, PROT_READ|PROT_WRITE, MAP_FILE,
                          open("/dev/mem", O_RDWR), 0xA0000);
  if (VGLMem <= (byte*)0)
    return 1;

  VGLBuf = (byte*)malloc(256*1024);
  if (VGLBuf == NULL)
    return 1;

  VGLDisplay = (VGLBitmap*) malloc(sizeof(VGLBitmap));
  if (VGLDisplay == NULL) {
    free(VGLBuf);
    return 1;
  }

  switch (mode) {
  case SW_BG640x480: case SW_CG640x480:
    VGLDisplay->Type = VIDBUF4;
    break;
  case SW_VGA_CG320:
    VGLDisplay->Type = VIDBUF8;
    break;
  case SW_VGA_MODEX:
    VGLDisplay->Type = VIDBUF8X;
    break;
  default:
    VGLEnd();
    return 1;
  }

  if ((error = ioctl(0, KDENABIO, 0)))
    return error;

  ioctl(0, VT_WAITACTIVE, 0);
  ioctl(0, KDSETMODE, KD_GRAPHICS);
  if ((error = ioctl(0, mode, 0))) {
    ioctl(0, KDSETMODE, KD_TEXT);
    ioctl(0, KDDISABIO, 0);
    return error;
  }

  VGLMode = mode;

  outb(0x3c4, 0x02);
  outb(0x3c5, 0x0f);
  bzero(VGLMem, 64*1024);

  if (ioctl(0, TIOCGWINSZ, &winsz)) {
    VGLEnd();
    return 1;
  }

  VGLDisplay->Bitmap = VGLMem;
  VGLDisplay->Xsize = winsz.ws_xpixel;
  VGLDisplay->Ysize = winsz.ws_ypixel;
  VGLSavePalette();

  smode.mode = VT_PROCESS;
  smode.waitv = 0;
  smode.relsig = SIGUSR1;
  smode.acqsig = SIGUSR1;
  smode.frsig  = SIGINT;	
  if (ioctl(0, VT_SETMODE, &smode) == -1) {
    VGLEnd();
    return 1;
  }
  VGLTextSetFontFile((byte*)0);
  return 0;
}

void
VGLCheckSwitch()
{
  if (VGLSwitchPending) {
    int i;

    VGLSwitchPending = 0;
    if (VGLOnDisplay) {
      ioctl(0, KDENABIO, 0);
      ioctl(0, KDSETMODE, KD_GRAPHICS);
      ioctl(0, VGLMode, 0);
      outb(0x3c6, 0xff);
      for (i=0; i<4; i++) {
        outb(0x3c4, 0x02);
        outb(0x3c5, 0x01<<i);
        bcopy(&VGLBuf[i*64*1024], VGLMem, 64*1024);
      }
      VGLRestorePalette();
      ioctl(0, VT_RELDISP, VT_ACKACQ);
      VGLDisplay->Bitmap = VGLMem;
      switch (VGLMode) {
      case SW_BG640x480: case SW_CG640x480:
        VGLDisplay->Type = VIDBUF4;
        break;
      case SW_VGA_CG320:
        VGLDisplay->Type = VIDBUF8;
        break;
      case SW_VGA_MODEX:
        VGLDisplay->Type = VIDBUF8X;
        break;
      default:
        VGLDisplay->Type = VIDBUF8;			/* XXX */
        break;
      }
    }
    else {
      for (i=0; i<4; i++) {
        outb(0x3ce, 0x04);
        outb(0x3cf, i);
        bcopy(VGLMem, &VGLBuf[i*64*1024], 64*1024);
      }
      ioctl(0, VGLOldMode, 0);
      ioctl(0, KDSETMODE, KD_TEXT);
      ioctl(0, KDDISABIO, 0);
      ioctl(0, VT_RELDISP, VT_TRUE);
      VGLDisplay->Bitmap = VGLBuf;
      VGLDisplay->Type = MEMBUF;
    }
  }
  while (!VGLOnDisplay) pause();
}
  
