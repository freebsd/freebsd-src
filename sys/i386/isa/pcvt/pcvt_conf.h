/*
 * Copyright (c) 1999 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1994 Brian Dunford-Shore.
 *
 * All rights reserved.
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
 *	This product includes software developed by
 *	Hellmuth Michaelis, Brian Dunford-Shore and Joerg Wunsch.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*---------------------------------------------------------------------------
 *
 *	pcvt_conf.h	VT220 driver global configuration file
 *	------------------------------------------------------
 *
 *	Last Edit-Date: [Mon Dec 27 14:09:58 1999]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 *
 * from: Onno van der Linden    c/o   frank@fwi.uva.nl
 *
 * Here's an idea how to automatically detect the version of NetBSD pcvt is
 * being compiled on:
 *
 * NetBSD 0.8 : NetBSD0_8 defined in <sys/param.h>
 * NetBSD 0.9 : NetBSD0_9 defined in <sys/param.h>
 * NetBSD 1.0 : NetBSD1_0 defined as 1 in <sys/param.h>
 * NetBSD 1.0A: NetBSD1_0 defined as 2 in <sys/param.h>
 *
 * The NetBSDx_y defines are mutual exclusive.
 *
 * This leads to something like this in pcvt_hdr.h (#elif is possible too):
 *
 *---------------------------------------------------------------------------*/

#ifdef NetBSD0_8
#define PCVT_NETBSD 8
#endif

#ifdef NetBSD0_9
#define PCVT_NETBSD 9
#endif

#ifdef NetBSD1_0
#if NetBSD1_0 > 1
#define PCVT_NETBSD 199
#else
#define PCVT_NETBSD 100
#endif
#endif

/*---------------------------------------------------------------------------
 * Note that each of the options below should rather be overriden by the
 * kernel config file instead of this .h file - this allows for different
 * definitions in different kernels compiled at the same machine
 *
 * The convention is as follows:
 *
 *	options "PCVT_FOO=1"  - enables the option
 * 	options "PCVT_FOO"    - is a synonym for the above
 *	options "PCVT_FOO=0"  - disables the option
 *
 * omitting an option defaults to what is shown below
 *
 * exceptions from this rule are i.e.:
 *
 *	options "PCVT_NSCREENS=x"
 *	options "PCVT_SCANSET=x"
 *	options "PCVT_UPDATEFAST=x"
 *	options "PCVT_UPDATESLOW=x"
 *	options "PCVT_SYSBEEPF=x"
 *
 * which are always numeric!
 *---------------------------------------------------------------------------*/

/* -------------------------------------------------------------------- */
/* -------------------- OPERATING SYSTEM ------------------------------ */
/* -------------------------------------------------------------------- */

/*
 *  one of the following options must be set in the kernel config file:
 *
 *======================================================================*
 *			N e t B S D					*
 *======================================================================*
 *
 *	options "PCVT_NETBSD=xxx" enables support for NetBSD
 *
 *	select:
 *		PCVT_NETBSD =   9	for NetBSD 0.9
 *		PCVT_NETBSD =  99	for PRE-1.0 NetBSD-current
 *		PCVT_NETBSD = 100	for NetBSD 1.0
 *		PCVT_NETBSD = 199	for PRE-2.0 NetBSD-current
 *
 *
 *======================================================================*
 *			F r e e B S D					*
 *======================================================================*
 *
 *	options "PCVT_FREEBSD=xxx" enables support for FreeBSD
 *
 *	select:
 *		PCVT_FREEBSD = 102	for 1.0 release (actually 1.0.2)
 *		PCVT_FREEBSD = 110	for FreeBSD 1.1-Release
 *		PCVT_FREEBSD = 115	for FreeBSD 1.1.5.1-Release
 *		PCVT_FREEBSD = 200	for FreeBSD 2.0-Release
 *		PCVT_FREEBSD = 210	for FreeBSD 2.1-Release
 *
 */

/* -------------------------------------------------------------------- */
/* ---------------- USER PREFERENCE DRIVER OPTIONS -------------------- */
/* -------------------------------------------------------------------- */

/*----------------------------------------------------------------------*/
/* NOTE: if FAT_CURSOR is defined, a block cursor is used instead of	*/
/*       the cursor shape we got from the BIOS, see pcvt_out.c		*/
/*----------------------------------------------------------------------*/

#if !defined PCVT_NSCREENS	/* ---------- DEFAULT: 8 -------------- */
# define PCVT_NSCREENS 8	/* this option defines how many virtual	*/
#endif				/* screens you want to have in your	*/
				/* system. each screen allocates memory,*/
				/* so you can't have an unlimited num-	*/
				/* ber...; the value is intented to be	*/
				/* compile-time overridable by a config	*/
				/* options "PCVT_NSCREENS=x" line	*/

#if !defined PCVT_VT220KEYB	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_VT220KEYB 0	/* this compiles a more vt220-like	*/
#elif PCVT_VT220KEYB != 0	/* keyboardlayout as described in the	*/
# undef PCVT_VT220KEYB		/* file Keyboard.VT220.			*/
# define PCVT_VT220KEYB 1	/* if undefined, a more HP-like         */
#endif				/* keyboardlayout is compiled		*/
				/* try to find out what YOU like !	*/

#if !defined PCVT_SCREENSAVER	/* ---------- DEFAULT: ON ------------- */
# define PCVT_SCREENSAVER 1	/* enable screen saver feature - this	*/
#elif PCVT_SCREENSAVER != 0	/* just blanks the display screen.	*/
# undef PCVT_SCREENSAVER	/* see PCVT_PRETTYSCRNS below ...	*/
# define PCVT_SCREENSAVER 1
#endif

#if !defined PCVT_PRETTYSCRNS	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_PRETTYSCRNS 0	/* for the cost of some microseconds of	*/
#elif PCVT_PRETTYSCRNS != 0	/* cpu time this adds a more "pretty"	*/
# undef PCVT_PRETTYSCRNS	/* version to the screensaver, an "*"	*/
# define PCVT_PRETTYSCRNS 1	/* in random locations of the display.	*/
#endif				/* NOTE: this should not be defined if	*/
				/* you have an energy-saving monitor 	*/
				/* which turns off the display if its	*/
				/* black !!!!!!				*/

#if !defined PCVT_CTRL_ALT_DEL	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_CTRL_ALT_DEL 0	/* this enables the execution of a cpu	*/
#elif PCVT_CTRL_ALT_DEL != 0	/* reset by pressing the CTRL, ALT and	*/
# undef PCVT_CTRL_ALT_DEL	/* DEL keys simultanously. Because this	*/
# define PCVT_CTRL_ALT_DEL 1	/* is a feature of an ancient simple	*/
#endif				/* bootstrap loader, it does not belong */
				/* into modern operating systems and 	*/
				/* was commented out by default ...	*/

#if !defined PCVT_KBD_FIFO	/* ---------- DEFAULT: ON ------------- */
# define PCVT_KBD_FIFO 1	/* this enables Keyboad fifo so that we */
#elif PCVT_KBD_FIFO != 0	/* are not any longer forced to switch  */
# undef PCVT_KBD_FIFO		/* off tty interrupts while switching   */
# define PCVT_KBD_FIFO 1	/* virtual screens - AND loosing chars  */
#endif				/* on the serial lines is gone :-)      */

#if PCVT_KBD_FIFO

# if !defined PCVT_KBD_FIFO_SZ	/* ---------- DEFAULT: 256 ------------ */
#  define PCVT_KBD_FIFO_SZ 256	/* this specifies the size of the above */
# elif PCVT_KBD_FIFO_SZ < 16	/* mentioned keyboard buffer. buffer    */
#  undef PCVT_KBD_FIFO_SZ	/* overflows are logged via syslog, so  */
#  define PCVT_KBD_FIFO_SZ 256	/* have a look at /var/log/messages     */
# endif

#endif /* PCVT_KBD_FIFO */

#if !defined PCVT_USEKBDSEC	/* ---------- DEFAULT: ON ------------- */
# define PCVT_USEKBDSEC 1	/* do not set the COMMAND_INHOVR bit	*/
#elif PCVT_USEKBDSEC != 0	/* (1 = override security lock inhibit) */
# undef PCVT_USEKBDSEC		/* when initializing the keyboard, so   */
# define PCVT_USEKBDSEC 1	/* that security locking should work    */
#endif				/* now. I guess this has to be done also*/
				/* in the boot code to prevent single   */
				/* user startup ....                    */

#if !defined PCVT_24LINESDEF	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_24LINESDEF 0	/* use 24 lines in VT 25 lines mode and	*/
#elif PCVT_24LINESDEF != 0	/* HP 28 lines mode by default to have	*/
# undef PCVT_24LINESDEF		/* the the better compatibility to the	*/
# define PCVT_24LINESDEF 1	/* real VT220 - you can switch between	*/
#endif				/* the maximum possible screensizes in	*/
				/* those two modes (25 lines) and true	*/
				/* compatibility (24 lines) by using	*/
				/* the scon utility at runtime		*/

#if !defined PCVT_EMU_MOUSE	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_EMU_MOUSE 0	/* emulate a mouse systems mouse via	*/
#elif PCVT_EMU_MOUSE != 0	/* the keypad; this is experimental	*/
# undef PCVT_EMU_MOUSE		/* code intented to be used on note-	*/
# define PCVT_EMU_MOUSE 1	/* books in conjunction with XFree86;	*/
#endif				/* look at the comments in pcvt_kbd.c	*/
				/* if you are interested in testing it.	*/

#if !defined PCVT_META_ESC      /* ---------- DEFAULT: OFF ------------ */
# define PCVT_META_ESC 0        /* if ON, send the sequence "ESC key"	*/
#elif PCVT_META_ESC != 0        /* for a meta-shifted key; if OFF,	*/
# undef PCVT_META_ESC           /* send the normal key code with 0x80	*/
# define PCVT_META_ESC 1        /* added.				*/
#endif

#if !defined PCVT_SW0CNOUTP     /* ---------- DEFAULT: OFF ------------ */
# define PCVT_SW0CNOUTP 0       /* if ON, on console/kernel output the  */
#elif PCVT_SW0CNOUTP != 0       /* current screen is switched to screen */
# undef PCVT_SW0CNOUTP          /* 0 if not already at screen 0.        */
# define PCVT_SW0CNOUTP 1	/* CAUTION: CURRENTLY THIS CAUSES AN X- */
#endif				/* SESSION TO CLUTTER VIDEO MEMORY !!!! */

/* -------------------------------------------------------------------- */
/* -------------------- DRIVER DEBUGGING ------------------------------ */
/* -------------------------------------------------------------------- */

#if !defined PCVT_SHOWKEYS	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_SHOWKEYS 0	/* this replaces the system load line	*/
#elif PCVT_SHOWKEYS != 0	/* on the vt 0 in hp mode with a display*/
# undef PCVT_SHOWKEYS		/* of the most recent keyboard scan-	*/
# define PCVT_SHOWKEYS 1	/* and status codes received from the	*/
#endif				/* keyboard controller chip.		*/
				/* this is just for some hardcore	*/
				/* keyboarders ....			*/

/* -------------------------------------------------------------------- */
/* -------------------- DRIVER OPTIONS -------------------------------- */
/* -------------------------------------------------------------------- */
/*     it is unlikely that anybody wants to change anything below       */

#if !defined PCVT_NO_LED_UPDATE	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_NO_LED_UPDATE 0	/* On some (Notebook?) keyboards it is	*/
#elif PCVT_NO_LED_UPDATE != 0	/* not possible to update the LED's	*/
# undef PCVT_NO_LED_UPDATE	/* without hanging the keyboard after-	*/
# define PCVT_NO_LED_UPDATE 1	/* wards. If you experience Problems	*/
#endif				/* like this, try to enable this option	*/

#if !defined PCVT_PORTIO_DELAY  /* ---------- DEFAULT: ON ------------- */
# define PCVT_PORTIO_DELAY 1	/* Defining PCVT_PORTIO_DELAY lets pcvt */
#elif PCVT_PORTIO_DELAY != 0	/* use multiple accesses to port 0x84   */
# undef PCVT_PORTIO_DELAY	/* to produce a delay of 7 us needed for*/
# define PCVT_PORTIO_DELAY 1	/* accessing the keyboard controller,   */
#endif				/* otherwise the system delay functions */
				/* are used.                            */

#if !defined PCVT_PCBURST	/* ---------- DEFAULT: 256 ------------ */
# define PCVT_PCBURST 256	/* NETBSD and FreeBSD >= 2.0 only: this */
#endif				/* is the number of output characters	*/
				/* handled together as a burst in 	*/
				/* routine pcstart(), file pcvt_drv.c	*/

#if !defined PCVT_SCANSET	/* ---------- DEFAULT: 1 -------------- */
# define PCVT_SCANSET 1		/* define the keyboard scancode set you	*/
#endif				/* want to use:				*/
				/* 1 - code set 1	(supported)	*/
				/* 2 - code set 2	(supported)	*/
				/* 3 - code set 3	(UNsupported)	*/

#if !defined PCVT_KEYBDID	/* ---------- DEFAULT: ON ------------- */
# define PCVT_KEYBDID 1		/* check type of keyboard connected. at	*/
#elif PCVT_KEYBDID != 0		/* least HP-keyboards send an id other	*/
# undef PCVT_KEYBDID		/* than the industry standard, so it	*/
# define PCVT_KEYBDID 1		/* CAN lead to problems. if you have	*/
#endif				/* problems with this, TELL ME PLEASE !	*/

#if !defined PCVT_SIGWINCH	/* ---------- DEFAULT: ON ------------- */
# define PCVT_SIGWINCH 1	/* this sends a SIGWINCH signal in case	*/
#elif PCVT_SIGWINCH != 0	/* the window size is changed. to try,	*/
# undef PCVT_SIGWINCH		/* issue "scons -s<size>" while in elvis*/
# define PCVT_SIGWINCH 1	/* and you'll see the effect.		*/
#endif				/* i'm not sure, whether this feature	*/
				/* has to be in the driver or has to    */
				/* move as an ioctl call to scon ....	*/

#if !defined PCVT_NULLCHARS	/* ---------- DEFAULT: ON ------------- */
# define PCVT_NULLCHARS 1	/* allow the keyboard to send null	*/
#elif PCVT_NULLCHARS != 0	/* (0x00) characters to the calling	*/
# undef PCVT_NULLCHARS		/* program. this has the side effect	*/
# define PCVT_NULLCHARS 1	/* that every undefined key also sends	*/
#endif				/* out nulls. take it as experimental	*/
				/* code, this behaviour will change in	*/
				/* a future release			*/

#if !defined PCVT_BACKUP_FONTS	/* ---------- DEFAULT: ON ------------- */
# define PCVT_BACKUP_FONTS 1	/* fonts are always kept memory-backed;	*/
#elif  PCVT_BACKUP_FONTS != 0	/* otherwise copies are only made if	*/
# undef PCVT_BACKUP_FONTS	/* they are needed.			*/
# define PCVT_BACKUP_FONTS 1
#endif

#ifndef PCVT_UPDATEFAST		/* this is the rate at which the cursor */
# define PCVT_UPDATEFAST (hz/10) /* gets updated with its new position	*/
#endif				/* see: async_update() in pcvt_sup.c	*/

#ifndef PCVT_UPDATESLOW		/* this is the rate at which the cursor	*/
# define PCVT_UPDATESLOW 3	/* position display and the system load	*/
#endif				/* (or the keyboard scancode display)	*/
				/* is updated. the relation is:		*/
				/* PCVT_UPDATEFAST/PCVT_UPDATESLOW	*/

#ifndef PCVT_SYSBEEPF		/* timer chip value to be used for the	*/
# define PCVT_SYSBEEPF 1193182	/* sysbeep frequency value.		*/
#endif				/* this should really go somewhere else,*/
				/* e.g. in isa.h; but it used to be in 	*/
				/* each driver, sometimes even with	*/
				/* different values (:-)		*/

#if !defined PCVT_SETCOLOR	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_SETCOLOR 0	/* enable making colors settable. this	*/
#elif PCVT_SETCOLOR != 0	/* introduces a new escape sequence	*/
# undef PCVT_SETCOLOR		/* <ESC d> which is (i think) not 	*/
# define PCVT_SETCOLOR 1	/* standardized, so this is an option	*/
#endif				/* (Birthday present for Bruce ! :-)    */

#if !defined PCVT_132GENERIC	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_132GENERIC 0	/* if you #define this, you enable	*/
#elif PCVT_132GENERIC != 0	/*	EXPERIMENTAL (!!!!!!!!!!!!)	*/
# undef PCVT_132GENERIC		/* 	USE-AT-YOUR-OWN-RISK, 		*/
# define PCVT_132GENERIC 1	/*	MAY-DAMAGE-YOUR-MONITOR		*/
#endif				/* code to switch generic VGA boards/	*/
				/* chipsets to 132 column mode. Since	*/
				/* i could not verify this option, i	*/
				/* prefer to NOT generally enable this,	*/
				/* if you want to play, look at the 	*/
				/* hints and the code in pcvt_sup.c and	*/
				/* get in contact with Joerg Wunsch, who*/
				/* submitted this code. Be careful !!!	*/

#if !defined PCVT_PALFLICKER	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_PALFLICKER 0	/* this option turns off the screen 	*/
#elif PCVT_PALFLICKER != 0	/* during accesses to the VGA DAC	*/
# undef PCVT_PALFLICKER		/* registers. why: on one fo the tested	*/
# define PCVT_PALFLICKER 1	/* pc's (WD-chipset), accesses to the	*/
#endif				/* vga dac registers caused distortions	*/
				/* on the screen. Ferraro says, one has	*/
				/* to blank the screen. the method used	*/
				/* to accomplish this stopped the noise	*/
				/* but introduced another flicker, so	*/
				/* this is for you to experiment .....	*/
				/* - see also PCVT_WAITRETRACE below --	*/

#if !defined PCVT_WAITRETRACE	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_WAITRETRACE 0	/* this option waits for being in a 	*/
#elif PCVT_WAITRETRACE != 0	/* retrace window prior to accessing	*/
# undef PCVT_WAITRETRACE	/* the VGA DAC registers.		*/
# define PCVT_WAITRETRACE 1	/* this is the other method Ferraro	*/
#endif				/* mentioned in his book. this option 	*/
				/* did eleminate the flicker noticably	*/
				/* but not completely. besides that, it	*/
				/* is implemented as a busy-wait loop	*/
				/* which is a no-no-no in environments	*/
				/* like this - VERY BAD PRACTICE !!!!!	*/
				/* the other method implementing it is	*/
				/* using the vertical retrace irq, but	*/
				/* we get short of irq-lines on pc's.	*/
				/* this is for you to experiment .....	*/
				/* -- see also PCVT_PALFLICKER above -- */

#if !defined PCVT_INHIBIT_NUMLOCK /* --------- DEFAULT: OFF ----------- */
# define PCVT_INHIBIT_NUMLOCK 0 /* A notebook hack: since i am getting	*/
#elif PCVT_INHIBIT_NUMLOCK != 0	/* tired of the numlock LED always	*/
# undef PCVT_INHIBIT_NUMLOCK    /* being turned on - which causes the	*/
# define PCVT_INHIBIT_NUMLOCK 1 /* right half of my keyboard being	*/
#endif                         	/* interpreted as a numeric keypad and	*/
				/* thus going unusable - i want to	*/
				/* have a better control over it. If	*/
				/* this option is enabled, only the	*/
				/* numlock key itself and the related	*/
				/* ioctls will modify the numlock	*/
				/* LED. (The ioctl is needed for the	*/
				/* ServerNumLock feature of XFree86.)	*/
				/* The default state is changed to	*/
				/* numlock off, and the escape		*/
				/* sequences to switch between numeric	*/
				/* and application mode keypad are	*/
				/* silently ignored.			*/

#if !defined PCVT_NOFASTSCROLL	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_NOFASTSCROLL 0	/* If off, enables code for fast scroll.*/
#elif PCVT_NOFASTSCROLL != 0	/* This is done by changing the CRTC	*/
# undef PCVT_NOFASTSCROLL	/* screen start address for scrolling	*/
# define PCVT_NOFASTSCROLL 1	/* and using 2 times the screen size as	*/
#endif				/* buffer. The fastscroll code works	*/
				/* ONLY for VGA/EGA/CGA because it uses */
				/* the crtc for hardware scrolling and	*/
				/* therefore needs more than the one	*/
				/* page video memory MDA and most 	*/
				/* Hercules boards support.		*/
				/* If you run pcvt ONLY on MDA/Hercules */
				/* you should disable fastscroll to save*/
				/* the time to decide which board you	*/
				/* are running pcvt on at runtime.	*/
				/*     [see roll_up() and roll_down().]	*/

#if !defined PCVT_SLOW_INTERRUPT/* ---------- DEFAULT: OFF ------------ */
# define PCVT_SLOW_INTERRUPT 0	/* If off, protecting critical regions	*/
#elif PCVT_SLOW_INTERRUPT != 0	/* in the keyboard fifo code is done by	*/
# undef PCVT_SLOW_INTERRUPT	/* disabling the processor irq's, if on */
# define PCVT_SLOW_INTERRUPT 1	/* this is done by spl()/splx() calls.  */
#endif

#ifdef XSERVER

#if !defined PCVT_USL_VT_COMPAT	/* ---------- DEFAULT: ON ------------- */
# define PCVT_USL_VT_COMPAT 1	/* this option enables multiple virtual */
#elif PCVT_USL_VT_COMPAT != 0	/* screen support for XFree86. If set	*/
# undef PCVT_USL_VT_COMPAT	/* to off, support for a "classic"	*/
# define PCVT_USL_VT_COMPAT 1	/* single screen only X server is	*/
#endif				/* compiled in. If enabled, most of the	*/
				/* ioctl's from SYSV/USL are supported	*/
				/* to run multiple X servers and/or 	*/
				/* character terminal sessions.		*/

#endif /* XSERVER */

/*---------------------------------------------------------------------------*
 *	Kernel messages attribute definitions
 *	These define the foreground and background attributes used to
 *	emphasize messages from the kernel on color and mono displays.
 *---------------------------------------------------------------------------*/

#if !defined COLOR_KERNEL_FG		/* color displays		*/
#define COLOR_KERNEL_FG	FG_LIGHTGREY	/* kernel messages, foreground	*/
#endif
#if !defined COLOR_KERNEL_BG
#define COLOR_KERNEL_BG	BG_RED		/* kernel messages, background	*/
#endif

#if !defined MONO_KERNEL_FG		/* monochrome displays		*/
#define MONO_KERNEL_FG	FG_UNDERLINE	/* kernel messages, foreground	*/
#endif
#if !defined MONO_KERNEL_BG
#define MONO_KERNEL_BG	BG_BLACK	/* kernel messages, background	*/
#endif

/*---------------------------------- E O F ----------------------------------*/
