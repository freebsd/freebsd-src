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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <mint/osbind.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/cookie_manager.h"
#include "desktop/mouse.h"
#include "desktop/tree.h"
#include "utils/nsoption.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/log.h"
#include "content/fetch.h"

#include "atari/gui.h"
#include "atari/toolbar.h"

#include "atari/misc.h"
#include "atari/encoding.h"
#include "atari/gemtk/gemtk.h"
#include "cflib.h"

extern void * h_gem_rsrc;

struct is_process_running_callback_data {
	const char * fname;
	bool found;
};

void warn_user(const char *warning, const char *detail)
{
	size_t len = 1 + ((warning != NULL) ? strlen(messages_get(warning)) :
			0) + ((detail != 0) ? strlen(detail) : 0);
	char message[len];
	snprintf(message, len, messages_get(warning), detail);

	printf("%s\n", message);
	gemtk_msg_box_show(GEMTK_MSG_BOX_ALERT, message);
}

void die(const char *error)
{
	printf("%s\n", error);
	gemtk_msg_box_show(GEMTK_MSG_BOX_ALERT, error);
	exit(1);
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */

bool path_add_part(char *path, int length, const char *newpart)
{
	if(path[strlen(path) - 1] != '/')
		strncat(path, "/", length);

	strncat(path, newpart, length);

	return true;
}

struct gui_window * find_guiwin_by_aes_handle(short handle){

	struct gui_window * gw;
	gw = window_list;

	if( handle == 0 ){
		return( NULL );
	}

	while(gw != NULL) {
		if(gw->root->win != NULL
			&& gemtk_wm_get_handle(gw->root->win) == handle) {
				return(gw);
		}
		else
			gw = gw->next;
	}

	return( NULL );
}


static int scan_process_list(scan_process_callback cb, void *data)
{
	int pid, count = 0;
	DIR	*dir;
	char*dirname;
	struct dirent *de;

	if (( dir = opendir("U:/kern")) == NULL)
		return(0);

	while ((de = readdir( dir)) != NULL) {
		dirname = de->d_name;

		if( dirname[0] != '1' && dirname[0] != '2' && dirname[0] != '3' && dirname[0] != '4' && dirname[0] != '5'
		 && dirname[0] != '6' && dirname[0] != '7' && dirname[0] != '8' && dirname[0] != '9')
			continue;

		count++;
		if (cb != NULL) {
			/* when callback returns negative value, we stop scanning: */
			pid = atoi(dirname);
			if (cb(pid, data)<0) {
				break;
			}
		}
	}

	closedir(dir);

	return(count);
}

static int proc_running_callback(int pid, void * arg)
{
	char buf[PATH_MAX], fnamepath[256];
	FILE *fp;
	int nread;
	struct is_process_running_callback_data *data;

	data = (struct is_process_running_callback_data *)arg;

	sprintf(fnamepath, "U:\\kern\\%d\\fname", pid);
	printf("checking: %s\n", fnamepath);

	fp = fopen(fnamepath, "r");
	if(!fp)
		return(0);

	nread = fread(buf, 1, PATH_MAX-1, fp);
	fclose(fp);
	nread = MIN(PATH_MAX-1, nread);

	if (nread > 0) {
		buf[nread] = 0;

		char *lastslash = strrchr(buf, '/');

		if(lastslash == NULL)
			lastslash = strrchr(buf, '\\');

		if(lastslash==NULL)
			lastslash = buf;
		else
			lastslash++;

		if(strcasecmp(lastslash, data->fname)==0){
			/* found process, check status: */
			sprintf(fnamepath, "U:\\kern\\%d\\status", pid);
			fp = fopen(fnamepath, "r");
			if (fp) {
				nread = fread(buf, 1, PATH_MAX-1, fp);
				fclose(fp);
				if (nread>0) {
					nread = MIN(PATH_MAX-1,nread);
				}
				buf[nread] = 0;
				if (strstr(buf, "zombie")==NULL) {
					data->found = true;
					return(-1);
				}
			}

		}
	}
	return(0);
}

bool is_process_running(const char * name)
{
	struct is_process_running_callback_data data = {name, false};

	scan_process_list(proc_running_callback, &data);

	return( (data.found==1) ? true : false );
}


/**
 * Callback for load_icon(). Should be removed once bitmaps get loaded directly
 * from disc
 */
static nserror load_icon_callback(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	return NSERROR_OK;
}


/**
 * utility function. Copied from NetSurf tree API.
 *
 * \param name	the name of the loaded icon, if it's not a full path the icon is
 *		looked for in the directory specified by icons_dir
 * \return the icon in form of a content or NULL on failure
 */
hlcache_handle *load_icon(const char *name, hlcache_handle_callback cb,
						void * pw )
{
	char *url = NULL;
	const char *icon_url = NULL;
	int len;
	hlcache_handle *c;
	nserror err;
	nsurl *icon_nsurl;
	char * icons_dir = nsoption_charp(tree_icons_path);

	/** @todo something like bitmap_from_disc is needed here */

	if (!strncmp(name, "file://", 7)) {
		icon_url = name;
	} else {
		char *native_path;

		if (icons_dir == NULL)
			return NULL;

		/* path + separator + leafname + '\0' */
		len = strlen(icons_dir) + 1 + strlen(name) + 1;
		native_path = malloc(len);
		if (native_path == NULL) {
			LOG(("malloc failed"));
			warn_user("NoMemory", 0);
			return NULL;
		}

		/* Build native path */
		memcpy(native_path, icons_dir,
		       strlen(icons_dir) + 1);
		path_add_part(native_path, len, name);

		/* Convert native path to URL */
		url = path_to_url(native_path);

		free(native_path);
		icon_url = url;
	}

	err = nsurl_create(icon_url, &icon_nsurl);
	if (err != NSERROR_OK) {
		if (url != NULL)
			free(url);
		return NULL;
	}

	/* Fetch the icon */
	err = hlcache_handle_retrieve(icon_nsurl, 0, 0, 0,
				      ((cb != NULL) ? cb : load_icon_callback), pw, 0,
				      CONTENT_IMAGE, &c);

	nsurl_unref(icon_nsurl);

	/* If we built the URL here, free it */
	if (url != NULL)
		free(url);

	if (err != NSERROR_OK) {
		return NULL;
	}

	return c;
}

void gem_set_cursor( MFORM_EX * cursor )
{
	static unsigned char flags = 255;
	static int number = 255;
	if( flags == cursor->flags && number == cursor->number )
		return;
	if( cursor->flags & MFORM_EX_FLAG_USERFORM ) {
		gemtk_obj_mouse_sprite(cursor->tree, cursor->number);
	} else {
		graf_mouse(cursor->number, NULL );
	}
	number = cursor->number;
	flags = cursor->flags;
}

/**
 * Convert NKC (atari normalized key code) to netsurf
 * Input key code and/or to ucs4 (depends on keycode).
 * When the input key can not be found for the NKC,
 * the function will return 0 and fill ucs4_out with
 * the NKC converted to UC4 encoding.
*/
long nkc_to_input_key(short nkc, long * ucs4_out)
{
	unsigned char ascii = (nkc & 0xFF);
	long ik = 0;

	// initialize result:
	*ucs4_out = 0;

	// sanitize input key:
	nkc = (nkc & (NKF_CTRL|NKF_SHIFT|0xFF));

	/* shift + cntrl key: */
	if( ((nkc & NKF_CTRL) == NKF_CTRL) && ((nkc & (NKF_SHIFT))!=0) ) {

	}
	/* cntrl key only: */
	else if( (nkc & NKF_CTRL) == NKF_CTRL ) {
		switch ( ascii ) {
			case 'A':
				ik = KEY_SELECT_ALL;
			break;

			case 'C':
				ik = KEY_COPY_SELECTION;
			break;

			case 'X':
				ik = KEY_CUT_SELECTION;
			break;

			case 'V':
				ik = KEY_PASTE;
			break;

			default:
			break;
		}
	}
	/* shift key only: */
	else if( (nkc & NKF_SHIFT) != 0 ) {
		switch( ascii ) {
			case NK_TAB:
				ik = KEY_SHIFT_TAB;
			break;

			case NK_LEFT:
				ik = KEY_LINE_START;
			break;

			case NK_RIGHT:
				ik = KEY_LINE_END;
			break;

			case NK_UP:
				ik = KEY_PAGE_UP;
			break;

			case NK_DOWN:
				ik = KEY_PAGE_DOWN;
			break;

			default:
			break;
		}
	}
	/* No modifier keys: */
	else {
		switch( ascii ) {

			case NK_INS:
				ik = KEY_PASTE;
				break;

			case NK_BS:
				ik = KEY_DELETE_LEFT;
			break;

			case NK_DEL:
				ik = KEY_DELETE_RIGHT;
			break;

			case NK_TAB:
				ik = KEY_TAB;
			break;


			case NK_ENTER:
				ik = KEY_NL;
			break;

			case NK_RET:
				ik = KEY_CR;
			break;

			case NK_ESC:
				ik = KEY_ESCAPE;
			break;

			case NK_CLRHOME:
				ik = KEY_TEXT_START;
			break;

			case NK_RIGHT:
				ik = KEY_RIGHT;
			break;

			case NK_LEFT:
				ik = KEY_LEFT;
			break;

			case NK_UP:
				ik = KEY_UP;
			break;

			case NK_UNDO:
				ik = KEY_UNDO;
			break;

			case NK_DOWN:
				ik = KEY_DOWN;
			break;

			case NK_M_PGUP:
				ik = KEY_PAGE_UP;
			break;

			case NK_M_PGDOWN:
				ik = KEY_PAGE_DOWN;
			break;

			default:
			break;
		}
	}

	if( ik == 0 && ( (nkc & NKF_CTRL)==0)  ) {
		if (ascii >= 9 ) {
			*ucs4_out = atari_to_ucs4(ascii);
		}
	}
	return ( ik );
}

/**
 * Show default file selector
 *
 * \param title  The selector title.
 * \param name	 Default file name
 * \return a static char pointer or null if the user aborted the selection.
 */
const char * file_select(const char * title, const char * name) {

	static char path[PATH_MAX]=""; // First usage : current directory
	static char fullname[PATH_MAX]="";
	char tmpname[255];
	char * use_title = (char*)title;

	if( strlen(name)>254)
		return( NULL );

	strcpy(tmpname, name);

	if( use_title == NULL ){
		use_title = (char*)"";
	}

	if (select_file(path, tmpname, (char*)"*", use_title, NULL)) {
		snprintf(fullname, PATH_MAX, "%s%s", path, tmpname);
		return((const char*)&fullname);
	}

	return( NULL );
}


void dbg_grect(const char * str, GRECT * r)
{
	printf("%s: x: %d, y: %d, w: %d, h: %d (x2: %d, y2: %d)\n", str,
		r->g_x, r->g_y, r->g_w, r->g_h, r->g_x + r->g_w, r->g_y + r->g_h);
}

void dbg_pxy(const char * str, short * pxy )
{
	printf("%s: x: %d, y: %d, w: %d, h: %d\n", str,
		pxy[0], pxy[1], pxy[2], pxy[3] );
}

void dbg_rect(const char * str, int * pxy)
{
	printf("%s: x0: %d, y0: %d, x1: %d, y1: %d (w: %d, h: %d)\n", str,
		pxy[0], pxy[1], pxy[2], pxy[3],
		pxy[2] - pxy[0],
		pxy[3] - pxy[1] );
}

