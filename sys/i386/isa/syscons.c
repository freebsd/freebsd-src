/*-
 * Copyright (c) 1992-1995 Søren Schmidt
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *
 *	$Id: syscons.c,v 1.91 1995/01/13 03:19:22 ache Exp $
 */

#include "sc.h"

#if NSC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/devconf.h>

#include <machine/clock.h>
#include <machine/console.h>
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/pc/display.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/timerreg.h>
#include <i386/isa/kbdtables.h>
#include <i386/i386/cons.h>

#if !defined(MAXCONS)
#define MAXCONS 12
#endif

/* this may break on older VGA's but is usefull on real 32 bit systems */
#define	bcopyw	bcopy		

#if defined(HARDFONTS)
#include <i386/isa/iso8859.font>
#endif

/* vm things */
#define	ISMAPPED(pa, width) \
	(((pa) <= (u_long)0x1000 - (width)) \
	 || ((pa) >= 0xa0000 && (pa) <= 0x100000 - (width)))
#define	pa_to_va(pa)	(KERNBASE + (pa))	/* works if ISMAPPED(pa...) */

/* status flags */
#define LOCK_KEY_MASK	0x0000F
#define LED_MASK	0x00007
#define UNKNOWN_MODE	0x00010
#define KBD_RAW_MODE	0x00020
#define SWITCH_WAIT_REL	0x00040
#define SWITCH_WAIT_ACQ	0x00080
#define BUFFER_SAVED	0x00100

/* configuration flags */
#define BLINK_CURSOR	0x00001
#define VISUAL_BELL	0x00002

/* video hardware memory addresses */
#define VIDEOMEM	0x000A0000

/* misc defines */
#define MAX_ESC_PAR 	5
#define	LOAD		1
#define SAVE		0
#define	COL		80
#define	ROW		25
#define BELL_DURATION	5
#define BELL_PITCH	800
#define TIMER_FREQ	1193182			/* should be in isa.h */
#define CONSOLE_BUFSIZE 1024
#define PCBURST		128
#define FONT_8_LOADED	0x001
#define FONT_14_LOADED	0x002
#define FONT_16_LOADED	0x004
#define HISTORY_SIZE	100*80

/* defines related to hardware addresses */
#define	MONO_BASE	0x3B4			/* crt controller base mono */
#define	COLOR_BASE	0x3D4			/* crt controller base color */
#define MISC		0x3C2			/* misc output register */
#define ATC		IO_VGA+0x00		/* attribute controller */
#define TSIDX		IO_VGA+0x04		/* timing sequencer idx */
#define TSREG		IO_VGA+0x05		/* timing sequencer data */
#define PIXMASK		IO_VGA+0x06		/* pixel write mask */
#define PALRADR		IO_VGA+0x07		/* palette read address */
#define PALWADR		IO_VGA+0x08		/* palette write address */
#define PALDATA		IO_VGA+0x09		/* palette data register */
#define GDCIDX		IO_VGA+0x0E		/* graph data controller idx */
#define GDCREG		IO_VGA+0x0F		/* graph data controller data */

/* special characters */
#define cntlc	0x03	
#define cntld	0x04
#define bs	0x08
#define lf	0x0a
#define cr	0x0d	
#define del	0x7f	

typedef struct term_stat {
	int 		esc;			/* processing escape sequence */
	int 		num_param;		/* # of parameters to ESC */
	int	 	last_param;		/* last parameter # */
	int 		param[MAX_ESC_PAR];	/* contains ESC parameters */
	int 		cur_attr;		/* current attributes */
	int 		std_attr;		/* normal attributes */
	int 		rev_attr;		/* reverse attributes */
} term_stat;

typedef struct scr_stat {
	u_short 	*crt_base;		/* address of screen memory */
	u_short 	*scr_buf;		/* buffer when off screen */
	u_short 	*crtat;			/* cursor address */
	int 		xpos;			/* current X position */
	int 		ypos;			/* current Y position */
	int 		xsize;			/* X size */
	int 		ysize;			/* Y size */
	term_stat 	term;			/* terminal emulation stuff */
	char		cursor_start;		/* cursor start line # */
	char		cursor_end;		/* cursor end line # */
	char		cursor_protect;		/* protect cursor */
	u_short		cursor_saveunder;	/* saved char under cursor */
	u_char		border;			/* border color */
	u_short		bell_duration;
	u_short		bell_pitch;
	u_short 	status;			/* status (bitfield) */
	u_short 	mode;			/* mode */
	pid_t 		pid;			/* pid of controlling proc */
	struct proc 	*proc;			/* proc* of controlling proc */
	struct vt_mode 	smode;			/* switch mode */
	u_short		history[HISTORY_SIZE];
	u_short		*history_head;
	u_short		*history_pos;
} scr_stat;

typedef struct default_attr {
	int             std_attr;               /* normal attributes */
	int 		rev_attr;		/* reverse attributes */
} default_attr;

static default_attr user_default = {
	(FG_LIGHTGREY | BG_BLACK) << 8,
	(FG_BLACK | BG_LIGHTGREY) << 8
};

static default_attr kernel_default = {
	(FG_WHITE | BG_BLACK) << 8,
	(FG_BLACK | BG_LIGHTGREY) << 8
};

static	scr_stat	main_console;
static	scr_stat	*console[MAXCONS];
static	scr_stat	*cur_console;
static	scr_stat	*new_scp, *old_scp;
static	term_stat	kernel_console; 
static	default_attr	*current_default;
/* SOS
static	int 		console_buffer_count;
static	char 		console_buffer[CONSOLE_BUFSIZE];
*/
static	char		switch_in_progress = 0;
static	char		blink_in_progress = 0;
static	char 		write_in_progress = 0;
static	char	 	polling = 0;
static 	u_short	 	*crtat = 0;
static	u_int		crtc_addr = MONO_BASE;
static	char		crtc_vga = 0;
static 	u_char		shfts = 0, ctls = 0, alts = 0, agrs = 0, metas = 0;
static 	u_char		nlkcnt = 0, clkcnt = 0, slkcnt = 0, alkcnt = 0;
static	char		*font_8 = NULL, *font_14 = NULL, *font_16 = NULL;
static  int		fonts_loaded = 0;
static	char		palette[3*256];
static 	const u_int 	n_fkey_tab = sizeof(fkey_tab) / sizeof(*fkey_tab);
#if ASYNCH
static  u_char		kbd_reply = 0;
#endif
static	int	 	delayed_next_scr;
#if 0 /* SOS */
static	char		saved_console = -1;	/* saved console number	*/
#endif
static	int		configuration = 0;	/* current setup */
static	long		scrn_blank_time = 0;	/* screen saver timeout value */
static	int		scrn_blanked = 0;	/* screen saver active flag */
static	int		scrn_saver = 0;		/* screen saver routine */
static	long 		scrn_time_stamp;
static  u_char		scr_map[256];
static	char 		*video_mode_ptr = NULL;

/* function prototypes */
int pcprobe(struct isa_device *dev);
int pcattach(struct isa_device *dev);
int pcopen(dev_t dev, int flag, int mode, struct proc *p);
int pcclose(dev_t dev, int flag, int mode, struct proc *p);
int pcread(dev_t dev, struct uio *uio, int flag);
int pcwrite(dev_t dev, struct uio *uio, int flag);
int pcparam(struct tty *tp, struct termios *t);
int pcioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p);
void pcxint(dev_t dev);
void pcstart(struct tty *tp);
void pccnprobe(struct consdev *cp);
void pccninit(struct consdev *cp);
void pccnputc(dev_t dev, char c);
int pccngetc(dev_t dev);
int pccncheckc(dev_t dev);
void scintr(int unit);
int pcmmap(dev_t dev, int offset, int nprot);
int getchar(void);
static void scinit(void);
static void scput(u_char c);
static u_int scgetc(int noblock);
static struct tty *get_tty_ptr(dev_t dev);
static scr_stat *get_scr_stat(dev_t dev);
static scr_stat *alloc_scp();
static void init_scp(scr_stat *scp);
static int get_scr_num();
static void cursor_shape(int start, int end);
static void get_cursor_shape(int *start, int *end);
static void scrn_timer();
static void clear_screen(scr_stat *scp);
static int switch_scr(scr_stat *scp, u_int next_scr);
static void exchange_scr(void);
static void move_crsr(scr_stat *scp, int x, int y);
static void scan_esc(scr_stat *scp, u_char c);
static void undraw_cursor(scr_stat *scp);
static void draw_cursor(scr_stat *scp);
static void ansi_put(scr_stat *scp, u_char c);
static u_char *get_fstr(u_int c, u_int *len);
static void update_leds(int which);
static void kbd_wait(void);
static void kbd_cmd(u_char command);
static void set_mode(scr_stat *scp);
static void set_border(int color);
static void set_vgaregs(char *modetable);
static void copy_font(int direction, int segment, int size, char* font);
static void save_palette(void);
static void load_palette(void);
static void do_bell(scr_stat *scp, int pitch, int duration);
static void blink_screen(scr_stat *scp);

/* available screen savers */
static void none_saver(int test);
static void blank_saver(int test);
static void fade_saver(int test);
static void star_saver(int test);
static void snake_saver(int test);
static void green_saver(int test);

static const struct {
	char	*name;
	void	(*routine)();
} screen_savers[] = {
	{ "none",	none_saver },	/* 0 */
	{ "blank",	blank_saver },	/* 1 */
	{ "fade",	fade_saver },	/* 2 */
	{ "star",	star_saver },	/* 3 */
	{ "snake",	snake_saver },	/* 4 */
	{ "green",	green_saver },	/* 5 */
};
#define SCRN_SAVER(arg)	(*screen_savers[scrn_saver].routine)(arg)
#define NUM_SCRN_SAVERS	(sizeof(screen_savers) / sizeof(screen_savers[0]))

/* OS specific stuff */
#if 0
#define VIRTUAL_TTY(x)	(pccons[x] = ttymalloc(pccons[x]))
struct	tty 		*pccons[MAXCONS];
#else
#define VIRTUAL_TTY(x)	&pccons[x]
struct	tty 		pccons[MAXCONS];
int			npccons	= MAXCONS;
#endif
#define	MONO_BUF	pa_to_va(0xB0000)
#define	CGA_BUF		pa_to_va(0xB8000)
u_short			*Crtat = (u_short *)MONO_BUF;
/*void 	consinit(void) 	{scinit();} SOS */

struct	isa_driver scdriver = {
	pcprobe, pcattach, "sc", 1
};

int
pcprobe(struct isa_device *dev)
{
	int i, retries = 5;
	unsigned char val;

	/* Enable interrupts and keyboard controller */
	kbd_wait();
	outb(KB_STAT, KB_WRITE);
	kbd_wait();
	outb(KB_DATA, KB_MODE);

	/* flush any noise in the buffer */
	while (inb(KB_STAT) & KB_BUF_FULL) {
		DELAY(10);
		(void) inb(KB_DATA);
	}

	/* Reset keyboard hardware */
	while (retries--) {
		kbd_wait();
		outb(KB_DATA, KB_RESET);
		for (i=0; i<100000; i++) {
			DELAY(10);
			val = inb(KB_DATA);
			if (val == KB_ACK || val == KB_ECHO)
				goto gotres;	
			if (val == KB_RESEND)
				break;
		}
	}
gotres:
	if (!retries)
		printf("scprobe: keyboard won't accept RESET command\n");
	else {
gotack:
		DELAY(10);
		while ((inb(KB_STAT) & KB_BUF_FULL) == 0) DELAY(10);
		DELAY(10);
		val = inb(KB_DATA);
		if (val == KB_ACK)
			goto gotack;
		if (val != KB_RESET_DONE) 
			printf("scprobe: keyboard RESET failed %02x\n", val);
	}
#ifdef XT_KEYBOARD
	kbd_wait();
	outb(KB_DATA, 0xF0);
	kbd_wait();
	outb(KB_DATA, 1)
	kbd_wait();
#endif /* XT_KEYBOARD */
	return (IO_KBDSIZE);
}

static struct kern_devconf kdc_sc[NSC] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"sc", 0, { MDDT_ISA, 0, "tty" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNKNOWN,		/* not supported */
	"Graphics console"
} };

static inline void
sc_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_sc[id->id_unit] = kdc_sc[0];
	kdc_sc[id->id_unit].kdc_unit = id->id_unit;
	kdc_sc[id->id_unit].kdc_isa = id;
	dev_attach(&kdc_sc[id->id_unit]);
}


int
pcattach(struct isa_device *dev)
{
	if (crtat == 0)
		scinit();

	configuration = dev->id_flags;
	printf("sc%d: ", dev->id_unit);
	if (crtc_vga)
		if (crtc_addr == MONO_BASE)
			printf("VGA mono");
		else	
			printf("VGA color");
	else
		if (crtc_addr == MONO_BASE)
			printf("MDA/hercules");
		else	
			printf("CGA/EGA");
	if (MAXCONS > 1) 
		printf(" <%d virtual consoles, flags=%x>\n", 
			MAXCONS, configuration);
	else
		printf("\n");
	console[0]->scr_buf = 
		(u_short *)malloc(console[0]->xsize * console[0]->ysize * 
				  sizeof(u_short), M_DEVBUF, M_NOWAIT);
	if (crtc_vga) {
#if defined(HARDFONTS)
		font_8 = font_8x8;
		font_14 = font_8x14;
		font_16 = font_8x16;
		fonts_loaded = FONT_8_LOADED|FONT_14_LOADED|FONT_16_LOADED;
		copy_font(LOAD, 1, 8, font_8);
		copy_font(LOAD, 2, 14, font_14);
		copy_font(LOAD, 0, 16, font_16);
#else
		font_8 = (char *)malloc(8*256, M_DEVBUF, M_NOWAIT);
		font_14 = (char *)malloc(14*256, M_DEVBUF, M_NOWAIT);
		font_16 = (char *)malloc(16*256, M_DEVBUF, M_NOWAIT);
		copy_font(SAVE, 0, 16, font_16);
		fonts_loaded = FONT_16_LOADED;
#endif
		save_palette();
	}
	/* get screensaver going */
	scrn_timer();
	update_leds(console[0]->status);
	sc_registerdev(dev);
	return 0;
}

static struct tty 
*get_tty_ptr(dev_t dev)
{
	int unit = minor(dev);

	if (unit > MAXCONS)
		return(NULL);
	return(VIRTUAL_TTY(unit));
}

static scr_stat 
*get_scr_stat(dev_t dev)
{
	int unit = minor(dev);

	if (unit > MAXCONS)
		return(NULL);
	return(console[unit]);
}

static int
get_scr_num()
{
	int i = 0;

	while ((i < MAXCONS) && (cur_console != console[i])) i++;
	return i < MAXCONS ? i : 0;
}

int 
pcopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return(ENXIO);

	tp->t_oproc = pcstart;
	tp->t_param = pcparam;
	tp->t_dev = dev;
	if (!(tp->t_state & TS_ISOPEN)) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return(EBUSY);
	tp->t_state |= TS_CARR_ON;
	tp->t_cflag |= CLOCAL;
	if (!console[minor(dev)])
		console[minor(dev)] = alloc_scp();
	return((*linesw[tp->t_line].l_open)(dev, tp));
}

int 
pcclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = get_tty_ptr(dev);
	struct scr_stat *scp;

	if (!tp)
		return(ENXIO);
	scp = get_scr_stat(tp->t_dev);
	if (scp->status & SWITCH_WAIT_ACQ)
		wakeup((caddr_t)&scp->smode);
	scp->pid = 0;
	scp->proc = NULL;
	scp->smode.mode = VT_AUTO;
	/* free scp SOS */
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp); 
	return(0);
}

int 
pcread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return(ENXIO);
	return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int 
pcwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return(ENXIO);
	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

void 
scintr(int unit)
{
	static struct tty *cur_tty;
	int c, len;
	u_char *cp;

	/* make screensaver happy */
	scrn_time_stamp = time.tv_sec;
	if (scrn_blanked)
		SCRN_SAVER(0);

	c = scgetc(1);

	cur_tty = VIRTUAL_TTY(get_scr_num());

	if (!(cur_tty->t_state & TS_ISOPEN) || polling)
		return;

	switch (c & 0xff00) {
	case 0x0000: /* normal key */
		(*linesw[cur_tty->t_line].l_rint)(c & 0xFF, cur_tty);
		break;
	case NOKEY:	/* nothing there */
		break;
	case FKEY:	/* function key, return string */
		if (cp = get_fstr((u_int)c, (u_int *)&len)) {
			while (len-- >  0)
				(*linesw[cur_tty->t_line].l_rint)
					(*cp++ & 0xFF, cur_tty);
		}
		break;
	case MKEY:	/* meta is active, prepend ESC */
		(*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
		(*linesw[cur_tty->t_line].l_rint)(c & 0xFF, cur_tty);
		break;
	}	
}

int 
pcparam(struct tty *tp, struct termios *t)
{
	int cflag = t->c_cflag;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;
	return 0;
}

int 
pcioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int i, error;
	struct tty *tp;
	struct trapframe *fp;
	scr_stat *scp; 

	tp = get_tty_ptr(dev);
	if (!tp)
		return ENXIO;
	scp = get_scr_stat(tp->t_dev);

	switch (cmd) {	/* process console hardware related ioctl's */

	case GIO_ATTR:		/* get current attributes */
		*(int*)data = scp->term.cur_attr;
		return 0;

	case GIO_COLOR:		/* is this a color console ? */
		if (crtc_addr == COLOR_BASE)
			*(int*)data = 1;
		else
			*(int*)data = 0;
		return 0;
			
	case CONS_CURRENT:	/* get current adapter type */
		if (crtc_vga)
			*(int*)data = KD_VGA;
		else
			if (crtc_addr == MONO_BASE)
				*(int*)data = KD_MONO;
			else	
				*(int*)data = KD_CGA;
		return 0;

	case CONS_GET:		/* get current video mode */
		*(int*)data = scp->mode;
		return 0;

	case CONS_BLANKTIME:	/* set screen saver timeout (0 = no saver) */
		scrn_blank_time = *(int*)data;
		return 0;

#define	SAVER(p) ((ssaver_t *)(p))
	case CONS_SSAVER:	/* set screen saver */
		if (SAVER(data)->num < 0 
		    || SAVER(data)->num >= NUM_SCRN_SAVERS)
			return EIO;
		SCRN_SAVER(0);
		scrn_saver = SAVER(data)->num;
		scrn_blank_time = SAVER(data)->time;
		return 0;

	case CONS_GSAVER:	/* get screen saver info */
		if (SAVER(data)->num < 0)
			SAVER(data)->num = scrn_saver;
		else if (SAVER(data)->num >= NUM_SCRN_SAVERS)
			return EIO;
		SAVER(data)->time = scrn_blank_time;
		strcpy(SAVER(data)->name, screen_savers[SAVER(data)->num].name);
		return 0;

	case CONS_CURSORTYPE:	/* set cursor type blink/noblink */
		if (*data)
			configuration |= BLINK_CURSOR;
		else
			configuration &= ~BLINK_CURSOR;
		return 0;

	case CONS_BELLTYPE:	/* set bell type sound/visual */
		if (*data)
			configuration |= VISUAL_BELL;
		else
			configuration &= ~VISUAL_BELL;
		return 0;

	case CONS_GETINFO:	/* get current (virtual) console info */
	{
		vid_info_t *ptr = (vid_info_t*)data;
		if (ptr->size == sizeof(struct vid_info)) {
			ptr->m_num = get_scr_num();
			ptr->mv_col = scp->xpos;
			ptr->mv_row = scp->ypos;
			ptr->mv_csz = scp->xsize;
			ptr->mv_rsz = scp->ysize;
			ptr->mv_norm.fore = (scp->term.std_attr & 0x0f00)>>8;
			ptr->mv_norm.back = (scp->term.std_attr & 0xf000)>>12;
			ptr->mv_rev.fore = (scp->term.rev_attr & 0x0f00)>>8;
			ptr->mv_rev.back = (scp->term.rev_attr & 0xf000)>>12;
			ptr->mv_grfc.fore = 0;		/* not supported */
			ptr->mv_grfc.back = 0;		/* not supported */
			ptr->mv_ovscan = scp->border;
			ptr->mk_keylock = scp->status & LOCK_KEY_MASK;
			return 0;
		}
		return EINVAL;
	}

	case CONS_GETVERS:	/* get version number */
		*(int*)data = 0x200;	/* version 2.0 */
		return 0;

        case SW_VGA_C40x25: case SW_VGA_C80x25:	/* VGA TEXT MODES */
        case SW_VGA_M80x25:
        case SW_VGA_C80x30: case SW_VGA_M80x30:
        case SW_VGA_C80x50: case SW_VGA_M80x50:
        case SW_VGA_C80x60: case SW_VGA_M80x60:
        case SW_B40x25:     case SW_C40x25:
        case SW_B80x25:     case SW_C80x25:
        case SW_ENH_B40x25: case SW_ENH_C40x25: 
        case SW_ENH_B80x25: case SW_ENH_C80x25:
        case SW_ENH_B80x43: case SW_ENH_C80x43:

        	if (!crtc_vga || video_mode_ptr == NULL)
        		return ENXIO;
		switch (cmd & 0xff) {
		case M_VGA_C80x60: case M_VGA_M80x60:
			if (!(fonts_loaded & FONT_8_LOADED))
				return EINVAL;
			scp->xsize = 80;
			scp->ysize = 60;
			break;
		case M_VGA_C80x50: case M_VGA_M80x50:
			if (!(fonts_loaded & FONT_8_LOADED))
				return EINVAL;
			scp->xsize = 80;
			scp->ysize = 50;
			break;
        	case M_ENH_B80x43: case M_ENH_C80x43:
			if (!(fonts_loaded & FONT_8_LOADED))
				return EINVAL;
			scp->xsize = 80;
			scp->ysize = 43;
			break;
		case M_VGA_C80x30: case M_VGA_M80x30:
			scp->xsize = 80;
			scp->ysize = 30;
			break;
		default:
			if ((cmd & 0xff) > M_VGA_CG320)
				return EINVAL;
			else 
            		    scp->xsize = *(video_mode_ptr+((cmd&0xff)*64));
            		    scp->ysize = *(video_mode_ptr+((cmd&0xff)*64)+1)+1;
			break;
		}
		scp->mode = cmd & 0xff;
            	scp->status &= ~UNKNOWN_MODE;	/* text mode */
		free(scp->scr_buf, M_DEVBUF); 
		scp->scr_buf = (u_short *)malloc(scp->xsize * scp->ysize * 
				sizeof(u_short), M_DEVBUF, M_NOWAIT);
		if (scp == cur_console)
            		set_mode(scp);
		else
			scp->crt_base = scp->scr_buf;
            	clear_screen(scp);
		if (tp->t_winsize.ws_col != scp->xsize 
		    || tp->t_winsize.ws_row != scp->ysize) {
			tp->t_winsize.ws_col = scp->xsize;
			tp->t_winsize.ws_row = scp->ysize;
			pgsignal(tp->t_pgrp, SIGWINCH, 1);
		}
            	return 0; 

        /* GRAPHICS MODES */
        case SW_BG320:      case SW_CG320:      case SW_BG640:
        case SW_CG320_D:    case SW_CG640_E:
        case SW_CG640x350:  case SW_ENH_CG640:
        case SW_BG640x480:  case SW_CG640x480:  case SW_VGA_CG320:

        	if (!crtc_vga || video_mode_ptr == NULL)
        		return ENXIO;
	    	scp->mode = cmd & 0xFF;
            	scp->status |= UNKNOWN_MODE;	/* graphics mode */
            	scp->xsize = (*(video_mode_ptr + (scp->mode*64))) * 8;
            	scp->ysize = (*(video_mode_ptr + (scp->mode*64) + 1) + 1)
		    	   * (*(video_mode_ptr + (scp->mode*64) + 2));
            	set_mode(scp);
            	/* clear_graphics();*/

		if (tp->t_winsize.ws_xpixel != scp->xsize 
		    || tp->t_winsize.ws_ypixel != scp->ysize) {
			tp->t_winsize.ws_xpixel = scp->xsize;
			tp->t_winsize.ws_ypixel = scp->ysize;
			pgsignal(tp->t_pgrp, SIGWINCH, 1);
		}
            	return 0;

	case VT_SETMODE:	/* set screen switcher mode */
		bcopy(data, &scp->smode, sizeof(struct vt_mode));
		if (scp->smode.mode == VT_PROCESS) {
			scp->proc = p;
			scp->pid = scp->proc->p_pid;
		}
		return 0;
	
	case VT_GETMODE:	/* get screen switcher mode */
		bcopy(&scp->smode, data, sizeof(struct vt_mode));
		return 0;
	
	case VT_RELDISP:	/* screen switcher ioctl */
		switch(*data) {
		case VT_FALSE:	/* user refuses to release screen, abort */
			if (scp == old_scp && (scp->status & SWITCH_WAIT_REL)) {
				old_scp->status &= ~SWITCH_WAIT_REL;
				switch_in_progress = 0;
				return 0;
			}
			return EINVAL;

		case VT_TRUE:	/* user has released screen, go on */
			if (scp == old_scp && (scp->status & SWITCH_WAIT_REL)) {
				scp->status &= ~SWITCH_WAIT_REL;
				exchange_scr();
				if (new_scp->smode.mode == VT_PROCESS) {
					new_scp->status |= SWITCH_WAIT_ACQ;
					psignal(new_scp->proc, 
						new_scp->smode.acqsig);
				}
				else 
					switch_in_progress = 0;
				return 0;
			}
			return EINVAL;

		case VT_ACKACQ:	/* acquire acknowledged, switch completed */
			if (scp == new_scp && (scp->status & SWITCH_WAIT_ACQ)) {
				scp->status &= ~SWITCH_WAIT_ACQ;
				switch_in_progress = 0;
				return 0;
			}
			return EINVAL;

		default:
			return EINVAL;
		}
		/* NOT REACHED */

	case VT_OPENQRY:	/* return free virtual console */
 		for (i = 0; i < MAXCONS; i++) {
			tp = VIRTUAL_TTY(i);
 			if (!(tp->t_state & TS_ISOPEN)) {
 				*data = i + 1;
 				return 0;
 			}
		} 
 		return EINVAL;

	case VT_ACTIVATE:	/* switch to screen *data */
		return switch_scr(scp, (*data) - 1);

	case VT_WAITACTIVE:	/* wait for switch to occur */
		if (*data > MAXCONS || *data < 0) 
			return EINVAL;
		if (minor(dev) == (*data) - 1) 
			return 0;
		if (*data == 0) {
			if (scp == cur_console)
				return 0;
		}
		else 
			scp = console[(*data) - 1];
		while ((error=tsleep((caddr_t)&scp->smode, PZERO|PCATCH, 
			"waitvt", 0)) == ERESTART) ;
		return error;

	case VT_GETACTIVE:
		*data = get_scr_num()+1;
		return 0;

	case KDENABIO:		/* allow io operations */
	 	fp = (struct trapframe *)p->p_md.md_regs;
	 	fp->tf_eflags |= PSL_IOPL;
		return 0; 

	case KDDISABIO:		/* disallow io operations (default) */
	 	fp = (struct trapframe *)p->p_md.md_regs;
	 	fp->tf_eflags &= ~PSL_IOPL;
	 	return 0;

        case KDSETMODE:		/* set current mode of this (virtual) console */
		switch (*data) {
		case KD_TEXT:	/* switch to TEXT (known) mode */
				/* restore fonts & palette ! */
			if (crtc_vga) {
				if (fonts_loaded & FONT_16_LOADED)
					copy_font(LOAD, 0, 16, font_16);
				if (fonts_loaded & FONT_8_LOADED)
					copy_font(LOAD, 1, 8, font_8);
				if (fonts_loaded & FONT_14_LOADED)
					copy_font(LOAD, 2, 14, font_14);
				load_palette();
			}
			/* FALL THROUGH */

		case KD_TEXT1:	/* switch to TEXT (known) mode */
				/* no restore fonts & palette */
			scp->status &= ~UNKNOWN_MODE;
        		if (crtc_vga && video_mode_ptr)
				set_mode(scp);
			clear_screen(scp);
			return 0;

		case KD_GRAPHICS:/* switch to GRAPHICS (unknown) mode */
			scp->status |= UNKNOWN_MODE;
			return 0;
		default:
			return EINVAL;
		}
		/* NOT REACHED */

	case KDGETMODE:		/* get current mode of this (virtual) console */
		*data = (scp->status & UNKNOWN_MODE) ? KD_GRAPHICS : KD_TEXT;
		return 0;

	case KDSBORDER:		/* set border color of this (virtual) console */
		if (!crtc_vga)
			return ENXIO;
		scp->border = *data;
		if (scp == cur_console) 
			set_border(scp->border);
		return 0;

	case KDSKBSTATE:	/* set keyboard state (locks) */
		if (*data >= 0 && *data <= LOCK_KEY_MASK) {
			scp->status &= ~LOCK_KEY_MASK;
			scp->status |= *data;
			if (scp == cur_console) 
				update_leds(scp->status);
			return 0;
		}
		return EINVAL;

	case KDGKBSTATE:	/* get keyboard state (locks) */
		*data = scp->status & LOCK_KEY_MASK;
		return 0;

	case KDSETRAD:		/* set keyboard repeat & delay rates */
		if (*data & 0x80)
			return EINVAL;
		i = spltty();
		kbd_cmd(KB_SETRAD);
		kbd_cmd(*data);
		splx(i);
		return 0;

	case KDSKBMODE:		/* set keyboard mode */
		switch (*data) {
		case K_RAW:	/* switch to RAW scancode mode */
			scp->status |= KBD_RAW_MODE;
			return 0;

		case K_XLATE:	/* switch to XLT ascii mode */
			if (scp == cur_console && scp->status == KBD_RAW_MODE)
				shfts = ctls = alts = agrs = metas = 0;
			scp->status &= ~KBD_RAW_MODE;
			return 0;
		default:
			return EINVAL;
		}
		/* NOT REACHED */

	case KDGKBMODE:		/* get keyboard mode */
		*data = (scp->status & KBD_RAW_MODE) ? K_RAW : K_XLATE;
		return 0;

	case KDMKTONE:		/* sound the bell */
		if (*(int*)data) {
  			do_bell(scp, (*(int*)data)&0xffff,
				(((*(int*)data)>>16)&0xffff)*hz/1000);
		}
		else
			do_bell(scp, scp->bell_pitch, scp->bell_duration);
		return 0;

	case KIOCSOUND:		/* make tone (*data) hz */
		if (scp == cur_console) {
			if (*(int*)data) {
			int pitch = TIMER_FREQ/(*(int*)data);
				/* set command for counter 2, 2 byte write */
				if (acquire_timer2(TIMER_16BIT|TIMER_SQWAVE)) {
					return EBUSY;
				}
				/* set pitch */
				outb(TIMER_CNTR2, pitch);
				outb(TIMER_CNTR2, (pitch>>8));
				/* enable counter 2 output to speaker */
				outb(IO_PPI, inb(IO_PPI) | 3);
			}
			else {
				/* disable counter 2 output to speaker */
				outb(IO_PPI, inb(IO_PPI) & 0xFC);
				release_timer2();
			}
		}
		return 0;

	case KDGKBTYPE:		/* get keyboard type */
		*data = 0;	/* type not known (yet) */
		return 0;

	case KDSETLED:		/* set keyboard LED status */
		if (*data >= 0 && *data <= LED_MASK) {
			scp->status &= ~LED_MASK;
			scp->status |= *data;
			if (scp == cur_console)
				update_leds(scp->status);
			return 0;
		}
		return EINVAL;

	case KDGETLED:		/* get keyboard LED status */
		*data = scp->status & LED_MASK;
		return 0;

	case GETFKEY:		/* get functionkey string */
		if (*(u_short*)data < n_fkey_tab) {
		 	fkeyarg_t *ptr = (fkeyarg_t*)data;
			bcopy(&fkey_tab[ptr->keynum].str,
			      ptr->keydef,
			      fkey_tab[ptr->keynum].len);
			ptr->flen = fkey_tab[ptr->keynum].len;
			return 0;
		}
		else
			return EINVAL;

	case SETFKEY:		/* set functionkey string */
		if (*(u_short*)data < n_fkey_tab) {
		 	fkeyarg_t *ptr = (fkeyarg_t*)data;
			bcopy(ptr->keydef, 
			      &fkey_tab[ptr->keynum].str, 
			      min(ptr->flen, MAXFK));
			fkey_tab[ptr->keynum].len = min(ptr->flen, MAXFK);
			return 0;
		}
		else
			return EINVAL;

	case GIO_SCRNMAP: 	/* get output translation table */
		bcopy(&scr_map, data, sizeof(scr_map));
		return 0;

	case PIO_SCRNMAP:	/* set output translation table */
		bcopy(data, &scr_map, sizeof(scr_map));
		return 0;

	case GIO_KEYMAP: 	/* get keyboard translation table */
		bcopy(&key_map, data, sizeof(key_map));
		return 0;

	case PIO_KEYMAP:	/* set keyboard translation table */
		bcopy(data, &key_map, sizeof(key_map));
		return 0;

	case PIO_FONT8x8:	/* set 8x8 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(data, font_8, 8*256);
		fonts_loaded |= FONT_8_LOADED;
		copy_font(LOAD, 1, 8, font_8);
		return 0;

	case GIO_FONT8x8:	/* get 8x8 dot font */
		if (!crtc_vga)
			return ENXIO;
		if (fonts_loaded & FONT_8_LOADED) {
			bcopy(font_8, data, 8*256);
			return 0;
		}
		else
			return ENXIO;

	case PIO_FONT8x14:	/* set 8x14 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(data, font_14, 14*256);
		fonts_loaded |= FONT_14_LOADED;
		copy_font(LOAD, 2, 14, font_14);
		return 0;

	case GIO_FONT8x14:	/* get 8x14 dot font */
		if (!crtc_vga)
			return ENXIO;
		if (fonts_loaded & FONT_14_LOADED) {
			bcopy(font_14, data, 14*256);
			return 0;
		}
		else
			return ENXIO;

	case PIO_FONT8x16:	/* set 8x16 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(data, font_16, 16*256);
		fonts_loaded |= FONT_16_LOADED;
		copy_font(LOAD, 0, 16, font_16);
		return 0;

	case GIO_FONT8x16:	/* get 8x16 dot font */
		if (!crtc_vga)
			return ENXIO;
		if (fonts_loaded & FONT_16_LOADED) {
			bcopy(font_16, data, 16*256);
			return 0;
		}
		else
			return ENXIO;
#if 0	/* this should really go away !! */
	case CONSOLE_X_MODE_ON:	/* just to be compatible */
		if (saved_console < 0) {
			saved_console = get_scr_num();
			switch_scr(scp, minor(dev));
	 		fp = (struct trapframe *)p->p_md.md_regs;
	 		fp->tf_eflags |= PSL_IOPL;
			scp->status |= UNKNOWN_MODE;
			scp->status |= KBD_RAW_MODE;
			return 0;
		}
		return EAGAIN;

	case CONSOLE_X_MODE_OFF:/* just to be compatible */
	 	fp = (struct trapframe *)p->p_md.md_regs;
	 	fp->tf_eflags &= ~PSL_IOPL;
		if (crtc_vga) {
			if (fonts_loaded & FONT_16_LOADED)
				copy_font(LOAD, 0, 16, font_16);
			if (fonts_loaded & FONT_8_LOADED)
				copy_font(LOAD, 1, 8, font_8);
			if (fonts_loaded & FONT_14_LOADED)
				copy_font(LOAD, 2, 14, font_14);
			load_palette();
			set_mode(scp);
		}
		scp->status &= ~UNKNOWN_MODE;
		clear_screen(scp);
		scp->status &= ~KBD_RAW_MODE;
		switch_scr(scp, saved_console);
		saved_console = -1;
		return 0;

	 case CONSOLE_X_BELL:	/* more compatibility */
                /*
                 * if set, data is a pointer to a length 2 array of
                 * integers. data[0] is the pitch in Hz and data[1]
                 * is the duration in msec.
                 */
               	if (data)
    			do_bell(scp, TIMER_FREQ/((int*)data)[0], 
				((int*)data)[1]*hz/1000);
               	else
			do_bell(scp, scp->bell_pitch, scp->bell_duration);
               	return 0;
#endif
	default:
		break;
	}	
	
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return(error);
	return(ENOTTY);
}

void 
pcxint(dev_t dev)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return;
	tp->t_state &= ~TS_BUSY;
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		pcstart(tp);
}

void 
pcstart(struct tty *tp)
{
	struct clist *rbp;
	int i, s, len;
	u_char buf[PCBURST];
	scr_stat *scp = get_scr_stat(tp->t_dev);

	if (scp->status & SLKED) 
		return;
	s = spltty();
	if (!(tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))) {
		tp->t_state |= TS_BUSY;
		splx(s);
		rbp = &tp->t_outq;
		scp->cursor_protect = 1;
		undraw_cursor(scp);
		while (rbp->c_cc) {
			len = q_to_b(rbp, buf, PCBURST);
			for (i=0; i<len; i++)
				if (buf[i]) ansi_put(scp, buf[i]);
		}
		draw_cursor(scp);
		scp->cursor_protect = 0;
		s = spltty();
		tp->t_state &= ~TS_BUSY;
#if 0
		if (rbp->c_cc) {
			tp->t_state |= TS_TIMEOUT;
			timeout((timeout_func_t)ttrstrt, (caddr_t)tp, 1);
		}
#endif
		if (rbp->c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t)rbp);
			}
			selwakeup(&tp->t_wsel);
		}
	}
	splx(s);
}

void 
pccnprobe(struct consdev *cp)
{
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if ((void*)cdevsw[maj].d_open == (void*)pcopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_INTERNAL;
}

void 
pccninit(struct consdev *cp)
{
	scinit();
}

void 
pccnputc(dev_t dev, char c)
{
	if (c == '\n')
		scput('\r');
	scput(c);
}

int 
pccngetc(dev_t dev)
{
	int s = spltty();		/* block scintr while we poll */
	int c = scgetc(0);
	splx(s);
	return(c);
}

int 
pccncheckc(dev_t dev)
{
	return (scgetc(1) & 0xff);
}

static void 
none_saver(int test)
{
}

static void 
fade_saver(int test)
{
	static int count = 0;
	int i;

	if (test) {
		scrn_blanked = 1;
		if (count < 64) {
  			outb(PIXMASK, 0xFF);		/* no pixelmask */
  			outb(PALWADR, 0x00);
    			outb(PALDATA, 0);
    			outb(PALDATA, 0);
    			outb(PALDATA, 0);
  			for (i = 3; i < 768; i++) {
  				if (palette[i] - count > 15)
    					outb(PALDATA, palette[i]-count);
				else
    					outb(PALDATA, 15);
			}
			inb(crtc_addr+6);		/* reset flip/flop */
			outb(ATC, 0x20);		/* enable palette */
			count++;
		}
	}
	else {
		count = scrn_blanked = 0;
		load_palette();
	}
}

static void 
blank_saver(int test)
{
	u_char val;
	if (test) {
		scrn_blanked = 1;
  		outb(TSIDX, 0x01); val = inb(TSREG); 
		outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
	}
	else {
		scrn_blanked = 0;
  		outb(TSIDX, 0x01); val = inb(TSREG); 
		outb(TSIDX, 0x01); outb(TSREG, val & 0xDF);
	}
}

static void 
green_saver(int test)
{
	u_char val;
	if (test) {
		scrn_blanked = 1;
  		outb(TSIDX, 0x01); val = inb(TSREG); 
		outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
		outb(crtc_addr, 0x17); val = inb(crtc_addr + 1);
		outb(crtc_addr + 1, val & ~0x80);
	}
	else {
		scrn_blanked = 0;
  		outb(TSIDX, 0x01); val = inb(TSREG); 
		outb(TSIDX, 0x01); outb(TSREG, val & 0xDF);
                outb(crtc_addr, 0x17); val = inb(crtc_addr + 1);
                outb(crtc_addr + 1, val | 0x80);
	}
}

#define NUM_STARS	50

/*
 * Alternate saver that got its inspiration from a well known utility
 * package for an inferior^H^H^H^H^H^Hfamous OS.
 */
static void 
star_saver(int test)
{
	scr_stat	*scp = cur_console;
	int		cell, i;
	char 		pattern[] = {"...........++++***   "};
	char		colors[] = {FG_DARKGREY, FG_LIGHTGREY, 
				    FG_WHITE, FG_LIGHTCYAN};
	static u_short 	stars[NUM_STARS][2];
 
	if (test) {
		if (!scrn_blanked) {
			bcopyw(Crtat, scp->scr_buf, 
			      scp->xsize * scp->ysize * 2);
			fillw((FG_LIGHTGREY|BG_BLACK)<<8 | scr_map[0x20], Crtat, 
			      scp->xsize * scp->ysize);
			set_border(0);
			i = scp->ysize * scp->xsize + 5;
			outb(crtc_addr, 14);
			outb(crtc_addr+1, i >> 8);
			outb(crtc_addr, 15);
			outb(crtc_addr+1, i & 0xff);
			scrn_blanked = 1;
 			for(i=0; i<NUM_STARS; i++) {
  				stars[i][0] = 
					random() % (scp->xsize*scp->ysize);
  				stars[i][1] = 0;
 			}
		}
   		cell = random() % NUM_STARS;
		*((u_short*)(Crtat + stars[cell][0])) = 
			scr_map[pattern[stars[cell][1]]] | 
			        colors[random()%sizeof(colors)] << 8;
		if ((stars[cell][1]+=(random()%4)) >= sizeof(pattern)-1) {
    			stars[cell][0] = random() % (scp->xsize*scp->ysize);
   			stars[cell][1] = 0;
		}
	}
	else {
		if (scrn_blanked) {
			bcopyw(scp->scr_buf, Crtat, scp->xsize*scp->ysize*2);
			set_border(scp->border);
			scrn_blanked = 0;
		}
	}
}

static void 
snake_saver(int test)
{
	const char	saves[] = {"FreeBSD-2.0"};
	static u_char	*savs[sizeof(saves)-1];
	static int	dirx, diry;
	int		f;
	scr_stat	*scp = cur_console;

	if (test) {
		if (!scrn_blanked) {
			bcopyw(Crtat, scp->scr_buf,
			      scp->xsize * scp->ysize * 2);
			fillw((FG_LIGHTGREY|BG_BLACK)<<8 | scr_map[0x20],
			      Crtat, scp->xsize * scp->ysize);
			set_border(0);
			dirx = (scp->xpos ? 1 : -1);
			diry = (scp->ypos ?
				scp->xsize : -scp->xsize);
			for (f=0; f< sizeof(saves)-1; f++)
				savs[f] = (u_char *)Crtat + 2 *
					  (scp->xpos+scp->ypos*scp->xsize);
			*(savs[0]) = scr_map[*saves];
			f = scp->ysize * scp->xsize + 5;
			outb(crtc_addr, 14);
			outb(crtc_addr+1, f >> 8);
			outb(crtc_addr, 15);
			outb(crtc_addr+1, f & 0xff);
			scrn_blanked = 1;
		}
		if (scrn_blanked++ < 4) 
			return;
		scrn_blanked = 1;
		*(savs[sizeof(saves)-2]) = scr_map[0x20];
		for (f=sizeof(saves)-2; f > 0; f--)
			savs[f] = savs[f-1];
		f = (savs[0] - (u_char *)Crtat) / 2;
		if ((f % scp->xsize) == 0 ||
		    (f % scp->xsize) == scp->xsize - 1 ||
		    (random() % 50) == 0)
			dirx = -dirx;
		if ((f / scp->xsize) == 0 ||
		    (f / scp->xsize) == scp->ysize - 1 ||
		    (random() % 20) == 0)
			diry = -diry;
		savs[0] += 2*dirx + 2*diry;
		for (f=sizeof(saves)-2; f>=0; f--)
			*(savs[f]) = scr_map[saves[f]];
	}
	else {
		if (scrn_blanked) {
			bcopyw(scp->scr_buf, Crtat,
			      scp->xsize * scp->ysize * 2);
			set_border(scp->border);
			scrn_blanked = 0;
		}
	}
}

static void 
cursor_shape(int start, int end)
{
	outb(crtc_addr, 10);
	outb(crtc_addr+1, start & 0xFF);
	outb(crtc_addr, 11);
	outb(crtc_addr+1, end & 0xFF);
}

#if !defined(FAT_CURSOR)
static void 
get_cursor_shape(int *start, int *end)
{
	outb(crtc_addr, 10);
	*start = inb(crtc_addr+1) & 0x1F;
	outb(crtc_addr, 11);
	*end = inb(crtc_addr+1) & 0x1F;
}
#endif

static void 
scrn_timer()
{
	timeout((timeout_func_t)scrn_timer, 0, hz/5);
	if (cur_console->status & UNKNOWN_MODE) 
		return;
	if (!cur_console->cursor_protect && configuration & BLINK_CURSOR) {
		if (cur_console->cursor_saveunder)
			undraw_cursor(cur_console);
		else
			draw_cursor(cur_console);
	}
	if (scrn_blank_time && (time.tv_sec > scrn_time_stamp+scrn_blank_time))
		SCRN_SAVER(1);
}

static void 
clear_screen(scr_stat *scp)
{
	move_crsr(scp, 0, 0);
	fillw(scp->term.cur_attr | scr_map[0x20], scp->crt_base,
	       scp->xsize * scp->ysize);
}

static int 
switch_scr(scr_stat *scp, u_int next_scr)
{
	if (switch_in_progress && 
	    (cur_console->proc != pfind(cur_console->pid)))
		switch_in_progress = 0;

    	if (next_scr >= MAXCONS || switch_in_progress
	    || (cur_console->smode.mode == VT_AUTO 
	       	&& cur_console->status & UNKNOWN_MODE)) {
		do_bell(scp, BELL_PITCH, BELL_DURATION);
		return EINVAL;
	}

	/* is the wanted virtual console open ? */
	if (next_scr) {
		struct tty *tp = VIRTUAL_TTY(next_scr);
		if (!(tp->t_state & TS_ISOPEN)) {
			do_bell(scp, BELL_PITCH, BELL_DURATION);
			return EINVAL;
		}
	}
	/* delay switch if actively updating screen */
	if (write_in_progress || blink_in_progress) {
		delayed_next_scr = next_scr+1;
		return 0;
	}
	switch_in_progress = 1;
	old_scp = cur_console;
	new_scp = console[next_scr];
	wakeup((caddr_t)&new_scp->smode);
	if (new_scp == old_scp) {
		switch_in_progress = 0;
		return 0;
	}
	
	/* has controlling process died? */
	if (old_scp->proc && (old_scp->proc != pfind(old_scp->pid)))
		old_scp->smode.mode = VT_AUTO;
	if (new_scp->proc && (new_scp->proc != pfind(new_scp->pid)))
		new_scp->smode.mode = VT_AUTO;

	/* check the modes and switch approbiatly */
	if (old_scp->smode.mode == VT_PROCESS) {
		old_scp->status |= SWITCH_WAIT_REL;
		psignal(old_scp->proc, old_scp->smode.relsig);
	}
	else {
		exchange_scr();
		if (new_scp->smode.mode == VT_PROCESS) {
			new_scp->status |= SWITCH_WAIT_ACQ;
			psignal(new_scp->proc, new_scp->smode.acqsig);
		}
		else
			switch_in_progress = 0;
	}
	return 0;
}

static void 
exchange_scr(void) 
{
	bcopyw(Crtat, old_scp->scr_buf, old_scp->xsize * old_scp->ysize * 2);
	old_scp->crt_base = old_scp->scr_buf;
	move_crsr(old_scp, old_scp->xpos, old_scp->ypos);
	cur_console = new_scp;
	if (old_scp->mode != new_scp->mode || (old_scp->status & UNKNOWN_MODE)){
        	if (crtc_vga && video_mode_ptr)
			set_mode(new_scp);
	}
	new_scp->crt_base = Crtat;
	move_crsr(new_scp, new_scp->xpos, new_scp->ypos);
	bcopyw(new_scp->scr_buf, Crtat, new_scp->xsize * new_scp->ysize * 2);
	update_leds(new_scp->status);
	if ((old_scp->status & UNKNOWN_MODE) && crtc_vga) {
		if (fonts_loaded & FONT_16_LOADED)
			copy_font(LOAD, 0, 16, font_16);
		if (fonts_loaded & FONT_8_LOADED)
			copy_font(LOAD, 1, 8, font_8);
		if (fonts_loaded & FONT_14_LOADED)
			copy_font(LOAD, 2, 14, font_14);
		load_palette();
	}
	if (old_scp->status & KBD_RAW_MODE || new_scp->status & KBD_RAW_MODE)
		shfts = ctls = alts = agrs = metas = 0;
	delayed_next_scr = 0;
}

static void 
move_crsr(scr_stat *scp, int x, int y)
{
	if (x < 0 || y < 0 || x >= scp->xsize || y >= scp->ysize)
		return;
	scp->xpos = x;
	scp->ypos = y;
	scp->crtat = scp->crt_base + scp->ypos * scp->xsize + scp->xpos;
}

static void 
scan_esc(scr_stat *scp, u_char c)
{
	static u_char ansi_col[16] = 
		{0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
	int i, n;
	u_short *src, *dst, count;

	if (scp->term.esc == 1) {
		switch (c) {

		case '[': 	/* Start ESC [ sequence */
			scp->term.esc = 2;
			scp->term.last_param = -1;
			for (i = scp->term.num_param; i < MAX_ESC_PAR; i++)
				scp->term.param[i] = 1;
			scp->term.num_param = 0;
			return;

		case 'M':	/* Move cursor up 1 line, scroll if at top */
			if (scp->ypos > 0)
				move_crsr(scp, scp->xpos, scp->ypos - 1);
			else {
				bcopyw(scp->crt_base, 
					scp->crt_base + scp->xsize,
					(scp->ysize - 1) * scp->xsize *
					sizeof(u_short));
				fillw(scp->term.cur_attr | scr_map[0x20], 
				      scp->crt_base, scp->xsize);
			}
			break;
#if notyet
		case 'Q':
			scp->term.esc = 4;
			break;
#endif
		case 'c':	/* Clear screen & home */
			clear_screen(scp);
			break;
		}
	}
	else if (scp->term.esc == 2) {
		if (c >= '0' && c <= '9') {
		    if (scp->term.num_param < MAX_ESC_PAR) {
			if (scp->term.last_param != scp->term.num_param) {
		    	    scp->term.last_param = scp->term.num_param;
		    	    scp->term.param[scp->term.num_param] = 0;
			}
			else
		    	    scp->term.param[scp->term.num_param] *= 10;
		    	scp->term.param[scp->term.num_param] += c - '0';
		    	return;
		    }
		}
		scp->term.num_param = scp->term.last_param + 1;
		switch (c) {

		case ';':
			if (scp->term.num_param < MAX_ESC_PAR)
				return;
			break;

		case '=':
			scp->term.esc = 3;
			scp->term.last_param = -1;
			for (i = scp->term.num_param; i < MAX_ESC_PAR; i++)
				scp->term.param[i] = 1;
			scp->term.num_param = 0;
			return;

		case 'A': /* up n rows */
			n = scp->term.param[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->xpos, scp->ypos - n);
			break;

		case 'B': /* down n rows */
			n = scp->term.param[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->xpos, scp->ypos + n);
			break;

		case 'C': /* right n columns */
			n = scp->term.param[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->xpos + n, scp->ypos);
			break;

		case 'D': /* left n columns */
			n = scp->term.param[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->xpos - n, scp->ypos);
			break;

		case 'E': /* cursor to start of line n lines down */
			n = scp->term.param[0]; if (n < 1) n = 1;
			move_crsr(scp, 0, scp->ypos + n);
			break;

		case 'F': /* cursor to start of line n lines up */
			n = scp->term.param[0]; if (n < 1) n = 1;
			move_crsr(scp, 0, scp->ypos - n);
			break;

		case 'f': /* System V consoles .. */
		case 'H': /* Cursor move */
			if (scp->term.num_param == 0) 
				move_crsr(scp, 0, 0);
			else if (scp->term.num_param == 2)
				move_crsr(scp, scp->term.param[1] - 1, 
					  scp->term.param[0] - 1);
			break;

		case 'J': /* Clear all or part of display */
			if (scp->term.num_param == 0)
				n = 0;
			else
				n = scp->term.param[0];
			switch (n) {
			case 0: /* clear form cursor to end of display */
				fillw(scp->term.cur_attr | scr_map[0x20],
				      scp->crtat, scp->crt_base + 
				      scp->xsize * scp->ysize - 
				      scp->crtat);
				break;
			case 1: /* clear from beginning of display to cursor */
				fillw(scp->term.cur_attr | scr_map[0x20],
				      scp->crt_base, 
				      scp->crtat - scp->crt_base);
				break;
			case 2: /* clear entire display */
				clear_screen(scp);
				break;
			}
			break;

		case 'K': /* Clear all or part of line */
			if (scp->term.num_param == 0)
				n = 0;
			else
				n = scp->term.param[0];
			switch (n) {
			case 0: /* clear form cursor to end of line */
				fillw(scp->term.cur_attr | scr_map[0x20],
				      scp->crtat, scp->xsize - scp->xpos);
				break;
			case 1: /* clear from beginning of line to cursor */
				fillw(scp->term.cur_attr|scr_map[0x20], 
				      scp->crtat - (scp->xsize - scp->xpos),
				      (scp->xsize - scp->xpos) + 1);
				break;
			case 2: /* clear entire line */
				fillw(scp->term.cur_attr|scr_map[0x20], 
				      scp->crtat - (scp->xsize - scp->xpos),
				      scp->xsize);
				break;
			}
			break;

		case 'L':	/* Insert n lines */
			n = scp->term.param[0]; if (n < 1) n = 1;
			if (n > scp->ysize - scp->ypos)
				n = scp->ysize - scp->ypos;
			src = scp->crt_base + scp->ypos * scp->xsize;
			dst = src + n * scp->xsize;
			count = scp->ysize - (scp->ypos + n);
			bcopyw(src, dst, count * scp->xsize * sizeof(u_short));
			fillw(scp->term.cur_attr | scr_map[0x20], src,
			      n * scp->xsize);
			break;

		case 'M':	/* Delete n lines */
			n = scp->term.param[0]; if (n < 1) n = 1;
			if (n > scp->ysize - scp->ypos)
				n = scp->ysize - scp->ypos;
			dst = scp->crt_base + scp->ypos * scp->xsize;
			src = dst + n * scp->xsize;
			count = scp->ysize - (scp->ypos + n);
			bcopyw(src, dst, count * scp->xsize * sizeof(u_short));
			src = dst + count * scp->xsize;
			fillw(scp->term.cur_attr | scr_map[0x20], src,
			      n * scp->xsize);
			break;

		case 'P':	/* Delete n chars */
			n = scp->term.param[0]; if (n < 1) n = 1;
			if (n > scp->xsize - scp->xpos)
				n = scp->xsize - scp->xpos;
			dst = scp->crtat;
			src = dst + n;
			count = scp->xsize - (scp->xpos + n);
			bcopyw(src, dst, count * sizeof(u_short));
			src = dst + count;
			fillw(scp->term.cur_attr | scr_map[0x20], src, n);
			break;

		case '@':	/* Insert n chars */
			n = scp->term.param[0]; if (n < 1) n = 1;
			if (n > scp->xsize - scp->xpos)
				n = scp->xsize - scp->xpos;
			src = scp->crtat;
			dst = src + n;
			count = scp->xsize - (scp->xpos + n);
			bcopyw(src, dst, count * sizeof(u_short));
			fillw(scp->term.cur_attr | scr_map[0x20], src, n);
			break;

		case 'S':	/* scroll up n lines */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			if (n > scp->ysize)
				n = scp->ysize;
			bcopyw(scp->crt_base + (scp->xsize * n),
			      scp->crt_base, 
			      scp->xsize * (scp->ysize - n) * 
			      sizeof(u_short));
			fillw(scp->term.cur_attr | scr_map[0x20],
			      scp->crt_base + scp->xsize * 
			      (scp->ysize - n),
			      scp->xsize * n);
			break;

		case 'T':	/* scroll down n lines */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			if (n > scp->ysize)
				n = scp->ysize;
			bcopyw(scp->crt_base, 
			      scp->crt_base + (scp->xsize * n),
			      scp->xsize * (scp->ysize - n) * 
			      sizeof(u_short));
			fillw(scp->term.cur_attr | scr_map[0x20], 
			      scp->crt_base, scp->xsize * n);
			break;

		case 'X':	/* delete n characters in line */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			if (n > scp->xsize - scp->xpos)
				n = scp->xsize - scp->xpos;
			fillw(scp->term.cur_attr | scr_map[0x20], 
                              scp->crt_base + scp->xpos + 
			      ((scp->xsize*scp->ypos) * sizeof(u_short)), n);
			break;

		case 'Z':	/* move n tabs backwards */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			if ((i = scp->xpos & 0xf8) == scp->xpos)
				i -= 8*n;
			else 
				i -= 8*(n-1); 
			if (i < 0) 
				i = 0;
			move_crsr(scp, i, scp->ypos);
			break;

		case '`': 	/* move cursor to column n */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, n - 1, scp->ypos);
			break;

		case 'a': 	/* move cursor n columns to the right */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->xpos + n, scp->ypos);
			break;

		case 'd': 	/* move cursor to row n */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->xpos, n - 1);
			break;

		case 'e': 	/* move cursor n rows down */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->xpos, scp->ypos + n);
			break;

		case 'm': 	/* change attribute */
			if (scp->term.num_param == 0) {
				scp->term.cur_attr = scp->term.std_attr;
				break;
			}
			for (i = 0; i < scp->term.num_param; i++) {
				switch (n = scp->term.param[i]) {
				case 0:	/* back to normal */
					scp->term.cur_attr = scp->term.std_attr;
					break;
				case 1:	/* highlight (bold) */
					scp->term.cur_attr &= 0xFF00;
					scp->term.cur_attr |= 0x0800;
					break;
				case 4: /* highlight (underline) */
					scp->term.cur_attr &= 0xFF00;
					scp->term.cur_attr |= 0x0800;
					break;
				case 5: /* blink */
					scp->term.cur_attr &= 0xFF00;
					scp->term.cur_attr |= 0x8000;
					break;
				case 7: /* reverse video */
					scp->term.cur_attr = scp->term.rev_attr;
					break;
				case 30: case 31: /* set fg color */
				case 32: case 33: case 34: 
				case 35: case 36: case 37:
					scp->term.cur_attr = 
						(scp->term.cur_attr & 0xF8FF)
						| (ansi_col[(n-30) & 7] << 8);
					break;
				case 40: case 41: /* set bg color */
				case 42: case 43: case 44: 
				case 45: case 46: case 47:
					scp->term.cur_attr = 
						(scp->term.cur_attr & 0x8FFF)
						| (ansi_col[(n-40) & 7] << 12);
					break;
				}
			}
			break;

		case 'x':
			if (scp->term.num_param == 0)
				n = 0;
			else
				n = scp->term.param[0];
			switch (n) {
			case 0: 	/* reset attributes */
				scp->term.cur_attr = scp->term.std_attr =
				    current_default->std_attr;
				scp->term.rev_attr = current_default->rev_attr;
				break;
			case 1: 	/* set ansi background */
				scp->term.cur_attr = scp->term.std_attr =  
				    (scp->term.std_attr & 0x0F00) |
				    (ansi_col[(scp->term.param[1])&0x0F]<<12);
				break;
			case 2: 	/* set ansi foreground */
				scp->term.cur_attr = scp->term.std_attr =  
				    (scp->term.std_attr & 0xF000) |
				    (ansi_col[(scp->term.param[1])&0x0F]<<8);
				break;
			case 3: 	/* set ansi attribute directly */
				scp->term.cur_attr = scp->term.std_attr =
				    (scp->term.param[1]&0xFF)<<8;
				break;
			case 5: 	/* set ansi reverse video background */
				scp->term.rev_attr = 
				    (scp->term.rev_attr & 0x0F00) |
				    (ansi_col[(scp->term.param[1])&0x0F]<<12);
				break;
			case 6: 	/* set ansi reverse video foreground */
				scp->term.rev_attr = 
				    (scp->term.rev_attr & 0xF000) |
				    (ansi_col[(scp->term.param[1])&0x0F]<<8);
				break;
			case 7: 	/* set ansi reverse video directly */
				scp->term.rev_attr =
				    (scp->term.param[1]&0xFF)<<8;
				break;
			}
			break;

		case 'z':	/* switch to (virtual) console n */
			if (scp->term.num_param == 1)
				switch_scr(scp, scp->term.param[0]);
			break;
		}
	}
	else if (scp->term.esc == 3) {
		if (c >= '0' && c <= '9') {
			if (scp->term.num_param < MAX_ESC_PAR) {
				if (scp->term.last_param != scp->term.num_param) {
					scp->term.last_param = scp->term.num_param;
					scp->term.param[scp->term.num_param] = 0;
				}
				else
					scp->term.param[scp->term.num_param] *= 10;
				scp->term.param[scp->term.num_param] += c - '0';
				return;
			}
		}
		scp->term.num_param = scp->term.last_param + 1;
		switch (c) {

		case ';':
			if (scp->term.num_param < MAX_ESC_PAR)
				return;
			break;

		case 'A':	/* set display border color */
			if (scp->term.num_param == 1)
				scp->border=scp->term.param[0] & 0xff;
				if (scp == cur_console)
					set_border(scp->border);
			break;

		case 'B':	/* set bell pitch and duration */
			if (scp->term.num_param == 2) {
				scp->bell_pitch = scp->term.param[0];
				scp->bell_duration = scp->term.param[1]*10;
			}
			break;

		case 'C': 	/* set cursor type & shape */
			if (scp->term.num_param == 1) {
				if (scp->term.param[0] & 0x01)
					configuration |= BLINK_CURSOR;
				else
					configuration &= ~BLINK_CURSOR;
			}
#if 0
			else if (scp->term.num_param == 2) {
				scp->cursor_start = scp->term.param[0] & 0x1F; 
				scp->cursor_end = scp->term.param[1] & 0x1F; 
				if (scp == cur_console)
					cursor_shape(scp->cursor_start,
						     scp->cursor_end);
			}
#endif
			break;

		case 'F':	/* set ansi foreground */
			if (scp->term.num_param == 1) 
				scp->term.cur_attr = scp->term.std_attr =  
					(scp->term.std_attr & 0xF000) 
					| ((scp->term.param[0] & 0x0F) << 8);
			break;

		case 'G': 	/* set ansi background */
			if (scp->term.num_param == 1) 
				scp->term.cur_attr = scp->term.std_attr =  
					(scp->term.std_attr & 0x0F00) 
					| ((scp->term.param[0] & 0x0F) << 12);
			break;

		case 'H':	/* set ansi reverse video foreground */
			if (scp->term.num_param == 1) 
				scp->term.rev_attr = 
					(scp->term.rev_attr & 0xF000) 
					| ((scp->term.param[0] & 0x0F) << 8);
			break;

		case 'I': 	/* set ansi reverse video background */
			if (scp->term.num_param == 1) 
				scp->term.rev_attr = 
					(scp->term.rev_attr & 0x0F00) 
					| ((scp->term.param[0] & 0x0F) << 12);
			break;
		}
	}
	scp->term.esc = 0;
}

static void 
undraw_cursor(scr_stat *scp)
{
	if (scp->cursor_saveunder) {
		*scp->crtat = scp->cursor_saveunder;
		scp->cursor_saveunder = NULL;
	}
}

static void 
draw_cursor(scr_stat *scp)
{
	scp->cursor_saveunder = *scp->crtat;
	if ((scp->cursor_saveunder & 0x7000) == 0x7000) {
		*scp->crtat &= 0x8fff;
		if(!(scp->cursor_saveunder & 0x0700))
			*scp->crtat |= 0x0700;
	} else {
		*scp->crtat |= 0x7000;
		if ((scp->cursor_saveunder & 0x0f00) == 0x0700)
			*scp->crtat &= 0xf8ff;
	}
}

static void 
ansi_put(scr_stat *scp, u_char c)
{
	if (scp->status & UNKNOWN_MODE) 
		return;

	/* make screensaver happy */
	if (scp == cur_console) {
		scrn_time_stamp = time.tv_sec;
		if (scrn_blanked)
			SCRN_SAVER(0);
	}
	write_in_progress++;
	if (scp->term.esc)
		scan_esc(scp, c);
	else switch(c) {
	case 0x1B:	/* start escape sequence */
		scp->term.esc = 1;
		scp->term.num_param = 0;
		break;
	case 0x07:
		do_bell(scp, scp->bell_pitch, scp->bell_duration);
		break;
	case '\t':	/* non-destructive tab */
		scp->crtat += (8 - scp->xpos % 8);
		scp->xpos += (8 - scp->xpos % 8);
		break;
	case '\b':      /* non-destructive backspace */
		if (scp->crtat > scp->crt_base) {
			scp->crtat--;
			if (scp->xpos > 0)
				scp->xpos--;
			else {
				scp->xpos += scp->xsize - 1;
				scp->ypos--;
			}
		}
		break;
	case '\r':	/* return to pos 0 */
		move_crsr(scp, 0, scp->ypos);
		break;
	case '\n':	/* newline, same pos */
		scp->crtat += scp->xsize;
		scp->ypos++;
		break;
	case '\f':	/* form feed, clears screen */
		clear_screen(scp);
		break;
	default:
		/* Print only printables */
		*scp->crtat = (scp->term.cur_attr | scr_map[c]);
		scp->crtat++;
		if (++scp->xpos >= scp->xsize) {
			scp->xpos = 0;
			scp->ypos++;
		}
		break;
	}
	/* do we have to scroll ?? */
	if (scp->crtat >= scp->crt_base + scp->ysize * scp->xsize) {
		/* process history lines begin */
		if (scp->history_head+scp->xsize > scp->history+HISTORY_SIZE) {
			bcopy(scp->history + scp->xsize, scp->history,
			      (HISTORY_SIZE - scp->xsize) * sizeof(u_short));
			bcopyw(scp->crt_base, scp->history_head - scp->xsize,
				scp->xsize * sizeof(u_short));
		}
		else {
			bcopyw(scp->crt_base, scp->history_head,
				scp->xsize * sizeof(u_short));
			scp->history_head += scp->xsize;
		}
		/* process history lines end */

		bcopyw(scp->crt_base + scp->xsize, scp->crt_base,
			scp->xsize * (scp->ysize - 1) * sizeof(u_short));
		fillw(scp->term.cur_attr | scr_map[0x20],
			scp->crt_base + scp->xsize * (scp->ysize - 1), 
			scp->xsize);
		scp->crtat -= scp->xsize;
		scp->ypos--;
	}
	write_in_progress--;
	if (delayed_next_scr)
		switch_scr(scp, delayed_next_scr - 1);
}

static void 
scinit(void)
{
	u_short volatile *cp = Crtat + (CGA_BUF-MONO_BUF)/sizeof(u_short), was;
	unsigned cursorat;
	int i;

	/*
	 * catch that once in a blue moon occurence when scinit is called 
	 * TWICE, adding the CGA_BUF offset again -> poooff
	 */
	if (crtat != 0) 	
		return;
	/*
	 * Crtat initialized to point to MONO buffer, if not present change
	 * to CGA_BUF offset. ONLY ADD the difference since locore.s adds
	 * in the remapped offset at the "right" time
	 */
	was = *cp;
	*cp = (u_short) 0xA55A;
	if (*cp != 0xA55A)
		crtc_addr = MONO_BASE;
	else {
		*cp = was;
		crtc_addr = COLOR_BASE;
		Crtat = Crtat + (CGA_BUF-MONO_BUF)/sizeof(u_short);
	}

	/* extract cursor location */
	outb(crtc_addr,14);
	cursorat = inb(crtc_addr+1)<<8 ;
	outb(crtc_addr,15);
	cursorat |= inb(crtc_addr+1);
	crtat = Crtat + cursorat;

	/* move hardware cursor out of the way */
	outb(crtc_addr,14);
	outb(crtc_addr+1, 0xff);
	outb(crtc_addr,15);
	outb(crtc_addr+1, 0xff);

	/* is this a VGA or higher ? */
	outb(crtc_addr, 7);
	if (inb(crtc_addr) == 7) {
		u_long	pa;
		u_long	segoff;

		crtc_vga = 1;

		/*
		 * Get the BIOS video mode pointer.
		 */
		segoff = *(u_long *)pa_to_va(0x4a8);
		pa = (((segoff & 0xffff0000) >> 12) + (segoff & 0xffff));
		if (ISMAPPED(pa, sizeof(u_long))) {
			segoff = *(u_long *)pa_to_va(pa);
			pa = (((segoff & 0xffff0000) >> 12)
			      + (segoff & 0xffff));
			if (ISMAPPED(pa, 64))
				video_mode_ptr = (char *)pa_to_va(pa);
		}
	}
	current_default = &user_default;
	console[0] = &main_console;
	init_scp(console[0]);
	console[0]->crt_base = Crtat;
	console[0]->crtat = crtat;
	console[0]->xpos = cursorat % COL;
	console[0]->ypos = cursorat / COL;
	cur_console = console[0];
	for (i=1; i<MAXCONS; i++)
		console[i] = NULL;
	kernel_console.esc = 0;
	kernel_console.std_attr = kernel_default.std_attr;
	kernel_console.rev_attr = kernel_default.rev_attr;
	kernel_console.cur_attr = kernel_default.std_attr;
	/* initialize mapscrn array to a one to one map */
	for (i=0; i<sizeof(scr_map); i++)
		scr_map[i] = i;
}

static scr_stat 
*alloc_scp()
{
	scr_stat *scp;

	scp = (scr_stat *)malloc(sizeof(scr_stat), M_DEVBUF, M_NOWAIT);
	init_scp(scp);
	scp->crt_base = scp->crtat = scp->scr_buf =
		(u_short *)malloc(scp->xsize*scp->ysize*2, M_DEVBUF, M_NOWAIT);
        if (crtc_vga && video_mode_ptr)
		set_mode(scp);
	clear_screen(scp);
	return scp;
}

static void
init_scp(scr_stat *scp)
{
	scp->mode = M_VGA_C80x25;
	scp->xsize = COL;
	scp->ysize = ROW;
	scp->term.esc = 0;
	scp->term.std_attr = current_default->std_attr;
	scp->term.rev_attr = current_default->rev_attr;
	scp->term.cur_attr = scp->term.std_attr;
	scp->border = BG_BLACK;
	scp->cursor_start = -1;
	scp->cursor_end = -1;
	scp->cursor_protect = 0;
	scp->cursor_saveunder = scr_map[0x20];
	scp->bell_pitch = BELL_PITCH;
	scp->bell_duration = BELL_DURATION;
	scp->status = (*(char *)pa_to_va(0x417) & 0x20) ? NLKED : 0;
	scp->pid = 0;
	scp->proc = NULL;
	scp->smode.mode = VT_AUTO;
	scp->history_head = scp->history_pos = scp->history;
}

static void 
scput(u_char c)
{
	scr_stat *scp = console[0];
	term_stat save;

	if (crtat == 0)
		scinit();
/* SOS
	if (!write_in_progress) {
		++write_in_progress;
*/
		save = scp->term;
		scp->term = kernel_console;
		current_default = &kernel_default;
		scp->cursor_protect = 1;
		undraw_cursor(scp);
		ansi_put(scp, c);
		draw_cursor(scp);
		scp->cursor_protect = 0;
		kernel_console = scp->term;
		current_default = &user_default;
		scp->term = save;
/* SOS
		--write_in_progress;
	} else {
		if (console_buffer_count < CONSOLE_BUFSIZE)
			console_buffer[console_buffer_count++] = c;
	}
*/
}

static u_char 
*get_fstr(u_int c, u_int *len)
{
	u_int i;

	if (!(c & FKEY))
		return(NULL);
	i = (c & 0xFF) - F_FN;
	if (i > n_fkey_tab)
		return(NULL);
	*len = fkey_tab[i].len;
	return(fkey_tab[i].str);
}

static void 
update_leds(int which)
{
	int s;
  	static u_char xlate_leds[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	/* replace CAPS led with ALTGR led for ALTGR keyboards */
	if (key_map.n_keys > ALTGR_OFFSET) {
		if (which & ALKED)
			which |= CLKED;
		else
			which &= ~CLKED;
	}
	s = spltty();
	kbd_cmd(KB_SETLEDS);
	kbd_cmd(xlate_leds[which & LED_MASK]);
	splx(s);
}
  
/*
 * scgetc(noblock) - get character from keyboard. 
 * If noblock = 0 wait until a key is pressed.
 * Else return NOKEY.
 */
u_int 
scgetc(int noblock)
{
	u_char scancode, keycode;
	u_int state, action;
	struct key_t *key;
	static u_char esc_flag = 0, compose = 0;
	static u_int chr = 0;

next_code:
	kbd_wait();
	/* First see if there is something in the keyboard port */
	if (inb(KB_STAT) & KB_BUF_FULL)
		scancode = inb(KB_DATA);
	else if (noblock)
		return(NOKEY);
	else
		goto next_code;

	if (cur_console->status & KBD_RAW_MODE)
		return scancode;
#if ASYNCH
	if (scancode == KB_ACK || scancode == KB_RESEND) {
		kbd_reply = scancode;
		if (noblock)
			return(NOKEY);
		goto next_code;
	}
#endif
	keycode = scancode & 0x7F;
	switch (esc_flag) {
	case 0x00:		/* normal scancode */
		switch(scancode) {
		case 0xB8:	/* left alt  (compose key) */
			if (compose) {
				compose = 0;	
				if (chr > 255) {
					do_bell(cur_console, 
						BELL_PITCH, BELL_DURATION);
					chr = 0;
				}
			}
			break;
		case 0x38:
			if (!compose) {
				compose = 1;
				chr = 0;
			}
			break;
		case 0xE0:
		case 0xE1:
			esc_flag = scancode;
			goto next_code;		
		}
		break;
	case 0xE0:		/* 0xE0 prefix */
		esc_flag = 0;
		switch (keycode) {
		case 0x1C:	/* right enter key */
			keycode = 0x59;
			break;
		case 0x1D:	/* right ctrl key */
			keycode = 0x5A;
			break;
		case 0x35:	/* keypad divide key */
			keycode = 0x5B;
			break;
		case 0x37:	/* print scrn key */
			keycode = 0x5C;
			break;
		case 0x38:	/* right alt key (alt gr) */
			keycode = 0x5D;
			break;
		case 0x47:	/* grey home key */
			keycode = 0x5E;
			break;
		case 0x48:	/* grey up arrow key */
			keycode = 0x5F;
			break;
		case 0x49:	/* grey page up key */
			keycode = 0x60;
			break;
		case 0x4B:	/* grey left arrow key */
			keycode = 0x61;
			break;
		case 0x4D:	/* grey right arrow key */
			keycode = 0x62;
			break;
		case 0x4F:	/* grey end key */
			keycode = 0x63;
			break;
		case 0x50:	/* grey down arrow key */
			keycode = 0x64;
			break;
		case 0x51:	/* grey page down key */
			keycode = 0x65;
			break;
		case 0x52:	/* grey insert key */
			keycode = 0x66;
			break;
		case 0x53:	/* grey delete key */
			keycode = 0x67;
			break;

		/* the following 3 are only used on the MS "Natural" keyboard */
 		case 0x5b:	/* left Window key */
 			keycode = 0x69;
 			break;
 		case 0x5c:	/* right Window key */
 			keycode = 0x6a;
 			break;
 		case 0x5d:	/* menu key */
 			keycode = 0x6b;
 			break;
		default:	/* ignore everything else */
			goto next_code;
		}
		break;
	case 0xE1:		/* 0xE1 prefix */
		esc_flag = 0;	
		if (keycode == 0x1D)
			esc_flag = 0x1D;
		goto next_code;
		/* NOT REACHED */
	case 0x1D:		/* pause / break */
		esc_flag = 0;	
		if (keycode != 0x45)
			goto next_code;
		keycode = 0x68;
		break;
	}

	/* if scroll-lock pressed allow history browsing */
	if (cur_console->status & SLKED) {
		cur_console->cursor_protect = 1;
		undraw_cursor(cur_console);
		if (!(cur_console->status & BUFFER_SAVED)) {
			bcopyw(Crtat, cur_console->scr_buf, 
			      cur_console->xsize * cur_console->ysize * 
			      sizeof(u_short));
			cur_console->status |= BUFFER_SAVED;
			cur_console->history_pos = cur_console->history_head;
		}
		switch (scancode) {
		case 0x50:	/* down arrow key */
	    	    if (cur_console->history_pos < cur_console->history_head) {
			bcopyw(cur_console->crt_base+cur_console->xsize,
			       cur_console->crt_base, 
		               cur_console->xsize * (cur_console->ysize - 1) *
			       sizeof(u_short));
			if (cur_console->history_pos + 
		    	    (cur_console->xsize*cur_console->ysize) <
			    cur_console->history_head) {
			    /* from history buffer */
			    bcopyw(cur_console->history_pos +
		      	             cur_console->xsize*(cur_console->ysize),
		  		   cur_console->crt_base +
		  		     cur_console->xsize*(cur_console->ysize-1),
	          		   cur_console->xsize * sizeof(u_short));
			    }
			else {
			    /* from screen buffer */
	    		    bcopyw(cur_console->scr_buf + 
		      	    	     cur_console->xsize * cur_console->ysize -
				     (cur_console->history_head -
				     cur_console->history_pos),
		  	    	   cur_console->crt_base +
				     cur_console->xsize*(cur_console->ysize-1),
	              	    	   cur_console->xsize * sizeof(u_short));
			}	
			cur_console->history_pos += cur_console->xsize;
		    }
		    else
			do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		    goto next_code;

		case 0x48:	/* up arrow key */
		    if (cur_console->history_pos > cur_console->history) {
		        bcopyw(cur_console->crt_base, 
		               cur_console->crt_base + cur_console->xsize,
		               cur_console->xsize * cur_console->ysize * 
		                 sizeof(u_short));
		        bcopyw(cur_console->history_pos - cur_console->xsize,
			       cur_console->crt_base, 
		               cur_console->xsize * sizeof(u_short));
		        cur_console->history_pos -= cur_console->xsize;
		    }
		    else
		    	do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		    goto next_code;
		}
	}

	if (compose) {
		switch (scancode) {
		/* key pressed process it */
		case 0x47: case 0x48: case 0x49:	/* keypad 7,8,9 */
			chr = (scancode - 0x40) + chr*10;
			goto next_code;
		case 0x4B: case 0x4C: case 0x4D:	/* keypad 4,5,6 */ 
			chr = (scancode - 0x47) + chr*10;
			goto next_code;
		case 0x4F: case 0x50: case 0x51:	/* keypad 1,2,3 */ 
			chr = (scancode - 0x4E) + chr*10;
			goto next_code;
		case 0x52:				/* keypad 0 */
			chr *= 10;
			goto next_code;

		/* key release, no interest here */
		case 0xC7: case 0xC8: case 0xC9:	/* keypad 7,8,9 */
		case 0xCB: case 0xCC: case 0xCD:	/* keypad 4,5,6 */ 
		case 0xCF: case 0xD0: case 0xD1:	/* keypad 1,2,3 */ 
		case 0xD2:				/* keypad 0 */
			goto next_code;

		case 0x38:				/* left alt key */
			break;
		default:
			if (chr) {
				compose = chr = 0;
				do_bell(cur_console, BELL_PITCH, BELL_DURATION);
				goto next_code;		
			}
			break;
		}
	}
		
	state = (shfts ? 1 : 0 ) | (2 * (ctls ? 1 : 0)) | (4 * (alts ? 1 : 0));
	if ((!agrs && (cur_console->status & ALKED))
	    || (agrs && !(cur_console->status & ALKED)))
		keycode += ALTGR_OFFSET;
	key = &key_map.key[keycode];
	if ( ((key->flgs & FLAG_LOCK_C) && (cur_console->status & CLKED))
	     || ((key->flgs & FLAG_LOCK_N) && (cur_console->status & NLKED)) )
		state ^= 1;

	/* Check for make/break */
	action = key->map[state];
	if (scancode & 0x80) { 		/* key released */
		if (key->spcl & 0x80) {
			switch (action) {
			case LSH:
				shfts &= ~1;
				break;
			case RSH:
				shfts &= ~2;
				break;
			case LCTR:
				ctls &= ~1;
				break;
			case RCTR:
				ctls &= ~2;
				break;
			case LALT:
				alts &= ~1;
				break;
			case RALT:
				alts &= ~2;
				break;
			case NLK:
				nlkcnt = 0;				
				break;
			case CLK:
				clkcnt = 0;
				break;
			case SLK:
				slkcnt = 0;
				break;
			case ASH:
				agrs = 0;
				break;
			case ALK:
				alkcnt = 0;
				break;
			case META:
				metas = 0;
				break;
			}
		}
		if (chr && !compose) {
			action = chr;
			chr = 0;
			return(action);
		}
	} else {
		/* key pressed */
		if (key->spcl & (0x80>>state)) {
			switch (action) {
			/* LOCKING KEYS */
			case NLK:
				if (!nlkcnt) {
					nlkcnt++;
					if (cur_console->status & NLKED) 
						cur_console->status &= ~NLKED;
					else
						cur_console->status |= NLKED;
					update_leds(cur_console->status);
				}
				break;
			case CLK:
				if (!clkcnt) {
					clkcnt++;
					if (cur_console->status & CLKED)
						cur_console->status &= ~CLKED;
					else
						cur_console->status |= CLKED;
					update_leds(cur_console->status);
				}
				break;
			case SLK:
				if (!slkcnt) {
				    slkcnt++;
				    if (cur_console->status & SLKED) {
					cur_console->status &= ~SLKED;
					if (cur_console->status & BUFFER_SAVED){
					    bcopyw(cur_console->scr_buf, Crtat,
					    	   cur_console->xsize *
					    	     cur_console->ysize *
						     sizeof(u_short));
					    cur_console->status&=~BUFFER_SAVED;
					    cur_console->history_pos = 
					        cur_console->history_head;
					    draw_cursor(cur_console);
					    cur_console->cursor_protect = 0;
					}
					pcstart(VIRTUAL_TTY(get_scr_num()));
				    } 
				    else 
				        cur_console->status |= SLKED;
				    update_leds(cur_console->status);
				}
				break;
 			case ALK:
				if (!alkcnt) {
					alkcnt++;
 					if (cur_console->status & ALKED)
 						cur_console->status &= ~ALKED;
 					else
 						cur_console->status |= ALKED;
					update_leds(cur_console->status);
				}
  				break;

			/* NON-LOCKING KEYS */
			case NOP:
				break;
			case RBT:
				shutdown_nice();
				break;	
			case SUSP:
				break;

			case DBG:
#ifdef DDB			/* try to switch to console 0 */
				if (cur_console->smode.mode == VT_AUTO &&
		    		    console[0]->smode.mode == VT_AUTO)
					switch_scr(cur_console, 0); 
				Debugger("manual escape to debugger");
				return(NOKEY);
#else
				printf("No debugger in kernel\n");
#endif
				break;
			case LSH:
				shfts |= 1;
				break;
			case RSH:
				shfts |= 2;
				break;
			case LCTR:
				ctls |= 1;
				break;
			case RCTR:
				ctls |= 2;
				break;
			case LALT:
				alts |= 1;
				break;
			case RALT:
				alts |= 2;
				break;
			case ASH:
				agrs = 1;
				break;
			case META:
				metas = 1;
				break;
			case NEXT:
				switch_scr(cur_console,
					   (get_scr_num() + 1) % MAXCONS);
				break;
			case BTAB:
				action = F(16);
			default:
				if (action >= F_SCR && action <= L_SCR) {
					switch_scr(cur_console, action - F_SCR);
					break;
				}
				if (action >= F_FN && action <= L_FN) 
					action |= FKEY;
				return(action);
			}
		}
		else {
			if (metas)
				action |= MKEY;
 			return(action);
		}
	}
	goto next_code;
}

int 
getchar(void)
{
	u_char thechar;
	int s;

	polling = 1;
	s = splhigh();
	scput('>');
	thechar = (u_char) scgetc(0);
	polling = 0;
	splx(s);
	switch (thechar) {
	default: 
		if (thechar >= scr_map[0x20])
			scput(thechar);
		return(thechar);
	case cr:
	case lf: 
		scput(cr); scput(lf);
		return(lf);
	case bs:
	case del:
		scput(bs); scput(scr_map[0x20]); scput(bs);
		return(thechar);
	case cntld:
		scput('^'); scput('D'); scput('\r'); scput('\n');
		return(0);
	}
}

int 
pcmmap(dev_t dev, int offset, int nprot)
{
	if (offset > 0x20000 - PAGE_SIZE)
		return -1;
	return i386_btop((VIDEOMEM + offset));
}

static void 
kbd_wait(void)
{
	int i = 1000;

	while (i--) {
		if ((inb(KB_STAT) & KB_READY) == 0) 
			break;
		DELAY (10);
	}
}

static void 
kbd_cmd(u_char command)
{
	int retry = 5;
	do {
		int i = 100000;

		kbd_wait();
#if ASYNCH
		kbd_reply = 0;
		outb(KB_DATA, command);
		while (i--) {
			if (kbd_reply == KB_ACK)
				return;
			if (kbd_reply == KB_RESEND)
				break;
		}
#else
		outb(KB_DATA, command);
		while (i--) {
			if (inb(KB_STAT) & KB_BUF_FULL) {
				int val;
				DELAY(10);
				val = inb(KB_DATA);
				if (val == KB_ACK)
					return;
				if (val == KB_RESEND)
					break;
			}
		}
#endif
	} while (retry--);
}

static void 
set_mode(scr_stat *scp)
{
	char *modetable;
	char special_modetable[64];
	int mode, font_size;

	if (scp != cur_console)
		return;

	/* setup video hardware for the given mode */
	switch (scp->mode) {
	case M_VGA_M80x60: 
		bcopyw(video_mode_ptr+(64*M_VGA_M80x25),&special_modetable, 64);
		special_modetable[2] = 8;
		special_modetable[19] = 7;
		special_modetable[16] = 0xdf; /* 480 lines */
		special_modetable[28] = 0xdf; /* 480 lines */
		modetable = special_modetable;
		goto setup_mode;

	case M_VGA_C80x60: 
		bcopyw(video_mode_ptr+(64*M_VGA_C80x25),&special_modetable, 64);
		special_modetable[2] = 8;
		special_modetable[19] = 7;
		special_modetable[16] = 0xdf; /* 480 lines */
		special_modetable[28] = 0xdf; /* 480 lines */
		modetable = special_modetable;
		goto setup_mode;

	case M_VGA_M80x50: 
		bcopyw(video_mode_ptr+(64*M_VGA_M80x25),&special_modetable, 64);
		special_modetable[2] = 8;
		special_modetable[19] = 7;
		modetable = special_modetable;
		goto setup_mode;

       	case M_VGA_C80x50:
		bcopyw(video_mode_ptr+(64*M_VGA_C80x25),&special_modetable, 64);
		special_modetable[2] = 8;
		special_modetable[19] = 7;
		modetable = special_modetable;
		goto setup_mode;

       	case M_ENH_B80x43:
		bcopyw(video_mode_ptr+(64*M_ENH_B80x25),&special_modetable, 64);
		special_modetable[2] = 8;
		special_modetable[19] = 7;
		special_modetable[28] = 87;
		modetable = special_modetable;
		goto setup_mode;

	case M_ENH_C80x43:
		bcopyw(video_mode_ptr+(64*M_ENH_C80x25),&special_modetable, 64);
		special_modetable[2] = 8;
		special_modetable[19] = 7;
		special_modetable[28] = 87;
		modetable = special_modetable;
		goto setup_mode;

       	case M_VGA_M80x30:
		bcopyw(video_mode_ptr+(64*M_VGA_M80x25),&special_modetable, 64);
		special_modetable[16] = 0xdf; /* 480 lines */
		special_modetable[28] = 0xdf; /* 480 lines */
		modetable = special_modetable;
		goto setup_mode;

       	case M_VGA_C80x30:
		bcopyw(video_mode_ptr+(64*M_VGA_C80x25),&special_modetable, 64);
		special_modetable[16] = 0xdf; /* 480 lines */
		special_modetable[28] = 0xdf; /* 480 lines */
		modetable = special_modetable;
		goto setup_mode;

       	case M_VGA_C40x25: case M_VGA_C80x25:	/* VGA TEXT MODES */
	case M_VGA_M80x25: 
       	case M_B40x25:     case M_C40x25:	
       	case M_B80x25:     case M_C80x25:
       	case M_ENH_B40x25: case M_ENH_C40x25: 
       	case M_ENH_B80x25: case M_ENH_C80x25:

		modetable = video_mode_ptr + (scp->mode * 64);
setup_mode:
		set_vgaregs(modetable);
		font_size = *(modetable + 2);

		/* set font type (size) */
 		switch (font_size) {
 		case 0x08:
 	    		outb(TSIDX, 0x03); outb(TSREG, 0x05);	/* font 1 */
 			break;
 		case 0x0E:
 	    		outb(TSIDX, 0x03); outb(TSREG, 0x0A);	/* font 2 */
 			break;
 		case 0x10:
 	    		outb(TSIDX, 0x03); outb(TSREG, 0x00);	/* font 0 */
 			break;
 		default:
 	    		outb(TSIDX, 0x03); outb(TSREG, 0x05);	/* font 1 */
 		}
		break;

	case M_BG320:      case M_CG320:      case M_BG640:
	case M_CG320_D:    case M_CG640_E:
	case M_CG640x350:  case M_ENH_CG640:
	case M_BG640x480:  case M_CG640x480:  case M_VGA_CG320:
 
 		set_vgaregs(video_mode_ptr + (scp->mode * 64));
		break;

	default:
		/* call user defined function XXX */
		break;
	}

	/* set border color for this (virtual) console */
	set_border(scp->border);
	return;
}

static void 
set_border(int color)
{
	inb(crtc_addr+6); 				/* reset flip-flop */
	outb(ATC, 0x11); outb(ATC, color); 
 	inb(crtc_addr+6); 				/* reset flip-flop */
 	outb(ATC, 0x20);				/* enable Palette */
}

static void
set_vgaregs(char *modetable)
{
	int i, s = splhigh();

	outb(TSIDX, 0x00); outb(TSREG, 0x01);	/* stop sequencer */
	outb(TSIDX, 0x07); outb(TSREG, 0x00);	/* unlock registers */
	for (i=0; i<4; i++) {			/* program sequencer */
		outb(TSIDX, i+1); 
		outb(TSREG, modetable[i+5]);
	}
	outb(MISC, modetable[9]);		/* set dot-clock */
	outb(TSIDX, 0x00); outb(TSREG, 0x03);	/* start sequencer */
	outb(crtc_addr, 0x11);
	outb(crtc_addr+1, inb(crtc_addr+1) & 0x7F);
	for (i=0; i<25; i++) {			/* program crtc */
		outb(crtc_addr, i); 
		if (i == 14 || i == 15)		/* no hardware cursor */
			outb(crtc_addr+1, 0xff);
		else
			outb(crtc_addr+1, modetable[i+10]);
	}
	inb(crtc_addr+6); 			/* reset flip-flop */
	for (i=0; i<20; i++) {			/* program attribute ctrl */
		outb(ATC, i); 
		outb(ATC, modetable[i+35]);
	}
	for (i=0; i<9; i++) {			/* program graph data ctrl */
		outb(GDCIDX, i); 
		outb(GDCREG, modetable[i+55]);
	}
	inb(crtc_addr+6); 			/* reset flip-flop */
	outb(ATC ,0x20);			/* enable palette */
	splx(s);
}

static void 
copy_font(int direction, int segment, int size, char* font)
{
  	int ch, line, s;
	u_char val;

 	outb(TSIDX, 0x01); val = inb(TSREG); 		/* blank screen */
	outb(TSIDX, 0x01); outb(TSREG, val | 0x20);

	/* setup vga for loading fonts (graphics plane mode) */
	inb(crtc_addr+6);				/* reset flip/flop */
	outb(ATC, 0x30); outb(ATC, 0x01);
	outb(TSIDX, 0x02); outb(TSREG, 0x04);
	outb(TSIDX, 0x04); outb(TSREG, 0x06);
	outb(GDCIDX, 0x04); outb(GDCREG, 0x02);
	outb(GDCIDX, 0x05); outb(GDCREG, 0x00);
	outb(GDCIDX, 0x06); outb(GDCREG, 0x05);		/* addr = a0000, 64kb */
    	for (ch=0; ch < 256; ch++) 
	    for (line=0; line < size; line++) 
		if (direction)
		    *(char *)pa_to_va(VIDEOMEM+(segment*0x4000)+(ch*32)+line) =
					font[(ch*size)+line];	
		else
		    font[(ch*size)+line] =
		    *(char *)pa_to_va(VIDEOMEM+(segment*0x4000)+(ch*32)+line);
	/* setup vga for text mode again */
	s = splhigh();
	inb(crtc_addr+6);				/* reset flip/flop */
	outb(ATC, 0x30); outb(ATC, 0x0C);
	outb(TSIDX, 0x02); outb(TSREG, 0x03);
	outb(TSIDX, 0x04); outb(TSREG, 0x02);
	outb(GDCIDX, 0x04); outb(GDCREG, 0x00);
	outb(GDCIDX, 0x05); outb(GDCREG, 0x10);
	if (crtc_addr == MONO_BASE) {
		outb(GDCIDX, 0x06); outb(GDCREG, 0x0A);	/* addr = b0000, 32kb */
	}
	else {
		outb(GDCIDX, 0x06); outb(GDCREG, 0x0E);	/* addr = b8000, 32kb */
	}
	splx(s);
 	outb(TSIDX, 0x01); val = inb(TSREG); 		/* unblank screen */
	outb(TSIDX, 0x01); outb(TSREG, val & 0xDF);
}

static void 
save_palette(void)
{
	int i;

  	outb(PALRADR, 0x00);	
  	for (i=0x00; i<0x300; i++)            
    		palette[i] = inb(PALDATA);
	inb(crtc_addr+6);			/* reset flip/flop */
}

static void 
load_palette(void)
{
	int i;

  	outb(PIXMASK, 0xFF);			/* no pixelmask */
  	outb(PALWADR, 0x00);	
  	for (i=0x00; i<0x300; i++)            
    		 outb(PALDATA, palette[i]);
	inb(crtc_addr+6);			/* reset flip/flop */
	outb(ATC, 0x20);			/* enable palette */
}

static void
do_bell(scr_stat *scp, int pitch, int duration)
{
	if (scp == cur_console) {
		if (configuration & VISUAL_BELL) {
			if (blink_in_progress)
				return;
			undraw_cursor(scp);
			bcopy(scp->crt_base, scp->scr_buf,
			      scp->xsize * scp->ysize * sizeof(u_short)); 
			blink_in_progress = 4;
			timeout((timeout_func_t)blink_screen, scp, hz/10);
		}	
		else
			sysbeep(pitch, duration);
	}
}

static void
blink_screen(scr_stat *scp)
{
	if (blink_in_progress > 1) {
		if (blink_in_progress & 1)
			fillw(kernel_default.std_attr | scr_map[0x20],
			      scp->crt_base, scp->xsize * scp->ysize);
		else
			fillw(kernel_default.rev_attr | scr_map[0x20],
			      scp->crt_base, scp->xsize * scp->ysize);
		blink_in_progress--;
		timeout((timeout_func_t)blink_screen, scp, hz/10);
	}
	else {
		bcopy(scp->scr_buf, scp->crt_base,
		      scp->xsize * scp->ysize * sizeof(u_short)); 
		draw_cursor(scp);
		blink_in_progress = 0;
	}
}

#endif /* NSC */
