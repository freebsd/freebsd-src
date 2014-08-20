/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#ifndef NETSURF_BEOS_SCAFFOLDING_H
#define NETSURF_BEOS_SCAFFOLDING_H 1

#include <View.h>
#include <Window.h>
#include <NetPositive.h>

extern "C" {
#include "desktop/plotters.h"
}

typedef struct beos_scaffolding nsbeos_scaffolding;

class NSBaseView : public BView {
public:
		NSBaseView(BRect frame);
		NSBaseView(BMessage *archive);
virtual	~NSBaseView();

virtual void	MessageReceived(BMessage *message);
//virtual void	Draw(BRect updateRect);

//virtual void	FrameMoved(BPoint new_location);
//virtual void	FrameResized(float new_width, float new_height);

virtual void		AllAttached(void);

virtual status_t	Archive(BMessage *archive, bool deep=true) const;
static BArchivable	*Instantiate(BMessage *archive);

void			SetScaffolding(struct beos_scaffolding *scaf);
private:
	struct beos_scaffolding *fScaffolding;
};

class NSBrowserWindow : public BWindow {
public:
		NSBrowserWindow(BRect frame, struct beos_scaffolding *scaf);
virtual	~NSBrowserWindow();

virtual	void	DispatchMessage(BMessage *message, BHandler *handler);
virtual void	MessageReceived(BMessage *message);
virtual bool	QuitRequested(void);
void			WindowActivated(bool active);

struct beos_scaffolding *Scaffolding() const { return fScaffolding; };

static BWindow* activeWindow;
private:
	struct beos_scaffolding *fScaffolding;


};


// XXX: clean up
typedef enum {

	/* no/unknown actions */
	NO_ACTION = 'nsMA',

	/* help actions */
	HELP_OPEN_CONTENTS,
	HELP_OPEN_GUIDE,
	HELP_OPEN_INFORMATION,
	HELP_OPEN_ABOUT,
	HELP_LAUNCH_INTERACTIVE,

	/* history actions */
	HISTORY_SHOW_LOCAL,
	HISTORY_SHOW_GLOBAL,

	/* hotlist actions */
	HOTLIST_ADD_URL,
	HOTLIST_SHOW,

	/* cookie actions */
	COOKIES_SHOW,
	COOKIES_DELETE,

	/* page actions */
	BROWSER_PAGE,
	BROWSER_PAGE_INFO,
	BROWSER_PRINT,
	BROWSER_NEW_WINDOW,
	BROWSER_VIEW_SOURCE,

	/* object actions */
	BROWSER_OBJECT,
	BROWSER_OBJECT_INFO,
	BROWSER_OBJECT_RELOAD,

	/* save actions */
	BROWSER_OBJECT_SAVE,
	BROWSER_OBJECT_EXPORT_SPRITE,
	BROWSER_OBJECT_SAVE_URL_URI,
	BROWSER_OBJECT_SAVE_URL_URL,
	BROWSER_OBJECT_SAVE_URL_TEXT,
	BROWSER_SAVE,
	BROWSER_SAVE_COMPLETE,
	BROWSER_EXPORT_DRAW,
	BROWSER_EXPORT_TEXT,
	BROWSER_SAVE_URL_URI,
	BROWSER_SAVE_URL_URL,
	BROWSER_SAVE_URL_TEXT,
	HOTLIST_EXPORT,
	HISTORY_EXPORT,

	/* navigation actions */
	BROWSER_NAVIGATE_HOME,
	BROWSER_NAVIGATE_BACK,
	BROWSER_NAVIGATE_FORWARD,
	BROWSER_NAVIGATE_UP,
	BROWSER_NAVIGATE_RELOAD,
	BROWSER_NAVIGATE_RELOAD_ALL,
	BROWSER_NAVIGATE_STOP,
	BROWSER_NAVIGATE_URL,

	/* browser window/display actions */
	BROWSER_SCALE_VIEW,
	BROWSER_FIND_TEXT,
	BROWSER_IMAGES_FOREGROUND,
	BROWSER_IMAGES_BACKGROUND,
	BROWSER_BUFFER_ANIMS,
	BROWSER_BUFFER_ALL,
	BROWSER_SAVE_VIEW,
	BROWSER_WINDOW_DEFAULT,
	BROWSER_WINDOW_STAGGER,
	BROWSER_WINDOW_COPY,
	BROWSER_WINDOW_RESET,

	/* tree actions */
	TREE_NEW_FOLDER,
	TREE_NEW_LINK,
	TREE_EXPAND_ALL,
	TREE_EXPAND_FOLDERS,
	TREE_EXPAND_LINKS,
	TREE_COLLAPSE_ALL,
	TREE_COLLAPSE_FOLDERS,
	TREE_COLLAPSE_LINKS,
	TREE_SELECTION,
	TREE_SELECTION_EDIT,
	TREE_SELECTION_LAUNCH,
	TREE_SELECTION_DELETE,
	TREE_SELECT_ALL,
	TREE_CLEAR_SELECTION,

	/* toolbar actions */
	TOOLBAR_BUTTONS,
	TOOLBAR_ADDRESS_BAR,
	TOOLBAR_THROBBER,
	TOOLBAR_EDIT,

	/* misc actions */
	CHOICES_SHOW,
	ABOUT_BUTTON,
	APPLICATION_QUIT,
} menu_action;


NSBrowserWindow *nsbeos_find_last_window(void);

NSBrowserWindow *nsbeos_get_bwindow_for_scaffolding(nsbeos_scaffolding *scaffold);

NSBaseView *nsbeos_get_baseview_for_scaffolding(nsbeos_scaffolding *scaffold);

nsbeos_scaffolding *nsbeos_new_scaffolding(struct gui_window *toplevel);

bool nsbeos_scaffolding_is_busy(nsbeos_scaffolding *scaffold);

void nsbeos_attach_toplevel_view(nsbeos_scaffolding *g, BView *view);


void nsbeos_scaffolding_dispatch_event(nsbeos_scaffolding *scaffold, BMessage *message);

void nsbeos_scaffolding_destroy(nsbeos_scaffolding *scaffold);

//void nsbeos_window_destroy_event(NSBrowserWindow *window, nsbeos_scaffolding *g, BMessage *event);


void nsbeos_scaffolding_popup_menu(nsbeos_scaffolding *g, BPoint where);

void gui_window_set_title(struct gui_window *_g, const char *title);
void gui_window_set_url(struct gui_window *_g, const char *url);
void gui_window_set_icon(struct gui_window *_g, hlcache_handle *icon);
void gui_window_set_status(struct gui_window *_g, const char *text);
void gui_window_start_throbber(struct gui_window* _g);
void gui_window_stop_throbber(struct gui_window* _g);

#endif /* NETSURF_BEOS_SCAFFOLDING_H */
