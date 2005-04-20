/*-
 * Copyright (c) 1999, 2000 Hellmuth Michaelis
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
 *	Last Edit-Date: [Fri Mar 31 10:20:27 2000]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

/* -------------------------------------------------------------------- */
/* ---------------- USER PREFERENCE DRIVER OPTIONS -------------------- */
/* -------------------------------------------------------------------- */

#if !defined PCVT_NSCREENS	/* ---------- DEFAULT: 8 -------------- */
# define PCVT_NSCREENS 8	/* this option defines how many virtual	*/
#endif				/* screens you want to have in your	*/
				/* system.				*/

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

#if !defined PCVT_GREENSAVER	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_GREENSAVER 0	/* screensaver mode that enables 	*/
#elif PCVT_GREENSAVER != 0	/* power-save mode			*/
# undef PCVT_GREENSAVER
# define PCVT_GREENSAVER 1
#endif

#if !defined PCVT_CTRL_ALT_DEL	/* ---------- DEFAULT: OFF ------------ */
# define PCVT_CTRL_ALT_DEL 0	/* this enables the execution of a cpu	*/
#elif PCVT_CTRL_ALT_DEL != 0	/* reset by pressing the CTRL, ALT and	*/
# undef PCVT_CTRL_ALT_DEL	/* DEL keys simultanously. Because this	*/
# define PCVT_CTRL_ALT_DEL 1	/* is a feature of an ancient simple	*/
#endif				/* bootstrap loader, it does not belong */
				/* into modern operating systems and 	*/
				/* was commented out by default ...	*/

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

#if !defined PCVT_META_ESC      /* ---------- DEFAULT: OFF ------------ */
# define PCVT_META_ESC 0        /* if ON, send the sequence "ESC key"	*/
#elif PCVT_META_ESC != 0        /* for a meta-shifted key; if OFF,	*/
# undef PCVT_META_ESC           /* send the normal key code with 0x80	*/
# define PCVT_META_ESC 1        /* added.				*/
#endif

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

#if !defined PCVT_SCANSET	/* ---------- DEFAULT: 1 -------------- */
# define PCVT_SCANSET 1		/* define the keyboard scancode set you	*/
#endif				/* want to use:				*/
				/* 1 - code set 1	(supported)	*/
				/* 2 - code set 2	(supported)	*/
				/* 3 - code set 3	(UNsupported)	*/

#if !defined PCVT_NULLCHARS	/* ---------- DEFAULT: ON ------------- */
# define PCVT_NULLCHARS 1	/* allow the keyboard to send null	*/
#elif PCVT_NULLCHARS != 0	/* (0x00) characters to the calling	*/
# undef PCVT_NULLCHARS		/* program. this has the side effect	*/
# define PCVT_NULLCHARS 1	/* that every undefined key also sends	*/
#endif				/* out nulls.				*/

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

#if !defined PCVT_SLOW_INTERRUPT/* ---------- DEFAULT: OFF ------------ */
# define PCVT_SLOW_INTERRUPT 0	/* If off, protecting critical regions	*/
#elif PCVT_SLOW_INTERRUPT != 0	/* in the keyboard fifo code is done by	*/
# undef PCVT_SLOW_INTERRUPT	/* disabling the processor irq's, if on */
# define PCVT_SLOW_INTERRUPT 1	/* this is done by spl()/splx() calls.  */
#endif

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
