/*-
 * Copyright (c) 1992-1997 Søren Schmidt
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
 *    derived from this software withough specific prior written permission
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
 *  $Id: syscons.c,v 1.244 1998/01/09 09:06:55 yokota Exp $
 */

#include "sc.h"
#include "apm.h"
#include "opt_ddb.h"
#include "opt_syscons.h"

#if NSC > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#ifdef	DEVFS
#include <sys/devfsext.h>
#endif

#include <machine/clock.h>
#include <machine/cons.h>
#include <machine/console.h>
#include <machine/mouse.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/pc/display.h>
#include <machine/apm_bios.h>
#include <machine/random.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/timerreg.h>
#include <i386/isa/kbdtables.h>
#include <i386/isa/kbdio.h>
#include <i386/isa/syscons.h>

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

#define COLD 0
#define WARM 1

#define MODE_MAP_SIZE		(M_VGA_CG320 + 1)
#define MODE_PARAM_SIZE		64

/* XXX use sc_bcopy where video memory is concerned */
#define sc_bcopy generic_bcopy
extern void generic_bcopy(const void *, void *, size_t);

static default_attr user_default = {
    (FG_LIGHTGREY | BG_BLACK) << 8,
    (FG_BLACK | BG_LIGHTGREY) << 8
};

static default_attr kernel_default = {
    (FG_WHITE | BG_BLACK) << 8,
    (FG_BLACK | BG_LIGHTGREY) << 8
};

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
static  int     	flags = 0;
static  int		sc_port = IO_KBD;
static  KBDC		sc_kbdc = NULL;
static  char        	init_done = COLD;
static  u_short		sc_buffer[ROW*COL];
static  char        	switch_in_progress = FALSE;
static  char        	write_in_progress = FALSE;
static  char        	blink_in_progress = FALSE;
static  int        	blinkrate = 0;
	u_int       	crtc_addr = MONO_BASE;
	char		crtc_type = KD_MONO;
	char        	crtc_vga = FALSE;
static  u_char      	shfts = 0, ctls = 0, alts = 0, agrs = 0, metas = 0;
static  u_char		accents = 0;
static  u_char      	nlkcnt = 0, clkcnt = 0, slkcnt = 0, alkcnt = 0;
static  const u_int     n_fkey_tab = sizeof(fkey_tab) / sizeof(*fkey_tab);
static  int     	delayed_next_scr = FALSE;
static  long        	scrn_blank_time = 0;    /* screen saver timeout value */
	int     	scrn_blanked = 0;       /* screen saver active flag */
static  long       	scrn_time_stamp;
	u_char      	scr_map[256];
	u_char      	scr_rmap[256];
	char        	*video_mode_ptr = NULL;
	int     	fonts_loaded = 0
#ifdef STD8X16FONT
	| FONT_16
#endif
	;

	char        	font_8[256*8];
	char		font_14[256*14];
#ifdef STD8X16FONT
extern
#endif
	unsigned char	font_16[256*16];
	char        	palette[256*3];
static  char		*mode_map[MODE_MAP_SIZE];
static  char		vgaregs[MODE_PARAM_SIZE];
static  char		vgaregs2[MODE_PARAM_SIZE];
static  int		rows_offset = 1;
static	char 		*cut_buffer;
static	int		mouse_level = 0;	/* sysmouse protocol level */
static	mousestatus_t	mouse_status = { 0, 0, 0, 0, 0, 0 };
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

static int		extra_history_size = 
			    SC_MAX_HISTORY_SIZE - SC_HISTORY_SIZE * MAXCONS;

static void    		none_saver(int blank) { }
static void    		(*current_saver)(int blank) = none_saver;
int  			(*sc_user_ioctl)(dev_t dev, int cmd, caddr_t data,
					 int flag, struct proc *p) = NULL;

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
#define MONO_BUF    	pa_to_va(0xB0000)
#define CGA_BUF     	pa_to_va(0xB8000)
u_short         	*Crtat;
static const int	nsccons = MAXCONS+2;

#define WRAPHIST(scp, pointer, offset)\
    ((scp->history) + ((((pointer) - (scp->history)) + (scp->history_size)\
    + (offset)) % (scp->history_size)))
#define ISSIGVALID(sig)	((sig) > 0 && (sig) < NSIG)

/* this should really be in `rtc.h' */
#define RTC_EQUIPMENT		0x14

/* prototypes */
static int scattach(struct isa_device *dev);
static int scparam(struct tty *tp, struct termios *t);
static int scprobe(struct isa_device *dev);
static int scvidprobe(int unit, int flags);
static int sckbdprobe(int unit, int flags);
static void scstart(struct tty *tp);
static void scmousestart(struct tty *tp);
static void scinit(void);
static void map_mode_table(char *map[], char *table, int max);
static u_char map_mode_num(u_char mode);
static char *get_mode_param(scr_stat *scp, u_char mode);
static u_int scgetc(u_int flags);
#define SCGETC_CN	1
#define SCGETC_NONBLOCK	2
static scr_stat *get_scr_stat(dev_t dev);
static scr_stat *alloc_scp(void);
static void init_scp(scr_stat *scp);
static int get_scr_num(void);
static timeout_t scrn_timer;
static void stop_scrn_saver(void (*saver)(int));
static void clear_screen(scr_stat *scp);
static int switch_scr(scr_stat *scp, u_int next_scr);
static void exchange_scr(void);
static inline void move_crsr(scr_stat *scp, int x, int y);
static void scan_esc(scr_stat *scp, u_char c);
static void draw_cursor_image(scr_stat *scp); 
static void remove_cursor_image(scr_stat *scp); 
static void ansi_put(scr_stat *scp, u_char *buf, int len);
static u_char *get_fstr(u_int c, u_int *len);
static void history_to_screen(scr_stat *scp);
static int history_up_line(scr_stat *scp);
static int history_down_line(scr_stat *scp);
static int mask2attr(struct term_stat *term);
static void set_keyboard(int command, int data);
static void update_leds(int which);
static void set_vgaregs(char *modetable);
static void read_vgaregs(char *buf);
#define COMP_IDENTICAL	0
#define COMP_SIMILAR	1
#define COMP_DIFFERENT	2
static int comp_vgaregs(u_char *buf1, u_char *buf2);
static void dump_vgaregs(u_char *buf);
#define PARAM_BUFSIZE	6
static void set_font_mode(u_char *buf);
static void set_normal_mode(u_char *buf);
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
static void save_palette(void);
static void do_bell(scr_stat *scp, int pitch, int duration);
static timeout_t blink_screen;
#ifdef SC_SPLASH_SCREEN
static void toggle_splash_screen(scr_stat *scp);
#endif

struct  isa_driver scdriver = {
    scprobe, scattach, "sc", 1
};

static	d_open_t	scopen;
static	d_close_t	scclose;
static	d_read_t	scread;
static	d_write_t	scwrite;
static	d_ioctl_t	scioctl;
static	d_devtotty_t	scdevtotty;
static	d_mmap_t	scmmap;

#define CDEV_MAJOR 12
static	struct cdevsw	scdevsw = {
	scopen,		scclose,	scread,		scwrite,
	scioctl,	nullstop,	noreset,	scdevtotty,
	ttpoll,		scmmap,		nostrategy,	"sc",	NULL,	-1 };

/*
 * These functions need to be before calls to them so they can be inlined.
 */
static inline void
draw_cursor_image(scr_stat *scp)
{
    u_short cursor_image, *ptr = Crtat + (scp->cursor_pos - scp->scr_buf);
    u_short prev_image;

    /* do we have a destructive cursor ? */
    if (flags & CHAR_CURSOR) {
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
	if (!(flags & BLINK_CURSOR)||((flags & BLINK_CURSOR)&&(blinkrate & 4))){
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
    }
    else {
	cursor_image = (*(ptr) & 0x00ff) | *(scp->cursor_pos) & 0xff00;
	scp->cursor_saveunder = cursor_image;
	if (!(flags & BLINK_CURSOR)||((flags & BLINK_CURSOR)&&(blinkrate & 4))){
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
}

static inline void
remove_cursor_image(scr_stat *scp)
{
    *(Crtat + (scp->cursor_oldpos - scp->scr_buf)) = scp->cursor_saveunder;
}

static inline void
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
}

static int
scprobe(struct isa_device *dev)
{
    if (!scvidprobe(dev->id_unit, dev->id_flags)) {
	if (bootverbose)
	    printf("sc%d: no video adapter is found.\n", dev->id_unit);
	return (0);
    }

    sc_port = dev->id_iobase;
    if (sckbdprobe(dev->id_unit, dev->id_flags))
	return (IO_KBDSIZE);
    else
        return ((dev->id_flags & DETECT_KBD) ? 0 : IO_KBDSIZE);
}

/* probe video adapters, return TRUE if found */ 
static int
scvidprobe(int unit, int flags)
{
    /* 
     * XXX don't try to `printf' anything here, the console may not have 
     * been configured yet. 
     */
    u_short volatile *cp;
    u_short was;
    u_long  pa;
    u_long  segoff;

    /* do this test only once */
    if (init_done != COLD)
	return (Crtat != 0);

    /*
     * Finish defaulting crtc variables for a mono screen.  Crtat is a
     * bogus common variable so that it can be shared with pcvt, so it
     * can't be statically initialized.  XXX.
     */
    Crtat = (u_short *)MONO_BUF;
    crtc_type = KD_MONO;
    /* If CGA memory seems to work, switch to color.  */
    cp = (u_short *)CGA_BUF;
    was = *cp;
    *cp = (u_short) 0xA55A;
    if (*cp == 0xA55A) {
	Crtat = (u_short *)CGA_BUF;
	crtc_addr = COLOR_BASE;
	crtc_type = KD_CGA;
    } else {
        cp = Crtat;
	was = *cp;
	*cp = (u_short) 0xA55A;
	if (*cp != 0xA55A) {
	    /* no screen at all, bail out */
	    Crtat = 0;
	    return FALSE;
	}
    }
    *cp = was;

    /* 
     * Check rtc and BIOS date area.
     * XXX: don't use BIOSDATA_EQUIPMENT, it is not a dead copy
     * of RTC_EQUIPMENT. The bit 4 and 5 of the ETC_EQUIPMENT are
     * zeros for EGA and VGA. However, the EGA/VGA BIOS will set 
     * these bits in BIOSDATA_EQUIPMENT according to the monitor
     * type detected.
     */
    switch ((rtcin(RTC_EQUIPMENT) >> 4) & 3) {	/* bit 4 and 5 */
    case 0: /* EGA/VGA, or nothing */
	crtc_type = KD_EGA;
	/* the color adapter may be in the 40x25 mode... XXX */
	break;
    case 1: /* CGA 40x25 */
	/* switch to the 80x25 mode? XXX */
	/* FALL THROUGH */
    case 2: /* CGA 80x25 */
	/* `crtc_type' has already been set... */
	/* crtc_type = KD_CGA; */
	break;
    case 3: /* MDA */
	/* `crtc_type' has already been set... */
	/* crtc_type = KD_MONO; */
	break;
    }

    /* is this a VGA or higher ? */
    outb(crtc_addr, 7);
    if (inb(crtc_addr) == 7) {

        crtc_type = KD_VGA;
	crtc_vga = TRUE;
	read_vgaregs(vgaregs);

	/* Get the BIOS video mode pointer */
	segoff = *(u_long *)pa_to_va(0x4a8);
	pa = (((segoff & 0xffff0000) >> 12) + (segoff & 0xffff));
	if (ISMAPPED(pa, sizeof(u_long))) {
	    segoff = *(u_long *)pa_to_va(pa);
	    pa = (((segoff & 0xffff0000) >> 12) + (segoff & 0xffff));
	    if (ISMAPPED(pa, MODE_PARAM_SIZE))
		video_mode_ptr = (char *)pa_to_va(pa);
	}
    }

    return TRUE;
}

/* probe the keyboard, return TRUE if found */
static int
sckbdprobe(int unit, int flags)
{
    int codeset;
    int c = -1;
    int m;

    sc_kbdc = kbdc_open(sc_port);

    if (!kbdc_lock(sc_kbdc, TRUE)) {
	/* driver error? */
	printf("sc%d: unable to lock the controller.\n", unit);
        return ((flags & DETECT_KBD) ? FALSE : TRUE);
    }

    /* discard anything left after UserConfig */
    empty_both_buffers(sc_kbdc, 10);

    /* save the current keyboard controller command byte */
    m = kbdc_get_device_mask(sc_kbdc) & ~KBD_KBD_CONTROL_BITS;
    c = get_controller_command_byte(sc_kbdc);
    if (c == -1) {
	/* CONTROLLER ERROR */
	printf("sc%d: unable to get the current command byte value.\n", unit);
	goto fail;
    }
    if (bootverbose)
	printf("sc%d: the current keyboard controller command byte %04x\n",
	    unit, c);
#if 0
    /* override the keyboard lock switch */
    c |= KBD_OVERRIDE_KBD_LOCK;
#endif

    /* 
     * The keyboard may have been screwed up by the boot block.
     * We may just be able to recover from error by testing the controller
     * and the keyboard port. The controller command byte needs to be saved
     * before this recovery operation, as some controllers seem to set 
     * the command byte to particular values.
     */
    test_controller(sc_kbdc);
    test_kbd_port(sc_kbdc);

    /* enable the keyboard port, but disable the keyboard intr. */
    if (!set_controller_command_byte(sc_kbdc,
            KBD_KBD_CONTROL_BITS,
            KBD_ENABLE_KBD_PORT | KBD_DISABLE_KBD_INT)) {
	/* CONTROLLER ERROR 
	 * there is very little we can do...
	 */
	printf("sc%d: unable to set the command byte.\n", unit);
	goto fail;
     }

     /* 
      * Check if we have an XT keyboard before we attempt to reset it. 
      * The procedure assumes that the keyboard and the controller have 
      * been set up properly by BIOS and have not been messed up 
      * during the boot process.
      */
     codeset = -1;
     if (flags & XT_KEYBD)
	 /* the user says there is a XT keyboard */
	 codeset = 1;
#ifdef DETECT_XT_KEYBOARD
     else if ((c & KBD_TRANSLATION) == 0) {
	 /* SET_SCANCODE_SET is not always supported; ignore error */
	 if (send_kbd_command_and_data(sc_kbdc, KBDC_SET_SCANCODE_SET, 0)
		 == KBD_ACK) 
	     codeset = read_kbd_data(sc_kbdc);
     }
     if (bootverbose)
         printf("sc%d: keyboard scancode set %d\n", unit, codeset);
#endif /* DETECT_XT_KEYBOARD */
 
    if (flags & KBD_NORESET) {
        write_kbd_command(sc_kbdc, KBDC_ECHO);
        if (read_kbd_data(sc_kbdc) != KBD_ECHO) {
            empty_both_buffers(sc_kbdc, 10);
            test_controller(sc_kbdc);
            test_kbd_port(sc_kbdc);
            if (bootverbose)
                printf("sc%d: failed to get response from the keyboard.\n", 
		    unit);
	    goto fail;
	}
    } else {
        /* reset keyboard hardware */
        if (!reset_kbd(sc_kbdc)) {
            /* KEYBOARD ERROR
             * Keyboard reset may fail either because the keyboard doen't
             * exist, or because the keyboard doesn't pass the self-test,
             * or the keyboard controller on the motherboard and the keyboard
             * somehow fail to shake hands. It is just possible, particularly
             * in the last case, that the keyoard controller may be left 
             * in a hung state. test_controller() and test_kbd_port() appear
             * to bring the keyboard controller back (I don't know why and
             * how, though.)
             */
            empty_both_buffers(sc_kbdc, 10);
            test_controller(sc_kbdc);
            test_kbd_port(sc_kbdc);
            /* We could disable the keyboard port and interrupt... but, 
             * the keyboard may still exist (see above). 
             */
            if (bootverbose)
                printf("sc%d: failed to reset the keyboard.\n", unit);
            goto fail;
        }
    }

    /*
     * Allow us to set the XT_KEYBD flag in UserConfig so that keyboards
     * such as those on the IBM ThinkPad laptop computers can be used
     * with the standard console driver.
     */
    if (codeset == 1) {
	if (send_kbd_command_and_data(
	        sc_kbdc, KBDC_SET_SCANCODE_SET, codeset) == KBD_ACK) {
	    /* XT kbd doesn't need scan code translation */
	    c &= ~KBD_TRANSLATION;
	} else {
	    /* KEYBOARD ERROR 
	     * The XT kbd isn't usable unless the proper scan code set
	     * is selected. 
	     */
	    printf("sc%d: unable to set the XT keyboard mode.\n", unit);
	    goto fail;
	}
    }
    /* enable the keyboard port and intr. */
    if (!set_controller_command_byte(sc_kbdc, 
            KBD_KBD_CONTROL_BITS | KBD_TRANSLATION | KBD_OVERRIDE_KBD_LOCK,
	    (c & (KBD_TRANSLATION | KBD_OVERRIDE_KBD_LOCK))
	        | KBD_ENABLE_KBD_PORT | KBD_ENABLE_KBD_INT)) {
	/* CONTROLLER ERROR 
	 * This is serious; we are left with the disabled keyboard intr. 
	 */
	printf("sc%d: unable to enable the keyboard port and intr.\n", unit);
	goto fail;
    }

    kbdc_set_device_mask(sc_kbdc, m | KBD_KBD_CONTROL_BITS),
    kbdc_lock(sc_kbdc, FALSE);
    return TRUE;

fail:
    if (c != -1)
        /* try to restore the command byte as before, if possible */
        set_controller_command_byte(sc_kbdc, 0xff, c);
    kbdc_set_device_mask(sc_kbdc, 
        (flags & DETECT_KBD) ? m : m | KBD_KBD_CONTROL_BITS);
    kbdc_lock(sc_kbdc, FALSE);
    return FALSE;
}

#if NAPM > 0
static int
scresume(void *dummy)
{
	shfts = ctls = alts = agrs = metas = accents = 0; 
	return 0;
}
#endif

static int
scattach(struct isa_device *dev)
{
    scr_stat *scp;
    dev_t cdev = makedev(CDEV_MAJOR, 0);
    char *p;
#ifdef DEVFS
    int vc;
#endif

    scinit();
    flags = dev->id_flags;
    if (!crtc_vga)
	flags &= ~CHAR_CURSOR;

    scp = console[0];

    if (crtc_vga) {
    	cut_buffer = (char *)malloc(scp->xsize*scp->ysize, M_DEVBUF, M_NOWAIT);
    }

    scp->scr_buf = (u_short *)malloc(scp->xsize*scp->ysize*sizeof(u_short),
				     M_DEVBUF, M_NOWAIT);

    /* copy temporary buffer to final buffer */
    bcopy(sc_buffer, scp->scr_buf, scp->xsize * scp->ysize * sizeof(u_short));

    scp->cursor_pos = scp->cursor_oldpos =
	scp->scr_buf + scp->xpos + scp->ypos * scp->xsize;
    scp->mouse_pos = scp->mouse_oldpos = 
	scp->scr_buf + ((scp->mouse_ypos/scp->font_size)*scp->xsize +
	    		scp->mouse_xpos/8);

    /* initialize history buffer & pointers */
    scp->history_head = scp->history_pos = 
	(u_short *)malloc(scp->history_size*sizeof(u_short),
			  M_DEVBUF, M_NOWAIT);
    if (scp->history_head != NULL)
        bzero(scp->history_head, scp->history_size*sizeof(u_short));
    scp->history = scp->history_head;

    /* initialize cursor stuff */
    if (!(scp->status & UNKNOWN_MODE))
    	draw_cursor_image(scp);

    /* get screen update going */
    scrn_timer(NULL);

    update_leds(scp->status);

    if ((crtc_type == KD_VGA) && bootverbose) {
        printf("sc%d: BIOS video mode:%d\n", 
	    dev->id_unit, *(u_char *)pa_to_va(0x449));
        printf("sc%d: VGA registers upon power-up\n", dev->id_unit);
        dump_vgaregs(vgaregs);
        printf("sc%d: video mode:%d\n", dev->id_unit, scp->mode);
        printf("sc%d: VGA registers in BIOS for mode:%d\n", 
		dev->id_unit, scp->mode);
        dump_vgaregs(vgaregs2);
	p = get_mode_param(scp, scp->mode);
        if (p != NULL) {
            printf("sc%d: VGA registers to be used for mode:%d\n", 
		dev->id_unit, scp->mode);
            dump_vgaregs(p);
        }
        printf("sc%d: rows_offset:%d\n", dev->id_unit, rows_offset);
    }
    if ((crtc_type == KD_VGA) && (video_mode_ptr == NULL))
        printf("sc%d: WARNING: video mode switching is only partially supported\n",
	        dev->id_unit); 

    printf("sc%d: ", dev->id_unit);
    switch(crtc_type) {
    case KD_VGA:
	if (crtc_addr == MONO_BASE)
	    printf("VGA mono");
	else
	    printf("VGA color");
	break;
    case KD_EGA:
	if (crtc_addr == MONO_BASE)
	    printf("EGA mono");
	else
	    printf("EGA color");
	break;
    case KD_CGA:
	printf("CGA");
	break;
    case KD_MONO:
    case KD_HERCULES:
    default:
	printf("MDA/hercules");
	break;
    }
    printf(" <%d virtual consoles, flags=0x%x>\n", MAXCONS, flags);

#if NAPM > 0
    scp->r_hook.ah_fun = scresume;
    scp->r_hook.ah_arg = NULL;
    scp->r_hook.ah_name = "system keyboard";
    scp->r_hook.ah_order = APM_MID_ORDER;
    apm_hook_establish(APM_HOOK_RESUME , &scp->r_hook);
#endif

    cdevsw_add(&cdev, &scdevsw, NULL);

#ifdef DEVFS
    for (vc = 0; vc < MAXCONS; vc++)
        sc_devfs_token[vc] = devfs_add_devswf(&scdevsw, vc, DV_CHR,
				UID_ROOT, GID_WHEEL, 0600, "ttyv%n", vc);
    sc_mouse_devfs_token = devfs_add_devswf(&scdevsw, SC_MOUSE, DV_CHR,
				UID_ROOT, GID_WHEEL, 0600, "sysmouse");
    sc_console_devfs_token = devfs_add_devswf(&scdevsw, SC_CONSOLE, DV_CHR,
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

    if (!tp)
	return(ENXIO);

    tp->t_oproc = (minor(dev) == SC_MOUSE) ? scmousestart : scstart;
    tp->t_param = scparam;
    tp->t_dev = dev;
    if (!(tp->t_state & TS_ISOPEN)) {
	ttychars(tp);
        /* Use the current setting of the <-- key as default VERASE. */  
        /* If the Delete key is preferable, an stty is necessary     */
        tp->t_cc[VERASE] = key_map.key[0x0e].map[0];
	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_cflag = TTYDEF_CFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	scparam(tp, &tp->t_termios);
	ttsetwater(tp);
	(*linesw[tp->t_line].l_modem)(tp, 1);
    	if (minor(dev) == SC_MOUSE)
	    mouse_level = 0;		/* XXX */
    }
    else
	if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
	    return(EBUSY);
    if (minor(dev) < MAXCONS && !console[minor(dev)]) {
	console[minor(dev)] = alloc_scp();
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
	scp = get_scr_stat(tp->t_dev);
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
	    if (scp->history != NULL) {
		free(scp->history, M_DEVBUF);
		if (scp->history_size / scp->xsize
			> imax(SC_HISTORY_SIZE, scp->ysize))
		    extra_history_size += scp->history_size / scp->xsize 
			- imax(SC_HISTORY_SIZE, scp->ysize);
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

void
scintr(int unit)
{
    static struct tty *cur_tty;
    int c, len;
    u_char *cp;

    /* make screensaver happy */
    scrn_time_stamp = mono_time.tv_sec;

    /* 
     * Loop while there is still input to get from the keyboard.
     * I don't think this is nessesary, and it doesn't fix
     * the Xaccel-2.1 keyboard hang, but it can't hurt.		XXX
     */
    while ((c = scgetc(SCGETC_NONBLOCK)) != NOKEY) {

	cur_tty = VIRTUAL_TTY(get_scr_num());
	if (!(cur_tty->t_state & TS_ISOPEN))
	    if (!((cur_tty = CONSOLE_TTY)->t_state & TS_ISOPEN))
		continue;

	switch (c & 0xff00) {
	case 0x0000: /* normal key */
	    (*linesw[cur_tty->t_line].l_rint)(c & 0xFF, cur_tty);
	    break;
	case FKEY:  /* function key, return string */
	    if (cp = get_fstr((u_int)c, (u_int *)&len)) {
	    	while (len-- >  0)
		    (*linesw[cur_tty->t_line].l_rint)(*cp++ & 0xFF, cur_tty);
	    }
	    break;
	case MKEY:  /* meta is active, prepend ESC */
	    (*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)(c & 0xFF, cur_tty);
	    break;
	case BKEY:  /* backtab fixed sequence (esc [ Z) */
	    (*linesw[cur_tty->t_line].l_rint)(0x1b, cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)('[', cur_tty);
	    (*linesw[cur_tty->t_line].l_rint)('Z', cur_tty);
	    break;
	}
    }

    if (cur_console->status & MOUSE_ENABLED) {
	cur_console->status &= ~MOUSE_VISIBLE;
	remove_mouse_image(cur_console);
    }
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
scioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
    int error;
    u_int i;
    struct tty *tp;
    scr_stat *scp;
    u_short *usp;
    char *mp;
    int s;

    tp = scdevtotty(dev);
    if (!tp)
	return ENXIO;
    scp = get_scr_stat(tp->t_dev);

    /* If there is a user_ioctl function call that first */
    if (sc_user_ioctl) {
	if (error = (*sc_user_ioctl)(dev, cmd, data, flag, p))
	    return error;
    }

    switch (cmd) {  		/* process console hardware related ioctl's */

    case GIO_ATTR:      	/* get current attributes */
	*(int*)data = (scp->term.cur_attr >> 8) & 0xFF;
	return 0;

    case GIO_COLOR:     	/* is this a color console ? */
	if (crtc_addr == COLOR_BASE)
	    *(int*)data = 1;
	else
	    *(int*)data = 0;
	return 0;

    case CONS_CURRENT:  	/* get current adapter type */
	*(int *)data = crtc_type;
	return 0;

    case CONS_GET:      	/* get current video mode */
	*(int*)data = scp->mode;
	return 0;

    case CONS_BLANKTIME:    	/* set screen saver timeout (0 = no saver) */
	if (*(int *)data < 0)
            return EINVAL;
	scrn_blank_time = *(int *)data;
	if (scrn_blank_time == 0)
	    scrn_time_stamp = mono_time.tv_sec;
	return 0;

    case CONS_CURSORTYPE:   	/* set cursor type blink/noblink */
	if ((*(int*)data) & 0x01)
	    flags |= BLINK_CURSOR;
	else
	    flags &= ~BLINK_CURSOR;
	if ((*(int*)data) & 0x02) {
	    if (!crtc_vga)
		return ENXIO;
	    flags |= CHAR_CURSOR;
	} else
	    flags &= ~CHAR_CURSOR;
	/* 
	 * The cursor shape is global property; all virtual consoles
	 * are affected. Update the cursor in the current console...
	 */
	if (!(cur_console->status & UNKNOWN_MODE)) {
            remove_cursor_image(cur_console);
	    if (flags & CHAR_CURSOR)
	        set_destructive_cursor(cur_console);
	    draw_cursor_image(cur_console);
	}
	return 0;

    case CONS_BELLTYPE: 	/* set bell type sound/visual */
	if (*data)
	    flags |= VISUAL_BELL;
	else
	    flags &= ~VISUAL_BELL;
	return 0;

    case CONS_HISTORY:  	/* set history size */
	if (*(int *)data > 0) {
	    int lines;	/* buffer size to allocate */
	    int lines0;	/* current buffer size */

	    lines = imax(*(int *)data, scp->ysize);
	    lines0 = (scp->history != NULL) ? 
		      scp->history_size / scp->xsize : scp->ysize;
	    /*
	     * syscons unconditionally allocates buffers upto SC_HISTORY_SIZE
	     * lines or scp->ysize lines, whichever is larger. A value 
	     * greater than that is allowed, subject to extra_history_size.
	     */
	    if (lines > imax(lines0, SC_HISTORY_SIZE) + extra_history_size)
                return EINVAL;
            if (cur_console->status & BUFFER_SAVED)
                return EBUSY;
	    usp = scp->history;
	    scp->history = NULL;
	    if (usp != NULL)
		free(usp, M_DEVBUF);
	    scp->history_size = lines * scp->xsize;
	    /*
	     * extra_history_size += 
	     *    (lines0 > imax(SC_HISTORY_SIZE, scp->ysize)) ? 
	     *     lines0 - imax(SC_HISTORY_SIZE, scp->ysize)) : 0;
	     * extra_history_size -= 
	     *    (lines > imax(SC_HISTORY_SIZE, scp->ysize)) ? 
	     *	   lines - imax(SC_HISTORY_SIZE, scp->ysize)) : 0;
	     * lines0 >= ysize && lines >= ysize... Hey, the above can be 
	     * reduced to the following...
	     */
	    extra_history_size += 
		imax(lines0, SC_HISTORY_SIZE) - imax(lines, SC_HISTORY_SIZE);
	    usp = (u_short *)malloc(scp->history_size * sizeof(u_short),
				    M_DEVBUF, M_WAITOK);
	    bzero(usp, scp->history_size * sizeof(u_short));
	    scp->history_head = scp->history_pos = usp;
	    scp->history = usp;
	    return 0;
	}
	else
	    return EINVAL;

    case CONS_MOUSECTL:		/* control mouse arrow */
    {
	/* MOUSE_BUTTON?DOWN -> MOUSE_MSC_BUTTON?UP */
	static butmap[8] = {
            MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP 
		| MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON3UP,
            MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP,
            MOUSE_MSC_BUTTON2UP,
            MOUSE_MSC_BUTTON1UP,
            0,
	};
	mouse_info_t *mouse = (mouse_info_t*)data;

	if (!crtc_vga)
	    return ENODEV;
	
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
	    break;

	case MOUSE_SHOW:
	    if (!(scp->status & MOUSE_ENABLED)) {
		scp->status |= (MOUSE_ENABLED | MOUSE_VISIBLE);
		scp->mouse_oldpos = scp->mouse_pos;
		mark_all(scp);
	    }
	    else
		return EINVAL;
	    break;

	case MOUSE_HIDE:
	    if (scp->status & MOUSE_ENABLED) {
		scp->status &= ~(MOUSE_ENABLED | MOUSE_VISIBLE);
		mark_all(scp);
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
	    break;

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

	    if (cur_console->status & MOUSE_ENABLED)
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
	    else if (mouse->operation == MOUSE_ACTION) {
		/* process button presses */
		if ((cur_console->mouse_buttons ^ mouse->u.data.buttons) && 
		    !(cur_console->status & UNKNOWN_MODE)) {
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
	    mouse_status.flags |= 
		((mouse->u.data.x || mouse->u.data.y || mouse->u.data.z) ? 
		    MOUSE_POSCHANGED : 0)
		| (mouse_status.obutton ^ mouse_status.button);

	    if (cur_console->status & MOUSE_ENABLED)
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

	    if (cur_console->status & UNKNOWN_MODE)
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
	scrn_time_stamp = mono_time.tv_sec;
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
	    ptr->mk_keylock = scp->status & LOCK_KEY_MASK;
	    return 0;
	}
	return EINVAL;
    }

    case CONS_GETVERS:  	/* get version number */
	*(int*)data = 0x200;    /* version 2.0 */
	return 0;

    /* VGA TEXT MODES */
    case SW_VGA_C40x25:
    case SW_VGA_C80x25: case SW_VGA_M80x25:
    case SW_VGA_C80x30: case SW_VGA_M80x30:
    case SW_VGA_C80x50: case SW_VGA_M80x50:
    case SW_VGA_C80x60: case SW_VGA_M80x60:
    case SW_B40x25:     case SW_C40x25:
    case SW_B80x25:     case SW_C80x25:
    case SW_ENH_B40x25: case SW_ENH_C40x25:
    case SW_ENH_B80x25: case SW_ENH_C80x25:
    case SW_ENH_B80x43: case SW_ENH_C80x43:
    case SW_EGAMONO80x25:

	if (!crtc_vga)
 	    return ENODEV;
 	mp = get_mode_param(scp, cmd & 0xff);
 	if (mp == NULL)
 	    return ENODEV;
 
	if (scp->history != NULL)
	    i = imax(scp->history_size / scp->xsize 
		     - imax(SC_HISTORY_SIZE, scp->ysize), 0);
	else
	    i = 0;
	switch (cmd & 0xff) {
	case M_VGA_C80x60: case M_VGA_M80x60:
	    if (!(fonts_loaded & FONT_8))
		return EINVAL;
	    scp->xsize = 80;
	    scp->ysize = 60;
	    break;
	case M_VGA_C80x50: case M_VGA_M80x50:
	    if (!(fonts_loaded & FONT_8))
		return EINVAL;
	    scp->xsize = 80;
	    scp->ysize = 50;
	    break;
	case M_ENH_B80x43: case M_ENH_C80x43:
	    if (!(fonts_loaded & FONT_8))
		return EINVAL;
	    scp->xsize = 80;
	    scp->ysize = 43;
	    break;
	case M_VGA_C80x30: case M_VGA_M80x30:
	    scp->xsize = 80;
	    scp->ysize = 30;
	    break;
	case M_ENH_C40x25: case M_ENH_B40x25:
	case M_ENH_C80x25: case M_ENH_B80x25:
	case M_EGAMONO80x25:
	    if (!(fonts_loaded & FONT_14))
		return EINVAL;
	    /* FALL THROUGH */
	default:
	    if ((cmd & 0xff) > M_VGA_CG320)
		return EINVAL;
            scp->xsize = mp[0];
            scp->ysize = mp[1] + rows_offset;
	    break;
	}
	scp->mode = cmd & 0xff;
	free(scp->scr_buf, M_DEVBUF);
	scp->scr_buf = (u_short *)
	    malloc(scp->xsize*scp->ysize*sizeof(u_short), M_DEVBUF, M_WAITOK);
    	scp->cursor_pos = scp->cursor_oldpos =
	    scp->scr_buf + scp->xpos + scp->ypos * scp->xsize;
    	scp->mouse_pos = scp->mouse_oldpos = 
	    scp->scr_buf + ((scp->mouse_ypos/scp->font_size)*scp->xsize +
	    scp->mouse_xpos/8);
	free(cut_buffer, M_DEVBUF);
    	cut_buffer = (char *)malloc(scp->xsize*scp->ysize, M_DEVBUF, M_NOWAIT);
	cut_buffer[0] = 0x00;
	usp = scp->history;
	scp->history = NULL;
	if (usp != NULL) {
	    free(usp, M_DEVBUF);
	    extra_history_size += i;
	}
	scp->history_size = imax(SC_HISTORY_SIZE, scp->ysize) * scp->xsize;
	usp = (u_short *)malloc(scp->history_size * sizeof(u_short), 
				M_DEVBUF, M_NOWAIT);
	if (usp != NULL)
	    bzero(usp, scp->history_size * sizeof(u_short));
	scp->history_head = scp->history_pos = usp;
	scp->history = usp;
	if (scp == cur_console)
	    set_mode(scp);
	scp->status &= ~UNKNOWN_MODE;
	clear_screen(scp);

	if (tp->t_winsize.ws_col != scp->xsize
	    || tp->t_winsize.ws_row != scp->ysize) {
	    tp->t_winsize.ws_col = scp->xsize;
	    tp->t_winsize.ws_row = scp->ysize;
	    pgsignal(tp->t_pgrp, SIGWINCH, 1);
	}
	return 0;

    /* GRAPHICS MODES */
    case SW_BG320:     case SW_BG640:
    case SW_CG320:     case SW_CG320_D:   case SW_CG640_E:
    case SW_CG640x350: case SW_ENH_CG640:
    case SW_BG640x480: case SW_CG640x480: case SW_VGA_CG320:

	if (!crtc_vga)
	    return ENODEV;
	mp = get_mode_param(scp, cmd & 0xff);
	if (mp == NULL)
	    return ENODEV;

	scp->mode = cmd & 0xFF;
	scp->xpixel = mp[0] * 8;
	scp->ypixel = (mp[1] + rows_offset) * mp[2];
	if (scp == cur_console)
	    set_mode(scp);
	scp->status |= UNKNOWN_MODE;    /* graphics mode */
	/* clear_graphics();*/

	if (tp->t_winsize.ws_xpixel != scp->xpixel
	    || tp->t_winsize.ws_ypixel != scp->ypixel) {
	    tp->t_winsize.ws_xpixel = scp->xpixel;
	    tp->t_winsize.ws_ypixel = scp->ypixel;
	    pgsignal(tp->t_pgrp, SIGWINCH, 1);
	}
	return 0;

    case SW_VGA_MODEX:
	if (!crtc_vga)
	    return ENODEV;
	mp = get_mode_param(scp, cmd & 0xff);
	if (mp == NULL)
	    return ENODEV;

	scp->mode = cmd & 0xFF;
	if (scp == cur_console)
	    set_mode(scp);
	scp->status |= UNKNOWN_MODE;    /* graphics mode */
	/* clear_graphics();*/
	scp->xpixel = 320;
	scp->ypixel = 240;
	if (tp->t_winsize.ws_xpixel != scp->xpixel
	    || tp->t_winsize.ws_ypixel != scp->ypixel) {
	    tp->t_winsize.ws_xpixel = scp->xpixel;
	    tp->t_winsize.ws_ypixel = scp->ypixel;
	    pgsignal(tp->t_pgrp, SIGWINCH, 1);
	}
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
	switch(*data) {
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
		*data = i + 1;
		return 0;
	    }
	}
	return EINVAL;

    case VT_ACTIVATE:   	/* switch to screen *data */
	return switch_scr(scp, (*data) - 1);

    case VT_WAITACTIVE: 	/* wait for switch to occur */
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

    case KDENABIO:      	/* allow io operations */
	error = suser(p->p_ucred, &p->p_acflag);
	if (error != 0)
	    return error;
	if (securelevel > 0)
	    return EPERM;
	p->p_md.md_regs->tf_eflags |= PSL_IOPL;
	return 0;

    case KDDISABIO:     	/* disallow io operations (default) */
	p->p_md.md_regs->tf_eflags &= ~PSL_IOPL;
	return 0;

    case KDSETMODE:     	/* set current mode of this (virtual) console */
	switch (*data) {
	case KD_TEXT:   	/* switch to TEXT (known) mode */
	    /* restore fonts & palette ! */
	    if (crtc_vga) {
		if (fonts_loaded & FONT_8)
		    copy_font(LOAD, FONT_8, font_8);
		if (fonts_loaded & FONT_14)
		    copy_font(LOAD, FONT_14, font_14);
		if (fonts_loaded & FONT_16)
		    copy_font(LOAD, FONT_16, font_16);
		load_palette(palette);
	    }

	    /* move hardware cursor out of the way */
	    outb(crtc_addr, 14);
	    outb(crtc_addr + 1, 0xff);
	    outb(crtc_addr, 15);
	    outb(crtc_addr + 1, 0xff);

	    /* FALL THROUGH */

	case KD_TEXT1:  	/* switch to TEXT (known) mode */
	    /* no restore fonts & palette */
	    if (crtc_vga)
		set_mode(scp);
	    scp->status &= ~UNKNOWN_MODE;
	    clear_screen(scp);
	    return 0;

	case KD_GRAPHICS:	/* switch to GRAPHICS (unknown) mode */
	    scp->status |= UNKNOWN_MODE;
	    return 0;
	default:
	    return EINVAL;
	}
	/* NOT REACHED */

    case KDGETMODE:     	/* get current mode of this (virtual) console */
	*data = (scp->status & UNKNOWN_MODE) ? KD_GRAPHICS : KD_TEXT;
	return 0;

    case KDSBORDER:     	/* set border color of this (virtual) console */
	scp->border = *data;
	if (scp == cur_console)
	    set_border(scp->border);
	return 0;

    case KDSKBSTATE:    	/* set keyboard state (locks) */
	if (*data >= 0 && *data <= LOCK_KEY_MASK) {
	    scp->status &= ~LOCK_KEY_MASK;
	    scp->status |= *data;
	    if (scp == cur_console)
		update_leds(scp->status);
	    return 0;
	}
	return EINVAL;

    case KDGKBSTATE:    	/* get keyboard state (locks) */
	*data = scp->status & LOCK_KEY_MASK;
	return 0;

    case KDSETRAD:      	/* set keyboard repeat & delay rates */
	if (*data & 0x80)
	    return EINVAL;
	if (sc_kbdc != NULL) 
	    set_keyboard(KBDC_SET_TYPEMATIC, *data);
	return 0;

    case KDSKBMODE:     	/* set keyboard mode */
	switch (*data) {
	case K_RAW: 		/* switch to RAW scancode mode */
	    scp->status &= ~KBD_CODE_MODE;
	    scp->status |= KBD_RAW_MODE;
	    return 0;

	case K_CODE: 		/* switch to CODE mode */
	    scp->status &= ~KBD_RAW_MODE;
	    scp->status |= KBD_CODE_MODE;
	    return 0;

	case K_XLATE:   	/* switch to XLT ascii mode */
	    if (scp == cur_console && scp->status & KBD_RAW_MODE)
		shfts = ctls = alts = agrs = metas = accents = 0;
	    scp->status &= ~(KBD_RAW_MODE | KBD_CODE_MODE);
	    return 0;
	default:
	    return EINVAL;
	}
	/* NOT REACHED */

    case KDGKBMODE:     	/* get keyboard mode */
	*data = (scp->status & KBD_RAW_MODE) ? K_RAW : 
		((scp->status & KBD_CODE_MODE) ? K_CODE : K_XLATE);
	return 0;

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

		/* set command for counter 2, 2 byte write */
		if (acquire_timer2(TIMER_16BIT|TIMER_SQWAVE))
		    return EBUSY;

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

    case KDGKBTYPE:     	/* get keyboard type */
	*data = 0;  		/* type not known (yet) */
	return 0;

    case KDSETLED:      	/* set keyboard LED status */
	if (*data >= 0 && *data <= LED_MASK) {
	    scp->status &= ~LED_MASK;
	    scp->status |= *data;
	    if (scp == cur_console)
		update_leds(scp->status);
	    return 0;
	}
	return EINVAL;

    case KDGETLED:      	/* get keyboard LED status */
	*data = scp->status & LED_MASK;
	return 0;

    case GETFKEY:       	/* get functionkey string */
	if (*(u_short*)data < n_fkey_tab) {
	    fkeyarg_t *ptr = (fkeyarg_t*)data;
	    bcopy(&fkey_tab[ptr->keynum].str, ptr->keydef,
		  fkey_tab[ptr->keynum].len);
	    ptr->flen = fkey_tab[ptr->keynum].len;
	    return 0;
	}
	else
	    return EINVAL;

    case SETFKEY:       	/* set functionkey string */
	if (*(u_short*)data < n_fkey_tab) {
	    fkeyarg_t *ptr = (fkeyarg_t*)data;
	    bcopy(ptr->keydef, &fkey_tab[ptr->keynum].str,
		  min(ptr->flen, MAXFK));
	    fkey_tab[ptr->keynum].len = min(ptr->flen, MAXFK);
	    return 0;
	}
	else
	    return EINVAL;

    case GIO_SCRNMAP:   	/* get output translation table */
	bcopy(&scr_map, data, sizeof(scr_map));
	return 0;

    case PIO_SCRNMAP:   	/* set output translation table */
	bcopy(data, &scr_map, sizeof(scr_map));
	for (i=0; i<sizeof(scr_map); i++)
	    scr_rmap[scr_map[i]] = i;
	return 0;

    case GIO_KEYMAP:    	/* get keyboard translation table */
	bcopy(&key_map, data, sizeof(key_map));
	return 0;

    case PIO_KEYMAP:    	/* set keyboard translation table */
	accents = 0;
	bzero(&accent_map, sizeof(accent_map));
	bcopy(data, &key_map, sizeof(key_map));
	return 0;

    case GIO_DEADKEYMAP:    	/* get accent key translation table */
	bcopy(&accent_map, data, sizeof(accent_map));
	return 0;

    case PIO_DEADKEYMAP:    	/* set accent key translation table */
	accents = 0;
	bcopy(data, &accent_map, sizeof(accent_map));
	return 0;

    case PIO_FONT8x8:   	/* set 8x8 dot font */
	if (!crtc_vga)
	    return ENXIO;
	bcopy(data, font_8, 8*256);
	fonts_loaded |= FONT_8;
	if (!(cur_console->status & UNKNOWN_MODE)) {
	    copy_font(LOAD, FONT_8, font_8);
	    if (flags & CHAR_CURSOR)
	        set_destructive_cursor(cur_console);
	}
	return 0;

    case GIO_FONT8x8:   	/* get 8x8 dot font */
	if (!crtc_vga)
	    return ENXIO;
	if (fonts_loaded & FONT_8) {
	    bcopy(font_8, data, 8*256);
	    return 0;
	}
	else
	    return ENXIO;

    case PIO_FONT8x14:  	/* set 8x14 dot font */
	if (!crtc_vga)
	    return ENXIO;
	bcopy(data, font_14, 14*256);
	fonts_loaded |= FONT_14;
	if (!(cur_console->status & UNKNOWN_MODE)) {
	    copy_font(LOAD, FONT_14, font_14);
	    if (flags & CHAR_CURSOR)
	        set_destructive_cursor(cur_console);
	}
	return 0;

    case GIO_FONT8x14:  	/* get 8x14 dot font */
	if (!crtc_vga)
	    return ENXIO;
	if (fonts_loaded & FONT_14) {
	    bcopy(font_14, data, 14*256);
	    return 0;
	}
	else
	    return ENXIO;

    case PIO_FONT8x16:  	/* set 8x16 dot font */
	if (!crtc_vga)
	    return ENXIO;
	bcopy(data, font_16, 16*256);
	fonts_loaded |= FONT_16;
	if (!(cur_console->status & UNKNOWN_MODE)) {
	    copy_font(LOAD, FONT_16, font_16);
	    if (flags & CHAR_CURSOR)
	        set_destructive_cursor(cur_console);
	}
	return 0;

    case GIO_FONT8x16:  	/* get 8x16 dot font */
	if (!crtc_vga)
	    return ENXIO;
	if (fonts_loaded & FONT_16) {
	    bcopy(font_16, data, 16*256);
	    return 0;
	}
	else
	    return ENXIO;
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
    scr_stat *scp = get_scr_stat(tp->t_dev);

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

void
sccnprobe(struct consdev *cp)
{
    struct isa_device *dvp;

    /*
     * Take control if we are the highest priority enabled display device.
     */
    dvp = find_display();
    if (dvp == NULL || dvp->id_driver != &scdriver) {
	cp->cn_pri = CN_DEAD;
	return;
    }

    if (!scvidprobe(dvp->id_unit, dvp->id_flags)) {
	cp->cn_pri = CN_DEAD;
	return;
    }

    /* initialize required fields */
    cp->cn_dev = makedev(CDEV_MAJOR, SC_CONSOLE);
    cp->cn_pri = CN_INTERNAL;

    sc_kbdc = kbdc_open(sc_port);
}

void
sccninit(struct consdev *cp)
{
    scinit();
}

void
sccnputc(dev_t dev, int c)
{
    u_char buf[1];
    int s;
    scr_stat *scp = console[0];
    term_stat save = scp->term;

    scp->term = kernel_console;
    current_default = &kernel_default;
    if (scp == cur_console && !(scp->status & UNKNOWN_MODE))
	remove_cursor_image(scp);
    buf[0] = c;
    ansi_put(scp, buf, 1);
    kernel_console = scp->term;
    current_default = &user_default;
    scp->term = save;
    s = splclock();
    if (scp == cur_console && !(scp->status & UNKNOWN_MODE)) {
	if (/* timer not running && */ (scp->start <= scp->end)) {
	    sc_bcopy(scp->scr_buf + scp->start, Crtat + scp->start,
		   (1 + scp->end - scp->start) * sizeof(u_short));
	    scp->start = scp->xsize * scp->ysize;
	    scp->end = 0;
	}
    	scp->cursor_oldpos = scp->cursor_pos;
	draw_cursor_image(scp);
    }
    splx(s);
}

int
sccngetc(dev_t dev)
{
    int s = spltty();	/* block scintr and scrn_timer while we poll */
    int c;

    /* 
     * Stop the screen saver if necessary.
     * What if we have been running in the screen saver code... XXX
     */
    if (scrn_blanked > 0)
        stop_scrn_saver(current_saver);

    c = scgetc(SCGETC_CN);

    /* make sure the screen saver won't be activated soon */
    scrn_time_stamp = mono_time.tv_sec;
    splx(s);
    return(c);
}

int
sccncheckc(dev_t dev)
{
    int c, s;

    s = spltty();
    if (scrn_blanked > 0)
        stop_scrn_saver(current_saver);
    c = scgetc(SCGETC_CN | SCGETC_NONBLOCK);
    if (c != NOKEY)
        scrn_time_stamp = mono_time.tv_sec;
    splx(s);
    return(c == NOKEY ? -1 : c);	/* c == -1 can't happen */
}

static scr_stat
*get_scr_stat(dev_t dev)
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
    scr_stat *scp = cur_console;
    int s = spltty();

    /* 
     * With release 2.1 of the Xaccel server, the keyboard is left
     * hanging pretty often. Apparently an interrupt from the
     * keyboard is lost, and I don't know why (yet).
     * This ugly hack calls scintr if input is ready for the keyboard
     * and conveniently hides the problem.			XXX
     */
    /* Try removing anything stuck in the keyboard controller; whether
     * it's a keyboard scan code or mouse data. `scintr()' doesn't
     * read the mouse data directly, but `kbdio' routines will, as a
     * side effect.
     */
    if (kbdc_lock(sc_kbdc, TRUE)) {
	/*
	 * We have seen the lock flag is not set. Let's reset the flag early;
	 * otherwise `update_led()' failes which may want the lock 
	 * during `scintr()'.
	 */
	kbdc_lock(sc_kbdc, FALSE);
	if (kbdc_data_ready(sc_kbdc)) 
	    scintr(0);
    }

    /* should we just return ? */
    if ((scp->status&UNKNOWN_MODE) || blink_in_progress || switch_in_progress) {
	timeout(scrn_timer, NULL, hz / 10);
	splx(s);
	return;
    }

    /* should we stop the screen saver? */
    if (mono_time.tv_sec <= scrn_time_stamp + scrn_blank_time)
	if (scrn_blanked > 0)
            stop_scrn_saver(current_saver);

    if (scrn_blanked <= 0) {
	/* update screen image */
	if (scp->start <= scp->end) {
	    sc_bcopy(scp->scr_buf + scp->start, Crtat + scp->start,
		   (1 + scp->end - scp->start) * sizeof(u_short));
	}

	/* update "pseudo" mouse pointer image */
	if ((scp->status & MOUSE_VISIBLE) && crtc_vga) {
	    /* did mouse move since last time ? */
	    if (scp->status & MOUSE_MOVED) {
		/* do we need to remove old mouse pointer image ? */
		if (scp->mouse_cut_start != NULL ||
		    (scp->mouse_pos-scp->scr_buf) <= scp->start ||
		    (scp->mouse_pos+scp->xsize+1-scp->scr_buf) >= scp->end) {
		    remove_mouse_image(scp);
		}
		scp->status &= ~MOUSE_MOVED;
		draw_mouse_image(scp);
	    }
	    else {
		/* mouse didn't move, has it been overwritten ? */
		if ((scp->mouse_pos+scp->xsize+1-scp->scr_buf) >= scp->start &&
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
		    if (flags & BLINK_CURSOR)
			draw_cursor_image(scp);
		}
	    }
	    blinkrate++;
	}

	if (scp->mouse_cut_start != NULL)
	    draw_cutmarking(scp);

	scp->end = 0;
	scp->start = scp->xsize*scp->ysize;
    }

    /* should we activate the screen saver? */
    if ((scrn_blank_time != 0) 
	    && (mono_time.tv_sec > scrn_time_stamp + scrn_blank_time))
	(*current_saver)(TRUE);

    timeout(scrn_timer, NULL, hz / 25);
    splx(s);
}

int
add_scrn_saver(void (*this_saver)(int))
{
    if (current_saver != none_saver)
	return EBUSY;
    current_saver = this_saver;
    return 0;
}

int
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
    if (scrn_blanked > 0)
        stop_scrn_saver(this_saver);

    return 0;
}

static void
stop_scrn_saver(void (*saver)(int))
{
    (*saver)(FALSE);
    scrn_time_stamp = mono_time.tv_sec;
    mark_all(cur_console);
}

static void
clear_screen(scr_stat *scp)
{
    move_crsr(scp, 0, 0);
    scp->cursor_oldpos = scp->cursor_pos;
    fillw(scp->term.cur_color | scr_map[0x20], scp->scr_buf,
	  scp->xsize * scp->ysize);
    mark_all(scp);
    remove_cutmarking(scp);
}

static int
switch_scr(scr_stat *scp, u_int next_scr)
{
    if (switch_in_progress && (cur_console->proc != pfind(cur_console->pid)))
	switch_in_progress = FALSE;

    if (next_scr >= MAXCONS || switch_in_progress ||
	(cur_console->smode.mode == VT_AUTO
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
    move_crsr(old_scp, old_scp->xpos, old_scp->ypos);
    cur_console = new_scp;
    if (old_scp->mode != new_scp->mode || (old_scp->status & UNKNOWN_MODE)){
	if (crtc_vga)
	    set_mode(new_scp);
    }
    move_crsr(new_scp, new_scp->xpos, new_scp->ypos);
    if (!(new_scp->status & UNKNOWN_MODE) && (flags & CHAR_CURSOR))
	set_destructive_cursor(new_scp);
    if ((old_scp->status & UNKNOWN_MODE) && crtc_vga)
	load_palette(palette);
    if (old_scp->status & KBD_RAW_MODE || new_scp->status & KBD_RAW_MODE ||
        old_scp->status & KBD_CODE_MODE || new_scp->status & KBD_CODE_MODE)
	shfts = ctls = alts = agrs = metas = accents = 0;
    set_border(new_scp->border);
    update_leds(new_scp->status);
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

    if (scp->term.esc == 1) {	/* seen ESC */
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

	case 'M':   /* Move cursor up 1 line, scroll if at top */
	    if (scp->ypos > 0)
		move_crsr(scp, scp->xpos, scp->ypos - 1);
	    else {
		bcopy(scp->scr_buf, scp->scr_buf + scp->xsize,
		       (scp->ysize - 1) * scp->xsize * sizeof(u_short));
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->scr_buf, scp->xsize);
    		mark_all(scp);
	    }
	    break;
#if notyet
	case 'Q':
	    scp->term.esc = 4;
	    return;
#endif
	case 'c':   /* Clear screen & home */
	    clear_screen(scp);
	    break;

	case '(':   /* iso-2022: designate 94 character set to G0 */
	    scp->term.esc = 5;
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
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos,
		      scp->scr_buf + scp->xsize * scp->ysize - scp->cursor_pos);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
    		mark_for_update(scp, scp->xsize * scp->ysize);
		remove_cutmarking(scp);
		break;
	    case 1: /* clear from beginning of display to cursor */
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->scr_buf,
		      scp->cursor_pos - scp->scr_buf);
    		mark_for_update(scp, 0);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
		remove_cutmarking(scp);
		break;
	    case 2: /* clear entire display */
		fillw(scp->term.cur_color | scr_map[0x20], scp->scr_buf,
		      scp->xsize * scp->ysize);
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
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos,
		      scp->xsize - scp->xpos);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf +
				scp->xsize - scp->xpos);
		break;
	    case 1: /* clear from beginning of line to cursor */
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos - scp->xpos,
		      scp->xpos + 1);
    		mark_for_update(scp, scp->ypos * scp->xsize);
    		mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
		break;
	    case 2: /* clear entire line */
		fillw(scp->term.cur_color | scr_map[0x20],
		      scp->cursor_pos - scp->xpos,
		      scp->xsize);
    		mark_for_update(scp, scp->ypos * scp->xsize);
    		mark_for_update(scp, (scp->ypos + 1) * scp->xsize);
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
	    fillw(scp->term.cur_color | scr_map[0x20], src,
		  n * scp->xsize);
	    mark_for_update(scp, scp->ypos * scp->xsize);
	    mark_for_update(scp, scp->xsize * scp->ysize);
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
	    fillw(scp->term.cur_color | scr_map[0x20], src,
		  n * scp->xsize);
	    mark_for_update(scp, scp->ypos * scp->xsize);
	    mark_for_update(scp, scp->xsize * scp->ysize);
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
	    fillw(scp->term.cur_color | scr_map[0x20], src, n);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf + n + count);
	    break;

	case '@':   /* Insert n chars */
	    n = scp->term.param[0]; if (n < 1) n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    src = scp->cursor_pos;
	    dst = src + n;
	    count = scp->xsize - (scp->xpos + n);
	    bcopy(src, dst, count * sizeof(u_short));
	    fillw(scp->term.cur_color | scr_map[0x20], src, n);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf + n + count);
	    break;

	case 'S':   /* scroll up n lines */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->ysize)
		n = scp->ysize;
	    bcopy(scp->scr_buf + (scp->xsize * n),
		   scp->scr_buf,
		   scp->xsize * (scp->ysize - n) * sizeof(u_short));
	    fillw(scp->term.cur_color | scr_map[0x20],
		  scp->scr_buf + scp->xsize * (scp->ysize - n),
		  scp->xsize * n);
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
	    fillw(scp->term.cur_color | scr_map[0x20],
		  scp->scr_buf, scp->xsize * n);
    	    mark_all(scp);
	    break;

	case 'X':   /* erase n characters in line */
	    n = scp->term.param[0]; if (n < 1)  n = 1;
	    if (n > scp->xsize - scp->xpos)
		n = scp->xsize - scp->xpos;
	    fillw(scp->term.cur_color | scr_map[0x20],
		  scp->cursor_pos, n);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf + n);
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
		    set_border(scp->border);
            }
	    break;

	case 'B':   /* set bell pitch and duration */
	    if (scp->term.num_param == 2) {
		scp->bell_pitch = scp->term.param[0];
		scp->bell_duration = scp->term.param[1]*10;
	    }
	    break;

	case 'C':   /* set cursor type & shape */
	    if (scp->term.num_param == 1) {
		if (scp->term.param[0] & 0x01)
		    flags |= BLINK_CURSOR;
		else
		    flags &= ~BLINK_CURSOR;
		if ((scp->term.param[0] & 0x02) && crtc_vga)
		    flags |= CHAR_CURSOR;
		else
		    flags &= ~CHAR_CURSOR;
	    }
	    else if (scp->term.num_param == 2) {
		scp->cursor_start = scp->term.param[0] & 0x1F;
		scp->cursor_end = scp->term.param[1] & 0x1F;
	    }
	    /* 
	     * The cursor shape is global property; all virtual consoles
	     * are affected. Update the cursor in the current console...
	     */
	    if (!(cur_console->status & UNKNOWN_MODE)) {
		remove_cursor_image(cur_console);
		if (crtc_vga && (flags & CHAR_CURSOR))
	            set_destructive_cursor(cur_console);
		draw_cursor_image(cur_console);
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

static void
ansi_put(scr_stat *scp, u_char *buf, int len)
{
    u_char *ptr = buf;

    /* make screensaver happy */
    if (scp == cur_console)
	scrn_time_stamp = mono_time.tv_sec;

    write_in_progress++;
outloop:
    if (scp->term.esc) {
	scan_esc(scp, *ptr++);
	len--;
    }
    else if (PRINTABLE(*ptr)) {     /* Print only printables */
 	int cnt = len <= (scp->xsize-scp->xpos) ? len : (scp->xsize-scp->xpos);
 	u_short cur_attr = scp->term.cur_attr;
 	u_short *cursor_pos = scp->cursor_pos;
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
	len -= (cursor_pos - scp->cursor_pos);
	scp->xpos += (cursor_pos - scp->cursor_pos);
	mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	mark_for_update(scp, cursor_pos - scp->scr_buf);
	scp->cursor_pos = cursor_pos;
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
		scp->cursor_pos--;
	    	mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
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
	    scp->cursor_pos += (8 - scp->xpos % 8u);
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    if ((scp->xpos += (8 - scp->xpos % 8u)) >= scp->xsize) {
	        scp->xpos = 0;
	        scp->ypos++;
	    }
	    break;

	case 0x0a:  /* newline, same pos */
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    scp->cursor_pos += scp->xsize;
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    scp->ypos++;
	    break;

	case 0x0c:  /* form feed, clears screen */
	    clear_screen(scp);
	    break;

	case 0x0d:  /* return, return to pos 0 */
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    scp->cursor_pos -= scp->xpos;
	    mark_for_update(scp, scp->cursor_pos - scp->scr_buf);
	    scp->xpos = 0;
	    break;

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
	if (scp->history) {
	    bcopy(scp->scr_buf, scp->history_head,
		   scp->xsize * sizeof(u_short));
	    scp->history_head += scp->xsize;
	    if (scp->history_head + scp->xsize >
		scp->history + scp->history_size)
		scp->history_head = scp->history;
	}
	bcopy(scp->scr_buf + scp->xsize, scp->scr_buf,
	       scp->xsize * (scp->ysize - 1) * sizeof(u_short));
	fillw(scp->term.cur_color | scr_map[0x20],
	      scp->scr_buf + scp->xsize * (scp->ysize - 1),
	      scp->xsize);
	scp->cursor_pos -= scp->xsize;
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
    u_int hw_cursor;
    u_int i;

    if (init_done != COLD)
	return;
    init_done = WARM;

    /*
     * Ensure a zero start address.  This is mainly to recover after
     * switching from pcvt using userconfig().  The registers are w/o
     * for old hardware so it's too hard to relocate the active screen
     * memory.
     */
    outb(crtc_addr, 12);
    outb(crtc_addr + 1, 0);
    outb(crtc_addr, 13);
    outb(crtc_addr + 1, 0);

    /* extract cursor location */
    outb(crtc_addr, 14);
    hw_cursor = inb(crtc_addr + 1) << 8;
    outb(crtc_addr, 15);
    hw_cursor |= inb(crtc_addr + 1);

    /*
     * Validate cursor location.  It may be off the screen.  Then we must
     * not use it for the initial buffer offset.
     */
    if (hw_cursor >= ROW * COL)
	hw_cursor = (ROW - 1) * COL;

    /* move hardware cursor out of the way */
    outb(crtc_addr, 14);
    outb(crtc_addr + 1, 0xff);
    outb(crtc_addr, 15);
    outb(crtc_addr + 1, 0xff);

    /* set up the first console */
    current_default = &user_default;
    console[0] = &main_console;
    init_scp(console[0]);
    cur_console = console[0];

    /* discard the video mode table if we are not familiar with it... */
    if (video_mode_ptr) {
        bzero(mode_map, sizeof(mode_map));
	bcopy(video_mode_ptr + MODE_PARAM_SIZE*console[0]->mode, 
	      vgaregs2, sizeof(vgaregs2));
        switch (comp_vgaregs(vgaregs, video_mode_ptr 
                    + MODE_PARAM_SIZE*console[0]->mode)) {
        case COMP_IDENTICAL:
            map_mode_table(mode_map, video_mode_ptr, M_VGA_CG320 + 1);
            /* 
             * This is a kludge for Toshiba DynaBook SS433 whose BIOS video
             * mode table entry has the actual # of rows at the offset 1; 
	     * BIOSes from other manufacturers store the # of rows - 1 there. 
	     * XXX
             */
	    rows_offset = vgaregs[1] + 1 
		- video_mode_ptr[MODE_PARAM_SIZE*console[0]->mode + 1];
            break;
        case COMP_SIMILAR:
            map_mode_table(mode_map, video_mode_ptr, M_VGA_CG320 + 1);
            mode_map[console[0]->mode] = vgaregs;
	    rows_offset = vgaregs[1] + 1 
		- video_mode_ptr[MODE_PARAM_SIZE*console[0]->mode + 1];
            vgaregs[1] -= rows_offset - 1;
            break;
        case COMP_DIFFERENT:
        default:
            video_mode_ptr = NULL;
            mode_map[console[0]->mode] = vgaregs;
	    rows_offset = 1;
            break;
        }
    }

    /* copy screen to temporary buffer */
    sc_bcopy(Crtat, sc_buffer,
	   console[0]->xsize * console[0]->ysize * sizeof(u_short));

    console[0]->scr_buf = console[0]->mouse_pos = sc_buffer;
    console[0]->cursor_pos = console[0]->cursor_oldpos = sc_buffer + hw_cursor;
    console[0]->cursor_saveunder = *console[0]->cursor_pos;
    console[0]->xpos = hw_cursor % COL;
    console[0]->ypos = hw_cursor / COL;
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

    /* Save font and palette if VGA */
    if (crtc_vga) {
	if (fonts_loaded & FONT_16) {
		copy_font(LOAD, FONT_16, font_16);
	} else {
		copy_font(SAVE, FONT_16, font_16);
		fonts_loaded = FONT_16;
	}
	save_palette();
	set_destructive_cursor(console[0]);
    }

#ifdef SC_SPLASH_SCREEN
    /* 
     * Now put up a graphics image, and maybe cycle a
     * couble of palette entries for simple animation.
     */
    toggle_splash_screen(cur_console);
#endif
}

static void
map_mode_table(char *map[], char *table, int max)
{
    int i;

    for(i = 0; i < max; ++i)
	map[i] = table + i*MODE_PARAM_SIZE;
    for(; i < MODE_MAP_SIZE; ++i)
	map[i] = NULL;
}

static u_char
map_mode_num(u_char mode)
{
    static struct {
        u_char from;
        u_char to;
    } mode_map[] = {
        { M_ENH_B80x43, M_ENH_B80x25 },
        { M_ENH_C80x43, M_ENH_C80x25 },
        { M_VGA_M80x30, M_VGA_M80x25 },
        { M_VGA_C80x30, M_VGA_C80x25 },
        { M_VGA_M80x50, M_VGA_M80x25 },
        { M_VGA_C80x50, M_VGA_C80x25 },
        { M_VGA_M80x60, M_VGA_M80x25 },
        { M_VGA_C80x60, M_VGA_C80x25 },
        { M_VGA_MODEX,  M_VGA_CG320 },
    };
    int i;

    for (i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i) {
        if (mode_map[i].from == mode)
            return mode_map[i].to;
    }
    return mode;
}

static char 
*get_mode_param(scr_stat *scp, u_char mode)
{
    if (mode >= MODE_MAP_SIZE)
	mode = map_mode_num(mode);
    if (mode < MODE_MAP_SIZE)
	return mode_map[mode];
    else
	return NULL;
}

static scr_stat
*alloc_scp()
{
    scr_stat *scp;

    scp = (scr_stat *)malloc(sizeof(scr_stat), M_DEVBUF, M_WAITOK);
    init_scp(scp);
    scp->scr_buf = scp->cursor_pos = scp->cursor_oldpos =
	(u_short *)malloc(scp->xsize*scp->ysize*sizeof(u_short),
			  M_DEVBUF, M_WAITOK);
    scp->mouse_pos = scp->mouse_oldpos = 
	scp->scr_buf + ((scp->mouse_ypos/scp->font_size)*scp->xsize +
			scp->mouse_xpos/8);
    scp->history_head = scp->history_pos =
	(u_short *)malloc(scp->history_size*sizeof(u_short),
			  M_DEVBUF, M_WAITOK);
    bzero(scp->history_head, scp->history_size*sizeof(u_short));
    scp->history = scp->history_head;
/* SOS
    if (crtc_vga && video_mode_ptr)
	set_mode(scp);
*/
    clear_screen(scp);
    scp->cursor_saveunder = *scp->cursor_pos;
    return scp;
}

static void
init_scp(scr_stat *scp)
{
    if (crtc_vga)
	if (crtc_addr == MONO_BASE)
	    scp->mode = M_VGA_M80x25;
	else
	    scp->mode = M_VGA_C80x25;
    else
	if (crtc_addr == MONO_BASE)
	    scp->mode = M_B80x25;
	else
	    scp->mode = M_C80x25;
    scp->initial_mode = scp->mode;

    scp->font_size = 16;
    scp->xsize = COL;
    scp->ysize = ROW;
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
    scp->cursor_start = *(char *)pa_to_va(0x461);
    scp->cursor_end = *(char *)pa_to_va(0x460);
    scp->mouse_xpos = scp->xsize*8/2;
    scp->mouse_ypos = scp->ysize*scp->font_size/2;
    scp->mouse_cut_start = scp->mouse_cut_end = NULL;
    scp->mouse_signal = 0;
    scp->mouse_pid = 0;
    scp->mouse_proc = NULL;
    scp->bell_pitch = BELL_PITCH;
    scp->bell_duration = BELL_DURATION;
    scp->status = (*(char *)pa_to_va(0x417) & 0x20) ? NLKED : 0;
    scp->status |= CURSOR_ENABLED;
    scp->pid = 0;
    scp->proc = NULL;
    scp->smode.mode = VT_AUTO;
    scp->history_head = scp->history_pos = scp->history = NULL;
    scp->history_size = imax(SC_HISTORY_SIZE, scp->ysize) * scp->xsize;
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
history_to_screen(scr_stat *scp)
{
    int i;

    for (i=0; i<scp->ysize; i++)
	bcopy(scp->history + (((scp->history_pos - scp->history) +
	       scp->history_size-((i+1)*scp->xsize))%scp->history_size),
	       scp->scr_buf + (scp->xsize * (scp->ysize-1 - i)),
	       scp->xsize * sizeof(u_short));
    mark_all(scp);
}

static int
history_up_line(scr_stat *scp)
{
    if (WRAPHIST(scp, scp->history_pos, -(scp->xsize*scp->ysize)) !=
	scp->history_head) {
	scp->history_pos = WRAPHIST(scp, scp->history_pos, -scp->xsize);
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
scgetc(u_int flags)
{
    struct key_t *key;
    u_char scancode, keycode;
    u_int state, action;
    int c;
    static u_char esc_flag = 0, compose = 0;
    static u_int chr = 0;

next_code:
    /* first see if there is something in the keyboard port */
    if (flags & SCGETC_NONBLOCK) {
	c = read_kbd_data_no_wait(sc_kbdc);
	if (c == -1)
	    return(NOKEY);
    } else {
	do {
	    c = read_kbd_data(sc_kbdc);
	} while(c == -1);
    }
    scancode = (u_char)c;

    /* do the /dev/random device a favour */
    if (!(flags & SCGETC_CN))
	add_keyboard_randomness(scancode);

    if (cur_console->status & KBD_RAW_MODE)
	return scancode;

    keycode = scancode & 0x7F;
    switch (esc_flag) {
    case 0x00:      /* normal scancode */
	switch(scancode) {
	case 0xB8:  /* left alt (compose key) */
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
    case 0xE0:      /* 0xE0 prefix */
	esc_flag = 0;
	switch (keycode) {
	case 0x1C:  /* right enter key */
	    keycode = 0x59;
	    break;
	case 0x1D:  /* right ctrl key */
	    keycode = 0x5A;
	    break;
	case 0x35:  /* keypad divide key */
	    keycode = 0x5B;
	    break;
	case 0x37:  /* print scrn key */
	    keycode = 0x5C;
	    break;
	case 0x38:  /* right alt key (alt gr) */
	    keycode = 0x5D;
	    break;
	case 0x47:  /* grey home key */
	    keycode = 0x5E;
	    break;
	case 0x48:  /* grey up arrow key */
	    keycode = 0x5F;
	    break;
	case 0x49:  /* grey page up key */
	    keycode = 0x60;
	    break;
	case 0x4B:  /* grey left arrow key */
	    keycode = 0x61;
	    break;
	case 0x4D:  /* grey right arrow key */
	    keycode = 0x62;
	    break;
	case 0x4F:  /* grey end key */
	    keycode = 0x63;
	    break;
	case 0x50:  /* grey down arrow key */
	    keycode = 0x64;
	    break;
	case 0x51:  /* grey page down key */
	    keycode = 0x65;
	    break;
	case 0x52:  /* grey insert key */
	    keycode = 0x66;
	    break;
	case 0x53:  /* grey delete key */
	    keycode = 0x67;
	    break;

	/* the following 3 are only used on the MS "Natural" keyboard */
	case 0x5b:  /* left Window key */
	    keycode = 0x69;
	    break;
	case 0x5c:  /* right Window key */
	    keycode = 0x6a;
	    break;
	case 0x5d:  /* menu key */
	    keycode = 0x6b;
	    break;
	default:    /* ignore everything else */
	    goto next_code;
	}
	break;
    case 0xE1:      /* 0xE1 prefix */
	esc_flag = 0;
	if (keycode == 0x1D)
	    esc_flag = 0x1D;
	goto next_code;
	/* NOT REACHED */
    case 0x1D:      /* pause / break */
	esc_flag = 0;
	if (keycode != 0x45)
	    goto next_code;
	keycode = 0x68;
	break;
    }

    if (cur_console->status & KBD_CODE_MODE)
	return (keycode | (scancode & 0x80));

    /* if scroll-lock pressed allow history browsing */
    if (cur_console->history && cur_console->status & SLKED) {
	int i;

	cur_console->status &= ~CURSOR_ENABLED;
	if (!(cur_console->status & BUFFER_SAVED)) {
	    cur_console->status |= BUFFER_SAVED;
	    cur_console->history_save = cur_console->history_head;

	    /* copy screen into top of history buffer */
	    for (i=0; i<cur_console->ysize; i++) {
		bcopy(cur_console->scr_buf + (cur_console->xsize * i),
		       cur_console->history_head,
		       cur_console->xsize * sizeof(u_short));
		cur_console->history_head += cur_console->xsize;
		if (cur_console->history_head + cur_console->xsize >
		    cur_console->history + cur_console->history_size)
		    cur_console->history_head=cur_console->history;
	    }
	    cur_console->history_pos = cur_console->history_head;
	    history_to_screen(cur_console);
	}
	switch (scancode) {
	case 0x47:  /* home key */
	    cur_console->history_pos = cur_console->history_head;
	    history_to_screen(cur_console);
	    goto next_code;

	case 0x4F:  /* end key */
	    cur_console->history_pos =
		WRAPHIST(cur_console, cur_console->history_head,
			 cur_console->xsize*cur_console->ysize);
	    history_to_screen(cur_console);
	    goto next_code;

	case 0x48:  /* up arrow key */
	    if (history_up_line(cur_console))
		do_bell(cur_console, BELL_PITCH, BELL_DURATION);
	    goto next_code;

	case 0x50:  /* down arrow key */
	    if (history_down_line(cur_console))
		do_bell(cur_console, BELL_PITCH, BELL_DURATION);
	    goto next_code;

	case 0x49:  /* page up key */
	    for (i=0; i<cur_console->ysize; i++)
	    if (history_up_line(cur_console)) {
		do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		break;
	    }
	    goto next_code;

	case 0x51:  /* page down key */
	    for (i=0; i<cur_console->ysize; i++)
	    if (history_down_line(cur_console)) {
		do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		break;
	    }
	    goto next_code;
	}
    }

    if (compose) {
	switch (scancode) {
	/* key pressed process it */
	case 0x47: case 0x48: case 0x49:    /* keypad 7,8,9 */
	    chr = (scancode - 0x40) + chr*10;
	    goto next_code;
	case 0x4B: case 0x4C: case 0x4D:    /* keypad 4,5,6 */
	    chr = (scancode - 0x47) + chr*10;
	    goto next_code;
	case 0x4F: case 0x50: case 0x51:    /* keypad 1,2,3 */
	    chr = (scancode - 0x4E) + chr*10;
	    goto next_code;
	case 0x52:              /* keypad 0 */
	    chr *= 10;
	    goto next_code;

	/* key release, no interest here */
	case 0xC7: case 0xC8: case 0xC9:    /* keypad 7,8,9 */
	case 0xCB: case 0xCC: case 0xCD:    /* keypad 4,5,6 */
	case 0xCF: case 0xD0: case 0xD1:    /* keypad 1,2,3 */
	case 0xD2:              /* keypad 0 */
	    goto next_code;

	case 0x38:              /* left alt key */
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
    if (scancode & 0x80) {      /* key released */
	if (key->spcl & (0x80>>state)) {
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
#ifdef SC_SPLASH_SCREEN
		toggle_splash_screen(cur_console); /* SOS XXX */
#endif
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
			    int i;
			    u_short *ptr = cur_console->history_save;

			    for (i=0; i<cur_console->ysize; i++) {
				bcopy(ptr,
				       cur_console->scr_buf +
				       (cur_console->xsize*i),
				       cur_console->xsize * sizeof(u_short));
				ptr += cur_console->xsize;
				if (ptr + cur_console->xsize >
				    cur_console->history +
				    cur_console->history_size)
				    ptr = cur_console->history;
			    }
			    cur_console->status &= ~BUFFER_SAVED;
			    cur_console->history_head=cur_console->history_save;
			    cur_console->status |= CURSOR_ENABLED;
			    mark_all(cur_console);
			}
			scstart(VIRTUAL_TTY(get_scr_num()));
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
	    case SPSC:
#ifdef SC_SPLASH_SCREEN
		accents = 0;
		toggle_splash_screen(cur_console);
#endif
		break;
	    case RBT:
		accents = 0;
		shutdown_nice();
		break;
	    case SUSP:
#if NAPM > 0
		accents = 0;
		apm_suspend();
#endif
		break;

	    case DBG:
#ifdef DDB          /* try to switch to console 0 */
		accents = 0;
		if (cur_console->smode.mode == VT_AUTO &&
		    console[0]->smode.mode == VT_AUTO)
		    switch_scr(cur_console, 0);
		Debugger("manual escape to debugger");
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
		{
		int next, this = get_scr_num();
		accents = 0;
		for (next = this+1; next != this; next = (next+1)%MAXCONS) {
		    struct tty *tp = VIRTUAL_TTY(next);
		    if (tp->t_state & TS_ISOPEN) {
			switch_scr(cur_console, next);
			break;
		    }
		}
		}
		break;
	    case BTAB:
		accents = 0;
		return(BKEY);
	    default:
		if (action >= F_ACC && action <= L_ACC) {
		    /* turn it into an index */
		    action -= F_ACC - 1;
		    if ((action > accent_map.n_accs) 
			|| (accent_map.acc[action - 1].accchar == 0)) {
			/* 
			 * The index is out of range or pointing to an 
			 * empty entry.
			 */
			accents = 0;
			do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		    }
		    /* 
		     * If the same accent key has been hit twice,
		     * produce the accent char itself.
		     */
		    if (action == accents) {
			action = accent_map.acc[accents - 1].accchar;
			accents = 0;
			if (metas)
			    action |= MKEY;
			return (action);
		    }
		    /* remember the index and wait for the next key stroke */
		    accents = action; 
		    break;
		}
		if (accents > 0) {
		    accents = 0;
		    do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		}
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
	    if (accents) {
		struct acc_t *acc;
		int i;

		acc = &accent_map.acc[accents - 1];
		accents = 0;
		/* 
		 * If the accent key is followed by the space key,
		 * produce the accent char itself.
		 */
		if (action == ' ') {
		    action = acc->accchar;
		    if (metas)
			action |= MKEY;
		    return (action);
		}
		for (i = 0; i < NUM_ACCENTCHARS; ++i) {
		    if (acc->map[i][0] == 0)	/* end of the map entry */
			break;
		    if (acc->map[i][0] == action) {
			action = acc->map[i][1];
			if (metas)
			    action |= MKEY;
			return (action);
		    }
		}
		do_bell(cur_console, BELL_PITCH, BELL_DURATION);
		goto next_code;
	    }
	    if (metas)
		action |= MKEY;
	    return(action);
	}
    }
    goto next_code;
}

int
scmmap(dev_t dev, int offset, int nprot)
{
    if (offset > 0x20000 - PAGE_SIZE)
	return -1;
    return i386_btop((VIDEOMEM + offset));
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

static void
set_keyboard(int command, int data)
{
    int s;

    if (sc_kbdc == NULL)
	return;

    /* prevent the timeout routine from polling the keyboard */
    if (!kbdc_lock(sc_kbdc, TRUE)) 
	return;

    /* disable the keyboard and mouse interrupt */
    s = spltty();
#if 0
    c = get_controller_command_byte(sc_kbdc);
    if ((c == -1) 
	|| !set_controller_command_byte(sc_kbdc, 
            kbdc_get_device_mask(sc_kbdc),
            KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
                | KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
	/* CONTROLLER ERROR */
        kbdc_lock(sc_kbdc, FALSE);
	splx(s);
	return;
    }
    /* 
     * Now that the keyboard controller is told not to generate 
     * the keyboard and mouse interrupts, call `splx()' to allow 
     * the other tty interrupts. The clock interrupt may also occur, 
     * but the timeout routine (`scrn_timer()') will be blocked 
     * by the lock flag set via `kbdc_lock()'
     */
    splx(s);
#endif

    if (send_kbd_command_and_data(sc_kbdc, command, data) != KBD_ACK)
        send_kbd_command(sc_kbdc, KBDC_ENABLE_KBD);

#if 0
    /* restore the interrupts */
    if (!set_controller_command_byte(sc_kbdc,
            kbdc_get_device_mask(sc_kbdc),
	    c & (KBD_KBD_CONTROL_BITS | KBD_AUX_CONTROL_BITS))) { 
	/* CONTROLLER ERROR */
    }
#else
    splx(s);
#endif
    kbdc_lock(sc_kbdc, FALSE);
}

static void
update_leds(int which)
{
    static u_char xlate_leds[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

    /* replace CAPS led with ALTGR led for ALTGR keyboards */
    if (key_map.n_keys > ALTGR_OFFSET) {
	if (which & ALKED)
	    which |= CLKED;
	else
	    which &= ~CLKED;
    }

    set_keyboard(KBDC_SET_LEDS, xlate_leds[which & LED_MASK]);
}

void
set_mode(scr_stat *scp)
{
    char special_modetable[MODE_PARAM_SIZE];
    char *mp;

    if (scp != cur_console)
	return;

    /* 
     * even if mode switching is disabled, we can change back
     * to the initial mode or the custom mode based on the initial
     * mode if we have saved register values upon start-up.
     */
    mp = get_mode_param(scp, scp->mode);
    if (mp == NULL)
	return;
    bcopy(mp, &special_modetable, sizeof(special_modetable));

    /* setup video hardware for the given mode */
    switch (scp->mode) {
    case M_VGA_C80x60: case M_VGA_M80x60:
	special_modetable[2]  = 0x08;
	special_modetable[19] = 0x47;
	goto special_480l;

    case M_VGA_C80x30: case M_VGA_M80x30:
	special_modetable[19] = 0x4f;
special_480l:
	special_modetable[9] |= 0xc0;
	special_modetable[16] = 0x08;
	special_modetable[17] = 0x3e;
	special_modetable[26] = 0xea;
	special_modetable[28] = 0xdf;
	special_modetable[31] = 0xe7;
	special_modetable[32] = 0x04;
	goto setup_mode;

    case M_ENH_C80x43: case M_ENH_B80x43:
	special_modetable[28] = 87;
	goto special_80x50;

    case M_VGA_C80x50: case M_VGA_M80x50:
special_80x50:
	special_modetable[2] = 8;
	special_modetable[19] = 7;
	goto setup_mode;

    case M_VGA_C40x25: case M_VGA_C80x25:
    case M_VGA_M80x25:
    case M_B40x25:     case M_C40x25:
    case M_B80x25:     case M_C80x25:
    case M_ENH_B40x25: case M_ENH_C40x25:
    case M_ENH_B80x25: case M_ENH_C80x25:
    case M_EGAMONO80x25:

setup_mode:
	set_vgaregs(special_modetable);
	scp->font_size = special_modetable[2];

	/* set font type (size) */
	if (scp->font_size < 14) {
	    if (fonts_loaded & FONT_8)
		copy_font(LOAD, FONT_8, font_8);
	    outb(TSIDX, 0x03); outb(TSREG, 0x0A);   /* font 2 */
	} else if (scp->font_size >= 16) {
	    if (fonts_loaded & FONT_16)
		copy_font(LOAD, FONT_16, font_16);
	    outb(TSIDX, 0x03); outb(TSREG, 0x00);   /* font 0 */
	} else {
	    if (fonts_loaded & FONT_14)
		copy_font(LOAD, FONT_14, font_14);
	    outb(TSIDX, 0x03); outb(TSREG, 0x05);   /* font 1 */
	}
	if (flags & CHAR_CURSOR)
	    set_destructive_cursor(scp);
	mark_all(scp);
	break;

    case M_VGA_MODEX:
	/* "unchain" the VGA mode */
	special_modetable[5-1+0x04] &= 0xf7;
	special_modetable[5-1+0x04] |= 0x04;
	/* turn off doubleword mode */
	special_modetable[10+0x14] &= 0xbf;
	/* turn off word adressing */
	special_modetable[10+0x17] |= 0x40;
	/* set logical screen width */
	special_modetable[10+0x13] = 80;
	/* set 240 lines */
	special_modetable[10+0x11] = 0x2c;
	special_modetable[10+0x06] = 0x0d;
	special_modetable[10+0x07] = 0x3e;
	special_modetable[10+0x10] = 0xea;
	special_modetable[10+0x11] = 0xac;
	special_modetable[10+0x12] = 0xdf;
	special_modetable[10+0x15] = 0xe7;
	special_modetable[10+0x16] = 0x06;
	/* set vertical sync polarity to reflect aspect ratio */
	special_modetable[9] = 0xe3;
	goto setup_grmode;

    case M_BG320:     case M_CG320:     case M_BG640:
    case M_CG320_D:   case M_CG640_E:
    case M_CG640x350: case M_ENH_CG640:
    case M_BG640x480: case M_CG640x480: case M_VGA_CG320:

setup_grmode:
	set_vgaregs(special_modetable);
	scp->font_size = FONT_NONE;
	break;

    default:
	/* call user defined function XXX */
	break;
    }

    /* set border color for this (virtual) console */
    set_border(scp->border);
    return;
}

void
set_border(u_char color)
{
    switch (crtc_type) {
    case KD_EGA:
    case KD_VGA:
        inb(crtc_addr + 6);		/* reset flip-flop */
        outb(ATC, 0x31); outb(ATC, color);
	break;
    case KD_CGA:
	outb(crtc_addr + 5, color & 0x0f); /* color select register */
	break;
    case KD_MONO:
    case KD_HERCULES:
    default:
	break;
    }
}

static void
set_vgaregs(char *modetable)
{
    int i, s = splhigh();

    outb(TSIDX, 0x00); outb(TSREG, 0x01);   	/* stop sequencer */
    outb(TSIDX, 0x07); outb(TSREG, 0x00);   	/* unlock registers */
    for (i=0; i<4; i++) {           		/* program sequencer */
	outb(TSIDX, i+1);
	outb(TSREG, modetable[i+5]);
    }
    outb(MISC, modetable[9]);       		/* set dot-clock */
    outb(TSIDX, 0x00); outb(TSREG, 0x03);   	/* start sequencer */
    outb(crtc_addr, 0x11);
    outb(crtc_addr+1, inb(crtc_addr+1) & 0x7F);
    for (i=0; i<25; i++) {          		/* program crtc */
	outb(crtc_addr, i);
	if (i == 14 || i == 15)     		/* no hardware cursor */
	    outb(crtc_addr+1, 0xff);
	else
	    outb(crtc_addr+1, modetable[i+10]);
    }
    inb(crtc_addr+6);           		/* reset flip-flop */
    for (i=0; i<20; i++) {          		/* program attribute ctrl */
	outb(ATC, i);
	outb(ATC, modetable[i+35]);
    }
    for (i=0; i<9; i++) {           		/* program graph data ctrl */
	outb(GDCIDX, i);
	outb(GDCREG, modetable[i+55]);
    }
    inb(crtc_addr+6);           		/* reset flip-flop */
    outb(ATC, 0x20);            		/* enable palette */
    splx(s);
}

static void
read_vgaregs(char *buf)
{
    int i, j;
    int s;

    bzero(buf, MODE_PARAM_SIZE);

    s = splhigh();

    outb(TSIDX, 0x00); outb(TSREG, 0x01);   	/* stop sequencer */
    outb(TSIDX, 0x07); outb(TSREG, 0x00);   	/* unlock registers */
    for (i=0, j=5; i<4; i++) {           
	outb(TSIDX, i+1);
	buf[j++] = inb(TSREG);
    }
    buf[9] = inb(MISC + 10);      		/* dot-clock */
    outb(TSIDX, 0x00); outb(TSREG, 0x03);   	/* start sequencer */

    for (i=0, j=10; i<25; i++) {       		/* crtc */
	outb(crtc_addr, i);
	buf[j++] = inb(crtc_addr+1);
    }
    for (i=0, j=35; i<20; i++) {          	/* attribute ctrl */
        inb(crtc_addr+6);           		/* reset flip-flop */
	outb(ATC, i);
	buf[j++] = inb(ATC + 1);
    }
    for (i=0, j=55; i<9; i++) {           	/* graph data ctrl */
	outb(GDCIDX, i);
	buf[j++] = inb(GDCREG);
    }
    inb(crtc_addr+6);           		/* reset flip-flop */
    outb(ATC, 0x20);            		/* enable palette */

    buf[0] = *(char *)pa_to_va(0x44a);		/* COLS */
    buf[1] = *(char *)pa_to_va(0x484);		/* ROWS */
    buf[2] = *(char *)pa_to_va(0x485);		/* POINTS */
    buf[3] = *(char *)pa_to_va(0x44c);
    buf[4] = *(char *)pa_to_va(0x44d);

    splx(s);
}

static int 
comp_vgaregs(u_char *buf1, u_char *buf2)
{
    static struct {
        u_char mask;
    } params[MODE_PARAM_SIZE] = {
	0xff, 0x00, 0xff, 		/* COLS, ROWS, POINTS */
	0xff, 0xff, 			/* page length */
	0xfe, 0xff, 0xff, 0xff,		/* sequencer registers */
	0xf3,				/* misc register */
	0xff, 0xff, 0xff, 0x7f, 0xff,	/* CRTC */
	0xff, 0xff, 0xff, 0x7f, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xff, 0x7f, 0xff, 0xff,
	0x7f, 0xff, 0xff, 0xef, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff,	/* attribute controller registers */
	0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xf0,
	0xff, 0xff, 0xff, 0xff, 0xff,	/* GDC register */
	0xff, 0xff, 0xff, 0xff, 
    }; 
    int identical = TRUE;
    int i;

    for (i = 0; i < sizeof(params)/sizeof(params[0]); ++i) {
	if (params[i].mask == 0)	/* don't care */
	    continue;
	if ((buf1[i] & params[i].mask) != (buf2[i] & params[i].mask))
	    return COMP_DIFFERENT;
	if (buf1[i] != buf2[i])
	    identical = FALSE;
    }
    return (identical) ? COMP_IDENTICAL : COMP_SIMILAR;

#if 0
    for(i = 0; i < 20; ++i) {
	if (*buf1++ != *buf2++)
	    return COMP_DIFFERENT;
    }
    buf1 += 2;  /* skip the cursor shape */
    buf2 += 2;
    for(i = 22; i < 24; ++i) {
	if (*buf1++ != *buf2++)
	    return COMP_DIFFERENT;
    }
    buf1 += 2;  /* skip the cursor position */
    buf2 += 2;
    for(i = 26; i < MODE_PARAM_SIZE; ++i) {
	if (*buf1++ != *buf2++)
	    return COMP_DIFFERENT;
    }
    return COMP_IDENTICAL;
#endif
}

static void
dump_vgaregs(u_char *buf)
{
    int i;

    for(i = 0; i < MODE_PARAM_SIZE;) {
	printf("%02x ", buf[i]);
	if ((++i % 16) == 0)
	    printf("\n");
    }
}

static void
set_font_mode(u_char *buf)
{
    int s = splhigh();

    /* save register values */
    outb(TSIDX, 0x02); buf[0] = inb(TSREG);
    outb(TSIDX, 0x04); buf[1] = inb(TSREG);
    outb(GDCIDX, 0x04); buf[2] = inb(GDCREG);
    outb(GDCIDX, 0x05); buf[3] = inb(GDCREG);
    outb(GDCIDX, 0x06); buf[4] = inb(GDCREG);
    inb(crtc_addr + 6);
    outb(ATC, 0x10); buf[5] = inb(ATC + 1);

    /* setup vga for loading fonts (graphics plane mode) */
    inb(crtc_addr+6);           		/* reset flip-flop */
    outb(ATC, 0x10); outb(ATC, 0x01);
    inb(crtc_addr+6);               		/* reset flip-flop */
    outb(ATC, 0x20);            		/* enable palette */

#if SLOW_VGA
    outb(TSIDX, 0x02); outb(TSREG, 0x04);
    outb(TSIDX, 0x04); outb(TSREG, 0x06);
    outb(GDCIDX, 0x04); outb(GDCREG, 0x02);
    outb(GDCIDX, 0x05); outb(GDCREG, 0x00);
    outb(GDCIDX, 0x06); outb(GDCREG, 0x05);
#else
    outw(TSIDX, 0x0402);
    outw(TSIDX, 0x0604);
    outw(GDCIDX, 0x0204);
    outw(GDCIDX, 0x0005);
    outw(GDCIDX, 0x0506);               /* addr = a0000, 64kb */
#endif
    splx(s);
}

static void
set_normal_mode(u_char *buf)
{
    char *modetable;
    int s = splhigh();

    /* setup vga for normal operation mode again */
    inb(crtc_addr+6);           		/* reset flip-flop */
    outb(ATC, 0x10); outb(ATC, buf[5]);
    inb(crtc_addr+6);               		/* reset flip-flop */
    outb(ATC, 0x20);            		/* enable palette */

#if SLOW_VGA
    outb(TSIDX, 0x02); outb(TSREG, buf[0]);
    outb(TSIDX, 0x04); outb(TSREG, buf[1]);
    outb(GDCIDX, 0x04); outb(GDCREG, buf[2]);
    outb(GDCIDX, 0x05); outb(GDCREG, buf[3]);
    if (crtc_addr == MONO_BASE) {
	outb(GDCIDX, 0x06); outb(GDCREG,(buf[4] & 0x03) | 0x08);
    } else {
	outb(GDCIDX, 0x06); outb(GDCREG,(buf[4] & 0x03) | 0x0c);
    }
#else
    outw(TSIDX, 0x0002 | (buf[0] << 8));
    outw(TSIDX, 0x0004 | (buf[1] << 8));
    outw(GDCIDX, 0x0004 | (buf[2] << 8));
    outw(GDCIDX, 0x0005 | (buf[3] << 8));
    if (crtc_addr == MONO_BASE)
        outw(GDCIDX, 0x0006 | (((buf[4] & 0x03) | 0x08)<<8));
    else
        outw(GDCIDX, 0x0006 | (((buf[4] & 0x03) | 0x0c)<<8));
#endif
    splx(s);
}

void
copy_font(int operation, int font_type, char* font_image)
{
    int ch, line, segment, fontsize;
    u_char buf[PARAM_BUFSIZE];
    u_char val;

    switch (font_type) {
    default:
    case FONT_8:
	segment = 0x8000;
	fontsize = 8;
	break;
    case FONT_14:
	segment = 0x4000;
	fontsize = 14;
	break;
    case FONT_16:
	segment = 0x0000;
	fontsize = 16;
	break;
    }
    outb(TSIDX, 0x01); val = inb(TSREG);        /* disable screen */
    outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
    set_font_mode(buf);
    for (ch=0; ch < 256; ch++)
	for (line=0; line < fontsize; line++)
	if (operation)
	    *(char *)pa_to_va(VIDEOMEM+(segment)+(ch*32)+line) =
		    font_image[(ch*fontsize)+line];
	else
	    font_image[(ch*fontsize)+line] =
	    *(char *)pa_to_va(VIDEOMEM+(segment)+(ch*32)+line);
    set_normal_mode(buf);
    outb(TSIDX, 0x01); outb(TSREG, val & 0xDF); /* enable screen */
}

static void
set_destructive_cursor(scr_stat *scp)
{
    u_char buf[PARAM_BUFSIZE];
    u_char cursor[32];
    caddr_t address;
    int i;
    char *font_buffer;

    if (scp->font_size < 14) {
	font_buffer = font_8;
	address = (caddr_t)VIDEOMEM + 0x8000;
    }
    else if (scp->font_size >= 16) {
	font_buffer = font_16;
	address = (caddr_t)VIDEOMEM;
    }
    else {
	font_buffer = font_14;
	address = (caddr_t)VIDEOMEM + 0x4000;
    }

    if (scp->status & MOUSE_VISIBLE) {
	if ((scp->cursor_saveunder & 0xff) == 0xd0)
    	    bcopy(&scp->mouse_cursor[0], cursor, scp->font_size);
	else if ((scp->cursor_saveunder & 0xff) == 0xd1)
    	    bcopy(&scp->mouse_cursor[32], cursor, scp->font_size);
	else if ((scp->cursor_saveunder & 0xff) == 0xd2)
    	    bcopy(&scp->mouse_cursor[64], cursor, scp->font_size);
	else if ((scp->cursor_saveunder & 0xff) == 0xd3)
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
    while (!(inb(crtc_addr+6) & 0x08)) /* wait for vertical retrace */ ;
#endif
    set_font_mode(buf);
    sc_bcopy(cursor, (char *)pa_to_va(address) + DEAD_CHAR * 32, 32);
    set_normal_mode(buf);
}

static void
set_mouse_pos(scr_stat *scp)
{
    static int last_xpos = -1, last_ypos = -1;

    if (scp->mouse_xpos < 0)
	scp->mouse_xpos = 0;
    if (scp->mouse_ypos < 0)
	scp->mouse_ypos = 0;
    if (scp->status & UNKNOWN_MODE) {
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
	    cut_buffer[j++] = '\n';
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
	cut_buffer[j++] = '\n';
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
	    cut_buffer[0] = '\n';
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
	cut_buffer[i++] = '\n';
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
    caddr_t address;
    int i;
    char *font_buffer;
    u_char buf[PARAM_BUFSIZE];
    u_short buffer[32];
    u_short xoffset, yoffset;
    u_short *crt_pos = Crtat + (scp->mouse_pos - scp->scr_buf);
    int font_size = scp->font_size;

    if (font_size < 14) {
	font_buffer = font_8;
	address = (caddr_t)VIDEOMEM + 0x8000;
    }
    else if (font_size >= 16) {
	font_buffer = font_16;
	address = (caddr_t)VIDEOMEM;
    }
    else {
	font_buffer = font_14;
	address = (caddr_t)VIDEOMEM + 0x4000;
    }
    xoffset = scp->mouse_xpos % 8;
    yoffset = scp->mouse_ypos % font_size;

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

    scp->mouse_oldpos = scp->mouse_pos;

#if 1
    /* wait for vertical retrace to avoid jitter on some videocards */
    while (!(inb(crtc_addr+6) & 0x08)) /* idle */ ;
#endif
    set_font_mode(buf);
    sc_bcopy(scp->mouse_cursor, (char *)pa_to_va(address) + 0xd0 * 32, 128);
    set_normal_mode(buf);
    *(crt_pos) = (*(scp->mouse_pos)&0xff00)|0xd0;
    *(crt_pos+scp->xsize) = (*(scp->mouse_pos+scp->xsize)&0xff00)|0xd2;
    if (scp->mouse_xpos < (scp->xsize-1)*8) {
    	*(crt_pos+1) = (*(scp->mouse_pos+1)&0xff00)|0xd1;
    	*(crt_pos+scp->xsize+1) = (*(scp->mouse_pos+scp->xsize+1)&0xff00)|0xd3;
    }
    mark_for_update(scp, scp->mouse_pos - scp->scr_buf);
    mark_for_update(scp, scp->mouse_pos + scp->xsize + 1 - scp->scr_buf);
}

static void
remove_mouse_image(scr_stat *scp)
{
    u_short *crt_pos = Crtat + (scp->mouse_oldpos - scp->scr_buf);

    *(crt_pos) = *(scp->mouse_oldpos);
    *(crt_pos+1) = *(scp->mouse_oldpos+1);
    *(crt_pos+scp->xsize) = *(scp->mouse_oldpos+scp->xsize);
    *(crt_pos+scp->xsize+1) = *(scp->mouse_oldpos+scp->xsize+1);
    mark_for_update(scp, scp->mouse_oldpos - scp->scr_buf);
    mark_for_update(scp, scp->mouse_oldpos + scp->xsize + 1 - scp->scr_buf);
}

static void
draw_cutmarking(scr_stat *scp)
{
    u_short *ptr;
    u_short och, nch;

    for (ptr=scp->scr_buf; ptr<=(scp->scr_buf+(scp->xsize*scp->ysize)); ptr++) {
	nch = och = *(Crtat + (ptr - scp->scr_buf));
	/* are we outside the selected area ? */
	if ( ptr < (scp->mouse_cut_start > scp->mouse_cut_end ? 
	            scp->mouse_cut_end : scp->mouse_cut_start) ||
	     ptr >= (scp->mouse_cut_start > scp->mouse_cut_end ?
	            scp->mouse_cut_start : scp->mouse_cut_end)) {
	    if (ptr != scp->cursor_pos)
		nch = (och & 0xff) | (*ptr & 0xff00);
	}
	else {
	    /* are we clear of the cursor image ? */
	    if (ptr != scp->cursor_pos)
		nch = (och & 0x88ff) | (*ptr & 0x7000)>>4 | (*ptr & 0x0700)<<4;
	    else {
		if (flags & CHAR_CURSOR)
		    nch = (och & 0x88ff)|(*ptr & 0x7000)>>4|(*ptr & 0x0700)<<4;
		else 
		    if (!(flags & BLINK_CURSOR))
		        nch = (och & 0xff) | (*ptr & 0xff00);
	    }
	}
	if (nch != och)
	    *(Crtat + (ptr - scp->scr_buf)) = nch;
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
save_palette(void)
{
    int i;

    outb(PALRADR, 0x00);
    for (i=0x00; i<0x300; i++)
	palette[i] = inb(PALDATA);
    inb(crtc_addr+6);           /* reset flip/flop */
}

void
load_palette(char *palette)
{
    int i;

    outb(PIXMASK, 0xFF);            /* no pixelmask */
    outb(PALWADR, 0x00);
    for (i=0x00; i<0x300; i++)
	 outb(PALDATA, palette[i]);
    inb(crtc_addr+6);           /* reset flip/flop */
    outb(ATC, 0x20);            /* enable palette */
}

static void
do_bell(scr_stat *scp, int pitch, int duration)
{
    if (flags & VISUAL_BELL) {
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

    if ((scp->status & UNKNOWN_MODE) || (blink_in_progress <= 1)) {
	blink_in_progress = FALSE;
    	mark_all(scp);
	if (delayed_next_scr)
	    switch_scr(scp, delayed_next_scr - 1);
    }
    else {
	if (blink_in_progress & 1)
	    fillw(kernel_default.std_color | scr_map[0x20],
		  Crtat, scp->xsize * scp->ysize);
	else
	    fillw(kernel_default.rev_color | scr_map[0x20],
		  Crtat, scp->xsize * scp->ysize);
	blink_in_progress--;
	timeout(blink_screen, scp, hz / 10);
    }
}

#ifdef SC_SPLASH_SCREEN
static void
toggle_splash_screen(scr_stat *scp)
{
    static int toggle = 0;
    static u_char save_mode;
    int s;

    if (video_mode_ptr == NULL)
	return;

    s = splhigh();
    if (toggle) {
	scp->mode = save_mode;
	scp->status &= ~UNKNOWN_MODE;
	set_mode(scp);
	load_palette(palette);
	toggle = 0;
    }
    else {
	save_mode = scp->mode;
	scp->mode = M_VGA_CG320;
	scp->status |= UNKNOWN_MODE;
	set_mode(scp);
	/* load image */
	toggle = 1;
    }
    splx(s);
}
#endif
#endif /* NSC */
