/*-
 * Copyright (c) 1991-1996 Søren Schmidt
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
 *
 *	$Id$
 */

#ifndef	_MACHINE_CONSOLE_H_
#define	_MACHINE_CONSOLE_H_

#ifndef KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#define KDGKBMODE 	_IOR('K', 6, int)
#define KDSKBMODE 	_IO('K', 7 /*, int */)
#define KDMKTONE	_IO('K', 8 /*, int */)
#define KDGETMODE	_IOR('K', 9, int)
#define KDSETMODE	_IO('K', 10 /*, int */)
#define KDSBORDER	_IO('K', 13 /*, int */)
#define KDGKBSTATE	_IOR('K', 19, int)
#define KDSKBSTATE	_IO('K', 20 /*, int */)
#define KDENABIO	_IO('K', 60)
#define KDDISABIO	_IO('K', 61)
#define KIOCSOUND	_IO('K', 63 /*, int */)
#define KDGKBTYPE	_IOR('K', 64, int)
#define KDGETLED	_IOR('K', 65, int)
#define KDSETLED	_IO('K', 66 /*, int */)
#define KDSETRAD	_IO('K', 67 /*, int */)		/* obsolete */
#define KDRASTER	_IOW('K', 100, scr_size_t)
#define KDGKBINFO	_IOR('K', 101, keyboard_info_t)
#define KDSETREPEAT	_IOW('K', 102, keyboard_delay_t)

#define GETFKEY		_IOWR('k', 0, fkeyarg_t)
#define SETFKEY		_IOWR('k', 1, fkeyarg_t)
#define GIO_SCRNMAP	_IOR('k', 2, scrmap_t)
#define PIO_SCRNMAP	_IOW('k', 3, scrmap_t)
#define GIO_KEYMAP 	_IOR('k', 6, keymap_t)
#define PIO_KEYMAP 	_IOW('k', 7, keymap_t)
#define GIO_DEADKEYMAP 	_IOR('k', 8, accentmap_t)
#define PIO_DEADKEYMAP 	_IOW('k', 9, accentmap_t)
#define GIO_KEYMAPENT 	_IOWR('k', 10, keyarg_t)
#define PIO_KEYMAPENT 	_IOW('k', 11, keyarg_t)

#define GIO_ATTR	_IOR('a', 0, int)
#define GIO_COLOR	_IOR('c', 0, int)
#define CONS_CURRENT	_IOR('c', 1, int)
#define CONS_GET	_IOR('c', 2, int)
#define CONS_IO		_IO('c', 3)
#define CONS_BLANKTIME	_IOW('c', 4, int)
#define CONS_SSAVER	_IOW('c', 5, ssaver_t)
#define CONS_GSAVER	_IOWR('c', 6, ssaver_t)
#define CONS_CURSORTYPE	_IOW('c', 7, int)
#define CONS_BELLTYPE	_IOW('c', 8, int)
#define CONS_HISTORY	_IOW('c', 9, int)
#define CONS_MOUSECTL	_IOWR('c', 10, mouse_info_t)
#define CONS_IDLE	_IOR('c', 11, int)
#define CONS_SAVERMODE	_IOW('c', 12, int)
#define CONS_SAVERSTART	_IOW('c', 13, int)
#define PIO_FONT8x8	_IOW('c', 64, fnt8_t)
#define GIO_FONT8x8	_IOR('c', 65, fnt8_t)
#define PIO_FONT8x14	_IOW('c', 66, fnt14_t)
#define GIO_FONT8x14	_IOR('c', 67, fnt14_t)
#define PIO_FONT8x16	_IOW('c', 68, fnt16_t)
#define GIO_FONT8x16	_IOR('c', 69, fnt16_t)
#define CONS_GETINFO    _IOWR('c', 73, vid_info_t)
#define CONS_GETVERS	_IOR('c', 74, int)
#define CONS_CURRENTADP	_IOR('c', 100, int)
#define CONS_ADPINFO	_IOWR('c', 101, video_adapter_info_t)
#define CONS_MODEINFO	_IOWR('c', 102, video_info_t)
#define CONS_FINDMODE	_IOWR('c', 103, video_info_t)
#define CONS_SETWINORG	_IO('c', 104 /* u_int */) 

#define CONS_SETKBD	_IO('c', 110 /* int */)
#define CONS_RELKBD	_IO('c', 111)

/* CONS_SAVERMODE */
#define CONS_LKM_SAVER	0
#define CONS_USR_SAVER	1

#ifdef PC98
#define ADJUST_CLOCK		_IO('t',100)		/* for 98note resume */
#endif

#define VT_OPENQRY	_IOR('v', 1, int)
#define VT_SETMODE	_IOW('v', 2, vtmode_t)
#define VT_GETMODE	_IOR('v', 3, vtmode_t)
#define VT_RELDISP	_IO('v', 4 /*, int */)
#define VT_ACTIVATE	_IO('v', 5 /*, int */)
#define VT_WAITACTIVE	_IO('v', 6 /*, int */)
#define VT_GETACTIVE	_IOR('v', 7, int)

#define VT_FALSE	0
#define VT_TRUE		1
#define VT_ACKACQ	2

#define VT_AUTO		0		/* switching is automatic 	*/
#define VT_PROCESS	1		/* switching controlled by prog */
#define VT_KERNEL	255		/* switching controlled in kernel */

#ifndef _VT_MODE_DECLARED
#define	_VT_MODE_DECLARED
struct vt_mode {
	char	mode;
	char	waitv;			/* not implemented yet 	SOS	*/
	short	relsig;
	short	acqsig;
	short	frsig;			/* not implemented yet	SOS	*/
};

typedef struct vt_mode vtmode_t;
#endif /* !_VT_MODE_DECLARED */

struct mouse_data {
	int	x;
	int 	y;
	int 	z;
	int 	buttons;
};

struct mouse_mode {
	int	mode;
	int	signal;
};

struct mouse_event {
	int	id;			/* one based */
	int	value;
};

#define MOUSE_SHOW		0x01
#define MOUSE_HIDE		0x02
#define MOUSE_MOVEABS		0x03
#define MOUSE_MOVEREL		0x04
#define MOUSE_GETINFO		0x05
#define MOUSE_MODE		0x06
#define MOUSE_ACTION		0x07
#define MOUSE_MOTION_EVENT	0x08
#define MOUSE_BUTTON_EVENT	0x09

struct mouse_info {
	int	operation;
	union {
		struct mouse_data data;
		struct mouse_mode mode;
		struct mouse_event event;
	}u;
};

#define KD_MONO		1		/* monochrome adapter        	*/
#define KD_HERCULES	2		/* hercules adapter          	*/
#define KD_CGA		3		/* color graphics adapter    	*/
#define KD_EGA		4		/* enhanced graphics adapter 	*/
#define KD_VGA		5		/* video graphics adapter    	*/
#define KD_PC98		6		/* PC-98 display            	*/

#define KD_TEXT		0		/* set text mode restore fonts  */
#define KD_TEXT0	0		/* ditto			*/
#define KD_TEXT1	2		/* set text mode !restore fonts */
#define KD_GRAPHICS	1		/* set graphics mode 		*/
#define KD_PIXEL	3		/* set pixel mode		*/

#define K_RAW		0		/* keyboard returns scancodes	*/
#define K_XLATE		1		/* keyboard returns ascii 	*/
#define K_CODE		2		/* keyboard returns keycodes 	*/

#define KB_84		1		/* 'old' 84 key AT-keyboard	*/
#define KB_101		2		/* MF-101 or MF-102 keyboard	*/
#define KB_OTHER	3		/* keyboard not known 		*/

#define CLKED		1		/* Caps locked			*/
#define NLKED		2		/* Num locked			*/
#define SLKED		4		/* Scroll locked		*/
#define ALKED		8		/* AltGr locked			*/
#define LOCK_MASK	(CLKED | NLKED | SLKED | ALKED)
#define LED_CAP		1		/* Caps lock LED 		*/
#define LED_NUM		2		/* Num lock LED 		*/
#define LED_SCR		4		/* Scroll lock LED 		*/
#define LED_MASK	(LED_CAP | LED_NUM | LED_SCR)

/* possible flag values */
#define	FLAG_LOCK_O	0
#define	FLAG_LOCK_C	1
#define FLAG_LOCK_N	2

#define NUM_KEYS	256		/* number of keys in table	*/
#define NUM_STATES	8		/* states per key		*/
#define ALTGR_OFFSET	128		/* offset for altlock keys	*/

#ifndef _KEYMAP_DECLARED
#define	_KEYMAP_DECLARED
struct keyent_t {
	u_char map[NUM_STATES];
	u_char spcl;
	u_char flgs;
};

struct keymap {
	u_short	n_keys;
	struct keyent_t key[NUM_KEYS];
};

typedef struct keymap keymap_t;

struct keyarg {
	u_short	keynum;
	struct keyent_t key;
};

typedef struct keyarg keyarg_t;
#endif /* !_KEYMAP_DECLARED */

#define NUM_DEADKEYS	15		/* number of accent keys */
#define NUM_ACCENTCHARS	52		/* max number of accent chars */

struct acc_t {
	u_char accchar;
	u_char map[NUM_ACCENTCHARS][2];
};

struct accentmap {
	u_short n_accs;
	struct acc_t acc[NUM_DEADKEYS];
};

#define MAXFK		16
#define NUM_FKEYS	96

struct fkeytab {
	u_char	str[MAXFK];
	u_char	len;
};

struct fkeyarg {
	u_short	keynum;
	char	keydef[MAXFK];
	char	flen;
};

struct colors	{
	char	fore;
	char	back;
};

struct vid_info {
	short	size;
	short	m_num;
	u_short	mv_row, mv_col;
	u_short	mv_rsz, mv_csz;
	struct colors	mv_norm,
			mv_rev,
			mv_grfc;
	u_char	mv_ovscan;
	u_char	mk_keylock;
};

#define MAXSSAVER	16

struct ssaver	{
	char	name[MAXSSAVER];
	int	num;
	long	time;
};

/* video mode information block */
struct video_info {
    int			vi_mode;
    int			vi_flags;
#define V_INFO_COLOR	(1<<0)
#define V_INFO_GRAPHICS	(1<<1)
#define V_INFO_LINEAR	(1<<2)
#define V_INFO_VESA	(1<<3)
    int			vi_width;
    int			vi_height;
    int			vi_cwidth;
    int			vi_cheight;
    int			vi_depth;
    int			vi_planes;
    u_int		vi_window;	/* physical address */
    size_t		vi_window_size;
    size_t		vi_window_gran;
    u_int		vi_buffer;	/* physical address */
    size_t		vi_buffer_size;
    /* XXX pixel format, memory model,... */
};

/* adapter infromation block */
struct video_adapter {
    int			va_index;
    int			va_type;
    char		*va_name;
    int			va_unit;
    int			va_minor;
    int			va_flags;
#define V_ADP_COLOR	(1<<0)
#define V_ADP_MODECHANGE (1<<1)
#define V_ADP_STATESAVE	(1<<2)
#define V_ADP_STATELOAD	(1<<3)
#define V_ADP_FONT	(1<<4)
#define V_ADP_PALETTE	(1<<5)
#define V_ADP_BORDER	(1<<6)
#define V_ADP_VESA	(1<<7)
#define V_ADP_PROBED	(1<<16)
#define V_ADP_INITIALIZED (1<<17)
#define V_ADP_REGISTERED (1<<18)
    int			va_io_base;
    int			va_io_size;
    int			va_crtc_addr;
    int			va_mem_base;
    int			va_mem_size;
    u_int		va_window;	/* virtual address */
    size_t		va_window_size;
    size_t		va_window_gran;
    u_int		va_buffer;	/* virtual address */
    size_t		va_buffer_size;
    int			va_initial_mode;
    int			va_initial_bios_mode;
    int			va_mode;
    struct video_info	va_info;
    int			va_line_width;
    void		*va_token;
};

struct video_adapter_info {
    int			va_index;
    int			va_type;
    char		va_name[16];
    int			va_unit;
    int			va_flags;
    int			va_io_base;
    int			va_io_size;
    int			va_crtc_addr;
    int			va_mem_base;
    int			va_mem_size;
    u_int		va_window;	/* virtual address */
    size_t		va_window_size;
    size_t		va_window_gran;
    u_int		va_buffer;	/* virtual address */
    size_t		va_buffer_size;
    int			va_initial_mode;
    int			va_initial_bios_mode;
    int			va_mode;
    int			va_line_width;
};

#define V_ADP_PRIMARY		0
#define V_ADP_SECONDARY		1

struct keyboard_info {
    int			kb_index;	/* kbdio index# */
    char		kb_name[16];	/* driver name */
    int			kb_unit;	/* unit# */
    int			kb_type;	/* KB_84, KB_101, KB_OTHER,... */
    int			kb_config;	/* device configuration flags */
    int			kb_flags;	/* internal flags */
};

typedef struct accentmap accentmap_t;
typedef struct fkeytab fkeytab_t;
typedef struct fkeyarg fkeyarg_t;
typedef struct vid_info vid_info_t;
typedef struct mouse_info mouse_info_t;
typedef struct {char scrmap[256];} scrmap_t;
typedef struct {char fnt8x8[8*256];} fnt8_t;
typedef struct {char fnt8x14[14*256];} fnt14_t;
typedef struct {char fnt8x16[16*256];} fnt16_t;
typedef struct ssaver ssaver_t;
typedef struct video_adapter video_adapter_t;
typedef struct video_adapter_info video_adapter_info_t;
typedef struct video_info video_info_t;
typedef struct keyboard_info keyboard_info_t;
typedef struct {int scr_size[3];} scr_size_t;
typedef struct {int kbd_delay[2];} keyboard_delay_t;

/* defines for "special" keys (spcl bit set in keymap) */
#define NOP		0x00		/* nothing (dead key)		*/
#define LSH		0x02		/* left shift key		*/
#define RSH		0x03		/* right shift key		*/
#define CLK		0x04		/* caps lock key		*/
#define NLK		0x05		/* num lock key			*/
#define SLK		0x06		/* scroll lock key		*/
#define LALT		0x07		/* left alt key			*/
#define BTAB		0x08		/* backwards tab		*/
#define LCTR		0x09		/* left control key		*/
#define NEXT		0x0a		/* switch to next screen 	*/
#define F_SCR		0x0b		/* switch to first screen 	*/
#define L_SCR		0x1a		/* switch to last screen 	*/
#define F_FN		0x1b		/* first function key 		*/
#define L_FN		0x7a		/* last function key 		*/
/*			0x7b-0x7f	   reserved do not use !	*/
#define RCTR		0x80		/* right control key		*/
#define RALT		0x81		/* right alt (altgr) key	*/
#define ALK		0x82		/* alt lock key			*/
#define ASH		0x83		/* alt shift key		*/
#define META		0x84		/* meta key			*/
#define RBT		0x85		/* boot machine			*/
#define DBG		0x86		/* call debugger		*/
#define SUSP		0x87		/* suspend power (APM)		*/
#define SPSC		0x88		/* toggle splash/text screen	*/

#define F_ACC		DGRA		/* first accent key		*/
#define DGRA		0x89		/* grave			*/
#define DACU		0x8a		/* acute			*/
#define DCIR		0x8b		/* circumflex			*/
#define DTIL		0x8c		/* tilde			*/
#define DMAC		0x8d		/* macron			*/
#define DBRE		0x8e		/* breve			*/
#define DDOT		0x8f		/* dot				*/
#define DUML		0x90		/* umlaut/diaresis		*/
#define DDIA		0x90		/* diaresis			*/
#define DSLA		0x91		/* slash			*/
#define DRIN		0x92		/* ring				*/
#define DCED		0x93		/* cedilla			*/
#define DAPO		0x94		/* apostrophe			*/
#define DDAC		0x95		/* double acute			*/
#define DOGO		0x96		/* ogonek			*/
#define DCAR		0x97		/* caron			*/
#define L_ACC		DCAR		/* last accent key		*/

#define STBY		0x98		/* Go into standby mode (apm)   */

#define F(x)		((x)+F_FN-1)
#define	S(x)		((x)+F_SCR-1)
#define ACC(x)		((x)+F_ACC)
#define NOKEY		0x100		/* no key pressed marker 	*/
#define FKEY		0x200		/* function key marker 		*/
#define MKEY		0x400		/* meta key marker (prepend ESC)*/
#define BKEY		0x800		/* backtab (ESC [ Z)		*/

#define SPCLKEY		0x8000		/* special key */
#define RELKEY		0x4000		/* key released */
#define ERRKEY		0x2000		/* error */

#define KEYCHAR(c)	((c) & 0x00ff)
#define KEYFLAGS(c)	((c) & ~0x00ff)

/* video mode definitions */
#define M_B40x25	0	/* black & white 40 columns */
#define M_C40x25	1	/* color 40 columns */
#define M_B80x25	2	/* black & white 80 columns */
#define M_C80x25	3	/* color 80 columns */
#define M_BG320		4	/* black & white graphics 320x200 */
#define M_CG320		5	/* color graphics 320x200 */
#define M_BG640		6	/* black & white graphics 640x200 hi-res */
#define M_EGAMONO80x25  7       /* ega-mono 80x25 */
#define M_CG320_D	13	/* ega mode D */
#define M_CG640_E	14	/* ega mode E */
#define M_EGAMONOAPA	15	/* ega mode F */
#define M_CG640x350	16	/* ega mode 10 */
#define M_ENHMONOAPA2	17	/* ega mode F with extended memory */
#define M_ENH_CG640	18	/* ega mode 10* */
#define M_ENH_B40x25    19      /* ega enhanced black & white 40 columns */
#define M_ENH_C40x25    20      /* ega enhanced color 40 columns */
#define M_ENH_B80x25    21      /* ega enhanced black & white 80 columns */
#define M_ENH_C80x25    22      /* ega enhanced color 80 columns */
#define M_VGA_C40x25	23	/* vga 8x16 font on color */
#define M_VGA_C80x25	24	/* vga 8x16 font on color */
#define M_VGA_M80x25	25	/* vga 8x16 font on mono */

#define M_VGA11		26	/* vga 640x480 2 colors */
#define M_BG640x480	26
#define M_VGA12		27	/* vga 640x480 16 colors */
#define M_CG640x480	27
#define M_VGA13		28	/* vga 640x200 256 colors */
#define M_VGA_CG320	28

#define M_VGA_C80x50	30	/* vga 8x8 font on color */
#define M_VGA_M80x50	31	/* vga 8x8 font on color */
#define M_VGA_C80x30	32	/* vga 8x16 font on color */
#define M_VGA_M80x30	33	/* vga 8x16 font on color */
#define M_VGA_C80x60	34	/* vga 8x8 font on color */
#define M_VGA_M80x60	35	/* vga 8x8 font on color */
#define M_VGA_CG640	36	/* vga 640x400 256 color */
#define M_VGA_MODEX	37	/* vga 320x240 256 color */

#define M_ENH_B80x43	0x70	/* ega black & white 80x43 */
#define M_ENH_C80x43	0x71	/* ega color 80x43 */

#define M_PC98_80x25	98	/* PC98 80x25 */
#define M_PC98_80x30	99	/* PC98 80x30 */

#define M_HGC_P0	0xe0	/* hercules graphics - page 0 @ B0000 */
#define M_HGC_P1	0xe1	/* hercules graphics - page 1 @ B8000 */
#define M_MCA_MODE	0xff	/* monochrome adapter mode */

#define M_TEXT_80x25	200	/* generic text modes */
#define M_TEXT_80x30	201
#define M_TEXT_80x43	202
#define M_TEXT_80x50	203
#define M_TEXT_80x60	204
#define M_TEXT_132x25	205
#define M_TEXT_132x30	206
#define M_TEXT_132x43	207
#define M_TEXT_132x50	208
#define M_TEXT_132x60	209

#define SW_PC98_80x25	_IO('S', M_PC98_80x25)
#define SW_PC98_80x30	_IO('S', M_PC98_80x30)
#define SW_B40x25 	_IO('S', M_B40x25)
#define SW_C40x25  	_IO('S', M_C40x25)
#define SW_B80x25  	_IO('S', M_B80x25)
#define SW_C80x25  	_IO('S', M_C80x25)
#define SW_BG320   	_IO('S', M_BG320)
#define SW_CG320   	_IO('S', M_CG320)
#define SW_BG640   	_IO('S', M_BG640)
#define SW_EGAMONO80x25 _IO('S', M_EGAMONO80x25)
#define SW_CG320_D    	_IO('S', M_CG320_D)
#define SW_CG640_E    	_IO('S', M_CG640_E)
#define SW_EGAMONOAPA 	_IO('S', M_EGAMONOAPA)
#define SW_CG640x350  	_IO('S', M_CG640x350)
#define SW_ENH_MONOAPA2 _IO('S', M_ENHMONOAPA2)
#define SW_ENH_CG640  	_IO('S', M_ENH_CG640)
#define SW_ENH_B40x25  	_IO('S', M_ENH_B40x25)
#define SW_ENH_C40x25  	_IO('S', M_ENH_C40x25)
#define SW_ENH_B80x25  	_IO('S', M_ENH_B80x25)
#define SW_ENH_C80x25  	_IO('S', M_ENH_C80x25)
#define SW_ENH_B80x43  	_IO('S', M_ENH_B80x43)
#define SW_ENH_C80x43  	_IO('S', M_ENH_C80x43)
#define SW_MCAMODE    	_IO('S', M_MCA_MODE)
#define SW_VGA_C40x25	_IO('S', M_VGA_C40x25)
#define SW_VGA_C80x25	_IO('S', M_VGA_C80x25)
#define SW_VGA_C80x30	_IO('S', M_VGA_C80x30)
#define SW_VGA_C80x50	_IO('S', M_VGA_C80x50)
#define SW_VGA_C80x60	_IO('S', M_VGA_C80x60)
#define SW_VGA_M80x25	_IO('S', M_VGA_M80x25)
#define SW_VGA_M80x30	_IO('S', M_VGA_M80x30)
#define SW_VGA_M80x50	_IO('S', M_VGA_M80x50)
#define SW_VGA_M80x60	_IO('S', M_VGA_M80x60)
#define SW_VGA11	_IO('S', M_VGA11)
#define SW_BG640x480	_IO('S', M_VGA11)
#define SW_VGA12	_IO('S', M_VGA12)
#define SW_CG640x480	_IO('S', M_VGA12)
#define SW_VGA13	_IO('S', M_VGA13)
#define SW_VGA_CG320	_IO('S', M_VGA13)
#define SW_VGA_CG640	_IO('S', M_VGA_CG640)
#define SW_VGA_MODEX	_IO('S', M_VGA_MODEX)

#define SW_TEXT_80x25	_IO('S', M_TEXT_80x25)
#define SW_TEXT_80x30	_IO('S', M_TEXT_80x30)
#define SW_TEXT_80x43	_IO('S', M_TEXT_80x43)
#define SW_TEXT_80x50	_IO('S', M_TEXT_80x50)
#define SW_TEXT_80x60	_IO('S', M_TEXT_80x60)
#define SW_TEXT_132x25	_IO('S', M_TEXT_132x25)
#define SW_TEXT_132x30	_IO('S', M_TEXT_132x30)
#define SW_TEXT_132x43	_IO('S', M_TEXT_132x43)
#define SW_TEXT_132x50	_IO('S', M_TEXT_132x50)
#define SW_TEXT_132x60	_IO('S', M_TEXT_132x60)

#define M_VESA_BASE		0x100	/* VESA mode number base */

#define M_VESA_CG640x400	0x100	/* 640x400, 256 color */
#define M_VESA_CG640x480	0x101	/* 640x480, 256 color */
#define M_VESA_800x600		0x102	/* 800x600, 16 color */
#define M_VESA_CG800x600	0x103	/* 800x600, 256 color */
#define M_VESA_1024x768		0x104	/* 1024x768, 16 color */
#define M_VESA_CG1024x768	0x105	/* 1024x768, 256 color */
#define M_VESA_1280x1024	0x106	/* 1280x1024, 16 color */
#define M_VESA_CG1280x1024	0x107	/* 1280x1024, 256 color */
#define M_VESA_C80x60		0x108	/* 8x8 font */
#define M_VESA_C132x25		0x109	/* 8x16 font */
#define M_VESA_C132x43		0x10a	/* 8x14 font */
#define M_VESA_C132x50		0x10b	/* 8x8 font */
#define M_VESA_C132x60		0x10c	/* 8x8 font */
#define M_VESA_32K_320		0x10d	/* 320x200, 5:5:5 */
#define M_VESA_64K_320		0x10e	/* 320x200, 5:6:5 */
#define M_VESA_FULL_320		0x10f	/* 320x200, 8:8:8 */
#define M_VESA_32K_640		0x110	/* 640x480, 5:5:5 */
#define M_VESA_64K_640		0x111	/* 640x480, 5:6:5 */
#define M_VESA_FULL_640		0x112	/* 640x480, 8:8:8 */
#define M_VESA_32K_800		0x113	/* 800x600, 5:5:5 */
#define M_VESA_64K_800		0x114	/* 800x600, 5:6:5 */
#define M_VESA_FULL_800		0x115	/* 800x600, 8:8:8 */
#define M_VESA_32K_1024		0x116	/* 1024x768, 5:5:5 */
#define M_VESA_64K_1024		0x117	/* 1024x768, 5:6:5 */
#define M_VESA_FULL_1024	0x118	/* 1024x768, 8:8:8 */
#define M_VESA_32K_1280		0x119	/* 1280x1024, 5:5:5 */
#define M_VESA_64K_1280		0x11a	/* 1280x1024, 5:6:5 */
#define M_VESA_FULL_1280	0x11b	/* 1280x1024, 8:8:8 */
#define M_VESA_MODE_MAX		0x1ff

#define SW_VESA_CG640x400	_IO('V', M_VESA_CG640x400 - M_VESA_BASE)
#define SW_VESA_CG640x480	_IO('V', M_VESA_CG640x480 - M_VESA_BASE)
#define SW_VESA_800x600		_IO('V', M_VESA_800x600 - M_VESA_BASE)
#define SW_VESA_CG800x600	_IO('V', M_VESA_CG800x600 - M_VESA_BASE)
#define SW_VESA_1024x768	_IO('V', M_VESA_1024x768 - M_VESA_BASE)
#define SW_VESA_CG1024x768	_IO('V', M_VESA_CG1024x768 - M_VESA_BASE)
#define SW_VESA_1280x1024	_IO('V', M_VESA_1280x1024 - M_VESA_BASE)
#define SW_VESA_CG1280x1024	_IO('V', M_VESA_CG1280x1024 - M_VESA_BASE)
#define SW_VESA_C80x60		_IO('V', M_VESA_C80x60 - M_VESA_BASE)
#define SW_VESA_C132x25		_IO('V', M_VESA_C132x25 - M_VESA_BASE)
#define SW_VESA_C132x43		_IO('V', M_VESA_C132x43 - M_VESA_BASE)
#define SW_VESA_C132x50		_IO('V', M_VESA_C132x50 - M_VESA_BASE)
#define SW_VESA_C132x60		_IO('V', M_VESA_C132x60 - M_VESA_BASE)
#define SW_VESA_32K_320		_IO('V', M_VESA_32K_320 - M_VESA_BASE)
#define SW_VESA_64K_320		_IO('V', M_VESA_64K_320 - M_VESA_BASE)
#define SW_VESA_FULL_320	_IO('V', M_VESA_FULL_320 - M_VESA_BASE)
#define SW_VESA_32K_640		_IO('V', M_VESA_32K_640 - M_VESA_BASE)
#define SW_VESA_64K_640		_IO('V', M_VESA_64K_640 - M_VESA_BASE)
#define SW_VESA_FULL_640	_IO('V', M_VESA_FULL_640 - M_VESA_BASE)
#define SW_VESA_32K_800		_IO('V', M_VESA_32K_800 - M_VESA_BASE)
#define SW_VESA_64K_800		_IO('V', M_VESA_64K_800 - M_VESA_BASE)
#define SW_VESA_FULL_800	_IO('V', M_VESA_FULL_800 - M_VESA_BASE)
#define SW_VESA_32K_1024	_IO('V', M_VESA_32K_1024 - M_VESA_BASE)
#define SW_VESA_64K_1024	_IO('V', M_VESA_64K_1024 - M_VESA_BASE)
#define SW_VESA_FULL_1024	_IO('V', M_VESA_FULL_1024 - M_VESA_BASE)
#define SW_VESA_32K_1280	_IO('V', M_VESA_32K_1280 - M_VESA_BASE)
#define SW_VESA_64K_1280	_IO('V', M_VESA_64K_1280 - M_VESA_BASE)
#define SW_VESA_FULL_1280	_IO('V', M_VESA_FULL_1280 - M_VESA_BASE)

#endif /* !_MACHINE_CONSOLE_H_ */

