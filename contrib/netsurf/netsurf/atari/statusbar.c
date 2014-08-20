/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"

#include "atari/gui.h"
#include "atari/statusbar.h"
#include "atari/rootwin.h"
#include "atari/misc.h"

#include "atari/res/netsurf.rsh"
#include "atari/plot/plot.h"
#include "atari/osspec.h"

#ifdef WITH_CUSTOM_STATUSBAR
extern int atari_plot_vdi_handle;

static
void __CDECL evnt_sb_redraw( COMPONENT *c, long buff[8] )
{
	size_t i;
	struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app, c, CDT_OWNER);
	assert(gw != NULL);
	CMP_STATUSBAR sb = gw->root->statusbar;
	assert( sb != NULL );
	if( sb == NULL )
		return;

	if( sb->attached == false )
		return;

	LGRECT work, lclip;
	short pxy[8], d, pxyclip[4];

	mt_CompGetLGrect(&app, sb->comp, WF_WORKXYWH, &work);
	lclip = work;
	if ( !rc_lintersect( (LGRECT*)&buff[4], &lclip ) ) {
		return;
	}
	vsf_interior(atari_plot_vdi_handle, FIS_SOLID );
	vsl_color(atari_plot_vdi_handle, BLACK );
	vsl_type(atari_plot_vdi_handle, 1);
	vsl_width(atari_plot_vdi_handle, 1 );
	vst_color(atari_plot_vdi_handle, BLACK);

	vst_height(atari_plot_vdi_handle, atari_sysinfo.medium_sfont_pxh, &pxy[0], &pxy[1], &pxy[2], &pxy[3] );
	vst_alignment(atari_plot_vdi_handle, 0, 5, &d, &d );
	vst_effects(atari_plot_vdi_handle, 0 );
	pxyclip[0] = lclip.g_x;
	pxyclip[1] = lclip.g_y;
	pxyclip[2] = lclip.g_x + lclip.g_w-1;
	pxyclip[3] = lclip.g_y + lclip.g_h-1;

	vs_clip(atari_plot_vdi_handle, 1, (short*)&pxyclip );
	vswr_mode(atari_plot_vdi_handle, MD_REPLACE );

	if( lclip.g_y <= work.g_y ) {
		pxy[0] = work.g_x;
		pxy[1] = work.g_y;
		pxy[2] = MIN( work.g_x + work.g_w, lclip.g_x + lclip.g_w );
		pxy[3] = work.g_y;
		v_pline(atari_plot_vdi_handle, 2, (short*)&pxy );
	}

	if(app.nplanes > 2) {
		vsf_color(atari_plot_vdi_handle, LWHITE);
	} else {
		vsf_color(atari_plot_vdi_handle, WHITE );
	}

	pxy[0] = work.g_x;
	pxy[1] = work.g_y+1;
	pxy[2] = work.g_x + work.g_w-1;
	pxy[3] = work.g_y + work.g_h-1;
	v_bar(atari_plot_vdi_handle, pxy );


	if( sb->textlen > 0 ) {
		short curx;
		short vqw[4];
		char t[2];
		short cw = 8;
		t[1]=0;
		if( atari_sysinfo.sfont_monospaced ) {
			t[0]='A';
			int r = vqt_width(atari_plot_vdi_handle, t[0], &vqw[0], &vqw[1], &vqw[2] );
			cw = vqw[0];
		}
		vswr_mode(atari_plot_vdi_handle, MD_TRANS );
		for( curx = work.g_x + 2, i=0 ; (curx+cw < work.g_x+work.g_w ) && i < sb->textlen; i++ ){
			t[0] = sb->text[i];
			if( !atari_sysinfo.sfont_monospaced ) {
				vqt_width(atari_plot_vdi_handle, t[0], &vqw[0], &vqw[1], &vqw[2] );
				cw = vqw[0];
			}
			if( curx >= lclip.g_x - cw ) {
				v_gtext(atari_plot_vdi_handle, curx, work.g_y + 5, (char*)&t );
			}
			curx += cw;
			if( curx >= lclip.g_x + lclip.g_w )
				break;
		}
	}
	vswr_mode(atari_plot_vdi_handle, MD_REPLACE );
	pxy[0] = work.g_x + work.g_w;
	pxy[1] = work.g_y + work.g_h;
	pxy[2] = work.g_x + work.g_w;
	pxy[3] = work.g_y + work.g_h-work.g_h;
	v_pline(atari_plot_vdi_handle, 2, (short*)&pxy );

	vs_clip(atari_plot_vdi_handle, 0, (short*)&pxyclip );
	return;
}

static void __CDECL evnt_sb_click( COMPONENT *c, long buff[8] )
{
	static bool prevstate;
	short sbuff[8], mx, my;
	LGRECT work;
	mt_CompGetLGrect(&app, c, WF_WORKXYWH, &work);
	if( evnt.mx >= work.g_x + (work.g_w) && evnt.mx <= work.g_x + work.g_w &&
		evnt.my >= work.g_y + (work.g_h) && evnt.my <= work.g_y + work.g_h ) {
		// click within sb button
	}
}

CMP_STATUSBAR sb_create( struct gui_window * gw )
{
	CMP_STATUSBAR s = malloc( sizeof(struct s_statusbar) );
	s->attached = false;
	s->comp = (COMPONENT*)mt_CompCreate(&app, CLT_HORIZONTAL, STATUSBAR_HEIGHT, 0);
	s->comp->rect.g_h = STATUSBAR_HEIGHT;
	s->comp->bounds.max_height = STATUSBAR_HEIGHT;
	mt_CompDataAttach( &app, s->comp, CDT_OWNER, gw );
	mt_CompEvntAttach( &app, s->comp, WM_REDRAW, evnt_sb_redraw );
	mt_CompEvntAttach( &app, s->comp, WM_XBUTTON, evnt_sb_click );
	sb_set_text( s, (char*)"" );
	return( s );
}

void sb_destroy( CMP_STATUSBAR s )
{
	LOG(("%s\n", __FUNCTION__ ));
	if( s ) {
		if( s->comp ){
			mt_CompDelete( &app, s->comp );
		}
		free( s );
	}
}

void sb_set_text(CMP_STATUSBAR sb , const char * text)
{

	LGRECT work;
	assert( sb != NULL );
	assert( sb->comp != NULL );
	strncpy( (char*)&sb->text, text, STATUSBAR_MAX_SLEN );
	sb->text[STATUSBAR_MAX_SLEN]=0;
	sb->textlen = strlen( (char*)&sb->text );
	if( sb->attached ){
		struct gui_window * gw = (struct gui_window *)mt_CompDataSearch(&app, sb->comp, CDT_OWNER);
		if( gw != NULL ){
			mt_CompGetLGrect(&app, sb->comp, WF_WORKXYWH, &work);
			ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
						work.g_x, work.g_y, work.g_w, work.g_h );
		}
	}
}

#else

CMP_STATUSBAR sb_create( struct gui_window * gw )
{
	CMP_STATUSBAR s = malloc( sizeof(struct s_statusbar) );
	s->attached = false;
	sb_set_text( s, (char*)"" );
	return( s );
}

void sb_destroy( CMP_STATUSBAR s )
{
	LOG(("%s\n", __FUNCTION__ ));
	if( s ) {
		free( s );
	}
}

void sb_attach(CMP_STATUSBAR sb, struct gui_window * gw)
{
	sb->aes_win = gemtk_wm_get_handle(gw->root->win);
	sb->attached = true;
}

void sb_set_text(CMP_STATUSBAR sb, const char * text )
{
	assert( sb != NULL );
	strncpy(sb->text, text, STATUSBAR_MAX_SLEN);
	sb->text[STATUSBAR_MAX_SLEN]=0;
	sb->textlen = strlen(sb->text);
	if(sb->attached){
		wind_set_str(sb->aes_win, WF_INFO, sb->text);
	}
}

#endif
