/*-
 * Copyright (c) 1992-1998 Sen Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *  $Id: syscons.c,v 1.120 1999/05/31 11:28:41 phk Exp $
 */

#include "sc.h"
#include "splash.h"
#include "apm.h"
#include "opt_ddb.h"
#include "opt_devfs.h"
#include "opt_syscons.h"

#if NSC > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#ifdef	DEVFS
#include <sys/devfsext.h>
#endif

#include <machine/bootinfo.h>
#include <machine/clock.h>
#include <machine/cons.h>
#include <machine/console.h>
#include <machine/mouse.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/pc/display.h>
#include <machine/pc/vesa.h>
#include <machine/apm_bios.h>
#include <machine/random.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/kbd/kbdreg.h>
#include <dev/fb/fbreg.h>
#include <dev/fb/splashreg.h>

#ifdef PC98
#define KANJI
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/timerreg.h>
#include <pc98/pc98/syscons.h>
#include <isa/isavar.h>
#else
#include <dev/fb/vgareg.h>
#include <dev/syscons/syscons.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/timerreg.h>
#endif /* PC98 */

#if !defined(MAXCONS)
#define MAXCONS 16
#endif

#if !defined(SC_MAX_HISTORY_SIZE)
#define SC_MAX_HISTORY_SIZE	(1000 * MAXCONS)
#endif

#if !defined(SC_HISTORY_SIZE)
#define SC_HISTORY_SIZE		(ROW * 4)
#endif

#if (SC_HISTORY_SIZE * MAXCONS) > SC_MAX_HISTORY_SIZE
#undef SC_MAX_HISTORY_SIZE
#define SC_MAX_HISTORY_SIZE	(SC_HISTORY_SIZE * MAXCONS)
#endif

#if !defined(SC_MOUSE_CHAR)
#define SC_MOUSE_CHAR		(0xd0)
#endif

#define COLD 0
#define WARM 1

#define DEFAULT_BLANKTIME	(5*60)		/* 5 minutes */
#define MAX_BLANKTIME		(7*24*60*60)	/* 7 days!? */

/* for backward compatibility */
#define OLD_CONS_MOUSECTL	_IOWR('c', 10, old_mouse_info_t)

typedef struct old_mouse_data {
    int x;
    int y;
    int buttons;
} old_mouse_data_t;

typedef struct old_mouse_info {
    int operation;
    union {
	struct old_mouse_data data;
	struct mouse_mode mode;
    } u;
} old_mouse_info_t;

static default_attr user_default = {
    (FG_LIGHTGREY | BG_BLACK) << 8,
    (FG_BLACK | BG_LIGHTGREY) << 8
};

#ifdef PC98
static default_attr kernel_default = {
    (FG_LIGHTGREY | BG_BLACK) << 8,
    (FG_BLACK | BG_LIGHTGREY) << 8
};
#else
static default_attr kernel_default = {
    (FG_WHITE | BG_BLACK) << 8,
    (FG_BLACK | BG_LIGHTGREY) << 8
};
#endif /* PC98 */

static  scr_stat    	main_console;
static  scr_stat    	*console[MAXCONS];
#ifdef DEVFS
static	void		*sc_devfs_token[MAXCONS];
static	void		*sc_mouse_devfs_token;
static	void		*sc_console_devfs_token;
#endif
	scr_stat    	*cur_console;
static  scr_stat    	*new_scp, *old_scp;
static  term_stat   	kernel_console;
static  default_attr    *current_default;
static  int		sc_flags;
static  char        	init_done = COLD;
static  u_short		sc_buffer[ROW*COL];
static  char		shutdown_in_progress = FALSE;
static  char        	font_loading_in_progress = FALSE;
static  char        	switch_in_progress = FALSE;
static  char        	write_in_progress = FALSE;
static  char        	blink_in_progress = FALSE;
static  int        	blinkrate = 0;
static	int		adapter = -1;
static	int		keyboard = -1;
static	keyboard_t	*kbd;
static  int     	delayed_next_scr = FALSE;
static  long        	scrn_blank_time = 0;    /* screen saver timeout value */
static	int     	scrn_blanked = FALSE;	/* screen saver active flag */
static  long		scrn_time_stamp;
static	int		saver_mode = CONS_LKM_SAVER; /* LKM/user saver */
static	int		run_scrn_saver = FALSE;	/* should run the saver? */
static	int		scrn_idle = FALSE;	/* about to run the saver */
static	int		scrn_saver_failed;
	u_char      	scr_map[256];
	u_char      	scr_rmap[256];
static	int		initial_video_mode;	/* initial video mode # */
	int     	fonts_loaded = 0
#ifdef STD8X16FONT
	| FONT_16
#endif
	;

	u_char		font_8[256*8];
	u_char		font_14[256*14];
#ifdef STD8X16FONT
extern
#endif
	u_char		font_16[256*16];
	u_char        	palette[256*3];
static	u_char 		*cut_buffer;
static	int		cut_buffer_size;
static	int		mouse_level;		/* sysmouse protocol level */
static	mousestatus_t	mouse_status = { 0, 0, 0, 0, 0, 0 };
#ifndef PC98
static  u_short 	mouse_and_mask[16] = {
				0xc000, 0xe000, 0xf000, 0xf800,
				0xfc00, 0xfe00, 0xff00, 0xff80,
				0xfe00, 0x1e00, 0x1f00, 0x0f00,
				0x0f00, 0x0000, 0x0000, 0x0000
			};
static  u_short 	mouse_or_mask[16] = {
				0x0000, 0x4000, 0x6000, 0x7000,
				0x7800, 0x7c00, 0x7e00, 0x6800,
				0x0c00, 0x0c00, 0x0600, 0x0600,
				0x0000, 0x0000, 0x0000, 0x0000
			};
#endif

	int		sc_history_size = SC_HISTORY_SIZE;
static	int		extra_history_size = 
			    SC_MAX_HISTORY_SIZE - SC_HISTORY_SIZE * MAXCONS;

static void    		none_saver(int blank) { }
static void    		(*current_saver)(int blank) = none_saver;
       d_ioctl_t  	*sc_user_ioctl;

static int		sticky_splash = FALSE;
static struct 		{
			    u_int8_t	cursor_start;
			    u_int8_t	cursor_end;
			    u_int8_t	shift_state;
			} bios_value;

/* OS specific stuff */
#ifdef not_yet_done
#define VIRTUAL_TTY(x)  (sccons[x] = ttymalloc(sccons[x]))
struct  CONSOLE_TTY 	(sccons[MAXCONS] = ttymalloc(sccons[MAXCONS]))
struct  MOUSE_TTY 	(sccons[MAXCONS+1] = ttymalloc(sccons[MAXCONS+1]))
struct  tty         	*sccons[MAXCONS+2];
#else
#define VIRTUAL_TTY(x)  &sccons[x]
#define CONSOLE_TTY 	&sccons[MAXCONS]
#define MOUSE_TTY 	&sccons[MAXCONS+1]
static struct tty     	sccons[MAXCONS+2];
#endif
#define SC_MOUSE 	128
#define SC_CONSOLE	255
u_short         	*Crtat;
#ifdef PC98
u_short                 *Atrat;
static u_char		default_kanji = UJIS;
#endif
static const int	nsccons = MAXCONS+2;

#define WRAPHIST(scp, pointer, offset)\
    ((scp)->history + ((((pointer) - (scp)->history) + (scp)->history_size \
    + (offset)) % (scp)->history_size))
#ifdef PC98
#define WRAPHIST_A(scp, pointer, offset)\
    ((scp->his_atr) + ((((pointer) - (scp->his_atr)) + (scp)->history_size \
    + (offset)) % (scp)->history_size))
#endif
#define ISSIGVALID(sig)	((sig) > 0 && (sig) < NSIG)

/* some useful macros */
#define kbd_read_char(kbd, wait)					\
		(*kbdsw[(kbd)->kb_index]->read_char)((kbd), (wait))
#define kbd_check_char(kbd)						\
		(*kbdsw[(kbd)->kb_index]->check_char)((kbd))
#define kbd_enable(kbd)							\
		(*kbdsw[(kbd)->kb_index]->enable)((kbd))
#define kbd_disable(kbd)						\
		(*kbdsw[(kbd)->kb_index]->disable)((kbd))
#define kbd_lock(kbd, lockf)						\
		(*kbdsw[(kbd)->kb_index]->lock)((kbd), (lockf))
#define kbd_ioctl(kbd, cmd, arg)					\
	    (((kbd) == NULL) ?						\
		ENODEV : (*kbdsw[(kbd)->kb_index]->ioctl)((kbd), (cmd), (arg)))
#define kbd_clear_state(kbd)						\
		(*kbdsw[(kbd)->kb_index]->clear_state)((kbd))
#define kbd_get_fkeystr(kbd, fkey, len)					\
		(*kbdsw[(kbd)->kb_index]->get_fkeystr)((kbd), (fkey), (len))
#define kbd_poll(kbd, on)						\
		(*kbdsw[(kbd)->kb_index]->poll)((kbd), (on))

/* prototypes */
static int scattach(device_t dev);
static kbd_callback_func_t sckbdevent;
static int scparam(struct tty *tp, struct termios *t);
static int scprobe(device_t dev);
static int scvidprobe(int unit, int flags, int cons);
static int sckbdprobe(int unit, int flags, int cons);
static void scstart(struct tty *tp);
static void scmousestart(struct tty *tp);
static void scinit(void);
static void scshutdown(int howto, void *arg);
static u_int scgetc(keyboard_t *kbd, u_int flags);
#define SCGETC_CN	1
#define SCGETC_NONBLOCK	2
static int sccngetch(int flags);
static void sccnupdate(scr_stat *scp);
static scr_stat *alloc_scp(void);
static void init_scp(scr_stat *scp);
static void get_bios_values(void);
static void sc_bcopy(scr_stat *scp, u_short *p, int from, int to, int mark);
static int get_scr_num(void);
static timeout_t scrn_timer;
static void scrn_update(scr_stat *scp, int show_cursor);
#if NSPLASH > 0
static int scsplash_callback(int);
static void scsplash_saver(int show);
static int add_scrn_saver(void (*this_saver)(int));
static int remove_scrn_saver(void (*this_saver)(int));
static int set_scrn_saver_mode(scr_stat *scp, int mode, u_char *pal, int border);
static int restore_scrn_saver_mode(scr_stat *scp, int changemode);
static void stop_scrn_saver(void (*saver)(int));
static int wait_scrn_saver_stop(void);
#define scsplash_stick(stick)		(sticky_splash = (stick))
#else
#define scsplash_stick(stick)
#endif /* NSPLASH */
static int switch_scr(scr_stat *scp, u_int next_scr);
static void exchange_scr(void);
static void scan_esc(scr_stat *scp, u_char c);
static void ansi_put(scr_stat *scp, u_char *buf, int len);
static void draw_cursor_image(scr_stat *scp); 
static void remove_cursor_image(scr_stat *scp); 
static void move_crsr(scr_stat *scp, int x, int y);
static void history_to_screen(scr_stat *scp);
static int history_up_line(scr_stat *scp);
static int history_down_line(scr_stat *scp);
static int mask2attr(struct term_stat *term);
static int save_kbd_state(scr_stat *scp);
static int update_kbd_state(int state, int mask);
static int update_kbd_leds(int which);
static void set_destructive_cursor(scr_stat *scp);
static void set_mouse_pos(scr_stat *scp);
static int skip_spc_right(scr_stat *scp, u_short *p);
static int skip_spc_left(scr_stat *scp, u_short *p);
static void mouse_cut(scr_stat *scp);
static void mouse_cut_start(scr_stat *scp);
static void mouse_cut_end(scr_stat *scp);
static void mouse_cut_word(scr_stat *scp);
static void mouse_cut_line(scr_stat *scp);
static void mouse_cut_extend(scr_stat *scp);
static void mouse_paste(scr_stat *scp);
static void draw_mouse_image(scr_stat *scp); 
static void remove_mouse_image(scr_stat *scp); 
static void draw_cutmarking(scr_stat *scp); 
static void remove_cutmarking(scr_stat *scp); 
static void do_bell(scr_stat *scp, int pitch, int duration);
static timeout_t blink_screen;

static cn_probe_t	sccnprobe;
static cn_init_t	sccninit;
static cn_getc_t	sccngetc;
static cn_checkc_t	sccncheckc;
static cn_putc_t	sccnputc;

CONS_DRIVER(sc, sccnprobe, sccninit, sccngetc, sccncheckc, sccnputc);

devclass_t	sc_devclass;

static device_method_t sc_methods[] = {
	DEVMETHOD(device_probe,         scprobe),
	DEVMETHOD(device_attach,        scattach),
	{ 0, 0 }
};

static driver_t sc_driver = {
	"sc",
	sc_methods,
	1,                          /* XXX */
};

DRIVER_MODULE(sc, isa, sc_driver, sc_devclass, 0, 0);

static	d_open_t	scopen;
static	d_close_t	scclose;
static	d_read_t	scread;
static	d_write_t	scwrite;
static	d_ioctl_t	scioctl;
static	d_mmap_t	scmmap;

#define	CDEV_MAJOR	12
static struct cdevsw sc_cdevsw = {
	/* open */	scopen,
	/* close */	scclose,
	/* read */	scread,
	/* write */	scwrite,
	/* ioctl */	scioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	scdevtotty,
	/* poll */	ttpoll,
	/* mmap */	scmmap,
	/* strategy */	nostrategy,
	/* name */	"sc",
	/* parms */	noparms,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
	/* maxio */	0,
	/* bmaj */	-1
};

#ifdef PC98
static u_char	ibmpc_to_pc98[16] =
 { 0x01,0x21,0x81,0xa1,0x41,0x61,0xc1,0xe1, 0x09,0x29,0x89,0xa9,0x49,0x69,0xc9,0xe9 };
static u_char	ibmpc_to_pc98rev[16] = 
 { 0x05,0x25,0x85,0xa5,0x45,0x65,0xc5,0xe5, 0x0d,0x2d,0x8d,0xad,0x4d,0x6d,0xcd,0xed };

unsigned int
at2pc98(unsigned int attr)
{
    unsigned char fg_at, bg_at;
    unsigned int at;

    fg_at = ((attr >> 8) & 0x0F);
    bg_at = ((attr >> 8) & 0xF0);

    if (bg_at) {
	if (bg_at & 0x80) {
	    if (bg_at & 0x70) {
		/* reverse & blink */
		at = ibmpc_to_pc98rev[bg_at >> 4] | 0x02;
	    } else {
		/* normal & blink */
		at = ibmpc_to_pc98[fg_at] | 0x02;
	    }
	} else {
	    /* reverse */
	    at = ibmpc_to_pc98rev[bg_at >> 4];
	}
    } else {
	/* normal */
	at = ibmpc_to_pc98[fg_at];
    }
    at |= ((fg_at|bg_at) << 8);
    return (at);
}
#endif

static void
draw_cursor_image(scr_stat *scp)
{
#ifdef PC98
    (*vidsw[scp->ad]->set_hw_cursor)(scp->adp, scp->xpos, scp->ypos);
#else
    u_short cursor_image;
    u_short *ptr;
    u_short prev_image;

    if (ISPIXELSC(scp)) {
	sc_bcopy(scp, scp->scr_buf, scp->cursor_pos - scp->scr_buf, 
	  scp->cursor_pos - scp->scr_buf, 1);
	return;
    }

    ptr = (u_short *)(scp->adp->va_window)
			 + (scp->cursor_pos - scp->scr_buf);

    /* do we have a destructive cursor ? */
    if (sc_flags & CHAR_CURSOR) {
	prev_image = scp->cursor_saveunder;
	cursor_image = *ptr & 0x00ff;
	if (cursor_image == DEAD_CHAR) 
	    cursor_image = prev_image & 0x00ff;
	cursor_image |= *(scp->cursor_pos) & 0xff00;
	scp->cursor_saveunder = cursor_image;
	/* update the cursor bitmap if the char under the cursor has changed */
	if (prev_image != cursor_image) 
	    set_destructive_cursor(scp);
	/* modify cursor_image */
	if (!(sc_flags & BLINK_CURSOR)||((sc_flags & BLINK_CURSOR)&&(blinkrate & 4))){
	    /* 
	     * When the mouse pointer is at the same position as the cursor,
	     * the cursor bitmap needs to be updated even if the char under 
	     * the cursor hasn't changed, because the mouse pionter may 
	     * have moved by a few dots within the cursor cel.
	     */
	    if ((prev_image == cursor_image) 
		    && (cursor_image != *(scp->cursor_pos)))
	        set_destructive_cursor(scp);
	    cursor_image &= 0xff00;
	    cursor_image |= DEAD_CHAR;
	}
    } else {
	cursor_image = (*(ptr) & 0x00ff) | *(scp->cursor_pos) & 0xff00;
	scp->cursor_saveunder = cursor_image;
	if (!(sc_flags & BLINK_CURSOR)||((sc_flags & BLINK_CURSOR)&&(blinkrate & 4))){
	    if ((cursor_image & 0x7000) == 0x7000) {
		cursor_image &= 0x8fff;
		if(!(cursor_image & 0x0700))
		    cursor_image |= 0x0700;
	    } else {
		cursor_image |= 0x7000;
		if ((cursor_image & 0x0700) == 0x0700)
		    cursor_image &= 0xf0ff;
	    }
	}
    }
    *ptr = cursor_image;
#endif
}

static void
remove_cursor_image(scr_stat *scp)
{
#ifndef PC98
    if (ISPIXELSC(scp))
	sc_bcopy(scp, scp->scr_buf, scp->cursor_oldpos - scp->scr_buf, 
		 scp->cursor_oldpos - scp->scr_buf, 0);
    else
	*((u_short *)(scp->adp->va_window)
			 + (scp->cursor_oldpos - scp->scr_buf))
	    = scp->cursor_saveunder;
#endif
}

static void
move_crsr(scr_stat *scp, int x, int y)
{
    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;
    if (x >= scp->xsize)
	x = scp->xsize-1;
    if (y >= scp->ysize)
	y = scp->ysize-1;
    scp->xpos = x;
    scp->ypos = y;
    scp->cursor_pos = scp->scr_buf + scp->ypos * scp->xsize + scp->xpos;
#ifdef PC98
    scp->cursor_atr = scp->atr_buf + scp->ypos * scp->xsize + scp->xpos;
#endif
}

static int
scprobe(device_t dev)
{
    int unit = device_get_unit(dev);
    int flags = isa_get_flags(dev);

    device_set_desc(dev, "System console");

    if (!scvidprobe(unit, flags, FALSE)) {
	if (bootverbose)
	    printf("sc%d: no video adapter is found.\n", unit);
	return ENXIO;
    }

    return ((sckbdprobe(unit, flags, FALSE)) ? 0 : ENXIO);
}

/* probe video adapters, return TRUE if found */ 
static int
scvidprobe(int unit, int flags, int cons)
{
    video_adapter_t *adp;

    /*
     * Access the video adapter driver through the back door!
     * Video adapter drivers need to be configured before syscons.
     * However, when syscons is being probed as the low-level console,
     * they have not been initialized yet.  We force them to initialize
     * themselves here. XXX
     */
    vid_configure(cons ? VIO_PROBE_ONLY : 0);

    /* allocate a frame buffer */
    if (adapter < 0) {
	adapter = vid_allocate("*", -1, (void *)&adapter);
	if (adapter < 0)
	    return FALSE;
    }
    adp = vid_get_adapter(adapter);	/* shouldn't fail */

    Crtat = (u_short *)adp->va_window;
#ifdef PC98
    Atrat = Crtat + ATTR_OFFSET;
#endif
    initial_video_mode = adp->va_initial_mode;

    return TRUE;
}

/* probe the keyboard, return TRUE if found */
static int
sckbdprobe(int unit, int flags, int cons)
{
    /* access the keyboard driver through the backdoor! */
    kbd_configure(cons ? KB_CONF_PROBE_ONLY : 0);

    /* allocate a keyboard and register the keyboard event handler */
    if (keyboard < 0) {
	keyboard = kbd_allocate("*", -1, (void *)&keyboard, sckbdevent, NULL);
	if (keyboard < 0)
	    return FALSE;
    }
    kbd = kbd_get_keyboard(keyboard);	/* shouldn't fail */

    return TRUE;
}

#if NAPM > 0
static int
scresume(void *dummy)
{
    if (kbd != NULL)
	kbd_clear_state(kbd);
    return 0;
}
#endif

static int
scattach(device_t dev)
{
    scr_stat *scp;
#if defined(VESA)
    video_info_t info;
#endif
    dev_t cdev = makedev(CDEV_MAJOR, 0);
#ifdef DEVFS
    int vc;
#endif

    scinit();
    scp = console[0];
    sc_flags = isa_get_flags(dev);
    if (!ISFONTAVAIL(scp->adp->va_flags))
	sc_flags &= ~CHAR_CURSOR;

    /* copy temporary buffer to final buffer */
    scp->scr_buf = NULL;
#ifdef PC98
    scp->atr_buf = NULL;
#endif
    sc_alloc_scr_buffer(scp, FALSE, FALSE);
    bcopy(sc_buffer, scp->scr_buf, scp->xsize*scp->ysize*sizeof(u_short));
#ifdef PC98
    bcopy(Atrat, scp->atr_buf, scp->xsize*scp->ysize*sizeof(u_short));
#endif

    /* cut buffer is available only when the mouse pointer is used */
    if (ISMOUSEAVAIL(scp->adp->va_flags))
	sc_alloc_cut_buffer(scp, FALSE);

    /* initialize history buffer & pointers */
    sc_alloc_history_buffer(scp, sc_history_size, 0, FALSE);

#if defined(VESA)
    if ((sc_flags & VESA800X600)
	&& ((*vidsw[scp->ad]->get_info)(scp->adp, M_VESA_800x600, &info) == 0)) {
#if NSPLASH > 0
	splash_term(scp->adp);
#endif
	sc_set_graphics_mode(scp, NULL, M_VESA_800x600);
	sc_set_pixel_mode(scp, NULL, COL, ROW, 16);
	initial_video_mode = M_VESA_800x600;
#if NSPLASH > 0
	/* put up the splash again! */
    	splash_init(scp->adp, scsplash_callback);
#endif
    }
#endif /* VESA */

    /* initialize cursor stuff */
    if (!ISGRAPHSC(scp))
    	draw_cursor_image(scp);

    /* get screen update going */
    scrn_timer((void *)TRUE);

    /* set up the keyboard */
    kbd_ioctl(kbd, KDSKBMODE, (caddr_t)&scp->kbd_mode);
    update_kbd_state(scp->status, LOCK_MASK);

    if (bootverbose) {
	printf("sc%d:", device_get_unit(dev));
    	if (adapter >= 0)
	    printf(" fb%d", adapter);
	if (keyboard >= 0)
	    printf(" kbd%d", keyboard);
	printf("\n");
    }
    printf("sc%d: ", device_get_unit(dev));
    switch(scp->adp->va_type) {
#ifdef PC98
    case KD_PC98:
	printf(" <text mode>");
	break;
#else
    case KD_VGA:
	printf("VGA %s", (scp->adp->va_flags & V_ADP_COLOR) ? "color" : "mono");
	break;
    case KD_EGA:
	printf("EGA %s", (scp->adp->va_flags & V_ADP_COLOR) ? "color" : "mono");
	break;
    case KD_CGA:
	printf("CGA");
	break;
    case KD_MONO:
    case KD_HERCULES:
    default:
	printf("MDA/Hercules");
	break;
#endif /* PC98 */
    }
    printf(" <%d virtual consoles, flags=0x%x>\n", MAXCONS, sc_flags);

#if NAPM > 0
    scp->r_hook.ah_fun = scresume;
    scp->r_hook.ah_arg = NULL;
    scp->r_hook.ah_name = "system keyboard";
    scp->r_hook.ah_order = APM_MID_ORDER;
    apm_hook_establish(APM_HOOK_RESUME , &scp->r_hook);
#endif

    at_shutdown(scshutdown, NULL, SHUTDOWN_PRE_SYNC);

    cdevsw_add(&sc_cdevsw);

#ifdef DEVFS
    for (vc = 0; vc < MAXCONS; vc++)
        sc_devfs_token[vc] = devfs_add_devswf(&sc_cdevsw, vc, DV_CHR,
				UID_ROOT, GID_WHEEL, 0600, "ttyv%r", vc);
    sc_mouse_devfs_token = devfs_add_devswf(&sc_cdevsw, SC_MOUSE, DV_CHR,
				UID_ROOT, GID_WHEEL, 0600, "sysmouse");
    sc_console_devfs_token = devfs_add_devswf(&sc_cdevsw, SC_CONSOLE, DV_CHR,
				UID_ROOT, GID_WHEEL, 0600, "consolectl");
#endif
    return 0;
}

struct tty
*scdevtotty(dev_t dev)
{
    int unit = minor(dev);

    if (init_done == COLD)
	return(NULL);
    if (unit == SC_CONSOLE)
	return CONSOLE_TTY;
    if (unit == SC_MOUSE)
	return MOUSE_TTY;
    if (unit >= MAXCONS || unit < 0)
	return(NULL);
    return VIRTUAL_TTY(unit);
}

int
scopen(dev_t dev, int flag, int mode, struct proc *p)
{
    struct tty *tp = scdevtotty(dev);
    keyarg_t key;

    if (!tp)
	return(ENXIO);

    tp->t_oproc = (minor(dev) == SC_MOUSE) ? scmousestart : scstart;
    tp->t_param = scparam;
    tp->t_dev = dev;
    if (!(tp->t_state & TS_ISOPEN)) {
	ttychars(tp);
        /* Use the current setting of the <-- key as default VERASE. */  
        /* If the Delete key is preferable, an stty is necessary     */
	key.keynum = 0x0e;	/* how do we know this magic number... XXX */
	kbd_ioctl(kbd, GIO_KEYMAPENT, (caddr_t)&key);
        tp->t_cc[VERASE] = key.key.map[0];
	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_cflag = TTYDEF_CFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	scparam(tp, &tp->t_termios);
	(*linesw[tp->t_line].l_modem)(tp, 1);
    	if (minor(dev) == SC_MOUSE)
	    mouse_level = 0;		/* XXX */
    }
    else
	if (tp->t_state & TS_XCLUDE && suser(p))
	    return(EBUSY);
    if (minor(dev) < MAXCONS && !console[minor(dev)]) {
	console[minor(dev)] = alloc_scp();
	if (ISGRAPHSC(console[minor(dev)]))
	    sc_set_pixel_mode(console[minor(dev)], NULL, COL, ROW, 16);
    }
    if (minor(dev)<MAXCONS && !tp->t_winsize.ws_col && !tp->t_winsize.ws_row) {
	tp->t_winsize.ws_col = console[minor(dev)]->xsize;
	tp->t_winsize.ws_row = console[minor(dev)]->ysize;
    }
    return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
scclose(dev_t dev, int flag, int mode, struct proc *p)
{
    struct tty *tp = scdevtotty(dev);
    struct scr_stat *scp;

    if (!tp)
	return(ENXIO);
    if (minor(dev) < MAXCONS) {
	scp = sc_get_scr_stat(tp->t_dev);
	if (scp->status & SWITCH_WAIT_ACQ)
	    wakeup((caddr_t)&scp->smode);
#if not_yet_done
	if (scp == &main_console) {
	    scp->pid = 0;
	    scp->proc = NULL;
	    scp->smode.mode = VT_AUTO;
	}
	else {
	    free(scp->scr_buf, M_DEVBUF);
#ifdef PC98
	    free(scp->atr_buf, M_DEVBUF);
	    if (scp->his_atr != NULL)
	    free(scp->his_atr, M_DEVBUF);
#endif
	    if (scp->history != NULL) {
		free(scp->history, M_DEVBUF);
		if (scp->history_size / scp->xsize
			> imax(sc_history_size, scp->ysize))
		    extra_history_size += scp->history_size / scp->xsize 
			- imax(sc_history_size, scp->ysize);
	    }
	    free(scp, M_DEVBUF);
	    console[minor(dev)] = NULL;
	}
#else
	scp->pid = 0;
	scp->proc = NULL;
	scp->smode.mode = VT_AUTO;
#endif
    }
    spltty();
    (*linesw[tp->t_line].l_close)(tp, flag);
    ttyclose(tp);
    spl0();
    return(0);
}

int
scread(dev_t dev, struct uio *uio, int flag)
{
    struct tty *tp = scdevtotty(dev);

    if (!tp)
	return(ENXIO);
    sc_touch_scrn_saver();
    return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
scwrite(dev_t dev, struct uio *uio, int flag)
{
    struct tty *tp = scdevtotty(dev);

    if (!tp)
	return(ENXIO);
    return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

static int
sckbdevent(keyboard_t *thiskbd, int event, void *arg)
{
    static struct tty *cur_tty;
    int c; 
    size_t len;
    u_char *cp;

    /* assert(thiskbd == kbd) */

    switch (event) {
    case KBDIO_KEYINPUT:
	break;
    case KBDIO_UNLOADING:
	kbd = NULL;
	kbd_release(thiskbd, (void *)&keyboard);
	return 0;
    default:
	return EINVAL;
    }

    /* 
     * Loop while there is still input to get from the keyboard.
     * I don't think this is nessesary, and it doesn't fix
     * the Xaccel-2.1 keyboard hang, but it can't hurt.		XXX
     */
    while ((c = scgetc(thiskbd, SCGETC_NONBLOCK)) != NOKEY) {

	cur_tty = VIRTUAL_TTY(get_scr_num());
	if (!(cur_tty->t_state & TS_ISOPEN))
	    if (!((cur_tty = CONSOLE_TTY)->t_state & TS_ISOPEN))
		continue;

	switch (KEYFLAGS(c)) {
	case 0x0000: /* normal key */
	    (*linesw[cur_tty->t_line].l_rint)(KEYCHAR(c), cur_tty);
	    break;
	case FKEY:  /* function key, return string */
	    cp = kbd_get_fkeystr(thiskbd, KEYCHAR(c), &len);
	    if (cp != NULL) {
	    	while (len-- >  0)
		    (*linesw[cur_tty->t_line].l_rint)(*cp++, cur_tty);
	    }
	    break;
	case MKEY:  /* meta is active, prepend ESC */
	    (*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)(KEYCHAR(c), cur_tty);
	    break;
	case BKEY:  /* backtab fixed sequence (esc [ Z) */
	    (*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)('[', cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)('Z', cur_tty);
	    break;
	}
    }

    if (cur_console->status & MOUSE_VISIBLE) {
	remove_mouse_image(cur_console);
	cur_console->status &= ~MOUSE_VISIBLE;
    }
    return 0;
}

static int
scparam(struct tty *tp, struct termios *t)
{
    tp->t_ispeed = t->c_ispeed;
    tp->t_ospeed = t->c_ospeed;
    tp->t_cflag = t->c_cflag;
    return 0;
}

int
scioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
    u_int delta_ehs;
    int error;
    int i;
    struct tty *tp;
    scr_stat *scp;
    int s;

    tp = scdevtotty(dev);
    if (!tp)
	return ENXIO;
    scp = sc_get_scr_stat(tp->t_dev);

    /* If there is a user_ioctl function call that first */
    if (sc_user_ioctl) {
	error = (*sc_user_ioctl)(dev, cmd, data, flag, p);
	if (error != ENOIOCTL)
	    return error;
    }

    error = sc_vid_ioctl(tp, cmd, data, flag, p);
    if (error != ENOIOCTL)
	return error;

    switch (cmd) {  		/* process console hardware related ioctl's */

    case GIO_ATTR:      	/* get current attributes */
	*(int*)data = (scp->term.cur_attr >> 8) & 0xFF;
	return 0;

    case GIO_COLOR:     	/* is this a color console ? */
	*(int *)data = (scp->adp->va_flags & V_ADP_COLOR) ? 1 : 0;
	return 0;

    case CONS_BLANKTIME:    	/* set screen saver timeout (0 = no saver) */
	if (*(int *)data < 0 || *(int *)data > MAX_BLANKTIME)
            return EINVAL;
	s = spltty();
	scrn_blank_time = *(int *)data;
	run_scrn_saver = (scrn_blank_time != 0);
	splx(s);
	return 0;

    case CONS_CURSORTYPE:   	/* set cursor type blink/noblink */
	if ((*(int*)data) & 0x01)
	    sc_flags |= BLINK_CURSOR;
	else
	    sc_flags &= ~BLINK_CURSOR;
	if ((*(int*)data) & 0x02) {
	    if (!ISFONTAVAIL(scp->adp->va_flags))
		return ENXIO;
	    sc_flags |= CHAR_CURSOR;
	} else
	    sc_flags &= ~CHAR_CURSOR;
	/* 
	 * The cursor shape is global property; all virtual consoles
	 * are affected. Update the cursor in the current console...
	 */
	if (!ISGRAPHSC(cur_console)) {
	    s = spltty();
            remove_cursor_image(cur_console);
	    if (sc_flags & CHAR_CURSOR)
	        set_destructive_cursor(cur_console);
	    draw_cursor_image(cur_console);
	    splx(s);
	}
	return 0;

    case CONS_BELLTYPE: 	/* set bell type sound/visual */
	if ((*(int *)data) & 0x01)
	    sc_flags |= VISUAL_BELL;
	else
	    sc_flags &= ~VISUAL_BELL;
	if ((*(int *)data) & 0x02)
	    sc_flags |= QUIET_BELL;
	else
	    sc_flags &= ~QUIET_BELL;
	return 0;

    case CONS_HISTORY:  	/* set history size */
	if (*(int *)data > 0) {
	    int lines;	/* buffer size to allocate */
	    int lines0;	/* current buffer size */

	    lines = imax(*(int *)data, scp->ysize);
	    lines0 = (scp->history != NULL) ? 
		      scp->history_size / scp->xsize : scp->ysize;
	    if (lines0 > imax(sc_history_size, scp->ysize))
		delta_ehs = lines0 - imax(sc_history_size, scp->ysize);
	    else
		delta_ehs = 0;
	    /*
	     * syscons unconditionally allocates buffers upto SC_HISTORY_SIZE
	     * lines or scp->ysize lines, whichever is larger. A value 
	     * greater than that is allowed, subject to extra_history_size.
	     */
	    if (lines > imax(sc_history_size, scp->ysize))
		if (lines - imax(sc_history_size, scp->ysize) >
		    extra_history_size + delta_ehs)
		    return EINVAL;
            if (cur_console->status & BUFFER_SAVED)
                return EBUSY;
	    sc_alloc_history_buffer(scp, lines, delta_ehs, TRUE);
	    return 0;
	}
	else
	    return EINVAL;

    case CONS_MOUSECTL:		/* control mouse arrow */
    case OLD_CONS_MOUSECTL:
    {
	/* MOUSE_BUTTON?DOWN -> MOUSE_MSC_BUTTON?UP */
	static int butmap[8] = {
            MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP,
            MOUSE_MSC_BUTTON2UP,
            MOUSE_MSC_BUTTON1UP,
            0,
	};
	mouse_info_t *mouse = (mouse_info_t*)data;
	mouse_info_t buf;

#ifndef PC98
	/* FIXME: */
	if (!ISMOUSEAVAIL(scp->adp->va_flags))
	    return ENODEV;
#endif
	
	if (cmd == OLD_CONS_MOUSECTL) {
	    static u_char swapb[] = { 0, 4, 2, 6, 1, 5, 3, 7 };
	    old_mouse_info_t *old_mouse = (old_mouse_info_t *)data;

	    mouse = &buf;
	    mouse->operation = old_mouse->operation;
	    switch (mouse->operation) {
	    case MOUSE_MODE:
		mouse->u.mode = old_mouse->u.mode;
		break;
	    case MOUSE_SHOW:
	    case MOUSE_HIDE:
		break;
	    case MOUSE_MOVEABS:
	    case MOUSE_MOVEREL:
	    case MOUSE_ACTION:
		mouse->u.data.x = old_mouse->u.data.x;
		mouse->u.data.y = old_mouse->u.data.y;
		mouse->u.data.z = 0;
		mouse->u.data.buttons = swapb[old_mouse->u.data.buttons & 0x7];
		break;
	    case MOUSE_GETINFO:
		old_mouse->u.data.x = scp->mouse_xpos;
		old_mouse->u.data.y = scp->mouse_ypos;
		old_mouse->u.data.buttons = swapb[scp->mouse_buttons & 0x7];
		break;
	    default:
		return EINVAL;
	    }
	}

	switch (mouse->operation) {
	case MOUSE_MODE:
	    if (ISSIGVALID(mouse->u.mode.signal)) {
		scp->mouse_signal = mouse->u.mode.signal;
		scp->mouse_proc = p;
		scp->mouse_pid = p->p_pid;
	    }
	    else {
		scp->mouse_signal = 0;
		scp->mouse_proc = NULL;
		scp->mouse_pid = 0;
	    }
	    return 0;

	case MOUSE_SHOW:
	    if (ISTEXTSC(scp) && !(scp->status & MOUSE_ENABLED)) {
		scp->status |= (MOUSE_ENABLED | MOUSE_VISIBLE);
		scp->mouse_oldpos = scp->mouse_pos;
		mark_all(scp);
		return 0;
	    }
	    else
		return EINVAL;
	    break;

	case MOUSE_HIDE:
	    if (ISTEXTSC(scp) && (scp->status & MOUSE_ENABLED)) {
		scp->status &= ~(MOUSE_ENABLED | MOUSE_VISIBLE);
		mark_all(scp);
		return 0;
	    }
	    else
		return EINVAL;
	    break;

	case MOUSE_MOVEABS:
	    scp->mouse_xpos = mouse->u.data.x;
	    scp->mouse_ypos = mouse->u.data.y;
	    set_mouse_pos(scp);
	    break;

	case MOUSE_MOVEREL:
	    scp->mouse_xpos += mouse->u.data.x;
	    scp->mouse_ypos += mouse->u.data.y;
	    set_mouse_pos(scp);
	    break;

	case MOUSE_GETINFO:
	    mouse->u.data.x = scp->mouse_xpos;
	    mouse->u.data.y = scp->mouse_ypos;
	    mouse->u.data.z = 0;
	    mouse->u.data.buttons = scp->mouse_buttons;
	    return 0;

	case MOUSE_ACTION:
	case MOUSE_MOTION_EVENT:
	    /* this should maybe only be settable from /dev/consolectl SOS */
	    /* send out mouse event on /dev/sysmouse */

	    mouse_status.dx += mouse->u.data.x;
	    mouse_status.dy += mouse->u.data.y;
	    mouse_status.dz += mouse->u.data.z;
	    if (mouse->operation == MOUSE_ACTION)
	        mouse_status.button = mouse->u.data.buttons;
	    mouse_status.flags |= 
		((mouse->u.data.x || mouse->u.data.y || mouse->u.data.z) ? 
		    MOUSE_POSCHANGED : 0)
		| (mouse_status.obutton ^ mouse_status.button);
	    if (mouse_status.flags == 0)
		return 0;

	    if (ISTEXTSC(cur_console) && (cur_console->status & MOUSE_ENABLED))
	    	cur_console->status |= MOUSE_VISIBLE;

	    if ((MOUSE_TTY)->t_state & TS_ISOPEN) {
		u_char buf[MOUSE_SYS_PACKETSIZE];
		int j;

		/* the first five bytes are compatible with MouseSystems' */
		buf[0] = MOUSE_MSC_SYNC
		    | butmap[mouse_status.button & MOUSE_STDBUTTONS];
		j = imax(imin(mouse->u.data.x, 255), -256);
		buf[1] = j >> 1;
		buf[3] = j - buf[1];
		j = -imax(imin(mouse->u.data.y, 255), -256);
		buf[2] = j >> 1;
		buf[4] = j - buf[2];
		for (j = 0; j < MOUSE_MSC_PACKETSIZE; j++)
	    		(*linesw[(MOUSE_TTY)->t_line].l_rint)(buf[j],MOUSE_TTY);
		if (mouse_level >= 1) { 	/* extended part */
		    j = imax(imin(mouse->u.data.z, 127), -128);
		    buf[5] = (j >> 1) & 0x7f;
		    buf[6] = (j - (j >> 1)) & 0x7f;
		    /* buttons 4-10 */
		    buf[7] = (~mouse_status.button >> 3) & 0x7f;
		    for (j = MOUSE_MSC_PACKETSIZE; 
			 j < MOUSE_SYS_PACKETSIZE; j++)
	    		(*linesw[(MOUSE_TTY)->t_line].l_rint)(buf[j],MOUSE_TTY);
		}
	    }

	    if (cur_console->mouse_signal) {
		cur_console->mouse_buttons = mouse->u.data.buttons;
    		/* has controlling process died? */
		if (cur_console->mouse_proc && 
		    (cur_console->mouse_proc != pfind(cur_console->mouse_pid))){
		    	cur_console->mouse_signal = 0;
			cur_console->mouse_proc = NULL;
			cur_console->mouse_pid = 0;
		}
		else
		    psignal(cur_console->mouse_proc, cur_console->mouse_signal);
	    }
	    else if (mouse->operation == MOUSE_ACTION && cut_buffer != NULL) {
		/* process button presses */
		if ((cur_console->mouse_buttons ^ mouse->u.data.buttons) && 
		    ISTEXTSC(cur_console)) {
		    cur_console->mouse_buttons = mouse->u.data.buttons;
		    if (cur_console->mouse_buttons & MOUSE_BUTTON1DOWN)
			mouse_cut_start(cur_console);
		    else
			mouse_cut_end(cur_console);
		    if (cur_console->mouse_buttons & MOUSE_BUTTON2DOWN ||
			cur_console->mouse_buttons & MOUSE_BUTTON3DOWN)
			mouse_paste(cur_console);
		}
	    }

	    if (mouse->u.data.x != 0 || mouse->u.data.y != 0) {
		cur_console->mouse_xpos += mouse->u.data.x;
		cur_console->mouse_ypos += mouse->u.data.y;
		set_mouse_pos(cur_console);
	    }

	    break;

	case MOUSE_BUTTON_EVENT:
	    if ((mouse->u.event.id & MOUSE_BUTTONS) == 0)
		return EINVAL;
	    if (mouse->u.event.value < 0)
		return EINVAL;

	    if (mouse->u.event.value > 0) {
	        cur_console->mouse_buttons |= mouse->u.event.id;
	        mouse_status.button |= mouse->u.event.id;
	    } else {
	        cur_console->mouse_buttons &= ~mouse->u.event.id;
	        mouse_status.button &= ~mouse->u.event.id;
	    }
	    mouse_status.flags |= mouse_status.obutton ^ mouse_status.button;
	    if (mouse_status.flags == 0)
		return 0;

	    if (ISTEXTSC(cur_console) && (cur_console->status & MOUSE_ENABLED))
	    	cur_console->status |= MOUSE_VISIBLE;

	    if ((MOUSE_TTY)->t_state & TS_ISOPEN) {
		u_char buf[8];
		int i;

		buf[0] = MOUSE_MSC_SYNC 
			 | butmap[mouse_status.button & MOUSE_STDBUTTONS];
		buf[7] = (~mouse_status.button >> 3) & 0x7f;
		buf[1] = buf[2] = buf[3] = buf[4] = buf[5] = buf[6] = 0;
		for (i = 0; 
		     i < ((mouse_level >= 1) ? MOUSE_SYS_PACKETSIZE 
					     : MOUSE_MSC_PACKETSIZE); i++)
	    	    (*linesw[(MOUSE_TTY)->t_line].l_rint)(buf[i],MOUSE_TTY);
	    }

	    if (cur_console->mouse_signal) {
		if (cur_console->mouse_proc && 
		    (cur_console->mouse_proc != pfind(cur_console->mouse_pid))){
		    	cur_console->mouse_signal = 0;
			cur_console->mouse_proc = NULL;
			cur_console->mouse_pid = 0;
		}
		else
		    psignal(cur_console->mouse_proc, cur_console->mouse_signal);
		break;
	    }

	    if (!ISTEXTSC(cur_console) || (cut_buffer == NULL))
		break;

	    switch (mouse->u.event.id) {
	    case MOUSE_BUTTON1DOWN:
	        switch (mouse->u.event.value % 4) {
		case 0:	/* up */
		    mouse_cut_end(cur_console);
		    break;
		case 1:
		    mouse_cut_start(cur_console);
		    break;
		case 2:
		    mouse_cut_word(cur_console);
		    break;
		case 3:
		    mouse_cut_line(cur_console);
		    break;
		}
		break;
	    case MOUSE_BUTTON2DOWN:
	        switch (mouse->u.event.value) {
		case 0:	/* up */
		    break;
		default:
		    mouse_paste(cur_console);
		    break;
		}
		break;
	    case MOUSE_BUTTON3DOWN:
	        switch (mouse->u.event.value) {
		case 0:	/* up */
		    if (!(cur_console->mouse_buttons & MOUSE_BUTTON1DOWN))
		        mouse_cut_end(cur_console);
		    break;
		default:
		    mouse_cut_extend(cur_console);
		    break;
		}
		break;
	    }
	    break;

	default:
	    return EINVAL;
	}
	/* make screensaver happy */
	sc_touch_scrn_saver();
	return 0;
    }

    /* MOUSE_XXX: /dev/sysmouse ioctls */
    case MOUSE_GETHWINFO:	/* get device information */
    {
	mousehw_t *hw = (mousehw_t *)data;

	if (tp != MOUSE_TTY)
	    return ENOTTY;
	hw->buttons = 10;		/* XXX unknown */
	hw->iftype = MOUSE_IF_SYSMOUSE;
	hw->type = MOUSE_MOUSE;
	hw->model = MOUSE_MODEL_GENERIC;
	hw->hwid = 0;
	return 0;
    }

    case MOUSE_GETMODE:		/* get protocol/mode */
    {
	mousemode_t *mode = (mousemode_t *)data;

	if (tp != MOUSE_TTY)
	    return ENOTTY;
	mode->level = mouse_level;
	switch (mode->level) {
	case 0:
	    /* at this level, sysmouse emulates MouseSystems protocol */
	    mode->protocol = MOUSE_PROTO_MSC;
	    mode->rate = -1;		/* unknown */
	    mode->resolution = -1;	/* unknown */
	    mode->accelfactor = 0;	/* disabled */
	    mode->packetsize = MOUSE_MSC_PACKETSIZE;
	    mode->syncmask[0] = MOUSE_MSC_SYNCMASK;
	    mode->syncmask[1] = MOUSE_MSC_SYNC;
	    break;

	case 1:
	    /* at this level, sysmouse uses its own protocol */
	    mode->protocol = MOUSE_PROTO_SYSMOUSE;
	    mode->rate = -1;
	    mode->resolution = -1;
	    mode->accelfactor = 0;
	    mode->packetsize = MOUSE_SYS_PACKETSIZE;
	    mode->syncmask[0] = MOUSE_SYS_SYNCMASK;
	    mode->syncmask[1] = MOUSE_SYS_SYNC;
	    break;
	}
	return 0;
    }

    case MOUSE_SETMODE:		/* set protocol/mode */
    {
	mousemode_t *mode = (mousemode_t *)data;

	if (tp != MOUSE_TTY)
	    return ENOTTY;
	if ((mode->level < 0) || (mode->level > 1))
	    return EINVAL;
	mouse_level = mode->level;
	return 0;
    }

    case MOUSE_GETLEVEL:	/* get operation level */
	if (tp != MOUSE_TTY)
	    return ENOTTY;
	*(int *)data = mouse_level;
	return 0;

    case MOUSE_SETLEVEL:	/* set operation level */
	if (tp != MOUSE_TTY)
	    return ENOTTY;
	if ((*(int *)data  < 0) || (*(int *)data > 1))
	    return EINVAL;
	mouse_level = *(int *)data;
	return 0;

    case MOUSE_GETSTATUS:	/* get accumulated mouse events */
	if (tp != MOUSE_TTY)
	    return ENOTTY;
	s = spltty();
	*(mousestatus_t *)data = mouse_status;
	mouse_status.flags = 0;
	mouse_status.obutton = mouse_status.button;
	mouse_status.dx = 0;
	mouse_status.dy = 0;
	mouse_status.dz = 0;
	splx(s);
	return 0;

#if notyet
    case MOUSE_GETVARS:		/* get internal mouse variables */
    case MOUSE_SETVARS:		/* set internal mouse variables */
	if (tp != MOUSE_TTY)
	    return ENOTTY;
	return ENODEV;
#endif

    case MOUSE_READSTATE:	/* read status from the device */
    case MOUSE_READDATA:	/* read data from the device */
	if (tp != MOUSE_TTY)
	    return ENOTTY;
	return ENODEV;

    case CONS_GETINFO:  	/* get current (virtual) console info */
    {
	vid_info_t *ptr = (vid_info_t*)data;
	if (ptr->size == sizeof(struct vid_info)) {
	    ptr->m_num = get_scr_num();
	    ptr->mv_col = scp->xpos;
	    ptr->mv_row = scp->ypos;
	    ptr->mv_csz = scp->xsize;
	    ptr->mv_rsz = scp->ysize;
	    ptr->mv_norm.fore = (scp->term.std_color & 0x0f00)>>8;
	    ptr->mv_norm.back = (scp->term.std_color & 0xf000)>>12;
	    ptr->mv_rev.fore = (scp->term.rev_color & 0x0f00)>>8;
	    ptr->mv_rev.back = (scp->term.rev_color & 0xf000)>>12;
	    ptr->mv_grfc.fore = 0;      /* not supported */
	    ptr->mv_grfc.back = 0;      /* not supported */
	    ptr->mv_ovscan = scp->border;
	    if (scp == cur_console)
		save_kbd_state(scp);
	    ptr->mk_keylock = scp->status & LOCK_MASK;
	    return 0;
	}
	return EINVAL;
    }

    case CONS_GETVERS:  	/* get version number */
	*(int*)data = 0x200;    /* version 2.0 */
	return 0;

    case CONS_IDLE:		/* see if the screen has been idle */
	/*
	 * When the screen is in the GRAPHICS_MODE or UNKNOWN_MODE,
	 * the user process may have been writing something on the
	 * screen and syscons is not aware of it. Declare the screen
	 * is NOT idle if it is in one of these modes. But there is
	 * an exception to it; if a screen saver is running in the 
	 * graphics mode in the current screen, we should say that the
	 * screen has been idle.
	 */
	*(int *)data = scrn_idle 
		       && (!ISGRAPHSC(cur_console)
			   || (cur_console->status & SAVER_RUNNING));
	return 0;

    case CONS_SAVERMODE:	/* set saver mode */
	switch(*(int *)data) {
	case CONS_USR_SAVER:
	    /* if a LKM screen saver is running, stop it first. */
	    scsplash_stick(FALSE);
	    saver_mode = *(int *)data;
	    s = spltty();
#if NSPLASH > 0
	    if ((error = wait_scrn_saver_stop())) {
		splx(s);
		return error;
	    }
#endif /* NSPLASH */
	    scp->status |= SAVER_RUNNING;
	    scsplash_stick(TRUE);
	    splx(s);
	    break;
	case CONS_LKM_SAVER:
	    s = spltty();
	    if ((saver_mode == CONS_USR_SAVER) && (scp->status & SAVER_RUNNING))
		scp->status &= ~SAVER_RUNNING;
	    saver_mode = *(int *)data;
	    splx(s);
	    break;
	default:
	    return EINVAL;
	}
	return 0;

    case CONS_SAVERSTART:	/* immediately start/stop the screen saver */
	/*
	 * Note that this ioctl does not guarantee the screen saver 
	 * actually starts or stops. It merely attempts to do so...
	 */
	s = spltty();
	run_scrn_saver = (*(int *)data != 0);
	if (run_scrn_saver)
	    scrn_time_stamp -= scrn_blank_time;
	splx(s);
	return 0;

    case VT_SETMODE:    	/* set screen switcher mode */
    {
	struct vt_mode *mode;

	mode = (struct vt_mode *)data;
	if (ISSIGVALID(mode->relsig) && ISSIGVALID(mode->acqsig) &&
	    ISSIGVALID(mode->frsig)) {
	    bcopy(data, &scp->smode, sizeof(struct vt_mode));
	    if (scp->smode.mode == VT_PROCESS) {
		scp->proc = p;
		scp->pid = scp->proc->p_pid;
	    }
	    return 0;
	} else
	    return EINVAL;
    }

    case VT_GETMODE:    	/* get screen switcher mode */
	bcopy(&scp->smode, data, sizeof(struct vt_mode));
	return 0;

    case VT_RELDISP:    	/* screen switcher ioctl */
	switch(*(int *)data) {
	case VT_FALSE:  	/* user refuses to release screen, abort */
	    if (scp == old_scp && (scp->status & SWITCH_WAIT_REL)) {
		old_scp->status &= ~SWITCH_WAIT_REL;
		switch_in_progress = FALSE;
		return 0;
	    }
	    return EINVAL;

	case VT_TRUE:   	/* user has released screen, go on */
	    if (scp == old_scp && (scp->status & SWITCH_WAIT_REL)) {
		scp->status &= ~SWITCH_WAIT_REL;
		exchange_scr();
		if (new_scp->smode.mode == VT_PROCESS) {
		    new_scp->status |= SWITCH_WAIT_ACQ;
		    psignal(new_scp->proc, new_scp->smode.acqsig);
		}
		else
		    switch_in_progress = FALSE;
		return 0;
	    }
	    return EINVAL;

	case VT_ACKACQ: 	/* acquire acknowledged, switch completed */
	    if (scp == new_scp && (scp->status & SWITCH_WAIT_ACQ)) {
		scp->status &= ~SWITCH_WAIT_ACQ;
		switch_in_progress = FALSE;
		return 0;
	    }
	    return EINVAL;

	default:
	    return EINVAL;
	}
	/* NOT REACHED */

    case VT_OPENQRY:    	/* return free virtual console */
	for (i = 0; i < MAXCONS; i++) {
	    tp = VIRTUAL_TTY(i);
	    if (!(tp->t_state & TS_ISOPEN)) {
		*(int *)data = i + 1;
		return 0;
	    }
	}
	return EINVAL;

    case VT_ACTIVATE:   	/* switch to screen *data */
	s = spltty();
	sc_clean_up(cur_console);
	splx(s);
	return switch_scr(scp, *(int *)data - 1);

    case VT_WAITACTIVE: 	/* wait for switch to occur */
	if (*(int *)data > MAXCONS || *(int *)data < 0)
	    return EINVAL;
	s = spltty();
	error = sc_clean_up(cur_console);
	splx(s);
	if (error)
	    return error;
	if (minor(dev) == *(int *)data - 1)
	    return 0;
	if (*(int *)data == 0) {
	    if (scp == cur_console)
		return 0;
	}
	else
	    scp = console[*(int *)data - 1];
	while ((error=tsleep((caddr_t)&scp->smode, PZERO|PCATCH,
			     "waitvt", 0)) == ERESTART) ;
	return error;

    case VT_GETACTIVE:
	*(int *)data = get_scr_num()+1;
	return 0;

    case KDENABIO:      	/* allow io operations */
	error = suser(p);
	if (error != 0)
	    return error;
	if (securelevel > 0)
	    return EPERM;
	p->p_md.md_regs->tf_eflags |= PSL_IOPL;
	return 0;

    case KDDISABIO:     	/* disallow io operations (default) */
	p->p_md.md_regs->tf_eflags &= ~PSL_IOPL;
	return 0;

    case KDSKBSTATE:    	/* set keyboard state (locks) */
	if (*(int *)data & ~LOCK_MASK)
	    return EINVAL;
	scp->status &= ~LOCK_MASK;
	scp->status |= *(int *)data;
	if (scp == cur_console)
	    update_kbd_state(scp->status, LOCK_MASK);
	return 0;

    case KDGKBSTATE:    	/* get keyboard state (locks) */
	if (scp == cur_console)
	    save_kbd_state(scp);
	*(int *)data = scp->status & LOCK_MASK;
	return 0;

    case KDSETRAD:      	/* set keyboard repeat & delay rates */
	if (*(int *)data & ~0x7f)
	    return EINVAL;
	error = kbd_ioctl(kbd, KDSETRAD, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

    case KDSKBMODE:     	/* set keyboard mode */
	switch (*(int *)data) {
	case K_XLATE:   	/* switch to XLT ascii mode */
	case K_RAW: 		/* switch to RAW scancode mode */
	case K_CODE: 		/* switch to CODE mode */
	    scp->kbd_mode = *(int *)data;
	    if (scp == cur_console)
		kbd_ioctl(kbd, cmd, data);
	    return 0;
	default:
	    return EINVAL;
	}
	/* NOT REACHED */

    case KDGKBMODE:     	/* get keyboard mode */
	*(int *)data = scp->kbd_mode;
	return 0;

    case KDGKBINFO:
	error = kbd_ioctl(kbd, cmd, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

    case KDMKTONE:      	/* sound the bell */
	if (*(int*)data)
	    do_bell(scp, (*(int*)data)&0xffff,
		    (((*(int*)data)>>16)&0xffff)*hz/1000);
	else
	    do_bell(scp, scp->bell_pitch, scp->bell_duration);
	return 0;

    case KIOCSOUND:     	/* make tone (*data) hz */
	if (scp == cur_console) {
	    if (*(int*)data) {
		int pitch = timer_freq / *(int*)data;

#ifdef PC98
		/* enable counter 1 */
		outb(0x35, inb(0x35) & 0xf7);
		/* set command for counter 1, 2 byte write */
		if (acquire_timer1(TIMER_16BIT|TIMER_SQWAVE)) {
			return EBUSY;
		}
		/* set pitch */
		outb(TIMER_CNTR1, pitch);
		outb(TIMER_CNTR1, (pitch>>8));
#else
		/* set command for counter 2, 2 byte write */
		if (acquire_timer2(TIMER_16BIT|TIMER_SQWAVE))
		    return EBUSY;

		/* set pitch */
		outb(TIMER_CNTR2, pitch);
		outb(TIMER_CNTR2, (pitch>>8));

		/* enable counter 2 output to speaker */
		outb(IO_PPI, inb(IO_PPI) | 3);
#endif
	    }
	    else {
#ifdef PC98
	      /* disable counter 1 */
	      outb(0x35, inb(0x35) | 0x08);
	      release_timer1();
#else
		/* disable counter 2 output to speaker */
		outb(IO_PPI, inb(IO_PPI) & 0xFC);
		release_timer2();
#endif
	    }
	}
	return 0;

    case KDGKBTYPE:     	/* get keyboard type */
	error = kbd_ioctl(kbd, cmd, data);
	if (error == ENOIOCTL) {
	    /* always return something? XXX */
	    *(int *)data = 0;
	}
	return 0;

    case KDSETLED:      	/* set keyboard LED status */
	if (*(int *)data & ~LED_MASK)	/* FIXME: LOCK_MASK? */
	    return EINVAL;
	scp->status &= ~LED_MASK;
	scp->status |= *(int *)data;
	if (scp == cur_console)
	    update_kbd_leds(scp->status);
	return 0;

    case KDGETLED:      	/* get keyboard LED status */
	if (scp == cur_console)
	    save_kbd_state(scp);
	*(int *)data = scp->status & LED_MASK;
	return 0;

    case CONS_SETKBD: 		/* set the new keyboard */
	{
	    keyboard_t *newkbd;

	    s = spltty();
	    newkbd = kbd_get_keyboard(*(int *)data);
	    if (newkbd == NULL) {
		splx(s);
		return EINVAL;
	    }
	    error = 0;
	    if (kbd != newkbd) {
		i = kbd_allocate(newkbd->kb_name, newkbd->kb_unit,
				 (void *)&keyboard, sckbdevent, NULL);
		/* i == newkbd->kb_index */
		if (i >= 0) {
		    if (kbd != NULL) {
			save_kbd_state(cur_console);
			kbd_release(kbd, (void *)&keyboard);
		    }
		    kbd = kbd_get_keyboard(i);	/* kbd == newkbd */
		    keyboard = i;
		    kbd_ioctl(kbd, KDSKBMODE, (caddr_t)&cur_console->kbd_mode);
		    update_kbd_state(cur_console->status, LOCK_MASK);
		} else {
		    error = EPERM;	/* XXX */
		}
	    }
	    splx(s);
	    return error;
	}

    case CONS_RELKBD: 		/* release the current keyboard */
	s = spltty();
	error = 0;
	if (kbd != NULL) {
	    save_kbd_state(cur_console);
	    error = kbd_release(kbd, (void *)&keyboard);
	    if (error == 0) {
		kbd = NULL;
		keyboard = -1;
	    }
	}
	splx(s);
	return error;
 
    case GIO_SCRNMAP:   	/* get output translation table */
	bcopy(&scr_map, data, sizeof(scr_map));
	return 0;

    case PIO_SCRNMAP:   	/* set output translation table */
	bcopy(data, &scr_map, sizeof(scr_map));
	for (i=0; i<sizeof(scr_map); i++)
	    scr_rmap[scr_map[i]] = i;
	return 0;

    case GIO_KEYMAP:		/* get keyboard translation table */
    case PIO_KEYMAP:		/* set keyboard translation table */
    case GIO_DEADKEYMAP:	/* get accent key translation table */
    case PIO_DEADKEYMAP:	/* set accent key translation table */
    case GETFKEY:		/* get function key string */
    case SETFKEY:		/* set function key string */
	error = kbd_ioctl(kbd, cmd, data);
	if (error == ENOIOCTL)
	    error = ENODEV;
	return error;

    case PIO_FONT8x8:   	/* set 8x8 dot font */
	if (!ISFONTAVAIL(scp->adp->va_flags))
	    return ENXIO;
	bcopy(data, font_8, 8*256);
	fonts_loaded |= FONT_8;
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 * Don't load if the current font size is not 8x8.
	 */
	if (ISTEXTSC(cur_console) && (cur_console->font_size < 14))
	    copy_font(cur_console, LOAD, 8, font_8);
	return 0;

    case GIO_FONT8x8:   	/* get 8x8 dot font */
	if (!ISFONTAVAIL(scp->adp->va_flags))
	    return ENXIO;
	if (fonts_loaded & FONT_8) {
	    bcopy(font_8, data, 8*256);
	    return 0;
	}
	else
	    return ENXIO;

    case PIO_FONT8x14:  	/* set 8x14 dot font */
	if (!ISFONTAVAIL(scp->adp->va_flags))
	    return ENXIO;
	bcopy(data, font_14, 14*256);
	fonts_loaded |= FONT_14;
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 * Don't load if the current font size is not 8x14.
	 */
	if (ISTEXTSC(cur_console)
	    && (cur_console->font_size >= 14) && (cur_console->font_size < 16))
	    copy_font(cur_console, LOAD, 14, font_14);
	return 0;

    case GIO_FONT8x14:  	/* get 8x14 dot font */
	if (!ISFONTAVAIL(scp->adp->va_flags))
	    return ENXIO;
	if (fonts_loaded & FONT_14) {
	    bcopy(font_14, data, 14*256);
	    return 0;
	}
	else
	    return ENXIO;

    case PIO_FONT8x16:  	/* set 8x16 dot font */
	if (!ISFONTAVAIL(scp->adp->va_flags))
	    return ENXIO;
	bcopy(data, font_16, 16*256);
	fonts_loaded |= FONT_16;
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 * Don't load if the current font size is not 8x16.
	 */
	if (ISTEXTSC(cur_console) && (cur_console->font_size >= 16))
	    copy_font(cur_console, LOAD, 16, font_16);
	return 0;

    case GIO_FONT8x16:  	/* get 8x16 dot font */
	if (!ISFONTAVAIL(scp->adp->va_flags))
	    return ENXIO;
	if (fonts_loaded & FONT_16) {
	    bcopy(font_16, data, 16*256);
	    return 0;
	}
	else
	    return ENXIO;

#ifdef PC98
    case ADJUST_CLOCK:	/* /dev/rtc for 98note resume */
	inittodr(0);
	return 0;
#endif
    default:
	break;
    }

    error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
    if (error != ENOIOCTL)
	return(error);
    error = ttioctl(tp, cmd, data, flag);
    if (error != ENOIOCTL)
	return(error);
    return(ENOTTY);
}

static void
scstart(struct tty *tp)
{
    struct clist *rbp;
    int s, len;
    u_char buf[PCBURST];
    scr_stat *scp = sc_get_scr_stat(tp->t_dev);

    if (scp->status & SLKED || blink_in_progress)
	return; /* XXX who repeats the call when the above flags are cleared? */
    s = spltty();
    if (!(tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))) {
	tp->t_state |= TS_BUSY;
	rbp = &tp->t_outq;
	while (rbp->c_cc) {
	    len = q_to_b(rbp, buf, PCBURST);
	    splx(s);
	    ansi_put(scp, buf, len);
	    s = spltty();
	}
	tp->t_state &= ~TS_BUSY;
	ttwwakeup(tp);
    }
    splx(s);
}

static void
scmousestart(struct tty *tp)
{
    struct clist *rbp;
    int s;
    u_char buf[PCBURST];

    s = spltty();
    if (!(tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))) {
	tp->t_state |= TS_BUSY;
	rbp = &tp->t_outq;
	while (rbp->c_cc) {
	    q_to_b(rbp, buf, PCBURST);
	}
	tp->t_state &= ~TS_BUSY;
	ttwwakeup(tp);
    }
    splx(s);
}

static void
sccnprobe(struct consdev *cp)
{
#if 0
    struct isa_device *dvp;

    /*
     * Take control if we are the highest priority enabled display device.
     */
    dvp = find_display();
    if (dvp == NULL || dvp->id_driver != &scdriver) {
	cp->cn_pri = CN_DEAD;
	return;
    }

    if (!scvidprobe(dvp->id_unit, dvp->id_flags, TRUE)) {
	cp->cn_pri = CN_DEAD;
	return;
    }
    sckbdprobe(dvp->id_unit, dvp->id_flags, TRUE);
#else
    if (!scvidprobe(0, 0, TRUE)) {
	cp->cn_pri = CN_DEAD;
	return;
    }
    sckbdprobe(0, 0, TRUE);
#endif

    /* initialize required fields */
    cp->cn_dev = makedev(CDEV_MAJOR, SC_CONSOLE);
    cp->cn_pri = CN_INTERNAL;
}

static void
sccninit(struct consdev *cp)
{
    scinit();
}

static void
sccnputc(dev_t dev, int c)
{
    u_char buf[1];
    scr_stat *scp = console[0];
    term_stat save = scp->term;
    u_short *p;
    int s;
    int i;

    if (scp == cur_console && scp->status & SLKED) {
	scp->status &= ~SLKED;
	update_kbd_state(scp->status, SLKED);
	if (cur_console->status & BUFFER_SAVED) {
	    p = cur_console->history_save;
	    for (i = 0; i < cur_console->ysize; ++i) {
		bcopy(p, cur_console->scr_buf + (cur_console->xsize*i),
		      cur_console->xsize*sizeof(u_short));
		p += cur_console->xsize;
		if (p + cur_console->xsize
		    > cur_console->history + cur_console->history_size)
		    p = cur_console->history;
	    }
	    cur_console->status &= ~BUFFER_SAVED;
	    cur_console->history_head = cur_console->history_save;
	    cur_console->status |= CURSOR_ENABLED;
	    mark_all(cur_console);
	}
#if 1 /* XXX */
	scstart(VIRTUAL_TTY(get_scr_num()));
#endif
    }

    scp->term = kernel_console;
    current_default = &kernel_default;
    if (scp == cur_console && !ISGRAPHSC(scp))
	remove_cursor_image(scp);
    buf[0] = c;
    ansi_put(scp, buf, 1);
    kernel_console = scp->term;
    current_default = &user_default;
    scp->term = save;

    s = spltty();	/* block sckbdevent and scrn_timer */
    sccnupdate(scp);
    splx(s);
}

static int
sccngetc(dev_t dev)
{
    return sccngetch(0);
}

static int
sccncheckc(dev_t dev)
{
    return sccngetch(SCGETC_NONBLOCK);
}

static int
sccngetch(int flags)
{
    int cur_mode;
    int s = spltty();	/* block sckbdevent and scrn_timer while we poll */
    int c;

    /* 
     * Stop the screen saver and update the screen if necessary.
     * What if we have been running in the screen saver code... XXX
     */
    sc_touch_scrn_saver();
    sccnupdate(cur_console);

    if (kbd == NULL) {
	splx(s);
	return -1;
    }

    /* 
     * Make sure the keyboard is accessible even when the kbd device
     * driver is disabled.
     */
    kbd_enable(kbd);

    /* we shall always use the keyboard in the XLATE mode here */
    cur_mode = cur_console->kbd_mode;
    cur_console->kbd_mode = K_XLATE;
    kbd_ioctl(kbd, KDSKBMODE, (caddr_t)&cur_console->kbd_mode);

    kbd_poll(kbd, TRUE);
    c = scgetc(kbd, SCGETC_CN | flags);
    kbd_poll(kbd, FALSE);

    cur_console->kbd_mode = cur_mode;
    kbd_ioctl(kbd, KDSKBMODE, (caddr_t)&cur_console->kbd_mode);
    kbd_disable(kbd);
    splx(s);

    switch (KEYFLAGS(c)) {
    case 0:	/* normal char */
	return KEYCHAR(c);
    case FKEY:	/* function key */
	return c;	/* XXX */
    case NOKEY:
    case ERRKEY:
    default:
	return -1;
    }
    /* NOT REACHED */
}

static void
sccnupdate(scr_stat *scp)
{
    /* this is a cut-down version of scrn_timer()... */

    if (font_loading_in_progress)
	return;

    if (panicstr || shutdown_in_progress) {
	sc_touch_scrn_saver();
    } else if (scp != cur_console) {
	return;
    }

    if (!run_scrn_saver)
	scrn_idle = FALSE;
#if NSPLASH > 0
    if ((saver_mode != CONS_LKM_SAVER) || !scrn_idle)
	if (scrn_blanked)
            stop_scrn_saver(current_saver);
#endif /* NSPLASH */
    if (scp != cur_console || blink_in_progress || switch_in_progress)
	return;

    if (!ISGRAPHSC(scp) && !(scp->status & SAVER_RUNNING))
	scrn_update(scp, TRUE);
}

scr_stat
*sc_get_scr_stat(dev_t dev)
{
    int unit = minor(dev);

    if (unit == SC_CONSOLE)
	return console[0];
    if (unit >= MAXCONS || unit < 0)
	return(NULL);
    return console[unit];
}

static int
get_scr_num()
{
    int i = 0;

    while ((i < MAXCONS) && (cur_console != console[i]))
	i++;
    return i < MAXCONS ? i : 0;
}

static void
scrn_timer(void *arg)
{
    struct timeval tv;
    scr_stat *scp;
    int s;

    /* don't do anything when we are touching font */
    if (font_loading_in_progress) {
	if (arg)
	    timeout(scrn_timer, (void *)TRUE, hz / 10);
	return;
    }
    s = spltty();

    /* should we stop the screen saver? */
    getmicrouptime(&tv);
    if (panicstr || shutdown_in_progress)
	sc_touch_scrn_saver();
    if (run_scrn_saver) {
	scrn_idle = (tv.tv_sec > scrn_time_stamp + scrn_blank_time);
    } else {
	scrn_time_stamp = tv.tv_sec;
	scrn_idle = FALSE;
	if (scrn_blank_time > 0)
	    run_scrn_saver = TRUE;
    }
#if NSPLASH > 0
    if ((saver_mode != CONS_LKM_SAVER) || !scrn_idle)
	if (scrn_blanked)
            stop_scrn_saver(current_saver);
#endif /* NSPLASH */
    /* should we just return ? */
    if (blink_in_progress || switch_in_progress) {
	if (arg)
	    timeout(scrn_timer, (void *)TRUE, hz / 10);
	splx(s);
	return;
    }

    /* Update the screen */
    scp = cur_console;
    if (!ISGRAPHSC(scp) && !(scp->status & SAVER_RUNNING))
	scrn_update(scp, TRUE);

    /* should we activate the screen saver? */
    if ((saver_mode == CONS_LKM_SAVER) && scrn_idle)
	if (!ISGRAPHSC(scp) || scrn_blanked)
	    (*current_saver)(TRUE);

    if (arg)
	timeout(scrn_timer, (void *)TRUE, hz / 25);
    splx(s);
}

static void 
scrn_update(scr_stat *scp, int show_cursor)
{
    /* update screen image */
    if (scp->start <= scp->end) {
        sc_bcopy(scp, scp->scr_buf, scp->start, scp->end, 0);
#ifdef PC98
        generic_bcopy(scp->atr_buf + scp->start, Atrat + scp->start,
					  (1 + scp->end - scp->start) * sizeof(u_short));
#endif
    }

    /* we are not to show the cursor and the mouse pointer... */
    if (!show_cursor) {
        scp->end = 0;
        scp->start = scp->xsize*scp->ysize - 1;
	return;
    }

    /* update "pseudo" mouse pointer image */
    if (scp->status & MOUSE_VISIBLE) {
        /* did mouse move since last time ? */
        if (scp->status & MOUSE_MOVED) {
            /* do we need to remove old mouse pointer image ? */
            if (scp->mouse_cut_start != NULL ||
                (scp->mouse_pos-scp->scr_buf) <= scp->start ||
                (scp->mouse_pos+scp->xsize + 1 - scp->scr_buf) >= scp->end) {
                remove_mouse_image(scp);
            }
            scp->status &= ~MOUSE_MOVED;
            draw_mouse_image(scp);
        }
        else {
            /* mouse didn't move, has it been overwritten ? */
            if ((scp->mouse_pos+scp->xsize + 1 - scp->scr_buf) >= scp->start &&
                (scp->mouse_pos - scp->scr_buf) <= scp->end) {
                draw_mouse_image(scp);
            }
        }
    }
	
    /* update cursor image */
    if (scp->status & CURSOR_ENABLED) {
        /* did cursor move since last time ? */
        if (scp->cursor_pos != scp->cursor_oldpos) {
            /* do we need to remove old cursor image ? */
            if ((scp->cursor_oldpos - scp->scr_buf) < scp->start ||
                ((scp->cursor_oldpos - scp->scr_buf) > scp->end)) {
                remove_cursor_image(scp);
            }
            scp->cursor_oldpos = scp->cursor_pos;
            draw_cursor_image(scp);
        }
        else {
            /* cursor didn't move, has it been overwritten ? */
            if (scp->cursor_pos - scp->scr_buf >= scp->start &&
                scp->cursor_pos - scp->scr_buf <= scp->end) {
                draw_cursor_image(scp);
            } else {
                /* if its a blinking cursor, we may have to update it */
		if (sc_flags & BLINK_CURSOR)
                    draw_cursor_image(scp);
            }
        }
        blinkrate++;
    }

    if (scp->mouse_cut_start != NULL)
        draw_cutmarking(scp);

    scp->end = 0;
    scp->start = scp->xsize*scp->ysize - 1;
}

#if NSPLASH > 0

static int
scsplash_callback(int event)
{
    int error;

    switch (event) {
    case SPLASH_INIT:
	scrn_saver_failed = FALSE;
	if (add_scrn_saver(scsplash_saver) == 0) {
	    run_scrn_saver = TRUE;
	    if (cold && !(boothowto & (RB_VERBOSE | RB_CONFIG))) {
		scsplash_stick(TRUE);
		(*current_saver)(TRUE);
	    }
	}
	return 0;

    case SPLASH_TERM:
	if (current_saver == scsplash_saver) {
	    scsplash_stick(FALSE);
	    error = remove_scrn_saver(scsplash_saver);
	    if (error)
		return error;
	}
	return 0;

    default:
	return EINVAL;
    }
}

static void
scsplash_saver(int show)
{
    static int busy = FALSE;
    scr_stat *scp;

    if (busy)
	return;
    busy = TRUE;

    scp = cur_console;
    if (show) {
	if (!scrn_saver_failed) {
	    if (!scrn_blanked)
		set_scrn_saver_mode(scp, -1, NULL, 0);
	    switch (splash(scp->adp, TRUE)) {
	    case 0:		/* succeeded */
		scrn_blanked = TRUE;
		break;
	    case EAGAIN:	/* try later */
		restore_scrn_saver_mode(scp, FALSE);
		break;
	    default:
		scrn_saver_failed = TRUE;
		scsplash_stick(FALSE);
		printf("scsplash_saver(): failed to put up the image\n");
		restore_scrn_saver_mode(scp, TRUE);
		break;
	    }
	}
    } else if (!sticky_splash) {
	if (scrn_blanked && (splash(scp->adp, FALSE) == 0)) {
	    restore_scrn_saver_mode(scp, TRUE);
	    scrn_blanked = FALSE;
	}
    }
    busy = FALSE;
}

static int
add_scrn_saver(void (*this_saver)(int))
{
    int error;

    if (current_saver != none_saver) {
	error = remove_scrn_saver(current_saver);
	if (error)
	    return error;
    }

    run_scrn_saver = FALSE;
    saver_mode = CONS_LKM_SAVER;
    current_saver = this_saver;
    return 0;
}

static int
remove_scrn_saver(void (*this_saver)(int))
{
    if (current_saver != this_saver)
	return EINVAL;

    /*
     * In order to prevent `current_saver' from being called by
     * the timeout routine `scrn_timer()' while we manipulate 
     * the saver list, we shall set `current_saver' to `none_saver' 
     * before stopping the current saver, rather than blocking by `splXX()'.
     */
    current_saver = none_saver;
    if (scrn_blanked)
        stop_scrn_saver(this_saver);

    return (scrn_blanked ? EBUSY : 0);
}

static int
set_scrn_saver_mode(scr_stat *scp, int mode, u_char *pal, int border)
{
    int s;

    /* assert(scp == cur_console) */
    s = spltty();
    scp->splash_save_mode = scp->mode;
    scp->splash_save_status = scp->status & (GRAPHICS_MODE | PIXEL_MODE);
    scp->status &= ~(GRAPHICS_MODE | PIXEL_MODE);
    scp->status |= (UNKNOWN_MODE | SAVER_RUNNING);
    splx(s);
    if (mode < 0)
	return 0;
    scp->mode = mode;
    if (set_mode(scp) == 0) {
	if (scp->adp->va_info.vi_flags & V_INFO_GRAPHICS)
	    scp->status |= GRAPHICS_MODE;
	if (pal != NULL)
	    load_palette(scp->adp, pal);
	set_border(scp, border);
	return 0;
    } else {
	s = spltty();
	scp->mode = scp->splash_save_mode;
	scp->status &= ~(UNKNOWN_MODE | SAVER_RUNNING);
	scp->status |= scp->splash_save_status;
	splx(s);
	return 1;
    }
}

static int
restore_scrn_saver_mode(scr_stat *scp, int changemode)
{
    int mode;
    int status;
    int s;

    /* assert(scp == cur_console) */
    s = spltty();
    mode = scp->mode;
    status = scp->status;
    scp->mode = scp->splash_save_mode;
    scp->status &= ~(UNKNOWN_MODE | SAVER_RUNNING);
    scp->status |= scp->splash_save_status;
    if (!changemode) {
	splx(s);
	return 0;
    }
    if (set_mode(scp) == 0) {
	load_palette(scp->adp, palette);
	splx(s);
	return 0;
    } else {
	scp->mode = mode;
	scp->status = status;
	splx(s);
	return 1;
    }
}

static void
stop_scrn_saver(void (*saver)(int))
{
    (*saver)(FALSE);
    run_scrn_saver = FALSE;
    /* the screen saver may have chosen not to stop after all... */
    if (scrn_blanked)
	return;

    mark_all(cur_console);
    if (delayed_next_scr)
	switch_scr(cur_console, delayed_next_scr - 1);
    wakeup((caddr_t)&scrn_blanked);
}

static int
wait_scrn_saver_stop(void)
{
    int error = 0;

    while (scrn_blanked) {
	run_scrn_saver = FALSE;
	error = tsleep((caddr_t)&scrn_blanked, PZERO | PCATCH, "scrsav", 0);
	run_scrn_saver = FALSE;
	if (error != ERESTART)
	    break;
    }
    return error;
}

#endif /* NSPLASH */

void
sc_touch_scrn_saver(void)
{
    scsplash_stick(FALSE);
    run_scrn_saver = FALSE;
}

void
sc_clear_screen(scr_stat *scp)
{
    move_crsr(scp, 0, 0);
    scp->cursor_oldpos = scp->cursor_pos;
#ifdef PC98
    fillw(scr_map[0x20], scp->scr_buf,
	  scp->xsize * scp->ysize);
    fillw(at2pc98(scp->term.cur_color), scp->atr_buf,
	  scp->xsize * scp->ysize);
#else
    fillw(scp->term.cur_color | scr_map[0x20], scp->scr_buf,
	  scp->xsize * scp->ysize);
#endif
    mark_all(scp);
    remove_cutmarking(scp);
}

static int
switch_scr(scr_stat *scp, u_int next_scr)
{
    /* delay switch if actively updating screen */
    if (scrn_blanked || write_in_progress || blink_in_progress) {
	delayed_next_scr = next_scr+1;
	sc_touch_scrn_saver();
	return 0;
    }

    if (switch_in_progress && (cur_console->proc != pfind(cur_console->pid)))
	switch_in_progress = FALSE;

    if (next_scr >= MAXCONS || switch_in_progress ||
	(cur_console->smode.mode == VT_AUTO && ISGRAPHSC(cur_console))) {
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

    switch_in_progress = TRUE;
    old_scp = cur_console;
    new_scp = console[next_scr];
    wakeup((caddr_t)&new_scp->smode);
    if (new_scp == old_scp) {
	switch_in_progress = FALSE;
	delayed_next_scr = FALSE;
	return 0;
    }

    /* has controlling process died? */
    if (old_scp->proc && (old_scp->proc != pfind(old_scp->pid)))
	old_scp->smode.mode = VT_AUTO;
    if (new_scp->proc && (new_scp->proc != pfind(new_scp->pid)))
	new_scp->smode.mode = VT_AUTO;

    /* check the modes and switch appropriately */
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
	    switch_in_progress = FALSE;
    }
    return 0;
}

static void
exchange_scr(void)
{
    /* save the current state of video and keyboard */
    move_crsr(old_scp, old_scp->xpos, old_scp->ypos);
    if (old_scp->kbd_mode == K_XLATE)
	save_kbd_state(old_scp);
 
    /* set up the video for the new screen */
    cur_console = new_scp;
#ifdef PC98
    if (old_scp->mode != new_scp->mode || ISUNKNOWNSC(old_scp) || ISUNKNOWNSC(new_scp))
#else
    if (old_scp->mode != new_scp->mode || ISUNKNOWNSC(old_scp))
#endif
	set_mode(new_scp);
    move_crsr(new_scp, new_scp->xpos, new_scp->ypos);
#ifndef PC98
    if (ISTEXTSC(new_scp) && (sc_flags & CHAR_CURSOR))
	set_destructive_cursor(new_scp);
    if (ISGRAPHSC(old_scp))
	load_palette(new_scp->adp, palette);
#endif
    set_border(new_scp, new_scp->border);

    /* set up the keyboard for the new screen */
    if (old_scp->kbd_mode != new_scp->kbd_mode)
	kbd_ioctl(kbd, KDSKBMODE, (caddr_t)&new_scp->kbd_mode);
    update_kbd_state(new_scp->status, LOCK_MASK);

    delayed_next_scr = FALSE;
    mark_all(new_scp);
}

static void
scan_esc(scr_stat *scp, u_char c)
{
    static u_char ansi_col[16] =
	{0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
    int i, n;
    u_short *src, *dst, count;
#ifdef PC98
    u_short *src_attr, *dst_attr;
#endif

    if (scp->term.esc == 1) {	/* seen ESC */
#ifdef KANJI
	switch (scp->kanji_type) {
	case KTYPE_KANIN:	/* Kanji Invoke sequence */
	    switch (c) {
	    case 'B':
	    case '@':
		scp->kanji_type = KTYPE_7JIS;
		scp->term.esc = 0;
		scp->kanji_1st_char = 0;
		return;
	    default:
		scp->kanji_type = KTYPE_ASCII;
		scp->term.esc = 0;
		break;
	    }
	    break;
	case KTYPE_ASCIN:	/* Ascii Invoke sequence */
	    switch (c) {
	    case 'J':
	    case 'B':
	    case 'H':
		scp->kanji_type = KTYPE_ASCII;
		scp->term.esc = 0;
		scp->kanji_1st_char = 0;
		return;
	    case 'I':
		scp->kanji_type = KTYPE_JKANA;
		scp->term.esc = 0;
		scp->kanji_1st_char = 0;
		return;
	    default:
		scp->kanji_type = KTYPE_ASCII;
		scp->term.esc = 0;
		break;
	    }
	    break;
	default:
	    break;
	}
#endif
	switch (c) {

	case '7':   /* Save cursor position */
	    scp->saved_xpos = scp->xpos;
	    scp->saved_ypos = scp->ypos;
	    break;

	case '8':   /* Restore saved cursor position */
	    if (scp->saved_xpos >= 0 && scp->saved_ypos >= 0)
		move_crsr(scp, scp->saved_xpos, scp->saved_ypos);
	    break;

	case '[':   /* Start ESC [ sequence */
	    scp->term.esc = 2;
	    scp->term.last_param = -1;
	    for (i = scp->term.num_param; i < MAX_ESC_PAR; i++)
		scp->term.param[i] = 1;
	    scp->term.num_param = 0;
	    return;

#ifdef KANJI
	case '$':	/* Kanji Invoke sequence */
	    scp->kanji_type = KTYPE_KANIN;
	    return;
#endif

	case 'M':   /* Move cursor up 1 line, scroll if at top */
	    if (scp->ypos > 0)
		move_crsr(scp, scp->xpos, scp->ypos - 1);
	    else {
#ifdef PC98
		bcopy(scp->scr_buf, scp->scr_buf + scp->xsize,
		       (scp->ysize - 1) * scp->xsize * sizeof(u_short));
		bcopy(scp->atr_buf, scp->atr_buf + scp->xsize,
		       (scp->ysize - 1) * scp->xsize * sizeof(u_short));
		fillw(scr_map[0x20],
		      scp->scr_buf, scp->xsize);
		fillw(at2pc98(scp->term.cur_color),
		      scp->atr_buf, scp->xsize);
#else
		bcopy(scp->scr_buf, scp->scr_buf + scp->xsize,
		       (scp->ysize - 1) * scp->xsize * sizeof(u_short));
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->scr_buf, scp->xsize);
#endif
    		mark_all(scp);
	    }
	    break;
#if notyet
	case 'Q':
	    scp->term.esc = 4;
	    return;
#endif
	case 'c':   /* Clear screen & home */
	    sc_clear_screen(scp);
	    break;

	case '(':   /* iso-2022: designate 94 character set to G0 */
#ifdef KANJI
	    scp->kanji_type = KTYPE_ASCIN;
#else
	    scp->term.esc = 5;
#endif
	    return;
	}
    }
    else if (scp->term.esc == 2) {	/* seen ESC [ */
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

	case 'A':   /* up n rows */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos, scp->ypos - n);
	    break;

	case 'B':   /* down n rows */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos, scp->ypos + n);
	    break;

	case 'C':   /* right n columns */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos + n, scp->ypos);
	    break;

	case 'D':   /* left n columns */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, scp->xpos - n, scp->ypos);
	    break;

	case 'E':   /* cursor to start of line n lines down */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, 0, scp->ypos + n);
	    break;

	case 'F':   /* cursor to start of line n lines up */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    move_crsr(scp, 0, scp->ypos - n);
	    break;

	case 'f':   /* Cursor move */
	case 'H':
	    if (scp->term.num_param == 0)
		move_crsr(scp, 0, 0);
	    else if (scp->term.num_param == 2)
		move_crsr(scp, scp->term.param[1] - 1, scp->term.param[0] - 1);
	    break;

	case 'J':   /* Clear all or part of display */
	    if (scp->term.num_param == 0)
		n = 0;
	    else
		n = scp->term.param[0];
	    switch (n) {
	    case 0: /* clear form cursor to end of display */
#ifdef PC98
		fillw(scr_map[0x20],
		      scp->cursor_pos,
		      scp->scr_buf + scp->xsize * scp->ysize - scp->cursor_pos);
		fillw(at2pc98(scp->term.cur_color),
		      scp->cursor_atr,
		      scp->atr_buf + scp->xsize * scp->ysize - scp->cursor_atr);
#else
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos,
		      scp->scr_buf + scp->xsize * scp->ysize - scp->cursor_pos);
#endif
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
    		mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
    		mark_for_update(scp, scp->xsize * scp->ysize - 1);
		remove_cutmarking(scp);
		break;
	    case 1: /* clear from beginning of display to cursor */
#ifdef PC98
		fillw(scr_map[0x20],
		      scp->scr_buf,
		      scp->cursor_pos - scp->scr_buf);
		fillw(at2pc98(scp->term.cur_color),
		      scp->atr_buf,
		      scp->cursor_atr - scp->atr_buf);
#else
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->scr_buf,
		      scp->cursor_pos - scp->scr_buf);
#endif
    		mark_for_update(scp, 0);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
    		mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
		remove_cutmarking(scp);
		break;
	    case 2: /* clear entire display */
#ifdef PC98
		fillw(scr_map[0x20], scp->scr_buf,
		      scp->xsize * scp->ysize);
		fillw(at2pc98(scp->term.cur_color), scp->atr_buf,
		      scp->xsize * scp->ysize);
#else
		fillw(scp->term.cur_color | scr_map[0x20], scp->scr_buf,
		      scp->xsize * scp->ysize);
#endif
		mark_all(scp);
		remove_cutmarking(scp);
		break;
	    }
	    break;

	case 'K':   /* Clear all or part of line */
	    if (scp->term.num_param == 0)
		n = 0;
	    else
		n = scp->term.param[0];
	    switch (n) {
	    case 0: /* clear form cursor to end of line */
#ifdef PC98
		fillw(scr_map[0x20],
		      scp->cursor_pos,
		      scp->xsize - scp->xpos);
		fillw(at2pc98(scp->term.cur_color),
		      scp->cursor_atr,
		      scp->xsize - scp->xpos);
#else
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos,
		      scp->xsize - scp->xpos);
#endif
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
    		mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf +
				scp->xsize - 1 - scp->xpos);
#ifdef PC98
    		mark_for_update(scp, scp->cursor_atr - scp->atr_buf +
				scp->xsize - 1 - scp->xpos);
#endif
		break;
	    case 1: /* clear from beginning of line to cursor */
#ifdef PC98
		fillw(scr_map[0x20],
		      scp->cursor_pos - scp->xpos,
		      scp->xpos + 1);
		fillw(at2pc98(scp->term.cur_color),
		      scp->cursor_atr - scp->xpos,
		      scp->xpos + 1);
#else
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos - scp->xpos,
		      scp->xpos + 1);
#endif
    		mark_for_update(scp, scp->ypos * scp->xsize);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
    		mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
		break;
	    case 2: /* clear entire line */
#ifdef PC98
		fillw(scr_map[0x20],
		      scp->cursor_pos - scp->xpos,
		      scp->xsize);
		fillw(at2pc98(scp->term.cur_color),
		      scp->cursor_atr - scp->xpos,
		      scp->xsize);
#else
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos - scp->xpos,
		      scp->xsize);
#endif
    		mark_for_update(scp, scp->ypos * scp->xsize);
    		mark_for_update(scp, (scp->ypos + 1) * scp->xsize - 1);
		break;
	    }
	    break;

	case 'L':   /* Insert n lines */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->ysize - scp->ypos)
		n = scp->ysize - scp->ypos;
	    src = scp->scr_buf + scp->ypos * scp->xsize;
	    dst = src + n * scp->xsize;
	    count = scp->ysize - (scp->ypos + n);
	    bcopy(src, dst, count * scp->xsize * sizeof(u_short));
#ifdef PC98
	    src_attr = scp->atr_buf + scp->ypos * scp->xsize;
	    dst_attr = src_attr + n * scp->xsize;
	    bcopy(src_attr, dst_attr, count * scp->xsize * sizeof(u_short));
	    fillw(scr_map[0x20], src,
		  n * scp->xsize);
	    fillw(at2pc98(scp->term.cur_color), src_attr,
		  n * scp->xsize);
#else
	    fillw(scp->term.cur_color | scr_map[0x20], src,
		  n * scp->xsize);
#endif
	    mark_for_update(scp, scp->ypos * scp->xsize);
	    mark_for_update(scp, scp->xsize * scp->ysize - 1);
	    break;

	case 'M':   /* Delete n lines */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->ysize - scp->ypos)
		n = scp->ysize - scp->ypos;
	    dst = scp->scr_buf + scp->ypos * scp->xsize;
	    src = dst + n * scp->xsize;
	    count = scp->ysize - (scp->ypos + n);
	    bcopy(src, dst, count * scp->xsize * sizeof(u_short));
	    src = dst + count * scp->xsize;
#ifdef PC98
	    dst_attr = scp->atr_buf + scp->ypos * scp->xsize;
	    src_attr = dst_attr + n * scp->xsize;
	    bcopy(src_attr, dst_attr, count * scp->xsize * sizeof(u_short));
	    src_attr = dst_attr + count * scp->xsize;
	    fillw(scr_map[0x20], src,
		  n * scp->xsize);
	    fillw(at2pc98(scp->term.cur_color), src_attr,
		  n * scp->xsize);
#else
	    fillw(scp->term.cur_color | scr_map[0x20], src,
		  n * scp->xsize);
#endif
	    mark_for_update(scp, scp->ypos * scp->xsize);
	    mark_for_update(scp, scp->xsize * scp->ysize - 1);
	    break;

	case 'P':   /* Delete n chars */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    dst = scp->cursor_pos;
	    src = dst + n;
	    count = scp->xsize - (scp->xpos + n);
	    bcopy(src, dst, count * sizeof(u_short));
	    src = dst + count;
#ifdef PC98
	    dst_attr = scp->cursor_atr;
	    src_attr = dst_attr + n;
	    bcopy(src_attr, dst_attr, count * sizeof(u_short));
	    src_attr = dst_attr + count;
	    fillw(scr_map[0x20], src, n);
	    fillw(at2pc98(scp->term.cur_color), src_attr, n);
#else
	    fillw(scp->term.cur_color | scr_map[0x20], src, n);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf + n + count - 1);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf + n + count - 1);
#endif
	    break;

	case '@':   /* Insert n chars */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    src = scp->cursor_pos;
	    dst = src + n;
	    count = scp->xsize - (scp->xpos + n);
	    bcopy(src, dst, count * sizeof(u_short));
#ifdef PC98
	    src_attr = scp->cursor_atr;
	    dst_attr = src_attr + n;
	    bcopy(src_attr, dst_attr, count * sizeof(u_short));
	    fillw(scr_map[0x20], src, n);
	    fillw(at2pc98(scp->term.cur_color), src_attr, n);
#else
	    fillw(scp->term.cur_color | scr_map[0x20], src, n);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf + n + count - 1);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf + n + count - 1);
#endif
	    break;

	case 'S':   /* scroll up n lines */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->ysize)
		n = scp->ysize;
	    bcopy(scp->scr_buf + (scp->xsize * n),
		   scp->scr_buf,
		   scp->xsize * (scp->ysize - n) * sizeof(u_short));
#ifdef PC98
	    bcopy(scp->atr_buf + (scp->xsize * n),
		   scp->atr_buf,
		   scp->xsize * (scp->ysize - n) * sizeof(u_short));
	    fillw(scr_map[0x20],
		  scp->scr_buf + scp->xsize * (scp->ysize - n),
		  scp->xsize * n);
	    fillw(at2pc98(scp->term.cur_color),
		  scp->atr_buf + scp->xsize * (scp->ysize - n),
		  scp->xsize * n);
#else
	    fillw(scp->term.cur_color | scr_map[0x20],
		  scp->scr_buf + scp->xsize * (scp->ysize - n),
		  scp->xsize * n);
#endif
    	    mark_all(scp);
	    break;

	case 'T':   /* scroll down n lines */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->ysize)
		n = scp->ysize;
	    bcopy(scp->scr_buf,
		  scp->scr_buf + (scp->xsize * n),
		  scp->xsize * (scp->ysize - n) *
		  sizeof(u_short));
#ifdef PC98
	    bcopy(scp->atr_buf,
		  scp->atr_buf + (scp->xsize * n),
		  scp->xsize * (scp->ysize - n) *
		  sizeof(u_short));
	    fillw(scr_map[0x20],
		  scp->scr_buf, scp->xsize * n);
	    fillw(at2pc98(scp->term.cur_color),
		  scp->atr_buf, scp->xsize * n);
#else
	    fillw(scp->term.cur_color | scr_map[0x20],
		  scp->scr_buf, scp->xsize * n);
#endif
    	    mark_all(scp);
	    break;

	case 'X':   /* erase n characters in line */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
#ifdef PC98
	    fillw(scr_map[0x20],
		  scp->cursor_pos, n);
	    fillw(at2pc98(scp->term.cur_color),
		  scp->cursor_atr, n);
#else
	    fillw(scp->term.cur_color | scr_map[0x20],
		  scp->cursor_pos, n);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf + n - 1);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf + n - 1);
#endif
	    break;

	case 'Z':   /* move n tabs backwards */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if ((i = scp->xpos & 0xf8) == scp->xpos)
		i -= 8*n;
	    else
		i -= 8*(n-1);
	    if (i < 0)
		i = 0;
	    move_crsr(scp, i, scp->ypos);
	    break;

	case '`':   /* move cursor to column n */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, n - 1, scp->ypos);
	    break;

	case 'a':   /* move cursor n columns to the right */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, scp->xpos + n, scp->ypos);
	    break;

	case 'd':   /* move cursor to row n */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, scp->xpos, n - 1);
	    break;

	case 'e':   /* move cursor n rows down */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    move_crsr(scp, scp->xpos, scp->ypos + n);
	    break;

	case 'm':   /* change attribute */
	    if (scp->term.num_param == 0) {
		scp->term.attr_mask = NORMAL_ATTR;
		scp->term.cur_attr =
		    scp->term.cur_color = scp->term.std_color;
		break;
	    }
	    for (i = 0; i < scp->term.num_param; i++) {
		switch (n = scp->term.param[i]) {
		case 0: /* back to normal */
		    scp->term.attr_mask = NORMAL_ATTR;
		    scp->term.cur_attr =
			scp->term.cur_color = scp->term.std_color;
		    break;
		case 1: /* bold */
		    scp->term.attr_mask |= BOLD_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 4: /* underline */
		    scp->term.attr_mask |= UNDERLINE_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 5: /* blink */
		    scp->term.attr_mask |= BLINK_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 7: /* reverse video */
		    scp->term.attr_mask |= REVERSE_ATTR;
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 30: case 31: /* set fg color */
		case 32: case 33: case 34:
		case 35: case 36: case 37:
		    scp->term.attr_mask |= FOREGROUND_CHANGED;
		    scp->term.cur_color =
			(scp->term.cur_color&0xF000) | (ansi_col[(n-30)&7]<<8);
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		case 40: case 41: /* set bg color */
		case 42: case 43: case 44:
		case 45: case 46: case 47:
		    scp->term.attr_mask |= BACKGROUND_CHANGED;
		    scp->term.cur_color =
			(scp->term.cur_color&0x0F00) | (ansi_col[(n-40)&7]<<12);
		    scp->term.cur_attr = mask2attr(&scp->term);
		    break;
		}
	    }
	    break;

	case 's':   /* Save cursor position */
	    scp->saved_xpos = scp->xpos;
	    scp->saved_ypos = scp->ypos;
	    break;

	case 'u':   /* Restore saved cursor position */
	    if (scp->saved_xpos >= 0 && scp->saved_ypos >= 0)
		move_crsr(scp, scp->saved_xpos, scp->saved_ypos);
	    break;

	case 'x':
	    if (scp->term.num_param == 0)
		n = 0;
	    else
		n = scp->term.param[0];
	    switch (n) {
	    case 0:     /* reset attributes */
		scp->term.attr_mask = NORMAL_ATTR;
		scp->term.cur_attr =
		    scp->term.cur_color = scp->term.std_color =
		    current_default->std_color;
		scp->term.rev_color = current_default->rev_color;
		break;
	    case 1:     /* set ansi background */
		scp->term.attr_mask &= ~BACKGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0x0F00) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<12);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 2:     /* set ansi foreground */
		scp->term.attr_mask &= ~FOREGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0xF000) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<8);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 3:     /* set ansi attribute directly */
		scp->term.attr_mask &= ~(FOREGROUND_CHANGED|BACKGROUND_CHANGED);
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.param[1]&0xFF)<<8;
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 5:     /* set ansi reverse video background */
		scp->term.rev_color =
		    (scp->term.rev_color & 0x0F00) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<12);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 6:     /* set ansi reverse video foreground */
		scp->term.rev_color =
		    (scp->term.rev_color & 0xF000) |
		    (ansi_col[(scp->term.param[1])&0x0F]<<8);
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    case 7:     /* set ansi reverse video directly */
		scp->term.rev_color =
		    (scp->term.param[1]&0xFF)<<8;
		scp->term.cur_attr = mask2attr(&scp->term);
		break;
	    }
	    break;

	case 'z':   /* switch to (virtual) console n */
	    if (scp->term.num_param == 1)
		switch_scr(scp, scp->term.param[0]);
	    break;
	}
    }
    else if (scp->term.esc == 3) {	/* seen ESC [0-9]+ = */
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

	case 'A':   /* set display border color */
	    if (scp->term.num_param == 1) {
		scp->border=scp->term.param[0] & 0xff;
		if (scp == cur_console)
		    set_border(cur_console, scp->border);
            }
	    break;

	case 'B':   /* set bell pitch and duration */
	    if (scp->term.num_param == 2) {
		scp->bell_pitch = scp->term.param[0];
		scp->bell_duration = scp->term.param[1];
	    }
	    break;

	case 'C':   /* set cursor type & shape */
	    if (scp->term.num_param == 1) {
		if (scp->term.param[0] & 0x01)
		    sc_flags |= BLINK_CURSOR;
		else
		    sc_flags &= ~BLINK_CURSOR;
#ifdef PC98
		if (scp->term.param[0] & 0x02) 
		    sc_flags |= CHAR_CURSOR;
		else
		    sc_flags &= ~CHAR_CURSOR;
#else	/* PC98 */
		if ((scp->term.param[0] & 0x02) 
		    && ISFONTAVAIL(scp->adp->va_flags)) 
		    sc_flags |= CHAR_CURSOR;
		else
		    sc_flags &= ~CHAR_CURSOR;
#endif	/* PC98 */
	    }
	    else if (scp->term.num_param == 2) {
		scp->cursor_start = scp->term.param[0] & 0x1F;
		scp->cursor_end = scp->term.param[1] & 0x1F;
	    }
	    /* 
	     * The cursor shape is global property; all virtual consoles
	     * are affected. Update the cursor in the current console...
	     */
	    if (!ISGRAPHSC(cur_console)) {
		i = spltty();
		remove_cursor_image(cur_console);
		if (sc_flags & CHAR_CURSOR)
	            set_destructive_cursor(cur_console);
		draw_cursor_image(cur_console);
		splx(i);
	    }
	    break;

	case 'F':   /* set ansi foreground */
	    if (scp->term.num_param == 1) {
		scp->term.attr_mask &= ~FOREGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0xF000)
		    | ((scp->term.param[0] & 0x0F) << 8);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;

	case 'G':   /* set ansi background */
	    if (scp->term.num_param == 1) {
		scp->term.attr_mask &= ~BACKGROUND_CHANGED;
		scp->term.cur_color = scp->term.std_color =
		    (scp->term.std_color & 0x0F00)
		    | ((scp->term.param[0] & 0x0F) << 12);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;

	case 'H':   /* set ansi reverse video foreground */
	    if (scp->term.num_param == 1) {
		scp->term.rev_color =
		    (scp->term.rev_color & 0xF000)
		    | ((scp->term.param[0] & 0x0F) << 8);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;

	case 'I':   /* set ansi reverse video background */
	    if (scp->term.num_param == 1) {
		scp->term.rev_color =
		    (scp->term.rev_color & 0x0F00)
		    | ((scp->term.param[0] & 0x0F) << 12);
		scp->term.cur_attr = mask2attr(&scp->term);
	    }
	    break;
	}
    }
#if notyet
    else if (scp->term.esc == 4) {	/* seen ESC Q */
	/* to be filled */
    }
#endif
    else if (scp->term.esc == 5) {	/* seen ESC ( */
	switch (c) {
	case 'B':   /* iso-2022: desginate ASCII into G0 */
	    break;
	/* other items to be filled */
	default:
	    break;
	}
    }
    scp->term.esc = 0;
}

#ifdef KANJI
static u_char iskanji1(u_char mode, u_char c)
{
    if ((mode == KTYPE_7JIS) && (c >= 0x21) && (c <= 0x7e)) {
	/* JIS */
	default_kanji = UJIS;
	return KTYPE_7JIS;
    }

    if ((mode == KTYPE_JKANA) && (c >= 0x21) && (c <= 0x5f)) {
	/* JIS HANKAKU */
	default_kanji = UJIS;
	return KTYPE_JKANA;
    }

#if 1
    if ((c >= 0xa1) && (c <= 0xdf) && (default_kanji == UJIS)) {
	/* UJIS */
	return KTYPE_UJIS;
    }
#endif

    if ((c >= 0x81) && (c <= 0x9f) && (c != 0x8e)) {
	/* SJIS */
	default_kanji = SJIS;
	return KTYPE_SJIS;
    }

    if ((c >= 0xa1) && (c <= 0xdf) && (default_kanji == SJIS)) {
	/* SJIS HANKAKU */
	return KTYPE_KANA;
    }

#if 0
    if ((c >= 0xa1) && (c <= 0xdf) && (default_kanji == UJIS)) {
	/* UJIS */
	return KTYPE_UJIS;
    }
#endif

    if ((c >= 0xf0) && (c <= 0xfe)) {
	/* UJIS */
	default_kanji = UJIS;
	return KTYPE_UJIS;
    }

    if ((c >= 0xe0) && (c <= 0xef)) {
	/* SJIS or UJIS */
	return KTYPE_SUJIS;
    }

    if (c == 0x8e) {
	/* SJIS or UJIS HANKAKU */
	return KTYPE_SUKANA;
    }

    return KTYPE_ASCII;
}

static u_char iskanji2(u_char mode, u_char c)
{
    switch (mode) {
    case KTYPE_7JIS:
	if ((c >= 0x21) && (c <= 0x7e)) {
	    /* JIS */
	    return KTYPE_7JIS;
	}
	break;
    case KTYPE_SJIS:
	if ((c >= 0x40) && (c <= 0xfc) && (c != 0x7f)) {
	    /* SJIS */
	    return KTYPE_SJIS;
	}
	break;
    case KTYPE_UJIS:
	if ((c >= 0xa1) && (c <= 0xfe)) {
	    /* UJIS */
	    return KTYPE_UJIS;
	}
	break;
    case KTYPE_SUKANA:
	if ((c >= 0xa1) && (c <= 0xdf) && (default_kanji == UJIS)) {
	    /* UJIS HANKAKU */
	    return KTYPE_KANA;
	}
	if ((c >= 0x40) && (c <= 0xfc) && (c != 0x7f)) {
	    /* SJIS */
	    default_kanji = SJIS;
	    return KTYPE_SJIS;
	}
	break;
    case KTYPE_SUJIS:
	if ((c >= 0x40) && (c <= 0xa0) && (c != 0x7f)) {
	    /* SJIS */
	    default_kanji = SJIS;
	    return KTYPE_SJIS;
	}
	if ((c == 0xfd) || (c == 0xfe)) {
	    /* UJIS */
	    default_kanji = UJIS;
	    return KTYPE_UJIS;
	}
	if ((c >= 0xa1) && (c <= 0xfc)) {
	    if (default_kanji == SJIS)
		return KTYPE_SJIS;
	    if (default_kanji == UJIS)
		return KTYPE_UJIS;
	}
	break;
    }
    return KTYPE_ASCII;
}

/*
 * JIS X0208-83 keisen conversion table
 */
static u_short keiConv[32] = {
	0x240c, 0x260c, 0x300c, 0x340c, 0x3c0c, 0x380c, 0x400c, 0x500c,
	0x480c, 0x580c, 0x600c, 0x250c, 0x270c, 0x330c, 0x370c, 0x3f0c,
	0x3b0c, 0x470c, 0x570c, 0x4f0c, 0x5f0c, 0x6f0c, 0x440c, 0x530c,
	0x4c0c, 0x5b0c, 0x630c, 0x410c, 0x540c, 0x490c, 0x5c0c, 0x660c
};


static u_short kanji_convert(u_char mode, u_char h, u_char l)
{
    u_short tmp, high, low, c;
    high = (u_short) h;
    low  = (u_short) l;

    switch (mode) {
    case KTYPE_SJIS: /* SHIFT JIS */
	if (low >= 0xe0) {
	    low -= 0x40;
	}
	low = (low - 0x81) * 2 + 0x21;
	if (high > 0x7f) {
	    high--;
	}
	if (high > 0x9d) {
	    low++;
	    high -= 0x9e - 0x21;
	} else {
	    high -= 0x40 - 0x21;
	}
	high &= 0x7F;
	low  &= 0x7F;
	tmp = ((high << 8) | low) - 0x20;
	break;
    case KTYPE_7JIS: /* JIS */
    case KTYPE_UJIS: /* UJIS */
	high &= 0x7F;
	low &= 0x7F;
	tmp = ((high << 8) | low) - 0x20;
	break;
    default:
	tmp = 0;
	break;
    }

    /* keisen */
    c = ((tmp & 0xff) << 8) | (tmp >> 8);
    /* 0x2821 .. 0x2840 */
    if (0x0821 <= c && c <= 0x0840)
    tmp = keiConv[c - 0x0821];

    return (tmp);
}
#endif

static void
ansi_put(scr_stat *scp, u_char *buf, int len)
{
    u_char *ptr = buf;
#ifdef KANJI
    u_short i, kanji_code;
#endif

    /* make screensaver happy */
    if (!sticky_splash && scp == cur_console)
	run_scrn_saver = FALSE;

    write_in_progress++;
outloop:
    if (scp->term.esc) {
	scan_esc(scp, *ptr++);
	len--;
    }
    else if (PRINTABLE(*ptr)) {     /* Print only printables */
#ifndef PC98
 	int cnt = len <= (scp->xsize-scp->xpos) ? len : (scp->xsize-scp->xpos);
#endif
 	u_short cur_attr = scp->term.cur_attr;
 	u_short *cursor_pos = scp->cursor_pos;
#ifdef PC98
	u_char c = *ptr;
	u_short *cursor_atr = scp->cursor_atr;
#ifdef KANJI
	if (scp->kanji_1st_char == 0) {
	    scp->kanji_type = iskanji1(scp->kanji_type, c);
	    if (!IS_KTYPE_ASCII_or_HANKAKU(scp->kanji_type)) {
		/* not Ascii & not HANKAKU */
		scp->kanji_1st_char = c;
		ptr++; len--;
		goto kanji_end;
	    } else {
		scp->kanji_1st_char = 0;
	    }
	} else {
	    if ((scp->kanji_type = iskanji2(scp->kanji_type, c)) & 0xee) {
		/* print kanji on TEXT VRAM */
		kanji_code = kanji_convert(scp->kanji_type, c, scp->kanji_1st_char);
		for (i=0; i<2; i++){
		    /* *cursor_pos = (kanji_code | (i*0x80)); */
		    *cursor_pos = kanji_code | ((i==0) ? 0x00 : 0x80);
		    *cursor_atr = (at2pc98(cur_attr));
		    cursor_pos++;
		    cursor_atr++;
		    if (++scp->xpos >= scp->xsize) {
			scp->xpos = 0;
			scp->ypos++;
		    }
		}
		KTYPE_MASK_CTRL(scp->kanji_type);
		scp->kanji_1st_char = 0;
		ptr++; len--;
		goto kanji_end;
	    } else {
		scp->kanji_1st_char = 0;
	    }
	}				
	if (IS_KTYPE_KANA(scp->kanji_type))
		c |= 0x80;
	KTYPE_MASK_CTRL(scp->kanji_type);
#endif /* KANJI */
	*cursor_pos++ = (scr_map[c]);
	*cursor_atr++ = at2pc98(cur_attr);
	ptr++;
#else
	do {
	    /*
	     * gcc-2.6.3 generates poor (un)sign extension code.  Casting the
	     * pointers in the following to volatile should have no effect,
	     * but in fact speeds up this inner loop from 26 to 18 cycles
	     * (+ cache misses) on i486's.
	     */
#define	UCVP(ucp)	((u_char volatile *)(ucp))
	    *cursor_pos++ = UCVP(scr_map)[*UCVP(ptr)] | cur_attr;
	    ptr++;
	    cnt--;
	} while (cnt && PRINTABLE(*ptr));
#endif /* PC98 */
	len -= (cursor_pos - scp->cursor_pos);
	scp->xpos += (cursor_pos - scp->cursor_pos);
#ifdef KANJI
kanji_end:
#endif
	mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	mark_for_update(scp, cursor_pos - scp->scr_buf);
#ifdef PC98
	mark_for_update(scp, cursor_atr - scp->atr_buf);
#endif
	scp->cursor_pos = cursor_pos;
#ifdef PC98
	scp->cursor_atr = cursor_atr;
#endif
	if (scp->xpos >= scp->xsize) {
	    scp->xpos = 0;
	    scp->ypos++;
	}
    }
    else  {
	switch(*ptr) {
	case 0x07:
	    do_bell(scp, scp->bell_pitch, scp->bell_duration);
	    break;

	case 0x08:      /* non-destructive backspace */
	    if (scp->cursor_pos > scp->scr_buf) {
	    	mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    	mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
		scp->cursor_pos--;
#ifdef PC98
		scp->cursor_atr--;
#endif
	    	mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    	mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
		if (scp->xpos > 0)
		    scp->xpos--;
		else {
		    scp->xpos += scp->xsize - 1;
		    scp->ypos--;
		}
	    }
	    break;

	case 0x09:  /* non-destructive tab */
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    scp->cursor_pos += (8 - scp->xpos % 8u);
#ifdef PC98
	    scp->cursor_atr += (8 - scp->xpos % 8u);
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    if ((scp->xpos += (8 - scp->xpos % 8u)) >= scp->xsize) {
	        scp->xpos = 0;
	        scp->ypos++;
	    }
	    break;

	case 0x0a:  /* newline, same pos */
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    scp->cursor_pos += scp->xsize;
#ifdef PC98
	    scp->cursor_atr += scp->xsize;
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    scp->ypos++;
	    break;

	case 0x0c:  /* form feed, clears screen */
	    sc_clear_screen(scp);
	    break;

	case 0x0d:  /* return, return to pos 0 */
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    scp->cursor_pos -= scp->xpos;
#ifdef PC98
	    scp->cursor_atr -= scp->xpos;
#endif
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
#ifdef PC98
	    mark_for_update(scp, scp->cursor_atr - scp->atr_buf);
#endif
	    scp->xpos = 0;
	    break;

#ifdef PC98
	case 0x0e:		/* ^N */
	    scp->kanji_type = KTYPE_JKANA;
	    scp->term.esc = 0;
	    scp->kanji_1st_char = 0;
	    break;

	case 0x0f:		/* ^O */
	    scp->kanji_type = KTYPE_ASCII;
	    scp->term.esc = 0;
	    scp->kanji_1st_char = 0;
	    break;
#endif

	case 0x1b:  /* start escape sequence */
	    scp->term.esc = 1;
	    scp->term.num_param = 0;
	    break;
	}
	ptr++; len--;
    }
    /* do we have to scroll ?? */
    if (scp->cursor_pos >= scp->scr_buf + scp->ysize * scp->xsize) {
	remove_cutmarking(scp);
	if (scp->history != NULL) {
	    bcopy(scp->scr_buf, scp->history_head,
		   scp->xsize * sizeof(u_short));
	    scp->history_head += scp->xsize;
#ifdef PC98
	    bcopy(scp->atr_buf, scp->his_atr_head,
		   scp->xsize * sizeof(u_short));
	    scp->his_atr_head += scp->xsize;
#endif
	    if (scp->history_head + scp->xsize >
		scp->history + scp->history_size)
#ifdef PC98
	    {
#endif
		scp->history_head = scp->history;
#ifdef PC98
		scp->his_atr_head = scp->his_atr;
	    }
#endif
	}
	bcopy(scp->scr_buf + scp->xsize, scp->scr_buf,
	       scp->xsize * (scp->ysize - 1) * sizeof(u_short));
#ifdef PC98
	bcopy(scp->atr_buf + scp->xsize, scp->atr_buf,
	       scp->xsize * (scp->ysize - 1) * sizeof(u_short));
	fillw(scr_map[0x20],
	      scp->scr_buf + scp->xsize * (scp->ysize - 1),
	      scp->xsize);	
	fillw(at2pc98(scp->term.cur_color),
	      scp->atr_buf + scp->xsize * (scp->ysize - 1),
	      scp->xsize);
#else
	fillw(scp->term.cur_color | scr_map[0x20],
	      scp->scr_buf + scp->xsize * (scp->ysize - 1),
	      scp->xsize);
#endif

	scp->cursor_pos -= scp->xsize;
#ifdef PC98
	scp->cursor_atr -= scp->xsize;
#endif
	scp->ypos--;
    	mark_all(scp);
    }
    if (len)
	goto outloop;
    write_in_progress--;
    if (delayed_next_scr)
	switch_scr(scp, delayed_next_scr - 1);
}

static void
scinit(void)
{
    video_adapter_t *adp;
    int col;
    int row;
    u_int i;

    if (init_done != COLD)
	return;
    init_done = WARM;

    get_bios_values();

#ifdef PC98
    if (pc98_machine_type & M_8M)
	BELL_PITCH = 1339;
    else
	BELL_PITCH = 1678;
#endif

    /* extract the hardware cursor location and hide the cursor for now */
    adp = vid_get_adapter(adapter);
    (*vidsw[adapter]->read_hw_cursor)(adp, &col, &row);
#ifndef PC98
    (*vidsw[adapter]->set_hw_cursor)(adp, -1, -1);
#endif

    /* set up the first console */
    current_default = &user_default;
    console[0] = &main_console;
    init_scp(console[0]);
    cur_console = console[0];

    /* copy screen to temporary buffer */
    if (ISTEXTSC(console[0]))
	generic_bcopy((ushort *)(console[0]->adp->va_window), sc_buffer,
		      console[0]->xsize * console[0]->ysize * sizeof(u_short));

    console[0]->scr_buf = console[0]->mouse_pos = console[0]->mouse_oldpos
	= sc_buffer;
    if (col >= console[0]->xsize)
	col = 0;
    if (row >= console[0]->ysize)
	row = console[0]->ysize - 1;
    console[0]->xpos = col;
    console[0]->ypos = row;
    console[0]->cursor_pos = console[0]->cursor_oldpos =
	sc_buffer + row*console[0]->xsize + col;
#ifndef PC98
    console[0]->cursor_saveunder = *console[0]->cursor_pos;
#else
    console[0]->atr_buf = Atrat;
    console[0]->cursor_atr = Atrat + row*console[0]->xsize + col;
#endif
    for (i=1; i<MAXCONS; i++)
	console[i] = NULL;
    kernel_console.esc = 0;
    kernel_console.attr_mask = NORMAL_ATTR;
    kernel_console.cur_attr =
	kernel_console.cur_color = kernel_console.std_color =
	kernel_default.std_color;
    kernel_console.rev_color = kernel_default.rev_color;

    /* initialize mapscrn arrays to a one to one map */
    for (i=0; i<sizeof(scr_map); i++) {
	scr_map[i] = scr_rmap[i] = i;
    }
#ifdef PC98
    scr_map[0x5c] = (u_char)0xfc;	/* for backslash */
#endif

    /* Save font and palette */
    if (ISFONTAVAIL(cur_console->adp->va_flags)) {
	if (fonts_loaded & FONT_16) {
	    copy_font(cur_console, LOAD, 16, font_16);
	} else {
	    copy_font(cur_console, SAVE, 16, font_16);
	    fonts_loaded = FONT_16;
	    set_destructive_cursor(cur_console);
	}
	/*
	 * FONT KLUDGE
	 * Always use the font page #0. XXX
	 */
	(*vidsw[cur_console->ad]->show_font)(cur_console->adp, 0);
    }
    save_palette(cur_console->adp, palette);

#if NSPLASH > 0
    /* we are ready to put up the splash image! */
    splash_init(cur_console->adp, scsplash_callback);
#endif
}

static void
scshutdown(int howto, void *arg)
{
    sc_touch_scrn_saver();
    if (!cold && cur_console->smode.mode == VT_AUTO 
	&& console[0]->smode.mode == VT_AUTO)
	switch_scr(cur_console, 0);
    shutdown_in_progress = TRUE;
}

int
sc_clean_up(scr_stat *scp)
{
    int error;

    sc_touch_scrn_saver();
#if NSPLASH > 0
    if ((error = wait_scrn_saver_stop()))
	return error;
#endif /* NSPLASH */
    scp->status &= ~MOUSE_VISIBLE;
    remove_cutmarking(scp);
    return 0;
}

void
sc_alloc_scr_buffer(scr_stat *scp, int wait, int clear)
{
    if (scp->scr_buf)
	free(scp->scr_buf, M_DEVBUF);
    scp->scr_buf = (u_short *)malloc(scp->xsize*scp->ysize*sizeof(u_short), 
				     M_DEVBUF, (wait) ? M_WAITOK : M_NOWAIT);
#ifdef PC98
    if (scp->atr_buf)
	free(scp->atr_buf, M_DEVBUF);
    scp->atr_buf = (u_short *)malloc(scp->xsize*scp->ysize*sizeof(u_short),
				     M_DEVBUF, (wait) ? M_WAITOK : M_NOWAIT);
#endif

    if (clear) {
        /* clear the screen and move the text cursor to the top-left position */
	sc_clear_screen(scp);
    } else {
	/* retain the current cursor position, but adjust pointers */
	move_crsr(scp, scp->xpos, scp->ypos);
	scp->cursor_oldpos = scp->cursor_pos;
    }

    /* move the mouse cursor at the center of the screen */
    sc_move_mouse(scp, scp->xpixel / 2, scp->ypixel / 2);
}

void
sc_alloc_cut_buffer(scr_stat *scp, int wait)
{
    if ((cut_buffer == NULL)
	|| (cut_buffer_size < scp->xsize * scp->ysize + 1)) {
	if (cut_buffer != NULL)
	    free(cut_buffer, M_DEVBUF);
	cut_buffer_size = scp->xsize * scp->ysize + 1;
	cut_buffer = (u_char *)malloc(cut_buffer_size, 
				    M_DEVBUF, (wait) ? M_WAITOK : M_NOWAIT);
	if (cut_buffer != NULL)
	    cut_buffer[0] = '\0';
    }
}

void
sc_alloc_history_buffer(scr_stat *scp, int lines, int extra, int wait)
{
    u_short *usp;
#ifdef PC98
    u_short *atr_usp;
#endif

    if (lines < scp->ysize)
	lines = scp->ysize;

    usp = scp->history;
    scp->history = NULL;
    if (usp != NULL) {
	free(usp, M_DEVBUF);
	if (extra > 0)
	    extra_history_size += extra;
    }
#ifdef PC98
    atr_usp = scp->his_atr;
    scp->his_atr = NULL;
    if (atr_usp != NULL)
	free(atr_usp, M_DEVBUF);
#endif

    scp->history_size = lines * scp->xsize;
    if (lines > imax(sc_history_size, scp->ysize))
	extra_history_size -= lines - imax(sc_history_size, scp->ysize);
    usp = (u_short *)malloc(scp->history_size * sizeof(u_short), 
			    M_DEVBUF, (wait) ? M_WAITOK : M_NOWAIT);
    if (usp != NULL)
	bzero(usp, scp->history_size * sizeof(u_short));
    scp->history_head = scp->history_pos = usp;
    scp->history = usp;
#ifdef PC98
    atr_usp = (u_short *)malloc(scp->history_size * sizeof(u_short),
			    M_DEVBUF, (wait) ? M_WAITOK : M_NOWAIT);
    if (atr_usp != NULL)
	bzero(atr_usp, scp->history_size * sizeof(u_short));
    scp->his_atr_head = scp->his_atr_pos = atr_usp;
    scp->his_atr = atr_usp;
#endif
}

static scr_stat
*alloc_scp()
{
    scr_stat *scp;

    scp = (scr_stat *)malloc(sizeof(scr_stat), M_DEVBUF, M_WAITOK);
    init_scp(scp);
    sc_alloc_scr_buffer(scp, TRUE, TRUE);
#ifdef PC98
    sc_alloc_cut_buffer(scp, TRUE);
#else	/* PC98 */
    if (ISMOUSEAVAIL(scp->adp->va_flags))
	sc_alloc_cut_buffer(scp, TRUE);
#endif	/* PC98 */
    sc_alloc_history_buffer(scp, sc_history_size, 0, TRUE);
/* SOS
#ifndef PC98
    if (scp->adp->va_flags & V_ADP_MODECHANGE)
#endif
	set_mode(scp);
*/
    sc_clear_screen(scp);
#ifndef	PC98
    scp->cursor_saveunder = *scp->cursor_pos;
#endif
    return scp;
}

static void
init_scp(scr_stat *scp)
{
    video_info_t info;

    scp->ad = adapter;
    scp->adp = vid_get_adapter(scp->ad);
    (*vidsw[scp->ad]->get_info)(scp->adp, initial_video_mode, &info);

    scp->status = 0;
    scp->mode = initial_video_mode;
    scp->scr_buf = NULL;
#ifdef PC98
	scp->atr_buf = NULL;
#endif
    if (info.vi_flags & V_INFO_GRAPHICS) {
	scp->status |= GRAPHICS_MODE;
	scp->xpixel = info.vi_width;
	scp->ypixel = info.vi_height;
	scp->xsize = info.vi_width/8;
	scp->ysize = info.vi_height/info.vi_cheight;
	scp->font_size = FONT_NONE;
    } else {
	scp->xsize = info.vi_width;
	scp->ysize = info.vi_height;
	scp->xpixel = scp->xsize*8;
	scp->ypixel = scp->ysize*info.vi_cheight;
	scp->font_size = info.vi_cheight;
    }
    scp->xoff = scp->yoff = 0;
    scp->xpos = scp->ypos = 0;
    scp->saved_xpos = scp->saved_ypos = -1;
    scp->start = scp->xsize * scp->ysize;
    scp->end = 0;
    scp->term.esc = 0;
    scp->term.attr_mask = NORMAL_ATTR;
    scp->term.cur_attr =
	scp->term.cur_color = scp->term.std_color =
	current_default->std_color;
    scp->term.rev_color = current_default->rev_color;
    scp->border = BG_BLACK;
    scp->cursor_start = bios_value.cursor_start;
    scp->cursor_end = bios_value.cursor_end;
    scp->mouse_xpos = scp->xsize*8/2;
    scp->mouse_ypos = scp->ysize*scp->font_size/2;
    scp->mouse_cut_start = scp->mouse_cut_end = NULL;
    scp->mouse_signal = 0;
    scp->mouse_pid = 0;
    scp->mouse_proc = NULL;
    scp->kbd_mode = K_XLATE;
    scp->bell_pitch = BELL_PITCH;
    scp->bell_duration = BELL_DURATION;
#ifndef PC98
    scp->status |= (bios_value.shift_state & 0x20) ? NLKED : 0;
#endif
    scp->status |= CURSOR_ENABLED;
    scp->pid = 0;
    scp->proc = NULL;
    scp->smode.mode = VT_AUTO;
    scp->history_head = scp->history_pos = scp->history = NULL;
#ifdef PC98
    scp->his_atr_head = scp->his_atr_pos = scp->his_atr = NULL;
#endif
    scp->history_size = imax(sc_history_size, scp->ysize) * scp->xsize;
#ifdef KANJI
    scp->kanji_1st_char = 0;
    scp->kanji_type = KTYPE_ASCII;
#endif
}

static void
get_bios_values(void)
{
#ifdef PC98
    bios_value.cursor_start = 0;
    bios_value.cursor_end = 0;
    bios_value.shift_state = 0;
#else /* !PC98 */
    bios_value.cursor_start = *(u_int8_t *)pa_to_va(0x461);
    bios_value.cursor_end = *(u_int8_t *)pa_to_va(0x460);
    bios_value.shift_state = *(u_int8_t *)pa_to_va(0x417);
#endif
}

static void
history_to_screen(scr_stat *scp)
{
    int i;

    for (i=0; i<scp->ysize; i++)
#ifdef PC98
    {
#endif
	bcopy(scp->history + (((scp->history_pos - scp->history) +
	       scp->history_size-((i+1)*scp->xsize))%scp->history_size),
	       scp->scr_buf + (scp->xsize * (scp->ysize-1 - i)),
	       scp->xsize * sizeof(u_short));
#ifdef PC98
	bcopy(scp->his_atr + (((scp->his_atr_pos - scp->his_atr) +
	       scp->history_size-((i+1)*scp->xsize))%scp->history_size),
	       scp->atr_buf + (scp->xsize * (scp->ysize-1 - i)),
	       scp->xsize * sizeof(u_short)); }
#endif
    mark_all(scp);
}

static int
history_up_line(scr_stat *scp)
{
    if (WRAPHIST(scp, scp->history_pos, -(scp->xsize*scp->ysize)) !=
	scp->history_head) {
	scp->history_pos = WRAPHIST(scp, scp->history_pos, -scp->xsize);
#ifdef PC98
	scp->his_atr_pos = WRAPHIST_A(scp, scp->his_atr_pos, -scp->xsize);
#endif
	history_to_screen(scp);
	return 0;
    }
    else
	return -1;
}

static int
history_down_line(scr_stat *scp)
{
    if (scp->history_pos != scp->history_head) {
	scp->history_pos = WRAPHIST(scp, scp->history_pos, scp->xsize);
#ifdef PC98
	scp->his_atr_pos = WRAPHIST_A(scp, scp->his_atr_pos, scp->xsize);
#endif
	history_to_screen(scp);
	return 0;
    }
    else
	return -1;
}

/*
 * scgetc(flags) - get character from keyboard.
 * If flags & SCGETC_CN, then avoid harmful side effects.
 * If flags & SCGETC_NONBLOCK, then wait until a key is pressed, else
 * return NOKEY if there is nothing there.
 */
static u_int
scgetc(keyboard_t *kbd, u_int flags)
{
    u_int c;
    int this_scr;
    int f;
    int i;

    if (kbd == NULL)
	return NOKEY;

next_code:
    /* I don't like this, but... XXX */
    if (flags & SCGETC_CN)
	sccnupdate(cur_console);
    /* first see if there is something in the keyboard port */
    for (;;) {
	c = kbd_read_char(kbd, !(flags & SCGETC_NONBLOCK));
	if (c == ERRKEY) {
	    if (!(flags & SCGETC_CN))
		do_bell(cur_console, BELL_PITCH, BELL_DURATION);
	} else if (c == NOKEY)
	    return c;
	else
	    break;
    }

    /* make screensaver happy */
    if (!(c & RELKEY))
	sc_touch_scrn_saver();

    if (!(flags & SCGETC_CN))
	/* do the /dev/random device a favour */
	add_keyboard_randomness(c);

    if (cur_console->kbd_mode != K_XLATE)
	return KEYCHAR(c);

    /* if scroll-lock pressed allow history browsing */
    if (!ISGRAPHSC(cur_console) && cur_console->history 
	&& cur_console->status & SLKED) {

	cur_console->status &= ~CURSOR_ENABLED;
	if (!(cur_console->status & BUFFER_SAVED)) {
	    cur_console->status |= BUFFER_SAVED;
	    cur_console->history_save = cur_console->history_head;
#ifdef PC98
	    cur_console->his_atr_save = cur_console->his_atr_head;
#endif

	    /* copy screen into top of history buffer */
	    for (i=0; i<cur_console->ysize; i++) {
		bcopy(cur_console->scr_buf + (cur_console->xsize * i),
		       cur_console->history_head,
		       cur_console->xsize * sizeof(u_short));
		cur_console->history_head += cur_console->xsize;
#ifdef PC98
		bcopy(cur_console->atr_buf + (cur_console->xsize * i),
		       cur_console->his_atr_head,
		       cur_console->xsize * sizeof(u_short));
		cur_console->his_atr_head += cur_console->xsize;
#endif
		if (cur_console->history_head + cur_console->xsize >
		    cur_console->history + cur_console->history_size)
#ifdef PC98
		{
		    cur_console->history_head=cur_console->history;
		    cur_console->his_atr_head=cur_console->his_atr;
		}
#else
		    cur_console->history_head=cur_console->history;
#endif
	    }
	    cur_console->history_pos = cur_console->history_head;
#ifdef PC98
	    cur_console->his_atr_pos = cur_console->his_atr_head;
#endif
	    history_to_screen(cur_console);
	}
	switch (c) {
	/* FIXME: key codes */
	case SPCLKEY | FKEY | F(49):  /* home key */
	    remove_cutmarking(cur_console);
	    cur_console->history_pos = cur_console->history_head;
#ifdef PC98
	    cur_console->his_atr_pos = cur_console->his_atr_head;
#endif
	    history_to_screen(cur_console);
	    goto next_code;

	case SPCLKEY | FKEY | F(57):  /* end key */
	    remove_cutmarking(cur_console);
	    cur_console->history_pos =
		WRAPHIST(cur_console, cur_console->history_head,
			 cur_console->xsize*cur_console->ysize);
#ifdef PC98
	    cur_console->his_atr_pos =
		WRAPHIST_A(cur_console, cur_console->his_atr_head,
			 cur_console->xsize*cur_console->ysize);
#endif
	    history_to_screen(cur_console);
	    goto next_code;

	case SPCLKEY | FKEY | F(50):  /* up arrow key */
	    remove_cutmarking(cur_console);
	    if (history_up_line(cur_console))
		if (!(flags & SCGETC_CN))
		    do_bell(cur_console, BELL_PITCH, BELL_DURATION);
	    goto next_code;

	case SPCLKEY | FKEY | F(58):  /* down arrow key */
	    remove_cutmarking(cur_console);
	    if (history_down_line(cur_console))
		if (!(flags & SCGETC_CN))
		    do_bell(cur_console, BELL_PITCH, BELL_DURATION);
	    goto next_code;

	case SPCLKEY | FKEY | F(51):  /* page up key */
	    remove_cutmarking(cur_console);
	    for (i=0; i<cur_console->ysize; i++)
	    if (history_up_line(cur_console)) {
		if (!(flags & SCGETC_CN))
		    do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		break;
	    }
	    goto next_code;

	case SPCLKEY | FKEY | F(59):  /* page down key */
	    remove_cutmarking(cur_console);
	    for (i=0; i<cur_console->ysize; i++)
	    if (history_down_line(cur_console)) {
		if (!(flags & SCGETC_CN))
		    do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		break;
	    }
	    goto next_code;
	}
    }

    /* 
     * Process and consume special keys here.  Return a plain char code
     * or a char code with the META flag or a function key code.
     */
    if (c & RELKEY) {
	/* key released */
	/* goto next_code */
    } else {
	/* key pressed */
	if (c & SPCLKEY) {
	    c &= ~SPCLKEY;
	    switch (KEYCHAR(c)) {
	    /* LOCKING KEYS */
	    case NLK: case CLK: case ALK:
		break;
	    case SLK:
		kbd_ioctl(kbd, KDGKBSTATE, (caddr_t)&f);
		if (f & SLKED) {
		    cur_console->status |= SLKED;
		} else {
		    if (cur_console->status & SLKED) {
			cur_console->status &= ~SLKED;
			if (cur_console->status & BUFFER_SAVED) {
			    int i;
			    u_short *ptr = cur_console->history_save;
#ifdef PC98
			    u_short *ptr_a = cur_console->his_atr_save;
#endif

			    for (i=0; i<cur_console->ysize; i++) {
				bcopy(ptr,
				       cur_console->scr_buf +
				       (cur_console->xsize*i),
				       cur_console->xsize * sizeof(u_short));
				ptr += cur_console->xsize;
#ifdef PC98
				bcopy(ptr_a,
				       cur_console->atr_buf +
				       (cur_console->xsize*i),
				       cur_console->xsize * sizeof(u_short));
				ptr_a += cur_console->xsize;
#endif
				if (ptr + cur_console->xsize >
				    cur_console->history +
				    cur_console->history_size)
#ifdef PC98
				{
				    ptr = cur_console->history;
				    ptr_a = cur_console->his_atr;
				}
#else
				    ptr = cur_console->history;
#endif
			    }
			    cur_console->status &= ~BUFFER_SAVED;
			    cur_console->history_head=cur_console->history_save;
#ifdef PC98
			    cur_console->his_atr_head=cur_console->his_atr_save;
#endif
			    cur_console->status |= CURSOR_ENABLED;
			    mark_all(cur_console);
			}
			scstart(VIRTUAL_TTY(get_scr_num()));
		    }
		}
		break;

	    /* NON-LOCKING KEYS */
	    case NOP:
	    case LSH:  case RSH:  case LCTR: case RCTR:
	    case LALT: case RALT: case ASH:  case META:
		break;

	    case BTAB:
		return c;

	    case SPSC:
		/* force activatation/deactivation of the screen saver */
		if (!scrn_blanked) {
		    run_scrn_saver = TRUE;
		    scrn_time_stamp -= scrn_blank_time;
		}
#if NSPLASH > 0
		if (cold) {
		    /*
		     * While devices are being probed, the screen saver need
		     * to be invoked explictly. XXX
		     */
		    if (scrn_blanked) {
			scsplash_stick(FALSE);
			stop_scrn_saver(current_saver);
		    } else {
			if (!ISGRAPHSC(cur_console)) {
			    scsplash_stick(TRUE);
			    (*current_saver)(TRUE);
			}
		    }
		}
#endif /* NSPLASH */
		break;

	    case RBT:
#ifndef SC_DISABLE_REBOOT
		shutdown_nice();
#endif
		break;

#if NAPM > 0
	    case SUSP:
		apm_suspend(PMST_SUSPEND);
		break;
	    case STBY:
		apm_suspend(PMST_STANDBY);
		break;
#else
	    case SUSP:
	    case STBY:
		break;
#endif

	    case DBG:
#ifdef DDB          /* try to switch to console 0 */
		/*
		 * TRY to make sure the screen saver is stopped, 
		 * and the screen is updated before switching to 
		 * the vty0.
		 */
		scrn_timer((void *)FALSE);
		if (cur_console->smode.mode == VT_AUTO &&
		    console[0]->smode.mode == VT_AUTO)
		    switch_scr(cur_console, 0);
		Debugger("manual escape to debugger");
#else
		printf("No debugger in kernel\n");
#endif
		break;

	    case NEXT:
    		this_scr = get_scr_num();
		for (i = this_scr + 1; i != this_scr; i = (i + 1)%MAXCONS) {
		    struct tty *tp = VIRTUAL_TTY(i);
		    if (tp->t_state & TS_ISOPEN) {
			switch_scr(cur_console, i);
			break;
		    }
		}
		break;

	    default:
		if (KEYCHAR(c) >= F_SCR && KEYCHAR(c) <= L_SCR) {
		    switch_scr(cur_console, KEYCHAR(c) - F_SCR);
		    break;
		}
		/* assert(c & FKEY) */
		return c;
	    }
	    /* goto next_code */
	} else {
	    /* regular keys (maybe MKEY is set) */
	    return c;
	}
    }

    goto next_code;
}

int
scmmap(dev_t dev, vm_offset_t offset, int nprot)
{
    struct tty *tp;
    struct scr_stat *scp;

    tp = scdevtotty(dev);
    if (!tp)
	return ENXIO;
    scp = sc_get_scr_stat(tp->t_dev);
    return (*vidsw[scp->ad]->mmap)(scp->adp, offset);
}

/*
 * Calculate hardware attributes word using logical attributes mask and
 * hardware colors
 */

static int
mask2attr(struct term_stat *term)
{
    int attr, mask = term->attr_mask;

    if (mask & REVERSE_ATTR) {
	attr = ((mask & FOREGROUND_CHANGED) ?
		((term->cur_color & 0xF000) >> 4) :
		(term->rev_color & 0x0F00)) |
	       ((mask & BACKGROUND_CHANGED) ?
		((term->cur_color & 0x0F00) << 4) :
		(term->rev_color & 0xF000));
    } else
	attr = term->cur_color;

    /* XXX: underline mapping for Hercules adapter can be better */
    if (mask & (BOLD_ATTR | UNDERLINE_ATTR))
	attr ^= 0x0800;
    if (mask & BLINK_ATTR)
	attr ^= 0x8000;

    return attr;
}

static int
save_kbd_state(scr_stat *scp)
{
    int state;
    int error;

    error = kbd_ioctl(kbd, KDGKBSTATE, (caddr_t)&state);
    if (error == ENOIOCTL)
	error = ENODEV;
    if (error == 0) {
	scp->status &= ~LOCK_MASK;
	scp->status |= state;
    }
    return error;
}

static int
update_kbd_state(int new_bits, int mask)
{
    int state;
    int error;

    if (mask != LOCK_MASK) {
	error = kbd_ioctl(kbd, KDGKBSTATE, (caddr_t)&state);
	if (error == ENOIOCTL)
	    error = ENODEV;
	if (error)
	    return error;
	state &= ~mask;
	state |= new_bits & mask;
    } else {
	state = new_bits & LOCK_MASK;
    }
    error = kbd_ioctl(kbd, KDSKBSTATE, (caddr_t)&state);
    if (error == ENOIOCTL)
	error = ENODEV;
    return error;
}

static int
update_kbd_leds(int which)
{
    int error;

    which &= LOCK_MASK;
    error = kbd_ioctl(kbd, KDSETLED, (caddr_t)&which);
    if (error == ENOIOCTL)
	error = ENODEV;
    return error;
}

int
set_mode(scr_stat *scp)
{
    video_info_t info;

    /* reject unsupported mode */
    if ((*vidsw[scp->ad]->get_info)(scp->adp, scp->mode, &info))
	return 1;

    /* if this vty is not currently showing, do nothing */
    if (scp != cur_console)
	return 0;

    /* setup video hardware for the given mode */
    (*vidsw[scp->ad]->set_mode)(scp->adp, scp->mode);
    Crtat = (u_short *)scp->adp->va_window;

#ifdef PC98
    if (scp->status & UNKNOWN_MODE) {
	while (!(inb(0x60) & 0x20)) {}	/* V-SYNC wait */
	outb(0x62, 0xc);		/* text off */
	outb(0xA2, 0xd);		/* graphics on */
    } else {
	while (!(inb(0x60) & 0x20)) {}	/* V-SYNC wait */
	outb(0x62, 0xd);		/* text off */
	outb(0xA2, 0xc);		/* graphics on */
    }
#endif

#ifndef PC98
    if (!(scp->status & GRAPHICS_MODE)) {
	/* load appropriate font */
	if (!(scp->status & PIXEL_MODE) && ISFONTAVAIL(scp->adp->va_flags)) {
	    if (scp->font_size < 14) {
		if (fonts_loaded & FONT_8)
		    copy_font(scp, LOAD, 8, font_8);
	    } else if (scp->font_size >= 16) {
		if (fonts_loaded & FONT_16)
		    copy_font(scp, LOAD, 16, font_16);
	    } else {
		if (fonts_loaded & FONT_14)
		    copy_font(scp, LOAD, 14, font_14);
	    }
	    /*
	     * FONT KLUDGE:
	     * This is an interim kludge to display correct font.
	     * Always use the font page #0 on the video plane 2.
	     * Somehow we cannot show the font in other font pages on
	     * some video cards... XXX
	     */ 
	    (*vidsw[scp->ad]->show_font)(scp->adp, 0);
	}
	mark_all(scp);
    }

    if (scp->status & PIXEL_MODE)
	generic_bzero((u_char *)(scp->adp->va_window),
		      scp->xpixel*scp->ypixel/8);
#endif
    set_border(scp, scp->border);

#ifndef PC98
    /* move hardware cursor out of the way */
    (*vidsw[scp->ad]->set_hw_cursor)(scp->adp, -1, -1);
#endif

    return 0;
}

void
set_border(scr_stat *scp, int color)
{
#ifndef PC98
    u_char *p;
    int xoff;
    int yoff;
    int xlen;
    int ylen;
    int i;

    (*vidsw[scp->ad]->set_border)(scp->adp, color);

    if (scp->status & PIXEL_MODE) {
	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
	outw(GDCIDX, (color << 8) | 0x00);	/* set/reset */
	p = (u_char *)(scp->adp->va_window);
	xoff = scp->xoff;
	yoff = scp->yoff*scp->font_size;
	xlen = scp->xpixel/8;
	ylen = scp->ysize*scp->font_size;
	if (yoff > 0) {
	    generic_bzero(p, xlen*yoff);
	    generic_bzero(p + xlen*(yoff + ylen),
			  xlen*scp->ypixel - xlen*(yoff + ylen));
	}
	if (xoff > 0) {
	    for (i = 0; i < ylen; ++i) {
		generic_bzero(p + xlen*(yoff + i), xoff);
		generic_bzero(p + xlen*(yoff + i) + xoff + scp->xsize, 
			      xlen - xoff - scp->xsize);
	    }
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
    }
#else	/* PC98 */
    (*vidsw[scp->ad]->set_border)(scp->adp, color);
#endif	/* PC98 */
}

void
copy_font(scr_stat *scp, int operation, int font_size, u_char *buf)
{
    /*
     * FONT KLUDGE:
     * This is an interim kludge to display correct font.
     * Always use the font page #0 on the video plane 2.
     * Somehow we cannot show the font in other font pages on
     * some video cards... XXX
     */ 
    font_loading_in_progress = TRUE;
    if (operation == LOAD) {
	(*vidsw[scp->ad]->load_font)(scp->adp, 0, font_size, buf, 0, 256);
	if (sc_flags & CHAR_CURSOR)
	    set_destructive_cursor(scp);
    } else if (operation == SAVE) {
	(*vidsw[scp->ad]->save_font)(scp->adp, 0, font_size, buf, 0, 256);
    }
    font_loading_in_progress = FALSE;
}

static void
set_destructive_cursor(scr_stat *scp)
{
#ifndef PC98
    u_char cursor[32];
    u_char *font_buffer;
    int font_size;
    int crtc_addr;
    int i;

    if (!ISFONTAVAIL(scp->adp->va_flags)
	|| (scp->status & (GRAPHICS_MODE | PIXEL_MODE)))
	return;

    if (scp->font_size < 14) {
	font_buffer = font_8;
	font_size = 8;
    } else if (scp->font_size >= 16) {
	font_buffer = font_16;
	font_size = 16;
    } else {
	font_buffer = font_14;
	font_size = 14;
    }

    if (scp->status & MOUSE_VISIBLE) {
	if ((scp->cursor_saveunder & 0xff) == SC_MOUSE_CHAR)
    	    bcopy(&scp->mouse_cursor[0], cursor, scp->font_size);
	else if ((scp->cursor_saveunder & 0xff) == SC_MOUSE_CHAR + 1)
    	    bcopy(&scp->mouse_cursor[32], cursor, scp->font_size);
	else if ((scp->cursor_saveunder & 0xff) == SC_MOUSE_CHAR + 2)
    	    bcopy(&scp->mouse_cursor[64], cursor, scp->font_size);
	else if ((scp->cursor_saveunder & 0xff) == SC_MOUSE_CHAR + 3)
    	    bcopy(&scp->mouse_cursor[96], cursor, scp->font_size);
	else
	    bcopy(font_buffer+((scp->cursor_saveunder & 0xff)*scp->font_size),
 	       	   cursor, scp->font_size);
    }
    else
    	bcopy(font_buffer + ((scp->cursor_saveunder & 0xff) * scp->font_size),
 	       cursor, scp->font_size);
    for (i=0; i<32; i++)
	if ((i >= scp->cursor_start && i <= scp->cursor_end) ||
	    (scp->cursor_start >= scp->font_size && i == scp->font_size - 1))
	    cursor[i] |= 0xff;
#if 1
    crtc_addr = scp->adp->va_crtc_addr;
    while (!(inb(crtc_addr+6) & 0x08)) /* wait for vertical retrace */ ;
#endif
    font_loading_in_progress = TRUE;
    (*vidsw[scp->ad]->load_font)(scp->adp, 0, font_size, cursor, DEAD_CHAR, 1);
    font_loading_in_progress = FALSE;
#endif
}

void
sc_move_mouse(scr_stat *scp, int x, int y)
{
    scp->mouse_xpos = x;
    scp->mouse_ypos = y;
    scp->mouse_pos = scp->mouse_oldpos = 
	scp->scr_buf + (y / scp->font_size) * scp->xsize + x / 8;
}

static void
set_mouse_pos(scr_stat *scp)
{
    static int last_xpos = -1, last_ypos = -1;

    if (scp->mouse_xpos < 0)
	scp->mouse_xpos = 0;
    if (scp->mouse_ypos < 0)
	scp->mouse_ypos = 0;
    if (!ISTEXTSC(scp)) {
        if (scp->mouse_xpos > scp->xpixel-1)
	    scp->mouse_xpos = scp->xpixel-1;
        if (scp->mouse_ypos > scp->ypixel-1)
	    scp->mouse_ypos = scp->ypixel-1;
	return;
    }
    if (scp->mouse_xpos > (scp->xsize*8)-1)
	scp->mouse_xpos = (scp->xsize*8)-1;
    if (scp->mouse_ypos > (scp->ysize*scp->font_size)-1)
	scp->mouse_ypos = (scp->ysize*scp->font_size)-1;

    if (scp->mouse_xpos != last_xpos || scp->mouse_ypos != last_ypos) {
	scp->status |= MOUSE_MOVED;

    	scp->mouse_pos = scp->scr_buf + 
	    ((scp->mouse_ypos/scp->font_size)*scp->xsize + scp->mouse_xpos/8);

	if ((scp->status & MOUSE_VISIBLE) && (scp->status & MOUSE_CUTTING))
	    mouse_cut(scp);
    }
}

#define isspace(c)	(((c) & 0xff) == ' ')

static int
skip_spc_right(scr_stat *scp, u_short *p)
{
    int i;

    for (i = (p - scp->scr_buf) % scp->xsize; i < scp->xsize; ++i) {
	if (!isspace(*p))
	    break;
	++p;
    }
    return i;
}

static int
skip_spc_left(scr_stat *scp, u_short *p)
{
    int i;

    for (i = (p-- - scp->scr_buf) % scp->xsize - 1; i >= 0; --i) {
	if (!isspace(*p))
	    break;
	--p;
    }
    return i;
}

static void
mouse_cut(scr_stat *scp)
{
    u_short *end;
    u_short *p;
    int i = 0;
    int j = 0;

    scp->mouse_cut_end = (scp->mouse_pos >= scp->mouse_cut_start) ?
	scp->mouse_pos + 1 : scp->mouse_pos;
    end = (scp->mouse_cut_start > scp->mouse_cut_end) ? 
	scp->mouse_cut_start : scp->mouse_cut_end;
    for (p = (scp->mouse_cut_start > scp->mouse_cut_end) ?
	    scp->mouse_cut_end : scp->mouse_cut_start; p < end; ++p) {
	cut_buffer[i] = *p & 0xff;
	/* remember the position of the last non-space char */
	if (!isspace(cut_buffer[i++]))
	    j = i;
	/* trim trailing blank when crossing lines */
	if (((p - scp->scr_buf) % scp->xsize) == (scp->xsize - 1)) {
	    cut_buffer[j++] = '\r';
	    i = j;
	}
    }
    cut_buffer[i] = '\0';

    /* scan towards the end of the last line */
    --p;
    for (i = (p - scp->scr_buf) % scp->xsize; i < scp->xsize; ++i) {
	if (!isspace(*p))
	    break;
	++p;
    }
    /* if there is nothing but blank chars, trim them, but mark towards eol */
    if (i >= scp->xsize) {
	if (scp->mouse_cut_start > scp->mouse_cut_end)
	    scp->mouse_cut_start = p;
	else
	    scp->mouse_cut_end = p;
	cut_buffer[j++] = '\r';
	cut_buffer[j] = '\0';
    }

    mark_for_update(scp, scp->mouse_cut_start - scp->scr_buf);
    mark_for_update(scp, scp->mouse_cut_end - scp->scr_buf);
}

static void
mouse_cut_start(scr_stat *scp) 
{
    int i;

    if (scp->status & MOUSE_VISIBLE) {
	if (scp->mouse_pos == scp->mouse_cut_start &&
	    scp->mouse_cut_start == scp->mouse_cut_end - 1) {
	    cut_buffer[0] = '\0';
	    remove_cutmarking(scp);
	} else if (skip_spc_right(scp, scp->mouse_pos) >= scp->xsize) {
	    /* if the pointer is on trailing blank chars, mark towards eol */
	    i = skip_spc_left(scp, scp->mouse_pos) + 1;
	    scp->mouse_cut_start = scp->scr_buf +
	        ((scp->mouse_pos - scp->scr_buf) / scp->xsize) * scp->xsize + i;
	    scp->mouse_cut_end = scp->scr_buf +
	        ((scp->mouse_pos - scp->scr_buf) / scp->xsize + 1) * scp->xsize;
	    cut_buffer[0] = '\r';
	    cut_buffer[1] = '\0';
	    scp->status |= MOUSE_CUTTING;
	} else {
	    scp->mouse_cut_start = scp->mouse_pos;
	    scp->mouse_cut_end = scp->mouse_cut_start + 1;
	    cut_buffer[0] = *scp->mouse_cut_start & 0xff;
	    cut_buffer[1] = '\0';
	    scp->status |= MOUSE_CUTTING;
	}
    	mark_all(scp);
	/* delete all other screens cut markings */
	for (i=0; i<MAXCONS; i++) {
	    if (console[i] == NULL || console[i] == scp)
		continue;
	    remove_cutmarking(console[i]);
	}
    }
}

static void
mouse_cut_end(scr_stat *scp) 
{
    if (scp->status & MOUSE_VISIBLE) {
	scp->status &= ~MOUSE_CUTTING;
    }
}

static void
mouse_cut_word(scr_stat *scp)
{
    u_short *p;
    u_short *sol;
    u_short *eol;
    int i;

    /*
     * Because we don't have locale information in the kernel,
     * we only distinguish space char and non-space chars.  Punctuation
     * chars, symbols and other regular chars are all treated alike.
     */
    if (scp->status & MOUSE_VISIBLE) {
	sol = scp->scr_buf
	    + ((scp->mouse_pos - scp->scr_buf) / scp->xsize) * scp->xsize;
	eol = sol + scp->xsize;
	if (isspace(*scp->mouse_pos)) {
	    for (p = scp->mouse_pos; p >= sol; --p)
	        if (!isspace(*p))
		    break;
	    scp->mouse_cut_start = ++p;
	    for (p = scp->mouse_pos; p < eol; ++p)
	        if (!isspace(*p))
		    break;
	    scp->mouse_cut_end = p;
	} else {
	    for (p = scp->mouse_pos; p >= sol; --p)
	        if (isspace(*p))
		    break;
	    scp->mouse_cut_start = ++p;
	    for (p = scp->mouse_pos; p < eol; ++p)
	        if (isspace(*p))
		    break;
	    scp->mouse_cut_end = p;
	}
	for (i = 0, p = scp->mouse_cut_start; p < scp->mouse_cut_end; ++p)
	    cut_buffer[i++] = *p & 0xff;
	cut_buffer[i] = '\0';
	scp->status |= MOUSE_CUTTING;
    }
}

static void
mouse_cut_line(scr_stat *scp)
{
    u_short *p;
    int i;

    if (scp->status & MOUSE_VISIBLE) {
	scp->mouse_cut_start = scp->scr_buf
	    + ((scp->mouse_pos - scp->scr_buf) / scp->xsize) * scp->xsize;
	scp->mouse_cut_end = scp->mouse_cut_start + scp->xsize;
	for (i = 0, p = scp->mouse_cut_start; p < scp->mouse_cut_end; ++p)
	    cut_buffer[i++] = *p & 0xff;
	cut_buffer[i++] = '\r';
	cut_buffer[i] = '\0';
	scp->status |= MOUSE_CUTTING;
    }
}

static void
mouse_cut_extend(scr_stat *scp) 
{
    if ((scp->status & MOUSE_VISIBLE) && !(scp->status & MOUSE_CUTTING)
	&& (scp->mouse_cut_start != NULL)) {
	mouse_cut(scp);
	scp->status |= MOUSE_CUTTING;
    }
}

static void
mouse_paste(scr_stat *scp) 
{
    if (scp->status & MOUSE_VISIBLE) {
	struct tty *tp;
	u_char *ptr = cut_buffer;

	tp = VIRTUAL_TTY(get_scr_num());
	while (*ptr)
	    (*linesw[tp->t_line].l_rint)(scr_rmap[*ptr++], tp);
    }
}

static void
draw_mouse_image(scr_stat *scp)
{
#ifdef PC98
    *(Atrat + (scp->mouse_pos - scp->scr_buf)) ^= 0x4;	/* reverse bit */
#else
    u_short buffer[32];
    u_short xoffset, yoffset;
    u_short *crt_pos = (u_short *)(scp->adp->va_window)
				      + (scp->mouse_pos - scp->scr_buf);
    u_char *font_buffer;
    int font_size;
    int crtc_addr;
    int i;

    if (scp->font_size < 14) {
	font_buffer = font_8;
	font_size = 8;
    } else if (scp->font_size >= 16) {
	font_buffer = font_16;
	font_size = 16;
    } else {
	font_buffer = font_14;
	font_size = 14;
    }

    xoffset = scp->mouse_xpos % 8;
    yoffset = scp->mouse_ypos % scp->font_size;

    /* prepare mousepointer char's bitmaps */
    bcopy(font_buffer + ((*(scp->mouse_pos) & 0xff) * font_size),
	   &scp->mouse_cursor[0], font_size);
    bcopy(font_buffer + ((*(scp->mouse_pos+1) & 0xff) * font_size),
	   &scp->mouse_cursor[32], font_size);
    bcopy(font_buffer + ((*(scp->mouse_pos+scp->xsize) & 0xff) * font_size),
	   &scp->mouse_cursor[64], font_size);
    bcopy(font_buffer + ((*(scp->mouse_pos+scp->xsize+1) & 0xff) * font_size),
	   &scp->mouse_cursor[96], font_size);
    for (i=0; i<font_size; i++) {
	buffer[i] = scp->mouse_cursor[i]<<8 | scp->mouse_cursor[i+32];
	buffer[i+font_size]=scp->mouse_cursor[i+64]<<8|scp->mouse_cursor[i+96];
    }

    /* now and-or in the mousepointer image */
    for (i=0; i<16; i++) {
	buffer[i+yoffset] =
	    ( buffer[i+yoffset] & ~(mouse_and_mask[i] >> xoffset))
	    | (mouse_or_mask[i] >> xoffset);
    }
    for (i=0; i<font_size; i++) {
	scp->mouse_cursor[i] = (buffer[i] & 0xff00) >> 8;
	scp->mouse_cursor[i+32] = buffer[i] & 0xff;
	scp->mouse_cursor[i+64] = (buffer[i+font_size] & 0xff00) >> 8;
	scp->mouse_cursor[i+96] = buffer[i+font_size] & 0xff;
    }
#endif

    scp->mouse_oldpos = scp->mouse_pos;
#ifndef	PC98
#if 1
    /* wait for vertical retrace to avoid jitter on some videocards */
    crtc_addr = scp->adp->va_crtc_addr;
    while (!(inb(crtc_addr+6) & 0x08)) /* idle */ ;
#endif
    font_loading_in_progress = TRUE;
    (*vidsw[scp->ad]->load_font)(scp->adp, 0, 32, scp->mouse_cursor, 
			   SC_MOUSE_CHAR, 4); 
    font_loading_in_progress = FALSE;

    *(crt_pos) = (*(scp->mouse_pos) & 0xff00) | SC_MOUSE_CHAR;
    *(crt_pos+scp->xsize) = 
	(*(scp->mouse_pos + scp->xsize) & 0xff00) | (SC_MOUSE_CHAR + 2);
    if (scp->mouse_xpos < (scp->xsize-1)*8) {
    	*(crt_pos + 1) = (*(scp->mouse_pos + 1) & 0xff00) | (SC_MOUSE_CHAR + 1);
    	*(crt_pos+scp->xsize + 1) = 
	    (*(scp->mouse_pos + scp->xsize + 1) & 0xff00) | (SC_MOUSE_CHAR + 3);
    }
#endif
    mark_for_update(scp, scp->mouse_pos - scp->scr_buf);
#ifndef	PC98
    mark_for_update(scp, scp->mouse_pos + scp->xsize + 1 - scp->scr_buf);
#endif
}

static void
remove_mouse_image(scr_stat *scp)
{
    u_short *crt_pos;

    if (!ISTEXTSC(scp))
	return;

#ifdef PC98
    crt_pos = Atrat + (scp->mouse_oldpos - scp->scr_buf);
#else
    crt_pos = (u_short *)(scp->adp->va_window)
			     + (scp->mouse_oldpos - scp->scr_buf);
#endif

#ifdef PC98
    *(crt_pos) = *(scp->atr_buf + (scp->mouse_oldpos - scp->scr_buf));
#else
    *(crt_pos) = *(scp->mouse_oldpos);
    *(crt_pos+1) = *(scp->mouse_oldpos+1);
    *(crt_pos+scp->xsize) = *(scp->mouse_oldpos+scp->xsize);
    *(crt_pos+scp->xsize+1) = *(scp->mouse_oldpos+scp->xsize+1);
#endif
    mark_for_update(scp, scp->mouse_oldpos - scp->scr_buf);
#ifndef	PC98
    mark_for_update(scp, scp->mouse_oldpos + scp->xsize + 1 - scp->scr_buf);
#endif
}

static void
draw_cutmarking(scr_stat *scp)
{
    u_short *crt_pos;
    u_short *ptr;
    u_short och, nch;

#ifdef	PC98
    crt_pos = Atrat;
#else
    crt_pos = (u_short *)(scp->adp->va_window);
#endif
    for (ptr=scp->scr_buf; ptr<=(scp->scr_buf+(scp->xsize*scp->ysize)); ptr++) {
	nch = och = *(crt_pos + (ptr - scp->scr_buf));
	/* are we outside the selected area ? */
	if ( ptr < (scp->mouse_cut_start > scp->mouse_cut_end ? 
	            scp->mouse_cut_end : scp->mouse_cut_start) ||
	     ptr >= (scp->mouse_cut_start > scp->mouse_cut_end ?
	            scp->mouse_cut_start : scp->mouse_cut_end)) {
#ifdef	PC98
	    if (ptr != scp->mouse_pos)
		nch = *(scp->atr_buf + (ptr - scp->scr_buf));
	    else
		nch = *(scp->atr_buf + (ptr - scp->scr_buf)) ^ 0x4;
#else
	    if (ptr != scp->cursor_pos)
		nch = (och & 0xff) | (*ptr & 0xff00);
#endif
	}
	else {
#ifdef	PC98
	    if (ptr != scp->mouse_pos)
		nch = *(scp->atr_buf + (ptr - scp->scr_buf)) ^ 0x4;	/* reverse bit */
#else
	    /* are we clear of the cursor image ? */
	    if (ptr != scp->cursor_pos)
		nch = (och & 0x88ff) | (*ptr & 0x7000)>>4 | (*ptr & 0x0700)<<4;
	    else {
		if (sc_flags & CHAR_CURSOR)
		    nch = (och & 0x88ff)|(*ptr & 0x7000)>>4|(*ptr & 0x0700)<<4;
		else 
		    if (!(sc_flags & BLINK_CURSOR))
		        nch = (och & 0xff) | (*ptr & 0xff00);
	    }
#endif
	}
	if (nch != och)
	    *(crt_pos + (ptr - scp->scr_buf)) = nch;
    }
}

static void
remove_cutmarking(scr_stat *scp)
{
    scp->mouse_cut_start = scp->mouse_cut_end = NULL;
    scp->status &= ~MOUSE_CUTTING;
    mark_all(scp);
}

static void
do_bell(scr_stat *scp, int pitch, int duration)
{
    if (cold || shutdown_in_progress)
	return;

    if (scp != cur_console && (sc_flags & QUIET_BELL))
	return;

    if (sc_flags & VISUAL_BELL) {
	if (blink_in_progress)
	    return;
	blink_in_progress = 4;
	if (scp != cur_console)
	    blink_in_progress += 2;
	blink_screen(cur_console);
    } else {
	if (scp != cur_console)
	    pitch *= 2;
	sysbeep(pitch, duration);
    }
}

static void
blink_screen(void *arg)
{
    scr_stat *scp = arg;

    if (!ISTEXTSC(scp) || (blink_in_progress <= 1)) {
	blink_in_progress = FALSE;
    	mark_all(scp);
	if (delayed_next_scr)
	    switch_scr(scp, delayed_next_scr - 1);
    }
    else {
#ifdef PC98
	if (blink_in_progress & 1){
	    fillw(scr_map[0x20],
		  (u_short *)(scp->adp->va_window),
		  scp->xsize * scp->ysize);
	    fillw(at2pc98(kernel_default.std_color),
		  Atrat, scp->xsize * scp->ysize);
	} else {
	    fillw(scr_map[0x20],
		  (u_short *)(scp->adp->va_window),
		  scp->xsize * scp->ysize);
	    fillw(at2pc98(kernel_default.rev_color),
		  Atrat, scp->xsize * scp->ysize);
	}
#else
	if (blink_in_progress & 1)
	    fillw(kernel_default.std_color | scr_map[0x20],
		  (u_short *)(scp->adp->va_window),
		  scp->xsize * scp->ysize);
	else
	    fillw(kernel_default.rev_color | scr_map[0x20],
		  (u_short *)(scp->adp->va_window),
		  scp->xsize * scp->ysize);
#endif
	blink_in_progress--;
	timeout(blink_screen, scp, hz / 10);
    }
}

void 
sc_bcopy(scr_stat *scp, u_short *p, int from, int to, int mark)
{
#ifdef PC98
    if (ISTEXTSC(scp))
	generic_bcopy(p + from, (u_short *)(scp->adp->va_window) + from,
		      (to - from + 1)*sizeof(u_short));
#else	/* PC98 */
    u_char *font;
    u_char volatile *d;
    u_char *e;
    u_char *f;
    int font_size;
    int line_length;
    int xsize;
    u_short bg;
    int i, j;
    u_char c;

    if (ISTEXTSC(scp)) {
	generic_bcopy(p + from, (u_short *)(scp->adp->va_window) + from,
		      (to - from + 1)*sizeof(u_short));
    } else /* if ISPIXELSC(scp) */ {
	if (mark)
	    mark = 255;
	font_size = scp->font_size;
	if (font_size < 14)
	    font = font_8;
	else if (font_size >= 16)
	    font = font_16;
	else
	    font = font_14;
	line_length = scp->xpixel/8;
	xsize = scp->xsize;
	d = (u_char *)(scp->adp->va_window)
	    + scp->xoff + scp->yoff*font_size*line_length 
	    + (from%xsize) + font_size*line_length*(from/xsize);

	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	bg = -1;
	for (i = from ; i <= to ; i++) {
	    /* set background color in EGA/VGA latch */
	    if (bg != (p[i] & 0xf000)) {
		bg = (p[i] & 0xf000);
		outw(GDCIDX, (bg >> 4) | 0x00); /* set/reset */
		outw(GDCIDX, 0xff08);		/* bit mask */
		*d = 0;
		c = *d;		/* set the background color in the latch */
	    }
	    /* foreground color */
	    outw(GDCIDX, (p[i] & 0x0f00) | 0x00); /* set/reset */
	    e = (u_char *)d;
	    f = &font[(p[i] & 0x00ff)*font_size];
	    for (j = 0 ; j < font_size; j++, f++) {
		outw(GDCIDX, ((*f^mark) << 8) | 0x08);	/* bit mask */
	        *e = 0;
		e += line_length;
	    }
	    d++;
	    if ((i % xsize) == xsize - 1)
		d += scp->xoff*2 + (font_size - 1)*line_length;
	}
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */

#if 0	/* VGA only */
	outw(GDCIDX, 0x0305);		/* read mode 0, write mode 3 */
	outw(GDCIDX, 0x0003);		/* data rotate/function select */
	outw(GDCIDX, 0x0f01);		/* set/reset enable */
	outw(GDCIDX, 0xff08);		/* bit mask */
	bg = -1;
	for (i = from ; i <= to ; i++) {
	    /* set background color in EGA/VGA latch */
	    if (bg != (p[i] & 0xf000)) {
		bg = (p[i] & 0xf000);
		outw(GDCIDX, 0x0005);	/* read mode 0, write mode 0 */
		outw(GDCIDX, (bg >> 4) | 0x00); /* set/reset */
		*d = 0;
		c = *d;		/* set the background color in the latch */
		outw(GDCIDX, 0x0305);	/* read mode 0, write mode 3 */
	    }
	    /* foreground color */
	    outw(GDCIDX, (p[i] & 0x0f00) | 0x00); /* set/reset */
	    e = (u_char *)d;
	    f = &font[(p[i] & 0x00ff)*font_size];
	    for (j = 0 ; j < font_size; j++, f++) {
	        *e = *f^mark;
		e += line_length;
	    }
	    d++;
	    if ((i % xsize) == xsize - 1)
		d += scp->xoff*2 + (font_size - 1)*line_length;
	}
	outw(GDCIDX, 0x0005);		/* read mode 0, write mode 0 */
	outw(GDCIDX, 0x0000);		/* set/reset */
	outw(GDCIDX, 0x0001);		/* set/reset enable */
#endif /* 0 */
    }
#endif	/* PC98 */
}

#endif /* NSC */
