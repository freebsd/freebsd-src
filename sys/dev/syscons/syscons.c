/*-
 * Copyright (c) 1992-1994 Søren Schmidt
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
 *
 *	from:@(#)syscons.c	1.3 940129
 *	$Id: syscons.c,v 1.30 1994/02/01 11:13:49 ache Exp $
 *
 */

#if !defined(FADE_SAVER) && !defined(BLANK_SAVER) && !defined(STAR_SAVER) && !defined(SNAKE_SAVER)
#define SNAKE_SAVER
#endif

#if !defined(__FreeBSD__)
#define FAT_CURSOR
#endif

#include "param.h"
#include "conf.h"
#include "ioctl.h"
#include "proc.h"
#include "user.h"
#include "tty.h"
#include "uio.h"
#include "callout.h"
#include "systm.h"
#include "kernel.h"
#include "syslog.h"
#include "errno.h"
#include "malloc.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/timerreg.h"
#include "i386/i386/cons.h"
#include "machine/console.h"
#include "machine/psl.h"
#include "machine/frame.h"
#include "machine/pc/display.h"
#include "iso8859.font"
#include "kbdtables.h"
#include "sc.h"

#if NSC > 0

#if !defined(NCONS)
#define NCONS 12
#endif

/* status flags */
#define LOCK_KEY_MASK	0x0000F
#define LED_MASK	0x00007
#define UNKNOWN_MODE	0x00010
#define KBD_RAW_MODE	0x00020
#define SWITCH_WAIT_REL	0x00040
#define SWITCH_WAIT_ACQ	0x00080

/* video hardware memory addresses */
#define VIDEOMEM	0x000A0000

/* misc defines */
#define MAX_ESC_PAR 	3
#define TEXT80x25	1
#define TEXT80x50	2
#define	COL		80
#define	ROW		25
#define BELL_DURATION	5
#define BELL_PITCH	800
#define TIMER_FREQ	1193182			/* should be in isa.h */
#define PCBURST		128

/* defines related to hardware addresses */
#define	MONO_BASE	0x3B4			/* crt controller base mono */
#define	COLOR_BASE	0x3D4			/* crt controller base color */
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
	u_char		border;			/* border color */
	u_short		bell_duration;
	u_short		bell_pitch;
	u_short 	status;			/* status (bitfield) */
	u_short 	mode;			/* mode */
	pid_t 		pid;			/* pid of controlling proc */
	struct proc 	*proc;			/* proc* of controlling proc */
	struct vt_mode 	smode;			/* switch mode */
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

static	scr_stat	console[NCONS];
static	scr_stat	*cur_console = &console[0];
static	scr_stat	*new_scp, *old_scp;
static	term_stat	kernel_console; 
static	default_attr	*current_default;
static	int		switch_in_progress = 0;
static 	u_short	 	*crtat = 0;
static	u_int		crtc_addr = MONO_BASE;
static	char		crtc_vga = 0;
static 	u_char		shfts = 0, ctls = 0, alts = 0, agrs = 0, metas = 0;
static 	u_char		nlkcnt = 0, clkcnt = 0, slkcnt = 0, alkcnt = 0;
static	char		palette[3*256];
static 	const u_int 	n_fkey_tab = sizeof(fkey_tab) / sizeof(*fkey_tab);
static	int 		cur_cursor_pos = -1;
static	char 		in_putc = 0;
static	char	 	polling = 0;
static	int	 	delayed_next_scr;
static	char		saved_console = -1;	/* saved console number	*/
static	long		scrn_blank_time = 0;	/* screen saver timout value */
static	int		scrn_blanked = 0;	/* screen saver active flag */
static	long 		scrn_time_stamp;
static  u_char		scr_map[256];
extern	int hz;
extern	struct timeval time;

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
void scintr(int unit);
int pcmmap(dev_t dev, int offset, int nprot);
u_int sgetc(int noblock);
int getchar(void);
static void scinit(void);
static void scput(u_char c);
static u_int scgetc(int noblock);
static void scrn_saver(int test);
static struct tty *get_tty_ptr(dev_t dev);
static scr_stat *get_scr_stat(dev_t dev);
static int get_scr_num();
static void cursor_shape(int start, int end);
static void get_cursor_shape(int *start, int *end);
static void cursor_pos(int force);
static void clear_screen(scr_stat *scp);
static int switch_scr(u_int next_scr);
static void exchange_scr(void);
static void move_crsr(scr_stat *scp, int x, int y);
static void move_up(u_short *s, u_short *d, u_int len);
static void move_down(u_short *s, u_short *d, u_int len);
static void scan_esc(scr_stat *scp, u_char c);
static void ansi_put(scr_stat *scp, u_char c);
static u_char *get_fstr(u_int c, u_int *len);
static void update_leds(int which);
static void kbd_wait(void);
static void kbd_cmd(u_char command);
static void kbd_cmd2(u_char command, u_char arg);
static int kbd_reply(void);
static void set_mode(scr_stat *scp);
static void set_border(int color);
static void load_font(int segment, int size, char* font);
static void save_palette(void);
static void load_palette(void);
static void change_winsize(struct tty *tp, int x, int y);


/* OS specific stuff */

#if defined(NetBSD)
#define VIRTUAL_TTY(x)	pc_tty[x] ? (pc_tty[x]) : (pc_tty[x] = ttymalloc())
#define	CONSOLE_TTY	pc_tty[NCONS] ? (pc_tty[NCONS]) : (pc_tty[NCONS] = ttymalloc())
#define	frametype	struct trapframe 
#define eflags		tf_eflags
extern	u_short		*Crtat;
struct	tty 		*pc_tty[NCONS+1];
int	ttrstrt();
#endif

#if defined(__FreeBSD__)
#define	frametype	struct trapframe 
#define eflags		tf_eflags
#define	timeout_t	timeout_func_t
#define	MONO_BUF	(KERNBASE+0xB0000)
#define	CGA_BUF		(KERNBASE+0xB8000)
#endif

#if defined(__386BSD__) && !defined(__FreeBSD__)
#define	frametype	struct syscframe
#define eflags		sf_eflags
#define	timeout_t	caddr_t
#define	MONO_BUF	(0xFE0B0000)
#define	CGA_BUF		(0xFE0B8000)
#endif

#if defined(__386BSD__) || defined(__FreeBSD__)
#define VIRTUAL_TTY(x)	&pccons[x]
#define	CONSOLE_TTY	&pccons[NCONS]
u_short			*Crtat = (u_short *)MONO_BUF;
struct	tty 		pccons[NCONS+1];
void 	consinit(void) 	{scinit();}
#include "ddb.h"
#if NDDB > 0
#define DDB	1
#endif
#endif


struct	isa_driver scdriver = {
	pcprobe, pcattach, "sc",
};


int pcprobe(struct isa_device *dev)
{
	/* Enable interrupts and keyboard controller */
	kbd_wait();
	outb(KB_STAT, KB_WRITE);
	kbd_cmd(0x4D);

	/* Start keyboard stuff RESET */
	for (;;) {
		kbd_cmd(KB_RESET);
		if (kbd_reply() == KB_ACK &&    /* command accepted */
		    kbd_reply() == 0xaa)        /* self test passed */
			break;
		printf("Keyboard reset failed\n");
	}
	return (IO_KBDSIZE);
}


int pcattach(struct isa_device *dev)
{
	scr_stat *scp;
	int start = -1, end = -1, i;

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

	if (NCONS > 1) 
		printf(" <%d virtual consoles>\n", NCONS);
	else
		printf("\n");
#if defined(FAT_CURSOR)
                start = 0;
                end = 18;
	if (crtc_vga) {
#else
	if (crtc_vga) {
		get_cursor_shape(&start, &end);
#endif
		save_palette();
		load_font(0, 16, font_8x16);
		load_font(1, 8, font_8x8);
		load_font(2, 14, font_8x14);
	}
	current_default = &user_default;
	for (i = 0; i < NCONS; i++) {
		scp = &console[i];
		scp->scr_buf = (u_short *)malloc(COL * ROW * 2, M_DEVBUF, M_NOWAIT);
		scp->mode = TEXT80x25;
		scp->term.esc = 0;
		scp->term.std_attr = current_default->std_attr;
		scp->term.rev_attr = current_default->rev_attr;
		scp->term.cur_attr = scp->term.std_attr;
		scp->border = BG_BLACK;
		scp->cursor_start = start;
		scp->cursor_end = end;
		scp->xsize = COL;
		scp->ysize = ROW;
		scp->bell_pitch = BELL_PITCH;
		scp->bell_duration = BELL_DURATION;
		scp->status = 0;
		scp->pid = 0;
		scp->proc = NULL;
		scp->smode.mode = VT_AUTO;
		if (i > 0) {
			scp->crt_base = scp->crtat = scp->scr_buf;
			fillw(scp->term.cur_attr|scr_map[0x20], scp->scr_buf, COL*ROW);
		}
	}
	/* get cursor going */
#if defined(FAT_CURSOR)
        cursor_shape(console[0].cursor_start,
                     console[0].cursor_end);
#endif
	cursor_pos(1);
	return 0;
}


static struct tty *get_tty_ptr(dev_t dev)
{
	int unit = minor(dev);

	if (unit > NCONS)
		return(NULL);
	if (unit == NCONS) 
		return(CONSOLE_TTY);
	return(VIRTUAL_TTY(unit));
}


static scr_stat *get_scr_stat(dev_t dev)
{
	int unit = minor(dev);

	if (unit > NCONS)
		return(NULL);
	if (unit == NCONS)
		return(&console[0]);
	return(&console[unit]);
}


static int get_scr_num()
{
	int i = 0;

	while ((i < NCONS) && (cur_console != &console[i])) i++;
	return i < NCONS ? i : 0;
}

int pcopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return(ENXIO);

	tp->t_oproc = pcstart;
	tp->t_param = pcparam;
	tp->t_dev = dev;
	if (!(tp->t_state & TS_ISOPEN)) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return(EBUSY);
	tp->t_state |= TS_CARR_ON;
	tp->t_cflag |= CLOCAL;
#if defined(__FreeBSD__)
	return((*linesw[tp->t_line].l_open)(dev, tp, 0));
#else
	return((*linesw[tp->t_line].l_open)(dev, tp));
#endif
}


int pcclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = get_tty_ptr(dev);
	struct scr_stat *scp;

	if (!tp)
		return(ENXIO);
	if (minor(dev) < NCONS) {
		scp = get_scr_stat(tp->t_dev);
		if (scp->status & SWITCH_WAIT_ACQ)
			wakeup((caddr_t)&scp->smode);
		scp->pid = 0;
		scp->proc = NULL;
		scp->smode.mode = VT_AUTO;
	}
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp); 
	return(0);
}


int pcread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return(ENXIO);
	return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}


int pcwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = get_tty_ptr(dev);

	if (!tp)
		return(ENXIO);
	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}


/*
 * Got a console interrupt, keyboard action !
 * Catch the character, and see who it goes to.
 */
void scintr(int unit)
{
	static struct tty *cur_tty;
	int c, len;
	u_char *cp;

	/* make screensaver happy */
	scrn_time_stamp = time.tv_sec;
	if (scrn_blanked)
		scrn_saver(0);

	c = scgetc(1);

	cur_tty = VIRTUAL_TTY(get_scr_num());
	if (!(cur_tty->t_state & TS_ISOPEN))
		cur_tty = CONSOLE_TTY;

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


/*
 * Set line parameters
 */
int pcparam(struct tty *tp, struct termios *t)
{
	int cflag = t->c_cflag;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;
	return 0;
}


int pcioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int i, error;
	struct tty *tp;
	frametype *fp;
	scr_stat *scp; 

	tp = get_tty_ptr(dev);
	if (!tp)
		return ENXIO;
	scp = get_scr_stat(tp->t_dev);

	switch (cmd) {	/* process console hardware related ioctl's */

	case CONS_BLANKTIME:	/* set screen saver timeout (0 = no saver) */
		scrn_blank_time = *(int*)data;
		return 0;

	case CONS_80x25TEXT:	/* set 80x25 text mode */
		if (!crtc_vga)
			return ENXIO;
		scp->mode = TEXT80x25;
		scp->ysize = 25;
		free(scp->scr_buf, M_DEVBUF); 
		scp->scr_buf = (u_short *)malloc(scp->xsize*scp->ysize*2,
					     M_DEVBUF, M_NOWAIT);
		if (scp != cur_console)
			scp->crt_base = scp->scr_buf;
		set_mode(scp);
		clear_screen(scp);
		change_winsize(tp, scp->xsize, scp->ysize); 
		return 0;

	case CONS_80x50TEXT:	/* set 80x50 text mode */
		if (!crtc_vga)
			return ENXIO;
		scp->mode = TEXT80x50;
		scp->ysize = 50;
		free(scp->scr_buf, M_DEVBUF); 
		scp->scr_buf = (u_short *)malloc(scp->xsize*scp->ysize*2,
					     M_DEVBUF, M_NOWAIT);
		if (scp != cur_console)
			scp->crt_base = scp->scr_buf;
		set_mode(scp);
		clear_screen(scp);
		change_winsize(tp, scp->xsize, scp->ysize); 
		return 0;

	case CONS_GETVERS:	/* get version number */
		*(int*)data = 0x103;	/* version 1.3 */
		return 0;

	case CONS_GETINFO:	/* get current (virtual) console info */
		if (*data == sizeof(struct vid_info)) {
			vid_info_t *ptr = (vid_info_t*)data;
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
 		for (i = 0; i < NCONS; i++) {
			tp = VIRTUAL_TTY(i);
 			if (!(tp->t_state & TS_ISOPEN)) {
 				*data = i + 1;
 				return 0;
 			}
		} 
 		return EINVAL;

	case VT_ACTIVATE:	/* switch to screen *data */
		return switch_scr((*data) - 1);

	case VT_WAITACTIVE:	/* wait for switch to occur */
		if (*data > NCONS) 
			return EINVAL;
		if (minor(dev) == (*data) - 1) 
			return 0;
		if (*data == 0) {
			if (scp == cur_console)
				return 0;
			while ((error=tsleep((caddr_t)&scp->smode, 
			    	PZERO|PCATCH, "waitvt", 0)) == ERESTART) ;
		}
		else 
			while ((error=tsleep(
      				(caddr_t)&console[*(data-1)].smode, 
			    	PZERO|PCATCH, "waitvt", 0)) == ERESTART) ;
		return error;

	case VT_GETACTIVE:
		*data = get_scr_num()+1;
		return 0;

	case KDENABIO:		/* allow io operations */
	 	fp = (frametype *)p->p_regs;
	 	fp->eflags |= PSL_IOPL;
		return 0; 

	case KDDISABIO:		/* disallow io operations (default) */
	 	fp = (frametype *)p->p_regs;
	 	fp->eflags &= ~PSL_IOPL;
	 	return 0;

        case KDSETMODE:		/* set current mode of this (virtual) console */
		switch (*data) {
		case KD_TEXT:	/* switch to TEXT (known) mode */
				/* restore fonts & palette ! */
			if (crtc_vga) {
				load_font(0, 16, font_8x16);
				load_font(1, 8, font_8x8);
				load_font(2, 14, font_8x14);
				load_palette();
			}
			/* FALL THROUGH */

		case KD_TEXT1:	/* switch to TEXT (known) mode */
				/* no restore fonts & palette */
			scp->status &= ~UNKNOWN_MODE;
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
				update_leds(scp->status & LED_MASK);
			return 0;
		}
		return EINVAL;

	case KDGKBSTATE:	/* get keyboard state (locks) */
		*data = scp->status & LOCK_KEY_MASK;
		return 0;

	case KDSETRAD:		/* set keyboard repeat & delay rates */
		if (*data & 0x80)
			return EINVAL;
		kbd_cmd2(KB_SETRAD, *data);
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
		if (scp == cur_console)
			sysbeep(scp->bell_pitch, scp->bell_duration);
		return 0;

	case KIOCSOUND:		/* make tone (*data) hz */
		if (scp == cur_console) {
			if (*(int*)data) {
			int pitch = TIMER_FREQ/(*(int*)data);
				/* enable counter 2 */
				outb(0x61, inb(0x61) | 3);
				/* set command for counter 2, 2 byte write */
				outb(TIMER_MODE, 
				     	TIMER_SEL2|TIMER_16BIT|TIMER_SQWAVE);
				/* set pitch */
				outb(TIMER_CNTR2, pitch);
				outb(TIMER_CNTR2, (pitch>>8));
			}
			else {
				/* disable counter 2 */
				outb(0x61, inb(0x61) & 0xFC);
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
			update_leds(scp->status & LED_MASK);
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
		bcopy(data, &font_8x8, sizeof(font_8x8));
		load_font(1, 8, font_8x8);
		return 0;

	case GIO_FONT8x8:	/* get 8x8 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(&font_8x8, data, sizeof(font_8x8));
		return 0;

	case PIO_FONT8x14:	/* set 8x14 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(data, &font_8x14, sizeof(font_8x14));
		load_font(2, 14, font_8x14);
		return 0;

	case GIO_FONT8x14:	/* get 8x14 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(&font_8x14, data, sizeof(font_8x14));
		return 0;

	case PIO_FONT8x16:	/* set 8x16 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(data, &font_8x16, sizeof(font_8x16));
		load_font(0, 16, font_8x16);
		return 0;

	case GIO_FONT8x16:	/* get 8x16 dot font */
		if (!crtc_vga)
			return ENXIO;
		bcopy(&font_8x16, data, sizeof(font_8x16));
		return 0;

	case CONSOLE_X_MODE_ON:	/* just to be compatible */
		if (saved_console < 0) {
			saved_console = get_scr_num();
			switch_scr(minor(dev));
	 		fp = (frametype *)p->p_regs;
	 		fp->eflags |= PSL_IOPL;
			scp->status |= UNKNOWN_MODE;
			scp->status |= KBD_RAW_MODE;
			return 0;
		}
		return EAGAIN;

	case CONSOLE_X_MODE_OFF:/* just to be compatible */
	 	fp = (frametype *)p->p_regs;
	 	fp->eflags &= ~PSL_IOPL;
		if (crtc_vga) {
			load_font(0, 16, font_8x16);
			load_font(1, 8, font_8x8);
			load_font(2, 14, font_8x14);
			load_palette();
		}
		scp->status &= ~UNKNOWN_MODE;
		set_mode(scp);
		clear_screen(scp);
		scp->status &= ~KBD_RAW_MODE;
		switch_scr(saved_console);
		saved_console = -1;
		return 0;

	 case CONSOLE_X_BELL:	/* more compatibility */
                /*
                 * if set, data is a pointer to a length 2 array of
                 * integers. data[0] is the pitch in Hz and data[1]
                 * is the duration in msec.
                 */
                if (data)
	    		sysbeep(TIMER_FREQ/((int*)data)[0], 
				((int*)data)[1]*hz/3000);
                else
			sysbeep(scp->bell_pitch, scp->bell_duration);
                return 0;

	default:
		break;
	}	
	
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
		return(error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return(error);
	return(ENOTTY);
}


void pcxint(dev_t dev)
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


void pcstart(struct tty *tp)
{
#if defined(NetBSD)
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
		len = q_to_b(rbp, buf, PCBURST);
		for (i=0; i<len; i++)
			if (buf[i]) ansi_put(scp, buf[i]);
		s = spltty();
		tp->t_state &= ~TS_BUSY;
		if (rbp->c_cc) {
			tp->t_state |= TS_TIMEOUT;
			timeout((timeout_t)ttrstrt, (caddr_t)tp, 1);
		}
		if (rbp->c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t)rbp);
			}
			selwakeup(&tp->t_wsel);
		}
	}
	splx(s);

#else /* __FreeBSD__ & __386BSD__ */

	int c, s;
	scr_stat *scp = get_scr_stat(tp->t_dev);

	if (scp->status & SLKED) 
		return;
	s = spltty();
	if (!(tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP)))
		for (;;) {
			if (RB_LEN(&tp->t_out) <= tp->t_lowat) {
				if (tp->t_state & TS_ASLEEP) {
					tp->t_state &= ~TS_ASLEEP;
					wakeup((caddr_t)&tp->t_out);
				}
				if (tp->t_wsel) {
					selwakeup(tp->t_wsel, 
						  tp->t_state & TS_WCOLL);
					tp->t_wsel = 0;
					tp->t_state &= ~TS_WCOLL;
				}
			}
			if (RB_LEN(&tp->t_out) == 0)
				break;
			if (scp->status & SLKED) 
				break;
			c = getc(&tp->t_out);
			tp->t_state |= TS_BUSY;
			splx(s);
			ansi_put(scp, c);
			s = spltty();
			tp->t_state &= ~TS_BUSY;
		}
	splx(s);
#endif
}


void pccnprobe(struct consdev *cp)
{
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if ((void*)cdevsw[maj].d_open == (void*)pcopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, NCONS);
	cp->cn_pri = CN_INTERNAL;
#if defined(__FreeBSD__) || defined(__386BSD__)
	cp->cn_tp = CONSOLE_TTY;
#endif
}


void pccninit(struct consdev *cp)
{
	scinit();
}


void pccnputc(dev_t dev, char c)
{
	if (c == '\n')
		scput('\r');
	scput(c);
	if (cur_console == &console[0]) {
	int 	pos = cur_console->crtat - cur_console->crt_base;
		if (pos != cur_cursor_pos) {
			cur_cursor_pos = pos;
			outb(crtc_addr,14);
			outb(crtc_addr+1,pos >> 8);
			outb(crtc_addr,15);
			outb(crtc_addr+1,pos&0xff);
		}
	}
}


int pccngetc(dev_t dev)
{
	int s = spltty();		/* block scintr while we poll */
	int c = scgetc(0);
	splx(s);
	if (c == '\r') c = '\n';
	return(c);
}

#if defined(FADE_SAVER)
static void scrn_saver(int test)
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
#endif

#if defined(BLANK_SAVER)
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
#endif

#if defined(STAR_SAVER) || defined(SNAKE_SAVER)
static u_long 	rand_next = 1;

static int rand()
{
	return ((rand_next = rand_next * 1103515245 + 12345) & 0x7FFFFFFF);
}
#endif

#if defined(STAR_SAVER)
/*
 * Alternate saver that got its inspiration from a well known utility
 * package for an unfamous OS.
 */

#define NUM_STARS	50

static void scrn_saver(int test)
{
	scr_stat	*scp = cur_console;
	int		cell, i;
	char 		pattern[] = {"...........++++***   "};
	char		colors[] = {FG_DARKGREY, FG_LIGHTGREY, 
				    FG_WHITE, FG_LIGHTCYAN};
	static u_short 	stars[NUM_STARS][2];
 
	if (test) {
		if (!scrn_blanked) {
			bcopy(Crtat, scp->scr_buf, 
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
					rand() % (scp->xsize*scp->ysize);
  				stars[i][1] = 0;
 			}
		}
   		cell = rand() % NUM_STARS;
		*((u_short*)(Crtat + stars[cell][0])) = 
			scr_map[pattern[stars[cell][1]]] | 
			        colors[rand()%sizeof(colors)] << 8;
		if ((stars[cell][1]+=(rand()%4)) >= sizeof(pattern)-1) {
    			stars[cell][0] = rand() % (scp->xsize*scp->ysize);
   			stars[cell][1] = 0;
		}
	}
	else {
		if (scrn_blanked) {
			bcopy(scp->scr_buf, Crtat, scp->xsize*scp->ysize*2);
			cur_cursor_pos = -1;
			set_border(scp->border);
			scrn_blanked = 0;
		}
	}
}
#endif

#if defined(SNAKE_SAVER)
/*
 * alternative screen saver for cards that do not like blanking
 */

static void scrn_saver(int test)
{
	const char	saves[] = {"FreeBSD"};
	static u_char	*savs[sizeof(saves)-1];
	static int	dirx, diry;
	int		f;
	scr_stat	*scp = cur_console;

	if (test) {
		if (!scrn_blanked) {
			bcopy(Crtat, scp->scr_buf,
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
		    (rand() % 50) == 0)
			dirx = -dirx;
		if ((f / scp->xsize) == 0 ||
		    (f / scp->xsize) == scp->ysize - 1 ||
		    (rand() % 20) == 0)
			diry = -diry;
		savs[0] += 2*dirx + 2*diry;
		for (f=sizeof(saves)-2; f>=0; f--)
			*(savs[f]) = scr_map[saves[f]];
	}
	else {
		if (scrn_blanked) {
			bcopy(scp->scr_buf, Crtat,
			      scp->xsize * scp->ysize * 2);
			cur_cursor_pos = -1;
			set_border(scp->border);
			scrn_blanked = 0;
		}
	}
}
#endif

static void cursor_shape(int start, int end)
{
	outb(crtc_addr, 10);
	outb(crtc_addr+1, start & 0xFF);
	outb(crtc_addr, 11);
	outb(crtc_addr+1, end & 0xFF);
}


#if !defined(FAT_CURSOR)
static void get_cursor_shape(int *start, int *end)
{
	outb(crtc_addr, 10);
	*start = inb(crtc_addr+1) & 0x1F;
	outb(crtc_addr, 11);
	*end = inb(crtc_addr+1) & 0x1F;
}
#endif


static void cursor_pos(int force)
{
	int pos;

	if (cur_console->status & UNKNOWN_MODE) 
		return;
	if (scrn_blank_time && (time.tv_sec > scrn_time_stamp+scrn_blank_time))
		scrn_saver(1);
	pos = cur_console->crtat - cur_console->crt_base;
	if (force || (!scrn_blanked && pos != cur_cursor_pos)) {
		cur_cursor_pos = pos;
		outb(crtc_addr, 14);
		outb(crtc_addr+1, pos>>8);
		outb(crtc_addr, 15);
		outb(crtc_addr+1, pos&0xff);
	}
	timeout((timeout_t)cursor_pos, 0, hz/20);
}


static void clear_screen(scr_stat *scp)
{
	move_crsr(scp, 0, 0);
	fillw(scp->term.cur_attr | scr_map[0x20], scp->crt_base,
	       scp->xsize * scp->ysize);
}


static int switch_scr(u_int next_scr)
{
	if (in_putc) {		/* delay switch if in putc */
		delayed_next_scr = next_scr+1;
		return 0;
	}
	if (switch_in_progress && 
	    (cur_console->proc != pfind(cur_console->pid)))
		switch_in_progress = 0;

    	if (next_scr >= NCONS || switch_in_progress) {  
		sysbeep(BELL_PITCH, BELL_DURATION);
		return EINVAL;
	}

	/* is the wanted virtual console open ? */
	if (next_scr) {
		struct tty *tp = VIRTUAL_TTY(next_scr);
		if (!(tp->t_state & TS_ISOPEN)) {
			sysbeep(BELL_PITCH, BELL_DURATION);
			return EINVAL;
		}
	}
		
	switch_in_progress = 1;
	old_scp = cur_console;
	new_scp = &console[next_scr];
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


static void exchange_scr(void) 
{
	struct tty *tp;

	bcopy(Crtat, old_scp->scr_buf, old_scp->xsize * old_scp->ysize * 2);
	old_scp->crt_base = old_scp->scr_buf;
	move_crsr(old_scp, old_scp->xpos, old_scp->ypos);
	cur_console = new_scp;
	set_mode(new_scp);
	new_scp->crt_base = Crtat;
	move_crsr(new_scp, new_scp->xpos, new_scp->ypos);
	bcopy(new_scp->scr_buf, Crtat, new_scp->xsize * new_scp->ysize * 2);
	update_leds(new_scp->status & LED_MASK);
	if (old_scp->status & KBD_RAW_MODE || new_scp->status & KBD_RAW_MODE)
		shfts = ctls = alts = agrs = metas = 0;
	delayed_next_scr = 0;
}


static void move_crsr(scr_stat *scp, int x, int y)
{
	if (x < 0 || y < 0 || x >= scp->xsize || y >= scp->ysize)
		return;
	scp->xpos = x;
	scp->ypos = y;
	scp->crtat = scp->crt_base + scp->ypos * scp->xsize + scp->xpos;
}


static void move_up(u_short *s, u_short *d, u_int len)
{
	s += len;
	d += len;
	while (len-- > 0)
		*--d = *--s;
}


static void move_down(u_short *s, u_short *d, u_int len)
{
	while (len-- > 0)
		*d++ = *s++;
}


static void scan_esc(scr_stat *scp, u_char c)
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
				move_up(scp->crt_base, 
					scp->crt_base + scp->xsize,
					(scp->ysize - 1) * scp->xsize);
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
			move_up(src, dst, count * scp->xsize);
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
			move_down(src, dst, count * scp->xsize);
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
			move_down(src, dst, count);
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
			move_up(src, dst, count);
			fillw(scp->term.cur_attr | scr_map[0x20], src, n);
			break;

		case 'S':	/* scroll up n lines */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			bcopy(scp->crt_base + (scp->xsize * n),
			      scp->crt_base, 
			      scp->xsize * (scp->ysize - n) * 
			      sizeof(u_short));
			fillw(scp->term.cur_attr | scr_map[0x20],
			      scp->crt_base + scp->xsize * 
			      (scp->ysize - 1), 
			      scp->xsize);
			break;

		case 'T':	/* scroll down n lines */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			bcopy(scp->crt_base, 
			      scp->crt_base + (scp->xsize * n),
			      scp->xsize * (scp->ysize - n) * 
			      sizeof(u_short));
			fillw(scp->term.cur_attr | scr_map[0x20], scp->crt_base, 
			      scp->xsize);
			break;

		case 'X':	/* delete n characters in line */
			n = scp->term.param[0]; if (n < 1)  n = 1;
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
			move_crsr(scp, n, scp->ypos);
			break;

		case 'a': 	/* move cursor n columns to the right */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->xpos + n, scp->ypos);
			break;

		case 'd': 	/* move cursor to row n */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->xpos, n);
			break;

		case 'e': 	/* move cursor n rows down */
			n = scp->term.param[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->xpos, scp->ypos + n);
			break;

		case 'm': 	/* change attribute */
			if (scp->term.num_param == 0)
				n = 0;
			else
				n = scp->term.param[0];
			switch (n) {
			case 0:	/* back to normal */
				scp->term.cur_attr = scp->term.std_attr;
				break;
			case 1:	/* highlight (bold) */
				scp->term.cur_attr &= 0xFF00;
				scp->term.cur_attr |= 0x0800;
				break;
			case 4: /* highlight (underline) */
				scp->term.cur_attr &= 0x0F00;
				scp->term.cur_attr |= 0x0800;
				break;
			case 5: /* blink */
				scp->term.cur_attr &= 0xFF00;
				scp->term.cur_attr |= 0x8000;
				break;
			case 7: /* reverse video */
				scp->term.cur_attr = scp->term.rev_attr; 
				break;
			case 30: case 31: case 32: case 33: /* set fg color */
			case 34: case 35: case 36: case 37:
				scp->term.cur_attr = (scp->term.cur_attr & 0xF0FF)
					    | (ansi_col[(n - 30) & 7] << 8);
				break;
			case 40: case 41: case 42: case 43: /* set bg color */
			case 44: case 45: case 46: case 47:
				scp->term.cur_attr = (scp->term.cur_attr & 0x0FFF)
					    | (ansi_col[(n - 40) & 7] << 12);
				break;
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
				scp->term.rev_attr = (scp->term.param[1]&0xFF)<<8;
				break;
			}
			break;

		case 'z':	/* switch to (virtual) console n */
			if (scp->term.num_param == 1)
				switch_scr(scp->term.param[0]);
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

		case 'C': 	/* set cursor shape (start & end line) */
			if (scp->term.num_param == 2) {
				scp->cursor_start = scp->term.param[0] & 0x1F; 
				scp->cursor_end = scp->term.param[1] & 0x1F; 
				if (scp == cur_console)
					cursor_shape(scp->cursor_start,
						     scp->cursor_end);
			}
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


static void ansi_put(scr_stat *scp, u_char c)
{
	if (scp->status & UNKNOWN_MODE) 
		return;

	/* make screensaver happy */
	if (scp == cur_console) {
		scrn_time_stamp = time.tv_sec;
		if (scrn_blanked)
			scrn_saver(0);
	}
	in_putc++;
	if (scp->term.esc)
		scan_esc(scp, c);
	else switch(c) {
	case 0x1B:	/* start escape sequence */
		scp->term.esc = 1;
		scp->term.num_param = 0;
		break;
	case 0x07:
		if (scp == cur_console)
		 	sysbeep(scp->bell_pitch, scp->bell_duration);
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
	if (scp->crtat >= scp->crt_base + scp->ysize * scp->xsize) {
		bcopy(scp->crt_base + scp->xsize, scp->crt_base,
			scp->xsize * (scp->ysize - 1) * sizeof(u_short));
		fillw(scp->term.cur_attr | scr_map[0x20],
			scp->crt_base + scp->xsize * (scp->ysize - 1), 
			scp->xsize);
		scp->crtat -= scp->xsize;
		scp->ypos--;
	}
	in_putc--;
	if (delayed_next_scr)
		switch_scr(delayed_next_scr - 1);
}

static void scinit(void)
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
	if (*cp != 0xA55A) {
		crtc_addr = MONO_BASE;
	} else {
		*cp = was;
		crtc_addr = COLOR_BASE;
		Crtat = Crtat + (CGA_BUF-MONO_BUF)/sizeof(u_short);
	}

	/* Extract cursor location */
	outb(crtc_addr,14);
	cursorat = inb(crtc_addr+1)<<8 ;
	outb(crtc_addr,15);
	cursorat |= inb(crtc_addr+1);
	crtat = Crtat + cursorat;

	/* is this a VGA or higher ? */
	outb(crtc_addr, 7);
	if (inb(crtc_addr) == 7)
		crtc_vga = 1;

	current_default = &user_default;
	console[0].crtat = crtat;
	console[0].crt_base = Crtat;
	console[0].term.esc = 0;
	console[0].term.std_attr = current_default->std_attr;
	console[0].term.rev_attr = current_default->rev_attr;
	console[0].term.cur_attr = current_default->std_attr;
	console[0].xpos = cursorat % COL;
	console[0].ypos = cursorat / COL;
	console[0].border = BG_BLACK;;
	console[0].xsize = COL;
	console[0].ysize = ROW;
	console[0].status = 0;
	console[0].pid = 0;
	console[0].proc = NULL;
	console[0].smode.mode = VT_AUTO;
	console[0].bell_pitch = BELL_PITCH;
	console[0].bell_duration = BELL_DURATION;
	kernel_console.esc = 0;
	kernel_console.std_attr = kernel_default.std_attr;
	kernel_console.rev_attr = kernel_default.rev_attr;
	kernel_console.cur_attr = kernel_default.std_attr;
	/* initialize mapscrn array to a one to one map */
	for (i=0; i<sizeof(scr_map); i++)
		scr_map[i] = i;
	clear_screen(&console[0]);
}


static void scput(u_char c)
{
	scr_stat *scp = &console[0];
	term_stat save;

	if (crtat == 0)
		scinit();
	save = scp->term;
	scp->term = kernel_console;
	current_default = &kernel_default;
	ansi_put(scp, c);
	kernel_console = scp->term;
	current_default = &user_default;
	scp->term = save;
}


static u_char *get_fstr(u_int c, u_int *len)
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


static void update_leds(int which)
{
  	static u_char xlate_leds[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	kbd_cmd2(KB_SETLEDS, xlate_leds[which & LED_MASK]);
}
  
  
/*
 * scgetc(noblock) : get a character from the keyboard. 
 * If noblock = 0 wait until a key is gotten.  Otherwise return NOKEY.
 */
u_int scgetc(int noblock)
{
	u_char val, code, release;
	u_int state, action;
	struct key_t *key;
	static u_char esc_flag = 0, compose = 0;
	static u_int chr = 0;

next_code:
	kbd_wait();
	/* First see if there is something in the keyboard port */
	if (inb(KB_STAT) & KB_BUF_FULL)
		val = inb(KB_DATA);
	else if (noblock)
		return(NOKEY);
	else
		goto next_code;

	if (cur_console->status & KBD_RAW_MODE)
		return val;

	code = val & 0x7F;
	release = val & 0x80;

	switch (esc_flag) {
	case 0x00:		/* normal scancode */
		switch(code) {
		case 0x38:	/* left alt  (compose key) */
			if (release && compose) {
				compose = 0;	
				if (chr > 255) {
					sysbeep(BELL_PITCH, BELL_DURATION);
					chr = 0;
				}
			}
			else {
				if (!compose) {
					compose = 1;
					chr = 0;
				}
			}
			break;
		case 0x60:
		case 0x61:
			esc_flag = code;
			goto next_code;		
		}
		break;
	case 0x60:		/* 0xE0 prefix */
		esc_flag = 0;
		switch (code) {
		case 0x1c:	/* right enter key */
			code = 0x59;
			break;
		case 0x1d:	/* right ctrl key */
			code = 0x5a;
			break;
		case 0x35:	/* keypad divide key */
			code = 0x5b;
			break;
		case 0x37:	/* print scrn key */
			code = 0x5c;
			break;
		case 0x38:	/* right alt key (alt gr) */
			code = 0x5d;
			break;
		case 0x47:	/* grey home key */
			code = 0x5e;
			break;
		case 0x48:	/* grey up arrow key */
			code = 0x5f;
			break;
		case 0x49:	/* grey page up key */
			code = 0x60;
			break;
		case 0x4b:	/* grey left arrow key */
			code = 0x61;
			break;
		case 0x4d:	/* grey right arrow key */
			code = 0x62;
			break;
		case 0x4f:	/* grey end key */
			code = 0x63;
			break;
		case 0x50:	/* grey down arrow key */
			code = 0x64;
			break;
		case 0x51:	/* grey page down key */
			code = 0x65;
			break;
		case 0x52:	/* grey insert key */
			code = 0x66;
			break;
		case 0x53:	/* grey delete key */
			code = 0x67;
			break;
		default:	/* ignore everything else */
			goto next_code;
		}
		break;
	case 0x61:		/* 0xE1 prefix */
		esc_flag = 0;	
		if (code == 0x1D)
			esc_flag = 0x1D;
		goto next_code;
		/* NOT REACHED */
	case 0x1D:		/* pause / break */
		esc_flag = 0;	
		if (code != 0x45)
			goto next_code;
		code = 0x68;
		break;
	}

	if (compose) {
		switch (code) {
		case 0x47: 
		case 0x48:				/* keypad 7,8,9 */ 
		case 0x49:
			if (!release)
				chr = (code - 0x40) + chr*10;
			goto next_code;
		case 0x4b: 
		case 0x4c:				/* keypad 4,5,6 */ 
		case 0x4d:
			if (!release)
				chr = (code - 0x47) + chr*10;
			goto next_code;
		case 0x4f: 
		case 0x50:				/* keypad 1,2,3 */ 
		case 0x51:
			if (!release)
				chr = (code - 0x4e) + chr*10;
			goto next_code;
		case 0x52:				/* keypad 0 */
			if (!release)
				chr *= 10;
			goto next_code;
		case 0x38:				/* left alt key */
			break;
		default:
			if (chr) {
				compose = chr = 0;
				sysbeep(BELL_PITCH, BELL_DURATION);
				goto next_code;		
			}
			break;
		}
	}
		
	state = (shfts ? 1 : 0 ) | (2 * (ctls ? 1 : 0)) | (4 * (alts ? 1 : 0));
	if ((!agrs && (cur_console->status & ALKED))
	    || (agrs && !(cur_console->status & ALKED)))
		code += ALTGR_OFFSET;
	key = &key_map.key[code];
	if ( ((key->flgs & FLAG_LOCK_C) && (cur_console->status & CLKED))
	     || ((key->flgs & FLAG_LOCK_N) && (cur_console->status & NLKED)) )
		state ^= 1;

	/* Check for make/break */
	action = key->map[state];
	if (release) { 		/* key released */
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
					update_leds(cur_console->status & LED_MASK); 
				}
				break;
			case CLK:
				if (!clkcnt) {
					clkcnt++;
					if (cur_console->status & CLKED)
						cur_console->status &= ~CLKED;
					else
						cur_console->status |= CLKED;
					update_leds(cur_console->status & LED_MASK);
				}
				break;
			case SLK:
				if (!slkcnt) {
					slkcnt++;
					if (cur_console->status & SLKED) {
						cur_console->status &= ~SLKED;
						pcstart(VIRTUAL_TTY(get_scr_num()));
					} 
					else 
						cur_console->status |= SLKED;
					update_leds(cur_console->status & LED_MASK);
				}
				break;
 			case ALK:
				if (!alkcnt) {
					alkcnt++;
 					if (cur_console->status & ALKED)
 						cur_console->status &= ~ALKED;
 					else
 						cur_console->status |= ALKED;
				}
  				break;

			/* NON-LOCKING KEYS */
			case NOP:
				break;
			case RBT:
#if defined(__FreeBSD__)
				shutdown_nice();
#else
				cpu_reset();
#endif
				break;	
			case DBG:
#if DDB > 0			/* try to switch to console 0 */
				if (cur_console->smode.mode == VT_AUTO &&
		    		    console[0].smode.mode == VT_AUTO)
					switch_scr(0); 
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
				switch_scr((get_scr_num()+1)%NCONS);
				break;
			default:
				if (action >= F_SCR && action <= L_SCR) {
					switch_scr(action - F_SCR);
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


int getchar(void)
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


u_int sgetc(int noblock)
{
	return (scgetc(noblock & 0xff));
}

int pcmmap(dev_t dev, int offset, int nprot)
{
	if (offset > 0x20000)
		return EINVAL;
	return i386_btop((VIDEOMEM + offset));
}


static void kbd_wait(void)
{
	int i;

	for (i=0; i<1000; i++) {        /* up to 10 msec */
		if ((inb(KB_STAT) & KB_READY) == 0) 
			break;
		DELAY (10);
	}
}


static void kbd_cmd(u_char command)
{
	kbd_wait();
	outb(KB_DATA, command);
}


static void kbd_cmd2(u_char command, u_char arg)
{
	int r, s = spltty();
	do {
		kbd_cmd(command);
		r = kbd_reply();
		if (r == KB_ACK) {
			kbd_cmd(arg & 0x7f);
			r = kbd_reply();
		}
	} while (r != KB_ACK);
	splx(s);
}


static int kbd_reply()
{
	int i;

	kbd_wait();
	for (i=0; i<50000; i++) {       /* at least 300 msec, 500 msec enough */
		if (inb(KB_STAT) & KB_BUF_FULL)
			return ((u_char) inb(KB_DATA));
		DELAY (10);
	}
	return(-1);
}


static void set_mode(scr_stat *scp)
{
	u_char byte;
	int s;

	if (scp != cur_console)
		return;

	/* (re)activate cursor */
	untimeout((timeout_t)cursor_pos, 0);
	cursor_pos(1);
	
	/* change cursor type if set */
	if (scp->cursor_start != -1 && scp->cursor_end != -1)
		cursor_shape(scp->cursor_start, scp->cursor_end);

	/* mode change only on VGA's */
	if (!crtc_vga) 
		return;

	/* setup video hardware for the given mode */
	s = splhigh();
	switch(scp->mode) {
	case TEXT80x25:
		outb(crtc_addr, 9); byte = inb(crtc_addr+1);
		outb(crtc_addr, 9); outb(crtc_addr+1, byte | 0x0F);
    		outb(TSIDX, 0x03); outb(TSREG, 0x00);	/* select font 0 */
		break;
	case TEXT80x50:
		outb(crtc_addr, 9); byte = inb(crtc_addr+1);
		outb(crtc_addr, 9); outb(crtc_addr+1, (byte & 0xF0) | 0x07);
    		outb(TSIDX, 0x03); outb(TSREG, 0x05);	/* select font 1 */
		break;
	default:
		break;
	}
	splx(s);

	/* set border color for this (virtual) console */
	set_border(scp->border);
	return;
}


static void set_border(int color)
{
	inb(crtc_addr+6); 				/* reset flip-flop */
	outb(ATC, 0x11); outb(ATC, color); 
 	inb(crtc_addr+6); 				/* reset flip-flop */
 	outb(ATC, 0x20);			/* enable Palette */
}

static void load_font(int segment, int size, char* font)
{
  	int ch, line, s;
	u_char val;

 	outb(TSIDX, 0x01); val = inb(TSREG); 		/* blank screen */
	outb(TSIDX, 0x01); outb(TSREG, val | 0x20);

	/* setup vga for loading fonts (graphics plane mode) */
	s = splhigh();
	inb(crtc_addr+6);				/* reset flip/flop */
	outb(ATC, 0x30); outb(ATC, 0x01);
	outb(TSIDX, 0x02); outb(TSREG, 0x04);
	outb(TSIDX, 0x04); outb(TSREG, 0x06);
	outb(GDCIDX, 0x04); outb(GDCREG, 0x02);
	outb(GDCIDX, 0x05); outb(GDCREG, 0x00);
	outb(GDCIDX, 0x06); outb(GDCREG, 0x05);		/* addr = a0000, 64kb */
	splx(s);
    	for (ch=0; ch < 256; ch++) 
		for (line=0; line < size; line++) 
			*((char *)atdevbase+(segment*0x4000)+(ch*32)+line) = 
				font[(ch*size)+line];	
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


static void load_palette(void)
{
	int i;

  	outb(PIXMASK, 0xFF);			/* no pixelmask */
  	outb(PALWADR, 0x00);	
  	for (i=0x00; i<0x300; i++)            
    		 outb(PALDATA, palette[i]);
	inb(crtc_addr+6);			/* reset flip/flop */
	outb(ATC, 0x20);			/* enable palette */
}

static void save_palette(void)
{
	int i;

  	outb(PALRADR, 0x00);	
  	for (i=0x00; i<0x300; i++)            
    		palette[i] = inb(PALDATA);
	inb(crtc_addr+6);			/* reset flip/flop */
}


static void change_winsize(struct tty *tp, int x, int y)
{
	if (tp->t_winsize.ws_col != x || tp->t_winsize.ws_row != y) {
		tp->t_winsize.ws_col = x;
		tp->t_winsize.ws_row = y;
		pgsignal(tp->t_pgrp, SIGWINCH, 1);
	}
}

#endif /* NSC */
