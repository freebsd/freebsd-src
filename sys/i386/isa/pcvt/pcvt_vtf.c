/*
 * Copyright (c) 1999 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore.
 *
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
 *	This product includes software developed by Hellmuth Michaelis,
 *	Brian Dunford-Shore and Joerg Wunsch.
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
 *	pcvt_vtf.c	VT220 Terminal Emulator Functions
 *	-------------------------------------------------
 *
 *	Last Edit-Date: [Mon Dec 27 14:13:33 1999]
 *
 * $FreeBSD: src/sys/i386/isa/pcvt/pcvt_vtf.c,v 1.11 1999/12/30 16:17:11 hm Exp $
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#if NVT > 0

#define PCVT_INCLUDE_VT_SELATTR	/* get inline function from pcvt_hdr.h */

#include <i386/isa/pcvt/pcvt_hdr.h>	/* global include */
#include <i386/isa/pcvt/pcvt_tbl.h>	/* character set conversion tables */

static void clear_dld ( struct video_state *svsp );
static void init_dld ( struct video_state *svsp );
static void init_udk ( struct video_state *svsp );
static void respond ( struct video_state *svsp );
static void roll_down ( struct video_state *svsp, int n );
static void selective_erase ( struct video_state *svsp, u_short *pcrtat,
			      int length );
static void swcsp ( struct video_state *svsp, u_short *ctp );

/*---------------------------------------------------------------------------*
 *	DECSTBM - set top and bottom margins
 *---------------------------------------------------------------------------*/
void
vt_stbm(struct video_state *svsp)
{
	/* both 0 => scrolling region = entire screen */

	if((svsp->parms[0] == 0) && (svsp->parms[1] == 0))
	{
		svsp->cur_offset = 0;
		svsp->scrr_beg = 0;
		svsp->scrr_len = svsp->screen_rows;
		svsp->scrr_end = svsp->scrr_len - 1;
		svsp->col = 0;
		return;
	}

	if(svsp->parms[1] <= svsp->parms[0])
		return;

	/* range parm 1 */

	if(svsp->parms[0] < 1)
		svsp->parms[0] = 1;
	else if(svsp->parms[0] > svsp->screen_rows-1)
		svsp->parms[0] = svsp->screen_rows-1;

	/* range parm 2 */

	if(svsp->parms[1] < 2)
		svsp->parms[1] = 2;
	else if(svsp->parms[1] > svsp->screen_rows)
		svsp->parms[1] = svsp->screen_rows;

	svsp->scrr_beg = svsp->parms[0]-1;	/* begin of scrolling region */
	svsp->scrr_len = svsp->parms[1] - svsp->parms[0] + 1; /* no of lines */
	svsp->scrr_end = svsp->parms[1]-1;

	/* cursor to first pos */
	if(svsp->m_om)
		svsp->cur_offset = svsp->scrr_beg * svsp->maxcol;
	else
		svsp->cur_offset = 0;

	svsp->col = 0;
}

/*---------------------------------------------------------------------------*
 *	SGR - set graphic rendition
 *---------------------------------------------------------------------------*/
void
vt_sgr(struct video_state *svsp)
{
	register int i = 0;
	u_short setcolor = 0;
	char colortouched = 0;

	do
	{
		switch(svsp->parms[i++])
		{
			case 0:		/* reset to normal attributes */
				svsp->vtsgr = VT_NORMAL;
				break;

			case 1:		/* bold */
				svsp->vtsgr |= VT_BOLD;
				break;

			case 4:		/* underline */
				svsp->vtsgr |= VT_UNDER;
				break;

			case 5:		/* blinking */
				svsp->vtsgr |= VT_BLINK;
				break;

			case 7:		/* reverse */
				svsp->vtsgr |= VT_INVERSE;
				break;

			case 22:	/* not bold */
				svsp->vtsgr &= ~VT_BOLD;
				break;

			case 24:	/* not underlined */
				svsp->vtsgr &= ~VT_UNDER;
				break;

			case 25:	/* not blinking */
				svsp->vtsgr &= ~VT_BLINK;
				break;

			case 27:	/* not reverse */
				svsp->vtsgr &= ~VT_INVERSE;
				break;

			case 30:	/* foreground colors */
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
				if(color)
				{
				 colortouched = 1;
				 setcolor |= ((fgansitopc[(svsp->parms[i-1]-30) & 7]) << 8);
				}
				break;

			case 40:	/* background colors */
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
				if(color)
				{
				 colortouched = 1;
				 setcolor |= ((bgansitopc[(svsp->parms[i-1]-40) & 7]) << 8);
				}
				break;
		}
	}
	while(i <= svsp->parmi);
	if(color)
	{
		if(colortouched)
			svsp->c_attr = setcolor;
		else
			svsp->c_attr = ((sgr_tab_color[svsp->vtsgr]) << 8);
	}
	else
	{
		if(adaptor_type == MDA_ADAPTOR)
			svsp->c_attr = ((sgr_tab_imono[svsp->vtsgr]) << 8);
		else
			svsp->c_attr = ((sgr_tab_mono[svsp->vtsgr]) << 8);
	}
}

/*---------------------------------------------------------------------------*
 *	CUU - cursor up
 *---------------------------------------------------------------------------*/
void
vt_cuu(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if (p <= 0)				/* parameter min */
		p = 1;

	p = min(p, svsp->row - svsp->scrr_beg);

	if (p <= 0)
		return;

	svsp->cur_offset -= (svsp->maxcol * p);
}

/*---------------------------------------------------------------------------*
 *	CUD - cursor down
 *---------------------------------------------------------------------------*/
void
vt_cud(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if (p <= 0)
		p = 1;

	p = min(p, svsp->scrr_end - svsp->row);

	if (p <= 0)
		return;

	svsp->cur_offset += (svsp->maxcol * p);
}

/*---------------------------------------------------------------------------*
 *	CUF - cursor forward
 *---------------------------------------------------------------------------*/
void
vt_cuf(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(svsp->col == ((svsp->maxcol)-1))	/* already at right margin */
		return;

	if(p <= 0)				/* parameter min = 1 */
		p = 1;
	else if(p > ((svsp->maxcol)-1))		/* parameter max = 79 */
		p = ((svsp->maxcol)-1);

	if((svsp->col + p) > ((svsp->maxcol)-1))/* not more than right margin */
		p = ((svsp->maxcol)-1) - svsp->col;

	svsp->cur_offset += p;
	svsp->col += p;
}

/*---------------------------------------------------------------------------*
 *	CUB - cursor backward
 *---------------------------------------------------------------------------*/
void
vt_cub(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(svsp->col == 0)			/* already at left margin ? */
		return;

	if(p <= 0)				/* parameter min = 1 */
		p = 1;
	else if(p > ((svsp->maxcol)-1))		/* parameter max = 79 */
		p = ((svsp->maxcol)-1);

	if((svsp->col - p) <= 0)		/* not more than left margin */
		p = svsp->col;

	svsp->cur_offset -= p;
	svsp->col -= p;
}

/*---------------------------------------------------------------------------*
 *	ED - erase in display
 *---------------------------------------------------------------------------*/
void
vt_clreos(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:
			fillw(user_attr | ' ', svsp->Crtat + svsp->cur_offset,
				svsp->Crtat +
				(svsp->maxcol * svsp->screen_rows) -
				(svsp->Crtat + svsp->cur_offset));
			break;

		case 1:
			fillw(user_attr | ' ', svsp->Crtat,
				svsp->Crtat + svsp->cur_offset -
				svsp->Crtat + 1 );
			break;

		case 2:
			fillw(user_attr | ' ', svsp->Crtat,
				svsp->maxcol * svsp->screen_rows);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	EL - erase in line
 *---------------------------------------------------------------------------*/
void
vt_clreol(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:
			fillw(user_attr | ' ',
				svsp->Crtat + svsp->cur_offset,
				svsp->maxcol-svsp->col);
			break;

		case 1:
			fillw(user_attr | ' ',
				svsp->Crtat + svsp->cur_offset - svsp->col,
				svsp->col + 1);
			break;

		case 2:
			fillw(user_attr | ' ',
				svsp->Crtat + svsp->cur_offset - svsp->col,
				svsp->maxcol);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	CUP - cursor position / HVP - horizontal & vertical position
 *---------------------------------------------------------------------------*/
void
vt_curadr(struct video_state *svsp)
{
	if(svsp->m_om)	/* relative to scrolling region */
	{
		if((svsp->parms[0] == 0) && (svsp->parms[1] == 0))
		{
			svsp->cur_offset = svsp->scrr_beg * svsp->maxcol;
			svsp->col = 0;
                        svsp->abs_write = 0;
			return;
		}

		if(svsp->parms[0] <= 0)
			svsp->parms[0] = 1;
		else if(svsp->parms[0] > svsp->scrr_len)
			svsp->parms[0] = svsp->scrr_len;

		if(svsp->parms[1] <= 0 )
			svsp->parms[1] = 1;
		if(svsp->parms[1] > svsp->maxcol)
			svsp->parms[1] = svsp->maxcol;

		svsp->cur_offset = (svsp->scrr_beg * svsp->maxcol) +
				   ((svsp->parms[0] - 1) * svsp->maxcol) +
				   svsp->parms[1] - 1;
		svsp->col = svsp->parms[1] - 1;
                svsp->abs_write = 0;
	}
	else	/* relative to screen start */
	{
		if((svsp->parms[0] == 0) && (svsp->parms[1] == 0))
		{
			svsp->cur_offset = 0;
			svsp->col = 0;
                        svsp->abs_write = 0;
			return;
		}

		if(svsp->parms[0] <= 0)
			svsp->parms[0] = 1;
		else if(svsp->parms[0] > svsp->screen_rows)
			svsp->parms[0] = svsp->screen_rows;

		if(svsp->parms[1] <= 0 )
			svsp->parms[1] = 1;
		if(svsp->parms[1] > svsp->maxcol)	/* col */
			svsp->parms[1] = svsp->maxcol;

		svsp->cur_offset = (((svsp->parms[0]-1)*svsp->maxcol) +
				    (svsp->parms[1]-1));
		svsp->col = svsp->parms[1]-1;

                if (svsp->cur_offset >=
                        ((svsp->scrr_beg + svsp->scrr_len + 1) * svsp->maxcol))

                        svsp->abs_write = 1;
                else
                        svsp->abs_write = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	RIS - reset to initial state (hard emulator runtime reset)
 *---------------------------------------------------------------------------*/
void
vt_ris(struct video_state *svsp)
{
	fillw(user_attr | ' ', svsp->Crtat, svsp->maxcol * svsp->screen_rows);
	svsp->cur_offset = 0;		/* cursor upper left corner */
	svsp->col = 0;
	svsp->row = 0;
	svsp->lnm = 0;			/* CR only */
	clear_dld(svsp);		/* clear download charset */
	vt_clearudk(svsp);		/* clear user defined keys */
	svsp->selchar = 0;		/* selective attribute off */
	vt_str(svsp);			/* and soft terminal reset */
}

/*---------------------------------------------------------------------------*
 *	DECSTR - soft terminal reset (SOFT emulator runtime reset)
 *---------------------------------------------------------------------------*/
void
vt_str(struct video_state *svsp)
{
	int i;

	clr_parms(svsp);			/* escape parameter init */
	svsp->state = STATE_INIT;		/* initial state */

	svsp->dis_fnc = 0;			/* display functions reset */

	svsp->sc_flag = 0;			/* save cursor position */
	svsp->transparent = 0;			/* enable control code processing */

	for(i = 0; i < MAXTAB; i++)		/* setup tabstops */
	{
		if(!(i % 8))
			svsp->tab_stops[i] = 1;
		else
			svsp->tab_stops[i] = 0;
	}

	svsp->irm = 0;				/* replace mode */
	svsp->m_om = 0;				/* origin mode */
	svsp->m_awm = 1;			/* auto wrap mode */

#if PCVT_INHIBIT_NUMLOCK
	svsp->num_lock = 0;			/* keypad application mode */
#else
	svsp->num_lock = 1;			/* keypad numeric mode */
#endif

	svsp->scroll_lock = 0;			/* reset keyboard modes */
	svsp->caps_lock = 0;

	svsp->ckm = 1;				/* cursor key mode = "normal" ... */
	svsp->scrr_beg = 0;			/* start of scrolling region */
	svsp->scrr_len = svsp->screen_rows;	/* no. of lines in scrolling region */
	svsp->abs_write = 0;			/* scrr is complete screen */
	svsp->scrr_end = svsp->scrr_len - 1;

	if(adaptor_type == EGA_ADAPTOR || adaptor_type == VGA_ADAPTOR)
	{
		svsp->G0 = cse_ascii;		/* G0 = ascii	*/
		svsp->G1 = cse_ascii;		/* G1 = ascii	*/
		svsp->G2 = cse_supplemental;	/* G2 = supplemental */
		svsp->G3 = cse_supplemental;	/* G3 = supplemental */
		svsp->GL = &svsp->G0;		/* GL = G0 */
		svsp->GR = &svsp->G2;		/* GR = G2 */
	}
	else
	{
		svsp->G0 = csd_ascii;		/* G0 = ascii	*/
		svsp->G1 = csd_ascii;		/* G1 = ascii	*/
		svsp->G2 = csd_supplemental;	/* G2 = supplemental */
		svsp->G3 = csd_supplemental;	/* G3 = supplemental */
		svsp->GL = &svsp->G0;		/* GL = G0 */
		svsp->GR = &svsp->G2;		/* GR = G2 */
	}

	svsp->vtsgr = VT_NORMAL;		/* no attributes */
	svsp->c_attr = user_attr;		/* reset sgr to normal */

	svsp->selchar = 0;			/* selective attribute off */
	vt_initsel(svsp);

	init_ufkl(svsp);			/* init user fkey labels */
	init_sfkl(svsp);			/* init system fkey labels */

	update_led();				/* update keyboard LED's */
}

/*---------------------------------------------------------------------------*
 *	RI - reverse index, move cursor up
 *---------------------------------------------------------------------------*/
void
vt_ri(struct video_state *svsp)
{
	if(svsp->cur_offset >= ((svsp->scrr_beg * svsp->maxcol) + svsp->maxcol))
		svsp->cur_offset -= svsp->maxcol;
	else
		roll_down(svsp, 1);
}

/*---------------------------------------------------------------------------*
 *	IND - index, move cursor down
 *---------------------------------------------------------------------------*/
void
vt_ind(struct video_state *svsp)
{
	if(svsp->cur_offset < (svsp->scrr_end * svsp->maxcol))
		svsp->cur_offset += svsp->maxcol;
	else
		roll_up(svsp, 1);
}

/*---------------------------------------------------------------------------*
 *	NEL - next line, first pos of next line
 *---------------------------------------------------------------------------*/
void
vt_nel(struct video_state *svsp)
{
	if(svsp->cur_offset < (svsp->scrr_end * svsp->maxcol))
	{
		svsp->cur_offset += (svsp->maxcol-svsp->col);
		svsp->col = 0;
	}
	else
	{
		roll_up(svsp, 1);
		svsp->cur_offset -= svsp->col;
		svsp->col = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	set dec private modes, esc [ ? x h
 *---------------------------------------------------------------------------*/
void
vt_set_dec_priv_qm(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:		/* error, ignored */
		case 1:		/* CKM - cursor key mode */
			svsp->ckm = 1;
			break;

		case 2:		/* ANM - ansi/vt52 mode */
			break;

		case 3:		/* COLM - column mode */
			vt_col(svsp, SCR_COL132);
			break;

		case 4:		/* SCLM - scrolling mode */
		case 5:		/* SCNM - screen mode */
			break;

		case 6:		/* OM - origin mode */
			svsp->m_om = 1;
			break;

		case 7:		/* AWM - auto wrap mode */
			svsp->m_awm = 1;
			swritefkl(7,(u_char *)"AUTOWRAPENABLE *",svsp);
			break;

		case 8:		/* ARM - auto repeat mode */
			kbrepflag = 1;
			break;

		case 9:		/* INLM - interlace mode */
		case 10:	/* EDM - edit mode */
		case 11:	/* LTM - line transmit mode */
		case 12:	/* */
		case 13:	/* SCFDM - space compression / field delimiting */
		case 14:	/* TEM - transmit execution mode */
		case 15:	/* */
		case 16:	/* EKEM - edit key execution mode */
			break;

		case 25:	/* TCEM - text cursor enable mode */
			if(vsp == svsp)
				sw_cursor(1);	/* cursor on */
			svsp->cursor_on = 1;
			break;

		case 42:	/* NRCM - 7bit NRC characters */
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	reset dec private modes, esc [ ? x l
 *---------------------------------------------------------------------------*/
void
vt_reset_dec_priv_qm(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:		/* error, ignored */
		case 1:		/* CKM - cursor key mode */
			svsp->ckm = 0;
			break;

		case 2:		/* ANM - ansi/vt52 mode */
			break;

		case 3:		/* COLM - column mode */
			vt_col(svsp, SCR_COL80);
			break;

		case 4:		/* SCLM - scrolling mode */
		case 5:		/* SCNM - screen mode */
			break;

		case 6:		/* OM - origin mode */
			svsp->m_om = 0;
			break;

		case 7:		/* AWM - auto wrap mode */
			svsp->m_awm = 0;
			swritefkl(7,(u_char *)"AUTOWRAPENABLE  ",svsp);
			break;

		case 8:		/* ARM - auto repeat mode */
			kbrepflag = 0;
			break;

		case 9:		/* INLM - interlace mode */
		case 10:	/* EDM - edit mode */
		case 11:	/* LTM - line transmit mode */
		case 12:	/* */
		case 13:	/* SCFDM - space compression / field delimiting */
		case 14:	/* TEM - transmit execution mode */
		case 15:	/* */
		case 16:	/* EKEM - edit key execution mode */
			break;

		case 25:	/* TCEM - text cursor enable mode */
			if(vsp == svsp)
				sw_cursor(0);	/* cursor off */
			svsp->cursor_on = 0;
			break;

		case 42:	/* NRCM - 7bit NRC characters */
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	set ansi modes, esc [ x
 *---------------------------------------------------------------------------*/
void
vt_set_ansi(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:		/* error, ignored */
		case 1:		/* GATM - guarded area transfer mode */
		case 2:		/* KAM - keyboard action mode */
		case 3:		/* CRM - Control Representation mode */
			break;

		case 4:		/* IRM - insert replacement mode */
			svsp->irm = 1; /* Insert mode */
			break;

		case 5:		/* SRTM - status report transfer mode */
		case 6:		/* ERM - erasue mode */
		case 7:		/* VEM - vertical editing mode */
		case 10:	/* HEM - horizontal editing mode */
		case 11:	/* PUM - position unit mode */
		case 12:	/* SRM - send-receive mode */
		case 13:	/* FEAM - format effector action mode */
		case 14:	/* FETM - format effector transfer mode */
		case 15:	/* MATM - multiple area transfer mode */
		case 16:	/* TTM - transfer termination */
		case 17:	/* SATM - selected area transfer mode */
		case 18:	/* TSM - tabulation stop mode */
		case 19:	/* EBM - editing boundary mode */
			break;

		case 20:	/* LNM - line feed / newline mode */
			svsp->lnm = 1;
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	reset ansi modes, esc [ x
 *---------------------------------------------------------------------------*/
void
vt_reset_ansi(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:		/* error, ignored */
		case 1:		/* GATM - guarded area transfer mode */
		case 2:		/* KAM - keyboard action mode */
		case 3:		/* CRM - Control Representation mode */
			break;

		case 4:		/* IRM - insert replacement mode */
			svsp->irm = 0;  /* Replace mode */
			break;

		case 5:		/* SRTM - status report transfer mode */
		case 6:		/* ERM - erasue mode */
		case 7:		/* VEM - vertical editing mode */
		case 10:	/* HEM - horizontal editing mode */
		case 11:	/* PUM - position unit mode */
		case 12:	/* SRM - send-receive mode */
		case 13:	/* FEAM - format effector action mode */
		case 14:	/* FETM - format effector transfer mode */
		case 15:	/* MATM - multiple area transfer mode */
		case 16:	/* TTM - transfer termination */
		case 17:	/* SATM - selected area transfer mode */
		case 18:	/* TSM - tabulation stop mode */
		case 19:	/* EBM - editing boundary mode */
			break;

		case 20:	/* LNM - line feed / newline mode */
			svsp->lnm = 0;
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	clear tab stop(s)
 *---------------------------------------------------------------------------*/
void
vt_clrtab(struct video_state *svsp)
{
	int i;

	if(svsp->parms[0] == 0)
		svsp->tab_stops[svsp->col] = 0;
	else if(svsp->parms[0] == 3)
	{
		for(i=0; i<MAXTAB; i++)
			svsp->tab_stops[i] = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	DECSC - save cursor & attributes
 *---------------------------------------------------------------------------*/
void
vt_sc(struct video_state *svsp)
{
	svsp->sc_flag = 1;
	svsp->sc_row = svsp->row;
	svsp->sc_col = svsp->col;
	svsp->sc_cur_offset = svsp->cur_offset;
	svsp->sc_attr = svsp->c_attr;
	svsp->sc_awm = svsp->m_awm;
	svsp->sc_om = svsp->m_om;
	svsp->sc_G0 = svsp->G0;
	svsp->sc_G1 = svsp->G1;
	svsp->sc_G2 = svsp->G2;
	svsp->sc_G3 = svsp->G3;
	svsp->sc_GL = svsp->GL;
	svsp->sc_GR = svsp->GR;
	svsp->sc_sel = svsp->selchar;
	svsp->sc_vtsgr = svsp->vtsgr;
}

/*---------------------------------------------------------------------------*
 *	DECRC - restore cursor & attributes
 *---------------------------------------------------------------------------*/
void
vt_rc(struct video_state *svsp)
{
	if(svsp->sc_flag == 1)
	{
		svsp->sc_flag = 0;
		svsp->row = svsp->sc_row;
		svsp->col = svsp->sc_col;
		svsp->cur_offset = svsp->sc_cur_offset;
		svsp->c_attr = svsp->sc_attr;
		svsp->m_awm = svsp->sc_awm;
		svsp->m_om = svsp->sc_om;
		svsp->G0 = svsp->sc_G0;
		svsp->G1 = svsp->sc_G1;
		svsp->G2 = svsp->sc_G2;
		svsp->G3 = svsp->sc_G3;
		svsp->GL = svsp->sc_GL;
		svsp->GR = svsp->sc_GR;
		svsp->selchar = svsp->sc_sel;
		svsp->vtsgr = svsp->sc_vtsgr;
	}
}

/*---------------------------------------------------------------------------*
 *	designate a character set as G0, G1, G2 or G3 for 94/96 char sets
 *---------------------------------------------------------------------------*/
void
vt_designate(struct video_state *svsp)
{
	u_short *ctp = NULL;
	u_char ch;

	if(svsp->whichi == 1)
		ch = svsp->which[0];
	else
	{
		int i;

		if(svsp->dld_id[0] == '\0')
			return;

		if(!(((adaptor_type == EGA_ADAPTOR) ||
		     (adaptor_type == VGA_ADAPTOR)) &&
		     (vgacs[svsp->vga_charset].secondloaded)))
		{
			return;
		}

		for(i = (svsp->whichi)-1; i >= 0; i--)
		{
			 if(svsp->which[i] != svsp->dld_id[i])
				return;
		}
#ifdef HAVECSE_DOWNLOADABLE
		ctp = cse_downloadable;
		swcsp(svsp, ctp);
#endif
		return;
	}

	if(((adaptor_type == EGA_ADAPTOR) || (adaptor_type == VGA_ADAPTOR)) &&
	   (vgacs[svsp->vga_charset].secondloaded))
	{
		if((ch == svsp->dld_id[0]) && (svsp->dld_id[1] == '\0'))
		{
#ifdef HAVECSE_DOWNLOADABLE
			ctp = cse_downloadable;
			swcsp(svsp, ctp);
#endif
			return;
		}

		switch(ch)
		{
			case 'A': /* British or ISO-Latin-1 */
				switch(svsp->state)
				{
					case STATE_BROPN: /* designate G0 */
					case STATE_BRCLO: /* designate G1 */
					case STATE_STAR:  /* designate G2 */
					case STATE_PLUS:  /* designate G3 */
#ifdef HAVECSE_BRITISH
						ctp = cse_british;
#endif
						break;

					case STATE_MINUS: /* designate G1 (96)*/
					case STATE_DOT:	  /* designate G2 (96)*/
					case STATE_SLASH: /* designate G3 (96)*/
#ifdef HAVECSE_ISOLATIN
						ctp = cse_isolatin;
#endif
						break;
				}
				break;

			case 'B': /* USASCII */
#ifdef HAVECSE_ASCII
				ctp = cse_ascii;
#endif
				break;

			case 'C': /* Finnish */
			case '5': /* Finnish */
#ifdef HAVECSE_FINNISH
				ctp = cse_finnish;
#endif
				break;

			case 'E': /* Norwegian/Danish */
			case '6': /* Norwegian/Danish */
#ifdef HAVECSE_NORWEGIANDANISH
				ctp = cse_norwegiandanish;
#endif
				break;

			case 'H': /* Swedish */
			case '7': /* Swedish */
#ifdef HAVECSE_SWEDISH
				ctp = cse_swedish;
#endif
				break;

			case 'K': /* German */
#ifdef HAVECSE_GERMAN
				ctp = cse_german;
#endif
				break;

			case 'Q': /* French Canadien */
#ifdef HAVECSE_FRENCHCANADA
				ctp = cse_frenchcanada;
#endif
				break;

			case 'R': /* French */
#ifdef HAVECSE_FRENCH
				ctp = cse_french;
#endif
				break;

			case 'Y': /* Italian */
#ifdef HAVECSE_ITALIAN
				ctp = cse_italian;
#endif
				break;

			case 'Z': /* Spanish */
#ifdef HAVECSE_SPANISH
				ctp = cse_spanish;
#endif
				break;

			case '0': /* special graphics */
#ifdef HAVECSE_SPECIAL
				ctp = cse_special;
#endif
				break;

			case '1': /* alternate ROM */
#ifdef HAVECSE_ALTERNATEROM1
				ctp = cse_alternaterom1;
#endif
				break;

			case '2': /* alt ROM, spec graphics */
#ifdef HAVECSE_ALTERNATEROM2
				ctp = cse_alternaterom2;
#endif
				break;

			case '3': /* HP Roman 8, upper 128 chars*/
#ifdef HAVECSE_ROMAN8
				ctp = cse_roman8;
#endif
				break;

			case '4': /* Dutch */
#ifdef HAVECSE_DUTCH
				ctp = cse_dutch;
#endif
				break;

			case '<': /* DEC Supplemental */
#ifdef HAVECSE_SUPPLEMENTAL
				ctp = cse_supplemental;
#endif
				break;

			case '=': /* Swiss */
#ifdef HAVECSE_SWISS
				ctp = cse_swiss;
#endif
				break;

			case '>': /* DEC Technical */
#ifdef HAVECSE_TECHNICAL
				ctp = cse_technical;
#endif
				break;

			default:
				break;
		}
	}
	else
	{
		switch(ch)
		{
			case 'A': /* British or ISO-Latin-1 */
				switch(svsp->state)
				{
					case STATE_BROPN: /* designate G0 */
					case STATE_BRCLO: /* designate G1 */
					case STATE_STAR:  /* designate G2 */
					case STATE_PLUS:  /* designate G3 */
#ifdef HAVECSD_BRITISH
						ctp = csd_british;
#endif
						break;

					case STATE_MINUS: /* designate G1 (96)*/
					case STATE_DOT:	  /* designate G2 (96)*/
					case STATE_SLASH: /* designate G3 (96)*/
#ifdef HAVECSD_ISOLATIN
						ctp = csd_isolatin;
#endif
						break;
				}
				break;

			case 'B': /* USASCII */
#ifdef HAVECSD_ASCII
				ctp = csd_ascii;
#endif
				break;

			case 'C': /* Finnish */
			case '5': /* Finnish */
#ifdef HAVECSD_FINNISH
				ctp = csd_finnish;
#endif
				break;

			case 'E': /* Norwegian/Danish */
			case '6': /* Norwegian/Danish */
#ifdef HAVECSD_NORWEGIANDANISH
				ctp = csd_norwegiandanish;
#endif
				break;

			case 'H': /* Swedish */
			case '7': /* Swedish */
#ifdef HAVECSD_SWEDISH
				ctp = csd_swedish;
#endif
				break;

			case 'K': /* German */
#ifdef HAVECSD_GERMAN
				ctp = csd_german;
#endif
				break;

			case 'Q': /* French Canadien */
#ifdef HAVECSD_FRENCHCANADA
				ctp = csd_frenchcanada;
#endif
				break;

			case 'R': /* French */
#ifdef HAVECSD_FRENCH
				ctp = csd_french;
#endif
				break;

			case 'Y': /* Italian */
#ifdef HAVECSD_ITALIAN
				ctp = csd_italian;
#endif
				break;

			case 'Z': /* Spanish */
#ifdef HAVECSD_SPANISH
				ctp = csd_spanish;
#endif
				break;

			case '0': /* special graphics */
#ifdef HAVECSD_SPECIAL
				ctp = csd_special;
#endif
				break;

			case '1': /* alternate ROM */
#ifdef HAVECSD_ALTERNATEROM1
				ctp = csd_alternaterom1;
#endif
				break;

			case '2': /* alt ROM, spec graphics */
#ifdef HAVECSD_ALTERNATEROM2
				ctp = csd_alternaterom2;
#endif
				break;

			case '3': /* HP Roman 8, upper 128 chars*/
#ifdef HAVECSD_ROMAN8
				ctp = csd_roman8;
#endif
				break;

			case '4': /* Dutch */
#ifdef HAVECSD_DUTCH
				ctp = csd_dutch;
#endif
				break;

			case '<': /* DEC Supplemental */
#ifdef HAVECSD_SUPPLEMENTAL
				ctp = csd_supplemental;
#endif
				break;

			case '=': /* Swiss */
#ifdef HAVECSD_SWISS
				ctp = csd_swiss;
#endif
				break;

			case '>': /* DEC Technical */
#ifdef HAVECSD_TECHNICAL
				ctp = csd_technical;
#endif
				break;

			default:
				break;
		}
	}
	swcsp(svsp, ctp);
}

/*---------------------------------------------------------------------------*
 *	device attributes
 *---------------------------------------------------------------------------*/
void
vt_da(struct video_state *svsp)
{
	static u_char *response = (u_char *)DA_VT220;

	svsp->report_chars = response;
	svsp->report_count = 18;
	respond(svsp);
}

/*---------------------------------------------------------------------------*
 *	screen alignment display
 *---------------------------------------------------------------------------*/
void
vt_aln(struct video_state *svsp)
{
	register int i;

	svsp->cur_offset = 0;
	svsp->col = 0;

	for(i=0; i < (svsp->screen_rows*svsp->maxcol); i++)
	{
		*(svsp->Crtat + svsp->cur_offset) = user_attr | 'E';
		vt_selattr(svsp);
		svsp->cur_offset++;
		svsp->col++;
	}

	svsp->cur_offset = 0;	/* reset everything ! */
	svsp->col = 0;
	svsp->row = 0;
}

/*---------------------------------------------------------------------------*
 *	request terminal parameters
 *---------------------------------------------------------------------------*/
void
vt_reqtparm(struct video_state *svsp)
{
	static u_char *answr = (u_char *)"\033[3;1;1;120;120;1;0x";

	svsp->report_chars = answr;
	svsp->report_count = 20;
	respond(svsp);
}

/*---------------------------------------------------------------------------*
 *	invoke selftest
 *---------------------------------------------------------------------------*/
void
vt_tst(struct video_state *svsp)
{
	clear_dld(svsp);
}

/*---------------------------------------------------------------------------*
 *	device status reports
 *---------------------------------------------------------------------------*/
void
vt_dsr(struct video_state *svsp)
{
	static u_char *answr = (u_char *)"\033[0n";
	static u_char *panswr = (u_char *)"\033[?13n"; /* Printer Unattached */
	static u_char *udkanswr = (u_char *)"\033[?21n"; /* UDK Locked */
	static u_char *langanswr = (u_char *)"\033[?27;1n"; /* North American*/
	static u_char buffer[16];
	int i = 0;

	switch(svsp->parms[0])
	{
		case 5:		/* return status */
			svsp->report_chars = answr;
			svsp->report_count = 4;
			respond(svsp);
			break;

		case 6:		/* return cursor position */
			buffer[i++] = 0x1b;
			buffer[i++] = '[';
			if((svsp->row+1) > 10)
				buffer[i++] = ((svsp->row+1) / 10) + '0';
			buffer[i++] = ((svsp->row+1) % 10) + '0';
			buffer[i++] = ';';
			if((svsp->col+1) > 10)
				buffer[i++] = ((svsp->col+1) / 10) + '0';
			buffer[i++] = ((svsp->col+1) % 10) + '0';
			buffer[i++] = 'R';
			buffer[i++] = '\0';

			svsp->report_chars = buffer;
			svsp->report_count = i;
			respond(svsp);
			break;

		case 15:	/* return printer status */
			svsp->report_chars = panswr;
			svsp->report_count = 6;
			respond(svsp);
			break;

		case 25:	/* return udk status */
			svsp->report_chars = udkanswr;
			svsp->report_count = 6;
			respond(svsp);
			break;

		case 26:	/* return language status */
			svsp->report_chars = langanswr;
			svsp->report_count = 8;
			respond(svsp);
			break;

		default:	/* nothing else valid */
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	IL - insert line
 *---------------------------------------------------------------------------*/
void
vt_il(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if((svsp->row >= svsp->scrr_beg) && (svsp->row <= svsp->scrr_end))
	{
		if(p <= 0)
			p = 1;
		else if(p > svsp->scrr_end - svsp->row)
			p = svsp->scrr_end - svsp->row;

		svsp->cur_offset -= svsp->col;
		svsp->col = 0;
		if(svsp->row == svsp->scrr_beg)
			roll_down(svsp, p);
		else
		{
		    bcopy(svsp->Crtat + svsp->cur_offset,
		          svsp->Crtat + svsp->cur_offset + (p * svsp->maxcol),
		          svsp->maxcol * (svsp->scrr_end-svsp->row+1-p) * CHR );

		    fillw(user_attr | ' ',
			  svsp->Crtat + svsp->cur_offset,
			  p * svsp->maxcol);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	ICH - insert character
 *---------------------------------------------------------------------------*/
void
vt_ic(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(p <= 0)
		p = 1;
	else if(p > svsp->maxcol-svsp->col)
		p = svsp->maxcol-svsp->col;

	while(p--)
	{
		bcopy((svsp->Crtat + svsp->cur_offset),
		      (svsp->Crtat + svsp->cur_offset) + 1,
		      (((svsp->maxcol)-1)-svsp->col) * CHR);

		*(svsp->Crtat + svsp->cur_offset) = user_attr | ' ';
		vt_selattr(svsp);
	}
}

/*---------------------------------------------------------------------------*
 *	DL - delete line
 *---------------------------------------------------------------------------*/
void
vt_dl(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if((svsp->row >= svsp->scrr_beg) && (svsp->row <= svsp->scrr_end))
	{
		if(p <= 0)
			p = 1;
		else if(p > svsp->scrr_end - svsp->row)
			p = svsp->scrr_end - svsp->row;

		svsp->cur_offset -= svsp->col;
		svsp->col = 0;

		if(svsp->row == svsp->scrr_beg)
			roll_up(svsp, p);
		else
		{
		    bcopy(svsp->Crtat + svsp->cur_offset + (p * svsp->maxcol),
			  svsp->Crtat + svsp->cur_offset,
			  svsp->maxcol * (svsp->scrr_end-svsp->row+1-p) * CHR );

		    fillw(user_attr | ' ',
			  svsp->Crtat + ((svsp->scrr_end-p+1) * svsp->maxcol),
			  p * svsp->maxcol);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	DCH - delete character
 *---------------------------------------------------------------------------*/
void
vt_dch(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(p <= 0)
		p = 1;
	else if(p > svsp->maxcol-svsp->col)
		p = svsp->maxcol-svsp->col;

	while(p--)
	{
		bcopy((svsp->Crtat + svsp->cur_offset)+1,
		      (svsp->Crtat + svsp->cur_offset),
		      (((svsp->maxcol)-1) - svsp->col)* CHR );

		*((svsp->Crtat + svsp->cur_offset) +
			((svsp->maxcol)-1)-svsp->col) = user_attr | ' ';
	}
}

/*---------------------------------------------------------------------------*
 *	scroll up
 *---------------------------------------------------------------------------*/
void
vt_su(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(p <= 0)
		p = 1;
	else if(p > svsp->screen_rows-1)
		p = svsp->screen_rows-1;

	roll_up(svsp, p);
}

/*---------------------------------------------------------------------------*
 *	scroll down
 *---------------------------------------------------------------------------*/
void
vt_sd(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(p <= 0)
		p = 1;
	else if(p > svsp->screen_rows-1)
		p = svsp->screen_rows-1;

	roll_down(svsp, p);
}

/*---------------------------------------------------------------------------*
 *	ECH - erase character
 *---------------------------------------------------------------------------*/
void
vt_ech(struct video_state *svsp)
{
	register int p = svsp->parms[0];

	if(p <= 0)
		p = 1;
	else if(p > svsp->maxcol-svsp->col)
		p = svsp->maxcol-svsp->col;

	fillw(user_attr | ' ', (svsp->Crtat + svsp->cur_offset), p);
}

/*---------------------------------------------------------------------------*
 *	media copy	(NO PRINTER AVAILABLE IN KERNEL ...)
 *---------------------------------------------------------------------------*/
void
vt_mc(struct video_state *svsp)
{
}

/*---------------------------------------------------------------------------*
 *	Device Control String State Machine Entry for:
 *
 *	DECUDK - user-defined keys	and
 *	DECDLD - downloadable charset
 *
 *---------------------------------------------------------------------------*/
void
vt_dcsentry(U_char ch, struct video_state *svsp)
{
	switch(svsp->dcs_state)
	{
		case DCS_INIT:
			switch(ch)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':	/* parameters */
					svsp->parms[svsp->parmi] *= 10;
					svsp->parms[svsp->parmi] += (ch -'0');
					break;

				case ';':	/* next parameter */
					svsp->parmi =
						(svsp->parmi+1 < MAXPARMS) ?
						svsp->parmi+1 : svsp->parmi;
					break;

				case '|':	/* DECUDK */
					svsp->transparent = 1;
					init_udk(svsp);
					svsp->dcs_state = DCS_AND_UDK;
					break;

				case '{':	/* DECDLD */
					svsp->transparent = 1;
					init_dld(svsp);
					svsp->dcs_state = DCS_DLD_DSCS;
					break;

				default:	 /* failsafe */
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					break;
			}
			break;

		case DCS_AND_UDK:	 /* DCS ... | */
			switch(ch)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':	/* fkey number */
					svsp->udk_fnckey *= 10;
					svsp->udk_fnckey += (ch -'0');
					break;

				case '/':	/* Key */
					svsp->dcs_state = DCS_UDK_DEF;
					break;

				case 0x1b:	 /* ESC */
					svsp->dcs_state = DCS_UDK_ESC;
					break;

				default:
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					break;
			}
			break;

		case DCS_UDK_DEF:	 /* DCS ... | fnckey / */
			switch(ch)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					if(svsp->udk_deflow)	/* low nibble */
					{
						svsp->udk_def[svsp->udk_defi] |= (ch -'0');
						svsp->udk_deflow = 0;
						svsp->udk_defi = (svsp->udk_defi+1 >= MAXUDKDEF) ?
						svsp->udk_defi : svsp->udk_defi+1;
					}
					else			/* high nibble */
					{
						svsp->udk_def[svsp->udk_defi] = ((ch -'0') << 4);
						svsp->udk_deflow = 1;
					}
					break;

				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
					if(svsp->udk_deflow) 	/* low nibble */
					{
						svsp->udk_def[svsp->udk_defi] |= (ch - 'a' + 10);
						svsp->udk_deflow = 0;
						svsp->udk_defi = (svsp->udk_defi+1 >= MAXUDKDEF) ?
						svsp->udk_defi : svsp->udk_defi+1;
					}
					else			/* high nibble */
					{
						svsp->udk_def[svsp->udk_defi] = ((ch - 'a' + 10) << 4);
						svsp->udk_deflow = 1;
					}
					break;



				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
					if(svsp->udk_deflow) 	/* low nibble */
					{
						svsp->udk_def[svsp->udk_defi] |= (ch - 'A' + 10);
						svsp->udk_deflow = 0;
						svsp->udk_defi = (svsp->udk_defi+1 >= MAXUDKDEF) ?
						svsp->udk_defi : svsp->udk_defi+1;
					}
					else			/* high nibble */
					{
						svsp->udk_def[svsp->udk_defi] = ((ch - 'A' + 10) << 4);
						svsp->udk_deflow = 1;
					}
					break;

				case ';':	/* next function key */
					vt_udk(svsp);
					svsp->dcs_state = DCS_AND_UDK;
					break;

				case 0x1b:	 /* ESC */
					svsp->dcs_state = DCS_UDK_ESC;
					break;

				default:
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					break;
			}
			break;

		case DCS_UDK_ESC:	 /* DCS ... | fkey/def ... ESC */
			switch(ch)
			{
				case '\\':	/* ST */
					vt_udk(svsp);
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					break;

				default:
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					break;
			}
			break;


		case DCS_DLD_DSCS:	 /* got DCS ... { */
			if(ch >= ' ' && ch <= '/')	/* intermediates ... */
			{
				svsp->dld_dscs[svsp->dld_dscsi] = ch;
				svsp->dld_id[svsp->dld_dscsi] = ch;
				if(svsp->dld_dscsi >= DSCS_LENGTH)
				{
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					svsp->dld_id[0] = '\0';
				}
				else
				{
					svsp->dld_dscsi++;
				}
			}
			else if(ch >= '0' && ch <= '~')	/* final .... */
			{
				svsp->dld_dscs[svsp->dld_dscsi] = ch;
				svsp->dld_id[svsp->dld_dscsi++] = ch;
				svsp->dld_id[svsp->dld_dscsi] = '\0';
				svsp->dcs_state = DCS_DLD_DEF;
			}
			else
			{
				svsp->transparent = 0;
				svsp->state = STATE_INIT;
				svsp->dcs_state = DCS_INIT;
				svsp->dld_id[0] = '\0';
			}
			break;

		case DCS_DLD_DEF:	 /* DCS ... { dscs */
			switch(ch)
			{
				case 0x1b:	 /* ESC */
					svsp->dcs_state = DCS_DLD_ESC;
					break;

				case '/':	 /* sixel upper / lower divider */
					svsp->dld_sixel_lower = 1;
					break;

				case ';':	 /* character divider */
					vt_dld(svsp);
					svsp->parms[1]++;	/* next char */
					break;

 				default:
					if (svsp->dld_sixel_lower)
					{
						if(ch >= '?' && ch <= '~')
							svsp->sixel.lower[svsp->dld_sixelli] = ch - '?';
						svsp->dld_sixelli =
						 (svsp->dld_sixelli+1 < MAXSIXEL) ?
						 svsp->dld_sixelli+1 : svsp->dld_sixelli;
					}
					else
					{
						if(ch >= '?' && ch <= '~')
							svsp->sixel.upper[svsp->dld_sixelui] = ch - '?';
						svsp->dld_sixelui =
						 (svsp->dld_sixelui+1 < MAXSIXEL) ?
						 svsp->dld_sixelui+1 : svsp->dld_sixelui;
					}
					break;
			}
			break;

		case DCS_DLD_ESC:	 /* DCS ... { dscs ... / ... ESC */
			switch(ch)
			{
				case '\\':	/* String Terminator ST */
					vt_dld(svsp);
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					break;

 				default:
					svsp->transparent = 0;
					svsp->state = STATE_INIT;
					svsp->dcs_state = DCS_INIT;
					svsp->dld_id[0] = '\0';
					break;
			}
			break;

		default:
			svsp->transparent = 0;
			svsp->state = STATE_INIT;
			svsp->dcs_state = DCS_INIT;
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	User Defineable Keys
 *---------------------------------------------------------------------------*/
void
vt_udk(struct video_state *svsp)
{
	int key, start, max, i;
	int usedff = 0;

	if(svsp->parms[0] != 1)		/* clear all ? */
	{
		vt_clearudk(svsp);
		svsp->parms[0] = 1;
	}

	if(svsp->udk_fnckey < 17 || svsp->udk_fnckey > 34)
	{
		init_udk(svsp);
		return;
	}

	key = svsp->udk_fnckey - 17;	/* index into table */

	if(svsp->ukt.length[key] == 0)			/* never used ? */
	{
		if(svsp->udkff < MAXUDKDEF-2)		/* space available ? */
		{
			start = svsp->udkff;		/* next sequential */
			max = MAXUDKDEF - svsp->udkff;	/* space available */
			svsp->ukt.first[key] = start;	/* start entry */
			usedff = 1;			/* flag to update later */
		}
		else					/* no space */
		{
			init_udk(svsp);
			return;
		}
	}
	else						/* in use, redefine */
	{
		start = svsp->ukt.first[key];		/* start entry */
		max = svsp->ukt.length[key];		/* space available */
	}

	if(max < 2)				/* hmmm .. */
	{
		init_udk(svsp);
		return;
	}

	max--;		/* adjust for tailing '\0' */

	for(i = 0; i < max && i < svsp->udk_defi; i++)
		svsp->udkbuf[start++] = svsp->udk_def[i];

	svsp->udkbuf[start] = '\0';	/* make it a string, see pcvt_kbd.c */
	svsp->ukt.length[key] = i+1;	/* count for tailing '\0' */
	if(usedff)
		svsp->udkff += (i+2);	/* new start location */

	init_udk(svsp);
}

/*---------------------------------------------------------------------------*
 *	clear all User Defineable Keys
 *---------------------------------------------------------------------------*/
void
vt_clearudk(struct video_state *svsp)
{
	register int i;

	for(i = 0; i < MAXUDKEYS; i++)
	{
		svsp->ukt.first[i] = 0;
		svsp->ukt.length[i] = 0;
	}
	svsp->udkff = 0;
}

/*---------------------------------------------------------------------------*
 *	Down line LoaDable Fonts
 *---------------------------------------------------------------------------*/
void
vt_dld(struct video_state *svsp)
{
	unsigned char vgacharset;
	unsigned char vgachar[16];
	unsigned char vgacharb[16];

	if(vgacs[svsp->vga_charset].secondloaded)
		vgacharset = vgacs[svsp->vga_charset].secondloaded;
	else
		return;

	svsp->parms[1] = (svsp->parms[1] < 1) ? 1 :
		((svsp->parms[1] > 0x7E) ? 0x7E : svsp->parms[1]);

	if(svsp->parms[2] != 1)   /* Erase all characters ? */
	{
		clear_dld(svsp);
		svsp->parms[2] = 1;   /* Only erase all characters once per sequence */
	}

	sixel_vga(&(svsp->sixel),vgachar);

	switch(vgacs[vgacharset].char_scanlines & 0x1F)
	{
		case 7:
			vga10_vga8(vgachar,vgacharb);
			break;

		case 9:
		default:
			vga10_vga10(vgachar,vgacharb);
			break;

		case 13:
			vga10_vga14(vgachar,vgacharb);
			break;

		case 15:
			vga10_vga16(vgachar,vgacharb);
			break;
	}

	loadchar(vgacharset, svsp->parms[1] + 0xA0, 16, vgacharb);

	init_dld(svsp);
}

/*---------------------------------------------------------------------------*
 *	select character attributes
 *---------------------------------------------------------------------------*/
void
vt_sca(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 1:
			svsp->selchar = 1;
			break;
		case 0:
		case 2:
		default:
			svsp->selchar = 0;
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	initalize selective attribute bit array
 *---------------------------------------------------------------------------*/
void
vt_initsel(struct video_state *svsp)
{
	register int i;

	for(i = 0;i < MAXDECSCA;i++)
		svsp->decsca[i] = 0;
}

/*---------------------------------------------------------------------------*
 *	DECSEL - selective erase in line
 *---------------------------------------------------------------------------*/
void
vt_sel(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:
			selective_erase(svsp, (svsp->Crtat + svsp->cur_offset),
					 svsp->maxcol-svsp->col);
			break;

		case 1:
			selective_erase(svsp, (svsp->Crtat + svsp->cur_offset)-
					svsp->col, svsp->col + 1);
			break;

		case 2:
			selective_erase(svsp, (svsp->Crtat + svsp->cur_offset)-
					svsp->col, svsp->maxcol);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	DECSED - selective erase in display
 *---------------------------------------------------------------------------*/
void
vt_sed(struct video_state *svsp)
{
	switch(svsp->parms[0])
	{
		case 0:
			selective_erase(svsp, (svsp->Crtat + svsp->cur_offset),
			      svsp->Crtat + (svsp->maxcol * svsp->screen_rows) -
			      (svsp->Crtat + svsp->cur_offset));
			break;

		case 1:
			selective_erase(svsp, svsp->Crtat,
			   (svsp->Crtat + svsp->cur_offset) - svsp->Crtat + 1 );
			break;

		case 2:
			selective_erase(svsp, svsp->Crtat,
				svsp->maxcol * svsp->screen_rows);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	scroll screen n lines up
 *---------------------------------------------------------------------------*/
void
roll_up(struct video_state *svsp, int n)
{

#if (PCVT_NOFASTSCROLL==0)

	if(svsp->scrr_beg == 0 &&	/* if scroll region is whole screen */
           svsp->scrr_len == svsp->screen_rows &&
	   (svsp != vsp ||		/* and either running in memory */
	    (svsp->screen_rows == svsp->screen_rowsize && /* or no fkeys */
	     adaptor_type != MDA_ADAPTOR)))	/* and not on MDA/Hercules */
	{
		u_short *Memory =

#if PCVT_USL_VT_COMPAT
		    (vsp != svsp || (vsp->vt_status & VT_GRAFX)) ?
#else
		    (vsp != svsp) ?
#endif

				svsp->Memory : Crtat;

		if(svsp->Crtat > (Memory + (svsp->screen_rows - n) *
					svsp->maxcol))
		{
			bcopy(svsp->Crtat + svsp->maxcol * n, Memory,
		       	      svsp->maxcol * (svsp->screen_rows - n) * CHR);

			svsp->Crtat = Memory;
		}
		else
		{
			svsp->Crtat += n * svsp->maxcol;
		}

#if PCVT_USL_VT_COMPAT
		if(vsp == svsp && !(vsp->vt_status & VT_GRAFX))
#else
		if(vsp == svsp)
#endif

		{
			outb(addr_6845, CRTC_STARTADRH);
			outb(addr_6845+1, (svsp->Crtat - Crtat) >> 8);
			outb(addr_6845, CRTC_STARTADRL);
			outb(addr_6845+1, (svsp->Crtat - Crtat));
		}
	}
	else
#endif
	{
		bcopy(	svsp->Crtat + ((svsp->scrr_beg + n) * svsp->maxcol),
			svsp->Crtat + (svsp->scrr_beg * svsp->maxcol),
			svsp->maxcol * (svsp->scrr_len - n) * CHR );
	}

	fillw(	user_attr | ' ',
		svsp->Crtat + ((svsp->scrr_end - n + 1) * svsp->maxcol),
		n * svsp->maxcol);

/*XXX*/	if(svsp->scroll_lock && svsp->openf && curproc)
		tsleep((caddr_t)&(svsp->scroll_lock), PPAUSE, "scrlck", 0);
}

/*---------------------------------------------------------------------------*
 *	scroll screen n lines down
 *---------------------------------------------------------------------------*/
static void
roll_down(struct video_state *svsp, int n)
{

#if (PCVT_NOFASTSCROLL==0)

	if(svsp->scrr_beg == 0 &&	/* if scroll region is whole screen */
           svsp->scrr_len == svsp->screen_rows &&
	   (svsp != vsp ||		/* and either running in memory */
	    (svsp->screen_rows == svsp->screen_rowsize && /* or no fkeys */
	     adaptor_type != MDA_ADAPTOR)))	/* and not on MDA/Hercules */
	{
		u_short *Memory =

#if PCVT_USL_VT_COMPAT
		    (vsp != svsp || (vsp->vt_status & VT_GRAFX)) ?
#else
		    (vsp != svsp) ?
#endif
				svsp->Memory : Crtat;

		if (svsp->Crtat < (Memory + n * svsp->maxcol))
		{
			bcopy(svsp->Crtat,
			      Memory + svsp->maxcol * (svsp->screen_rows + n),
		       	      svsp->maxcol * (svsp->screen_rows - n) * CHR);

			svsp->Crtat = Memory + svsp->maxcol * svsp->screen_rows;
		}
		else
		{
			svsp->Crtat -= n * svsp->maxcol;
		}

#if PCVT_USL_VT_COMPAT
		if(vsp == svsp && !(vsp->vt_status & VT_GRAFX))
#else
		if(vsp == svsp)
#endif

		{
			outb(addr_6845, CRTC_STARTADRH);
			outb(addr_6845+1, (svsp->Crtat - Crtat) >> 8);
			outb(addr_6845, CRTC_STARTADRL);
			outb(addr_6845+1, (svsp->Crtat - Crtat));
		}
	}
	else
#endif
	{
		bcopy(  svsp->Crtat + (svsp->scrr_beg * svsp->maxcol),
			svsp->Crtat + ((svsp->scrr_beg + n) * svsp->maxcol),
			svsp->maxcol * (svsp->scrr_len - n) * CHR );
	}

	fillw(	user_attr | ' ',
		svsp->Crtat + (svsp->scrr_beg * svsp->maxcol),
		n * svsp->maxcol);

/*XXX*/	if(svsp->scroll_lock && svsp->openf && curproc)
		tsleep((caddr_t)&(svsp->scroll_lock), PPAUSE, "scrlck", 0);
}

/*---------------------------------------------------------------------------*
 *	switch charset pointers
 *---------------------------------------------------------------------------*/
static void
swcsp(struct video_state *svsp, u_short *ctp)
{
	if(ctp == NULL)
		return;

	switch(svsp->state)
	{
		case STATE_BROPN:	/* designate G0 */
			svsp->G0 = ctp;
			break;

		case STATE_BRCLO:	/* designate G1 */
		case STATE_MINUS:	/* designate G1 (96) */
			svsp->G1 = ctp;
			break;

		case STATE_STAR:	/* designate G2 */
		case STATE_DOT:		/* designate G2 (96) */
			svsp->G2 = ctp;
			break;

		case STATE_PLUS:	/* designate G3 */
		case STATE_SLASH:	/* designate G3 (96) */
			svsp->G3 = ctp;
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	process terminal responses
 *---------------------------------------------------------------------------*/
static void
respond(struct video_state *svsp)
{
        if(!(svsp->openf))              /* are we opened ? */
                return;

        while (*svsp->report_chars && svsp->report_count > 0)
        {
		(*linesw[svsp->vs_tty->t_line].l_rint)
			(*svsp->report_chars++ & 0xff, svsp->vs_tty);
		svsp->report_count--;
        }
}

/*---------------------------------------------------------------------------*
 *	Initialization for User Defineable Keys
 *---------------------------------------------------------------------------*/
static void
init_udk(struct video_state *svsp)
{
	svsp->udk_defi = 0;
	svsp->udk_deflow = 0;
	svsp->udk_fnckey = 0;
}

/*---------------------------------------------------------------------------*
 *	Clear loaded downloadable (DLD) character set
 *---------------------------------------------------------------------------*/
static void
clear_dld(struct video_state *svsp)
{
	register int i;
	unsigned char vgacharset;
	unsigned char vgachar[16];

	if(vgacs[svsp->vga_charset].secondloaded)
		vgacharset = vgacs[svsp->vga_charset].secondloaded;
	else
		return;

	for(i=0;i < 16;i++)  /* A zeroed character, vt220 has inverted '?' */
		vgachar[i] = 0x00;

	for(i=1;i <= 94;i++) /* Load (erase) all characters */
		loadchar(vgacharset, i + 0xA0, 16, vgachar);
}

/*---------------------------------------------------------------------------*
 *	Initialization for Down line LoaDable Fonts
 *---------------------------------------------------------------------------*/
static void
init_dld(struct video_state *svsp)
{
	register int i;

	svsp->dld_dscsi = 0;
	svsp->dld_sixel_lower = 0;
	svsp->dld_sixelli = 0;
	svsp->dld_sixelui = 0;

	for(i = 0;i < MAXSIXEL;i++)
		svsp->sixel.lower[i] = svsp->sixel.upper[i] = 0;
}

/*---------------------------------------------------------------------------*
 *	selective erase a region
 *---------------------------------------------------------------------------*/
static void
selective_erase(struct video_state *svsp, u_short *pcrtat, int length)
{
	register int i, j;

	for(j = pcrtat - svsp->Crtat, i = 0;i < length;i++,pcrtat++)
	{
		if(!(svsp->decsca[INT_INDEX(j+i)] & (1 << BIT_INDEX(j+i))))
		{
			*pcrtat &= 0xFF00; /* Keep the video character attributes */
			*pcrtat += ' ';	   /* Erase the character */
		}
	}
}

#endif	/* NVT > 0 */

/* ------------------------- E O F ------------------------------------------*/

