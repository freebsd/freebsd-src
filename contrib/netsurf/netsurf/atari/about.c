/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "cflib.h"
#include "atari/misc.h"
#include "atari/plot/plot.h"
#include "atari/gemtk/gemtk.h"
#include "atari/res/netsurf.rsh"
#include "atari/about.h"

#include "utils/testament.h"
#include "utils/useragent.h"
#include "desktop/netsurf.h"
#include "utils/nsurl.h"
#include "utils/messages.h"


#include "curl/curlver.h"


static OBJECT * about_form = NULL;
static char * infocontent;
static char version[32];
VdiHdl vdihandle;


static short __CDECL about_userdraw(PARMBLK *parmblock)
{
	short pxy[8];
	short dummy;
	int content_len;
	char *content;
	short cur_x, cur_y;
	short cheight = 8, cwidth = gl_wchar;
	char c[2] = {0,0};

	/* Setup area to the full area...: */
	GRECT area = {
		.g_x = parmblock->pb_x,
		.g_y = parmblock->pb_y,
		.g_w = parmblock->pb_w-8,
		.g_h = parmblock->pb_h
	};

	/* Setup clip: */
	GRECT clip = {
		.g_x = parmblock->pb_xc,
		.g_y = parmblock->pb_yc,
		.g_w = parmblock->pb_wc,
		.g_h = parmblock->pb_hc
	};

	if(parmblock->pb_currstate == parmblock->pb_prevstate){

		content = (char*)parmblock->pb_parm;
		content_len = strlen(content);
		cur_x = area.g_x;
		cur_y = area.g_y;


		if(!rc_intersect(&area, &clip)){
			return (0);
		}

		pxy[0] = clip.g_x;
		pxy[1] = clip.g_y;
		pxy[2] = pxy[0] + clip.g_w-1;
		pxy[3] = pxy[1] + clip.g_h-1;
		vs_clip(vdihandle, 1, pxy);
		vst_alignment(vdihandle, 0, 5, &dummy, &dummy);
		vst_height(vdihandle, sys_sml_height, &dummy, &dummy, &dummy, &cheight);
		vswr_mode(vdihandle, 2);

		for (int i=0; i<content_len; i++) {
			if (content[i] == '\n') {
				cur_y += cheight;
				cur_x = area.g_x;
			} else {
				if (cur_x >= clip.g_x
						&& cur_x < (clip.g_x + clip.g_w)
							&& cur_y > clip.g_y
								&& cur_y < (clip.g_w + clip.g_h)) {
					c[0] = content[i];
					v_gtext(vdihandle, cur_x, cur_y, c);
				}
				cur_x += cwidth;
			}
		}

		vs_clip(vdihandle, 0, pxy);
	}
	return(0);
}

void atari_about_show(void)
{
	static USERBLK userblk;
	short elem = 0;
	const char * goto_url = NULL;
	nserror nserr;
	nsurl *url;

	vdihandle = plot_get_vdi_handle();

	infocontent = malloc(8000);
	memset(infocontent, 0, 8000);

	snprintf(infocontent, 8000,
			"Netsurf  : %s\n"
			"Build ID : %s\n"
			"Date     : %s\n"
			"MiNTLib  : %d.%d-%d%s\n"
			"GEMLib   : %d.%d-%d%s\n"
			"CFLib    : %d.%d-%d%s\n"
			"cURL     : %s\n",
			user_agent_string(),
			WT_REVID,
			WT_COMPILEDATE,
			__MINTLIB_MAJOR__, __MINTLIB_MINOR__, __MINTLIB_REVISION__,
			__MINTLIB_BETATAG__,
			__GEMLIB_MAJOR__, __GEMLIB_MINOR__, __GEMLIB_REVISION__,
			__GEMLIB_BETATAG__,
			__CFLIB_MAJOR__, __CFLIB_MINOR__, __CFLIB_REVISION__,
			__CFLIB_BETATAG__,
			LIBCURL_VERSION);

	about_form = gemtk_obj_get_tree(ABOUT);
	snprintf(version, 32, "%s%s", "NetSurf ", (char*)netsurf_version);
	set_string(about_form, ABOUT_LBL_VERSION, version);

	userblk.ub_code = about_userdraw;
    userblk.ub_parm = (long) infocontent;
    about_form[ABOUT_CONTENT].ob_spec.userblk = &userblk;

	elem = simple_dial(about_form, 0);
	switch (elem) {
			case ABOUT_CREDITS:
				goto_url = "about:credits";
			break;

			case ABOUT_LICENSE:
				goto_url = "about:licence";
			break;
	};

	free(infocontent);

	if (goto_url != NULL) {
		nserr = nsurl_create(goto_url, &url);
		if (nserr == NSERROR_OK) {
			nserr = browser_window_create(BW_CREATE_HISTORY,
				    url,
				    NULL,
				    NULL,
				    NULL);
			nsurl_unref(url);
		}
		if (nserr != NSERROR_OK) {
			warn_user(messages_get_errorcode(nserr), 0);
		}
	}
/*
    dlg = open_mdial(about_form, 0);
    do {
		elem = do_mdial(dlg);
		printf ("elem: %d\n", elem);
		switch (elem) {
			case ABOUT_OK:
				close_dlg = true;
			break;

			case ABOUT_CREDITS:
				close_dlg = true;
			break;

			case ABOUT_LICENSE:
				close_dlg = true;
			break;
		}
    } while (close_dlg == false);

    close_mdial(dlg);
*/

}
