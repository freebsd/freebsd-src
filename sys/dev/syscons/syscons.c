/*-
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
 */
/*
 * Heavily modified by Søren Schmidt (sos@kmd-ac.dk) to provide:
 *
 * 	virtual consoles, SYSV ioctl's, ANSI emulation 
 *
 *	@(#)syscons.c	0.2 930402
 *
 * Modified further to provide full (updated) pccons emulation and
 * hooks for init_main.c.   Jordan Hubbard, 930725
 *
 */

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
#include "console.h"
#include "malloc.h"
#include "i386/isa/icu.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/include/pc/display.h"
#include "i386/i386/cons.h"
#include "machine/psl.h"
#include "machine/frame.h"
#include "sc.h"
#include "ddb.h"
#include "iso8859.font"
#include "kbdtables.h"
/*#include "asm.h"*/

#if NSC > 0
#ifndef NCONS
#define NCONS 12
#endif

/* status flags */
#define LOCK_KEY_MASK	0x00007
#define UNKNOWN_MODE	0x00010
#define KBD_RAW_MODE	0x00020
#define SWITCH_WAIT_REL	0x00040
#define SWITCH_WAIT_ACQ	0x00080

/* virtual video memory addresses */
#define	MONO_BUF	0xFE0B0000
#define	CGA_BUF		0xFE0B8000
#define	VGA_BUF		0xFE0A0000
#define VIDEOMEM	0x000A0000

/* misc defines */
#define MAX_ESC_PAR 	2
#define TEXT80x25	1
#define TEXT80x50	2
#define	COL		80
#define	ROW		25
#ifndef XTALSPEED
#define XTALSPEED	1193182			/* should be in isa.h */
#endif

/* defines related to hardware addresses */
#define	MONO_BASE	0x3B4			/* crt controller base mono */
#define	COLOR_BASE	0x3D4			/* crt controller base color */
#define ATC		0x3C0			/* attribute controller */
#define TSIDX		0x3C4			/* timing sequencer idx */
#define TSREG		0x3C5			/* timing sequencer data */
#define PIXMASK		0x3C6			/* pixel write mask */
#define PALRADR		0x3C7			/* palette read address */
#define PALWADR		0x3C8			/* palette write address */
#define PALDATA		0x3C9			/* palette data register */
#define GDCIDX		0x3CE			/* graph data controller idx */
#define GDCREG		0x3CF			/* graph data controller data */

typedef struct scr_stat {
	u_short 	*crt_base;		/* address of screen memory */
	u_short 	*scr;			/* buffer when off screen */
	u_short 	*crtat;			/* cursor address */
	int 		esc;			/* processing escape sequence */
	int 		n_par;			/* # of parameters to ESC */
	int	 	last_par;		/* last parameter # */
	int 		par[MAX_ESC_PAR];	/* contains ESC parameters */
	int 		posx;			/* current X position */
	int 		posy;			/* current Y position */
	int 		max_posx;		/* X size */
	int 		max_posy;		/* X size */
	int 		attr;			/* current attributes */
	int 		std_attr;		/* normal attributes */
	int 		rev_attr;		/* reverse attributes */
	u_char		border;			/* border color */
	u_short		bell_duration;
	u_short		bell_pitch;
	u_short 	status;			/* status (bitfield) */
	u_short 	mode;			/* mode */
	pid_t 		pid;			/* pid of controlling proc */
	struct proc 	*proc;			/* proc* of controlling proc */
	struct vt_mode 	smode;			/* switch mode */
	} scr_stat;

static	scr_stat	cons_scr_stat[NCONS];
static	scr_stat	*cur_scr_stat = &cons_scr_stat[0];
static	scr_stat 	*new_scp, *old_scp;
static	int		switch_in_progress = 0;

u_short			*Crtat = (u_short *)MONO_BUF;
static 	u_short	 	*crtat = 0;
static	u_int		crtc_addr = MONO_BASE;
static 	u_char		shfts = 0, ctls = 0, alts = 0;
static	char		palette[3*256];
static 	const u_int 	n_fkey_tab = sizeof(fkey_tab) / sizeof(*fkey_tab);
static	int 		cur_cursor_pos = -1;
static	char 		in_putc, nx_scr;
static	char		saved_console = -1;	/* saved console number	*/
static	long		blank_time = 0;		/* screen saver timout value */
static  scrmap_t	scr_map;

struct	tty 		pccons[NCONS];
struct	tty 		*cur_pccons = &pccons[0];
struct  tty		*new_pccons;

extern	int hz;
extern	struct timeval time;

#define	CSF_ACTIVE	0x1			/* timeout active */
#define	CSF_POLLING	0x2			/* polling for input */

struct	pcconsoftc {
	char		cs_flags;
	char		cs_lastc;		/* last char sent */
	int		cs_timo;		/* timeouts since interrupt */
	u_long		cs_wedgecnt;		/* times restarted */
	} pcconsoftc = {0, 0, 0, 0};


/* special characters */
#define bs	8
#define lf	10	
#define cr	13	
#define cntlc	3	
#define del	0177	
#define cntld	4

/* function prototypes */
int pcprobe(struct isa_device *dev);
int pcattach(struct isa_device *dev);
int pcopen(dev_t dev, int flag, int mode, struct proc *p);
int pcclose(dev_t dev, int flag, int mode, struct proc *p);
int pcread(dev_t dev, struct uio *uio, int flag);
int pcwrite(dev_t dev, struct uio *uio, int flag);
int pcparam(struct tty *tp, struct termios *t);
int pcioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p);
int pcxint(dev_t dev);
int pcstart(struct tty *tp);
int pccnprobe(struct consdev *cp);
int pccninit(struct consdev *cp);
int pccnputc(dev_t dev, char c);
int pccngetc(dev_t dev);
int scintr(dev_t dev, int irq, int cpl);
void scrn_saver(int test);
static struct tty *get_pccons(dev_t dev);
static scr_stat *get_scr_stat(dev_t dev);
static int get_scr_num(scr_stat *scp);
static void cursor_shape(int start, int end);
static void cursor_pos(void);
static void clear_screen(scr_stat *scp);
static switch_scr(u_int next_scr);
static void exchange_scr(void);
static void move_crsr(scr_stat *scp, u_int x, u_int y);
static void move_up(u_short *s, u_short *d, u_int len);
static void move_down(u_short *s, u_short *d, u_int len);
static void scan_esc(scr_stat *scp, u_char c);
static void ansi_put(scr_stat *scp, u_char c);
void consinit(void);
static void sput(u_char c, u_char ca);
static u_char *get_fstr(u_int c, u_int *len);
static update_leds(int which);
void reset_cpu(void);
u_int sgetc(int noblock);
int pcmmap(dev_t dev, int offset, int nprot);
int getchar(void);
static void kbd_wait(void);
static void kbd_cmd(u_char command);
static void set_mode(scr_stat *scp);
static void set_border(int color);
static load_font(int segment, int size, char* font);
static void save_palette(void);
static void load_palette(void);
static change_winsize(struct tty *tp, int x, int y);

struct	isa_driver scdriver = {
	pcprobe, pcattach, "sc",
	};


int pcprobe(struct isa_device *dev)
{
	u_char c;
	int i, again = 0;

	/* Enable interrupts and keyboard controller */
	kbd_wait();
	outb(KB_STAT, KB_WRITE);
	kbd_cmd(0x4D);

	/* Start keyboard stuff RESET */
	kbd_cmd(KB_RESET);
	while ((c=inb(KB_DATA)) != KB_ACK) {
		if ((c == 0xFE) || (c == 0xFF)) {
			if (!again)
				printf("KEYBOARD disconnected: RECONNECT \n");
			kbd_cmd(KB_RESET);
			again = 1;
		}
	}
	/* 
	 * pick up keyboard reset return code                SOS
	 * some keyboards / controllers hangs if this is enabled
	 */
	/* while ((c=inb(KB_DATA))!=0xAA); */
	return 1;
}


int pcattach(struct isa_device *dev)
{
	scr_stat *scp;
	int i;

	if (crtc_addr == MONO_BASE)
		printf("VGA mono");
	else	
		printf("VGA color");

	if (NCONS > 1) 
		printf(" <%d virtual consoles>\n", NCONS);
	else
		printf("\n");
	save_palette();
	load_font(0, 16, font_8x16);
	load_font(1, 8, font_8x8);
	for (i = 0; i < NCONS; i++) {
		scp = &cons_scr_stat[i];
		scp->mode = TEXT80x25;
		scp->scr = (u_short *)malloc(COL * ROW * 2, M_DEVBUF, M_NOWAIT);
		scp->std_attr = (FG_LIGHTGREY | BG_BLACK) << 8;
		scp->rev_attr = (FG_BLACK | BG_LIGHTGREY) << 8;
		scp->attr = scp->std_attr;
		scp->border = BG_BLACK;
		scp->esc = 0;
		scp->max_posx = COL;
		scp->max_posy = ROW;
		scp->bell_pitch = 800;
		scp->bell_duration = 10;
		scp->status = 0;
		scp->pid = 0;
		scp->proc = NULL;
		scp->smode.mode = VT_AUTO;
		if (i > 0) {
			scp->crt_base = scp->crtat = scp->scr;
			fillw(scp->attr | ' ', scp->scr, COL * ROW);
		}
	}
	cursor_pos();
}


static struct tty *get_pccons(dev_t dev)
{
	int i = minor(dev);

	if (i >= NCONS)
		return(NULL);
	return(&pccons[i]);
}


static scr_stat *get_scr_stat(dev_t dev)
{
	int i = minor(dev);

	if (i >= NCONS)
		return(NULL);
	return(&cons_scr_stat[i]);
}


static int get_scr_num(scr_stat *scp)	/* allways call with legal scp !! */
{
	int i = 0;

	while ((i < NCONS) && (cur_scr_stat != &cons_scr_stat[i])) i++;
	return i;
}

pcopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = get_pccons(dev);

	if (!tp)
		return(ENXIO);
	tp->t_oproc = pcstart;
	tp->t_param = pcparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
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
	return((*linesw[tp->t_line].l_open)(dev, tp));
}


pcclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = get_pccons(dev);
	struct scr_stat *scp;

	if (!tp)
		return(ENXIO);
	scp = get_scr_stat(tp->t_dev);
	scp->pid = 0;
	scp->proc = NULL;
	scp->smode.mode = VT_AUTO;
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp); 
	return(0);
}


pcread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = get_pccons(dev);

	if (!tp)
		return(ENXIO);
	return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}


pcwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = get_pccons(dev);

	if (!tp)
		return(ENXIO);
	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}


/*
 * Got a console interrupt, keyboard action !
 * Catch the character, and see who it goes to.
 */
scintr(dev_t dev, int irq, int cpl)
{
	int c, len;
	u_char *cp;

	scrn_saver(0);
	c = sgetc(1);
	if (c & 0x100)
		return;
	if (pcconsoftc.cs_flags & CSF_POLLING)
		return;
	if (c < 0x100)
		(*linesw[cur_pccons->t_line].l_rint)(c & 0xFF, cur_pccons);
	else if (cp = get_fstr((u_int)c, (u_int *)&len)) {
		while (len-- >  0)
			(*linesw[cur_pccons->t_line].l_rint)
				(*cp++ & 0xFF, cur_pccons);
	}
}


/*
 * Set line parameters
 */
pcparam(struct tty *tp, struct termios *t)
{
	int cflag = t->c_cflag;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;
	return(0);
}


pcioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int i, error;
	struct tty *tp;
	struct syscframe *fp;
	scr_stat *scp; 

	tp = get_pccons(dev);
	if (!tp)
		return(ENXIO);
	scp = get_scr_stat(tp->t_dev);

	switch (cmd) {	/* process console hardware related ioctl's */

	case CONS_BLANKTIME:	/* set screen saver timeout (0 = no saver) */
		blank_time = *(int*)data;
		return 0;

	case CONS_80x25TEXT:	/* set 80x25 text mode */
		scp->mode = TEXT80x25;
		scp->max_posy = 25;
		set_mode(scp);
		clear_screen(scp);
		change_winsize(tp, scp->max_posx, scp->max_posy); 
		free(scp->scr, M_DEVBUF); 
		scp->scr = (u_short *)malloc(scp->max_posx*scp->max_posy*2,
					     M_DEVBUF, M_NOWAIT);
		return 0;

	case CONS_80x50TEXT:	/* set 80x50 text mode */
		scp->mode = TEXT80x50;
		scp->max_posy = 50;
		set_mode(scp);
		clear_screen(scp);
		change_winsize(tp, scp->max_posx, scp->max_posy); 
		free(scp->scr, M_DEVBUF); 
		scp->scr = (u_short *)malloc(scp->max_posx*scp->max_posy*2,
					     M_DEVBUF, M_NOWAIT);
		return 0;

	case CONS_GETINFO:	/* get current (virtual) console info */
		if (*data == sizeof(struct vid_info)) {
			vid_info_t *ptr = (vid_info_t*)data;
			ptr->m_num = get_scr_num(scp);
			ptr->mv_col = scp->posx;
			ptr->mv_row = scp->posy;
			ptr->mv_csz = scp->max_posx;
			ptr->mv_rsz = scp->max_posy;
			ptr->mv_norm.fore = (scp->std_attr & 0x0f00)>>8;
			ptr->mv_norm.back = (scp->std_attr & 0xf000)>>12;
			ptr->mv_rev.fore = (scp->rev_attr & 0x0f00)>>8;
			ptr->mv_rev.back = (scp->rev_attr & 0xf000)>>12;
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

	case VT_OPENQRY:	/* return free virtual cons, allways current */
		*data = get_scr_num(scp);
		return 0;

	case VT_ACTIVATE:	/* switch to screen *data */
		return switch_scr((*data) - 1);

	case VT_WAITACTIVE:	/* wait for switch to occur */
		if (*data > NCONS) 
			return EINVAL;
		if (minor(dev) == (*data) - 1) 
			return 0;
		if (*data == 0) {
			if (scp == cur_scr_stat)
				return 0;
			while ((error=tsleep(&scp->smode, 
					     PZERO|PCATCH, "waitvt", 0)) 
					     == ERESTART) ;
		}
		else 
			while ((error=tsleep(&cons_scr_stat[*data].smode, 
					     PZERO|PCATCH, "waitvt", 0)) 
					     == ERESTART) ;
		return error;

	case KDENABIO:		/* allow io operations */
	 	fp = (struct syscframe *)p->p_regs;
	 	fp->sf_eflags |= PSL_IOPL;
		return 0; 

	case KDDISABIO:		/* disallow io operations (default) */
	 	fp = (struct syscframe *)p->p_regs;
	 	fp->sf_eflags &= ~PSL_IOPL;
	 	return 0;

        case KDSETMODE:		/* set current mode of this (virtual) console */
		switch (*data) {
		case KD_TEXT:	/* switch to TEXT (known) mode */
				/* restore fonts & palette ! */
			load_font(0, 16, font_8x16);
			load_font(1, 8, font_8x8);
			load_palette();
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
		scp->border = *data;
		if (scp == cur_scr_stat) 
			set_border(scp->border);
		return 0;

	case KDSKBSTATE:	/* set keyboard state (locks) */
		if (*data >= 0 && *data < 4) {
			scp->status &= ~LOCK_KEY_MASK;
			scp->status |= *data;
			if (scp == cur_scr_stat) 
				update_leds(scp->status & LOCK_KEY_MASK);
			return 0;
		}
		return EINVAL;

	case KDGKBSTATE:	/* get keyboard state (locks) */
		*data = scp->status & LOCK_KEY_MASK;
		return 0;

	case KDSETRAD:		/* set keyboard repeat & delay rates */
		if (*(u_char*)data < 0x80) {
			kbd_cmd(KB_SETRAD);
			kbd_cmd(*data & 0x7f);
			return 0;
		}
		return EINVAL;

	case KDSKBMODE:		/* set keyboard mode */
		switch (*data) {
		case K_RAW:	/* switch to RAW scancode mode */
			scp->status |= KBD_RAW_MODE;
			return 0;

		case K_XLATE:	/* switch to XLT ascii mode */
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
		if (scp == cur_scr_stat)
			sysbeep(scp->bell_pitch, scp->bell_duration);
		return 0;

	case KIOCSOUND:		/* make tone (*data) hz */
		if (scp == cur_scr_stat) {
			if (*(int*)data) {
			int pitch = XTALSPEED/(*(int*)data);
				/* enable counter 2 */
				outb(0x61, inb(0x61) | 3);
				/* set command for counter 2, 2 byte write */
				outb(0x43, 0xb6);
				/* set pitch */
				outb(0x42, pitch);
				outb(0x42, (pitch>>8));
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
		scp->status &= ~LOCK_KEY_MASK;
		scp->status |= (*data & LOCK_KEY_MASK);
		if (scp == cur_scr_stat)
			update_leds(scp->status & LOCK_KEY_MASK);
		return 0;

	case KDGETLED:		/* get keyboard LED status */
		*data = scp->status & LOCK_KEY_MASK;
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
		bcopy(&scr_map, data, sizeof(scrmap_t));
		return 0;

	case PIO_SCRNMAP:	/* set output translation table */
		bcopy(data, &scr_map, sizeof(scrmap_t));
		return 0;

	case GIO_KEYMAP: 	/* get keyboard translation table */
		bcopy(&key_map, data, sizeof(key_map));
		return 0;

	case PIO_KEYMAP:	/* set keyboard translation table */
		bcopy(data, &key_map, sizeof(key_map));
		return 0;

	case PIO_FONT8x8:	/* set 8x8 dot font */
		bcopy(data, &font_8x8, sizeof(fnt8_t));
		load_font(1, 8, font_8x16);
		return 0;

	case GIO_FONT8x8:	/* get 8x8 dot font */
		bcopy(&font_8x8, data, sizeof(fnt8_t));
		return 0;

	case PIO_FONT8x14:	/* set 8x14 dot font */
		bcopy(data, &font_8x14, sizeof(fnt14_t));
		load_font(2, 14, font_8x14);
		return 0;

	case GIO_FONT8x14:	/* get 8x14 dot font */
		bcopy(&font_8x14, data, sizeof(fnt14_t));
		return 0;

	case PIO_FONT8x16:	/* set 8x16 dot font */
		bcopy(data, &font_8x16, sizeof(fnt16_t));
		load_font(0, 16, font_8x16);
		return 0;

	case GIO_FONT8x16:	/* get 8x16 dot font */
		bcopy(&font_8x16, data, sizeof(fnt16_t));
		return 0;

	case CONSOLE_X_MODE_ON:	/* just to be compatible */
		if (saved_console < 0) {
			saved_console = get_scr_num(cur_scr_stat);
			switch_scr(minor(dev));
	 		fp = (struct syscframe *)p->p_regs;
	 		fp->sf_eflags |= PSL_IOPL;
			scp->status |= UNKNOWN_MODE;
			scp->status |= KBD_RAW_MODE;
			return 0;
		}
		return EAGAIN;

	case CONSOLE_X_MODE_OFF:/* just to be compatible */
	 	fp = (struct syscframe *)p->p_regs;
	 	fp->sf_eflags &= ~PSL_IOPL;
		load_font(0, 16, font_8x16);
		load_font(1, 8, font_8x8);
		scp->status &= ~UNKNOWN_MODE;
		set_mode(scp);
		clear_screen(scp);
		scp->status &= ~KBD_RAW_MODE;
		switch_scr(saved_console);
		saved_console = -1;
		return 0;

	case CONSOLE_X_BELL:
		/*
		 * if set, data is a pointer to a length 2 array of
		 * integers. data[0] is the pitch in Hz and data[1]
		 * is the duration in msec.
		 */
		if (data)
			sysbeep(1187500/ ((int*)data)[0],
				((int*)data)[1] * hz/ 3000);
		else
			sysbeep(0x31b, hz/4);
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


pcxint(dev_t dev)
{
	pccons[minor(dev)].t_state &= ~TS_BUSY;
	pcconsoftc.cs_timo = 0;
	if (pccons[minor(dev)].t_line)
		(*linesw[pccons[minor(dev)].t_line].l_start)
			(&pccons[minor(dev)]);
	else
		pcstart(&pccons[minor(dev)]);
}


pcstart(struct tty *tp)
{
	int c, s;
	scr_stat *scp = get_scr_stat(tp->t_dev);

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
			splx(s);
			ansi_put(scp, c);
			s = spltty();
		}
	splx(s);
}


pccnprobe(struct consdev *cp)
{
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == pcopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_tp = &pccons[0];
	cp->cn_pri = CN_INTERNAL;
}


pccninit(struct consdev *cp)
{
}


pccnputc(dev_t dev, char c)
{
	int pos;

	if (cur_scr_stat->status & UNKNOWN_MODE) 
		return;
	if (c == '\n')
		sput('\r', FG_LIGHTGREY | BG_BLACK);
	sput(c, FG_LIGHTGREY | BG_BLACK);
 	pos = cur_scr_stat->crtat - cur_scr_stat->crt_base;
	if (pos != cur_cursor_pos) {
		cur_cursor_pos = pos;
		outb(crtc_addr,14);
		outb(crtc_addr+1,pos >> 8);
		outb(crtc_addr,15);
		outb(crtc_addr+1,pos&0xff);
	}
}


pccngetc(dev_t dev)
{
	int c, s;

	s = spltty();		/* block scintr while we poll */
	c = sgetc(0);
	splx(s);
	if (c == '\r') c = '\n';
	return(c);
}

#ifndef	DONT_BLANK

void scrn_saver(int test)
{
	u_char val;
	static int blanked = 0;
	static long time_stamp;

	if (test && blank_time) {
		if (time.tv_sec > time_stamp + blank_time) {
			blanked = 1;
  			outb(TSIDX, 0x01); val = inb(TSREG); 
			outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
		}
	}
	else {
		time_stamp = time.tv_sec;
		if (blanked) {
			blanked = 0;
  			outb(TSIDX, 0x01); val = inb(TSREG); 
			outb(TSIDX, 0x01); outb(TSREG, val & 0xDF);
		}
	}
}

#else
/*
 * alternative screen saver for cards that do not like blanking
 * donated by Christoph Robitchko
 */
static 	u_short		cur_cursor_shape;	/* remember cursor shape */	

void scrn_saver(int test)
{
	const char	saves[] = {"386BSD"};
	static int	blanked = 0;
	static u_char	*savs[sizeof(saves)-1];
	static int	dirx, diry;
	static long	time_stamp;
	static u_short	save_cursor;
	int		f;
	scr_stat	*scp = cur_scr_stat;

	if (test && blank_time) {
		if (time.tv_sec > time_stamp + blank_time) {
			if (!blanked) {
				bcopy(Crtat, scp->scr, 
				      scp->max_posx * scp->max_posy * 2);
				fillw(0x07<<8 | ' ', Crtat, 
				      scp->max_posx * scp->max_posy);
				set_border(0);
				dirx = (scp->posx ? 1 : -1);
				diry = (scp->posy ? 
					scp->max_posx : -scp->max_posx);
				for (f=0; f< sizeof(saves)-1; f++)
					savs[f] = (u_char *)Crtat + 2 *
					    (scp->posx+scp->posy*scp->max_posx);
				*(savs[0]) = *saves;
				f = scp->max_posy * scp->max_posx + 5;
				outb(crtc_addr, 14);
				outb(crtc_addr+1, f >> 8);
				outb(crtc_addr, 15);
				outb(crtc_addr+1, f & 0xff);
				save_cursor = cur_cursor_shape;
				blanked = 1;
			}
			if (blanked++ < 4) 
				return;
			blanked = 1;
			*(savs[sizeof(saves)-2]) = ' ';
			for (f=sizeof(saves)-2; f > 0; f--)
				savs[f] = savs[f-1];
			f = (savs[0] - (u_char *)Crtat) / 2;
			if ((f % scp->max_posx) == 0 ||
			    (f % scp->max_posx) == scp->max_posx - 1)
				dirx = -dirx;
			if ((f / scp->max_posx) == 0 || 
			    (f / scp->max_posx) == scp->max_posy - 1)
				diry = -diry;
			savs[0] += 2*dirx + 2*diry;
			for (f=sizeof(saves)-2; f>=0; f--)
				*(savs[f]) = saves[f];
		}
	}
	else {
		time_stamp = time.tv_sec;
		if (blanked) {
			bcopy(scp->scr, Crtat, 
			      scp->max_posx * scp->max_posy * 2);
			cur_cursor_pos = -1;
			cursor_shape((save_cursor >> 8) & 0xff, 
				      save_cursor & 0xff);
			set_border(scp->border);
			blanked = 0;
		}
	}
}

#endif	/* DONT_BLANK */

static void cursor_shape(int start, int end)
{
	outb(crtc_addr, 10);
	outb(crtc_addr+1, start & 0xFF);
	outb(crtc_addr, 11);
	outb(crtc_addr+1, end & 0xFF);
#ifdef DONT_BLANK
	cur_cursor_shape = ((start & 0xff) << 8) | (end & 0xff);
#endif
}


static void cursor_pos(void)
{
	int pos = cur_scr_stat->crtat - cur_scr_stat->crt_base;

	if (cur_scr_stat->status & UNKNOWN_MODE) 
		return;
	scrn_saver(1);
	if (pos != cur_cursor_pos) {
		cur_cursor_pos = pos;
		outb(crtc_addr,14);
		outb(crtc_addr+1,pos >> 8);
		outb(crtc_addr,15);
		outb(crtc_addr+1,pos&0xff);
	}
	timeout(cursor_pos,0,hz/20);
}


static void clear_screen(scr_stat *scp)
{
	move_crsr(scp, 0, 0);
	fillw(scp->attr | ' ', scp->crt_base,
	       scp->max_posx * scp->max_posy);
}


static switch_scr(u_int next_scr)
{
	if (in_putc) {		/* don't switch if in putc */
		nx_scr = next_scr + 1;
		return 0;
	}
	if (next_scr >= NCONS || switch_in_progress) {
		sysbeep(800, hz/4);
		return -1;
	}
	switch_in_progress = 1;
	old_scp = cur_scr_stat;
	new_scp = &cons_scr_stat[next_scr];
	wakeup(&new_scp->smode);
	if (new_scp == old_scp) {
		switch_in_progress = 0;
		return 0;
	}
	new_pccons = &pccons[next_scr];
	
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
	bcopy(Crtat, old_scp->scr, old_scp->max_posx * old_scp->max_posy * 2);
	old_scp->crt_base = old_scp->scr;
	move_crsr(old_scp, old_scp->posx, old_scp->posy);
	cur_scr_stat = new_scp;
	cur_pccons = new_pccons;
	if (old_scp->status & KBD_RAW_MODE || new_scp->status & KBD_RAW_MODE)
		shfts = ctls = alts = 0;
	update_leds(new_scp->status & LOCK_KEY_MASK);
	set_mode(new_scp);
	new_scp->crt_base = Crtat;
	move_crsr(new_scp, new_scp->posx, new_scp->posy);
	bcopy(new_scp->scr, Crtat, new_scp->max_posx * new_scp->max_posy * 2);
	nx_scr = 0;
}


static void move_crsr(scr_stat *scp, u_int x, u_int y)
{
	if (x < 0 || y < 0 || x >= scp->max_posx || y >= scp->max_posy)
		return;
	scp->posx = x;
	scp->posy = y;
	scp->crtat = scp->crt_base + scp->posy * scp->max_posx + scp->posx;
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
	int i, n, m;
	u_short *src, *dst, count;

	if (scp->esc == 1) {
		switch (c) {

		case '[': 	/* Start ESC [ sequence */
			scp->esc = 2;
			scp->last_par = -1;
			for (i = scp->n_par; i < MAX_ESC_PAR; i++)
				scp->par[i] = 1;
			scp->n_par = 0;
			return;

		case 'M':	/* Move cursor up 1 line, scroll if at top */
			if (scp->posy > 0)
				move_crsr(scp, scp->posx, scp->posy - 1);
			else {
				move_up(scp->crt_base, 
					scp->crt_base + scp->max_posx,
					(scp->max_posy - 1) * scp->max_posx);
				fillw(scp->attr | ' ', 
				      scp->crt_base, scp->max_posx);
			}
			break;
#if notyet
		case 'Q':
			scp->esc = 4;
			break;
#endif
		case 'c':	/* Clear screen & home */
			clear_screen(scp);
			break;
		}
	}
	else if (scp->esc == 2) {
		if (c >= '0' && c <= '9') {
			if (scp->n_par < MAX_ESC_PAR) {
				if (scp->last_par != scp->n_par) {
					scp->last_par = scp->n_par;
					scp->par[scp->n_par] = 0;
				}
				else
					scp->par[scp->n_par] *= 10;
				scp->par[scp->n_par] += c - '0';
				return;
			}
		}
		scp->n_par = scp->last_par + 1;
		switch (c) {

		case ';':
			if (scp->n_par < MAX_ESC_PAR)
				return;
			break;

		case '=':
			scp->esc = 3;
			scp->last_par = -1;
			for (i = scp->n_par; i < MAX_ESC_PAR; i++)
				scp->par[i] = 1;
			scp->n_par = 0;
			return;

		case 'A': /* up n rows */
			n = scp->par[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->posx, scp->posy - n);
			break;

		case 'B': /* down n rows */
			n = scp->par[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->posx, scp->posy + n);
			break;

		case 'C': /* right n columns */
			n = scp->par[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->posx + n, scp->posy);
			break;

		case 'D': /* left n columns */
			n = scp->par[0]; if (n < 1) n = 1;
			move_crsr(scp, scp->posx - n, scp->posy);
			break;

		case 'E': /* cursor to start of line n lines down */
			n = scp->par[0]; if (n < 1) n = 1;
			move_crsr(scp, 0, scp->posy + n);
			break;

		case 'F': /* cursor to start of line n lines up */
			n = scp->par[0]; if (n < 1) n = 1;
			move_crsr(scp, 0, scp->posy - n);
			break;

		case 'f': /* System V consoles .. */
		case 'H': /* Cursor move */
			if (scp->n_par == 0) 
				move_crsr(scp, 0, 0);
			else if (scp->n_par == 2)
				move_crsr(scp, scp->par[1] - 1, scp->par[0] - 1);
			break;

		case 'J': /* Clear all or part of display */
			if (scp->n_par == 0)
				n = 0;
			else
				n = scp->par[0];
			switch (n) {
			case 0: /* clear form cursor to end of display */
				fillw(scp->attr | ' ', scp->crtat,
				       scp->crt_base + scp->max_posx * 
				       scp->max_posy - scp->crtat);
				break;
			case 1: /* clear from beginning of display to cursor */
				fillw(scp->attr | ' ', scp->crt_base,
				       scp->crtat - scp->crt_base);
				break;
			case 2: /* clear entire display */
				clear_screen(scp);
				break;
			}
			break;

		case 'K': /* Clear all or part of line */
			if (scp->n_par == 0)
				n = 0;
			else
				n = scp->par[0];
			switch (n) {
			case 0: /* clear form cursor to end of line */
				fillw(scp->attr | ' ', scp->crtat,
				       scp->max_posx - scp->posx);
				break;
			case 1: /* clear from beginning of line to cursor */
				fillw(scp->attr | ' ', 
				       scp->crtat - (scp->max_posx - scp->posx),
				       (scp->max_posx - scp->posx) + 1);
				break;
			case 2: /* clear entire line */
				fillw(scp->attr | ' ', 
				       scp->crtat - (scp->max_posx - scp->posx),
				       scp->max_posx);
				break;
			}
			break;

		case 'L':	/* Insert n lines */
			n = scp->par[0]; if (n < 1) n = 1;
			if (n > scp->max_posy - scp->posy)
				n = scp->max_posy - scp->posy;
			src = scp->crt_base + scp->posy * scp->max_posx;
			dst = src + n * scp->max_posx;
			count = scp->max_posy - (scp->posy + n);
			move_up(src, dst, count * scp->max_posx);
			fillw(scp->attr | ' ', src, n * scp->max_posx);
			break;

		case 'M':	/* Delete n lines */
			n = scp->par[0]; if (n < 1) n = 1;
			if (n > scp->max_posy - scp->posy)
				n = scp->max_posy - scp->posy;
			dst = scp->crt_base + scp->posy * scp->max_posx;
			src = dst + n * scp->max_posx;
			count = scp->max_posy - (scp->posy + n);
			move_down(src, dst, count * scp->max_posx);
			src = dst + count * scp->max_posx;
			fillw(scp->attr | ' ', src, n * scp->max_posx);
			break;

		case 'P':	/* Delete n chars */
			n = scp->par[0]; if (n < 1) n = 1;
			if (n > scp->max_posx - scp->posx)
				n = scp->max_posx - scp->posx;
			dst = scp->crtat;
			src = dst + n;
			count = scp->max_posx - (scp->posx + n);
			move_down(src, dst, count);
			src = dst + count;
			fillw(scp->attr | ' ', src, n);
			break;

		case '@':	/* Insert n chars */
			n = scp->par[0]; if (n < 1) n = 1;
			if (n > scp->max_posx - scp->posx)
				n = scp->max_posx - scp->posx;
			src = scp->crtat;
			dst = src + n;
			count = scp->max_posx - (scp->posx + n);
			move_up(src, dst, count);
			fillw(scp->attr | ' ', src, n);
			break;

		case 'S':	/* scroll up n lines */
			n = scp->par[0]; if (n < 1)  n = 1;
			bcopy(scp->crt_base + (scp->max_posx * n),
			       scp->crt_base, 
			       scp->max_posx * (scp->max_posy - n) * 
			       sizeof(u_short));
			fillw(scp->attr | ' ',
			       scp->crt_base + scp->max_posx * 
			       (scp->max_posy - 1), 
			       scp->max_posx);
			break;

		case 'T':	/* scroll down n lines */
			n = scp->par[0]; if (n < 1)  n = 1;
			bcopy(scp->crt_base, 
			       scp->crt_base + (scp->max_posx * n),
			       scp->max_posx * (scp->max_posy - n) * 
			       sizeof(u_short));
			fillw(scp->attr | ' ', scp->crt_base,  scp->max_posx);
			break;

		case 'X':	/* delete n characters in line */
			n = scp->par[0]; if (n < 1)  n = 1;
			fillw(scp->attr | ' ', scp->crt_base + scp->posx + 
			      ((scp->max_posx*scp->posy) * sizeof(u_short)), n);
			break;

		case 'Z':	/* move n tabs backwards */
			n = scp->par[0]; if (n < 1)  n = 1;
			if ((i = scp->posx & 0xf8) == scp->posx)
				i -= 8*n;
			else 
				i -= 8*(n-1); 
			if (i < 0) 
				i = 0;
			move_crsr(scp, i, scp->posy);
			break;

		case '`': 	/* move cursor to column n */
			n = scp->par[0]; if (n < 1)  n = 1;
			move_crsr(scp, n, scp->posy);
			break;

		case 'a': 	/* move cursor n columns to the right */
			n = scp->par[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->posx + n, scp->posy);
			break;

		case 'd': 	/* move cursor to row n */
			n = scp->par[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->posx, n);
			break;

		case 'e': 	/* move cursor n rows down */
			n = scp->par[0]; if (n < 1)  n = 1;
			move_crsr(scp, scp->posx, scp->posy + n);
			break;

		case 'm': 	/* change attribute */
			if (scp->n_par == 0)
				n = 0;
			else
				n = scp->par[0];
			switch (n) {
			case 0:	/* back to normal */
				scp->attr = scp->std_attr;
				break;
			case 1:	/* highlight (bold) */
				scp->attr &= 0xFF00;
				scp->attr |= 0x0800;
				break;
			case 4: /* highlight (underline) */
				scp->attr &= 0x0F00;
				scp->attr |= 0x0800;
				break;
			case 5: /* blink */
				scp->attr &= 0xFF00;
				scp->attr |= 0x8000;
				break;
			case 7: /* reverse video */
				scp->attr = scp->rev_attr; 
				break;
			case 30: case 31: case 32: case 33: /* set fg color */
			case 34: case 35: case 36: case 37:
				scp->attr = (scp->attr & 0xF0FF)
					    | (ansi_col[(n - 30) & 7] << 8);
				break;
			case 40: case 41: case 42: case 43: /* set bg color */
			case 44: case 45: case 46: case 47:
				scp->attr = (scp->attr & 0x0FFF)
					    | (ansi_col[(n - 40) & 7] << 12);
				break;
			}
			break;

		case 'x':
			if (scp->n_par == 0)
				n = 0;
			else
				n = scp->par[0];
			switch (n) {
			case 0: 	/* reset attributes */
				scp->attr = scp->std_attr = FG_LIGHTGREY << 8;
				break;
			case 1: 	/* set ansi background */
				scp->attr = scp->std_attr =  
					(scp->std_attr & 0x0F00) 
					| (ansi_col[(scp->par[1])&0x0F] << 12);
				break;
			case 2: 	/* set ansi foreground */
				scp->attr = scp->std_attr =  
					(scp->std_attr & 0xF000) 
					| (ansi_col[(scp->par[1])&0x0F] << 8);
				break;
			case 3: 	/* set ansi attribute directly */
				scp->attr = scp->std_attr =
					(scp->par[1] & 0xFF) << 8;
				break;
			case 5: 	/* set ansi reverse video background */
				scp->rev_attr = 
					(scp->rev_attr & 0x0F00) 
					| (ansi_col[(scp->par[1]) & 0x0F] << 12);
				break;
			case 6: 	/* set ansi reverse video foreground */
				scp->rev_attr = 
					(scp->rev_attr & 0xF000) 
					| (ansi_col[(scp->par[1]) & 0x0F] << 8);
				break;
			case 7: 	/* set ansi reverse video directly */
				scp->rev_attr = (scp->par[1] & 0xFF) << 8;
				break;
			}
			break;

		case 'z':	/* switch to (virtual) console n */
			if (scp->n_par == 1)
				switch_scr(scp->par[0]);
			break;
		}
	}
	else if (scp->esc == 3) {
		if (c >= '0' && c <= '9') {
			if (scp->n_par < MAX_ESC_PAR) {
				if (scp->last_par != scp->n_par) {
					scp->last_par = scp->n_par;
					scp->par[scp->n_par] = 0;
				}
				else
					scp->par[scp->n_par] *= 10;
				scp->par[scp->n_par] += c - '0';
				return;
			}
		}
		scp->n_par = scp->last_par + 1;
		switch (c) {

		case ';':
			if (scp->n_par < MAX_ESC_PAR)
				return;
			break;

		case 'A':	/* set display border color */
			if (scp->n_par == 1)
				scp->border=scp->par[0] & 0xff;
				if (scp == cur_scr_stat)
					set_border(scp->border);
			break;

		case 'B':	/* set bell pitch and duration */
			if (scp->n_par == 2) {
				scp->bell_pitch = scp->par[0];
				scp->bell_duration = scp->par[1]*10;
			}
			break;

		case 'C': 	/* set cursor shape (start & end line) */
			if (scp->n_par == 2) 
				cursor_shape(scp->par[0], scp->par[1]);
			break;

		case 'F':	/* set ansi foreground */
			if (scp->n_par == 1) 
				scp->attr = scp->std_attr =  
					(scp->std_attr & 0xF000) 
					| ((scp->par[0] & 0x0F) << 8);
			break;

		case 'G': 	/* set ansi background */
			if (scp->n_par == 1) 
				scp->attr = scp->std_attr =  
					(scp->std_attr & 0x0F00) 
					| ((scp->par[0] & 0x0F) << 12);
			break;

		case 'H':	/* set ansi reverse video foreground */
			if (scp->n_par == 1) 
				scp->rev_attr = 
					(scp->rev_attr & 0xF000) 
					| ((scp->par[0] & 0x0F) << 8);
			break;

		case 'I': 	/* set ansi reverse video background */
			if (scp->n_par == 1) 
				scp->rev_attr = 
					(scp->rev_attr & 0x0F00) 
					| ((scp->par[0] & 0x0F) << 12);
			break;
		}
	}
	scp->esc = 0;
}


#define wrtchar(scp, c) ( *scp->crtat = (c), scp->crtat++, scp->posx++ )

static void ansi_put(scr_stat *scp, u_char c)
{
	int s;

	if (scp == cur_scr_stat )
		scrn_saver(0);
	if (scp->status & UNKNOWN_MODE) 
		return;
	in_putc++;
	if (scp->esc)
		scan_esc(scp, c);
	else switch(c) {
	case 0x1B:
		scp->esc = 1;
		scp->n_par = 0;
		break;
	case '\t':
		do {
			wrtchar(scp, scp->attr | ' ');
		} while (scp->posx % 8);
		break;
	case '\b':      /* non-destructive backspace */
		if (scp->crtat > scp->crt_base) {
			scp->crtat--;
			if (scp->posx > 0)
				scp->posx--;
			else {
				scp->posx += scp->max_posx - 1;
				scp->posy--;
			}
		}
		break;
	case '\r':
		move_crsr(scp, 0, scp->posy);
		break;
	case '\n':
		scp->crtat += scp->max_posx;
		scp->posy++;
		break;
	case '\f':
		clear_screen(scp);
		break;
	default:
		if (c == 7) {
			if (scp == cur_scr_stat)
			 	sysbeep(scp->bell_pitch, scp->bell_duration);
		}
		/* Print only printables */
		else {
			wrtchar(scp, scp->attr | c);
			if (scp->posx >= scp->max_posx) {
				scp->posx = 0;
				scp->posy++;
			}
			break;
		}
	}
	if (scp->crtat >= scp->crt_base + scp->max_posy * scp->max_posx) {
		bcopy(scp->crt_base + scp->max_posx, scp->crt_base,
			scp->max_posx * (scp->max_posy - 1) * sizeof(u_short));
		fillw(scp->attr | ' ',
			scp->crt_base + scp->max_posx * (scp->max_posy - 1), 
			scp->max_posx);
		scp->crtat -= scp->max_posx;
		scp->posy--;
	}
	s = spltty();
	in_putc--;
	splx(s);
	if (nx_scr)
		switch_scr(nx_scr - 1);
}


void consinit(void)
{
	u_short *cp = Crtat + (CGA_BUF-MONO_BUF)/sizeof(u_short), was;
	scr_stat *scp;
	unsigned cursorat;
	int i;

	/*
	 * catch that once in a blue moon occurence when consinit is called 
	 * TWICE, adding the CGA_BUF offset again -> poooff
	 * thanks to Christoph Robitchko for finding this one !!
	 */
	if (crtat != 0) 	
		return;
	/*
	 * Crtat initialized to point to MONO buffer, if not present change
	 * to CGA_BUF offset. ONLY ADD the difference since locore.s adds
	 * in the remapped offset at the right time
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

	crtat = Crtat+ cursorat;

	cons_scr_stat[0].crtat = crtat;
	cons_scr_stat[0].crt_base = Crtat;
	cons_scr_stat[0].posx = cursorat % COL;
	cons_scr_stat[0].posy = cursorat / COL;
	cons_scr_stat[0].esc = 0;
	cons_scr_stat[0].std_attr = (FG_LIGHTGREY | BG_BLACK) << 8;
	cons_scr_stat[0].rev_attr = (FG_BLACK | BG_LIGHTGREY) << 8;
	cons_scr_stat[0].attr = (FG_LIGHTGREY | BG_BLACK) << 8;
	cons_scr_stat[0].border = BG_BLACK;;
	cons_scr_stat[0].max_posx = COL;
	cons_scr_stat[0].max_posy = ROW;
	cons_scr_stat[0].status = 0;
	cons_scr_stat[0].pid = 0;
	cons_scr_stat[0].proc = NULL;
	cons_scr_stat[0].smode.mode = VT_AUTO;
	cons_scr_stat[0].bell_pitch = 800;
	cons_scr_stat[0].bell_duration = 10;
	clear_screen(&cons_scr_stat[0]);
}


static void sput(u_char c, u_char ca)
{
	int i;
	scr_stat *scp = &cons_scr_stat[0];

	if (crtat == 0)
		consinit();
	i = scp->attr;
	scp->attr = ca << 8;
	ansi_put(scp, c);
	scp->attr = i;
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


static update_leds(int which)
{
	u_char xlate_leds[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
	int i;
	
	kbd_cmd(KB_SETLEDS);		/* LED Command */
	kbd_cmd(xlate_leds[which]);
	kbd_wait();
}


volatile void reset_cpu(void)
{
	int i;

	while (1) {
		kbd_cmd(KB_RESET_CPU);	/* Reset Command */
		DELAY(4000000);
		kbd_cmd(KB_RESET);	/* Keyboard Reset Command */
	}
}


/*
 * sgetc(noblock) : get a character from the keyboard. 
 * If noblock = 0 wait until a key is gotten.  Otherwise return a 0x100.
 */
u_int sgetc(int noblock)
{
	u_char dt, modifier;
	static u_char esc_flag = 0;
	u_int state, action;
	int i;
	struct key_t *key;

next_code:
	kbd_wait();
	/* First see if there is something in the keyboard port */
	if (inb(KB_STAT) & KB_BUF_FULL)
		dt = inb(KB_DATA);
	else if (noblock)
		return(0x100);
	else
		goto next_code;

	if (cur_scr_stat->status & KBD_RAW_MODE)
		return dt;

	/* Check for cntl-alt-del */
	if ((dt == 83) && ctls && alts)
		cpu_reset();

#if NDDB > 0
	/* Check for cntl-alt-esc */
	if ((dt == 1) && ctls && alts) {
		/* if debugger called, try to switch to console 0 */
		if (cur_scr_stat->smode.mode == VT_AUTO &&
		    cons_scr_stat[0].smode.mode == VT_AUTO)
			switch_scr(0); 
		Debugger();
	}
#endif
	if (dt == 0xE0 || dt == 0xE1) {
		esc_flag = dt;
		goto next_code;
	}

	if ((dt & 0x7F) >= key_map.n_keys)
		goto next_code;

	if (esc_flag == 0xE0) {
		switch (dt & 0x7F) {
		case 0x2A:	/* This may come because the keyboard keeps */
		case 0x36: 	/* its own caps lock status, we ignore  SOS */
			goto next_code;
			/* NOT REACHED */
		case 0x1C:	/* keypad enter key */
			modifier = 0x59;
			break;
		case 0x1D:	/* right control key */
			modifier = 0x5a;
			break;
		case 0x35:	/* keypad divide key */
			modifier = 0x5b;
			break;
		case 0x37:	/* print scrn key */
			modifier = 0x5c;
			break;
		case 0x38:	/* right alt key (alt gr) */
			modifier = 0x5d;
			break;
		case 0x47:	/* grey home key */
			modifier = 0x5e;
			break;
		case 0x48:	/* grey up arrow key */
			modifier = 0x5f;
			break;
		case 0x49:	/* grey page up key */
			modifier = 0x60;
			break;
		case 0x4b:	/* grey left arrow key */
			modifier = 0x61;
			break;
		case 0x4d:	/* grey right arrow key */
			modifier = 0x62;
			break;
		case 0x4f:	/* grey end key */
			modifier = 0x63;
			break;
		case 0x50:	/* grey down arrow key */
			modifier = 0x64;
			break;
		case 0x51:	/* grey page down key */
			modifier = 0x65;
			break;
		case 0x52:	/* grey insert key */
			modifier = 0x66;
			break;
		case 0x53:	/* grey delete key */
			modifier = 0x67;
			break;
		default:	/* every thing else is ignored */
			goto next_code;
			/* NOT REACHED */
		}
		dt = (dt & 0x80) | modifier;
	}
	else if (esc_flag == (u_char)0xE1 && ((dt & 0x7F) == 0x1D)) {
		esc_flag = 0x1D;
		goto next_code;
	}
	else if (esc_flag == 0x1D && ((dt & 0x7F) == 0x45)) 
		dt = (dt & 0x80) | 0x68;
	esc_flag = 0;

	state = (shfts ? 1 : 0 ) | (2 * (ctls ? 1 : 0)) | (4 * (alts ? 1 : 0));
	key = &key_map.key[dt & 0x7F];
	if ( ((key->flgs & FLAG_LOCK_C) && (cur_scr_stat->status & CLKED))
	     || ((key->flgs & FLAG_LOCK_N) && (cur_scr_stat->status & NLKED)) )
		state ^= 1;

	/* Check for make/break */
	action = key->map[state];
	if (dt & 0x80) {
		/* break */
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
			}
		}
	} else {
		/* make */
		if (key->spcl & (0x80>>state)) {
			switch (action) {
			/* LOCKING KEYS */
			case NLK:
				if (cur_scr_stat->status & NLKED) 
					cur_scr_stat->status &= ~NLKED;
				else
					cur_scr_stat->status |= NLKED;
				update_leds(cur_scr_stat->status&LOCK_KEY_MASK); 
				break;
			case CLK:
				if (cur_scr_stat->status & CLKED)
					cur_scr_stat->status &= ~CLKED;
				else
					cur_scr_stat->status |= CLKED;
				update_leds(cur_scr_stat->status&LOCK_KEY_MASK);
				break;
			case SLK:
				if (cur_scr_stat->status & SLKED) {
					cur_scr_stat->status &= ~SLKED;
					pcstart(&pccons[get_scr_num(cur_scr_stat)]);
				} 
				else 
					cur_scr_stat->status |= SLKED;
				update_leds(cur_scr_stat->status&LOCK_KEY_MASK);
				break;
	
			/* NON-LOCKING KEYS */
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
			default:
				if (action >= F_SCR && action <= L_SCR) {
					switch_scr(action - F_SCR);
					break;
				}
				if (action >= F_FN && action <= L_FN) {
					return(action | FKEY);
				}
				return(action);
			}
		}
		else  return(action);
	}
	goto next_code;
}

/* -hv- 22-Apr-93: to make init_main more portable */
void cons_highlight(void) 
{
	scr_stat *scp = &cons_scr_stat[0];

	scp->attr &= 0xFF00;
	scp->attr |= 0x0800;
}

void cons_normal(void) 
{
	scr_stat *scp = &cons_scr_stat[0];

	scp->attr = scp->std_attr;
}

int getchar(void)
{
	char thechar;
	int delay, x;

	pcconsoftc.cs_flags |= CSF_POLLING;
	x = splhigh();
	sput('>', FG_RED | BG_BLACK);
	thechar = (char) sgetc(0);
	pcconsoftc.cs_flags &= ~CSF_POLLING;
	splx(x);
	switch (thechar) {
	default: 
		if (thechar >= ' ')
			sput(thechar, FG_RED | BG_BLACK);
		return(thechar);
	case cr:
	case lf: 
		sput(cr, FG_RED | BG_BLACK);
		sput(lf, FG_RED | BG_BLACK);
		return(lf);
	case bs:
	case del:
		sput(bs, FG_RED | BG_BLACK);
		sput(' ', FG_RED | BG_BLACK);
		sput(bs, FG_RED | BG_BLACK);
		return(thechar);
	case cntld:
		sput('^', FG_RED | BG_BLACK) ;
	 	sput('D', FG_RED | BG_BLACK) ; 
		sput('\r', FG_RED | BG_BLACK) ; 
		sput('\n', FG_RED | BG_BLACK) ;
		return(0);
	}
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
	for (i=0; i<10000; i++)
		if ((inb(KB_STAT) & KB_READY) == 0) 
			break;
}


static void kbd_cmd(u_char command)
{
	kbd_wait();
	outb(KB_DATA, command);
}


static void set_mode(scr_stat *scp)
{
	u_char byte;

	if (scp != cur_scr_stat)
		return;

	/* setup video hardware for the given mode */
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
		return;
	}

	/* set border color for this (virtual) console */
	set_border(scp->border);

	/* (re)activate cursor */
	untimeout(cursor_pos, 0);
	cursor_pos();
}


static void set_border(int color)
{
	inb(crtc_addr+6); 				/* reset flip-flop */
	outb(ATC, 0x11); outb(ATC, color); 
 	inb(crtc_addr+6); 				/* reset flip-flop */
 	outb(ATC, 0x20);			/* enable Palette */
}

static load_font(int segment, int size, char* font)
{
  	int ch, line;
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
			*((char *)atdevbase+(segment*0x4000)+(ch*32)+line) = 
				font[(ch*size)+line];	
	/* setup vga for text mode again */
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


static change_winsize(struct tty *tp, int x, int y)
{
	if (tp->t_winsize.ws_col != x || tp->t_winsize.ws_row != y) {
		tp->t_winsize.ws_col = x;
		tp->t_winsize.ws_row = y;
		pgsignal(tp->t_pgrp, SIGWINCH, 1);
	}
}

#endif /* NSC */
