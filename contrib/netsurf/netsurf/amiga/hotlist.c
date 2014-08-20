/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include "amiga/hotlist.h"
#include "amiga/tree.h"
#include "desktop/hotlist.h"

struct ami_hotlist_ctx {
	void *userdata;
	int level;
	int item;
	const char *folder; /* folder we're interested in */
	bool in_menu; /* set if we are in that folder */
	bool found; /* set if the folder is found */
	bool (*cb)(void *userdata, int level, int item, const char *title, nsurl *url, bool folder);
};


void ami_hotlist_initialise(const char *hotlist_file)
{
	tree_hotlist_path = hotlist_file;
	hotlist_window = ami_tree_create(TREE_HOTLIST, NULL);

	if(!hotlist_window) return;
}

void ami_hotlist_free(const char *hotlist_file)
{
	ami_tree_destroy(hotlist_window);
	hotlist_window = NULL;
}


static nserror ami_hotlist_folder_enter_cb(void *ctx, const char *title)
{
	struct ami_hotlist_ctx *menu_ctx = (struct ami_hotlist_ctx *)ctx;

	if(menu_ctx->in_menu == true) {
		if(menu_ctx->cb(menu_ctx->userdata, menu_ctx->level, menu_ctx->item, title, NULL, true) == true)
			menu_ctx->item++;
	} else {
		if((menu_ctx->level == 0) && (strcmp(title, menu_ctx->folder) == 0)) {
			menu_ctx->in_menu = true;
			menu_ctx->found = true;
		}
	}
	menu_ctx->level++;
	return NSERROR_OK;
}

static nserror ami_hotlist_address_cb(void *ctx, nsurl *url, const char *title)
{
	struct ami_hotlist_ctx *menu_ctx = (struct ami_hotlist_ctx *)ctx;

	if(menu_ctx->in_menu == true) {
		if(menu_ctx->cb(menu_ctx->userdata, menu_ctx->level, menu_ctx->item, title, url, false) == true)
			menu_ctx->item++;
	}
	
	return NSERROR_OK;
}

static nserror ami_hotlist_folder_leave_cb(void *ctx)
{
	struct ami_hotlist_ctx *menu_ctx = (struct ami_hotlist_ctx *)ctx;

	menu_ctx->level--;

	if((menu_ctx->in_menu == true) && (menu_ctx->level == 0))
		menu_ctx->in_menu = false;

	return NSERROR_OK;
}

nserror ami_hotlist_scan(void *userdata, int first_item, const char *folder,
	bool (*cb_add_item)(void *userdata, int level, int item, const char *title, nsurl *url, bool folder))
{
	nserror error;
	struct ami_hotlist_ctx ctx;

	ctx.level = 0;
	ctx.item = first_item;
	ctx.folder = folder;
	ctx.in_menu = false;
	ctx.userdata = userdata;
	ctx.cb = cb_add_item;
	ctx.found = false;

	error = hotlist_iterate(&ctx,
		ami_hotlist_folder_enter_cb,
		ami_hotlist_address_cb,
		ami_hotlist_folder_leave_cb);

	if((error == NSERROR_OK) && (ctx.found == false))
		hotlist_add_folder(folder, false, 0);

	return error;
}
