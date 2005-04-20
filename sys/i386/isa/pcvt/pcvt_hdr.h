/*-
 * Copyright (c) 1999, 2000 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore.
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
 *	pcvt_hdr.h	VT220 Driver Global Include File
 *	------------------------------------------------
 *
 *	Last Edit-Date: [Wed Apr  5 18:21:32 2000]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#define	PCVT_REL "3.60"		/* driver attach announcement	*/
				/* see also: pcvt_ioctl.h	*/

#include "opt_pcvt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/tty.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/pcvt_ioctl.h>
#include <machine/pc/display.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/stdarg.h>

#include <dev/kbd/kbdreg.h>
#include <dev/kbd/atkbdcreg.h>

#include <i386/isa/pcvt/pcvt_conf.h>

/*===========================================================================*
 *	definitions
 *===========================================================================*/

#define PCVT_KBD_FIFO_SZ	256	/* keyboard fifo size */
#define PCVT_PCBURST		256	/* # of burst out chars */
#define SCROLLBACK_PAGES	8	/* scrollback buffer pages */
#define SCROLLBACK_COOKIE	31337	/* scrollback buffer pages */
#define PCVT_NONRESP_KEYB_TRY	25	/* no of times to try to detect	*/
					/* a nonresponding keyboard	*/

/*
 * The following values are defined in sys/kbio.h, but the header
 * file is not included here due to conflicts with pcvt_ioctl.h.
 * XXX: Still relevant ?
 */
#define KDGKBTYPE	_IOR('K', 64, int)
#define KB_84		1
#define KB_101		2
#define KB_OTHER	3

/* setup irq disable function to use */

#if !(PCVT_SLOW_INTERRUPT)
# define PCVT_DISABLE_INTR()	disable_intr()
# define PCVT_ENABLE_INTR()	enable_intr()
# undef PCVT_SLOW_INTERRUPT
#else
# define PCVT_DISABLE_INTR()	s = spltty()
# define PCVT_ENABLE_INTR()	splx(s)
# undef PCVT_SLOW_INTERRUPT
# define PCVT_SLOW_INTERRUPT 1
#endif

#ifdef XSERVER

/* PCVT_NULLCHARS is mandatory for X server */

#undef PCVT_NULLCHARS
#define PCVT_NULLCHARS 1

#endif /* XSERVER */

/* PCVT_SCREENSAVER is mandatory for PCVT_PRETTYSCRNS */

#if PCVT_PRETTYSCRNS && !PCVT_SCREENSAVER
#undef PCVT_SCREENSAVER
#define PCVT_SCREENSAVER 1
#endif

#if PCVT_SCANSET !=1 && PCVT_SCANSET !=2
#error "Supported keyboard scancode sets are 1 and 2 only (for now)!!!"
#endif

/*---------------------------------------------------------------------------*
 *	keyboard
 *---------------------------------------------------------------------------*/

/* responses from the KEYBOARD (via the 8042 controller on mainboard..) */

#define	KEYB_R_OVERRUN0	0x00	/* keyboard buffer overflow */
#define KEYB_R_SELFOK	0xaa	/* keyboard selftest ok after KEYB_C_RESET */
#define KEYB_R_EXT0	0xe0	/* keyboard extended scancode prefix 1 */
#define KEYB_R_EXT1	0xe1	/* keyboard extended scancode prefix 2 */
#define KEYB_R_ECHO	0xee	/* keyboard response to KEYB_C_ECHO */
#define KEYB_R_BREAKPFX	0xf0	/* break code prefix for set 2 and 3 */
#define KEYB_R_ACK	0xfa	/* acknowledge after a command has rx'd */
#define KEYB_R_SELFBAD	0xfc	/*keyboard selftest FAILED after KEYB_C_RESET*/
#define KEYB_R_DIAGBAD	0xfd	/* keyboard self diagnostic failure */
#define KEYB_R_RESEND	0xfe	/* keyboard wants command resent or illegal
				 * command rx'd */
#define	KEYB_R_OVERRUN1	0xff	/* keyboard buffer overflow */

#define KEYB_R_MF2ID1	0xab	/* MF II Keyboard id-byte #1 */
#define KEYB_R_MF2ID2	0x41	/* MF II Keyboard id-byte #2 */
#define KEYB_R_MF2ID2HP	0x83	/* MF II Keyboard id-byte #2 from HP keybd's */

/* internal Keyboard Type */

#define KB_UNKNOWN	0	/* unknown keyboard type */
#define KB_AT		1	/* AT (84 keys) Keyboard */
#define KB_MFII		2	/* MF II (101/102 keys) Keyboard */

/*---------------------------------------------------------------------------*
 *	CMOS ram access to get the "Equipment Byte"
 *---------------------------------------------------------------------------*/

#define RTC_EQUIPMENT	0x14	/* equipment byte in cmos ram	*/
#define EQ_EGAVGA	0	/* reserved (= ega/vga)		*/
#define EQ_40COLOR	1	/* display = 40 col color	*/
#define EQ_80COLOR	2	/* display = 80 col color	*/
#define EQ_80MONO	3	/* display = 80 col mono	*/

/*---------------------------------------------------------------------------*
 *	VT220 -> internal color conversion table fields
 *---------------------------------------------------------------------------*/

#define VT_NORMAL	0x00		/* no attributes at all */
#define VT_BOLD		0x01		/* bold attribute */
#define VT_UNDER	0x02		/* underline attribute */
#define VT_BLINK	0x04		/* blink attribute */
#define VT_INVERSE	0x08		/* inverse attribute */

/*---------------------------------------------------------------------------*
 *	VGA GENERAL/EXTERNAL Registers          (3BA or 3DA and 3CA, 3C2, 3CC)
 *---------------------------------------------------------------------------*/

#define GN_MISCOUTR	0x3CC		/* misc output register read */
#define GN_MISCOUTW	0x3C2		/* misc output register write */
#define GN_INPSTAT0	0x3C2		/* input status 0, r/o */
#define GN_INPSTAT1M	0x3BA		/* input status 1, r/o, mono */
#define GN_INPSTAT1C	0x3DA		/* input status 1, r/o, color */
#define GN_FEATR	0x3CA		/* feature control, read */
#define GN_FEATWM	0x3BA		/* feature control, write, mono */
#define GN_FEATWC	0x3DA		/* feature control, write, color */
#define GN_VSUBSYS	0x3C3		/* video subsystem register r/w */
#define GN_DMCNTLM	0x3B8		/* display mode control, r/w, mono */
#define GN_DMCNTLC	0x3D8		/* display mode control, r/w, color */
#define GN_COLORSEL	0x3D9		/* color select register, w/o */
#define GN_HERCOMPAT	0x3BF		/* Hercules compatibility reg, w/o */

/*---------------------------------------------------------------------------*
 *	VGA CRTC Registers			  (3B4 and 3B5 or 3D4 and 3D5)
 *---------------------------------------------------------------------------*/

#define MONO_BASE	0x3B4		/* crtc index register address mono */
#define CGA_BASE	0x3D4		/* crtc index register address color */

#define	CRTC_ADDR	0x00		/* index register */

#define CRTC_HTOTAL	0x00		/* horizontal total */
#define CRTC_HDISPLE	0x01		/* horizontal display end */
#define CRTC_HBLANKS	0x02		/* horizontal blank start */
#define CRTC_HBLANKE	0x03		/* horizontal blank end */
#define CRTC_HSYNCS	0x04		/* horizontal sync start */
#define CRTC_HSYNCE	0x05		/* horizontal sync end */
#define CRTC_VTOTAL	0x06		/* vertical total */
#define CRTC_OVERFLL	0x07		/* overflow low */
#define CRTC_IROWADDR	0x08		/* inital row address */
#define CRTC_MAXROW	0x09		/* maximum row address */
#define CRTC_CURSTART	0x0A		/* cursor start row address */
#define 	CURSOR_ON_BIT 0x20	/* cursor on/off on mda/cga/vga */
#define CRTC_CUREND	0x0B		/* cursor end row address */
#define CRTC_STARTADRH	0x0C		/* linear start address mid */
#define CRTC_STARTADRL	0x0D		/* linear start address low */
#define CRTC_CURSORH	0x0E		/* cursor address mid */
#define CRTC_CURSORL	0x0F		/* cursor address low */
#define CRTC_VSYNCS	0x10		/* vertical sync start */
#define CRTC_VSYNCE	0x11		/* vertical sync end */
#define CRTC_VDE	0x12		/* vertical display end */
#define CRTC_OFFSET	0x13		/* row offset */
#define CRTC_ULOC	0x14		/* underline row address */
#define CRTC_VBSTART	0x15		/* vertical blank start */
#define CRTC_VBEND	0x16		/* vertical blank end */
#define CRTC_MODE	0x17		/* CRTC mode register */
#define CRTC_SPLITL	0x18		/* split screen start low */

/* start of ET4000 extensions */

#define CRTC_RASCAS	0x32		/* ras/cas configuration */
#define CRTC_EXTSTART	0x33		/* extended start address */
#define CRTC_COMPAT6845	0x34		/* 6845 comatibility control */
#define CRTC_OVFLHIGH	0x35		/* overflow high */
#define CRTC_SYSCONF1	0x36		/* video system configuration 1 */
#define CRTC_SYSCONF2	0x36		/* video system configuration 2 */

/* start of WD/Paradise extensions */

#define	CRTC_PR10	0x29		/* r/w unlocking */
#define	CRTC_PR11	0x2A		/* ega switches */
#define	CRTC_PR12	0x2B		/* scratch pad */
#define	CRTC_PR13	0x2C		/* interlace h/2 start */
#define	CRTC_PR14	0x2D		/* interlace h/2 end */
#define	CRTC_PR15	0x2E		/* misc control #1 */
#define	CRTC_PR16	0x2F		/* misc control #2 */
#define	CRTC_PR17	0x30		/* misc control #3 */
					/* 0x31 .. 0x3f reserved */
/* Video 7 */

#define CRTC_V7ID	0x1f		/* identification register */

/* Trident */

#define CRTC_MTEST	0x1e		/* module test register */
#define CRTC_SOFTPROG	0x1f		/* software programming */
#define CRTC_LATCHRDB	0x22		/* latch read back register */
#define CRTC_ATTRSRDB	0x24		/* attribute state read back register*/
#define CRTC_ATTRIRDB	0x26		/* attribute index read back register*/
#define CRTC_HOSTAR	0x27		/* high order start address register */

/*---------------------------------------------------------------------------*
 *	VGA TIMING & SEQUENCER Registers			 (3C4 and 3C5)
 *---------------------------------------------------------------------------*/

#define TS_INDEX	0x3C4		/* index register */
#define TS_DATA		0x3C5		/* data register */

#define TS_SYNCRESET	0x00		/* synchronous reset */
#define TS_MODE		0x01		/* ts mode register */
#define TS_WRPLMASK	0x02		/* write plane mask */
#define TS_FONTSEL	0x03		/* font select register */
#define TS_MEMMODE	0x04		/* memory mode register */

/* ET4000 only */

#define TS_RESERVED	0x05		/* undef, reserved */
#define TS_STATECNTL	0x06		/* state control register */
#define TS_AUXMODE	0x07		/* auxiliary mode control */

/* WD/Paradise only */

#define TS_UNLOCKSEQ	0x06		/* PR20 - unlock sequencer register */
#define TS_DISCFSTAT	0x07		/* PR21 - display config status */
#define TS_MEMFIFOCTL	0x10		/* PR30 - memory i/f & fifo control */
#define TS_SYSIFCNTL	0x11		/* PR31 - system interface control */
#define TS_MISC4	0x12		/* PR32 - misc control #4 */

/* Video 7 */

#define TS_EXTCNTL	0x06		/* extensions control */
#define TS_CLRVDISP	0x30		/* clear vertical display 0x30-0x3f */
#define TS_V7CHIPREV	0x8e		/* chipset revision 0x8e-0x8f */
#define TS_SWBANK	0xe8		/* single/write bank register, rev 5+*/
#define TS_RDBANK	0xe8		/* read bank register, rev 4+ */
#define TS_MISCCNTL	0xe8		/* misc control register, rev 4+ */
#define TS_SWSTROBE	0xea		/* switch strobe */
#define TS_MWRCNTL	0xf3		/* masked write control */
#define TS_MWRMVRAM	0xf4		/* masked write mask VRAM only */
#define TS_BANKSEL	0xf6		/* bank select */
#define TS_SWREADB	0xf7		/* switch readback */
#define TS_PAGESEL	0xf9		/* page select */
#define TS_COMPAT	0xfc		/* compatibility control */
#define TS_16BITCTL	0xff		/* 16 bit interface control */

/* Trident */

#define TS_HWVERS	0x0b		/* hardware version, switch old/new! */
#define TS_CONFPORT1	0x0c		/* config port 1 and 2    - caution! */
#define TS_MODEC2	0x0d		/* old/new mode control 2 - caution! */
#define TS_MODEC1	0x0e		/* old/new mode control 1 - caution! */
#define	TS_PUPM2	0x0f		/* power up mode 2 */

/*---------------------------------------------------------------------------*
 *	VGA GRAPHICS DATA CONTROLLER Registers		    (3CE, 3CF and 3CD)
 *---------------------------------------------------------------------------*/

#define GDC_SEGSEL	0x3CD		/* segment select register */
#define GDC_INDEX	0x3CE		/* index register */
#define GDC_DATA	0x3CF		/* data register */

#define GDC_SETRES	0x00		/* set / reset bits */
#define GDC_ENSETRES	0x01		/* enable set / reset */
#define GDC_COLORCOMP	0x02		/* color compare register */
#define GDC_ROTFUNC	0x03		/* data rotate / function select */
#define GDC_RDPLANESEL	0x04		/* read plane select */
#define GDC_MODE	0x05		/* gdc mode register */
#define GDC_MISC	0x06		/* gdc misc register */
#define GDC_COLORCARE	0x07		/* color care register */
#define GDC_BITMASK	0x08		/* bit mask register */

/* WD/Paradise only */

#define GDC_BANKSWA	0x09		/* PR0A - bank switch a */
#define GDC_BANKSWB	0x0a		/* PR0B - bank switch b */
#define GDC_MEMSIZE	0x0b		/* PR1 memory size */
#define GDC_VIDEOSEL	0x0c		/* PR2 video configuration */
#define GDC_CRTCNTL	0x0d		/* PR3 crt address control */
#define GDC_VIDEOCNTL	0x0e		/* PR4 video control */
#define GDC_PR5GPLOCK	0x0f		/* PR5 gp status and lock */

/* Video 7 */

#define GDC_DATALATCH	0x22		/* gdc data latch */

/*---------------------------------------------------------------------------*
 *	VGA ATTRIBUTE CONTROLLER Registers			 (3C0 and 3C1)
 *---------------------------------------------------------------------------*/

#define ATC_INDEX	0x3C0		/* index register  AND	*/
#define ATC_DATAW	0x3C0		/* data write	   !!!	*/
#define ATC_DATAR	0x3C1		/* data read */

#define ATC_ACCESS	0x20		/* access bit in ATC index register */

#define ATC_PALETTE0	0x00		/* color palette register 0 */
#define ATC_PALETTE1	0x01		/* color palette register 1 */
#define ATC_PALETTE2	0x02		/* color palette register 2 */
#define ATC_PALETTE3	0x03		/* color palette register 3 */
#define ATC_PALETTE4	0x04		/* color palette register 4 */
#define ATC_PALETTE5	0x05		/* color palette register 5 */
#define ATC_PALETTE6	0x06		/* color palette register 6 */
#define ATC_PALETTE7	0x07		/* color palette register 7 */
#define ATC_PALETTE8	0x08		/* color palette register 8 */
#define ATC_PALETTE9	0x09		/* color palette register 9 */
#define ATC_PALETTEA	0x0A		/* color palette register 10 */
#define ATC_PALETTEB	0x0B		/* color palette register 11 */
#define ATC_PALETTEC	0x0C		/* color palette register 12 */
#define ATC_PALETTED	0x0D		/* color palette register 13 */
#define ATC_PALETTEE	0x0E		/* color palette register 14 */
#define ATC_PALETTEF	0x0F		/* color palette register 15 */
#define ATC_MODE	0x10		/* atc mode register */
#define ATC_OVERSCAN	0x11		/* overscan register */
#define ATC_COLPLEN	0x12		/* color plane enable register */
#define ATC_HORPIXPAN	0x13		/* horizontal pixel panning */
#define ATC_COLRESET	0x14		/* color reset */
#define ATC_MISC	0x16		/* misc register (ET3000/ET4000) */

/*---------------------------------------------------------------------------*
 *	VGA palette handling (output DAC palette)
 *---------------------------------------------------------------------------*/

#define VGA_DAC		0x3C6		/* vga dac address */
#define VGA_PMSK	0x3F		/* palette mask, 64 distinct values */
#define NVGAPEL 	256		/* number of palette entries */

/*---------------------------------------------------------------------------*
 *	function key labels
 *---------------------------------------------------------------------------*/

#define LABEL_LEN	9		/* length of one label */
#define LABEL_MID	8		/* mid-part (row/col)	*/

#define LABEL_ROWH	((4*LABEL_LEN)+1)
#define LABEL_ROWL	((4*LABEL_LEN)+2)
#define LABEL_COLU	((4*LABEL_LEN)+4)
#define LABEL_COLH	((4*LABEL_LEN)+5)
#define LABEL_COLL	((4*LABEL_LEN)+6)

/* tab setting */

#define MAXTAB 132		/* no of possible tab stops */

/* escape detection state machine */

#define STATE_INIT	0	/* normal	*/
#define	STATE_ESC	1	/* got ESC	*/
#define STATE_BLANK	2	/* got ESC space*/
#define STATE_HASH	3	/* got ESC #	*/
#define STATE_BROPN	4	/* got ESC (	*/
#define STATE_BRCLO	5	/* got ESC )	*/
#define STATE_CSI	6	/* got ESC [	*/
#define STATE_CSIQM	7	/* got ESC [ ?	*/
#define STATE_AMPSND	8	/* got ESC &	*/
#define STATE_STAR	9	/* got ESC *	*/
#define STATE_PLUS	10	/* got ESC +	*/
#define STATE_DCS	11	/* got ESC P	*/
#define STATE_SCA	12	/* got ESC <Ps> " */
#define STATE_STR	13	/* got ESC !	*/
#define STATE_MINUS	14	/* got ESC -	*/
#define STATE_DOT	15	/* got ESC .	*/
#define STATE_SLASH	16	/* got ESC /	*/

/* for storing escape sequence parameters */

#define MAXPARMS 	10	/* maximum no of parms */

/* terminal responses */

#define DA_VT220	"\033[?62;1;2;6;7;8;9c"

/* sub-states for Device Control String processing */

#define DCS_INIT	0	/* got ESC P ... */
#define DCS_AND_UDK	1	/* got ESC P ... | */
#define DCS_UDK_DEF	2	/* got ESC P ... | fnckey / */
#define DCS_UDK_ESC	3	/* got ESC P ... | fnckey / ... ESC */
#define DCS_DLD_DSCS	4	/* got ESC P ... { */
#define DCS_DLD_DEF	5	/* got ESC P ... { dscs */
#define DCS_DLD_ESC	6	/* got ESC P ... { dscs ... / ... ESC */

/* vt220 user defined keys and vt220 downloadable charset */

#define MAXUDKDEF	300	/* max 256 char + 1 '\0' + space.. */
#define	MAXUDKEYS	18	/* plus holes .. */
#define DSCS_LENGTH	3	/* descriptor length */
#define MAXSIXEL	8	/* sixels forever ! */

/* sub-states for HP-terminal emulator */

#define SHP_INIT	0

/* esc & f family */

#define SHP_AND_F	1
#define SHP_AND_Fa	2
#define SHP_AND_Fak	3
#define SHP_AND_Fak1	4
#define SHP_AND_Fakd	5
#define SHP_AND_FakdL	6
#define SHP_AND_FakdLl	7
#define SHP_AND_FakdLls	8

/* esc & j family */

#define SHP_AND_J	9
#define SHP_AND_JL	10

/* esc & every-thing-else */

#define SHP_AND_ETE	11

/* additionals for function key labels */

#define MAX_LABEL	16
#define MAX_STRING	80
#define MAX_STATUS	160

/* MAX values for screen sizes for possible video adaptors */

#define MAXROW_MDACGA	25		/* MDA/CGA can do 25 x 80 max */
#define MAXCOL_MDACGA	80

#define MAXROW_EGA	43		/* EGA can do 43 x 80 max */
#define MAXCOL_EGA	80

#define MAXROW_VGA	50		/* VGA can do 50 x 80 max */
#define MAXCOL_VGA	80
#define MAXCOL_SVGA	132		/* Super VGA can do 50 x 132 max */

/* switch 80/132 columns */

#define SCR_COL80	80		/* in 80 col mode */
#define SCR_COL132	132		/* in 132 col mode */

#define MAXDECSCA	(((MAXCOL_SVGA * MAXROW_VGA) \
			/ (8 * sizeof(unsigned int)) ) + 1 )

/* screen memory start, monochrome */

#ifndef	MONO_BUF
#define MONO_BUF	(KERNBASE+0xB0000)
#endif

/* screen memory start, color */

#ifndef	CGA_BUF
#define CGA_BUF		(KERNBASE+0xB8000)
#endif

#define	CHR		2		/* bytes per word in screen mem */

#define NVGAFONTS	8		/* number of vga fonts loadable */

#define MAXKEYNUM	127		/* max no of keys in table */

/* charset tables */

#define	CSL	0x0000		/* ega/vga charset, lower half of 512 */
#define	CSH	0x0800		/* ega/vga charset, upper half of 512 */
#define CSSIZE	96		/* (physical) size of a character set */

/* charset designations */

#define D_G0		0	/* designated as G0 */
#define D_G1		1	/* designated as G1 */
#define D_G2		2	/* designated as G2 */
#define D_G3		3	/* designated as G3 */
#define D_G1_96		4	/* designated as G1 for 96-char charsets */
#define D_G2_96		5	/* designated as G2 for 96-char charsets */
#define D_G3_96		6	/* designated as G3 for 96-char charsets */

/* which fkey-labels */

#define SYS_FKL		0	/* in hp mode, sys-fkls are active */
#define USR_FKL		1	/* in hp mode, user-fkls are active */

/* arguments to async_update() */

#define UPDATE_START	((void *)0)	/* do cursor update and requeue */
#define UPDATE_STOP	((void *)1)	/* suspend cursor updates */
#define UPDATE_KERN	((void *)2)	/* do cursor updates for kernel output */


/*===========================================================================*
 *	variables
 *===========================================================================*/

#ifdef MAIN
# define EXTERN	/* actually define variables when included from pcvt_drv.c */
#else
# define EXTERN extern			/* declare them only */
#endif

EXTERN u_char	*more_chars;		/* response buffer via kbd */
EXTERN u_char	color;			/* color or mono display */

EXTERN u_short	kern_attr;		/* kernel messages char attributes */
EXTERN u_short	user_attr;		/* character attributes */

EXTERN struct tty *pcvt_tty[PCVT_NSCREENS];

struct sixels {
	u_char lower[MAXSIXEL];		/* lower half of char */
	u_char upper[MAXSIXEL];		/* upper half of char */
};

struct udkentry {
	u_short	first[MAXUDKEYS];	/* index to first char */
	u_char	length[MAXUDKEYS];	/* length of this entry */
};

/* VGA palette handling */
struct rgb {
	u_char	r, g, b;		/* red/green/blue, valid 0..VGA_PMSK */
};

typedef struct video_state {
	u_short	*Crtat;			/* video page start addr */
	u_short *Memory;		/* malloc'ed memory start address */
	struct tty *vs_tty;		/* pointer to this screen's tty */
	u_char	maxcol;			/* 80 or 132 cols on screen */
	u_char 	row, col;		/* current cursor position */
	u_short	c_attr;			/* current character attributes */
	u_char	vtsgr;			/* current sgr configuration */
	u_short	cur_offset;		/* current cursor position offset */
	u_char	bell_on;		/* flag, bell enabled */
	u_char	sevenbit;		/* flag, data path 7 bits wide */
	u_char	dis_fnc;		/* flag, display functions enable */
	u_char	transparent;		/* flag, mk path tmp trnsprnt for ctls*/
	u_char	scrr_beg;		/* scrolling region, begin */
	u_char	scrr_len;		/* scrolling region, length */
	u_char	scrr_end;		/* scrolling region, end */
	u_char	screen_rows;		/* screen size, length - status lines */
	u_char	screen_rowsize; 	/* screen size, length */
	u_char	vga_charset;		/* VGA character set value */
	u_char	lastchar;		/* flag, vt100 behaviour of last char */
	u_char	lastrow;		/* save row, --------- " -----------  */
	u_char	*report_chars;		/* ptr, status reports from terminal */
	u_char	report_count;		/* count, ----------- " ------------ */
	u_char	state;			/* escape sequence state machine */
	u_char	m_awm;			/* flag, vt100 mode, auto wrap */
	u_char	m_om;			/* flag, vt100 mode, origin mode */
	u_char	sc_flag;		/* flag, vt100 mode,saved parms valid */
	u_char	sc_row;			/* saved row */
	u_char	sc_col;			/* saved col */
	u_short sc_cur_offset;		/* saved cursor addr offset */
	u_short	sc_attr;		/* saved attributes */
	u_char	sc_vtsgr;		/* saved sgr configuration */
	u_char	sc_awm;			/* saved auto wrap mode */
	u_char	sc_om;			/* saved origin mode */
	u_short	*sc_G0;			/* save G0 ptr */
	u_short	*sc_G1;			/* save G1 ptr */
	u_short	*sc_G2;			/* save G2 ptr */
	u_short	*sc_G3;			/* save G3 ptr */
	u_short	**sc_GL;		/* save GL ptr */
	u_short	**sc_GR;		/* save GR ptr */
	u_char	sc_sel;			/* selective erase state */
	u_char	ufkl[8][17];		/* user fkey-labels */
	u_char	sfkl[8][17];		/* system fkey-labels */
	u_char	labels_on;		/* display fkey labels etc. on/off */
	u_char	which_fkl;		/* which fkey labels are active */
	char	tab_stops[MAXTAB]; 	/* table of active tab stops */
	u_char	parmi;			/* parameter index */
	u_char	parms[MAXPARMS];	/* parameter array */
	u_char	hp_state;		/* hp escape sequence state machine */
	u_char	attribute;		/* attribute normal, tx only, local */
	u_char	key;			/* fkey label no */
	u_char	l_len;			/* buffer length's */
	u_char	s_len;
	u_char	m_len;
	u_char	i;			/* help (got short of names ...) */
	u_char	l_buf[MAX_LABEL+1]; 	/* buffers */
	u_char	s_buf[MAX_STRING+1];
	u_char	m_buf[MAX_STATUS+1];
	u_char	openf;			/* we are opened ! */
	u_char	vt_pure_mode;		/* no fkey labels, row/col, status */
	u_char	cursor_start;		/* Start of cursor */
	u_char	cursor_end;		/* End of cursor */
	u_char	cursor_on;		/* cursor switched on */
	u_char	ckm;			/* true = cursor key normal mode */
	u_char	irm;			/* true = insert mode */
	u_char	lnm;			/* Line Feed/New Line Mode */
	u_char	dcs_state;		/* dcs escape sequence state machine */
	u_char	udk_def[MAXUDKDEF]; 	/* new definitions for vt220 FKeys */
	u_short	udk_defi;		/* index for FKey definitions */
	u_char	udk_deflow;		/* low or high nibble in sequence */
	u_char	udk_fnckey;		/* function key to assign to */
	u_char	dld_dscs[DSCS_LENGTH];	/* designate soft character set id */
	u_char	dld_dscsi;		/* index for dscs */
	u_char	dld_sixel_lower;	/* upper/lower sixels of character */
	u_char	dld_sixelli;		/* index for lower sixels */
	u_char	dld_sixelui;		/* index for upper sixels */
	struct sixels sixel;		/* structure for storing char sixels */
	u_char	selchar;		/* true = selective attribute on */
	u_int	decsca[MAXDECSCA];	/* Select Character Attrib bit array */
	u_short **GL;			/* ptr to current GL conversion table*/
	u_short **GR;			/* ptr to current GR conversion table*/
	u_short *G0;			/* ptr to current G0 conversion table*/
	u_short *G1;			/* ptr to current G1 conversion table*/
	u_char force24;			/* force 24 lines in DEC 25 and HP 28*/
	u_short *G2;			/* ptr to current G2 conversion table*/
	u_short *G3;			/* ptr to current G3 conversion table*/
	u_char	dld_id[DSCS_LENGTH+1];	/* soft character set id */
	u_char	which[DSCS_LENGTH+1];	/* which set to designate */
	u_char	whichi;			/* index into which ..	*/
	u_char  ss;			/* flag, single shift G2 / G3 -> GL */
	u_short **Gs;			/* ptr to cur. G2/G3 conversion table*/
	u_char	udkbuf[MAXUDKDEF];	/* buffer for user defined keys */
	struct udkentry ukt;		/* index & length for each udk */
	u_short	udkff;			/* index into buffer first free entry*/
	struct rgb palette[NVGAPEL];	/* saved VGA DAC palette */
	u_char	wd132col;		/* we are on a wd vga and in 132 col */
	u_char	scroll_lock; 		/* scroll lock active */
	u_char	caps_lock;		/* caps lock active */
	u_char	shift_lock;		/* shiftlock flag (virtual ..) */
	u_char	num_lock;		/* num lock, true = keypad num mode */
	u_char	abs_write;		/* write outside of scroll region */

	u_short *Scrollback;            /* scrollback buffer */
	u_short scrollback_pages;	/* size of scrollback buffer */
	u_short scr_offset;             /* current scrollback offset (lines) */
	short scrolling;                /* current scrollback page */
	u_short max_off;                /* maximum scrollback offset */
#ifdef XSERVER
	struct vt_mode smode;
	struct proc *proc;
	pid_t pid;
	unsigned vt_status;
#define	VT_WAIT_REL 1			/* wait till process released vt */
#define VT_WAIT_ACK 2			/* wait till process ack vt acquire */
#define VT_GRAFX    4			/* vt runs graphics mode */
#define VT_WAIT_ACT 8			/* a process is sleeping on this vt */
					/*  becoming active */
#endif /* XSERVER */
} video_state;

EXTERN video_state vs[PCVT_NSCREENS];	/* parameters for screens */

struct vga_char_state {
	int	loaded;			/* Whether a font is loaded here */
	int	secondloaded;		/* an extension characterset was loaded, */
					/*	the number is found here	 */
	u_char	char_scanlines;		/* Scanlines per character */
	u_char	scr_scanlines;		/* Low byte of scanlines per screen */
	int	screen_size;		/* Screen size in SIZ_YYROWS */
};

EXTERN struct vga_char_state vgacs[NVGAFONTS];	/* Character set states */

EXTERN u_short *Crtat;			/* screen start address */

#ifdef MAIN

u_char fgansitopc[] = {			/* foreground ANSI color -> pc */
	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
};

u_char bgansitopc[] = {			/* background ANSI color -> pc */
	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
};

video_state *vsp = &vs[0];		/* ptr to current screen parms */

#ifdef XSERVER
int	vt_switch_pending	= 0; 		/* if > 0, a vt switch is */
#endif /* XSERVER */				/* pending; contains the # */
						/* of the old vt + 1 */

u_int	addr_6845		= MONO_BASE;	/* crtc base addr */
u_char	do_initialization	= 1;		/* we have to init ourselves */
u_char	pcvt_is_console		= 0;		/* until we know it */
u_char 	shift_down 		= 0;		/* shift key down flag */
u_char	ctrl_down		= 0; 		/* ctrl key down flag */
u_char	meta_down		= 0; 		/* alt key down flag */
u_char	altgr_down		= 0; 		/* altgr key down flag */
u_char	kbrepflag		= 1;		/* key repeat flag */
u_char	totalscreens		= 1;		/* screens available */
u_char	current_video_screen	= 0;		/* displayed screen no */
u_char	adaptor_type 		= UNKNOWN_ADAPTOR;/* adaptor type */
u_char 	vga_type 		= VGA_UNKNOWN;	/* vga chipset */
u_char	can_do_132col		= 0;		/* vga chipset can 132 cols */
u_char	vga_family		= 0;		/* vga manufacturer */
u_char	totalfonts		= 0;		/* fonts available */
u_char	chargen_access		= 0;		/* synchronize access */
u_char	keyboard_type		= KB_UNKNOWN;	/* type of keyboard */
u_char	keyboard_is_initialized = 0;		/* for ddb sanity */
u_char	kbd_polling		= 0;		/* keyboard is being polled */
u_char	reset_keyboard		= 0;		/* OK to reset keyboard */
keyboard_t *kbd			= NULL;
struct consdev *pcvt_consptr    = NULL;

#if PCVT_SHOWKEYS
u_char	keyboard_show		= 0;		/* normal display */
#endif /* PCVT_SHOWKEYS */

u_char	cursor_pos_valid	= 0;		/* sput left a valid position*/

u_char	critical_scroll		= 0;		/* inside scrolling up */
int	switch_page		= -1;		/* which page to switch to */

#if PCVT_SCREENSAVER
u_char	reset_screen_saver	= 1;		/* reset the saver next time */
u_char	scrnsv_active		= 0;		/* active flag */
#endif /* PCVT_SCREENSAVER */

#ifdef XSERVER
unsigned scrnsv_timeout		= 0;		/* initially off */
u_char pcvt_kbd_raw		= 0;		/* keyboard sends scans */
#endif /* XSERVER */

u_char *saved_charsets[NVGAFONTS] = {0};	/* backup copy of fonts */

/*---------------------------------------------------------------------------

	VT220 attributes -> internal emulator attributes conversion tables

	be careful when designing color combinations, because on
	EGA and VGA displays, bit 3 of the attribute byte is used
	for characterset switching, and is no longer available for
	foreground intensity (bold)!

---------------------------------------------------------------------------*/

/* color displays */

u_char sgr_tab_color[16] = {
/*00*/  (BG_BLACK     | FG_LIGHTGREY),             /* normal               */
/*01*/  (BG_BLUE      | FG_LIGHTGREY),             /* bold                 */
/*02*/  (BG_BROWN     | FG_LIGHTGREY),             /* underline            */
/*03*/  (BG_MAGENTA   | FG_LIGHTGREY),             /* bold+underline       */
/*04*/  (BG_BLACK     | FG_LIGHTGREY | FG_BLINK),  /* blink                */
/*05*/  (BG_BLUE      | FG_LIGHTGREY | FG_BLINK),  /* bold+blink           */
/*06*/  (BG_BROWN     | FG_LIGHTGREY | FG_BLINK),  /* underline+blink      */
/*07*/  (BG_MAGENTA   | FG_LIGHTGREY | FG_BLINK),  /* bold+underline+blink */
/*08*/  (BG_LIGHTGREY | FG_BLACK),                 /* invers               */
/*09*/  (BG_LIGHTGREY | FG_BLUE),                  /* bold+invers          */
/*10*/  (BG_LIGHTGREY | FG_BROWN),                 /* underline+invers     */
/*11*/  (BG_LIGHTGREY | FG_MAGENTA),               /* bold+underline+invers*/
/*12*/  (BG_LIGHTGREY | FG_BLACK      | FG_BLINK), /* blink+invers         */
/*13*/  (BG_LIGHTGREY | FG_BLUE       | FG_BLINK), /* bold+blink+invers    */
/*14*/  (BG_LIGHTGREY | FG_BROWN      | FG_BLINK), /* underline+blink+invers*/
/*15*/  (BG_LIGHTGREY | FG_MAGENTA    | FG_BLINK)  /*bold+underl+blink+invers*/
};

/* monochrome displays (VGA version, no intensity) */

u_char sgr_tab_mono[16] = {
/*00*/  (BG_BLACK     | FG_LIGHTGREY),            /* normal               */
/*01*/  (BG_BLACK     | FG_UNDERLINE),            /* bold                 */
/*02*/  (BG_BLACK     | FG_UNDERLINE),            /* underline            */
/*03*/  (BG_BLACK     | FG_UNDERLINE),            /* bold+underline       */
/*04*/  (BG_BLACK     | FG_LIGHTGREY | FG_BLINK), /* blink                */
/*05*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK), /* bold+blink           */
/*06*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK), /* underline+blink      */
/*07*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK), /* bold+underline+blink */
/*08*/  (BG_LIGHTGREY | FG_BLACK),                /* invers               */
/*09*/  (BG_LIGHTGREY | FG_BLACK),                /* bold+invers          */
/*10*/  (BG_LIGHTGREY | FG_BLACK),                /* underline+invers     */
/*11*/  (BG_LIGHTGREY | FG_BLACK),                /* bold+underline+invers*/
/*12*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),     /* blink+invers         */
/*13*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),     /* bold+blink+invers    */
/*14*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),     /* underline+blink+invers*/
/*15*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK)      /*bold+underl+blink+invers*/
};

/* monochrome displays (MDA version, with intensity) */

u_char sgr_tab_imono[16] = {
/*00*/  (BG_BLACK     | FG_LIGHTGREY),                /* normal               */
/*01*/  (BG_BLACK     | FG_LIGHTGREY | FG_INTENSE),   /* bold                 */
/*02*/  (BG_BLACK     | FG_UNDERLINE),                /* underline            */
/*03*/  (BG_BLACK     | FG_UNDERLINE | FG_INTENSE),   /* bold+underline       */
/*04*/  (BG_BLACK     | FG_LIGHTGREY | FG_BLINK),     /* blink                */
/*05*/  (BG_BLACK     | FG_LIGHTGREY | FG_INTENSE | FG_BLINK), /* bold+blink  */
/*06*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK),     /* underline+blink      */
/*07*/  (BG_BLACK     | FG_UNDERLINE | FG_BLINK | FG_INTENSE), /* bold+underline+blink */
/*08*/  (BG_LIGHTGREY | FG_BLACK),                    /* invers               */
/*09*/  (BG_LIGHTGREY | FG_BLACK | FG_INTENSE),       /* bold+invers          */
/*10*/  (BG_LIGHTGREY | FG_BLACK),                    /* underline+invers     */
/*11*/  (BG_LIGHTGREY | FG_BLACK | FG_INTENSE),       /* bold+underline+invers*/
/*12*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),         /* blink+invers         */
/*13*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK | FG_INTENSE),/* bold+blink+invers*/
/*14*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK),         /* underline+blink+invers*/
/*15*/  (BG_LIGHTGREY | FG_BLACK | FG_BLINK | FG_INTENSE) /* bold+underl+blink+invers */
};

u_char	pcvt_kbd_fifo[PCVT_KBD_FIFO_SZ];
int	pcvt_kbd_rptr = 0;
int	pcvt_kbd_count= 0;

#else	/* ! MAIN */

extern u_char		pcvt_kbd_fifo[];
extern int		pcvt_kbd_rptr;
extern int		pcvt_kbd_count;

extern u_char		vga_type;
extern video_state	*vsp;

#ifdef XSERVER
extern int		vt_switch_pending;
#endif /* XSERVER */

extern u_int		addr_6845;
extern u_char		do_initialization;
extern u_char		pcvt_is_console;
extern u_char		bgansitopc[];
extern u_char		fgansitopc[];
extern u_char 		shift_down;
extern u_char		ctrl_down;
extern u_char		meta_down;
extern u_char		altgr_down;
extern u_char		kbrepflag;
extern u_char		adaptor_type;
extern u_char		current_video_screen;
extern u_char		totalfonts;
extern u_char		totalscreens;
extern u_char		chargen_access;
extern u_char		keyboard_type;
extern u_char		can_do_132col;
extern u_char		vga_family;
extern u_char		keyboard_is_initialized;
extern u_char		kbd_polling;
extern u_char		reset_keyboard;
extern keyboard_t	*kbd;
extern struct consdev	*pcvt_consptr;

#if PCVT_SHOWKEYS
extern u_char		keyboard_show;
#endif /* PCVT_SHOWKEYS */

extern	u_char		cursor_pos_valid;

extern	u_char		critical_scroll;
extern	int		switch_page;

#if PCVT_SCREENSAVER
extern	u_char		reset_screen_saver;
extern	u_char		scrnsv_active;
#endif /* PCVT_SCREENSAVER */

extern u_char		sgr_tab_color[];
extern u_char		sgr_tab_mono[];
extern u_char		sgr_tab_imono[];

#ifdef XSERVER
extern unsigned		scrnsv_timeout;
extern u_char		pcvt_xmode;
extern u_char		pcvt_kbd_raw;
#endif /* XSERVER */

extern u_char		*saved_charsets[NVGAFONTS];

#endif /* MAIN */

/*===========================================================================*
 *	forward declarations
 *===========================================================================*/

void	async_update ( void *arg );
void	clr_parms ( struct video_state *svsp );
void	cons_highlight ( void );
void	cons_normal ( void );
void	dprintf ( unsigned flgs, const char *fmt, ... );
int	egavga_test ( void );
void	fkl_off ( struct video_state *svsp );
void	fkl_on ( struct video_state *svsp );

#ifdef XSERVER
void	get_usl_keymap( keymap_t *map );
#endif

void	init_sfkl ( struct video_state *svsp );
void	init_ufkl ( struct video_state *svsp );
int	kbdioctl ( struct cdev *dev, int cmd, caddr_t data, int flag );
void	kbd_code_init ( void );
void	kbd_code_init1 ( void );

#if PCVT_SCANSET > 1
void	kbd_emulate_pc(int do_emulation);
#endif

void	loadchar ( int fontset, int character, int char_scanlines, u_char *char_table );
void	mda2egaorvga ( void );
void	pcvt_rint(int unit);

#if PCVT_SCREENSAVER
void 	pcvt_scrnsv_reset ( void );
#ifdef XSERVER
void 	pcvt_set_scrnsv_tmo ( int );
#endif
#endif

void	reallocate_scrollbuffer ( struct video_state *svsp, int pages );

#ifdef XSERVER
void	reset_usl_modes (struct video_state *vsx);
#endif

void	roll_up ( struct video_state *svsp, int n );
void	select_vga_charset ( int vga_charset );
void	set_2ndcharset ( void );
void	set_charset ( struct video_state *svsp, int curvgacs );
void	set_emulation_mode ( struct video_state *svsp, int mode );
void	set_screen_size ( struct video_state *svsp, int size );
u_char  *sgetc ( int noblock );
void	sixel_vga ( struct sixels *charsixel, u_char *charvga );
void	sput ( u_char *s, int attrib, int len, int page );

#ifdef XSERVER
void	switch_screen ( int n, int oldgrafx, int newgrafx );
#endif

void	swritefkl ( int num, u_char *string, struct video_state *svsp );
void	sw_cursor ( int onoff );
void	sw_sfkl ( struct video_state *svsp );
void	sw_ufkl ( struct video_state *svsp );
void	toggl_24l ( struct video_state *svsp );
void	toggl_awm ( struct video_state *svsp );
void	toggl_bell ( struct video_state *svsp );
void	toggl_columns ( struct video_state *svsp );
void	toggl_dspf ( struct video_state *svsp );
void	toggl_sevenbit ( struct video_state *svsp );
void 	update_hp ( struct video_state *svsp );
void	update_led ( void );

#ifdef XSERVER
int	usl_vt_ioctl (struct cdev *dev, int cmd, caddr_t data, int flag, struct thread *);
#endif

void	vga10_vga10 ( u_char *invga, u_char *outvga );
void	vga10_vga14 ( u_char *invga, u_char *outvga );
void	vga10_vga16 ( u_char *invga, u_char *outvga );
void	vga10_vga8 ( u_char *invga, u_char *outvga );
int	vgaioctl ( struct cdev *dev, int cmd, caddr_t data, int flag );

#ifdef XSERVER
int	vgapage ( int n );
#else
void	vgapage ( int n );
#endif

void	vgapaletteio ( unsigned idx, struct rgb *val, int writeit );
char    *vga_string ( int number );
u_char	vga_chipset ( void );
int	vga_col ( struct video_state *svsp, int cols );
void	vga_move_charset ( unsigned n, unsigned char *b, int save_it);
void	vga_screen_off ( void );
void	vga_screen_on ( void );
int	vga_test ( void );

#ifdef XSERVER
int	vt_activate ( int newscreen );
#endif

void	vt_aln ( struct video_state *svsp );
void	vt_clearudk ( struct video_state *svsp );
void	vt_clreol ( struct video_state *svsp );
void	vt_clreos ( struct video_state *svsp );
void	vt_clrtab ( struct video_state *svsp );
int	vt_col ( struct video_state *svsp, int cols );
void	vt_coldmalloc ( void );
void	vt_cub ( struct video_state *svsp );
void	vt_cud ( struct video_state *svsp );
void	vt_cuf ( struct video_state *svsp );
void	vt_curadr ( struct video_state *svsp );
void	vt_cuu ( struct video_state *svsp );
void	vt_da ( struct video_state *svsp );
void	vt_dch ( struct video_state *svsp );
void	vt_dcsentry ( int ch, struct video_state *svsp );
void	vt_designate ( struct video_state *svsp);
void	vt_dl ( struct video_state *svsp );
void	vt_dld ( struct video_state *svsp );
void	vt_dsr ( struct video_state *svsp );
void	vt_ech ( struct video_state *svsp );
void	vt_ic ( struct video_state *svsp );
void	vt_il ( struct video_state *svsp );
void	vt_ind ( struct video_state *svsp );
void	vt_initsel ( struct video_state *svsp );
void	vt_keyappl ( struct video_state *svsp );
void	vt_keynum ( struct video_state *svsp );
void	vt_mc ( struct video_state *svsp );
void	vt_nel ( struct video_state *svsp );
void	vt_rc ( struct video_state *svsp );
void	vt_reqtparm ( struct video_state *svsp );
void	vt_reset_ansi ( struct video_state *svsp );
void	vt_reset_dec_priv_qm ( struct video_state *svsp );
void	vt_ri ( struct video_state *svsp );
void	vt_ris ( struct video_state *svsp );
void	vt_sc ( struct video_state *svsp );
void	vt_sca ( struct video_state *svsp );
void	vt_sd ( struct video_state *svsp );
void	vt_sed ( struct video_state *svsp );
void	vt_sel ( struct video_state *svsp );
void	vt_set_ansi ( struct video_state *svsp );
void	vt_set_dec_priv_qm ( struct video_state *svsp );
void	vt_sgr ( struct video_state *svsp );
void	vt_stbm ( struct video_state *svsp );
void	vt_str ( struct video_state *svsp );
void	vt_su ( struct video_state *svsp );
void	vt_tst ( struct video_state *svsp );
void	vt_udk ( struct video_state *svsp );


#ifdef PCVT_INCLUDE_VT_SELATTR
/*---------------------------------------------------------------------------*
 *	set selective attribute if appropriate
 *---------------------------------------------------------------------------*/
#define INT_BITS	(sizeof(unsigned int) * 8)
#define INT_INDEX(n)	((n) / INT_BITS)
#define BIT_INDEX(n)	((n) % INT_BITS)

static __inline void
vt_selattr(struct video_state *svsp)
{
	int i;

	i = (svsp->Crtat + svsp->cur_offset) - svsp->Crtat;

	if(svsp->selchar)
		svsp->decsca[INT_INDEX(i)] |=  (1 << BIT_INDEX(i));
	else
		svsp->decsca[INT_INDEX(i)] &= ~(1 << BIT_INDEX(i));
}

#endif /* PCVT_INCLUDE_VT_SELATTR */

/*---------------------------------- E O F ----------------------------------*/
