/*
 * Copyright (c) 1999 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore and Scott Turner.
 *
 * Copyright (C) 1992, 1993 Soeren Schmidt.
 *
 * All rights reserved.
 *
 * For the sake of compatibility, portions of this code regarding the
 * X server interface are taken from Soeren Schmidt's syscons driver.
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
 *	This product includes software developed by Hellmuth Michaelis,
 *	Brian Dunford-Shore, Joerg Wunsch, Scott Turner and Soeren Schmidt.
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

/*---------------------------------------------------------------------------*
 *
 *	pcvt_sup.c	VT220 Driver Support Routines
 *	---------------------------------------------
 *
 *	Last Edit-Date: [Thu Dec 30 17:01:03 1999]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#if NVT > 0

#include <i386/isa/pcvt/pcvt_hdr.h>	/* global include */

#include <sys/resource.h>

static void vid_cursor ( struct cursorshape *data );
static void vgasetfontattr ( struct vgafontattr *data );
static void vgagetfontattr ( struct vgafontattr *data );
static void vgaloadchar ( struct vgaloadchar *data );
static void vid_getscreen ( struct screeninfo *data, Dev_t dev );
static void vid_setscreen ( struct screeninfo *data, Dev_t dev );
static void setchargen ( void );
static void setchargen3 ( void );
static void resetchargen ( void );
static void vgareadpel ( struct vgapel *data, Dev_t dev );
static void vgawritepel ( struct vgapel *data, Dev_t dev );
static void vgapcvtid ( struct pcvtid *data );
static void vgapcvtinfo ( struct pcvtinfo *data );

#ifdef XSERVER
static unsigned char * compute_charset_base ( unsigned fontset );
#endif /* XSERVER */

static struct callout_handle async_update_ch =
    CALLOUT_HANDLE_INITIALIZER(&async_update_ch);

#if PCVT_SCREENSAVER
static void scrnsv_timedout ( void *arg );
static struct callout_handle scrnsv_timeout_ch =
    CALLOUT_HANDLE_INITIALIZER(&scrnsv_timeout_ch);
static u_short *savedscreen = (u_short *)0;	/* ptr to screen contents */
static size_t scrnsv_size = (size_t)-1;		/* size of saved image */

#ifndef XSERVER
static unsigned scrnsv_timeout = 0;		/* initially off */
static void pcvt_set_scrnsv_tmo ( int timeout );/* else declared global */
#endif /* XSERVER */

#if PCVT_PRETTYSCRNS
static u_short *scrnsv_current = (u_short *)0;	/* attention char ptr */
static struct callout_handle scrnsv_blink_ch =
    CALLOUT_HANDLE_INITIALIZER(&scrnsv_blink_ch);
static void scrnsv_blink ( void * );
static u_short getrand ( void );
#endif /* PCVT_PRETTYSCRNS */

#endif /* PCVT_SCREENSAVER */


/*---------------------------------------------------------------------------*
 *	execute vga ioctls
 *---------------------------------------------------------------------------*/
int
vgaioctl(Dev_t dev, int cmd, caddr_t data, int flag)
{
	if(minor(dev) >= PCVT_NSCREENS)
		return -1;

/*
 * Some of the commands are not applicable if the vt in question, or the
 * current vt is in graphics mode (i.e., the X server acts on it); they
 * will cause an EAGAIN (resource temporarily unavailable) to be returned.
 */

#ifdef XSERVER
#if PCVT_USL_VT_COMPAT
#define is_dev_grafx vs[minor(dev)].vt_status & VT_GRAFX
#define is_current_grafx vsp->vt_status & VT_GRAFX
#else   /* old X interface */
#define is_dev_grafx pcvt_xmode
#define is_current_grafx pcvt_xmode
#endif /* PCVT_USL_VT_COMPAT */
#else /* !XSERVER */
#define is_dev_grafx 0  /* not applicable */
#define is_current_grafx 0
#endif /* XSERVER */

	switch(cmd)
	{
		case VGACURSOR:
			if(is_current_grafx)
				return EAGAIN;
			vid_cursor((struct cursorshape *)data);
			break;

		case VGALOADCHAR:
			if((adaptor_type != VGA_ADAPTOR) &&
			   (adaptor_type != EGA_ADAPTOR))
				return -1;
			if(is_current_grafx)
				return EAGAIN;
			vgaloadchar((struct vgaloadchar *)data);
			break;

		case VGASETFONTATTR:
			if((adaptor_type != VGA_ADAPTOR) &&
			   (adaptor_type != EGA_ADAPTOR))
				return -1;

#if PCVT_SCREENSAVER
			pcvt_scrnsv_reset();
#endif /* PCVT_SCREENSAVER */

			vgasetfontattr((struct vgafontattr *)data);
			break;

		case VGAGETFONTATTR:
			if((adaptor_type != VGA_ADAPTOR) &&
			   (adaptor_type != EGA_ADAPTOR))
				return -1;
			vgagetfontattr((struct vgafontattr *)data);
			break;

		case VGASETSCREEN:

#if defined XSERVER && !PCVT_USL_VT_COMPAT
			/* avoid screen switch if using old X mode */
			if(is_dev_grafx)
				return EAGAIN;
#endif /* XSERVER && !PCVT_USL_VT_COMPAT */

#if PCVT_SCREENSAVER
			pcvt_scrnsv_reset();
#endif /* PCVT_SCREENSAVER */

			vid_setscreen((struct screeninfo *)data, dev);
			break;

		case VGAGETSCREEN:
			vid_getscreen((struct screeninfo *)data, dev);
			break;

		case VGAREADPEL:
			if(adaptor_type != VGA_ADAPTOR)
				return -1;
			if(is_dev_grafx)
				return EAGAIN;
			vgareadpel((struct vgapel *)data, dev);
			break;

		case VGAWRITEPEL:
			if(adaptor_type != VGA_ADAPTOR)
				return -1;
			if(is_dev_grafx)
				return EAGAIN;
			vgawritepel((struct vgapel *)data, dev);
			break;

#if PCVT_SCREENSAVER
		case VGASCREENSAVER:
			if(is_current_grafx)
				return EAGAIN;
			pcvt_set_scrnsv_tmo(*(int *)data);
			pcvt_scrnsv_reset();
			break;
#endif /* PCVT_SCREENSAVER */

		case VGAPCVTID:
			vgapcvtid((struct pcvtid *)data);
			break;

		case VGAPCVTINFO:
			vgapcvtinfo((struct pcvtinfo *)data);
			break;

		case VGASETCOLMS:
			if(is_dev_grafx)
				return EAGAIN;
			if(*(int *)data == 80)
				(void)vt_col(&vs[minor(dev)], SCR_COL80);
			else if(*(int *)data == 132)
			{
				if(vt_col(&vs[minor(dev)], SCR_COL132) == 0)
					return EINVAL; /* not a VGA */
			}
			else
				return EINVAL;
			break;

#if 0			
		case SETSCROLLSIZE:
			reallocate_scrollbuffer(vsp, *(u_short *)data);
			break;
#endif

		case TIOCSWINSZ:
			/* do nothing here */
			break;

		default:
			return -1;
	}
	return 0;

#undef is_dev_grafx
#undef is_current_grafx
}

/*---------------------------------------------------------------------------*
 *	video ioctl - return driver id
 *---------------------------------------------------------------------------*/
static void
vgapcvtid(struct pcvtid *data)
{
	snprintf(data->name, sizeof(data->name), "%s", PCVTIDNAME);
	data->rmajor	= PCVTIDMAJOR;
	data->rminor	= PCVTIDMINOR;
}

/*---------------------------------------------------------------------------*
 *	video ioctl - return driver compile time options data
 *---------------------------------------------------------------------------*/
static void
vgapcvtinfo(struct pcvtinfo *data)
{
#if PCVT_NETBSD
	data->opsys	= CONF_NETBSD;
	data->opsysrel	= PCVT_NETBSD;
#elif PCVT_FREEBSD
	data->opsys	= CONF_FREEBSD;
	data->opsysrel	= PCVT_FREEBSD;
#else
	data->opsys	= CONF_UNKNOWNOPSYS;
	data->opsysrel	= 0;
#endif

	data->nscreens	= PCVT_NSCREENS;
	data->scanset	= PCVT_SCANSET;
	data->updatefast= PCVT_UPDATEFAST;
	data->updateslow= PCVT_UPDATESLOW;
	data->sysbeepf	= PCVT_SYSBEEPF;

#if PCVT_NETBSD || PCVT_FREEBSD >= 200
	data->pcburst	= PCVT_PCBURST;
#else
	data->pcburst	= 1;
#endif

#if PCVT_KBD_FIFO
	data->kbd_fifo_sz = PCVT_KBD_FIFO_SZ;
#else
	data->kbd_fifo_sz = 0;
#endif

	data->compile_opts = (0

#if PCVT_VT220KEYB
	| CONF_VT220KEYB
#endif
#if PCVT_SCREENSAVER
	| CONF_SCREENSAVER
#endif
#if PCVT_PRETTYSCRNS
	| CONF_PRETTYSCRNS
#endif
#if PCVT_CTRL_ALT_DEL
	| CONF_CTRL_ALT_DEL
#endif
#if PCVT_USEKBDSEC
	| CONF_USEKBDSEC
#endif
#if PCVT_24LINESDEF
	| CONF_24LINESDEF
#endif
#if PCVT_EMU_MOUSE
	| CONF_EMU_MOUSE
#endif
#if PCVT_SHOWKEYS
	| CONF_SHOWKEYS
#endif
#if PCVT_KEYBDID
	| CONF_KEYBDID
#endif
#if PCVT_SIGWINCH
	| CONF_SIGWINCH
#endif
#if PCVT_NULLCHARS
	| CONF_NULLCHARS
#endif
#if PCVT_BACKUP_FONTS
	| CONF_BACKUP_FONTS
#endif
#if PCVT_SW0CNOUTP	/* was FORCE8BIT */
	| CONF_SW0CNOUTP
#endif
#if PCVT_SETCOLOR
	| CONF_SETCOLOR
#endif
#if PCVT_132GENERIC
	| CONF_132GENERIC
#endif
#if PCVT_PALFLICKER
	| CONF_PALFLICKER
#endif
#if PCVT_WAITRETRACE
	| CONF_WAITRETRACE
#endif
#ifdef XSERVER
	| CONF_XSERVER
#endif
#if PCVT_USL_VT_COMPAT
	| CONF_USL_VT_COMPAT
#endif
#if PCVT_PORTIO_DELAY
	| CONF_PORTIO_DELAY
#endif
#if PCVT_INHIBIT_NUMLOCK
	| CONF_INHIBIT_NUMLOCK
#endif
#if PCVT_META_ESC
	| CONF_META_ESC
#endif
#if PCVT_KBD_FIFO
	| CONF_KBD_FIFO
#endif
#if PCVT_NOFASTSCROLL
	| CONF_NOFASTSCROLL
#endif
#if PCVT_SLOW_INTERRUPT
	| CONF_SLOW_INTERRUPT
#endif
#if PCVT_NO_LED_UPDATE
	| CONF_NO_LED_UPDATE
#endif
	);
}

/*---------------------------------------------------------------------------*
 *	video ioctl - set cursor appearence
 *---------------------------------------------------------------------------*/
static void
vid_cursor(struct cursorshape *data)
{
	int screen;
	int start;
	int end;
	int line_height;
	int character_set;

	/* for which virtual screen, -1 for current */
	screen = data->screen_no;

	if(screen == -1)	  /* current ? */
		screen = current_video_screen;
	else if(screen > totalscreens - 1)
		screen = totalscreens - 1;
	else if(screen < 0)
		screen = 0;

	if(adaptor_type == VGA_ADAPTOR || adaptor_type == EGA_ADAPTOR)
	{
		character_set = vs[screen].vga_charset;
		character_set = (character_set < 0) ? 0 :
			((character_set < totalfonts) ?
			 character_set :
			 totalfonts-1);

		line_height = vgacs[character_set].char_scanlines & 0x1F;
	}
	else if(adaptor_type == MDA_ADAPTOR)
	{
		line_height = 14;
	}
	else
	{
		line_height = 8;	/* CGA */
	}

	start = (data->start < 0) ? 0 :
		((data->start > line_height) ? line_height : data->start);

	if((vga_family == VGA_F_TRI) && (start == 0))
		start = 1;

	end = (data->end < 0) ? 0 :
		((data->end > line_height) ? line_height : data->end);

	vs[screen].cursor_start = start;
	vs[screen].cursor_end = end;

	if(screen == current_video_screen)
	{
		outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
		outb(addr_6845+1, start);
		outb(addr_6845,CRTC_CUREND);	/* cursor end reg */
		outb(addr_6845+1, end);
	}
}

/*---------------------------------------------------------------------------*
 *	ega/vga ioctl - set font attributes
 *---------------------------------------------------------------------------*/
static void
vgasetfontattr(struct vgafontattr *data)
{
	register int i;
	int vga_character_set;
	int lines_per_character;
	int totscanlines;
	int size;

	vga_character_set = data->character_set;
	vga_character_set = (vga_character_set < 0) ? 0 :
		((vga_character_set < totalfonts) ?
		vga_character_set : totalfonts-1);

	vgacs[vga_character_set].loaded = data->font_loaded;

	/* Limit Characters to 32 scanlines doubled */
	vgacs[vga_character_set].char_scanlines =
		(data->character_scanlines & 0x1F)
		| 0x40;	/* always set bit 9 of line cmp reg */

	if(adaptor_type == EGA_ADAPTOR)
		/* ...and screen height to scan 350 lines */
	        vgacs[vga_character_set].scr_scanlines =
		(data->screen_scanlines > 0x5d) ?
		0x5d : data->screen_scanlines;
	else
		/* ...and screen height to scan 480 lines */
	        vgacs[vga_character_set].scr_scanlines =
		(data->screen_scanlines > 0xdF) ?
		0xdF : data->screen_scanlines;

	lines_per_character =
		(int)(0x1F & vgacs[vga_character_set].char_scanlines)+1;

	totscanlines = 0x101 + (int)vgacs[vga_character_set].scr_scanlines;

	size = data->screen_size;

	if(adaptor_type == EGA_ADAPTOR)
	{
	        switch(size)
		{
			case SIZ_25ROWS: /* This case is always OK */
		    		break;

			case SIZ_35ROWS:
				if(totscanlines/lines_per_character >= 35)
		         		size = SIZ_35ROWS;
		    		else
		         		size = SIZ_25ROWS;
		    		break;

		  	case SIZ_43ROWS:
			default:
				if(totscanlines/lines_per_character >= 43)
					size = SIZ_43ROWS;
				else if(totscanlines/lines_per_character >= 35)
					size = SIZ_35ROWS;
				else
					size = SIZ_25ROWS;
				break;
		}
	}
	else
	{
	        switch(size)
		{
			case SIZ_25ROWS: /* This case is always OK */
		    		break;

			case SIZ_28ROWS:
		    		if(totscanlines/lines_per_character >= 28)
		         		size = SIZ_28ROWS;
		    		else
		         		size = SIZ_25ROWS;
				break;

			case SIZ_40ROWS:
				if(totscanlines/lines_per_character >= 40)
		         		size = SIZ_40ROWS;
		    		else if(totscanlines/lines_per_character >= 28)
		         		size = SIZ_28ROWS;
		    		else
		         		size = SIZ_25ROWS;
		    		break;

			case SIZ_50ROWS:
			default:
				if(totscanlines/lines_per_character >= 50)
					size = SIZ_50ROWS;
		    		else if(totscanlines/lines_per_character >= 40)
		         		size = SIZ_40ROWS;
		    		else if(totscanlines/lines_per_character >= 28)
		         		size = SIZ_28ROWS;
		    		else
		         		size = SIZ_25ROWS;
		    	break;
		}
	}

	vgacs[vga_character_set].screen_size = size;

	for (i = 0;i < PCVT_NSCREENS;i++)
	{
		if(vga_character_set == vs[i].vga_charset)
			set_charset(&(vs[i]),vga_character_set);
	}

#if !PCVT_USL_VT_COMPAT
	vgapage(current_video_screen);
#else
	switch_screen(current_video_screen, 0, 0);
#endif /* !PCVT_USL_VT_COMPAT */

}

/*---------------------------------------------------------------------------*
 *	ega/vga ioctl - get font attributes
 *---------------------------------------------------------------------------*/
static void
vgagetfontattr(struct vgafontattr *data)
{
	int vga_character_set;

	vga_character_set = data->character_set;
	vga_character_set = (vga_character_set < 0) ? 0 :
		((vga_character_set < (int)totalfonts) ?
		 vga_character_set :
		 (int)(totalfonts-1));

	data->character_set = (int)vga_character_set;

	data->font_loaded = (int)vgacs[vga_character_set].loaded;

	data->character_scanlines =
		(int)vgacs[vga_character_set].char_scanlines
		& 0x1f;		/* do not display the overflow bits */

	data->screen_scanlines = (int)vgacs[vga_character_set].scr_scanlines;

	data->screen_size = (int)vgacs[vga_character_set].screen_size;
}

/*---------------------------------------------------------------------------*
 *	ega/vga ioctl - load a character shape into character set
 *---------------------------------------------------------------------------*/
static void
vgaloadchar(struct vgaloadchar *data)
{
	int vga_character_set;
	int character;
	int lines_per_character;

	vga_character_set = data->character_set;
	vga_character_set = (vga_character_set < 0) ? 0 :
		((vga_character_set < (int)totalfonts) ?
		 vga_character_set : (int)(totalfonts-1));

	character = (data->character < 0) ? 0 :
		((data->character > 255) ? 255 : data->character);

	lines_per_character = (int)data->character_scanlines;
	lines_per_character = (lines_per_character < 0) ? 0 :
	        ((lines_per_character > 32) ? 32 : lines_per_character);

	loadchar(vga_character_set,character,lines_per_character,
		 data->char_table);
}

/*---------------------------------------------------------------------------*
 *	video ioctl - get screen information
 *---------------------------------------------------------------------------*/
static void
vid_getscreen(struct screeninfo *data, Dev_t dev)
{
	int device = minor(dev);
	data->adaptor_type = adaptor_type;	/* video adapter installed */
	data->monitor_type = color;		/* monitor type installed */
	data->totalfonts = totalfonts;		/* no of downloadble fonts */
	data->totalscreens = totalscreens;	/* no of virtual screens */
	data->screen_no = device;		/* this screen number */
	data->current_screen = current_video_screen; /* displayed screen no */
	/* screen size */
	data->screen_size = vgacs[(vs[device].vga_charset)].screen_size;
	/* pure VT mode or HP/VT mode */
	data->pure_vt_mode = vs[device].vt_pure_mode;
	data->vga_family = vga_family;		/* manufacturer, family */
	data->vga_type = vga_type;		/* detected chipset type */
	data->vga_132 = can_do_132col;		/* 132 column support */
	data->force_24lines = vs[device].force24; /* force 24 lines */
}

/*---------------------------------------------------------------------------*
 *	video ioctl - set screen information
 *---------------------------------------------------------------------------*/
static void
vid_setscreen(struct screeninfo *data, Dev_t dev)
{
	int screen;

	if(data->current_screen == -1)
	{
		screen = minor(dev);
	}
	else
	{
		if(data->current_screen >= PCVT_NSCREENS)
			return;					/* XXXXXX */
		screen = data->current_screen;
	}

	vgapage(screen);

#if defined XSERVER && PCVT_USL_VT_COMPAT
	{
		int x = spltty(), waitfor = screen + 1;
		/* if the vt is yet to be released by a process, wait here */
		if(vs[screen].vt_status & VT_WAIT_REL)
			(void)usl_vt_ioctl(dev, VT_WAITACTIVE,
					   (caddr_t)&waitfor, 0, 0);
		splx(x);
	}
	/* make sure the switch really happened */
	if(screen != current_video_screen)
		return;		/* XXX should say "EAGAIN" here */
#endif /* defined XSERVER && PCVT_USL_VT_COMPAT */

	if((data->screen_size != -1) || (data->force_24lines != -1))
	{
		if(data->screen_size == -1)
			data->screen_size =
				vgacs[(vs[screen].vga_charset)].screen_size;

		if(data->force_24lines != -1)
		{
			vs[screen].force24 = data->force_24lines;

			if(vs[screen].force24)
			{
				swritefkl(2,(u_char *)"FORCE24 ENABLE *",
					  &vs[screen]);
			}
			else
			{
				swritefkl(2,(u_char *)"FORCE24 ENABLE  ",
					  &vs[screen]);
			}
		}

		if((data->screen_size == SIZ_25ROWS) ||
		   (data->screen_size == SIZ_28ROWS) ||
		   (data->screen_size == SIZ_35ROWS) ||
		   (data->screen_size == SIZ_40ROWS) ||
		   (data->screen_size == SIZ_43ROWS) ||
		   (data->screen_size == SIZ_50ROWS))
		{
			if(data->screen_no == -1)
				set_screen_size(vsp, data->screen_size);
			else
				set_screen_size(&vs[minor(dev)],
						data->screen_size);
		}
	}

	if(data->pure_vt_mode != -1)
	{
		if((data->pure_vt_mode == M_HPVT) ||
		   (data->pure_vt_mode == M_PUREVT))
		{
			if(data->screen_no == -1)
				set_emulation_mode(vsp, data->pure_vt_mode);
			else
				set_emulation_mode(&vs[minor(dev)],
						   data->pure_vt_mode);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	set screen size/resolution for a virtual screen
 *---------------------------------------------------------------------------*/
void
set_screen_size(struct video_state *svsp, int size)
{
	int i;

	for(i = 0; i < totalfonts; i++)
	{
		if(vgacs[i].screen_size == size)
		{
			set_charset(svsp, i);
			clr_parms(svsp); 	/* escape parameter init */
			svsp->state = STATE_INIT; /* initial state */
			svsp->scrr_beg = 0;	/* start of scrolling region */
			svsp->sc_flag = 0;	/* invalidate saved cursor
						 * position */
			svsp->transparent = 0;	/* disable control code
						 * processing */

			/* Update tty to reflect screen size */

			if (svsp->vs_tty)
			{
				svsp->vs_tty->t_winsize.ws_col = svsp->maxcol;
				svsp->vs_tty->t_winsize.ws_xpixel =
					(svsp->maxcol == 80)? 720: 1056;
				svsp->vs_tty->t_winsize.ws_ypixel = 400;
				svsp->vs_tty->t_winsize.ws_row =
					svsp->screen_rows;
			}
			/* screen_rows already calculated in set_charset() */
			if(svsp->vt_pure_mode == M_HPVT && svsp->labels_on)
			{
				if(svsp->which_fkl == SYS_FKL)
					sw_sfkl(svsp);
				else if(svsp->which_fkl == USR_FKL)
					sw_ufkl(svsp);
			}


			svsp->scrr_len = svsp->screen_rows;
			svsp->scrr_end = svsp->scrr_len - 1;

#if PCVT_SIGWINCH
			if (svsp->vs_tty && svsp->vs_tty->t_pgrp)
				pgsignal(svsp->vs_tty->t_pgrp, SIGWINCH, 1);
#endif /* PCVT_SIGWINCH */

			reallocate_scrollbuffer(svsp, svsp->scrollback_pages);
			break;
		}
 	}
}

/*---------------------------------------------------------------------------*
 *	resize the scrollback buffer to the specified number of "pages"
 *---------------------------------------------------------------------------*/
void
reallocate_scrollbuffer(struct video_state *svsp, int pages)
{
	int s;
	u_short *stmp;
	
	if(pages < 2)
		pages = 2;
	if(pages > 50)
		pages = 50;

	s = splhigh();

	if((stmp = (u_short *)malloc(svsp->maxcol * svsp->screen_rows *
			pages * CHR, M_DEVBUF, M_NOWAIT)) == NULL)
	{
		splx(s);
		printf("pcvt: reallocate_scrollbuffer, malloc failed\n");
		return;
	}

	svsp->max_off = svsp->screen_rows * pages - 1;
	
	if(svsp->Scrollback)
	{
		bcopy(svsp->Scrollback, stmp,
			(min(pages, svsp->scrollback_pages)) *
			svsp->screen_rows * svsp->maxcol * CHR);
		free(svsp->Scrollback, M_DEVBUF);
		svsp->Scrollback = stmp;
	}
	else
	{
		svsp->scr_offset = 0;
		svsp->scrolling = 0;
		svsp->Scrollback = stmp;

		bcopy(svsp->Crtat, svsp->Scrollback,
			svsp->screen_rows * svsp->maxcol * CHR);

		svsp->scr_offset = svsp->row;
	}

	svsp->scrollback_pages = pages;

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	VGA ioctl - read DAC palette entry
 *---------------------------------------------------------------------------*/
static void
vgareadpel(struct vgapel *data, Dev_t dev)
{
	register unsigned vpage = minor(dev);
	register unsigned idx = data->idx;

	if(idx >= NVGAPEL)
		return;		/* no such entry */

	/* do not read VGA palette directly, use saved values */
	data->r = vs[vpage].palette[idx].r;
	data->g = vs[vpage].palette[idx].g;
	data->b = vs[vpage].palette[idx].b;
}

/*---------------------------------------------------------------------------*
 *	VGA ioctl - write DAC palette entry
 *---------------------------------------------------------------------------*/
static void
vgawritepel(struct vgapel *data, Dev_t dev)
{
	register unsigned vpage = minor(dev);
	register unsigned idx = data->idx;

	if(idx >= NVGAPEL)
		return;		/* no such entry */

	/* first, update saved values for this video screen */
	vs[vpage].palette[idx].r = data->r;
	vs[vpage].palette[idx].g = data->g;
	vs[vpage].palette[idx].b = data->b;

	/* if this happens on active screen, update VGA DAC, too */
	if(vpage == current_video_screen)
		vgapaletteio(idx, &vs[vpage].palette[idx], 1);
}

/*---------------------------------------------------------------------------*
 *	VGA physical IO - read/write one palette entry
 *---------------------------------------------------------------------------*/
void
vgapaletteio(unsigned idx, struct rgb *val, int writeit)
{

#if PCVT_PALFLICKER
	vga_screen_off();
#endif /* PCVT_PALFLICKER */

	if(writeit)
	{
		outb(VGA_DAC + 2, idx);

#if PCVT_WAITRETRACE
		wait_retrace();
#endif /* PCVT_WAITRETRACE */

		outb(VGA_DAC + 3, val->r & VGA_PMSK);

#if PCVT_WAITRETRACE
		wait_retrace();
#endif /* PCVT_WAITRETRACE */

		outb(VGA_DAC + 3, val->g & VGA_PMSK);

#if PCVT_WAITRETRACE
		wait_retrace();
#endif /* PCVT_WAITRETRACE */

		outb(VGA_DAC + 3, val->b & VGA_PMSK);
	}
	else	/* read it */
	{
		outb(VGA_DAC + 1, idx);

#if PCVT_WAITRETRACE
		wait_retrace();
#endif /* PCVT_WAITRETRACE */

		val->r = inb(VGA_DAC + 3) & VGA_PMSK;

#if PCVT_WAITRETRACE
		wait_retrace();
#endif /* PCVT_WAITRETRACE */

		val->g = inb(VGA_DAC + 3) & VGA_PMSK;

#if PCVT_WAITRETRACE
		wait_retrace();
#endif /* PCVT_WAITRETRACE */

		val->b = inb(VGA_DAC + 3) & VGA_PMSK;
	}

#if PCVT_PALFLICKER
	vga_screen_on();
#endif /* PCVT_PALFLICKER */

}

/*---------------------------------------------------------------------------*
 *
 *	update asynchronous: cursor, cursor pos displ, sys load, keyb scan
 *
 *	arg is:
 *		UPDATE_START = do update; requeue
 *		UPDATE_STOP  = suspend updates
 *		UPDATE_KERN  = do update for kernel printfs
 *
 *---------------------------------------------------------------------------*/
void
async_update(void *arg)
{
	static int lastpos = 0;
	static int counter = PCVT_UPDATESLOW;

#ifdef XSERVER
	/* need a method to suspend the updates */

	if(arg == UPDATE_STOP)
	{
		untimeout(async_update, UPDATE_START, async_update_ch);
		return;
	}
#endif /* XSERVER */

	/* first check if update is possible */

	if(chargen_access		/* does no-one load characters? */
#ifdef XSERVER				/* is vt0 not in graphics mode? */
#if !PCVT_USL_VT_COMPAT
	   || pcvt_xmode		/* XXX necessary ????? */
#endif /* PCVT_USL_VT_COMPAT */
#endif /* XSERVER */
	   )
	{
		goto async_update_exit;	/* do not update anything */
	}

#if PCVT_SCREENSAVER
	if(reset_screen_saver && (counter == PCVT_UPDATESLOW))
	{
		pcvt_scrnsv_reset();	/* yes, do it */
		reset_screen_saver = 0;	/* re-init */
	}
	else if(scrnsv_active)		/* is the screen not blanked? */
	{
		goto async_update_exit;	/* do not update anything */
	}
#endif /* PCVT_SCREENSAVER */

	/*-------------------------------------------------------------------*/
	/* this takes place on EVERY virtual screen (if not in X mode etc...)*/
	/*-------------------------------------------------------------------*/

	if ( cursor_pos_valid &&
	    (lastpos != (vsp->Crtat + vsp->cur_offset - Crtat)))
	{
		lastpos = vsp->Crtat + vsp->cur_offset - Crtat;
	 	outb(addr_6845, CRTC_CURSORH);	/* high register */
		outb(addr_6845+1, ((lastpos) >> 8));
		outb(addr_6845, CRTC_CURSORL);	/* low register */
		outb(addr_6845+1, (lastpos));
	}

	if (arg == UPDATE_KERN)		/* Magic arg: for kernel printfs */
		return;

	if(--counter)			/* below is possible update */
		goto async_update_exit;	/*  just now and then ..... */
	counter = PCVT_UPDATESLOW;	/* caution, see screensaver above !! */

	/*-------------------------------------------------------------------*/
	/* this takes place ONLY on screen 0 if in HP mode, labels on, !X    */
	/*-------------------------------------------------------------------*/

	/* additional processing for HP necessary ? */

	if((vs[0].vt_pure_mode == M_HPVT) && (vs[0].labels_on))
	{
		static volatile u_char buffer[] =
		       "System Load: 1min: 0.00 5min: 0.00 15min: 0.00";
		register int tmp, i;
#if PCVT_SHOWKEYS
		extern u_char rawkeybuf[80];

		if(keyboard_show)
		{
			for(i = 0; i < 80; i++)
			{
				*((vs[0].Crtat+((vs[0].screen_rows+2)
					* vs[0].maxcol))+i) =
				 user_attr | rawkeybuf[i];
			}
		}
		else
		{
#endif	/* PCVT_SHOWKEYS */

		/* display load averages in last line (taken from tty.c) */
			i = 18;
#ifdef NEW_AVERUNNABLE
	 		tmp = (averunnable.ldavg[0] * 100 + FSCALE / 2)
				>> FSHIFT;
#else
			tmp = (averunnable[0] * 100 + FSCALE / 2) >> FSHIFT;
#endif

			buffer[i++] =
				((((tmp/100)/10) == 0) ?
				 ' ' :
				 ((tmp/100)/10) + '0');
			buffer[i++] = ((tmp/100)%10) + '0';
			buffer[i++] = '.';
			buffer[i++] = ((tmp%100)/10) + '0';
			buffer[i++] = ((tmp%100)%10) + '0';
			i += 6;
#ifdef NEW_AVERUNNABLE
	 		tmp = (averunnable.ldavg[1] * 100 + FSCALE / 2)
				>> FSHIFT;
#else
			tmp = (averunnable[1] * 100 + FSCALE / 2) >> FSHIFT;
#endif
			buffer[i++] = ((((tmp/100)/10) == 0) ?
				       ' ' :
				       ((tmp/100)/10) + '0');
			buffer[i++] = ((tmp/100)%10) + '0';
			buffer[i++] = '.';
			buffer[i++] = ((tmp%100)/10) + '0';
			buffer[i++] = ((tmp%100)%10) + '0';
			i += 7;
#ifdef NEW_AVERUNNABLE
	 		tmp = (averunnable.ldavg[2] * 100 + FSCALE / 2)
				>> FSHIFT;
#else
			tmp = (averunnable[2] * 100 + FSCALE / 2) >> FSHIFT;
#endif
			buffer[i++] = ((((tmp/100)/10) == 0) ?
				       ' ' :
				       ((tmp/100)/10) + '0');
			buffer[i++] = ((tmp/100)%10) + '0';
			buffer[i++] = '.';
			buffer[i++] = ((tmp%100)/10) + '0';
			buffer[i++] = ((tmp%100)%10) + '0';
			buffer[i] = '\0';

			for(i = 0; buffer[i]; i++)
			{
				*((vs[0].Crtat +
				   ((vs[0].screen_rows + 2) * vs[0].maxcol)
				   ) + i
				  ) = user_attr | buffer[i];
			}

#if PCVT_SHOWKEYS
			for(; i < 77; i++)
			{
				*((vs[0].Crtat +
				   ((vs[0].screen_rows + 2) * vs[0].maxcol)
				   ) + i
				  ) = user_attr | ' ';
			}

		}
#endif	/* PCVT_SHOWKEYS */
	}

	/*-------------------------------------------------------------------*/
	/* this takes place on EVERY screen which is in HP mode, labels on,!X*/
	/*-------------------------------------------------------------------*/

	if((vsp->vt_pure_mode == M_HPVT) && (vsp->labels_on))
	{
		register int col = vsp->col+1;
		register u_short *p = vsp->Crtat +
				(vsp->screen_rows * vsp->maxcol);

		/* update column display between labels */

		if(vsp->maxcol == SCR_COL132)
		{
			p += (SCR_COL132 - SCR_COL80)/2;

			if(col >= 100)
			{
				*(p + LABEL_COLU) = user_attr | '1';
				col -= 100;
			}
			else
			{
				*(p + LABEL_COLU) = user_attr | '0';
			}
		}
		*(p + LABEL_COLH) = user_attr | ((col/10) + '0');
		*(p + LABEL_COLL) = user_attr | ((col%10) + '0');

		/* update row display between labels */

		*(p + LABEL_ROWH) = (user_attr | (((vsp->row+1)/10) + '0'));
		*(p + LABEL_ROWL) = (user_attr | (((vsp->row+1)%10) + '0'));
	}

async_update_exit:

 	if(arg == UPDATE_START)
	{
	   async_update_ch = timeout(async_update, UPDATE_START,
				     PCVT_UPDATEFAST);
	}
}

/*---------------------------------------------------------------------------*
 *	set character set for virtual screen
 *---------------------------------------------------------------------------*/
void
set_charset(struct video_state *svsp, int curvgacs)
{
	static int sizetab[] = { 25, 28, 35, 40, 43, 50 };
	int oldsize, oldrows, newsize, newrows;

	if((curvgacs < 0) || (curvgacs > (NVGAFONTS-1)))
		return;

	svsp->vga_charset = curvgacs;

	select_vga_charset(curvgacs);

	oldsize = svsp->screen_rowsize;
	oldrows = svsp->screen_rows;
	newsize = sizetab[(vgacs[curvgacs].screen_size)];
	newrows = newsize;
	if (svsp->vt_pure_mode == M_HPVT)
		newrows -= 3;
	if (newrows == 25 && svsp->force24)
		newrows = 24;
	if (newrows < oldrows) {
		int nscroll = svsp->row + 1 - newrows;

		if (svsp->row >= oldrows) /* Sanity check */
			nscroll = oldrows - newrows;
		if (nscroll > 0) {
			/* Scroll up */
			bcopy (svsp->Crtat + nscroll * svsp->maxcol,
			       svsp->Crtat,
			       newrows * svsp->maxcol * CHR);
			svsp->row -= nscroll;
			svsp->cur_offset -= nscroll * svsp->maxcol;
		}
		if (newrows < newsize)
			fillw(user_attr | ' ',
			      svsp->Crtat + newrows * svsp->maxcol,
			      (newsize - newrows) * svsp->maxcol);
	} else if (oldrows < newsize)
		fillw(user_attr | ' ',
		      svsp->Crtat + oldrows * svsp->maxcol,
		      (newsize - oldrows) * svsp->maxcol);

	svsp->screen_rowsize = newsize;
	svsp->screen_rows = newrows;

	/* Clip scrolling region */
	if(svsp->scrr_end > svsp->screen_rows - 1)
		svsp->scrr_end = svsp->screen_rows - 1;
	svsp->scrr_len = svsp->scrr_end - svsp->scrr_beg + 1;

	/* Clip cursor pos */

	if(svsp->cur_offset > (svsp->scrr_len * svsp->maxcol))
		svsp->cur_offset = (svsp->scrr_len * svsp->maxcol) + svsp->col;
}

/*---------------------------------------------------------------------------*
 *	select a vga character set
 *---------------------------------------------------------------------------*/
void
select_vga_charset(int vga_charset)
{
	int first, second;
	int fflag = 0;
	int sflag = 0;
	u_char cmap = 0;

	static u_char cmaptaba[] =
		{0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13};

	static u_char cmaptabb[] =
		{0x00, 0x04, 0x08, 0x0c, 0x20, 0x24, 0x28, 0x2c};

 	if((adaptor_type != EGA_ADAPTOR) && (adaptor_type != VGA_ADAPTOR))
 		return;

	if((vga_charset < 0) || (vga_charset >= totalfonts))
		return;

	if(!vgacs[vga_charset].loaded)
		return;

	/*--------------------------------------------------------------
	   find the the first and second charset of a given resolution.
	   the first is used for lower 256 and the second (if any) is
	   used for the upper 256 entries of a complete 512 entry ega/
	   vga charset.
	--------------------------------------------------------------*/

	for(first = 0; first < totalfonts; first++)
	{
		if(!vgacs[first].loaded)
			continue;
		if(vgacs[first].screen_size != vgacs[vga_charset].screen_size)
			continue;
		if(vgacs[first].char_scanlines !=
		   vgacs[vga_charset].char_scanlines)
			continue;
		if(vgacs[first].scr_scanlines !=
		   vgacs[vga_charset].scr_scanlines)
			continue;
		fflag = 1;
		break;
	}

	if(fflag != 1)
		return;

	for(second = first+1; second < totalfonts; second++)
	{
		if(!vgacs[second].loaded)
			continue;
		if(vgacs[second].screen_size != vgacs[vga_charset].screen_size)
			continue;
		if(vgacs[second].char_scanlines !=
		   vgacs[vga_charset].char_scanlines)
			continue;
		if(vgacs[second].scr_scanlines !=
		   vgacs[vga_charset].scr_scanlines)
			continue;
		sflag = 1;
		break;
	}

	cmap = cmaptaba[first];
	if(sflag)
	{
		cmap |= cmaptabb[second];
		vgacs[first].secondloaded = second;
	}
	else
	{
		vgacs[first].secondloaded = 0; /*cs 0 can never become a 2nd!*/
	}

	if(vsp->wd132col)
	{
		cmap = (vga_charset & 0x07);
		cmap |= 0x10;
	}

	outb(TS_INDEX, TS_FONTSEL);	/* character map select register */
	outb(TS_DATA, cmap);		/* new char map */

	outb(addr_6845, CRTC_MAXROW);	/* max scan line reg */
	outb(addr_6845+1,
		vgacs[first].char_scanlines); /* scanlines/char */

	outb(addr_6845, CRTC_VDE);	/* vert display enable end */
	outb(addr_6845+1,
		vgacs[first].scr_scanlines);  /* low byte of scr scanlines */

	if((color == 0) && (adaptor_type == VGA_ADAPTOR))
	{
		outb(addr_6845, CRTC_ULOC);	/* underline location reg */
		outb(addr_6845+1, (vgacs[first].char_scanlines & 0x1F));
	}
}

/*---------------------------------------------------------------------------*
 *	switch vga-card to load a character set
 *---------------------------------------------------------------------------*/
static void
setchargen(void)
{
	chargen_access = 1;	/* flag we are accessing the chargen ram */

	/* program sequencer to access character generator */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);	/* synchronous reset */

	outb(TS_INDEX, TS_WRPLMASK);
	outb(TS_DATA, 0x04);	/* write to map 2 */

	outb(TS_INDEX, TS_MEMMODE);
	outb(TS_DATA, 0x07);	/* sequential addressing */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);	/* clear synchronous reset */

	/* program graphics controller to access character generator */

	outb(GDC_INDEX, GDC_RDPLANESEL);
	outb(GDC_DATA, 0x02);	/* select map 2 for cpu reads */

	outb(GDC_INDEX, GDC_MODE);
	outb(GDC_DATA, 0x00);	/* disable odd-even addressing */

	outb(GDC_INDEX, GDC_MISC);
	outb(GDC_DATA, 0x00);	/* map starts at 0xA000 */
}

/*---------------------------------------------------------------------------*
 *	switch vga-card to load a character set to plane 3
 *---------------------------------------------------------------------------*/
static void
setchargen3(void)
{
	chargen_access = 1;	/* flag we are accessing the chargen ram */

	/* program sequencer to access character generator */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);	/* synchronous reset */

	outb(TS_INDEX, TS_WRPLMASK);
	outb(TS_DATA, 0x08);	/* write to map 3 */

	outb(TS_INDEX, TS_MEMMODE);
	outb(TS_DATA, 0x07);	/* sequential addressing */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);	/* clear synchronous reset */

	/* program graphics controller to access character generator */

	outb(GDC_INDEX, GDC_RDPLANESEL);
	outb(GDC_DATA, 0x03);	/* select map 3 for cpu reads */

	outb(GDC_INDEX, GDC_MODE);
	outb(GDC_DATA, 0x00);	/* disable odd-even addressing */

	outb(GDC_INDEX, GDC_MISC);
	outb(GDC_DATA, 0x00);	/* map starts at 0xA000 */
}

/*---------------------------------------------------------------------------*
 *	switch back vga-card to normal operation
 *---------------------------------------------------------------------------*/
static void
resetchargen(void)
{
	/* program sequencer to access video ram */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);	/* synchronous reset */

	outb(TS_INDEX, TS_WRPLMASK);
	outb(TS_DATA, 0x03);	/* write to map 0 & 1 */

	outb(TS_INDEX, TS_MEMMODE);
	outb(TS_DATA, 0x03);	/* odd-even addressing */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);	/* clear synchronous reset */

	/* program graphics controller to access character generator */

	outb(GDC_INDEX, GDC_RDPLANESEL);
	outb(GDC_DATA, 0x00);	/* select map 0 for cpu reads */

	outb(GDC_INDEX, GDC_MODE);
	outb(GDC_DATA, 0x10);	/* enable odd-even addressing */

	outb(GDC_INDEX, GDC_MISC);
	if(color)
		outb(GDC_DATA, 0x0e);	/* map starts at 0xb800 */
	else
		outb(GDC_DATA, 0x0a);	/* map starts at 0xb000 */

	chargen_access = 0;	/* flag we are NOT accessing the chargen ram */
}

#if PCVT_WAITRETRACE
/*---------------------------------------------------------------------------*
 *	wait for being in a retrace time window
 *	NOTE: this is __VERY__ bad programming practice in this environment !!
 *---------------------------------------------------------------------------*/

static void
wait_retrace(void)
{
	if(color)
	{
		while(!(inb(GN_INPSTAT1C) & 0x01))
			;
	}
	else
	{
		while(!(inb(GN_INPSTAT1M) & 0x01))
			;
	}
}

#endif /* PCVT_WAITRETRACE */

/*---------------------------------------------------------------------------*
 *	switch screen off (VGA only)
 *---------------------------------------------------------------------------*/
void
vga_screen_off(void)
{
	unsigned char old;

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);		/* synchronous reset */

	outb(TS_INDEX, TS_MODE);	/* clocking mode reg */
	old = inb(TS_DATA);		/* get current value */

	outb(TS_INDEX, TS_MODE);	/* clocking mode reg */
	outb(TS_DATA, (old | 0x20));	/* screen off bit on */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);		/* clear synchronous reset */
}

/*---------------------------------------------------------------------------*
 *	switch screen back on (VGA only)
 *---------------------------------------------------------------------------*/
void
vga_screen_on(void)
{
	unsigned char old;

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);		/* synchronous reset */

	outb(TS_INDEX, TS_MODE);	/* clocking mode reg */
	old = inb(TS_DATA);		/* get current value */

	outb(TS_INDEX, TS_MODE);	/* clocking mode reg */
	outb(TS_DATA, (old & ~0x20));	/* screen off bit off */

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);		/* clear synchronous reset */
}

/*---------------------------------------------------------------------------*
 *	compute character set base address (in kernel map)
 *---------------------------------------------------------------------------*/
static unsigned char *
compute_charset_base(unsigned fontset)
{
	unsigned char *d = (unsigned char *)Crtat;

	static int charset_offset[8] = { 0x0000, 0x4000, 0x8000, 0xC000,
					 0x2000, 0x6000, 0xA000, 0xE000 };

	static int charsetw_offset[8] = { 0x0000, 0x2000, 0x4000, 0x6000,
					  0x8000, 0xA000, 0xC000, 0xE000 };

	switch(adaptor_type)
	{
		case EGA_ADAPTOR:
			fontset = (fontset > 3) ? 3 : fontset;
			break;

		case VGA_ADAPTOR:
			fontset = (fontset > 7) ? 7 : fontset;
			break;

		default:
			return 0;
	}

	if(color)
		d -= (0xB8000 - 0xA0000);	/* Point to 0xA0000 */
	else
		d -= (0xB0000 - 0xA0000);	/* Point to 0xA0000 */

	if(vsp->wd132col)
		d += charsetw_offset[fontset];	/* Load into Character set n */
	else
		d += charset_offset[fontset];	/* Load into Character set n */

	return d;
}

/*---------------------------------------------------------------------------*
 *	load a char into ega/vga character generator ram
 *---------------------------------------------------------------------------*/
void
loadchar(int fontset, int character, int char_scanlines, u_char *char_table)
{
	unsigned char *d;

#if PCVT_BACKUP_FONTS
	unsigned char *bak;
#endif /* PCVT_BACKUP_FONTS */

	int j, k;

	if((d = compute_charset_base(fontset)) == 0)
		return;

	d += (character * 32);		/* 32 bytes per character */

	if(vsp->wd132col &&
	   (fontset == 1||fontset == 3||fontset == 5||fontset == 7))
		setchargen3();			/* access chargen ram */
	else
		setchargen();			/* access chargen ram */

	for(j = k = 0; j < char_scanlines; j++) /* x bit high characters */
	{
		*d = char_table[k];
		d++;
		k++;
	}
	for(; j < 32; j++)		/* Up to 32 bytes per character image*/
	{
		*d = 0x00;
		d++;
	}

	resetchargen();			/* access video ram */

#if PCVT_BACKUP_FONTS
	if(saved_charsets[fontset] == 0)
		saved_charsets[fontset] =
			(u_char *)malloc(32 * 256, M_DEVBUF, M_WAITOK);

	if((bak = saved_charsets[fontset]))
	{
		/* make a backup copy of this char */
		bak += (character * 32);
		bzero(bak, 32);
		bcopy(char_table, bak, char_scanlines);
	}
#ifdef DIAGNOSTIC
	else
		panic("pcvt loadchar: no backup buffer");
#endif /* DIAGNOSTIC */

#endif /* PCVT_BACKUP_FONTS */

}

/*---------------------------------------------------------------------------*
 *	save/restore character set n to addr b
 *---------------------------------------------------------------------------*/
#if !PCVT_BACKUP_FONTS

void
vga_move_charset(unsigned n, unsigned char *b, int save_it)
{
	unsigned char *d = compute_charset_base(n);

#ifdef DIAGNOSTIC
	if(d == 0)
		panic("vga_move_charset: wrong adaptor");
#endif

	if(vsp->wd132col && (n == 1||n == 3||n == 5||n == 7))
	{
		setchargen3();
		d -= 0x2000;
	}
	else
	{
		setchargen();
	}

	/* PLEASE, leave the following alone using bcopyb, as several	*/
	/* chipsets have problems if their memory is accessed with 32	*/
	/* or 16 bits wide, don't change this to using bcopy for speed!	*/

	if(save_it)
		bcopyb(d, b, 256 /* chars */ * 32 /* bytes per char */);
	else
		bcopyb(b, d, 256 /* chars */ * 32 /* bytes per char */);

	resetchargen();
}

#else /* PCVT_BACKUP_FONTS */

/* since there are always backed up copies, we do not save anything here */
/* parameter "b" is totally ignored */

void
vga_move_charset(unsigned n, unsigned char *b, int save_it)
{
	unsigned char *d = compute_charset_base(n);

	if(save_it)
		return;

	if(saved_charsets[n] == 0)
#ifdef DIAGNOSTIC
		panic("pcvt: restoring unbuffered charset");
#else
		return;
#endif

#ifdef DIAGNOSTIC
	if(d == 0)
		panic("vga_move_charset: wrong adaptor");
#endif

	if(vsp->wd132col && (n == 1||n == 3||n == 5||n == 7))
	{
		setchargen3();
		d -= 0x2000;
	}
	else
	{
		setchargen();
	}

	/* PLEASE, leave the following alone using bcopyb, as several	*/
	/* chipsets have problems if their memory is accessed with 32	*/
	/* or 16 bits wide, don't change this to using bcopy for speed!	*/

	bcopyb(saved_charsets[n], d,
	       256 /* chars */ * 32 /* bytes per char */);

	resetchargen();
}

#endif /* PCVT_BACKUP_FONTS */


#if !PCVT_USL_VT_COMPAT
/*---------------------------------------------------------------------------*
 *	switch to virtual screen n (0 ... PCVT_NSCREENS-1)
 *---------------------------------------------------------------------------*/
void
vgapage(int n)
{

#if !PCVT_KBD_FIFO
	int x;
#endif	/* !PCVT_KBD_FIFO */

	int cols = vsp->maxcol;		/* get current col val */

	if(n < 0 || n >= totalscreens)
		return;

#if !PCVT_KBD_FIFO
	x = spltty();			/* protect us */
#endif	/* !PCVT_KBD_FIFO */

	/* video board memory -> kernel memory */

	bcopy(vsp->Crtat, vsp->Memory, vsp->screen_rows * vsp->maxcol * CHR);

	vsp->Crtat = vsp->Memory;	/* operate in memory now */

	/* update global screen pointers/variables */

	current_video_screen = n;	/* current screen no */

#if !PCVT_NETBSD && !(PCVT_FREEBSD > 110 && PCVT_FREEBSD < 200)
	pcconsp = &pccons[n];		/* current tty */
#elif PCVT_FREEBSD > 110 && PCVT_FREEBSD < 200
	pcconsp = pccons[n];		/* current tty */
#else
	pcconsp = pc_tty[n];		/* current tty */
#endif

	vsp = &vs[n];			/* current video state ptr */

	/* kernel memory -> video board memory */

	bcopy(vsp->Crtat, Crtat, vsp->screen_rows * vsp->maxcol * CHR);

	vsp->Crtat = Crtat;		/* operate on screen now */

	outb(addr_6845, CRTC_STARTADRH);
	outb(addr_6845+1, 0);
	outb(addr_6845, CRTC_STARTADRL);
	outb(addr_6845+1, 0);

#if !PCVT_KBD_FIFO
	splx(x);
#endif	/* !PCVT_KBD_FIFO */

	select_vga_charset(vsp->vga_charset);

	if(vsp->maxcol != cols)
		vga_col(vsp, vsp->maxcol);	/* select 80/132 columns */

 	outb(addr_6845, CRTC_CURSORH);	/* select high register */
	outb(addr_6845+1, vsp->cur_offset >> 8);
	outb(addr_6845, CRTC_CURSORL);	/* select low register */
	outb(addr_6845+1, vsp->cur_offset);

	if(vsp->cursor_on)
	{
		outb(addr_6845, CRTC_CURSTART);	/* select high register */
		outb(addr_6845+1, vsp->cursor_start);
		outb(addr_6845, CRTC_CUREND);	/* select low register */
		outb(addr_6845+1, vsp->cursor_end);
	}
	else
	{
		sw_cursor(0);
	}

	if(adaptor_type == VGA_ADAPTOR)
	{
		unsigned i;

		/* switch VGA DAC palette entries */

		for(i = 0; i < NVGAPEL; i++)
			vgapaletteio(i, &vsp->palette[i], 1);
	}

	update_led();			/* update led's */

	update_hp(vsp);			/* update fkey labels, if present */
}
#endif /* !PCVT_USL_VT_COMPAT */

/*---------------------------------------------------------------------------*
 *	test if it is a vga
 *---------------------------------------------------------------------------*/

int
vga_test(void)
{
	u_char old, new, check;

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	old = inb(addr_6845+1);		/* get current value */

	new = old | CURSOR_ON_BIT;	/* set cursor on by setting bit 5 on */

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	outb(addr_6845+1,new);		/* cursor should be on now */

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	check = inb(addr_6845+1);	/* get current value */

	if(check != new)
	{
		outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
		outb(addr_6845+1,old);		/* failsafe */
		return(0);			/* must be ega */
	}

	new = old & ~CURSOR_ON_BIT;	/* turn cursor off by clearing bit 5 */

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	outb(addr_6845+1,new);		/* cursor should be off now */

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	check = inb(addr_6845+1);	/* get current value */

	if(check != new)
	{
		outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
		outb(addr_6845+1,old);		/* failsafe */
		return(0);			/* must be ega */
	}

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	outb(addr_6845+1,old);		/* failsafe */

        return(1);	/* vga */
}

/*---------------------------------------------------------------------------*
 *	convert upper/lower sixel font array to vga font array
 *---------------------------------------------------------------------------*/
void
sixel_vga(struct sixels *sixelp, u_char *vgachar)
{
	register int i, j;
	register int shift;
	register u_char mask;

	for(j = 0; j < 16; j++)
		vgachar[j] = 0;

	mask = 0x01;
	for(j = 0; j < 6; j++)
	{
		for(i = 0, shift = 7; i < 8; i++, shift--)
			vgachar[j] |= ((((sixelp->upper[i]) & mask) >> j)
				       << shift);
		mask <<= 1;
	}

	mask = 0x01;
	for(j = 0; j < 4; j++)
	{
		for(i = 0, shift = 7; i < 8; i++, shift--)
			vgachar[j+6] |= ((((sixelp->lower[i]) & mask) >>j)
					 << shift);
		mask <<= 1;
	}
}

/*---------------------------------------------------------------------------*
 *	Expand 8x10 EGA/VGA characters to 8x16 EGA/VGA characters
 *---------------------------------------------------------------------------*/
void
vga10_vga16(u_char *invga, u_char *outvga)
{
	register int i,j;

	/*
	 * Keep the top and bottom scanlines the same and double every scan
	 * line in between.
	 */

	outvga[0] = invga[0];
	outvga[1] = invga[1];
	outvga[14] = invga[8];
	outvga[15] = invga[9];

	for(i = j = 2;i < 8 && j < 14;i++,j += 2)
	{
		outvga[j]   = invga[i];
		outvga[j+1] = invga[i];
	}
}

/*---------------------------------------------------------------------------*
 *	Expand 8x10 EGA/VGA characters to 8x14 EGA/VGA characters
 *---------------------------------------------------------------------------*/
void
vga10_vga14(u_char *invga, u_char *outvga)
{
	register int i;

	/*
	 * Double the top two and bottom two scanlines and copy everything
	 * in between.
	 */

	outvga[0] = invga[0];
	outvga[1] = invga[0];
	outvga[2] = invga[1];
	outvga[3] = invga[1];
	outvga[10] = invga[8];
	outvga[11] = invga[8];
	outvga[12] = invga[9];
	outvga[13] = invga[9];

	for(i = 2;i < 8;i++)
		outvga[i+2]   = invga[i];
}

/*---------------------------------------------------------------------------*
 *	Expand 8x10 EGA/VGA characters to 8x10 EGA/VGA characters
 *---------------------------------------------------------------------------*/
void
vga10_vga10(u_char *invga, u_char *outvga)
{
	register int i;

	for(i = 0;i < 10;i++)
		outvga[i]   = invga[i];
}

/*---------------------------------------------------------------------------*
 *	Contract 8x10 EGA/VGA characters to 8x8 EGA/VGA characters
 *---------------------------------------------------------------------------*/
void
vga10_vga8(u_char *invga, u_char *outvga)
{
	/* Skip scanlines 3 and 7 */

	outvga[0] = invga[0];
	outvga[1] = invga[1];
	outvga[2] = invga[2];
	outvga[3] = invga[4];
	outvga[4] = invga[5];
	outvga[5] = invga[6];
	outvga[6] = invga[8];
	outvga[7] = invga[9];
}

/*---------------------------------------------------------------------------*
 *	force a vga card to behave like an ega for debugging
 *---------------------------------------------------------------------------*/
#if FORCE_EGA
void
force_ega(void)
{
	unsigned char vgareg;

	if(adaptor_type == VGA_ADAPTOR)
	{
		adaptor_type = EGA_ADAPTOR;
		totalfonts = 4;
		vgareg = inb(GN_MISCOUTR); /* Miscellaneous Output Register */
		vgareg |= 128;		   /* Set 350 scanline mode */
		vgareg &= ~64;
		outb(GN_MISCOUTW,vgareg);
	}
}
#endif /* FORCE_EGA */

/*---------------------------------------------------------------------------*
 *	disconnect attribute bit 3 from generating intensity
 *	(and use it for a second character set !)
 *---------------------------------------------------------------------------*/
void
set_2ndcharset(void)
{
	if(color)			/* prepare to access index register! */
		inb(GN_INPSTAT1C);
	else
		inb(GN_INPSTAT1M);

	/* select color plane enable reg, caution: set ATC access bit ! */

	outb(ATC_INDEX, (ATC_COLPLEN | ATC_ACCESS));
	outb(ATC_DATAW, 0x07);		/* disable plane 3 */
}

#if PCVT_SCREENSAVER
#if PCVT_PRETTYSCRNS

/*---------------------------------------------------------------------------*
 * produce some kinda random number, had a look into the system library...
 *---------------------------------------------------------------------------*/
static u_short
getrand(void)
{
#if !PCVT_FREEBSD
	extern struct timeval time; /* time-of-day register */
#endif
	static unsigned long seed = 1;
	register u_short res = (u_short)seed;
	seed = seed * 1103515245L + time_second;
	return res;
}

/*---------------------------------------------------------------------------*
 *	produce "nice" screensaving ....
 *---------------------------------------------------------------------------*/
static void
scrnsv_blink(void * arg)
{
	static struct rgb blink_rgb[8] =
	{
		{63, 63, 63},	/* white */
		{0, 63, 42},	/* pale green */
		{63, 63, 0},	/* yellow */
		{63, 21, 63},	/* violet */
		{42, 63, 0},	/* yellow-green */
		{63, 42, 0},	/* amber */
		{63, 42, 42},	/* rose */
		{21, 42, 42}	/* cyan */
	};
	register u_short r = getrand();
	unsigned pos = (r % (scrnsv_size / 2));

	*scrnsv_current = /* (0 << 8) + */ ' ';
	scrnsv_current = vsp->Crtat + pos;
	*scrnsv_current = (7 /* LIGHTGRAY */ << 8) + '*';
	if(adaptor_type == VGA_ADAPTOR)
		vgapaletteio(7 /* LIGHTGRAY */, &blink_rgb[(r >> 4) & 7], 1);
	scrnsv_blink_ch = timeout(scrnsv_blink, NULL, hz);
}

#endif /* PCVT_PRETTYSCRNS */

/*---------------------------------------------------------------------------*
 *	set timeout time
 *---------------------------------------------------------------------------*/
#ifndef XSERVER
static void
pcvt_set_scrnsv_tmo(int timeout)
#else
void
pcvt_set_scrnsv_tmo(int timeout)
#endif /* XSERVER */
{
	int x = splhigh();

	if(scrnsv_timeout)
		untimeout(scrnsv_timedout, NULL, scrnsv_timeout_ch);

	scrnsv_timeout = timeout;
	pcvt_scrnsv_reset();		/* sanity */
	splx(x);
	if(timeout == 0 && savedscreen)
	{
		/* release buffer when screen saver turned off */
		free(savedscreen, M_TEMP);
		savedscreen = (u_short *)0;
	}
}

/*---------------------------------------------------------------------------*
 *	we were timed out
 *---------------------------------------------------------------------------*/
static void
scrnsv_timedout(void *arg)
{
	/* this function is called by timeout() */
	/* raise priority to avoid conflicts with kbd intr */
	int x = spltty();

	/*
	 * due to some undefined problems with video adaptor RAM
	 * access timing, the following has been splitted into
	 * two pieces called subsequently with a time difference
	 * of 100 millisec
	 */

	if(++scrnsv_active == 1)
	{
		register size_t s;
		/*
		 * first, allocate a buffer
		 * do only if none allocated yet or another size required
		 * this reduces malloc() overhead by avoiding successive
		 * calls to malloc() and free() if they would have requested
		 * the same buffer
		 *
		 * XXX This is inherited from old days where no buffering
		 * happened at all. Meanwhile we should use the standard
		 * screen buffer instead. Any volunteers? :-) [At least,
		 * this code proved to work...]
		 */

		s = sizeof(u_short) * vsp->screen_rowsize * vsp->maxcol;

		if(savedscreen == (u_short *)0 || s != scrnsv_size)
		{
			/* really need to allocate */
			if(savedscreen)
				free(savedscreen, M_TEMP);
			scrnsv_size = s;
			if((savedscreen =
			    (u_short *)malloc(s, M_TEMP, M_NOWAIT))
			   == (u_short *)0)
			{
				/*
				 * didn't get the buffer memory,
				 * turn off screen saver
				 */
				scrnsv_timeout = scrnsv_active = 0;
				splx(x);
				return;
			}
		}
		/* save current screen */
		bcopy(vsp->Crtat, savedscreen, scrnsv_size);

		/* on VGA's, make sure palette is set to blank screen */
		if(adaptor_type == VGA_ADAPTOR)
		{
			struct rgb black = {0, 0, 0};
			vgapaletteio(0 /* BLACK */, &black, 1);
		}
		/* prepare for next time... */
		scrnsv_timeout_ch = timeout(scrnsv_timedout, NULL, hz / 10);
	}
	else
	{
		/* second call, now blank the screen */
		/* fill screen with blanks */
		fillw(/* (BLACK<<8) + */ ' ', vsp->Crtat, scrnsv_size / 2);

#if PCVT_PRETTYSCRNS
		scrnsv_current = vsp->Crtat;
		scrnsv_blink_ch = timeout(scrnsv_blink, NULL, hz);
#endif /* PCVT_PRETTYSCRNS */

		sw_cursor(0);	/* cursor off on mda/cga */
	}
	splx(x);
}

/*---------------------------------------------------------------------------*
 *	interface to screensaver "subsystem"
 *---------------------------------------------------------------------------*/
void
pcvt_scrnsv_reset(void)
{
	/*
	 * to save lotta time with superfluous timeout()/untimeout() calls
	 * when having massive output operations, we remember the last
	 * second of kernel timer we've rescheduled scrnsv_timedout()
	 */
	static long last_schedule = 0L;
	register int x = splhigh();
	int reschedule = 0;

	if((scrnsv_active == 1 || scrnsv_timeout) &&
	   last_schedule != time_second)
	{
		last_schedule = time_second;
		reschedule = 1;
		untimeout(scrnsv_timedout, NULL, scrnsv_timeout_ch);
	}
	if(scrnsv_active)
	{

#if PCVT_PRETTYSCRNS
		if(scrnsv_active > 1)
			untimeout(scrnsv_blink, NULL, scrnsv_blink_ch);
#endif /* PCVT_PRETTYSCRNS */

		bcopy(savedscreen, vsp->Crtat, scrnsv_size);
		if(adaptor_type == VGA_ADAPTOR)
		{
			/* back up VGA palette info */
			vgapaletteio(0 /* BLACK */, &vsp->palette[0], 1);

#if PCVT_PRETTYSCRNS
			vgapaletteio(7 /* LIGHTGRAY */, &vsp->palette[7], 1);
#endif /* PCVT_PRETTYSCRNS */

		}
		scrnsv_active = 0;

		if(vsp->cursor_on)
			sw_cursor(1);	/* cursor on */
	}

	if(reschedule)
	{
		/* mark next timeout */
		scrnsv_timeout_ch = timeout(scrnsv_timedout, NULL,
					    scrnsv_timeout * hz);
	}
	splx(x);
}

#endif /* PCVT_SCREENSAVER */

/*---------------------------------------------------------------------------*
 *	switch cursor on/off
 *---------------------------------------------------------------------------*/
void
sw_cursor(int onoff)
{
	if(adaptor_type == EGA_ADAPTOR)
	{
		int start, end;
		if(onoff)
		{
			start = vsp->cursor_start;
			end = vsp->cursor_end;
		}
		else
		{
			int cs = vs[current_video_screen].vga_charset;

			cs = (cs < 0) ? 0 : ((cs < totalfonts) ?
					     cs : totalfonts-1);

			start = (vgacs[cs].char_scanlines & 0x1F) + 1;
			end = 0;
		}
		outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
		outb(addr_6845+1, start);
		outb(addr_6845,CRTC_CUREND);	/* cursor end reg */
		outb(addr_6845+1, end);
	}
	else	/* mda, cga, vga */
	{
		outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
		if(onoff)
			outb(addr_6845+1, vsp->cursor_start);
		else
			outb(addr_6845+1, CURSOR_ON_BIT);
	}
}

/*---------------------------------------------------------------------------*
 *	cold init support, if a mono monitor is attached to a
 *	vga or ega, it comes up with a mda emulation. switch
 *	board to generic ega/vga mode in this case.
 *---------------------------------------------------------------------------*/
void
mda2egaorvga(void)
{
	/*
	 * program sequencer to access
	 * video ram
	 */

	/* synchronous reset */
	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);

	/* write to map 0 & 1 */
	outb(TS_INDEX, TS_WRPLMASK);
	outb(TS_DATA, 0x03);

	/* odd-even addressing */
	outb(TS_INDEX, TS_MEMMODE);
	outb(TS_DATA, 0x03);

	/* clear synchronous reset */
	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);

	/*
	 * program graphics controller
	 * to access character
	 * generator
	 */

	/* select map 0 for cpu reads */
	outb(GDC_INDEX, GDC_RDPLANESEL);
	outb(GDC_DATA, 0x00);

	/* enable odd-even addressing */
	outb(GDC_INDEX, GDC_MODE);
	outb(GDC_DATA, 0x10);

	/* map starts at 0xb000 */
	outb(GDC_INDEX, GDC_MISC);
	outb(GDC_DATA, 0x0a);
}

#endif	/* NVT > 0 */

/* ------------------------- E O F ------------------------------------------*/
