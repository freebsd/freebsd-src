/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)xneko.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*--------------------------------------------------------------
 *
 *	xneko  -  X11 G-
 *
 *			Original Writer: Masayuki Koba
 *			Programmed by Masayuki Koba, 1990
 *
 *--------------------------------------------------------------
 *
 *!!Introduction:
 *
 *!!!!K\@(#)xneko.c	8.1m%0%i%`$O Macintosh $N5/31/939%/%"%/%;%5%j!< "neko" $N
 *!!F0:n$r X11 $G%^%M$?$b$N$G$9!#
 *
 *!!!!Macintosh "neko" $N=(0o$J5/31/936%$%s$K7I0U$rI=$7$D$D!"$3$N
 *!!@(#)xneko.c	8.1m%0%i%`$r3'$5$s$KJ{$2$^$9!#
 *
 *--------------------------------------------------------------
 *
 *!!Special Thanks to
 *
 *	toshi-w	!D!!Macintosh neko $N>R2p<T
 *	shio-m	!D!!!VX11 $N neko $,M_$7$$!*!W$H%?%@$r$3$M$??M
 *	disco	!D!!X11 SCCS/s.xneko.c/%K%+%k!&%"8.1P%$%6!<
 *
 *	HOMY	!D!!/usr/src/games/xneko/SCCS/s.xneko.c0;XE&<T
 *	BNS	!D!!J#?t@(#)xneko.c	8.1l!<%sBP1~May 31, 1993C%ADs6!<T
 *
 *		"xneko"  Presented by Masayuki Koba (masa-k).
 *
 *--------------------------------------------------------------
 *
 *!!Manifest:
 *
 *!!!!K\@(#)xneko.c	8.1m%0%i%`$O Public Domain Software $G$9!#E>:\!&2~NI$O
 *!!<+M3$K9T$C$F2<$5$$!#
 *
 *!!!!$J$*!"86:n<T$O!"K\@(#)xneko.c	8.1m%0%i%`$r;HMQ$9$k$3$H$K$h$C$F@8$8$?
 *!!>c32$dITMx1W$K$D$$$F$$$C$5$$@UG$$r;}$A$^$;$s!#
 *
 *--------------------------------------------------------------
 *
 *!!Bugs:
 *
 *!!!!!J#1!KX11 $N .Xdefaults $N@_Dj$r$^$k$C$-$jL5;k$7$F$$$^$9!#
 *
 *!!!!!J#2!KG-$NF0:n$,;~4V$HF14|$7$F$$$k$?$a!"%^%&%9$N0\F0>pJs
 *!!!!!!!!$r%]!<%j%s%0$7$F$$$^$9!#=>$C$F!"%^%&%9$,A4$/F0:n$7$F
 *!!!!!!!!$$$J$$;~$OL5BL$J%^%&%9:BI8FI$_<h$j$r9T$C$F$7$^$$$^$9!#
 *
 *!!!!!J#3!K%&%#%s8.1&$,%"%$%3%s2=$5$l$F$b!"$7$i$s$W$j$GIA2h$7
 *!!!!!!!!$D$E$1$^$9!#$3$NItJ,$O!"8=:_$N%&%#%s8.1&$N>uBV$r@(#) xneko.c 8.1@(#)'
 *!!!!!!!!70/$7$F!"%"%$%3%s2=$5$l$F$$$k;~$O40A4$K%$s%HBT$A
 *!!!!!!!!$K$J$k$h$&$K=q$-JQ$($J$1$l$P$J$j$^$;$s!# ($=$s$J$3$H!"
 *!!!!!!!!$G$-$k$N$+$J$!!#X10 $G$O$G$-$^$7$?$,!#)
 *
 *!!!!!J#4!K%j%5%$%:8e$N%&%#%s8.1&$,6KC<$K>.$5$/$J$C$?;~$NF0:n
 *!!!!!!!!$OJ]>Z$G$-$^$;$s!#
 *
 *!!!!!J#5!KK\Mh$J$i$P3NJ]$7$?%&%#%s8.1&$d Pixmap $O@(#)xneko.c	8.1m%0%i%`
 *!!!!!!!!=*N;;~$K2rJ|$9$kI,MW$,$"$j$^$9$,!"K\@(#)xneko.c	8.1m%0%i%`$O$=$N
 *!!!!!!!!$X$s$r%5%\$C$F$*$j!"Hs>o$K$*9T57$,0-$/$J$C$F$$$^$9!#
 *!!!!!!!!IaDL$O exit() ;~$K%7%9SCCS/s.xneko.c`$,M>J,$J%j%=!<%9$r2rJ|$7$F
 *!!!!!!!!$/$l$^$9$,!"#O#S$K/usr/src/games/xneko/SCCS/s.xneko.c0$,$"$k>l9g$O xneko $r2?EY$b5/
 *!!!!!!!!F0$9$k$H!"$=$N$&$A%9%o82WNN0h$,ITB-$7$F$7$^$&$3$H$K
 *!!!!!!!!$J$k$+$b$7$l$^$;$s!#
 *
 *!!!!!J#6!K;~4V$KF14|$7$FI,$:IA2h=hM}$r<B9T$9$k$?$a!"0BDj>uBV
 *!!!!!!!!$G$b Idle 90 !A 95% $H$J$j!"%7%9SCCS/s.xneko.c`#C#P#U$r 5 !A 10%
 *!!!!!!!!Dx>CHq$7$^$9!#!Jxtachos $GD4$Y$^$7$?!#!K
 *
 *--------------------------------------------------------------
 *
 *!!System (Machine):
 *
 *!!!!K\@(#)xneko.c	8.1m%0%i%`$NF0:n$r3NG'$7$?%7%9SCCS/s.xneko.c`9=@.$O0J2<$NDL$j!#
 *
 *	!&NWS-1750!"NWS-1720 (NEWS)!"NWP-512D
 *	!!NEWS-OS 3.2a (UNIX 4.3BSD)!"X11 Release 2
 *
 *	!&NWS-1750!"NWS-1720 (NEWS)!"NWP-512D
 *	!!NEWS-OS 3.3 (UNIX 4.3BSD)!"X11 Release 3
 *
 *	!&Sun 3!"X11 Release 4
 *
 *	!&LUNA!"X11 Release 3
 *
 *	!&DECstation 3100!"ULTRIX!"X11
 *
 *--------------------------------------------------------------*/


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <math.h>
#include <sys/time.h>


#ifndef	lint
static char
	rcsid[] = "$Header: /home/ncvs/src/games/x11/xneko/xneko.c,v 1.3 1995/09/23 09:44:11 asami Exp $";
static char	WriterMessage[] = "xneko: Programmed by Masayuki Koba, 1990";
#endif


/*
 *	X11 G- 0C5/31/93^129W18:45:36!%$%k0lMw!'
 *
 *		"icon.xbm"		!D!!%"%$%3%s
 *		"cursor.xbm"		!D!!%+!<%=%k
 *		"cursor_mask.xbm"	!D!!%+!<%=%k!J%^%9%/!K
 *
 *		"space.xbm"		!D!!%9%Z!<%9
 *
 *		"mati2.xbm"		!D!!BT$A#2
 *		"jare2.xbm"		!D!!$8$c$l#2
 *		"kaki1.xbm"		!D!!A_$-#1
 *		"kaki2.xbm"		!D!!A_$-#2
 *		"mati3.xbm"		!D!!BT$A#3!J$"$/$S!K
 *		"sleep1.xbm"		!D!!?2$k#1
 *		"sleep2.xbm"		!D!!?2$k#2
 *
 *		"awake.xbm"		!D!!L\3P$a
 *
 *		"up1.xbm"		!D!!>e#1
 *		"up2.xbm"		!D!!>e#2
 *		"down1.xbm"		!D!!2<#1
 *		"down2.xbm"		!D!!2<#2
 *		"left1.xbm"		!D!!:8#1
 *		"left2.xbm"		!D!!:8#2
 *		"right1.xbm"		!D!!1&#1
 *		"right2.xbm"		!D!!1&#2
 *		"upleft1.xbm"		!D!!:8>e#1
 *		"upleft2.xbm"		!D!!:8>e#2
 *		"upright1.xbm"		!D!!1&>e#1
 *		"upright2.xbm"		!D!!1&>e#2
 *		"dwleft1.xbm"		!D!!:82<#1
 *		"dwleft2.xbm"		!D!!:82<#2
 *		"dwright1.xbm"		!D!!1&2<#1
 *		"dwright2.xbm"		!D!!1&2<#2
 *
 *		"utogi1.xbm"		!D!!>eKa$.#1
 *		"utogi2.xbm"		!D!!>eKa$.#2
 *		"dtogi1.xbm"		!D!!2<Ka$.#1
 *		"dtogi2.xbm"		!D!!2<Ka$.#2
 *		"ltogi1.xbm"		!D!!:8Ka$.#1
 *		"ltogi2.xbm"		!D!!:8Ka$.#2
 *		"rtogi1.xbm"		!D!!1&Ka$.#1
 *		"rtogi2.xbm"		!D!!1&Ka$.#2
 *
 *	!!$3$l$i$N18:45:36!%$%k$O bitmap %3%^%s%I$GJT=82DG=$G$9!#
 *
 *		(bitmap size "* 32x32 ... Macintosh ICON resource size.)
 *
 */


#include "bitmaps/icon.xbm"
#include "bitmaps/cursor.xbm"
#include "bitmaps/cursor_mask.xbm"

#include "bitmaps/space.xbm"

#include "bitmaps/mati2.xbm"
#include "bitmaps/jare2.xbm"
#include "bitmaps/kaki1.xbm"
#include "bitmaps/kaki2.xbm"
#include "bitmaps/mati3.xbm"
#include "bitmaps/sleep1.xbm"
#include "bitmaps/sleep2.xbm"

#include "bitmaps/awake.xbm"

#include "bitmaps/up1.xbm"
#include "bitmaps/up2.xbm"
#include "bitmaps/down1.xbm"
#include "bitmaps/down2.xbm"
#include "bitmaps/left1.xbm"
#include "bitmaps/left2.xbm"
#include "bitmaps/right1.xbm"
#include "bitmaps/right2.xbm"
#include "bitmaps/upright1.xbm"
#include "bitmaps/upright2.xbm"
#include "bitmaps/upleft1.xbm"
#include "bitmaps/upleft2.xbm"
#include "bitmaps/dwleft1.xbm"
#include "bitmaps/dwleft2.xbm"
#include "bitmaps/dwright1.xbm"
#include "bitmaps/dwright2.xbm"

#include "bitmaps/utogi1.xbm"
#include "bitmaps/utogi2.xbm"
#include "bitmaps/dtogi1.xbm"
#include "bitmaps/dtogi2.xbm"
#include "bitmaps/ltogi1.xbm"
#include "bitmaps/ltogi2.xbm"
#include "bitmaps/rtogi1.xbm"
#include "bitmaps/rtogi2.xbm"


/*
 *	Dj?tDj5A
 */

#define	BITMAP_WIDTH		32	/* #1%-%c%i%/%?$NI} (18:45:53/%;%k) */
#define	BITMAP_HEIGHT		32	/* #1%-%c%i%/%?$N9b$5 (18:45:53/%;%k) */

#define	WINDOW_WIDTH		320	/* %&%#%s8.1&$NI} (18:45:53/%;%k) */
#define	WINDOW_HEIGHT		256	/* %&%#%s8.1&$N9b$5 (18:45:53/%;%k) */

#define	DEFAULT_BORDER		2	/* %\!<%@!<%5%$%: */

#define	DEFAULT_WIN_X		1	/* %&%#%s8.1&@8@.#X:BI8 */
#define	DEFAULT_WIN_Y		1	/* %&%#%s8.1&@8@.#Y:BI8 */

#define	AVAIL_KEYBUF		255

#define	EVENT_MASK1		( KeyPressMask | StructureNotifyMask )

#define	EVENT_MASK2		( KeyPressMask | \
				  ExposureMask | \
				  StructureNotifyMask )

#define	MAX_TICK		9999		/* Odd Only! */

#define	INTERVAL		125000L		/* %$%s%?!</usr/src/games/xneko/SCCS/s.xneko.ck%?%$%` */

#define	NEKO_SPEED		16

#define	IDLE_SPACE		6

#define	NORMAL_STATE		1
#define	DEBUG_LIST		2
#define	DEBUG_MOVE		3

/* G-$N>uBVDj?t */

#define	NEKO_STOP		0	/* N)$A;_$^$C$? */
#define	NEKO_JARE		1	/* 4i$r@v$C$F$$$k */
#define	NEKO_KAKI		2	/* F,$rA_$$$F$$$k */
#define	NEKO_AKUBI		3	/* $"$/$S$r$7$F$$$k */
#define	NEKO_SLEEP		4	/* ?2$F$7$^$C$? */
#define	NEKO_AWAKE		5	/* L\$,3P$a$? */
#define	NEKO_U_MOVE		6	/* >e$K0\F0Cf */
#define	NEKO_D_MOVE		7	/* 2<$K0\F0Cf */
#define	NEKO_L_MOVE		8	/* :8$K0\F0Cf */
#define	NEKO_R_MOVE		9	/* 1&$K0\F0Cf */
#define	NEKO_UL_MOVE		10	/* :8>e$K0\F0Cf */
#define	NEKO_UR_MOVE		11	/* 1&>e$K0\F0Cf */
#define	NEKO_DL_MOVE		12	/* :82<$K0\F0Cf */
#define	NEKO_DR_MOVE		13	/* 1&2<$K0\F0Cf */
#define	NEKO_U_TOGI		14	/* >e$NJI$r0z$CA_$$$F$$$k */
#define	NEKO_D_TOGI		15	/* 2<$NJI$r0z$CA_$$$F$$$k */
#define	NEKO_L_TOGI		16	/* :8$NJI$r0z$CA_$$$F$$$k */
#define	NEKO_R_TOGI		17	/* 1&$NJI$r0z$CA_$$$F$$$k */

/* G-$N%"%K%a!<%7%g%s7+$jJV$72s?t */

#define	NEKO_STOP_TIME		4
#define	NEKO_JARE_TIME		10
#define	NEKO_KAKI_TIME		4
#define	NEKO_AKUBI_TIME		3
#define	NEKO_AWAKE_TIME		3
#define	NEKO_TOGI_TIME		10

#define	PI_PER8			((double)3.1415926535/(double)8)

#define	DIRNAMELEN		255


/*
 *	%0%m!</usr/src/games/xneko/SCCS/s.xneko.ckJQ?t
 */

static char		*ProgramName;		/* %3%^%s%IL>>N */

Display			*theDisplay;
int			theScreen;
unsigned int		theDepth;
unsigned long		theBlackPixel;
unsigned long		theWhitePixel;
Window			theWindow;
Cursor			theCursor;

static unsigned int	WindowWidth;
static unsigned int	WindowHeight;

static int		WindowPointX;
static int		WindowPointY;

static unsigned int	BorderWidth = DEFAULT_BORDER;

long			IntervalTime = INTERVAL;

int			EventState;		/* %$s%H=hM}MQ >uBVJQ?t */

int			NekoTickCount;		/* G-F0:n%+%&%s%? */
int			NekoStateCount;		/* G-F10l>uBV%+%&%s%? */
int			NekoState;		/* G-$N>uBV */

int			MouseX;			/* %^%&%9#X:BI8 */
int			MouseY;			/* %^%&%9#Y:BI8 */

int			PrevMouseX = 0;		/* D>A0$N%^%&%9#X:BI8 */
int			PrevMouseY = 0;		/* D>A0$N%^%&%9#Y:BI8 */

int			NekoX;			/* G-#X:BI8 */
int			NekoY;			/* G-#Y:BI8 */

int			NekoMoveDx;		/* G-0\F05wN%#X */
int			NekoMoveDy;		/* G-0\F05wN%#Y */

int			NekoLastX;		/* G-:G=*IA2h#X:BI8 */
int			NekoLastY;		/* G-:G=*IA2h#Y:BI8 */
GC			NekoLastGC;		/* G-:G=*IA2h GC */

double			NekoSpeed = (double)NEKO_SPEED;

double			SinPiPer8Times3;	/* sin( #3&P!?#8 ) */
double			SinPiPer8;		/* sin( &P!?#8 ) */

Pixmap			SpaceXbm;

Pixmap			Mati2Xbm;
Pixmap			Jare2Xbm;
Pixmap			Kaki1Xbm;
Pixmap			Kaki2Xbm;
Pixmap			Mati3Xbm;
Pixmap			Sleep1Xbm;
Pixmap			Sleep2Xbm;

Pixmap			AwakeXbm;

Pixmap			Up1Xbm;
Pixmap			Up2Xbm;
Pixmap			Down1Xbm;
Pixmap			Down2Xbm;
Pixmap			Left1Xbm;
Pixmap			Left2Xbm;
Pixmap			Right1Xbm;
Pixmap			Right2Xbm;
Pixmap			UpLeft1Xbm;
Pixmap			UpLeft2Xbm;
Pixmap			UpRight1Xbm;
Pixmap			UpRight2Xbm;
Pixmap			DownLeft1Xbm;
Pixmap			DownLeft2Xbm;
Pixmap			DownRight1Xbm;
Pixmap			DownRight2Xbm;

Pixmap			UpTogi1Xbm;
Pixmap			UpTogi2Xbm;
Pixmap			DownTogi1Xbm;
Pixmap			DownTogi2Xbm;
Pixmap			LeftTogi1Xbm;
Pixmap			LeftTogi2Xbm;
Pixmap			RightTogi1Xbm;
Pixmap			RightTogi2Xbm;

GC			SpaceGC;

GC			Mati2GC;
GC			Jare2GC;
GC			Kaki1GC;
GC			Kaki2GC;
GC			Mati3GC;
GC			Sleep1GC;
GC			Sleep2GC;

GC			AwakeGC;

GC			Up1GC;
GC			Up2GC;
GC			Down1GC;
GC			Down2GC;
GC			Left1GC;
GC			Left2GC;
GC			Right1GC;
GC			Right2GC;
GC			UpLeft1GC;
GC			UpLeft2GC;
GC			UpRight1GC;
GC			UpRight2GC;
GC			DownLeft1GC;
GC			DownLeft2GC;
GC			DownRight1GC;
GC			DownRight2GC;

GC			UpTogi1GC;
GC			UpTogi2GC;
GC			DownTogi1GC;
GC			DownTogi2GC;
GC			LeftTogi1GC;
GC			LeftTogi2GC;
GC			RightTogi1GC;
GC			RightTogi2GC;

typedef struct {
    GC			*GCCreatePtr;
    Pixmap		*BitmapCreatePtr;
    char		*PixelPattern;
    unsigned int	PixelWidth;
    unsigned int	PixelHeight;
} BitmapGCData;

BitmapGCData	BitmapGCDataTable[] =
{
    { &SpaceGC, &SpaceXbm, space_bits, space_width, space_height },
    { &Mati2GC, &Mati2Xbm, mati2_bits, mati2_width, mati2_height },
    { &Jare2GC, &Jare2Xbm, jare2_bits, jare2_width, jare2_height },
    { &Kaki1GC, &Kaki1Xbm, kaki1_bits, kaki1_width, kaki1_height },
    { &Kaki2GC, &Kaki2Xbm, kaki2_bits, kaki2_width, kaki2_height },
    { &Mati3GC, &Mati3Xbm, mati3_bits, mati3_width, mati3_height },
    { &Sleep1GC, &Sleep1Xbm, sleep1_bits, sleep1_width, sleep1_height },
    { &Sleep2GC, &Sleep2Xbm, sleep2_bits, sleep2_width, sleep2_height },
    { &AwakeGC, &AwakeXbm, awake_bits, awake_width, awake_height },
    { &Up1GC, &Up1Xbm, up1_bits, up1_width, up1_height },
    { &Up2GC, &Up2Xbm, up2_bits, up2_width, up2_height },
    { &Down1GC, &Down1Xbm, down1_bits, down1_width, down1_height },
    { &Down2GC, &Down2Xbm, down2_bits, down2_width, down2_height },
    { &Left1GC, &Left1Xbm, left1_bits, left1_width, left1_height },
    { &Left2GC, &Left2Xbm, left2_bits, left2_width, left2_height },
    { &Right1GC, &Right1Xbm, right1_bits, right1_width, right1_height },
    { &Right2GC, &Right2Xbm, right2_bits, right2_width, right2_height },
    { &UpLeft1GC, &UpLeft1Xbm, upleft1_bits, upleft1_width, upleft1_height },
    { &UpLeft2GC, &UpLeft2Xbm, upleft2_bits, upleft2_width, upleft2_height },
    { &UpRight1GC,
      &UpRight1Xbm, upright1_bits, upright1_width, upright1_height },
    { &UpRight2GC,
      &UpRight2Xbm, upright2_bits, upright2_width, upright2_height },
    { &DownLeft1GC,
      &DownLeft1Xbm, dwleft1_bits, dwleft1_width, dwleft1_height },
    { &DownLeft2GC,
      &DownLeft2Xbm, dwleft2_bits, dwleft2_width, dwleft2_height },
    { &DownRight1GC,
      &DownRight1Xbm, dwright1_bits, dwright1_width, dwright1_height },
    { &DownRight2GC,
      &DownRight2Xbm, dwright2_bits, dwright2_width, dwright2_height },
    { &UpTogi1GC, &UpTogi1Xbm, utogi1_bits, utogi1_width, utogi1_height },
    { &UpTogi2GC, &UpTogi2Xbm, utogi2_bits, utogi2_width, utogi2_height },
    { &DownTogi1GC, &DownTogi1Xbm, dtogi1_bits, dtogi1_width, dtogi1_height },
    { &DownTogi2GC, &DownTogi2Xbm, dtogi2_bits, dtogi2_width, dtogi2_height },
    { &LeftTogi1GC, &LeftTogi1Xbm, ltogi1_bits, ltogi1_width, ltogi1_height },
    { &LeftTogi2GC, &LeftTogi2Xbm, ltogi2_bits, ltogi2_width, ltogi2_height },
    { &RightTogi1GC,
      &RightTogi1Xbm, rtogi1_bits, rtogi1_width, rtogi1_height },
    { &RightTogi2GC,
      &RightTogi2Xbm, rtogi2_bits, rtogi2_width, rtogi2_height },
    { NULL, NULL, NULL, NULL, NULL }
};

typedef struct {
    GC		*TickEvenGCPtr;
    GC		*TickOddGCPtr;
} Animation;

Animation	AnimationPattern[] =
{
    { &Mati2GC, &Mati2GC },		/* NekoState == NEKO_STOP */
    { &Jare2GC, &Mati2GC },		/* NekoState == NEKO_JARE */
    { &Kaki1GC, &Kaki2GC },		/* NekoState == NEKO_KAKI */
    { &Mati3GC, &Mati3GC },		/* NekoState == NEKO_AKUBI */
    { &Sleep1GC, &Sleep2GC },		/* NekoState == NEKO_SLEEP */
    { &AwakeGC, &AwakeGC },		/* NekoState == NEKO_AWAKE */
    { &Up1GC, &Up2GC }	,		/* NekoState == NEKO_U_MOVE */
    { &Down1GC, &Down2GC },		/* NekoState == NEKO_D_MOVE */
    { &Left1GC, &Left2GC },		/* NekoState == NEKO_L_MOVE */
    { &Right1GC, &Right2GC },		/* NekoState == NEKO_R_MOVE */
    { &UpLeft1GC, &UpLeft2GC },		/* NekoState == NEKO_UL_MOVE */
    { &UpRight1GC, &UpRight2GC },	/* NekoState == NEKO_UR_MOVE */
    { &DownLeft1GC, &DownLeft2GC },	/* NekoState == NEKO_DL_MOVE */
    { &DownRight1GC, &DownRight2GC },	/* NekoState == NEKO_DR_MOVE */
    { &UpTogi1GC, &UpTogi2GC },		/* NekoState == NEKO_U_TOGI */
    { &DownTogi1GC, &DownTogi2GC },	/* NekoState == NEKO_D_TOGI */
    { &LeftTogi1GC, &LeftTogi2GC },	/* NekoState == NEKO_L_TOGI */
    { &RightTogi1GC, &RightTogi2GC },	/* NekoState == NEKO_R_TOGI */
};


/*--------------------------------------------------------------
 *
 *	0C5/31/93^504W%G!<%?!&GC =i4|2=
 *
 *--------------------------------------------------------------*/

void
InitBitmapAndGCs()
{
    BitmapGCData	*BitmapGCDataTablePtr;
    XGCValues		theGCValues;

    theGCValues.function = GXcopy;
    theGCValues.foreground = BlackPixel( theDisplay, theScreen );
    theGCValues.background = WhitePixel( theDisplay, theScreen );
    theGCValues.fill_style = FillTiled;

    for ( BitmapGCDataTablePtr = BitmapGCDataTable;
	  BitmapGCDataTablePtr->GCCreatePtr != NULL;
	  BitmapGCDataTablePtr++ ) {

	*(BitmapGCDataTablePtr->BitmapCreatePtr)
	    = XCreatePixmapFromBitmapData(
		  theDisplay,
		  RootWindow( theDisplay, theScreen ),
		  BitmapGCDataTablePtr->PixelPattern,
		  BitmapGCDataTablePtr->PixelWidth,
		  BitmapGCDataTablePtr->PixelHeight,
		  BlackPixel( theDisplay, theScreen ),
		  WhitePixel( theDisplay, theScreen ),
		  DefaultDepth( theDisplay, theScreen ) );

	theGCValues.tile = *(BitmapGCDataTablePtr->BitmapCreatePtr);

	*(BitmapGCDataTablePtr->GCCreatePtr)
	    = XCreateGC( theDisplay, theWindow,
			 GCFunction | GCForeground | GCBackground |
			 GCTile | GCFillStyle,
			 &theGCValues );
    }

    XFlush( theDisplay );
}


/*--------------------------------------------------------------
 *
 *	%9%/%j!<%s4D6-=i4|2=
 *
 *--------------------------------------------------------------*/

void
InitScreen( DisplayName, theGeometry, TitleName, iconicState )
    char	*DisplayName;
    char	*theGeometry;
    char	*TitleName;
    Bool	iconicState;
{
    int				GeometryStatus;
    XSetWindowAttributes	theWindowAttributes;
    XSizeHints			theSizeHints;
    unsigned long		theWindowMask;
    Pixmap			theIconPixmap;
    Pixmap			theCursorSource;
    Pixmap			theCursorMask;
    XWMHints			theWMHints;
    Window			theRoot;
    Colormap			theColormap;
    XColor			theWhiteColor, theBlackColor, theExactColor;

    if ( ( theDisplay = XOpenDisplay( DisplayName ) ) == NULL ) {
	fprintf( stderr, "%s: Can't open display", ProgramName );
	if ( DisplayName != NULL ) {
	    fprintf( stderr, " %s.\n", DisplayName );
	} else {
	    fprintf( stderr, ".\n" );
	}
	exit( 1 );
    }

    theScreen = DefaultScreen( theDisplay );
    theDepth = DefaultDepth( theDisplay, theScreen );

    theBlackPixel = BlackPixel( theDisplay, theScreen );
    theWhitePixel = WhitePixel( theDisplay, theScreen );

    GeometryStatus = XParseGeometry( theGeometry,
				     &WindowPointX, &WindowPointY,
				     &WindowWidth, &WindowHeight );

    if ( !( GeometryStatus & XValue ) ) {
	WindowPointX = DEFAULT_WIN_X;
    }
    if ( !( GeometryStatus & YValue ) ) {
	WindowPointY = DEFAULT_WIN_Y;
    }
    if ( !( GeometryStatus & WidthValue ) ) {
	WindowWidth = WINDOW_WIDTH;
    }
    if ( !( GeometryStatus & HeightValue ) ) {
	WindowHeight = WINDOW_HEIGHT;
    }

    theCursorSource
	= XCreateBitmapFromData( theDisplay,
				 RootWindow( theDisplay, theScreen ),
				 cursor_bits,
				 cursor_width,
				 cursor_height );

    theCursorMask
	= XCreateBitmapFromData( theDisplay,
				 RootWindow( theDisplay, theScreen ),
				 cursor_mask_bits,
				 cursor_mask_width,
				 cursor_mask_height );

    theColormap = DefaultColormap( theDisplay, theScreen );

    if ( !XAllocNamedColor( theDisplay, theColormap,
			    "white", &theWhiteColor, &theExactColor ) ) {
	fprintf( stderr,
		 "%s: Can't XAllocNamedColor( \"white\" ).\n", ProgramName );
	exit( 1 );
    }

    if ( !XAllocNamedColor( theDisplay, theColormap,
			    "black", &theBlackColor, &theExactColor ) ) {
	fprintf( stderr,
		 "%s: Can't XAllocNamedColor( \"black\" ).\n", ProgramName );
	exit( 1 );
    }

    theCursor = XCreatePixmapCursor( theDisplay,
				     theCursorSource, theCursorMask,
				     &theBlackColor, &theWhiteColor,
				     cursor_x_hot, cursor_y_hot );

    theWindowAttributes.border_pixel = theBlackPixel;
    theWindowAttributes.background_pixel = theWhitePixel;
    theWindowAttributes.cursor = theCursor;
    theWindowAttributes.override_redirect = False;

    theWindowMask = CWBackPixel		|
		    CWBorderPixel	|
		    CWCursor		|
		    CWOverrideRedirect;

    theWindow = XCreateWindow( theDisplay,
			       RootWindow( theDisplay, theScreen ),
			       WindowPointX, WindowPointY,
			       WindowWidth, WindowHeight,
			       BorderWidth,
			       theDepth,
			       InputOutput,
			       CopyFromParent,
			       theWindowMask,
			       &theWindowAttributes );

    theIconPixmap = XCreateBitmapFromData( theDisplay, theWindow,
					   icon_bits,
					   icon_width,
					   icon_height );

    theWMHints.icon_pixmap = theIconPixmap;
    if ( iconicState ) {
	theWMHints.initial_state = IconicState;
    } else {
	theWMHints.initial_state = NormalState;
    }
    theWMHints.flags = IconPixmapHint | StateHint;

    XSetWMHints( theDisplay, theWindow, &theWMHints );

    theSizeHints.flags = PPosition | PSize;
    theSizeHints.x = WindowPointX;
    theSizeHints.y = WindowPointY;
    theSizeHints.width = WindowWidth;
    theSizeHints.height = WindowHeight;

    XSetNormalHints( theDisplay, theWindow, &theSizeHints );

    if ( strlen( TitleName ) >= 1 ) {
	XStoreName( theDisplay, theWindow, TitleName );
	XSetIconName( theDisplay, theWindow, TitleName );
    } else {
	XStoreName( theDisplay, theWindow, ProgramName );
	XSetIconName( theDisplay, theWindow, ProgramName );
    }

    XMapWindow( theDisplay, theWindow );

    XFlush( theDisplay );

    XGetGeometry( theDisplay, theWindow,
		  &theRoot,
		  &WindowPointX, &WindowPointY,
		  &WindowWidth, &WindowHeight,
		  &BorderWidth, &theDepth );

    InitBitmapAndGCs();

    XSelectInput( theDisplay, theWindow, EVENT_MASK1 );

    XFlush( theDisplay );
}


/*--------------------------------------------------------------
 *
 *	%$%s%?!</usr/src/games/xneko/SCCS/s.xneko.ck
 *
 *	!!$3$N4X?t$r8F$V$H!"$"$k0lDj$N;~4VJV$C$F$3$J$/$J$k!#G-
 *	$NF0:n%?%$%_%s%0D4@0$KMxMQ$9$k$3$H!#
 *
 *--------------------------------------------------------------*/

void
Interval()
{
    pause();
}


/*--------------------------------------------------------------
 *
 *	SCCS/s.xneko.c#728/%+%&%s%H=hM}
 *
 *--------------------------------------------------------------*/

void
TickCount()
{
    if ( ++NekoTickCount >= MAX_TICK ) {
	NekoTickCount = 0;
    }

    if ( NekoTickCount % 2 == 0 ) {
	if ( NekoStateCount < MAX_TICK ) {
	    NekoStateCount++;
	}
    }
}


/*--------------------------------------------------------------
 *
 *	G->uBV@_Dj
 *
 *--------------------------------------------------------------*/

void
SetNekoState( SetValue )
    int		SetValue;
{
    NekoTickCount = 0;
    NekoStateCount = 0;

    NekoState = SetValue;

#ifdef	DEBUG
    switch ( NekoState ) {
    case NEKO_STOP:
    case NEKO_JARE:
    case NEKO_KAKI:
    case NEKO_AKUBI:
    case NEKO_SLEEP:
    case NEKO_U_TOGI:
    case NEKO_D_TOGI:
    case NEKO_L_TOGI:
    case NEKO_R_TOGI:
	NekoMoveDx = NekoMoveDy = 0;
	break;
    default:
	break;
    }
#endif
}


/*--------------------------------------------------------------
 *
 *	G-IA2h=hM}
 *
 *--------------------------------------------------------------*/

void
DrawNeko( x, y, DrawGC )
    int		x;
    int		y;
    GC		DrawGC;
{
    if ( ( x != NekoLastX || y != NekoLastY )
	 && ( EventState != DEBUG_LIST ) ) {
	XFillRectangle( theDisplay, theWindow, SpaceGC,
			NekoLastX, NekoLastY,
			BITMAP_WIDTH, BITMAP_HEIGHT );
    }

    XSetTSOrigin( theDisplay, DrawGC, x, y );

    XFillRectangle( theDisplay, theWindow, DrawGC,
		    x, y, BITMAP_WIDTH, BITMAP_HEIGHT );

    XFlush( theDisplay );

    NekoLastX = x;
    NekoLastY = y;

    NekoLastGC = DrawGC;
}


/*--------------------------------------------------------------
 *
 *	G-:FIA2h=hM}
 *
 *--------------------------------------------------------------*/

void
RedrawNeko()
{
    XFillRectangle( theDisplay, theWindow, NekoLastGC,
		    NekoLastX, NekoLastY,
		    BITMAP_WIDTH, BITMAP_HEIGHT );

    XFlush( theDisplay );
}


/*--------------------------------------------------------------
 *
 *	G-0\F0J}K!7hDj
 *
 *--------------------------------------------------------------*/

void
NekoDirection()
{
    int			NewState;
    double		LargeX, LargeY;
    double		Length;
    double		SinTheta;

    if ( NekoMoveDx == 0 && NekoMoveDy == 0 ) {
	NewState = NEKO_STOP;
    } else {
	LargeX = (double)NekoMoveDx;
	LargeY = (double)(-NekoMoveDy);
	Length = sqrt( LargeX * LargeX + LargeY * LargeY );
	SinTheta = LargeY / Length;

	if ( NekoMoveDx > 0 ) {
	    if ( SinTheta > SinPiPer8Times3 ) {
		NewState = NEKO_U_MOVE;
	    } else if ( ( SinTheta <= SinPiPer8Times3 )
			&& ( SinTheta > SinPiPer8 ) ) {
		NewState = NEKO_UR_MOVE;
	    } else if ( ( SinTheta <= SinPiPer8 )
			&& ( SinTheta > -( SinPiPer8 ) ) ) {
		NewState = NEKO_R_MOVE;
	    } else if ( ( SinTheta <= -( SinPiPer8 ) )
			&& ( SinTheta > -( SinPiPer8Times3 ) ) ) {
		NewState = NEKO_DR_MOVE;
	    } else {
		NewState = NEKO_D_MOVE;
	    }
	} else {
	    if ( SinTheta > SinPiPer8Times3 ) {
		NewState = NEKO_U_MOVE;
	    } else if ( ( SinTheta <= SinPiPer8Times3 )
			&& ( SinTheta > SinPiPer8 ) ) {
		NewState = NEKO_UL_MOVE;
	    } else if ( ( SinTheta <= SinPiPer8 )
			&& ( SinTheta > -( SinPiPer8 ) ) ) {
		NewState = NEKO_L_MOVE;
	    } else if ( ( SinTheta <= -( SinPiPer8 ) )
			&& ( SinTheta > -( SinPiPer8Times3 ) ) ) {
		NewState = NEKO_DL_MOVE;
	    } else {
		NewState = NEKO_D_MOVE;
	    }
	}
    }

    if ( NekoState != NewState ) {
	SetNekoState( NewState );
    }
}


/*--------------------------------------------------------------
 *
 *	G-JI$V$D$+$jH=Dj
 *
 *--------------------------------------------------------------*/

Bool
IsWindowOver()
{
    Bool	ReturnValue = False;

    if ( NekoY <= 0 ) {
	NekoY = 0;
	ReturnValue = True;
    } else if ( NekoY >= WindowHeight - BITMAP_HEIGHT ) {
	NekoY = WindowHeight - BITMAP_HEIGHT;
	ReturnValue = True;
    }
    if ( NekoX <= 0 ) {
	NekoX = 0;
	ReturnValue = True;
    } else if ( NekoX >= WindowWidth - BITMAP_WIDTH ) {
	NekoX = WindowWidth - BITMAP_WIDTH;
	ReturnValue = True;
    }

    return( ReturnValue );
}


/*--------------------------------------------------------------
 *
 *	G-0\F0>u67H=Dj
 *
 *--------------------------------------------------------------*/

Bool
IsNekoDontMove()
{
    if ( NekoX == NekoLastX && NekoY == NekoLastY ) {
	return( True );
    } else {
	return( False );
    }
}


/*--------------------------------------------------------------
 *
 *	G-0\F03+;OH=Dj
 *
 *--------------------------------------------------------------*/

Bool
IsNekoMoveStart()
{
#ifndef	DEBUG
    if ( ( PrevMouseX >= MouseX - IDLE_SPACE
	 && PrevMouseX <= MouseX + IDLE_SPACE ) &&
	 ( PrevMouseY >= MouseY - IDLE_SPACE
	 && PrevMouseY <= MouseY + IDLE_SPACE ) ) {
	return( False );
    } else {
	return( True );
    }
#else
    if ( NekoMoveDx == 0 && NekoMoveDy == 0 ) {
	return( False );
    } else {
	return( True );
    }
#endif
}


/*--------------------------------------------------------------
 *
 *	G-0\F0 dx, dy 7W;;
 *
 *--------------------------------------------------------------*/

void
CalcDxDy()
{
    Window		QueryRoot, QueryChild;
    int			AbsoluteX, AbsoluteY;
    int			RelativeX, RelativeY;
    unsigned int	ModKeyMask;
    double		LargeX, LargeY;
    double		DoubleLength, Length;

    XQueryPointer( theDisplay, theWindow,
		   &QueryRoot, &QueryChild,
		   &AbsoluteX, &AbsoluteY,
		   &RelativeX, &RelativeY,
		   &ModKeyMask );

    PrevMouseX = MouseX;
    PrevMouseY = MouseY;

    MouseX = RelativeX;
    MouseY = RelativeY;

    LargeX = (double)( MouseX - NekoX - BITMAP_WIDTH / 2 );
    LargeY = (double)( MouseY - NekoY - BITMAP_HEIGHT );

    DoubleLength = LargeX * LargeX + LargeY * LargeY;

    if ( DoubleLength != (double)0 ) {
	Length = sqrt( DoubleLength );
	if ( Length <= NekoSpeed ) {
	    NekoMoveDx = (int)LargeX;
	    NekoMoveDy = (int)LargeY;
	} else {
	    NekoMoveDx = (int)( ( NekoSpeed * LargeX ) / Length );
	    NekoMoveDy = (int)( ( NekoSpeed * LargeY ) / Length );
	}
    } else {
	NekoMoveDx = NekoMoveDy = 0;
    }
}


/*--------------------------------------------------------------
 *
 *	F0:n2r@OG-IA2h=hM}
 *
 *--------------------------------------------------------------*/

void
NekoThinkDraw()
{
#ifndef	DEBUG
    CalcDxDy();
#endif

    if ( NekoState != NEKO_SLEEP ) {
	DrawNeko( NekoX, NekoY,
		  NekoTickCount % 2 == 0 ?
		  *(AnimationPattern[ NekoState ].TickEvenGCPtr) :
		  *(AnimationPattern[ NekoState ].TickOddGCPtr) );
    } else {
	DrawNeko( NekoX, NekoY,
		  NekoTickCount % 8 <= 3 ?
		  *(AnimationPattern[ NekoState ].TickEvenGCPtr) :
		  *(AnimationPattern[ NekoState ].TickOddGCPtr) );
    }

    TickCount();

    switch ( NekoState ) {
    case NEKO_STOP:
	if ( IsNekoMoveStart() ) {
	    SetNekoState( NEKO_AWAKE );
	    break;
	}
	if ( NekoStateCount < NEKO_STOP_TIME ) {
	    break;
	}
	if ( NekoMoveDx < 0 && NekoX <= 0 ) {
	    SetNekoState( NEKO_L_TOGI );
	} else if ( NekoMoveDx > 0 && NekoX >= WindowWidth - BITMAP_WIDTH ) {
	    SetNekoState( NEKO_R_TOGI );
	} else if ( NekoMoveDy < 0 && NekoY <= 0 ) {
	    SetNekoState( NEKO_U_TOGI );
	} else if ( NekoMoveDy > 0 && NekoY >= WindowHeight - BITMAP_HEIGHT ) {
	    SetNekoState( NEKO_D_TOGI );
	} else {
	    SetNekoState( NEKO_JARE );
	}
	break;
    case NEKO_JARE:
	if ( IsNekoMoveStart() ) {
	    SetNekoState( NEKO_AWAKE );
	    break;
	}
	if ( NekoStateCount < NEKO_JARE_TIME ) {
	    break;
	}
	SetNekoState( NEKO_KAKI );
	break;
    case NEKO_KAKI:
	if ( IsNekoMoveStart() ) {
	    SetNekoState( NEKO_AWAKE );
	    break;
	}
	if ( NekoStateCount < NEKO_KAKI_TIME ) {
	    break;
	}
	SetNekoState( NEKO_AKUBI );
	break;
    case NEKO_AKUBI:
	if ( IsNekoMoveStart() ) {
	    SetNekoState( NEKO_AWAKE );
	    break;
	}
	if ( NekoStateCount < NEKO_AKUBI_TIME ) {
	    break;
	}
	SetNekoState( NEKO_SLEEP );
	break;
    case NEKO_SLEEP:
	if ( IsNekoMoveStart() ) {
	    SetNekoState( NEKO_AWAKE );
	    break;
	}
	break;
    case NEKO_AWAKE:
	if ( NekoStateCount < NEKO_AWAKE_TIME ) {
	    break;
	}
	NekoDirection();	/* G-$,F0$/8~$-$r5a$a$k */
	break;
    case NEKO_U_MOVE:
    case NEKO_D_MOVE:
    case NEKO_L_MOVE:
    case NEKO_R_MOVE:
    case NEKO_UL_MOVE:
    case NEKO_UR_MOVE:
    case NEKO_DL_MOVE:
    case NEKO_DR_MOVE:
	NekoX += NekoMoveDx;
	NekoY += NekoMoveDy;
	NekoDirection();
	if ( IsWindowOver() ) {
	    if ( IsNekoDontMove() ) {
		SetNekoState( NEKO_STOP );
	    }
	}
	break;
    case NEKO_U_TOGI:
    case NEKO_D_TOGI:
    case NEKO_L_TOGI:
    case NEKO_R_TOGI:
	if ( IsNekoMoveStart() ) {
	    SetNekoState( NEKO_AWAKE );
	    break;
	}
	if ( NekoStateCount < NEKO_TOGI_TIME ) {
	    break;
	}
	SetNekoState( NEKO_KAKI );
	break;
    default:
	/* Internal Error */
	SetNekoState( NEKO_STOP );
	break;
    }

    Interval();
}


#ifdef	DEBUG

/*--------------------------------------------------------------
 *
 *	%-%c%i%/%?!<0lMwI=<(!J5/31/93P11500MQ!K
 *
 *--------------------------------------------------------------*/

void
DisplayCharacters()
{
    int		Index;
    int		x, y;

    for ( Index = 0, x = 0, y = 0;
	  BitmapGCDataTable[ Index ].GCCreatePtr != NULL; Index++ ) {

	DrawNeko( x, y, *(BitmapGCDataTable[ Index ].GCCreatePtr) );
	XFlush( theDisplay );

	x += BITMAP_WIDTH;

	if ( x > WindowWidth - BITMAP_WIDTH ) {
	    x = 0;
	    y += BITMAP_HEIGHT;
	    if ( y > WindowHeight - BITMAP_HEIGHT) {
		break;
	    }
	}
    }
}

#endif	/* DEBUG */


/*--------------------------------------------------------------
 *
 *	%-!<%$s%H=hM}
 *
 *--------------------------------------------------------------*/

Bool
ProcessKeyPress( theKeyEvent )
    XKeyEvent	*theKeyEvent;
{
    int			Length;
    int			theKeyBufferMaxLen = AVAIL_KEYBUF;
    char		theKeyBuffer[ AVAIL_KEYBUF + 1 ];
    KeySym		theKeySym;
    XComposeStatus	theComposeStatus;
    Bool		ReturnState;

    ReturnState = True;

    Length = XLookupString( theKeyEvent,
			    theKeyBuffer, theKeyBufferMaxLen,
			    &theKeySym, &theComposeStatus );

    if ( Length > 0 ) {
	switch ( theKeyBuffer[ 0 ] ) {
	case 'q':
	case 'Q':
	    if ( theKeyEvent->state & Mod1Mask ) {	/* META (Alt) %-!< */
		ReturnState = False;
	    }
	    break;
	default:
	    break;
	}
    }

#ifdef	DEBUG
    if ( EventState == DEBUG_MOVE ) {
	switch ( theKeySym ) {
	case XK_KP_1:
	    NekoMoveDx = -(int)( NekoSpeed / sqrt( (double)2 ) );
	    NekoMoveDy = -NekoMoveDx;
	    break;
	case XK_KP_2:
	    NekoMoveDx = 0;
	    NekoMoveDy = (int)NekoSpeed;
	    break;
	case XK_KP_3:
	    NekoMoveDx = (int)( NekoSpeed / sqrt( (double)2 ) );
	    NekoMoveDy = NekoMoveDx;
	    break;
	case XK_KP_4:
	    NekoMoveDx = -(int)NekoSpeed;
	    NekoMoveDy = 0;
	    break;
	case XK_KP_5:
	    NekoMoveDx = 0;
	    NekoMoveDy = 0;
	    break;
	case XK_KP_6:
	    NekoMoveDx = (int)NekoSpeed;
	    NekoMoveDy = 0;
	    break;
	case XK_KP_7:
	    NekoMoveDx = -(int)( NekoSpeed / sqrt( (double)2 ) );
	    NekoMoveDy = NekoMoveDx;
	    break;
	case XK_KP_8:
	    NekoMoveDx = 0;
	    NekoMoveDy = -(int)NekoSpeed;
	    break;
	case XK_KP_9:
	    NekoMoveDx = (int)( NekoSpeed / sqrt( (double)2 ) );
	    NekoMoveDy = -NekoMoveDx;
	    break;
	}
    }
#endif

    return( ReturnState );
}


/*--------------------------------------------------------------
 *
 *	G-0LCVD4@0
 *
 *--------------------------------------------------------------*/

void
NekoAdjust()
{
    if ( NekoX < 0 ) {
	NekoX = 0;
    } else if ( NekoX > WindowWidth - BITMAP_WIDTH ) {
	NekoX = WindowWidth - BITMAP_WIDTH;
    }

    if ( NekoY < 0 ) {
	NekoY = 0;
    } else if ( NekoY > WindowHeight - BITMAP_HEIGHT ) {
	NekoY = WindowHeight - BITMAP_HEIGHT;
    }
}


/*--------------------------------------------------------------
 *
 *	%$s%H=hM}
 *
 *--------------------------------------------------------------*/

Bool
ProcessEvent()
{
    XEvent	theEvent;
    Bool	ContinueState = True;

    switch ( EventState ) {
    case NORMAL_STATE:
	while ( XCheckMaskEvent( theDisplay, EVENT_MASK1, &theEvent ) ) {
	    switch ( theEvent.type ) {
	    case ConfigureNotify:
		WindowWidth = theEvent.xconfigure.width;
		WindowHeight = theEvent.xconfigure.height;
		WindowPointX = theEvent.xconfigure.x;
		WindowPointY = theEvent.xconfigure.y;
		BorderWidth = theEvent.xconfigure.border_width;
		NekoAdjust();
		break;
	    case Expose:
		if ( theEvent.xexpose.count == 0 ) {
		    RedrawNeko();
		}
		break;
	    case MapNotify:
		RedrawNeko();
		break;
	    case KeyPress:
		ContinueState = ProcessKeyPress( &theEvent.xkey );
		if ( !ContinueState ) {
		    return( ContinueState );
		}
		break;
	    default:
		/* Unknown Event */
		break;
	    }
	}
	break;
#ifdef	DEBUG
    case DEBUG_LIST:
	XNextEvent( theDisplay, &theEvent );
	switch ( theEvent.type ) {
	case ConfigureNotify:
	    WindowWidth = theEvent.xconfigure.width;
	    WindowHeight = theEvent.xconfigure.height;
	    WindowPointX = theEvent.xconfigure.x;
	    WindowPointY = theEvent.xconfigure.y;
	    BorderWidth = theEvent.xconfigure.border_width;
	    break;
	case Expose:
	    if ( theEvent.xexpose.count == 0 ) {
		DisplayCharacters();
	    }
	    break;
	case MapNotify:
	    DisplayCharacters();
	    break;
	case KeyPress:
	    ContinueState = ProcessKeyPress( &theEvent );
	    break;
	default:
	    /* Unknown Event */
	    break;
	}
	break;
    case DEBUG_MOVE:
	while ( XCheckMaskEvent( theDisplay, EVENT_MASK1, &theEvent ) ) {
	    switch ( theEvent.type ) {
	    case ConfigureNotify:
		WindowWidth = theEvent.xconfigure.width;
		WindowHeight = theEvent.xconfigure.height;
		WindowPointX = theEvent.xconfigure.x;
		WindowPointY = theEvent.xconfigure.y;
		BorderWidth = theEvent.xconfigure.border_width;
		NekoAdjust();
		break;
	    case Expose:
		if ( theEvent.xexpose.count == 0 ) {
		    RedrawNeko();
		}
		break;
	    case MapNotify:
		RedrawNeko();
		break;
	    case KeyPress:
		ContinueState = ProcessKeyPress( &theEvent );
		if ( !ContinueState ) {
		    return( ContinueState );
		}
		break;
	    default:
		/* Unknown Event */
		break;
	    }
	}
	break;
#endif
    default:
	/* Internal Error */
	break;
    }

    return( ContinueState );
}


/*--------------------------------------------------------------
 *
 *	G-=hM}
 *
 *--------------------------------------------------------------*/

void
ProcessNeko()
{
    struct itimerval	Value;

    /* 4D6-$N=i4|2= */

    EventState = NORMAL_STATE;

    /* G-$N=i4|2= */

    NekoX = ( WindowWidth - BITMAP_WIDTH / 2 ) / 2;
    NekoY = ( WindowHeight - BITMAP_HEIGHT / 2 ) / 2;

    NekoLastX = NekoX;
    NekoLastY = NekoY;

    SetNekoState( NEKO_STOP );

    /* %?%$%^!<@_Dj */

    timerclear( &Value.it_interval );
    timerclear( &Value.it_value );

    Value.it_interval.tv_usec = IntervalTime;
    Value.it_value.tv_usec = IntervalTime;

    setitimer( ITIMER_REAL, &Value, 0 );

    /* %a%$%s=hM} */

    do {
	NekoThinkDraw();
    } while ( ProcessEvent() );
}


#ifdef	DEBUG

/*--------------------------------------------------------------
 *
 *	G-0lMw!J5/31/93P14460MQ!K
 *
 *--------------------------------------------------------------*/

void
NekoList()
{
    EventState = DEBUG_LIST;

    fprintf( stderr, "\n" );
    fprintf( stderr, "G-0lMw$rI=<($7$^$9!#(Quit !D Alt-Q)\n" );
    fprintf( stderr, "\n" );

    XSelectInput( theDisplay, theWindow, EVENT_MASK2 );

    while ( ProcessEvent() );
}


/*--------------------------------------------------------------
 *
 *	G-0\F0SCCS/s.xneko.c9%H!J5/31/93P14670MQ!K
 *
 *--------------------------------------------------------------*/

void
NekoMoveTest()
{
    struct itimerval	Value;

    /* 4D6-$N=i4|2= */

    EventState = DEBUG_MOVE;

    /* G-$N=i4|2= */

    NekoX = ( WindowWidth - BITMAP_WIDTH / 2 ) / 2;
    NekoY = ( WindowHeight - BITMAP_HEIGHT / 2 ) / 2;

    NekoLastX = NekoX;
    NekoLastY = NekoY;

    SetNekoState( NEKO_STOP );

    /* %?%$%^!<@_Dj */

    timerclear( &Value.it_interval );
    timerclear( &Value.it_value );

    Value.it_interval.tv_usec = IntervalTime;
    Value.it_value.tv_usec = IntervalTime;

    setitimer( ITIMER_REAL, &Value, 0 );

    /* %a%$%s=hM} */

    fprintf( stderr, "\n" );
    fprintf( stderr, "G-$N0\F0SCCS/s.xneko.c9%H$r9T$$$^$9!#(Quit !D Alt-Q)\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "\t%-!<May 31, 1993C%I>e$NSCCS/s.xneko.cs%-!<$GG-$r0\F0$5$;$F2<$5$$!#\n" );
    fprintf( stderr, "\t(M-8z$J%-!<$O#1!A#9$G$9!#)\n" );
    fprintf( stderr, "\n" );

    do {
	NekoThinkDraw();
    } while ( ProcessEvent() );
}


/*--------------------------------------------------------------
 *
 *	%a%K%e!<=hM}!J5/31/93P15170MQ!K
 *
 *--------------------------------------------------------------*/

void
ProcessDebugMenu()
{
    int		UserSelectNo = 0;
    char	UserAnswer[ BUFSIZ ];

    fprintf( stderr, "\n" );
    fprintf( stderr, "!Zxneko 5/31/93P15280%a%K%e!<![\n" );

    while ( !( UserSelectNo >= 1 && UserSelectNo <= 2 ) ) {
	fprintf( stderr, "\n" );
	fprintf( stderr, "\t1)!!G-%-%c%i%/%?!<0lMwI=<(\n" );
	fprintf( stderr, "\t2)!!G-0\F0SCCS/s.xneko.c9%H\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "Select: " );

	fgets( UserAnswer, sizeof( UserAnswer ), stdin );

	UserSelectNo = atoi( UserAnswer );

	if ( !( UserSelectNo >= 1 && UserSelectNo <= 2 ) ) {
	    fprintf( stderr, "\n" );
	    fprintf( stderr, "@5$7$$HV9f$rA*Br$7$F2<$5$$!#\n" );
	}
    }

    switch ( UserSelectNo ) {
    case 1:
	/* G-%-%c%i%/%?!<0lMwI=<( */
	NekoList();
	break;
    case 2:
	/* G-0\F0SCCS/s.xneko.c9%H */
	NekoMoveTest();
	break;
    default:
	/* Internal Error */
	break;
    }

    fprintf( stderr, "SCCS/s.xneko.c9%H=*N;!#\n" );
    fprintf( stderr, "\n" );
}

#endif	/* DEBUG */


/*--------------------------------------------------------------
 *
 *	SIGALRM %7%0%J%k=hM}
 *
 *--------------------------------------------------------------*/

void
NullFunction()
{
    /* No Operation */
}


/*--------------------------------------------------------------
 *
 *	Usage
 *
 *--------------------------------------------------------------*/

void
Usage()
{
    fprintf( stderr,
	     "Usage: %s [-display <display>] [-geometry <geometry>] \\\n",
	     ProgramName );
    fprintf( stderr, "  [-title <title>] [-name <title>] [-iconic] \\\n" );
    fprintf( stderr, "  [-speed <speed>] [-time <time>] [-help]\n" );
}


/*--------------------------------------------------------------
 *
 *	#XMay 31, 1993i%a!<%?I>2A
 *
 *--------------------------------------------------------------*/

Bool
GetArguments( argc, argv, theDisplayName, theGeometry, theTitle,
	      NekoSpeed, IntervalTime )
    int		argc;
    char	*argv[];
    char	*theDisplayName;
    char	*theGeometry;
    char	*theTitle;
    double	*NekoSpeed;
    long	*IntervalTime;
{
    int		ArgCounter;
    Bool	iconicState;

    theDisplayName[ 0 ] = '\0';
    theGeometry[ 0 ] = '\0';
    theTitle[ 0 ] = '\0';

    iconicState = False;

    for ( ArgCounter = 0; ArgCounter < argc; ArgCounter++ ) {

	if ( strncmp( argv[ ArgCounter ], "-h", 2 ) == 0 ) {
	    Usage();
	    exit( 0 );
	} else if ( strcmp( argv[ ArgCounter ], "-display" ) == 0 ) {
	    ArgCounter++;
	    if ( ArgCounter < argc ) {
		strcpy( theDisplayName, argv[ ArgCounter ] );
	    } else {
		fprintf( stderr, "%s: -display option error.\n", ProgramName );
		exit( 1 );
	    }
	} else if ( strncmp( argv[ ArgCounter ], "-geom", 5 ) == 0 ) {
	    ArgCounter++;
	    if ( ArgCounter < argc ) {
		strcpy( theGeometry, argv[ ArgCounter ] );
	    } else {
		fprintf( stderr,
			 "%s: -geometry option error.\n", ProgramName );
		exit( 1 );
	    }
	} else if ( ( strcmp( argv[ ArgCounter ], "-title" ) == 0 )
	     || ( strcmp( argv[ ArgCounter ], "-name" ) == 0 ) ) {
	    ArgCounter++;
	    if ( ArgCounter < argc ) {
		strcpy( theTitle, argv[ ArgCounter ] );
	    } else {
		fprintf( stderr, "%s: -title option error.\n", ProgramName );
		exit( 1 );
	    }
	} else if ( strcmp( argv[ ArgCounter ], "-iconic" ) == 0 ) {
	    iconicState = True;
	} else if ( strcmp( argv[ ArgCounter ], "-speed" ) == 0 ) {
	    ArgCounter++;
	    if ( ArgCounter < argc ) {
		*NekoSpeed = atof( argv[ ArgCounter ] );
	    } else {
		fprintf( stderr, "%s: -speed option error.\n", ProgramName );
		exit( 1 );
	    }
	} else if ( strcmp( argv[ ArgCounter ], "-time" ) == 0 ) {
	    ArgCounter++;
	    if ( ArgCounter < argc ) {
		*IntervalTime = atol( argv[ ArgCounter ] );
	    } else {
		fprintf( stderr, "%s: -time option error.\n", ProgramName );
		exit( 1 );
	    }
	} else {
	    fprintf( stderr,
		     "%s: Unknown option \"%s\".\n", ProgramName,
						     argv[ ArgCounter ] );
	    Usage();
	    exit( 1 );
	}
    }

    if ( strlen( theDisplayName ) < 1 ) {
	theDisplayName = NULL;
    }

    if ( strlen( theGeometry ) < 1 ) {
	theGeometry = NULL;
    }

    return( iconicState );
}


/*--------------------------------------------------------------
 *
 *	%a%$%s4X?t
 *
 *--------------------------------------------------------------*/

int
main( argc, argv )
    int		argc;
    char	*argv[];
{
    Bool	iconicState;
    char	theDisplayName[ DIRNAMELEN ];
    char	theGeometry[ DIRNAMELEN ];
    char	theTitle[ DIRNAMELEN ];

    ProgramName = argv[ 0 ];

    argc--;
    argv++;

    iconicState = GetArguments( argc, argv,
				theDisplayName,
				theGeometry,
				theTitle,
				&NekoSpeed,
				&IntervalTime );

    InitScreen( theDisplayName, theGeometry, theTitle, iconicState );

    signal( SIGALRM, NullFunction );

    SinPiPer8Times3 = sin( PI_PER8 * (double)3 );
    SinPiPer8 = sin( PI_PER8 );

#ifndef	DEBUG
    ProcessNeko();
#else
    ProcessDebugMenu();
#endif

    exit( 0 );
}
