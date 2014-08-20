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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mint/osbind.h>
#include <cflib.h>

#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/textinput.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "atari/gui.h"
#include "atari/rootwin.h"
#include "atari/misc.h"
#include "atari/clipboard.h"
#include "utils/nsoption.h"
#include "atari/res/netsurf.rsh"
#include "atari/ctxmenu.h"


#define CNT_INVALID 0
#define CNT_BROWSER 64
#define CNT_HREF 128
#define CNT_SELECTION 256
#define CNT_INTERACTIVE 512
#define CNT_IMG 1024

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy);

struct s_context_info {
	unsigned long flags;
	struct contextual_content ccdata;
};

struct s_context_info ctxinfo;

static struct s_context_info * get_context_info( struct gui_window * gw, short mx, short my )
{
	hlcache_handle *h;
	GRECT area;
	struct contextual_content ccdata;
	struct browser_window * bw = gw->browser->bw;
	int sx, sy;

	h = bw->current_content;
	ctxinfo.flags = 0;

	window_get_grect(gw->root, BROWSER_AREA_CONTENT, &area);
	if (POINT_WITHIN(mx, my, area)) {

		mx -= area.g_x;
		my -= area.g_y;

		if (!bw->current_content || content_get_type(h) != CONTENT_HTML){
			return(&ctxinfo);
		}

		ctxinfo.flags |= CNT_BROWSER;

		memset( &ctxinfo.ccdata, sizeof(struct contextual_content), 0 );

		gui_window_get_scroll(gw, &sx, &sy);

		browser_window_get_contextual_content( gw->browser->bw, mx+sx, my+sy,
				(struct contextual_content*)&ctxinfo.ccdata);

		if( ctxinfo.ccdata.link_url ){
			ctxinfo.flags |= CNT_HREF;
		}
		if( ctxinfo.ccdata.object) {
			if( content_get_type(ctxinfo.ccdata.object) == CONTENT_IMAGE ){
				ctxinfo.flags |= CNT_IMG;
			}
		}
		if ( ctxinfo.ccdata.form_features == CTX_FORM_TEXT )
			ctxinfo.flags |= (CNT_INTERACTIVE | CNT_SELECTION);
	}

	return(&ctxinfo);


}

/***
 * Generates an unique filename for temp. files
 * The generated filename includes the path.
 * If TMPDIR environmenrt vairable is set, this will be used as path,
 * otherwise U:\tmp\
 * \param prefix
 * \param sufffix
 * \return pointer to static buffer owned by get_tmpfilename()
 */
static char * get_tmpfilename(const char * prefix, const char * suffix)
{
    int i=0;
    static char tmpfilename[PATH_MAX];
    char * tmpdir;
    const char * tmp_path_suffix = "";

    // TODO: make function public?
    tmpdir = getenv("TMPDIR");
    if(tmpdir == NULL){
        tmpdir = (char*)"u:\\tmp\\";
    }

    if(tmpdir[strlen(tmpdir)-1] != '\\'){
        tmp_path_suffix = "\\";
    }

    do{
        /* generate a new filename: */
        snprintf(tmpfilename, PATH_MAX, "%s%s%s%d%s", tmpdir,
                 tmp_path_suffix, prefix, i++, suffix);
        /* check with cflib: */
    } while(file_exists(tmpfilename));

    return(tmpfilename);
}

//TODO: do not open popup for gui_window, but for a rootwin?
void context_popup(struct gui_window * gw, short x, short y)
{

#define POP_FIRST_ITEM POP_CTX_CUT_SEL
#define POP_LAST_ITEM POP_CTX_SAVE_LINK_AS

	OBJECT * pop;
	int choice;
	struct s_context_info * ctx;
	unsigned long size;
	const char * data;
	FILE * fp_tmpfile;
	char cmdline[128];
	/* skip first byte, which must hold length of commandline: */
	char * tempfile = &cmdline[1];
	int err = 0;
	char * editor, *lastslash;
	MENU pop_menu, me_data;

	pop = gemtk_obj_get_tree( POP_CTX );
	if (pop == NULL)
        	return;
	ctx = get_context_info(gw, x, y);

    /*
        Disable all items by default:
    */
	for( choice = POP_FIRST_ITEM; choice<=POP_LAST_ITEM; choice++ ){
		SET_BIT(pop[ choice ].ob_state, OS_DISABLED, 1);
	}

	if( ctx->flags & CNT_INTERACTIVE ){
        	SET_BIT(pop[ POP_CTX_PASTE_SEL ].ob_state, OS_DISABLED, 0);
    	}

	if( (ctx->flags & CNT_BROWSER) ){
		SET_BIT(pop[ POP_CTX_SELECT_ALL ].ob_state, OS_DISABLED, 0);
		SET_BIT(pop[ POP_CTX_COPY_SEL ].ob_state, OS_DISABLED, 0);
		SET_BIT(pop[ POP_CTX_VIEW_SOURCE ].ob_state, OS_DISABLED, 0);
	}

	if( ctx->flags & CNT_HREF ){
		SET_BIT(pop[ POP_CTX_COPY_LINK ].ob_state, OS_DISABLED, 0);
        SET_BIT(pop[ POP_CTX_OPEN_NEW ].ob_state, OS_DISABLED, 0);
        SET_BIT(pop[ POP_CTX_SAVE_LINK_AS ].ob_state, OS_DISABLED, 0);
	}

	if( ctx->flags & CNT_IMG ){
		SET_BIT(pop[ POP_CTX_SAVE_AS ].ob_state, OS_DISABLED, 0);
		SET_BIT(pop[ POP_CTX_COPY_URL ].ob_state, OS_DISABLED, 0);
		SET_BIT(pop[ POP_CTX_OPEN_NEW ].ob_state, OS_DISABLED, 0);
	}

	// point mn_tree tree to states popup:
    pop_menu.mn_tree = gemtk_obj_get_tree(POP_CTX);
    pop_menu.mn_menu = 0;
    pop_menu.mn_item = POP_CTX_CUT_SEL;
    pop_menu.mn_scroll = SCROLL_NO;
    pop_menu.mn_keystate = 0;

	menu_popup(&pop_menu, x, y, &me_data);
    choice = me_data.mn_item;

	switch( choice ){
		case POP_CTX_COPY_SEL:
			browser_window_key_press(gw->browser->bw, KEY_COPY_SELECTION);
		break;

		case POP_CTX_CUT_SEL:
			browser_window_key_press(gw->browser->bw, KEY_CUT_SELECTION);
		break;

		case POP_CTX_PASTE_SEL:
			browser_window_key_press(gw->browser->bw, KEY_PASTE);
		break;

		case POP_CTX_SELECT_ALL:
			browser_window_key_press(gw->browser->bw, KEY_SELECT_ALL);
		break;

		case POP_CTX_SAVE_AS:
			if (ctx->ccdata.object != NULL) {
				if( hlcache_handle_get_url(ctx->ccdata.object) != NULL ) {
					browser_window_navigate(
						gw->browser->bw,
						hlcache_handle_get_url(ctx->ccdata.object),
						hlcache_handle_get_url(gw->browser->bw->current_content),
						BW_NAVIGATE_DOWNLOAD,
						NULL,
						NULL,
						NULL
						);
				}
			}

		case POP_CTX_SAVE_LINK_AS:
			if (ctx->ccdata.link_url != NULL) {
				nsurl *url;
				nserror error;

				error = nsurl_create(ctx->ccdata.link_url, &url);
				if (error == NSERROR_OK) {
					error = browser_window_navigate(
						gw->browser->bw,
						url,
						hlcache_handle_get_url(gw->browser->bw->current_content),
						BW_NAVIGATE_DOWNLOAD,
						NULL,
						NULL,
						NULL
						);
						nsurl_unref(url);
				}
				if (error != NSERROR_OK) {
					warn_user(messages_get_errorcode(error), 0);
				}
			}

		break;

		case POP_CTX_COPY_URL:
			if ((ctx->flags & CNT_IMG) && (ctx->ccdata.object != NULL)) {
				if( hlcache_handle_get_url(ctx->ccdata.object) != NULL ){
					scrap_txt_write((char*)nsurl_access(
							hlcache_handle_get_url(ctx->ccdata.object)));
				}
			}
		break;

		case POP_CTX_COPY_LINK:
			if ((ctx->flags & CNT_HREF) && ctx->ccdata.link_url != NULL) {
				scrap_txt_write((char*)ctx->ccdata.link_url);
			}
		break;

		case POP_CTX_OPEN_NEW:
			if ((ctx->flags & CNT_HREF) && ctx->ccdata.link_url) {
				nsurl *url;
				nserror error;

				error = nsurl_create(ctx->ccdata.link_url, &url);
				if (error == NSERROR_OK) {
					error = browser_window_create(
							BW_CREATE_HISTORY | BW_CREATE_CLONE,
							url,
							hlcache_handle_get_url(gw->browser->bw->current_content),
								      gw->browser->bw,
								      NULL
						);
					nsurl_unref(url);
				}
				if (error != NSERROR_OK) {
					warn_user(messages_get_errorcode(error), 0);
				}
			}
		break;

		case POP_CTX_VIEW_SOURCE:
			editor = nsoption_charp(atari_editor);
			if (editor != NULL && strlen(editor)>0) {
				data = content_get_source_data(gw->browser->bw->current_content,
												&size);
				if (size > 0 && data != NULL){
				    snprintf(tempfile, 127, "%s", get_tmpfilename("ns-", ".html"));
				    /* the GEMDOS cmdline contains the length of the commandline
				       in the first byte: */
				    cmdline[0] = (unsigned char)strlen(tempfile);
					LOG(("Creating temporay source file: %s\n", tempfile));
					fp_tmpfile = fopen(tempfile, "w");
					if (fp_tmpfile != NULL){
						fwrite(data, size, 1, fp_tmpfile);
						fclose(fp_tmpfile);

						// Send SH_WDRAW to notify files changed:
						gemtk_send_msg(SH_WDRAW, 0, -1, 0, 0, 0, 0);

                        // start application:
						if(strlen(tempfile)<=125){
							shel_write(1, 1, 1, editor, cmdline);
						}
					} else {
						printf("Could not open temp file: %s!\n", tempfile );
					}

				} else {
					LOG(("Invalid content!"));
				}
			} else {
				form_alert(0, "[1][Set option \"atari_editor\".][OK]");
			}
		break;

		default: break;
	}

#undef POP_FIRST_ITEM
#undef POP_LAST_ITEM

}
