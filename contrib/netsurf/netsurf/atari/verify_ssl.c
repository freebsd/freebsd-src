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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <windom.h>

#include "utils/errors.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "content/urldb.h"
#include "content/fetch.h"
#include "atari/res/netsurf.rsh"
#include "atari/verify_ssl.h"

/*
   todo: this file need to use the treeview api - complete rework,
   current implementation is not used in any way.
*/

extern void * h_gem_rsrc;
extern short atari_plot_vdi_handle;


#define CERT_INF_LINES 8

static struct ssl_info_draw_param
{
	struct ssl_cert_info * cert_infos_n;
	unsigned long num_certs;
	int current;
	int scrollx;
	int cols;
	int scrolly;
	int rows;		/* assumed to be 8 */
	OBJECT * tree;
} dp;


static int cert_display_width( struct ssl_cert_info * cert_info )
{
	int l1, l2;
	int max=0;
	int i;
	int add = 16; /* strlen("Issuer:         "); */

	l1 = strlen(cert_info->issuer) + add;
	l2 = strlen(cert_info->subject) + add;
	return( MAX(l1, l2) );
}


static void __CDECL cert_info_draw( WINDOW * win, short buf[8], void * data)
{
	struct ssl_info_draw_param * dp = (struct ssl_info_draw_param *)data;
	GRECT work;
	short pxy[4];
	int maxchars;
	short d, cbh, cbw;
	int i = 0;
	short x,y,w,h;
	int px_ypos, px_xpos;
	char * line = malloc(512);
	if( line == NULL )
		return;

	LOG(("Cert info draw, win: %p, data: %p, scrollx: %d", win, data, dp->scrollx ));

	WindGet( win, WF_WORKXYWH, &x, &y, &w, &h );
	/*using static values here, as RsrcUserDraw has mem leaks & a very small stack */
	pxy[0] = work.g_x = x + 8;
	pxy[1] = work.g_y = y + 80;
	pxy[2] = x + 8 + 272;
	pxy[3] = y + 80 + 176;
	work.g_w = 272;
	work.g_h = 176;

	maxchars = (work.g_w / 8)+1;
	vs_clip( atari_plot_vdi_handle, 1,(short*) &pxy );
	vswr_mode( atari_plot_vdi_handle, MD_REPLACE );
	vsf_interior( atari_plot_vdi_handle, 1 );
	vsf_color( atari_plot_vdi_handle, LWHITE );
	v_bar( atari_plot_vdi_handle, (short*)&pxy );
	vst_height( atari_plot_vdi_handle, 16, &d, &d, &cbw, &cbh );
	vst_alignment(atari_plot_vdi_handle, 0, 5, &d, &d );
	vst_color( atari_plot_vdi_handle, BLACK );
	vst_effects( atari_plot_vdi_handle, 0 );
	px_ypos = px_xpos = 0;
	for(i=0; i<CERT_INF_LINES; i++ ) {
		switch( i ) {
			case 0:
				sprintf(line, "Cert Version:   %d", dp->cert_infos_n[dp->current].version  );
				break;

			case 1:
				sprintf(line, "Invalid before: %s", &dp->cert_infos_n[dp->current].not_before );
				break;

			case 2:
				sprintf(line, "Invalid after:  %s", &dp->cert_infos_n[dp->current].not_after );
				break;

			case 3:
				sprintf(line, "Signature type: %d", dp->cert_infos_n[dp->current].sig_type );
				break;

			case 4:
				sprintf(line, "Serial:         %d", dp->cert_infos_n[dp->current].serial );
				break;

			case 5:
				sprintf(line, "Issuer:         %s", &dp->cert_infos_n[dp->current].issuer );
				break;

			case 6:
				sprintf(line, "Subject:        %s", &dp->cert_infos_n[dp->current].subject );
				break;

			case 7:
				sprintf(line, "Cert type:      %d", dp->cert_infos_n[dp->current].cert_type );
				break;

			default:
				break;
		}
		if( (int)strlen(line) > dp->scrollx ) {
			if( dp->scrollx + maxchars < 511 && ( (signed int)strlen(line) - dp->scrollx) > maxchars )
				line[dp->scrollx + maxchars] = 0;
			v_gtext(atari_plot_vdi_handle, work.g_x + 1, work.g_y + px_ypos, &line[dp->scrollx]);
		}
		px_ypos += cbh;
	}
	vst_alignment(atari_plot_vdi_handle, 0, 0, &d, &d );
	vs_clip( atari_plot_vdi_handle, 0, (short*)&pxy );
	free( line );
}


static void do_popup( WINDOW *win, int index, int mode, void *data)
{
	struct ssl_info_draw_param * dp = (struct ssl_info_draw_param *)data;
	char * items[dp->num_certs];
	short x, y;
	unsigned int i;
	int dispw;
	LOG(("do_popup: num certs: %d", dp->num_certs));
	for( i = 0; i<dp->num_certs; i++) {
		items[i] = malloc( 48 );
		strncpy(items[i], (char*)&dp->cert_infos_n[i].issuer, 46 );
	}
	objc_offset( FORM(win), index, &x, &y );
	dp->current = MenuPopUp( items, x, y,
			   dp->num_certs, MIN( 3, dp->num_certs), 0,
			   P_LIST + P_WNDW + P_CHCK );
	ObjcChange( OC_FORM, win, index, NORMAL, TRUE );
	dp->cols = cert_display_width( &dp->cert_infos_n[dp->current] );
	dp->rows = 8;
	dp->scrollx = 0;
	dp->scrolly = 0;

	/* Send (!) redraw ( OC_MSG ) */
	ObjcDrawParent( OC_FORM, FORM(win), VERIFY_BOX_DETAILS, 1, 7 | OC_MSG );
	for( i = 0; i<dp->num_certs; i++) {
		free( items[i] );
	}
}



bool verify_ssl_form_do( const char * url, const struct ssl_cert_info * cert_infos_n ,
	unsigned long num_certs )
{
	OBJECT *tree;
	WINDOW * form;

	bool bres = false;
	bool cont = true;
	int res = 0;
	dp.cert_infos_n = (struct ssl_cert_info *)cert_infos_n;
	dp.num_certs = num_certs;
	dp.scrollx = 0;
	dp.scrolly = 0;
	dp.current = 0;
	dp.cols = cert_display_width( &dp.cert_infos_n[dp.current] );
	dp.rows = 8;
	dp.tree = tree;

	RsrcGaddr (h_gem_rsrc , R_TREE, VERIFY, &tree);
	ObjcString( tree, VERIFY_LBL_HOST, (char*)url );
	ObjcChange( OC_OBJC, tree, VERIFY_BT_ACCEPT, 0, 0 );
	ObjcChange( OC_OBJC, tree, VERIFY_BT_REJECT, 0, 0 );
	form = FormWindBegin( tree, (char*)"SSL Verify failed"  );
	EvntDataAdd( form, WM_REDRAW, cert_info_draw, (void*)&dp, EV_BOT );
	/* this results in some extended objects which can not be freed: :( */
	/* RsrcUserDraw( OC_FORM, tree, VERIFY_BOX_DETAILS, cert_info_draw,(void*)&dp ) ; */
	ObjcAttachFormFunc( form, VERIFY_BT_NEXT_CERT, do_popup, &dp );
	/*
	ObjcAttachFormFunc( form, VERIFY_BT_NEXT_CERT, do_popup, &dp );
	ObjcAttachFormFunc( form, VERIFY_BT_NEXT_CERT, do_popup, &dp );
	*/
	while( cont ) {
		res = FormWindDo( MU_MESAG );
		cont = false;
		switch( res ){
			case VERIFY_BT_ACCEPT:
				bres = true;
				break;

			case VERIFY_BT_NEXT_CERT:
			/* select box clicked or dragged... */
				cont = true;
				break;

			case VERIFY_BT_REJECT:
				bres = false;
				break;

			case VERIFY_BT_SCROLL_D:
					cont = true;
					dp.scrolly += 1;
					ObjcDrawParent( OC_FORM, form, VERIFY_BOX_DETAILS, 1, 7 | OC_MSG );
				break;

			case VERIFY_BT_SCROLL_U:
					cont = true;
					dp.scrolly -= 1;
					ObjcDrawParent( OC_FORM, form, VERIFY_BOX_DETAILS, 1, 7 | OC_MSG );
				break;

			case VERIFY_BT_SCROLL_R:
					LOG(( "scroll r!" ));
					cont = true;
					dp.scrollx += 1;
					if( dp.scrollx > (dp.cols - (272 / 8 )) )
						dp.scrollx -= 1;
					ObjcDrawParent( OC_FORM, form, VERIFY_BOX_DETAILS, 1, 7 | OC_MSG);
				break;

			case VERIFY_BT_SCROLL_L:
					cont = true;
					dp.scrollx -= 1;
					if( dp.scrollx < 0 )
						dp.scrollx = 0;
					ObjcDrawParent( OC_FORM, form, VERIFY_BOX_DETAILS, 1, 7 | OC_MSG );
				break;

			default:
				break;
		}
	}
	FormWindEnd( );
  return( bres );
}
