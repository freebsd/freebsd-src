/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI tty.c,v 2.4 1996/04/08 22:03:27 prb Exp
 *
 * $FreeBSD$
 */

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#ifdef __FreeBSD__
# include <osreldate.h>
# if __FreeBSD_version >= 500014
#   include <sys/kbio.h>
# else
#   include <machine/console.h>
# endif
#else
# ifdef __NetBSD__
#  include "machine/pccons.h"
# else	/* BSD/OS */
#  include "/sys/i386/isa/pcconsioctl.h"
# endif
#endif

#ifndef NO_X
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

#include "doscmd.h"
#include "AsyncIO.h"
#include "font8x8.h"
#include "font8x14.h"
#include "font8x16.h"
#include "mouse.h"
#include "trap.h"
#include "tty.h"
#include "video.h"

#ifndef NO_X
static int show = 1;
#endif
static int blink = 1;
int flipdelete = 0;		/* Flip meaning of delete and backspace */
static u_short break_code = 0x00;
static u_short scan_code = 0x00;
int height;
int width;
int vattr;
const char *xfont = 0;

#ifndef NO_X
Display *dpy;
Window win;
XFontStruct *font;
XImage *xi = 0;
Visual *visual;
unsigned int depth;
unsigned long black;
unsigned long white;
int FW, FH, FD;
GC gc;
GC cgc;
int xfd;

/* LUT for the vram -> XImage conversion */
u_int8_t lut[4][256][8];

/* X pixel values for the RGB triples */
unsigned long pixels[16];
#endif

typedef struct TextLine {
    u_short	*data;
    u_char	max_length;	/* Not used, but here for future use */
    u_char	changed:1;
} TextLine;
TextLine *lines = NULL;

int kbd_fd = -1;
int kbd_read = 0;

static struct termios tty_cook, tty_raw;

#define	row (CursRow0)
#define	col (CursCol0)

/* Local functions */
static void	_kbd_event(int, int, void *, regcontext_t *);
static void	Failure(void *);
static void	SetVREGCur(void);
static void	debug_event(int, int, void *, regcontext_t *);
static unsigned char	inb_port60(int);
static int	inrange(int, int, int);
static void	kbd_event(int, int, void *, regcontext_t *);
static u_short	read_raw_kbd(int, u_short *);
static void	setgc(u_short);
static void	video_async_event(int, int, void *, regcontext_t *);

#ifndef NO_X
static void	dac2rgb(XColor *, int);
static void	prepare_lut(void);
static void	putchar_graphics(int, int, int);
static void	tty_rwrite_graphics(int, int, int);
static int	video_event(XEvent *ev);
static void	video_update_graphics(void);
static void	video_update_text(void);
static void	vram2ximage(void);
#endif

#define	PEEKSZ	16

#define	K_NEXT		*(u_short *)0x41a
#define	K_FREE		*(u_short *)0x41c
#define	K_BUFSTARTP	*(u_short *)0x480
#define	K_BUFENDP	*(u_short *)0x482
#define	K_BUFSTART	((u_short *)(0x400 + *(u_short *)0x480))
#define	K_BUFEND	((u_short *)(0x400 + *(u_short *)0x482))
#define	K_BUF(i)	*((u_short *)((u_char *)0x400 + (i)))

#define	K1_STATUS	BIOSDATA[0x17]
#define	K1_RSHIFT	0x01
#define	K1_LSHIFT	0x02
#define	K1_SHIFT	0x03
#define	K1_CTRL		0x04
#define	K1_ALT		0x08
#define	K1_SLOCK	0x10		/* Active */
#define	K1_NLOCK	0x20		/* Active */
#define	K1_CLOCK	0x40		/* Active */
#define	K1_INSERT	0x80		/* Active */

#define	K2_STATUS	BIOSDATA[0x18]
#define	K2_LCTRL	0x01
#define	K2_LALT		0x02
#define	K2_SYSREQ	0x04
#define	K2_PAUSE	0x08
#define	K2_SLOCK	0x10		/* Actually held down */
#define	K2_NLOCK	0x20		/* Actually held down */
#define	K2_CLOCK	0x40		/* Actually held down */
#define	K2_INSERT	0x80		/* Actually held down */

#define	K3_STATUS	BIOSDATA[0x96]
#define	K3_E1		0x01		/* Last code read was e1 */
#define	K3_E2		0x02		/* Last code read was e2 */
#define	K3_RCTRL	0x04
#define	K3_RALT		0x08
#define	K3_ENHANCED	0x10
#define	K3_FORCENLOCK	0x20
#define	K3_TWOBYTE	0x40		/* last code was first of 2 */
#define	K3_READID	0x80		/* read ID in progress */

#define	K4_STATUS	BIOSDATA[0x97]
#define	K4_SLOCK_LED	0x01
#define	K4_NLOCK_LED	0x02
#define	K4_CLOCK_LED	0x04
#define	K4_ACK		0x10		/* ACK recieved from keyboard */
#define	K4_RESEND	0x20		/* RESEND recieved from keyboard */
#define	K4_LED		0x40		/* LED update in progress */
#define	K4_ERROR	0x80

static void
Failure(void *arg __unused)
{
        fprintf(stderr, "X Connection shutdown\n");
	quit(1);
}

static void
SetVREGCur()
{
    int cp = row * width + col;
    VGA_CRTC[CRTC_CurLocHi] = cp >> 8;
    VGA_CRTC[CRTC_CurLocLo] = cp & 0xff;
}

static void
console_denit(void *arg)
{
    int fd = *(int *)arg;

#ifdef __FreeBSD__
    if (ioctl(fd, KDSKBMODE, K_XLATE))
	perror("KDSKBMODE/K_XLATE");
#else
# ifdef __NetBSD__
    if (ioctl(fd, CONSOLE_X_MODE_OFF, 0))
	perror("CONSOLE_X_MODE_OFF");
# else /* BSD/OS */
    if (ioctl(fd, PCCONIOCCOOK, 0))
	perror("PCCONIOCCOOK");
# endif
#endif
    if (tcsetattr(fd, TCSANOW, &tty_cook))
	perror("tcsetattr");
}

void
_kbd_event(int fd, int cond, void *arg __unused, regcontext_t *REGS __unused)
{
    if (!(cond & AS_RD))
	return;
    printf("_kbd_event: fd=%d\n", fd);
    kbd_read = 1;
}

void
console_init()
{
    int fd;
    caddr_t addr;

    if ((fd = open("/dev/vga", 2)) < 0) {
	perror("/dev/vga");
	quit(1);
    }
    addr = mmap((caddr_t)0xA0000, 5 * 64 * 1024,
		PROT_EXEC | PROT_READ | PROT_WRITE,
		MAP_FILE | MAP_FIXED | MAP_SHARED,
		fd, 0);
    if (addr != (caddr_t)0xA0000) {
	perror("mmap");
	quit(1);
    }

#if 0
    addr = mmap((caddr_t)0x100000 - 0x1000, 0x1000,
		PROT_EXEC | PROT_READ | PROT_WRITE,
		MAP_FILE | MAP_FIXED | MAP_SHARED,
		fd, 0);
    if (addr != (caddr_t)(0x100000 - 0x1000)) {
	perror("mmap");
	quit(1);
    }
#endif

    if ((fd = open(_PATH_CONSOLE, 2)) < 0) {
	perror(_PATH_CONSOLE);
	quit(1);
    }

    fd = squirrel_fd(fd);
    kbd_fd = fd;

#ifdef __FreeBSD__
    if (ioctl(fd, KDSKBMODE, K_RAW)) {
	perror("KDSKBMODE/K_RAW");
	quit(1);    
    }
#else
# ifdef __NetBSD__
    if (ioctl(fd, CONSOLE_X_MODE_ON, 0)) {
	perror("CONSOLE_X_MODE_ON");
	quit(1);    
    }
# else /* BSD/OS */
      if (ioctl(fd, PCCONIOCRAW, 0)) {
        perror("PCCONIOCRAW");
        quit(1);    
      }
# endif
#endif

    call_on_quit(console_denit, &kbd_fd);

    if (fcntl(fd, F_SETFL, O_NDELAY|O_ASYNC) < 0) {
	perror("fcntl");
	quit(1);    
    }
    if (tcgetattr(fd, &tty_cook)) {
	perror("tcgetattr");
	quit(1);    
    }
    tty_raw = tty_cook;
    cfmakeraw(&tty_raw);
    if (tcsetattr(fd, TCSANOW, &tty_raw)) {
	perror("tcsetattr");
	quit(1);    
    }

#if 0
    _RegisterIO(STDIN_FILENO, debug_event, 0, Failure);
    _RegisterIO(fd, kbd_event, 0, Failure);
#endif
    _RegisterIO(fd, _kbd_event, 0, Failure);
}

void
video_setborder(int color)
{
#ifndef NO_X
	XSetWindowBackground(dpy, win, pixels[color & 0xf]);
#endif
}
void
video_blink(int mode)
{
	blink = mode;
}

static void
setgc(u_short attr)
{
#ifndef NO_X
	XGCValues v;
	if (blink && !show && (attr & 0x8000))
		v.foreground = pixels[(attr >> 12) & 0x07];
	else
		v.foreground = pixels[(attr >> 8) & 0x0f];

	v.background = pixels[(attr >> 12) & (blink ? 0x07 : 0x0f)];
	XChangeGC(dpy, gc, GCForeground|GCBackground, &v);
#endif
}

void
video_update(regcontext_t *REGS __unused)
{
#ifndef NO_X
	static int icnt = 3;

	if (kbd_read)
	    kbd_event(kbd_fd, AS_RD, 0, REGS);

    	if (--icnt == 0) {
	    icnt = 3;

	    lpt_poll();		/* Handle timeout on lpt code */

	    /* quick and dirty */
	    if (VGA_ATC[ATC_ModeCtrl] & 1)
		video_update_graphics();
	    else
		video_update_text();
	}
#endif
}

#ifndef NO_X
static void
video_update_graphics()
{
    vram2ximage();
    
    XPutImage(dpy, win, DefaultGC(dpy, DefaultScreen(dpy)),
	      xi, 0, 0, 0, 0, width, height);
    XFlush(dpy);
    
    return;
}

static void
video_update_text()
{
    static int or = -1;
    static int oc = -1;


    static char buf[256];
    int r, c;
    int attr = vmem[0] & 0xff00;
    XGCValues v;

    if (xmode) {
	wakeup_poll();	/* Wake up anyone waiting on kbd poll */

	show ^= 1;

	setgc(attr);

	for (r = 0; r < height; ++r) {
	    int cc = 0;

	    if (!lines[r].changed) {
		if ((r == or || r == row) && (or != row || oc != col))
		    lines[r].changed = 1;
		else {
		    for (c = 0; c < width; ++c) {
			if (lines[r].data[c] != vmem[r * width + c]) {
			    lines[r].changed = 1;
			    break;
			}
			if (blink && lines[r].data[c] & 0x8000) {
			    lines[r].changed = 1;
			    break;
			}
		    }
		}
	    }

	    if (!lines[r].changed)
		continue;

	    reset_poll();
	    lines[r].changed = 0;
	    memcpy(lines[r].data,
		   &vmem[r * width], sizeof(u_short) * width);

	    for (c = 0; c < width; ++c) {
		int cv = vmem[r * width + c];
		if ((cv & 0xff00) != attr) {
		    if (cc < c)
			XDrawImageString(dpy, win, gc,
					 2 + cc * FW,
					 2 + (r + 1) * FH,
					 buf + cc, c - cc);
		    cc = c;
		    attr = cv  & 0xff00;
		    setgc(attr);
		}
		buf[c] = (cv & 0xff) ? cv & 0xff : ' ';
	    }
	    if (cc < c) {
		XDrawImageString(dpy, win, gc,
				 2 + cc * FW,
				 2 + (r + 1) * FH,
				 buf + cc, c - cc);
	    }
	}
	or = row;
	oc = col;

	if (CursStart <= CursEnd && CursEnd <= FH &&
	    show && row < height && col < width) {

	    attr = vmem[row * width + col] & 0xff00;
	    v.foreground = pixels[(attr >> 8) & 0x0f] ^
		pixels[(attr >> 12) & (blink ? 0x07 : 0x0f)];
	    if (v.foreground) {
		v.function = GXxor;
	    } else {
		v.foreground = pixels[7];
		v.function = GXcopy;
	    }
	    XChangeGC(dpy, cgc, GCForeground | GCFunction, &v);
	    XFillRectangle(dpy, win, cgc,
			   2 + col * FW,
			   2 + row * FH + CursStart + FD,
			   FW, CursEnd + 1 - CursStart);
	}

	if (mouse_status.installed && mouse_status.show) {
	    c = mouse_status.x / mouse_status.hmickey;
	    r = mouse_status.y / mouse_status.vmickey;

	    lines[r].changed = 1;
	    attr = vmem[r * width + c] & 0xff00;
	    v.foreground = pixels[(attr >> 8) & 0x0f] ^
		pixels[(attr >> 12) & 0x0f];
	    if (v.foreground) {
		v.function = GXxor;
	    } else {
		v.foreground = pixels[7];
		v.function = GXcopy;
	    }
	    XChangeGC(dpy, cgc, GCForeground | GCFunction, &v);
	    XFillRectangle(dpy, win, cgc,
			   2 + c * FW,
			   2 + r * FH + 2,
			   FW, FH);
	}
		
	XFlush(dpy);
    }
}

/* Convert the contents of the video RAM into an XImage.

   Bugs: - The function is way too slow.
	 - It only works for the 16 color modes.
	 - It only works on 15/16-bit TrueColor visuals. */
static void
vram2ximage()
{
    int i, x, y, yoffset;
    u_int16_t *image = (u_int16_t *)xi->data;

    yoffset = 0;
    for (y = 0; y < height; y++) {
	yoffset += width / 8;
	for (x = 0; x < width; x += 8) {
	    int offset = yoffset + x / 8;
	    for (i = 0; i < 8; i++) {
		int color = lut[0][vplane0[offset]][i] |
		    lut[1][vplane1[offset]][i] |
		    lut[2][vplane2[offset]][i] |
		    lut[3][vplane3[offset]][i];
		*image++ = (u_int16_t)pixels[color];
	    }	    
	}
    }
    
    return;
}
#endif

static u_short Ascii2Scan[] = {
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0x000e, 0x000f, 0xffff, 0xffff, 0xffff, 0x001c, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0x0001, 0xffff, 0xffff, 0xffff, 0xffff,
 0x0039, 0x0102, 0x0128, 0x0104, 0x0105, 0x0106, 0x0108, 0x0028,
 0x010a, 0x010b, 0x0109, 0x010d, 0x0033, 0x000c, 0x0034, 0x0035,
 0x000b, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
 0x0009, 0x000a, 0x0127, 0x0027, 0x0133, 0x000d, 0x0134, 0x0135,
 0x0103, 0x011e, 0x0130, 0x012e, 0x0120, 0x0112, 0x0121, 0x0122,
 0x0123, 0x0117, 0x0124, 0x0125, 0x0126, 0x0132, 0x0131, 0x0118,
 0x0119, 0x0110, 0x0113, 0x011f, 0x0114, 0x0116, 0x012f, 0x0111,
 0x012d, 0x0115, 0x012c, 0x001a, 0x002b, 0x001b, 0x0107, 0x010c,
 0x0029, 0x001e, 0x0030, 0x002e, 0x0020, 0x0012, 0x0021, 0x0022,
 0x0023, 0x0017, 0x0024, 0x0025, 0x0026, 0x0032, 0x0031, 0x0018,
 0x0019, 0x0010, 0x0013, 0x001f, 0x0014, 0x0016, 0x002f, 0x0011,
 0x002d, 0x0015, 0x002c, 0x011a, 0x012b, 0x011b, 0x0129, 0xffff,
};

struct {
    u_short	base;
    u_short	shift;
    u_short	ctrl;
    u_short	alt;
} ScanCodes[] = {
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key  0 */
    {	0x011b, 0x011b, 0x011b, 0xffff }, /* key  1 - Escape key */
    {	0x0231, 0x0221, 0xffff, 0x7800 }, /* key  2 - '1' */
    {	0x0332, 0x0340, 0x0300, 0x7900 }, /* key  3 - '2' - special handling */
    {	0x0433, 0x0423, 0xffff, 0x7a00 }, /* key  4 - '3' */
    {	0x0534, 0x0524, 0xffff, 0x7b00 }, /* key  5 - '4' */
    {	0x0635, 0x0625, 0xffff, 0x7c00 }, /* key  6 - '5' */
    {	0x0736, 0x075e, 0x071e, 0x7d00 }, /* key  7 - '6' */
    {	0x0837, 0x0826, 0xffff, 0x7e00 }, /* key  8 - '7' */
    {	0x0938, 0x092a, 0xffff, 0x7f00 }, /* key  9 - '8' */
    {	0x0a39, 0x0a28, 0xffff, 0x8000 }, /* key 10 - '9' */
    {	0x0b30, 0x0b29, 0xffff, 0x8100 }, /* key 11 - '0' */
    {	0x0c2d, 0x0c5f, 0x0c1f, 0x8200 }, /* key 12 - '-' */
    {	0x0d3d, 0x0d2b, 0xffff, 0x8300 }, /* key 13 - '=' */
    {	0x0e08, 0x0e08, 0x0e7f, 0xffff }, /* key 14 - backspace */
    {	0x0f09, 0x0f00, 0xffff, 0xffff }, /* key 15 - tab */
    {	0x1071, 0x1051, 0x1011, 0x1000 }, /* key 16 - 'Q' */
    {	0x1177, 0x1157, 0x1117, 0x1100 }, /* key 17 - 'W' */
    {	0x1265, 0x1245, 0x1205, 0x1200 }, /* key 18 - 'E' */
    {	0x1372, 0x1352, 0x1312, 0x1300 }, /* key 19 - 'R' */
    {	0x1474, 0x1454, 0x1414, 0x1400 }, /* key 20 - 'T' */
    {	0x1579, 0x1559, 0x1519, 0x1500 }, /* key 21 - 'Y' */
    {	0x1675, 0x1655, 0x1615, 0x1600 }, /* key 22 - 'U' */
    {	0x1769, 0x1749, 0x1709, 0x1700 }, /* key 23 - 'I' */
    {	0x186f, 0x184f, 0x180f, 0x1800 }, /* key 24 - 'O' */
    {	0x1970, 0x1950, 0x1910, 0x1900 }, /* key 25 - 'P' */
    {	0x1a5b, 0x1a7b, 0x1a1b, 0xffff }, /* key 26 - '[' */
    {	0x1b5d, 0x1b7d, 0x1b1d, 0xffff }, /* key 27 - ']' */
    {	0x1c0d, 0x1c0d, 0x1c0a, 0xffff }, /* key 28 - CR */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 29 - control */
    {	0x1e61, 0x1e41, 0x1e01, 0x1e00 }, /* key 30 - 'A' */
    {	0x1f73, 0x1f53, 0x1f13, 0x1f00 }, /* key 31 - 'S' */
    {	0x2064, 0x2044, 0x2004, 0x2000 }, /* key 32 - 'D' */
    {	0x2166, 0x2146, 0x2106, 0x2100 }, /* key 33 - 'F' */
    {	0x2267, 0x2247, 0x2207, 0x2200 }, /* key 34 - 'G' */
    {	0x2368, 0x2348, 0x2308, 0x2300 }, /* key 35 - 'H' */
    {	0x246a, 0x244a, 0x240a, 0x2400 }, /* key 36 - 'J' */
    {	0x256b, 0x254b, 0x250b, 0x2500 }, /* key 37 - 'K' */
    {	0x266c, 0x264c, 0x260c, 0x2600 }, /* key 38 - 'L' */
    {	0x273b, 0x273a, 0xffff, 0xffff }, /* key 39 - ';' */
    {	0x2827, 0x2822, 0xffff, 0xffff }, /* key 40 - ''' */
    {	0x2960, 0x297e, 0xffff, 0xffff }, /* key 41 - '`' */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 42 - left shift */
    {	0x2b5c, 0x2b7c, 0x2b1c, 0xffff }, /* key 43 - '' */
    {	0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, /* key 44 - 'Z' */
    {	0x2d78, 0x2d58, 0x2d18, 0x2d00 }, /* key 45 - 'X' */
    {	0x2e63, 0x2e43, 0x2e03, 0x2e00 }, /* key 46 - 'C' */
    {	0x2f76, 0x2f56, 0x2f16, 0x2f00 }, /* key 47 - 'V' */
    {	0x3062, 0x3042, 0x3002, 0x3000 }, /* key 48 - 'B' */
    {	0x316e, 0x314e, 0x310e, 0x3100 }, /* key 49 - 'N' */
    {	0x326d, 0x324d, 0x320d, 0x3200 }, /* key 50 - 'M' */
    {	0x332c, 0x333c, 0xffff, 0xffff }, /* key 51 - ',' */
    {	0x342e, 0x343e, 0xffff, 0xffff }, /* key 52 - '.' */
    {	0x352f, 0x353f, 0xffff, 0xffff }, /* key 53 - '/' */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 54 - right shift - */
    {	0x372a, 0xffff, 0x3772, 0xffff }, /* key 55 - prt-scr - */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 56 - Alt - */
    {	0x3920, 0x3920, 0x3920, 0x3920 }, /* key 57 - space bar */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 58 - caps-lock -  */
    {	0x3b00, 0x5400, 0x5e00, 0x6800 }, /* key 59 - F1 */
    {	0x3c00, 0x5500, 0x5f00, 0x6900 }, /* key 60 - F2 */
    {	0x3d00, 0x5600, 0x6000, 0x6a00 }, /* key 61 - F3 */
    {	0x3e00, 0x5700, 0x6100, 0x6b00 }, /* key 62 - F4 */
    {	0x3f00, 0x5800, 0x6200, 0x6c00 }, /* key 63 - F5 */
    {	0x4000, 0x5900, 0x6300, 0x6d00 }, /* key 64 - F6 */
    {	0x4100, 0x5a00, 0x6400, 0x6e00 }, /* key 65 - F7 */
    {	0x4200, 0x5b00, 0x6500, 0x6f00 }, /* key 66 - F8 */
    {	0x4300, 0x5c00, 0x6600, 0x7000 }, /* key 67 - F9 */
    {	0x4400, 0x5d00, 0x6700, 0x7100 }, /* key 68 - F10 */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 69 - num-lock - */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 70 - scroll-lock -  */
    {	0x4700, 0x4737, 0x7700, 0xffff }, /* key 71 - home */
    {	0x4800, 0x4838, 0xffff, 0xffff }, /* key 72 - cursor up */
    {	0x4900, 0x4939, 0x8400, 0xffff }, /* key 73 - page up */
    {	0x4a2d, 0x4a2d, 0xffff, 0xffff }, /* key 74 - minus sign */
    {	0x4b00, 0x4b34, 0x7300, 0xffff }, /* key 75 - cursor left */
    {	0xffff, 0x4c35, 0xffff, 0xffff }, /* key 76 - center key */
    {	0x4d00, 0x4d36, 0x7400, 0xffff }, /* key 77 - cursor right */
    {	0x4e2b, 0x4e2b, 0xffff, 0xffff }, /* key 78 - plus sign */
    {	0x4f00, 0x4f31, 0x7500, 0xffff }, /* key 79 - end */
    {	0x5000, 0x5032, 0xffff, 0xffff }, /* key 80 - cursor down */
    {	0x5100, 0x5133, 0x7600, 0xffff }, /* key 81 - page down */
    {	0x5200, 0x5230, 0xffff, 0xffff }, /* key 82 - insert */
    {	0x5300, 0x532e, 0xffff, 0xffff }, /* key 83 - delete */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 84 - sys key */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 85 */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 86 */
    {	0x8500, 0x5787, 0x8900, 0x8b00 }, /* key 87 - F11 */
    {	0x8600, 0x5888, 0x8a00, 0x8c00 }, /* key 88 - F12 */
};

void
debug_event(int fd, int cond, void *arg, regcontext_t *REGS)
{
    static char ibuf[1024];
    static int icnt = 0;
    static u_short ds = 0;
    static u_short di = 0;
    static u_short cnt = 16 * 8;
    char *ep;
    int r;
    
    if (!(cond & AS_RD))
    	return;

    r = read(STDIN_FILENO, ibuf + icnt, sizeof(ibuf) - icnt);
    if (r <= 0)
	return;

    icnt += r;

    ibuf[icnt] = 0;
    while ((ep = strchr(ibuf, '\n')) != 0) {
    	int ac;
    	char *_av[16];
    	char **av;

    	*ep++ = 0;
    	ac = ParseBuffer(ibuf, av = _av, 16);

    	if (ac > 0) {
    	    if (!strcasecmp(av[0], "dump")) {
    	    	if (ac > 1) {
		    char *c;
		    if ((c = strchr(av[1], ':')) != 0) {
			ds = strtol(av[1], 0, 16);
			di = strtol(c+1, 0, 16);
		    } else
			di = strtol(av[1], 0, 16);
    	    	}
		if (ac > 2)
		    cnt = strtol(av[2], 0, 0);
	    	cnt = (cnt + 0xf) & ~0xf;
		if (cnt == 0)
		    cnt = 0x10;
	    	di &= ~0xf;

    	    	for (r = 0; r < cnt; r += 0x10, di = (di + 0x10) & 0xffff) {
		    int i;
		    u_char *ap = (u_char *)(((u_long)ds << 4) + di);

		    printf("%04x:%04x:", ds, di);
		    for (i = 0; i < 8; ++i)
			printf(" %02x", ap[i]);
    	    	    printf(" ");
		    for (i = 8; i < 16; ++i)
			printf(" %02x", ap[i]);
    	    	    printf(": ");
		    for (i = 0; i < 8; ++i)
			printf("%c",(ap[i] < ' ' || ap[i] > '~') ? '.' : ap[i]);
    	    	    printf(" ");
		    for (i = 8; i < 16; ++i)
			printf("%c",(ap[i] < ' ' || ap[i] > '~') ? '.' : ap[i]);
		    printf("\n");
    	    	}
    	    } else if (!strcasecmp(av[0], "dis")) {
		u_char *ap = (u_char *)(((u_long)ds << 4) + di);

    	    	if (ac > 1) {
		    char *c;
		    if ((c = strchr(av[1], ':')) != 0) {
			ds = strtol(av[1], 0, 16);
			di = strtol(c+1, 0, 16);
		    } else
			di = strtol(av[1], 0, 16);
    	    	}
		if (ac > 2)
		    cnt = strtol(av[2], 0, 0);

    	    	for (r = 0; r < cnt; ++r) {
    	    	    char buf[16];
		    int c = i386dis(ds, di, ap, buf, 0);
		    printf("%04x:%04x %s\n", ds, di, buf);
		    di += c;
		    ap += c;
    	    	}
    	    } else if (!strcasecmp(av[0], "regs")) {
		dump_regs(REGS);
    	    } else if (!strcasecmp(av[0], "force")) {
		char *p = av[1];

    	    	while ((p = *++av) != 0) {
		    while (*p) {
			if (*p >= ' ' && *p <= '~')
			    KbdWrite(ScanCodes[Ascii2Scan[(int)*p] & 0xff].base);
			++p;
    	    	    }
    	    	}
		KbdWrite(ScanCodes[28].base);
	    } else if (!strcasecmp(av[0], "bell")) {
#ifndef NO_X		
		XBell(dpy, 0);
                XFlush(dpy);
#endif
    	    } else {
		fprintf(stderr, "%s: unknown command\n", av[0]);
    	    }
    	}

    	if (ep < ibuf + icnt) {
    	    char *f = ep;
    	    char *t = ibuf;
    	    icnt -= ep - ibuf;
    	    while (icnt--)
	    	*t++ = *f++;
    	} else
	    icnt = 0;
	ibuf[icnt] = 0;
    }
}

unsigned char
inb_port60(int port __unused)
{       
    int r = break_code;
    break_code = 0;
    scan_code = 0xffff;
    return(r);
}       

void
kbd_event(int fd, int cond, void *arg, regcontext_t *REGS)
{
    if (!(cond & AS_RD))
       return;
    
    kbd_read = 0;

    printf("kbd_event: fd=%d\n", fd);
    if ((break_code = read_raw_kbd(fd, &scan_code)) != 0xffff)
	hardint(0x01);
}

void
int09(REGISTERS __unused)
{
    if (raw_kbd) {
	if (scan_code != 0xffff) {
	    KbdWrite(scan_code);
	    break_code = 0;
	    scan_code = 0xffff;
#if 0
	    kbd_event(kbd_fd, 0, sc, REGS);
#endif
	}
    }
    send_eoi();
}

u_short
read_raw_kbd(int fd, u_short *code)
{
    unsigned char c;

    *code = 0xffff;

    if (read(fd, &c, 1) == 1) {
    	if (c == 0xe0) {
	    K3_STATUS |= K3_TWOBYTE;
	    return(c);
    	}
    	switch (c) {
    	case 29:	/* Control */
	    K1_STATUS |= K1_CTRL;
	    if (K3_STATUS & K3_TWOBYTE)
		K3_STATUS |= K3_RCTRL;
	    else
		K2_STATUS |= K2_LCTRL;
	    break;
    	case 29 | 0x80:	/* Control */
	    K1_STATUS &= ~K1_CTRL;
	    if (K3_STATUS & K3_TWOBYTE)
		K3_STATUS &= ~K3_RCTRL;
	    else
		K2_STATUS &= ~K2_LCTRL;
	    break;

	case 42:	/* left shift */
	    K1_STATUS |= K1_LSHIFT;
	    break;
	case 42 | 0x80:	/* left shift */
	    K1_STATUS &= ~K1_LSHIFT;
	    break;

	case 54:	/* right shift */
	    K1_STATUS |= K1_RSHIFT;
	    break;
	case 54 | 0x80:	/* right shift */
	    K1_STATUS &= ~K1_RSHIFT;
	    break;

	case 56:	/* Alt */
	    K1_STATUS |= K1_ALT;
	    if (K3_STATUS & K3_TWOBYTE)
		K3_STATUS |= K3_RALT;
	    else
		K2_STATUS |= K2_LALT;
	    break;
	case 56 | 0x80:	/* Alt */
	    K1_STATUS &= ~K1_ALT;
	    if (K3_STATUS & K3_TWOBYTE)
		K3_STATUS &= ~K3_RALT;
	    else
		K2_STATUS &= ~K2_LALT;
	    break;

	case 58:	/* caps-lock */
	    K1_STATUS ^= K1_CLOCK;
    	    if (K1_STATUS & K1_CLOCK)
	    	K4_STATUS |= K4_CLOCK_LED;
	    else
	    	K4_STATUS &= ~K4_CLOCK_LED;
	    K2_STATUS |= K2_CLOCK;
    	    break;
	case 58 | 0x80:	/* caps-lock */
	    K2_STATUS &= ~K2_CLOCK;
    	    break;

	case 69:	/* num-lock */
	    K1_STATUS ^= K1_NLOCK;
    	    if (K1_STATUS & K1_NLOCK)
	    	K4_STATUS |= K4_NLOCK_LED;
	    else
	    	K4_STATUS &= ~K4_NLOCK_LED;
	    K2_STATUS |= K2_NLOCK;
    	    break;
	case 69 | 0x80:	/* num-lock */
	    K2_STATUS &= ~K2_NLOCK;
    	    break;

	case 70:	/* scroll-lock */
	    K1_STATUS ^= K1_SLOCK;
    	    if (K1_STATUS & K1_SLOCK)
	    	K4_STATUS |= K4_SLOCK_LED;
	    else
	    	K4_STATUS &= ~K4_SLOCK_LED;
	    K2_STATUS |= K2_SLOCK;
    	    break;
	case 70 | 0x80:	/* scroll-lock */
	    K2_STATUS &= ~K2_SLOCK;
    	    break;

	case 82:	/* insert */
	    K1_STATUS ^= K1_INSERT;
	    K2_STATUS |= K2_INSERT;
    	    break;
	case 82 | 0x80:	/* insert */
	    K2_STATUS &= ~K2_INSERT;
    	    break;

    	}

#if 0 /*XXXXX*/
	if ((K4_STATUS & 0x07) != oldled) {
	    oldled = K4_STATUS & 0x07;
	    ioctl (fd, PCCONIOCSETLED, &oldled);
	}
#endif

    	if (c == 83 && (K1_STATUS & (K1_ALT|K1_CTRL)) == (K1_ALT|K1_CTRL))
	    quit(0);

    	if (c < 89) {
	    u_short scode;

	    if (K1_STATUS & K1_ALT) {
		scode = ScanCodes[c].alt;
	    } else if (K1_STATUS & K1_CTRL) {
		scode = ScanCodes[c].ctrl;
	    } else if (K1_STATUS & K1_SHIFT) {
		scode = ScanCodes[c].shift;
	    } else {
		scode = ScanCodes[c].base;
		if (K1_STATUS & K1_CLOCK) {
		    if (islower(scode & 0xff)) {
		    	scode = (scode & 0xff00) | toupper(scode & 0xff);
		    }
		}
		if ((K1_STATUS & K1_NLOCK) && (K3_STATUS & K3_TWOBYTE) == 0) {
		    switch (c) {
		    case 71: /* home */
		    case 72: /* cursor up */
		    case 73: /* page up */
		    case 75: /* cursor left */
		    case 76: /* center key */
		    case 77: /* cursor right */
		    case 79: /* end */
		    case 80: /* cursor down */
		    case 81: /* page down */
		    case 82: /* insert */
		    case 83: /* delete */
			scode = ScanCodes[c].shift;
		    	break;
		    }
    	    	}
	    }
	    *code = scode;
    	}
	K3_STATUS &= ~K3_TWOBYTE;
	if ((K1_STATUS&(K1_ALT|K1_CTRL)) == (K1_ALT|K1_CTRL)) {
	    switch (c) {
	    case 0x13:	/* R */
                kill(getpid(), SIGALRM);	/* force redraw */
printf("FORCED REDRAW\n");
		return(0xffff);
	    case 0x14:	/* T */
		tmode ^= 1;
		if (!tmode)
		    resettrace((regcontext_t *)&saved_sigframe->
			sf_uc.uc_mcontext);
		return(0xffff);
	    case 0x53:	/* DEL */
		quit(0);
	    }
    	}
	return(c);
    } else {
	return(0xffff);
    }
}

void
video_async_event(int fd, int cond, void *arg, regcontext_t *REGS)
{
#ifndef NO_X
    	int int9 = 0;

	if (!(cond & AS_RD))
	    return;
	
	for (;;) {
                int x;
                fd_set fdset;
                XEvent ev;  
                static struct timeval tv;
 
                /*
                 * Handle any events just sitting around...
                 */
                XFlush(dpy);
                while (QLength(dpy) > 0) {
                        XNextEvent(dpy, &ev);
                        int9 |= video_event(&ev);
                }

                FD_ZERO(&fdset);
                FD_SET(fd, &fdset);

                x = select(FD_SETSIZE, &fdset, 0, 0, &tv);

                switch (x) {  
                case -1:
                        /*
                         * Errno might be wrong, so we just select again.
                         * This could cause a problem is something really
                         * was wrong with select....
                         */
                        perror("select");
                        return;
                case 0:
			XFlush(dpy);
			if (int9)
			    hardint(0x01);
                        return;
                default:
                        if (FD_ISSET(fd, &fdset)) {
                                do {
                                        XNextEvent(dpy, &ev);
                                        int9 |= video_event(&ev);
                                } while (QLength(dpy));
                        }
                        break;
                }
        }
#endif
}

#ifndef NO_X
static int
video_event(XEvent *ev)
{
	switch (ev->type) {
	case MotionNotify: {
		XMotionEvent *me = (XMotionEvent *)ev;
		me->x -= 2;
		me->y -= 2;

		mouse_status.x = (me->x < mouse_status.range.x)
				    ? mouse_status.range.x
				    : (me->x > mouse_status.range.w)
				    ? mouse_status.range.w : me->x;
		mouse_status.y = (me->y < mouse_status.range.y)
				    ? mouse_status.range.y
				    : (me->y > mouse_status.range.h)
				    ? mouse_status.range.h : me->y;
		break;
	    }
	case ButtonRelease: {
		XButtonEvent *be = (XButtonEvent *)ev;
		be->x -= 2;
		be->y -= 2;

		if (be->button < 3)
		    mouse_status.ups[be->button]++;

		mouse_status.x = (be->x < mouse_status.range.x)
				    ? mouse_status.range.x
				    : (be->x > mouse_status.range.w)
				    ? mouse_status.range.w : be->x;
		mouse_status.y = (be->y < mouse_status.range.y)
				    ? mouse_status.range.y
				    : (be->y > mouse_status.range.h)
				    ? mouse_status.range.h : be->y;
		break;
	    }
	case ButtonPress: {
		XButtonEvent *be = (XButtonEvent *)ev;
		be->x -= 2;
		be->y -= 2;

		if (be->button < 3)
		    mouse_status.downs[be->button]++;

		mouse_status.x = (be->x < mouse_status.range.x)
				    ? mouse_status.range.x
				    : (be->x > mouse_status.range.w)
				    ? mouse_status.range.w : be->x;
		mouse_status.y = (be->y < mouse_status.range.y)
				    ? mouse_status.range.y
				    : (be->y > mouse_status.range.h)
				    ? mouse_status.range.h : be->y;

		if ((K1_STATUS & (K1_ALT|K1_CTRL)) == (K1_ALT|K1_CTRL)) {
		    quit(0);
		}
		break;
	    }
        case NoExpose:
                break;
        case GraphicsExpose:
        case Expose: {
		int r;
		for (r = 0; r < height; ++r)
		    lines[r].changed = 1;
		break;
	    }
	case KeyRelease: {
		static char buf[128];
		KeySym ks;

		break_code |= 0x80;

    	    	if (!(ev->xkey.state & ShiftMask)) {
		    K1_STATUS &= ~K1_LSHIFT;
		    K1_STATUS &= ~K1_RSHIFT;
		}
    	    	if (!(ev->xkey.state & ControlMask)) {
			K1_STATUS &= ~K1_CTRL;
			K2_STATUS &= ~K2_LCTRL;
			K3_STATUS &= ~K3_RCTRL;
		}
    	    	if (!(ev->xkey.state & Mod1Mask)) {
                        K1_STATUS &= ~K1_ALT;
                        K2_STATUS &= ~K2_LALT;
                        K3_STATUS &= ~K3_RALT;
		}
    	    	if (!(ev->xkey.state & LockMask)) {
                        K2_STATUS &= ~K2_CLOCK;
		}

		XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &ks, 0);
		switch (ks) {
		case XK_Shift_L:
			K1_STATUS &= ~K1_LSHIFT;
			break;
		case XK_Shift_R:
			K1_STATUS &= ~K1_RSHIFT;
			break;
		case XK_Control_L:
			K1_STATUS &= ~K1_CTRL;
			K2_STATUS &= ~K2_LCTRL;
			break;
		case XK_Control_R:
			K1_STATUS &= ~K1_CTRL;
			K3_STATUS &= ~K3_RCTRL;
			break;
		case XK_Alt_L:
			K1_STATUS &= ~K1_ALT;
			K2_STATUS &= ~K2_LALT;
			break;
		case XK_Alt_R:
			K1_STATUS &= ~K1_ALT;
			K3_STATUS &= ~K3_RALT;
			break;
		case XK_Scroll_Lock:
			K2_STATUS &= ~K2_SLOCK;
			break;
		case XK_Num_Lock:
			K2_STATUS &= ~K2_NLOCK;
			break;
		case XK_Caps_Lock:
			K2_STATUS &= ~K2_CLOCK;
			break;
		case XK_Insert:
			K2_STATUS &= ~K2_INSERT;
			break;
		}
		return(1);
	    }
	case KeyPress: {
		static char buf[128];
		KeySym ks;
		int n;
    	    	int nlock = 0;
		u_short scan = 0xffff;

    	    	if (!(ev->xkey.state & ShiftMask)) {
		    K1_STATUS &= ~K1_LSHIFT;
		    K1_STATUS &= ~K1_RSHIFT;
		}
    	    	if (!(ev->xkey.state & ControlMask)) {
			K1_STATUS &= ~K1_CTRL;
			K2_STATUS &= ~K2_LCTRL;
			K3_STATUS &= ~K3_RCTRL;
		}
    	    	if (!(ev->xkey.state & Mod1Mask)) {
                        K1_STATUS &= ~K1_ALT;
                        K2_STATUS &= ~K2_LALT;
                        K3_STATUS &= ~K3_RALT;
		}
    	    	if (!(ev->xkey.state & LockMask)) {
                        K2_STATUS &= ~K2_CLOCK;
		}

		n = XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &ks, 0);

		switch (ks) {
		case XK_Shift_L:
			K1_STATUS |= K1_LSHIFT;
			break;
		case XK_Shift_R:
			K1_STATUS |= K1_RSHIFT;
			break;
		case XK_Control_L:
			K1_STATUS |= K1_CTRL;
			K2_STATUS |= K2_LCTRL;
			break;
		case XK_Control_R:
			K1_STATUS |= K1_CTRL;
			K3_STATUS |= K3_RCTRL;
			break;
		case XK_Alt_L:
			K1_STATUS |= K1_ALT;
			K2_STATUS |= K2_LALT;
			break;
		case XK_Alt_R:
			K1_STATUS |= K1_ALT;
			K3_STATUS |= K3_RALT;
			break;
		case XK_Scroll_Lock:
			K1_STATUS ^= K1_SLOCK;
			K2_STATUS |= K2_SLOCK;
			break;
		case XK_Num_Lock:
			K1_STATUS ^= K1_NLOCK;
			K2_STATUS |= K2_NLOCK;
			break;
		case XK_Caps_Lock:
			K1_STATUS ^= K1_CLOCK;
			K2_STATUS |= K2_CLOCK;
			break;
		case XK_Insert:
		case XK_KP_Insert:
			K1_STATUS ^= K1_INSERT;
			K2_STATUS |= K2_INSERT;
			scan = 82;
			goto docode;

		case XK_Escape:
			scan = 1;
			goto docode;

		case XK_Tab:
		case XK_ISO_Left_Tab:
			scan = 15;
			goto docode;
			
    	    	case XK_Return:
		case XK_KP_Enter:
			scan = 28;
		    	goto docode;

    	    	case XK_Print:
			scan = 55;
			goto docode;

		case XK_F1:
		case XK_F2:
		case XK_F3:
		case XK_F4:
		case XK_F5:
		case XK_F6:
		case XK_F7:
		case XK_F8:
		case XK_F9:
		case XK_F10:
			scan = ks - XK_F1 + 59;
			goto docode;

    	    	case XK_KP_7:
			nlock = 1;
		case XK_Home:
		case XK_KP_Home:
			scan = 71;
			goto docode;
    	    	case XK_KP_8:
			nlock = 1;
		case XK_Up:
		case XK_KP_Up:
			scan = 72;
			goto docode;
    	    	case XK_KP_9:
			nlock = 1;
		case XK_Prior:
		case XK_KP_Prior:
			scan = 73;
			goto docode;
    	    	case XK_KP_Subtract:
			scan = 74;
			goto docode;
    	    	case XK_KP_4:
			nlock = 1;
		case XK_Left:
		case XK_KP_Left:
			scan = 75;
			goto docode;
    	    	case XK_KP_5:
			nlock = 1;
		case XK_Begin:
		case XK_KP_Begin:
			scan = 76;
			goto docode;
    	    	case XK_KP_6:
			nlock = 1;
		case XK_Right:
		case XK_KP_Right:
			scan = 77;
			goto docode;
    	    	case XK_KP_Add:
			scan = 78;
			goto docode;
    	    	case XK_KP_1:
			nlock = 1;
		case XK_End:
		case XK_KP_End:
			scan = 79;
			goto docode;
    	    	case XK_KP_2:
			nlock = 1;
		case XK_Down:
		case XK_KP_Down:
			scan = 80;
			goto docode;
    	    	case XK_KP_3:
			nlock = 1;
		case XK_Next:
		case XK_KP_Next:
			scan = 81;
			goto docode;
    	    	case XK_KP_0:
			nlock = 1;
    	    	/* case XK_Insert: This is above */
			scan = 82;
			goto docode;

    	    	case XK_KP_Decimal:
			nlock = 1;
			scan = 83;
			goto docode;

    	    	case XK_Delete:
    	    	case XK_KP_Delete:
			scan = flipdelete ? 14 : 83;
			goto docode;

		case XK_BackSpace:
			scan = flipdelete ? 83 : 14;
			goto docode;

    	    	case XK_F11:
			scan = 87;
			goto docode;
    	    	case XK_F12:
			scan = 88;
			goto docode;


		case XK_KP_Divide:
			scan = Ascii2Scan['/'];
			goto docode;

		case XK_KP_Multiply:
			scan = Ascii2Scan['*'];
			goto docode;

		default:
    	    	    	if ((K1_STATUS&(K1_ALT|K1_CTRL)) == (K1_ALT|K1_CTRL)) {
				if (ks == 'T' || ks == 't') {
				    tmode ^= 1;
				    if (!tmode)
					    resettrace((regcontext_t *)&saved_sigframe->
						sf_uc.uc_mcontext); 
				    break;
				}
				if (ks == 'R' || ks == 'r') {
                                    kill(getpid(), SIGALRM);	/* redraw */
				    break;
				}
			}
			if (ks < ' ' || ks > '~')
				break;
			scan = Ascii2Scan[ks]; 
    	    	docode:
			if (nlock)
			    scan |= 0x100;

    	    	    	if ((scan & ~0x100) > 88) {
			    scan = 0xffff;
			    break;
    	    	    	}

    	    	    	if ((K1_STATUS & K1_SHIFT) || (scan & 0x100)) {
			    scan = ScanCodes[scan & 0xff].shift;
			} else if (K1_STATUS & K1_CTRL) {
			    scan = ScanCodes[scan & 0xff].ctrl;
			} else if (K1_STATUS & K1_ALT) {
			    scan = ScanCodes[scan & 0xff].alt;
			}  else
			    scan = ScanCodes[scan & 0xff].base;

			break;
		}
		if (scan != 0xffff) {
			break_code = scan >> 8;
			KbdWrite(scan);
		}
		return(1);
	    }
	default:
		break;
	}
    	return(0);
}
#endif

void
tty_move(int r, int c)
{
	row = r;
	col = c;
	SetVREGCur();
}

void
tty_report(int *r, int *c)
{
	*r = row;
	*c = col;
}

void
tty_flush()
{
	K_NEXT = K_FREE = K_BUFSTARTP;
}

void
tty_index(int scroll)
{
	int i;

	if (row > (height - 1))
		row = 0;
	else if (++row >= height) {
		row = height - 1;
		if (scroll) {
			memcpy(vmem, &vmem[width], 2 * width * (height - 1));
			for (i = 0; i < width; ++i)
				vmem[(height - 1) * width + i] = vattr | ' ';
		}
	}
	SetVREGCur();
}

void
tty_write(int c, int attr)
{
    	if (attr == TTYF_REDIRECT) {
		if (redirect1) {
		    write(1, &c, 1);
		    return;
		}
		attr = -1;
	}
    	if (capture_fd >= 0) {
	    char cc = c;
	    write(capture_fd, &cc, 1);
    	}
	c &= 0xff;
	switch (c) {
	case 0x07:
		if (xmode) {
#ifndef NO_X
			XBell(dpy, 0);
#endif
		} else
			write(1, "\007", 1);
		break;
	case 0x08:
		if (row > (height - 1) || col > width)
			break;
		if (col > 0)
			--col;
		vmem[row * width + col] &= 0xff00;
		break;
	case '\t':
		if (row > (height - 1))
			row = 0;
		col = (col + 8) & ~0x07;
		if (col > width) {
			col = 0;
			tty_index(1);
		}
		break;
	case '\r':
		col = 0;
		break;
	case '\n':
		tty_index(1);
		break;
	default:
		if (col >= width) {
			col = 0;
			tty_index(1);
		}
		if (row > (height - 1))
			row = 0;
		if (attr >= 0)
			vmem[row * width + col] = attr & 0xff00;
		else
			vmem[row * width + col] &= 0xff00;
		vmem[row * width + col++] |= c;
		break;
	}
	SetVREGCur();
}

void
tty_rwrite(int n, int c, int attr)
{
    u_char srow, scol;
    c &= 0xff;

#ifndef NO_X
    if (VGA_ATC[ATC_ModeCtrl] & 1) {
	tty_rwrite_graphics(n, c, attr);
	return;
    }
#endif
    
    srow = row;
    scol = col;
    while (n--) {
	if (col >= width) {
	    col = 0;
	    tty_index(0);
	}
	if (row > (height - 1))
	    row = 0;
	if (attr >= 0)
	    vmem[row * width + col] = attr & 0xff00;
	else
	    vmem[row * width + col] &= 0xff00;
	vmem[row * width + col++] |= c;
    }
    row = srow;
    col = scol;
    SetVREGCur();
}

#ifndef NO_X
/* Write a character in graphics mode. Note that the text is put at *text*
   coordinates. */
static void
tty_rwrite_graphics(int n, int c, int attr)
{
    u_int8_t srow, scol;
    int ht = height / CharHeight;
    int wd = width / 8;

    srow = row;
    scol = col;

    while (n--) {
	if (col >= wd) {
	    col = 0;
	    /* tty_index(0); *//* scroll up if last line is filled */
	}
	if (row > (ht - 1))
	    row = 0;
	putchar_graphics(row * wd * CharHeight + col, c, attr);
	col++;
    }
    row = srow;
    col = scol;
    SetVREGCur();

    return;
}

/* Put the character together from its pixel representation in 'font8xXX[]'
   and write it to 'vram'. The attribute byte gives the desired color; if bit
   7 is set, the pixels are XOR'd with the underlying color(s).

   XXX This must be updated for the 256 color modes. */
static void
putchar_graphics(int xy, int c, int attr)
{
    int i, j;
    u_int8_t cline;
    u_int8_t *cpos;
    
    /* get char position in the pixel representation */
    cpos = (u_int8_t *)(0xC3000 + c * CharHeight);

    for (i = 0; i < CharHeight; i++) {
	cline = cpos[i];
	for (j = 0; j < 4; j++) {
	    if (attr & 0x8000) {
		/* XOR */
		if (attr & (0x0100 << j))
		    vram[xy + i * width / 8 + j * 0x10000] ^= cline;
	    } else {
		/* replace */
		if (attr & (0x0100 << j))
		    vram[xy + i * width / 8 + j * 0x10000] &= ~cline;
		else
		    vram[xy + i * width / 8 + j * 0x10000] |= cline;
	    }
	}
    }

    return;
}
#endif

void tty_pause()
{
    sigset_t set;

    sigprocmask(0, 0, &set);
    sigdelset(&set, SIGIO);
    sigdelset(&set, SIGALRM);
    sigsuspend(&set);
}

static int nextchar = 0;

int
tty_read(REGISTERS, int flag)
{
    int r;

    if ((r = nextchar) != 0) {
	nextchar = 0;
	return(r & 0xff);
    }

    if ((flag & TTYF_REDIRECT) && redirect0) {
	char c;
    	if (read(STDIN_FILENO, &c, 1) != 1)
	    return(-1);
	if (c == '\n')
	    c = '\r';
	return(c);
    }

    if (KbdEmpty()) {
	if (flag & TTYF_BLOCK) {
	    while (KbdEmpty())
		tty_pause();
	} else {
	    return(-1);
	}
    }

    r = KbdRead();
    if ((r & 0xff) == 0)
	nextchar = r >> 8;
    r &= 0xff;
    if (flag & TTYF_CTRL) {
	if (r == 3) {
	    /*
	     * XXX - Not quite sure where we should return, maybe not
	     *       all the way to the user, but...
	     */
	    if (ivec[0x23] && (ivec[0x23] >> 16) != 0xF000) {
    	    	fake_int(REGS, 0x23);
		R_EIP = R_EIP - 2;
		return(-2);
	    }
    	}
    }
    if (flag & TTYF_ECHO) {
	if ((flag & TTYF_ECHONL) && (r == '\n' || r == '\r')) { 
	    tty_write('\r', -1);
	    tty_write('\n', -1);
    	} else
	    tty_write(r, -1);
    }
    return(r & 0xff);
}

int
tty_peek(REGISTERS, int flag)
{
	int c;

    	if (c == nextchar)
	    return(nextchar & 0xff);

	if (KbdEmpty()) {
		if (flag & TTYF_POLL) {
			sleep_poll();
			if (KbdEmpty())
				return(0);
		} else if (flag & TTYF_BLOCK) {
			while (KbdEmpty())
				tty_pause();
		} else
			return(0);
	}
	c = KbdPeek();
    	if ((c & 0xff) == 3) {
	    /*
	     * XXX - Not quite sure where we should return, maybe not
	     *       all the way to the user, but...
	     */
	    if (ivec[0x23] && (ivec[0x23] >> 16) != 0xF000) {
    	    	fake_int(REGS, 0x23);
		R_EIP = R_EIP - 2;
		return(-2);
	    }
    	}
	return(0xff);
}

int
tty_state()
{
	return(K1_STATUS);
}

int
tty_estate()
{
    int state = 0;
    if (K2_STATUS & K2_SYSREQ)
    	state |= 0x80;
    if (K2_STATUS & K2_CLOCK)
    	state |= 0x40;
    if (K2_STATUS & K2_NLOCK)
    	state |= 0x20;
    if (K2_STATUS & K2_SLOCK)
    	state |= 0x10;
    if (K3_STATUS & K3_RALT)
    	state |= 0x08;
    if (K3_STATUS & K3_RCTRL)
    	state |= 0x04;
    if (K2_STATUS & K2_LALT)
    	state |= 0x02;
    if (K2_STATUS & K2_LCTRL)
    	state |= 0x01;
    return(state);
}

static int
inrange(int a, int n, int x)
{
	return(a < n ? n : a > x ? x : a);
}

void
tty_scroll(int sr, int sc, int er, int ec, int n, int attr)
{
	int i, j;

	sr = inrange(sr, 0, height);
	er = inrange(er, 0, height);
	sc = inrange(sc, 0, width);
	ec = inrange(ec, 0, width);
	if (sr > er || sc > ec)
		return;
	++er;
	++ec;

	attr &= 0xff00;
	attr |= ' ';

	if (n > 0 && n < er - sr) {
		for (j = sr; j < er - n; ) {
			memcpy(&vmem[j * width + sc],
			       &vmem[(j + n) * width + sc],
			       sizeof(vmem[0]) * (ec - sc));
			++j;
		}
	} else
		n = er - sr;
	for (j = er - n; j < er; ) {
		for (i = sc; i < ec; ++i)
			vmem[j * width + i] = attr;
		++j;
	}
}

void
tty_rscroll(int sr, int sc, int er, int ec, int n, int attr)
{
	int i, j;

	sr = inrange(sr, 0, height);
	er = inrange(er, 0, height);
	sc = inrange(sc, 0, width);
	ec = inrange(ec, 0, width);
	if (sr > er || sc > ec)
		return;
	++er;
	++ec;

	attr &= 0xff00;
	attr |= ' ';

	if (n > 0 && n < er - sr) {
		for (j = er; j > sr + n; ) {
			--j;
			memcpy(&vmem[j * width + sc],
			       &vmem[(j - n) * width + sc],
			       sizeof(vmem[0]) * (ec - sc));
		}
	} else
		n = er - sr;
	for (j = sr + n; j > sr; ) {
		--j;
		for (i = sc; i < ec; ++i)
			vmem[j * width + i] = attr;
	}
}

int
tty_char(int r, int c)
{
	if (r == -1)
		r = row;
	if (c == -1)
		c = col;
	r = inrange(r, 0, height);
	c = inrange(c, 0, width);
	return(vmem[r * width + c]);
}

int
KbdEmpty()
{
	return(K_NEXT == K_FREE);
}

void
KbdWrite(u_short code)
{
	int kf;

	kf = K_FREE + 2;
	if (kf == K_BUFENDP)
		kf = K_BUFSTARTP;

	if (kf == K_NEXT) {
#ifndef NO_X
		XBell(dpy, 0);
#endif
		return;
	}
	K_BUF(K_FREE) = code;
	K_FREE = kf;
}

u_short
KbdRead()
{
	int kf = K_NEXT;

	K_NEXT = K_NEXT + 2;
	if (K_NEXT == K_BUFENDP)
		K_NEXT = K_BUFSTARTP;

	return(K_BUF(kf));
}

u_short
KbdPeek()
{
	return(K_BUF(K_NEXT));
}

void
kbd_init()
{
	u_long vec;
	
	define_input_port_handler(0x60, inb_port60);

	K_BUFSTARTP = 0x1e;	/* Start of keyboard buffer */
	K_BUFENDP = 0x3e;	/* End of keyboard buffer */
	K_NEXT = K_FREE = K_BUFSTARTP;
	
	vec = insert_hardint_trampoline();
	ivec[0x09] = vec;
	register_callback(vec, int09, "int 09");

	return;
}

void
kbd_bios_init()
{
	BIOSDATA[0x96] = 0x10;	/* MF II kbd, 101 keys */
	K1_STATUS = 0;
	K2_STATUS = 0;
	K3_STATUS = 0;
	K4_STATUS = 0;
}

#ifndef NO_X
/* Calculate 16 bit RGB values for X from the 6 bit DAC values and the
   palette. This works for 16 and 256 color modes, although we don't really
   support the latter yet. */
static void
dac2rgb(XColor *color, int i)
{
    int n, m;

    /* 256 colors are easy; just take the RGB values from the DAC and
       shift left. For the pedants: multiplication with 65535./63. and
       rounding makes a difference of less than two percent. */
    if (VGA_ATC[ATC_ModeCtrl] & 0x40) {
	color->red   = dac_rgb[i].red << 10;
	color->green = dac_rgb[i].green << 10;
	color->blue  = dac_rgb[i].blue << 10;

	return;
    }

    /* For the 16 color modes, check bit 7 of the Mode Control register in
       the ATC. If set, we take bits 0-3 of the Color Select register and
       bits 0-3 of the palette register 'i' to build the index into the
       DAC table; otherwise, bits 2 and 3 of the CS reg and bits 0-5 of
       the palette register are used. Note that the entries in 'palette[]'
       are supposed to be already masked to 6 bits. */
    if (VGA_ATC[ATC_ModeCtrl] & 0x80) {
	n = VGA_ATC[ATC_ColorSelect] & 0x0f;
	m = palette[i] & 0x0f;
    } else {
	n = VGA_ATC[ATC_ColorSelect] & 0x0c;
	m = palette[i];
    }
    color->red   = dac_rgb[16*n + m].red << 10;
    color->green = dac_rgb[16*n + m].green << 10;
    color->blue  = dac_rgb[16*n + m].blue << 10;
}
#endif

/* Get a connection to the X server and create the window. */
void
init_window()
{
#ifndef NO_X
    XGCValues gcv;
    int i;

    {
	/*
	 * Arg...  I can no longer change X's fd out from under it.
	 * Open up all the available fd's, leave 3 behind for X
	 * to play with, open X and then release all the other fds
	 */
	int nfds = sysconf(_SC_OPEN_MAX);
	int *fds = malloc(sizeof(int) * nfds);
	i = 0;
	if (fds)
	    for (i = 0; i < nfds && (i == 0 || fds[i-1] < 63); ++i)
		if ((fds[i] = open(_PATH_DEVNULL, 0)) < 0)
		    break;
	/*
	 * Leave 3 fds behind for X to play with
	 */
	if (i > 0) close(fds[--i]);
	if (i > 0) close(fds[--i]);
	if (i > 0) close(fds[--i]);
		
	dpy = XOpenDisplay(NULL);
		
	while (i > 0)
	    close(fds[--i]);
    }
    if (dpy == NULL)
	err(1, "Could not open display ``%s''\n", XDisplayName(NULL));
    xfd = ConnectionNumber(dpy);

    _RegisterIO(xfd, video_async_event, 0, Failure);
    if (debug_flags & D_DEBUGIN)
	_RegisterIO(0, debug_event, 0, Failure);

    /* Create window, but defer setting a size and GC. */
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
			      1, 1, 2, black, black);

    gcv.foreground = white;
    gcv.background = black;
    gc = XCreateGC(dpy, win, GCForeground | GCBackground, &gcv);

    gcv.foreground = 1;
    gcv.background = 0;
    gcv.function = GXxor;
    cgc = XCreateGC(dpy, win, GCForeground|GCBackground|GCFunction, &gcv);

    if (raw_kbd) {
	XSelectInput(dpy, win, ExposureMask | ButtonPressMask
		     | ButtonReleaseMask | PointerMotionMask );
    } else {
	XSelectInput(dpy, win, KeyReleaseMask | KeyPressMask |
		     ExposureMask | ButtonPressMask
		     | ButtonReleaseMask | PointerMotionMask );
    }

    XStoreName(dpy, win, "DOS");

    /* Get the default visual and depth for later use. */
    depth = DefaultDepth(dpy, DefaultScreen(dpy));
    visual = DefaultVisual(dpy, DefaultScreen(dpy));

    prepare_lut();

#if 0
    /* While we are developing the graphics code ... */
    call_on_quit(write_vram, NULL);
#endif
#endif
}

void
load_font()
{
#ifndef NO_X
    XGCValues gcv;
    
    if (!xfont)
	xfont = FONTVGA;

    font = XLoadQueryFont(dpy, xfont);

    if (font == NULL)
	font = XLoadQueryFont(dpy, FONTVGA);

    if (font == NULL)
	err(1, "Could not open font ``%s''\n", xfont);

    gcv.font = font->fid;
    XChangeGC(dpy, gc, GCFont, &gcv);
    
    FW = font->max_bounds.width;
    FH = font->max_bounds.ascent + font->max_bounds.descent;
    FD = font->max_bounds.descent;

    /* Put the pixel representation at c000:3000. */
    switch (CharHeight) {
    case 8:
	memcpy((void *)0xc3000, font8x8, sizeof(font8x8));
	break;
    case 14:
	memcpy((void *)0xc3000, font8x14, sizeof(font8x14));
	break;
    case 16:
	memcpy((void *)0xc3000, font8x16, sizeof(font8x16));
	break;
    default:
	err(1, "load_font: CharHeight = %d?", CharHeight);
    }
    
    return;
#endif
}

/* Get a new, or resize an old XImage as canvas for the graphics display. */
void
get_ximage()
{
#ifndef NO_X
    if (xi != NULL)
	XFree(xi);
    
    xi = XCreateImage(dpy, visual, depth, ZPixmap, 0, NULL,
                      width, height, 32, 0);
    if (xi == NULL)
        err(1, "Could not get ximage");

    xi->data = malloc(width * height * depth / 8);
    if (xi->data == NULL) {
        XDestroyImage(xi);
        err(1, "Could not get memory for ximage data");
    }

    return;
#endif
}

/* Get memory for the text line buffer. */
void
get_lines()
{
    int i;
    
    if (lines == NULL) {
	lines = (TextLine *)malloc(sizeof(TextLine) * height);
	if (lines == NULL)
	    err(1, "Could not allocate data structure for text lines\n");

	for (i = 0; i < height; ++i) {
	    lines[i].max_length = width;
	    lines[i].data = (u_short *)malloc(width * sizeof(u_short));
	    if (lines[i].data == NULL)
		err(1, "Could not allocate data structure for text lines\n");
	    lines[i].changed = 1;
	}
    } else {
	lines = (TextLine *)realloc(lines, sizeof(TextLine) * height);
	if (lines == NULL)
	    err(1, "Could not allocate data structure for text lines\n");

	for (i = 0; i < height; ++i) {
	    lines[i].max_length = width;
	    lines[i].data = (u_short *)realloc(lines[i].data,
					       width * sizeof(u_short));
	    if (lines[i].data == NULL)
		err(1, "Could not allocate data structure for text lines\n");
	    lines[i].changed = 1;
	}
    }
}

#ifndef NO_X
/* Prepare the LUT for the VRAM -> XImage conversion. */
static void
prepare_lut()
{
    int i, j, k;

    for (i = 0; i < 4; i++) {
	for (j = 0; j < 256; j++) {
	    for (k = 0; k < 8; k++) {
		lut[i][j][7 - k] = ((j & (1 << k)) ? (1 << i) : 0);
	    }
	}
    }

    return;
}
#endif

/* Resize the window, using information from 'vga_status[]'. This function is
   called after a mode change. */
void
resize_window()
{
#ifndef NO_X
    XSizeHints *sh;
    vmode_t vmode;
    
    sh = XAllocSizeHints();
    if (sh == NULL)
	err(1, "Could not get XSizeHints structure");
#endif
    
    width = DpyCols;
    height = DpyRows + 1;

#ifndef NO_X
    vmode = vmodelist[find_vmode(VideoMode)];
    if (vmode.type == TEXT) {
	sh->base_width = FW * width + 4;
	sh->base_height = FH * height + 4;
	sh->base_width += 4;
	sh->base_height += 4;
    } else {
	width *= 8;
	height *= CharHeight;
	sh->base_width = width;
	sh->base_height = height;
    }
    
    sh->min_width = sh->max_width = sh->base_width;
    sh->min_height = sh->max_height = sh->base_height;
    sh->flags = USSize | PMinSize | PMaxSize | PSize;

    debug(D_VIDEO, "VGA: Set window size %dx%d\n",
	  sh->base_width, sh->base_height);
    
    XSetWMNormalHints(dpy, win, sh);
    XResizeWindow(dpy, win, sh->base_width, sh->base_height);
    XMapWindow(dpy, win);
    XFlush(dpy);
    
    XFree(sh);
    
    return;
#endif
}

/* Calculate 'pixels[]' from the current DAC table and palette.

   To do: do not use 'pixels[]', use an array of 'XColor's which we can
   allocate and free on demand. Install a private colormap if necessary. */
void
update_pixels()
{
#ifndef NO_X
    int i;

    /* We support only 16 colors for now. */
    for (i = 0; i < 16; i++) {
	XColor color;

	dac2rgb(&color, i);
	if (XAllocColor(dpy,
			DefaultColormap(dpy, DefaultScreen(dpy)), &color)) {
	    pixels[i] = color.pixel;
	} else if (i < 7)
	    pixels[i] = BlackPixel(dpy, DefaultScreen(dpy));
	else
	    pixels[i] = WhitePixel(dpy, DefaultScreen(dpy));
    }
#endif
}

void
write_vram(void *arg __unused)
{
    int fd;

    if ((fd = open("vram", O_WRONLY | O_CREAT, 0644)) == -1)
	err(1, "Can't open vram file");
    (void)write(fd, (void *)vram, 256 * 1024);

    return;
}
