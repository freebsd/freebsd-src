/*-
 * Copyright (c) 1998 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $Id: scvidctl.c,v 1.1 1998/12/16 14:57:37 kato Exp $
 */

#include "sc.h"
#include "opt_syscons.h"

#if NSC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/kernel.h>

#include <machine/apm_bios.h>
#include <machine/console.h>

#include <dev/fb/fbreg.h>

#include <pc98/pc98/syscons.h>

/* for compatibility with previous versions */
typedef struct old_video_adapter {
    int			va_index;
    int			va_type;
    int			va_flags;
#define V_ADP_COLOR	(1<<0)
#define V_ADP_MODECHANGE (1<<1)
#define V_ADP_STATESAVE	(1<<2)
#define V_ADP_STATELOAD	(1<<3)
#define V_ADP_FONT	(1<<4)
#define V_ADP_PALETTE	(1<<5)
#define V_ADP_BORDER	(1<<6)
#define V_ADP_VESA	(1<<7)
    int			va_crtc_addr;
    u_int		va_window;	/* virtual address */
    size_t		va_window_size;
    size_t		va_window_gran;
    u_int		va_buffer;	/* virtual address */
    size_t		va_buffer_size;
    int			va_initial_mode;
    int			va_initial_bios_mode;
    int			va_mode;
} old_video_adapter_t;

#define OLD_CONS_ADPINFO _IOWR('c', 101, old_video_adapter_t)

/* variables */
extern scr_stat *cur_console;
extern int fonts_loaded;
extern int sc_history_size;
extern u_char palette[];

int
sc_set_text_mode(scr_stat *scp, struct tty *tp, int mode, int xsize, int ysize,
		 int fontsize)
{
    video_info_t info;
    int error;
    int s;
    int i;

    if ((*vidsw[scp->ad]->get_info)(scp->adp, mode, &info))
	return ENODEV;
 
    /* adjust argument values */
    if (fontsize <= 0)
	fontsize = info.vi_cheight;
    if (fontsize < 14) {
	fontsize = 8;
	if (!(fonts_loaded & FONT_8))
	    return EINVAL;
    } else if (fontsize >= 16) {
	fontsize = 16;
	if (!(fonts_loaded & FONT_16))
	    return EINVAL;
    } else {
	fontsize = 14;
	if (!(fonts_loaded & FONT_14))
	    return EINVAL;
    }
    if ((xsize <= 0) || (xsize > info.vi_width))
	xsize = info.vi_width;
    if ((ysize <= 0) || (ysize > info.vi_height))
	ysize = info.vi_height;

    /* stop screen saver, etc */
    s = spltty();
    if ((error = sc_clean_up(scp))) {
	splx(s);
	return error;
    }

    /* set up scp */
    if (scp->history != NULL)
	i = imax(scp->history_size / scp->xsize 
		 - imax(sc_history_size, scp->ysize), 0);
    else
	i = 0;
    /*
     * This is a kludge to fend off scrn_update() while we
     * muck around with scp. XXX
     */
    scp->status |= UNKNOWN_MODE;
    scp->status &= ~(GRAPHICS_MODE | PIXEL_MODE);
    scp->mode = mode;
    scp->font_size = fontsize;
    scp->xsize = xsize;
    scp->ysize = ysize;
    scp->xoff = 0;
    scp->yoff = 0;
    scp->xpixel = scp->xsize*8;
    scp->ypixel = scp->ysize*fontsize;

    /* allocate buffers */
    sc_alloc_scr_buffer(scp, TRUE, TRUE);
    if (ISMOUSEAVAIL(scp->adp->va_flags))
	sc_alloc_cut_buffer(scp, FALSE);
    sc_alloc_history_buffer(scp, sc_history_size, i, FALSE);
    splx(s);

    if (scp == cur_console)
	set_mode(scp);
    scp->status &= ~UNKNOWN_MODE;

    if (tp == NULL)
	return 0;
    if (tp->t_winsize.ws_col != scp->xsize
	|| tp->t_winsize.ws_row != scp->ysize) {
	tp->t_winsize.ws_col = scp->xsize;
	tp->t_winsize.ws_row = scp->ysize;
	pgsignal(tp->t_pgrp, SIGWINCH, 1);
    }

    return 0;
}

int
sc_set_graphics_mode(scr_stat *scp, struct tty *tp, int mode)
{
    video_info_t info;
    int error;
    int s;

    if ((*vidsw[scp->ad]->get_info)(scp->adp, mode, &info))
	return ENODEV;

    /* stop screen saver, etc */
    s = spltty();
    if ((error = sc_clean_up(scp))) {
	splx(s);
	return error;
    }

    /* set up scp */
    scp->status |= (UNKNOWN_MODE | GRAPHICS_MODE);
    scp->status &= ~PIXEL_MODE;
    scp->mode = mode;
    scp->xoff = 0;
    scp->yoff = 0;
    scp->xpixel = info.vi_width;
    scp->ypixel = info.vi_height;
    scp->xsize = info.vi_width/8;
    scp->ysize = info.vi_height/info.vi_cheight;
    scp->font_size = FONT_NONE;
    /* move the mouse cursor at the center of the screen */
    sc_move_mouse(scp, scp->xpixel / 2, scp->ypixel / 2);
    splx(s);

    if (scp == cur_console)
	set_mode(scp);
    /* clear_graphics();*/
    scp->status &= ~UNKNOWN_MODE;

    if (tp == NULL)
	return 0;
    if (tp->t_winsize.ws_xpixel != scp->xpixel
	|| tp->t_winsize.ws_ypixel != scp->ypixel) {
	tp->t_winsize.ws_xpixel = scp->xpixel;
	tp->t_winsize.ws_ypixel = scp->ypixel;
	pgsignal(tp->t_pgrp, SIGWINCH, 1);
    }

    return 0;
}

int
sc_set_pixel_mode(scr_stat *scp, struct tty *tp, int xsize, int ysize, 
		  int fontsize)
{
    video_info_t info;
    int error;
    int s;
    int i;

    if ((*vidsw[scp->ad]->get_info)(scp->adp, scp->mode, &info))
	return ENODEV;		/* this shouldn't happen */

#ifdef SC_VIDEO_DEBUG
    if (scp->scr_buf != NULL) {
	printf("set_pixel_mode(): mode:%x, col:%d, row:%d, font:%d\n",
	       scp->mode, xsize, ysize, fontsize);
    }
#endif

    /* adjust argument values */
    if ((fontsize <= 0) || (fontsize == FONT_NONE))
	fontsize = info.vi_cheight;
    if (fontsize < 14) {
	fontsize = 8;
	if (!(fonts_loaded & FONT_8))
	    return EINVAL;
    } else if (fontsize >= 16) {
	fontsize = 16;
	if (!(fonts_loaded & FONT_16))
	    return EINVAL;
    } else {
	fontsize = 14;
	if (!(fonts_loaded & FONT_14))
	    return EINVAL;
    }
    if (xsize <= 0)
	xsize = info.vi_width/8;
    if (ysize <= 0)
	ysize = info.vi_height/fontsize;

#ifdef SC_VIDEO_DEBUG
    if (scp->scr_buf != NULL) {
	printf("set_pixel_mode(): mode:%x, col:%d, row:%d, font:%d\n",
	       scp->mode, xsize, ysize, fontsize);
	printf("set_pixel_mode(): window:%x, %dx%d, xoff:%d, yoff:%d\n",
	       scp->adp->va_window, info.vi_width, info.vi_height, 
	       (info.vi_width/8 - xsize)/2,
	       (info.vi_height/fontsize - ysize)/2);
    }
#endif

    if ((info.vi_width < xsize*8) || (info.vi_height < ysize*fontsize))
	return EINVAL;

    /* only 16 color, 4 plane modes are supported XXX */
    if ((info.vi_depth != 4) || (info.vi_planes != 4))
	return ENODEV;

    /*
     * set_pixel_mode() currently does not support video modes whose
     * memory size is larger than 64K. Because such modes require
     * bank switching to access the entire screen. XXX
     */
    if (info.vi_width*info.vi_height/8 > info.vi_window_size)
	return ENODEV;

    /* stop screen saver, etc */
    s = spltty();
    if ((error = sc_clean_up(scp))) {
	splx(s);
	return error;
    }

    /* set up scp */
    if (scp->history != NULL)
	i = imax(scp->history_size / scp->xsize 
		 - imax(sc_history_size, scp->ysize), 0);
    else
	i = 0;
    scp->status |= (UNKNOWN_MODE | PIXEL_MODE);
    scp->status &= ~(GRAPHICS_MODE | MOUSE_ENABLED);
    scp->xsize = xsize;
    scp->ysize = ysize;
    scp->font_size = fontsize;
    scp->xoff = (scp->xpixel/8 - xsize)/2;
    scp->yoff = (scp->ypixel/fontsize - ysize)/2;

    /* allocate buffers */
    sc_alloc_scr_buffer(scp, TRUE, TRUE);
    if (ISMOUSEAVAIL(scp->adp->va_flags))
	sc_alloc_cut_buffer(scp, FALSE);
    sc_alloc_history_buffer(scp, sc_history_size, i, FALSE);
    splx(s);

    if (scp == cur_console)
	set_border(scp, scp->border);

    scp->status &= ~UNKNOWN_MODE;

#ifdef SC_VIDEO_DEBUG
    printf("set_pixel_mode(): status:%x\n", scp->status);
#endif

    if (tp == NULL)
	return 0;
    if (tp->t_winsize.ws_col != scp->xsize
	|| tp->t_winsize.ws_row != scp->ysize) {
	tp->t_winsize.ws_col = scp->xsize;
	tp->t_winsize.ws_row = scp->ysize;
	pgsignal(tp->t_pgrp, SIGWINCH, 1);
    }

    return 0;
}

int
sc_vid_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, struct proc *p)
{
    scr_stat *scp;
    int error;
    int s;

    scp = sc_get_scr_stat(tp->t_dev);

    switch (cmd) {

    case CONS_CURRENT:  	/* get current adapter type */
	if (scp->adp == NULL)
	    return ENODEV;
	*(int *)data = scp->adp->va_type;
	return 0;

    case CONS_CURRENTADP:	/* get current adapter index */
	*(int *)data = scp->ad;
	return 0;

    case OLD_CONS_ADPINFO:	/* adapter information */
	if (scp->adp == NULL)
	    return ENODEV;
	((old_video_adapter_t *)data)->va_index = scp->adp->va_index;
	((old_video_adapter_t *)data)->va_type = scp->adp->va_type;
	((old_video_adapter_t *)data)->va_flags = scp->adp->va_flags;
	((old_video_adapter_t *)data)->va_crtc_addr = scp->adp->va_crtc_addr;
	((old_video_adapter_t *)data)->va_window = scp->adp->va_window;
	((old_video_adapter_t *)data)->va_window_size
	    = scp->adp->va_window_size;
	((old_video_adapter_t *)data)->va_window_gran
	    = scp->adp->va_window_gran;
	((old_video_adapter_t *)data)->va_buffer = scp->adp->va_buffer;
	((old_video_adapter_t *)data)->va_buffer_size
	    = scp->adp->va_buffer_size;
	((old_video_adapter_t *)data)->va_mode = scp->adp->va_mode;
	((old_video_adapter_t *)data)->va_initial_mode
	    = scp->adp->va_initial_mode;
	((old_video_adapter_t *)data)->va_initial_bios_mode
	    = scp->adp->va_initial_bios_mode;
	return 0;

    case CONS_ADPINFO:		/* adapter information */
	if (scp->adp == NULL)
	    return ENODEV;
	((video_adapter_info_t *)data)->va_index = scp->adp->va_index;
	((video_adapter_info_t *)data)->va_type = scp->adp->va_type;
	bcopy(scp->adp->va_name, ((video_adapter_info_t *)data)->va_name,
	      imin(strlen(scp->adp->va_name) + 1,
		   sizeof(((video_adapter_info_t *)data)->va_name))); 
	((video_adapter_info_t *)data)->va_unit = scp->adp->va_unit;
	((video_adapter_info_t *)data)->va_flags = scp->adp->va_flags;
	((video_adapter_info_t *)data)->va_io_base = scp->adp->va_io_base;
	((video_adapter_info_t *)data)->va_io_size = scp->adp->va_io_size;
	((video_adapter_info_t *)data)->va_crtc_addr = scp->adp->va_crtc_addr;
	((video_adapter_info_t *)data)->va_mem_base = scp->adp->va_mem_base;
	((video_adapter_info_t *)data)->va_mem_size = scp->adp->va_mem_size;
	((video_adapter_info_t *)data)->va_window = scp->adp->va_window;
	((video_adapter_info_t *)data)->va_window_size
	    = scp->adp->va_window_size;
	((video_adapter_info_t *)data)->va_window_gran
	    = scp->adp->va_window_gran;
	((video_adapter_info_t *)data)->va_buffer = scp->adp->va_buffer;
	((video_adapter_info_t *)data)->va_buffer_size
	    = scp->adp->va_buffer_size;
	((video_adapter_info_t *)data)->va_mode = scp->adp->va_mode;
	((video_adapter_info_t *)data)->va_mode_flags = scp->adp->va_mode_flags;
	((video_adapter_info_t *)data)->va_initial_mode
	    = scp->adp->va_initial_mode;
	((video_adapter_info_t *)data)->va_initial_bios_mode
	    = scp->adp->va_initial_bios_mode;
	return 0;

    case CONS_GET:      	/* get current video mode */
	*(int *)data = scp->mode;
	return 0;

    case CONS_MODEINFO:		/* get mode information */
	return ((*vidsw[scp->ad]->get_info)(scp->adp, 
		    ((video_info_t *)data)->vi_mode, (video_info_t *)data) 
		? ENODEV : 0);

    case CONS_FINDMODE:		/* find a matching video mode */
	return ((*vidsw[scp->ad]->query_mode)(scp->adp, (video_info_t *)data) 
		? ENODEV : 0);

    case CONS_SETWINORG:
	return ((*vidsw[scp->ad]->set_win_org)(scp->adp, *(u_int *)data) 
		   ? ENODEV : 0);

    /* generic text modes */
    case SW_TEXT_80x25:	case SW_TEXT_80x30:
    case SW_TEXT_80x43: case SW_TEXT_80x50:
    case SW_TEXT_80x60:
	/* FALL THROUGH */

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

#ifdef PC98
    /* PC98 TEXT MODES */
    case SW_PC98_80x25:
    case SW_PC98_80x30:
#endif
	if (!(scp->adp->va_flags & V_ADP_MODECHANGE))
 	    return ENODEV;
	return sc_set_text_mode(scp, tp, cmd & 0xff, 0, 0, 0);

    /* GRAPHICS MODES */
    case SW_BG320:     case SW_BG640:
    case SW_CG320:     case SW_CG320_D:   case SW_CG640_E:
    case SW_CG640x350: case SW_ENH_CG640:
    case SW_BG640x480: case SW_CG640x480: case SW_VGA_CG320:
    case SW_VGA_MODEX:
	if (!(scp->adp->va_flags & V_ADP_MODECHANGE))
	    return ENODEV;
	return sc_set_graphics_mode(scp, tp, cmd & 0xff);

    case KDSETMODE:     	/* set current mode of this (virtual) console */
	switch (*(int *)data) {
	case KD_TEXT:   	/* switch to TEXT (known) mode */
	    /*
	     * If scp->mode is of graphics modes, we don't know which
	     * text mode to switch back to...
	     */
	    if (scp->status & GRAPHICS_MODE)
		return EINVAL;
	    /* restore fonts & palette ! */
#if 0
	    if (ISFONTAVAIL(scp->adp->va_flags) 
		&& !(scp->status & (GRAPHICS_MODE | PIXEL_MODE)))
		/*
		 * FONT KLUDGE
		 * Don't load fonts for now... XXX
		 */
		if (fonts_loaded & FONT_8)
		    copy_font(scp, LOAD, 8, font_8);
		if (fonts_loaded & FONT_14)
		    copy_font(scp, LOAD, 14, font_14);
		if (fonts_loaded & FONT_16)
		    copy_font(scp, LOAD, 16, font_16);
	    }
#endif
	    load_palette(scp->adp, palette);

	    /* move hardware cursor out of the way */
	    (*vidsw[scp->ad]->set_hw_cursor)(scp->adp, -1, -1);

	    /* FALL THROUGH */

	case KD_TEXT1:  	/* switch to TEXT (known) mode */
	    /*
	     * If scp->mode is of graphics modes, we don't know which
	     * text/pixel mode to switch back to...
	     */
	    if (scp->status & GRAPHICS_MODE)
		return EINVAL;
	    s = spltty();
	    if ((error = sc_clean_up(scp))) {
		splx(s);
		return error;
	    }
#ifndef PC98
	    scp->status |= UNKNOWN_MODE;
	    splx(s);
	    /* no restore fonts & palette */
	    if (scp == cur_console)
		set_mode(scp);
	    sc_clear_screen(scp);
	    scp->status &= ~UNKNOWN_MODE;
#else /* PC98 */
	    scp->status &= ~UNKNOWN_MODE;
	    /* no restore fonts & palette */
	    if (scp == cur_console)
		set_mode(scp);
	    sc_clear_screen(scp);
	    splx(s);
#endif /* PC98 */
	    return 0;

	case KD_PIXEL:		/* pixel (raster) display */
	    if (!(scp->status & (GRAPHICS_MODE | PIXEL_MODE)))
		return EINVAL;
	    if (scp->status & GRAPHICS_MODE)
		return sc_set_pixel_mode(scp, tp, scp->xsize, scp->ysize, 
					 scp->font_size);
	    s = spltty();
	    if ((error = sc_clean_up(scp))) {
		splx(s);
		return error;
	    }
	    scp->status |= (UNKNOWN_MODE | PIXEL_MODE);
	    splx(s);
	    if (scp == cur_console) {
		set_mode(scp);
		load_palette(scp->adp, palette);
	    }
	    sc_clear_screen(scp);
	    scp->status &= ~UNKNOWN_MODE;
	    return 0;

	case KD_GRAPHICS:	/* switch to GRAPHICS (unknown) mode */
	    s = spltty();
	    if ((error = sc_clean_up(scp))) {
		splx(s);
		return error;
	    }
	    scp->status |= UNKNOWN_MODE;
	    splx(s);
#ifdef PC98
	    if (scp == cur_console)
		set_mode(scp);
#endif
	    return 0;

	default:
	    return EINVAL;
	}
	/* NOT REACHED */

    case KDRASTER:		/* set pixel (raster) display mode */
	if (ISUNKNOWNSC(scp) || ISTEXTSC(scp))
	    return ENODEV;
	return sc_set_pixel_mode(scp, tp, ((int *)data)[0], ((int *)data)[1], 
				 ((int *)data)[2]);

    case KDGETMODE:     	/* get current mode of this (virtual) console */
	/* 
	 * From the user program's point of view, KD_PIXEL is the same 
	 * as KD_TEXT... 
	 */
	*data = ISGRAPHSC(scp) ? KD_GRAPHICS : KD_TEXT;
	return 0;

    case KDSBORDER:     	/* set border color of this (virtual) console */
	scp->border = *data;
	if (scp == cur_console)
	    set_border(scp, scp->border);
	return 0;
    }

    return ENOIOCTL;
}

#endif /* NSC > 0 */
