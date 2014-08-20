/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#define __STDBOOL_H__	1
#include <assert.h>
extern "C" {
#include "content/urldb.h"
#include "css/utils.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "utils/nsoption.h"
#include "desktop/netsurf.h"
#include "desktop/textinput.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/types.h"
#include "utils/utf8.h"
#include "utils/utils.h"
}
#include "beos/about.h"
#include "beos/window.h"
#include "beos/font.h"
#include "beos/gui.h"
#include "beos/scaffolding.h"
#include "beos/plotters.h"
//#include "beos/schedule.h"

#include <AppDefs.h>
#include <BeBuild.h>
#include <Clipboard.h>
#include <Cursor.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <ScrollBar.h>
#include <String.h>
#include <String.h>
#include <TextView.h>
#include <View.h>
#include <Window.h>

class NSBrowserFrameView;

struct gui_window {
	/* All gui_window objects have an ultimate scaffold */
	nsbeos_scaffolding	*scaffold;
	bool	toplevel;
	/* A gui_window is the rendering of a browser_window */
	struct browser_window	*bw;

	struct {
		int pressed_x;
		int pressed_y;
		int state; /* browser_mouse_state */
	} mouse;

	/* These are the storage for the rendering */
	int			caretx, carety, careth;
	gui_pointer_shape	current_pointer;
	int			last_x, last_y;

	NSBrowserFrameView	*view;

	// some cached events to speed up things
	// those are the last queued event of their kind,
	// we can safely drop others and avoid wasting cpu.
	// number of pending resizes
	int32				pending_resizes;
	// accumulated rects of pending redraws
	//volatile BMessage	*lastRedraw;
	// UNUSED YET
	BRect				pendingRedraw;

	/* Keep gui_windows in a list for cleanup later */
	struct gui_window	*next, *prev;
};



static const rgb_color kWhiteColor = {255, 255, 255, 255};

static struct gui_window *window_list = 0;	/**< first entry in win list*/

static BString current_selection;
static BList current_selection_textruns;

/* Methods which apply only to a gui_window */
static void nsbeos_window_expose_event(BView *view, gui_window *g, BMessage *message);
static void nsbeos_window_keypress_event(BView *view, gui_window *g, BMessage *event);
static void nsbeos_window_resize_event(BView *view, gui_window *g, BMessage *event);
static void nsbeos_window_moved_event(BView *view, gui_window *g, BMessage *event);
/* Other useful bits */
static void nsbeos_redraw_caret(struct gui_window *g);


// #pragma mark - class NSBrowserFrameView


NSBrowserFrameView::NSBrowserFrameView(BRect frame, struct gui_window *gui)
	: BView(frame, "NSBrowserFrameView", B_FOLLOW_ALL_SIDES, 
		B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS ),
	fGuiWindow(gui)
{
}


NSBrowserFrameView::~NSBrowserFrameView()
{
}


void
NSBrowserFrameView::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_ARGV_RECEIVED:
		case B_REFS_RECEIVED:
		case B_COPY:
		case B_CUT:
		case B_PASTE:
		case B_SELECT_ALL:
		//case B_MOUSE_WHEEL_CHANGED:
		case B_UI_SETTINGS_CHANGED:
		// NetPositive messages
		case B_NETPOSITIVE_OPEN_URL:
		case B_NETPOSITIVE_BACK:
		case B_NETPOSITIVE_FORWARD:
		case B_NETPOSITIVE_HOME:
		case B_NETPOSITIVE_RELOAD:
		case B_NETPOSITIVE_STOP:
		case B_NETPOSITIVE_DOWN:
		case B_NETPOSITIVE_UP:
		// messages for top-level
		case 'back':
		case 'forw':
		case 'stop':
		case 'relo':
		case 'home':
		case 'urlc':
		case 'urle':
		case 'menu':
		case NO_ACTION:
		case HELP_OPEN_CONTENTS:
		case HELP_OPEN_GUIDE:
		case HELP_OPEN_INFORMATION:
		case HELP_OPEN_ABOUT:
		case HELP_LAUNCH_INTERACTIVE:
		case HISTORY_SHOW_LOCAL:
		case HISTORY_SHOW_GLOBAL:
		case HOTLIST_ADD_URL:
		case HOTLIST_SHOW:
		case COOKIES_SHOW:
		case COOKIES_DELETE:
		case BROWSER_PAGE:
		case BROWSER_PAGE_INFO:
		case BROWSER_PRINT:
		case BROWSER_NEW_WINDOW:
		case BROWSER_VIEW_SOURCE:
		case BROWSER_OBJECT:
		case BROWSER_OBJECT_INFO:
		case BROWSER_OBJECT_RELOAD:
		case BROWSER_OBJECT_SAVE:
		case BROWSER_OBJECT_EXPORT_SPRITE:
		case BROWSER_OBJECT_SAVE_URL_URI:
		case BROWSER_OBJECT_SAVE_URL_URL:
		case BROWSER_OBJECT_SAVE_URL_TEXT:
		case BROWSER_SAVE:
		case BROWSER_SAVE_COMPLETE:
		case BROWSER_EXPORT_DRAW:
		case BROWSER_EXPORT_TEXT:
		case BROWSER_SAVE_URL_URI:
		case BROWSER_SAVE_URL_URL:
		case BROWSER_SAVE_URL_TEXT:
		case HOTLIST_EXPORT:
		case HISTORY_EXPORT:
		case BROWSER_NAVIGATE_HOME:
		case BROWSER_NAVIGATE_BACK:
		case BROWSER_NAVIGATE_FORWARD:
		case BROWSER_NAVIGATE_UP:
		case BROWSER_NAVIGATE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
		case BROWSER_NAVIGATE_STOP:
		case BROWSER_NAVIGATE_URL:
		case BROWSER_SCALE_VIEW:
		case BROWSER_FIND_TEXT:
		case BROWSER_IMAGES_FOREGROUND:
		case BROWSER_IMAGES_BACKGROUND:
		case BROWSER_BUFFER_ANIMS:
		case BROWSER_BUFFER_ALL:
		case BROWSER_SAVE_VIEW:
		case BROWSER_WINDOW_DEFAULT:
		case BROWSER_WINDOW_STAGGER:
		case BROWSER_WINDOW_COPY:
		case BROWSER_WINDOW_RESET:
		case TREE_NEW_FOLDER:
		case TREE_NEW_LINK:
		case TREE_EXPAND_ALL:
		case TREE_EXPAND_FOLDERS:
		case TREE_EXPAND_LINKS:
		case TREE_COLLAPSE_ALL:
		case TREE_COLLAPSE_FOLDERS:
		case TREE_COLLAPSE_LINKS:
		case TREE_SELECTION:
		case TREE_SELECTION_EDIT:
		case TREE_SELECTION_LAUNCH:
		case TREE_SELECTION_DELETE:
		case TREE_SELECT_ALL:
		case TREE_CLEAR_SELECTION:
		case TOOLBAR_BUTTONS:
		case TOOLBAR_ADDRESS_BAR:
		case TOOLBAR_THROBBER:
		case TOOLBAR_EDIT:
		case CHOICES_SHOW:
		case APPLICATION_QUIT:
			Window()->DetachCurrentMessage();
			nsbeos_pipe_message_top(message, NULL, fGuiWindow->scaffold);
			break;
		default:
			//message->PrintToStream();
			BView::MessageReceived(message);
	}
}


void
NSBrowserFrameView::Draw(BRect updateRect)
{
	BMessage *message = NULL;
	//message = Window()->DetachCurrentMessage();
	// might be called directly...
	if (message == NULL)
		message = new BMessage(_UPDATE_);
	message->AddRect("rect", updateRect);
	nsbeos_pipe_message(message, this, fGuiWindow);
}



void
NSBrowserFrameView::FrameResized(float new_width, float new_height)
{
	BMessage *message = Window()->DetachCurrentMessage();
	// discard any other pending resize, 
	// so we don't end up processing them all, the last one matters.
	atomic_add(&fGuiWindow->pending_resizes, 1);
	nsbeos_pipe_message(message, this, fGuiWindow);
	BView::FrameResized(new_width, new_height);
}


void
NSBrowserFrameView::KeyDown(const char *bytes, int32 numBytes)
{
	BMessage *message = Window()->DetachCurrentMessage();
	nsbeos_pipe_message(message, this, fGuiWindow);
}


void
NSBrowserFrameView::MouseDown(BPoint where)
{
	BMessage *message = Window()->DetachCurrentMessage();
	BPoint screenWhere;
	if (message->FindPoint("screen_where", &screenWhere) < B_OK) {
		screenWhere = ConvertToScreen(where);
		message->AddPoint("screen_where", screenWhere);
	}
	nsbeos_pipe_message(message, this, fGuiWindow);
}


void
NSBrowserFrameView::MouseUp(BPoint where)
{
	//BMessage *message = Window()->DetachCurrentMessage();
	//nsbeos_pipe_message(message, this, fGuiWindow);
	BMessage *message = Window()->DetachCurrentMessage();
	BPoint screenWhere;
	if (message->FindPoint("screen_where", &screenWhere) < B_OK) {
		screenWhere = ConvertToScreen(where);
		message->AddPoint("screen_where", screenWhere);
	}
	nsbeos_pipe_message(message, this, fGuiWindow);
}


void
NSBrowserFrameView::MouseMoved(BPoint where, uint32 transit, const BMessage *msg)
{
	if (transit != B_INSIDE_VIEW) {
		BView::MouseMoved(where, transit, msg);
		return;
	}
	BMessage *message = Window()->DetachCurrentMessage();
	nsbeos_pipe_message(message, this, fGuiWindow);
}


// #pragma mark - gui_window

struct browser_window *nsbeos_get_browser_window(struct gui_window *g)
{
	return g->bw;
}

nsbeos_scaffolding *nsbeos_get_scaffold(struct gui_window *g)
{
	return g->scaffold;
}

struct browser_window *nsbeos_get_browser_for_gui(struct gui_window *g)
{
	return g->bw;
}

float nsbeos_get_scale_for_gui(struct gui_window *g)
{
	return g->bw->scale;
}

/* Create a gui_window */
static struct gui_window *gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *g;		/**< what we're creating to return */

	g = (struct gui_window *)malloc(sizeof(*g));
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}

	LOG(("Creating gui window %p for browser window %p", g, bw));

	g->bw = bw;
	g->mouse.state = 0;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (existing != NULL)
		bw->scale = existing->bw->scale;
	else
		bw->scale = (float) nsoption_int(scale) / 100;

	g->careth = 0;
	g->pending_resizes = 0;

	/* Attach ourselves to the list (push_top) */
	if (window_list)
		window_list->prev = g;
	g->next = window_list;
	g->prev = NULL;
	window_list = g;

	/* Now construct and attach a scaffold */
	g->scaffold = nsbeos_new_scaffolding(g);
	if (!g->scaffold)
		return NULL;

	/* Construct our primary elements */
	BRect frame(0,0,-1,-1); // will be resized later
	g->view = new NSBrowserFrameView(frame, g);
	/* set the default background colour of the drawing area to white. */
	//g->view->SetViewColor(kWhiteColor);
	/* NOOO! Since we defer drawing (DetachCurrent()), the white flickers,
	 * besides sometimes text was drawn twice, making it ugly.
	 * Instead we set to transparent here, and implement plot_clg() to 
	 * do it just before the rest. This almost removes the flicker. */
	g->view->SetViewColor(B_TRANSPARENT_COLOR);
	g->view->SetLowColor(kWhiteColor);

#ifdef B_BEOS_VERSION_DANO
	/* enable double-buffering on the content view */
/*
	XXX: doesn't really work
	g->view->SetDoubleBuffering(B_UPDATE_INVALIDATED
		| B_UPDATE_SCROLLED
		//| B_UPDATE_RESIZED
		| B_UPDATE_EXPOSED);
*/
#endif


	g->toplevel = true;

	/* Attach our viewport into the scaffold */
	nsbeos_attach_toplevel_view(g->scaffold, g->view);


	return g;
}


void nsbeos_dispatch_event(BMessage *message)
{
	struct gui_window *gui = NULL;
	NSBrowserFrameView *view = NULL;
	struct beos_scaffolding *scaffold = NULL;
	NSBrowserWindow *window = NULL;

	//message->PrintToStream();
	if (message->FindPointer("View", (void **)&view) < B_OK)
		view = NULL;
	if (message->FindPointer("gui_window", (void **)&gui) < B_OK)
		gui = NULL;
	if (message->FindPointer("Window", (void **)&window) < B_OK)
		window = NULL;
	if (message->FindPointer("scaffolding", (void **)&scaffold) < B_OK)
		scaffold = NULL;

	struct gui_window *z;
	for (z = window_list; z && gui && z != gui; z = z->next)
		continue;

	struct gui_window *y;
	for (y = window_list; y && scaffold && y->scaffold != scaffold; y = y->next)
		continue;

	if (gui && gui != z) {
		LOG(("discarding event for destroyed gui_window"));
		delete message;
		return;
	}
	if (scaffold && (!y || scaffold != y->scaffold)) {
		LOG(("discarding event for destroyed scaffolding"));
		delete message;
		return;
	}

	// messages for top-level
	if (scaffold) {
		LOG(("dispatching to top-level"));
		nsbeos_scaffolding_dispatch_event(scaffold, message);
		delete message;
		return;
	}

	//LOG(("processing message"));
	switch (message->what) {
		case B_QUIT_REQUESTED:
			// from the BApplication
			netsurf_quit = true;
			break;
		case B_ABOUT_REQUESTED:
		{
			if (gui == NULL)
				gui = window_list;
			nsbeos_about(gui);
			break;
		}
		case _UPDATE_:
			if (gui && view)
				nsbeos_window_expose_event(view, gui, message);
			break;
		case B_MOUSE_MOVED:
		{
			if (gui == NULL)
				break;

			BPoint where;
			int32 mods;
			// where refers to Window coords !?
			// check be:view_where first
			if (message->FindPoint("be:view_where", &where) < B_OK) {
				if (message->FindPoint("where", &where) < B_OK)
					break;
			}
			if (message->FindInt32("modifiers", &mods) < B_OK)
				mods = 0;


			if (gui->mouse.state & BROWSER_MOUSE_PRESS_1) {
				/* Start button 1 drag */
				browser_window_mouse_click(gui->bw, BROWSER_MOUSE_DRAG_1,
					gui->mouse.pressed_x, gui->mouse.pressed_y);
				/* Replace PRESS with HOLDING and declare drag in progress */
				gui->mouse.state ^= (BROWSER_MOUSE_PRESS_1 |
					BROWSER_MOUSE_HOLDING_1);
				gui->mouse.state |= BROWSER_MOUSE_DRAG_ON;
			} else if (gui->mouse.state & BROWSER_MOUSE_PRESS_2) {
				/* Start button 2 drag */
				browser_window_mouse_click(gui->bw, BROWSER_MOUSE_DRAG_2,
					gui->mouse.pressed_x, gui->mouse.pressed_y);
				/* Replace PRESS with HOLDING and declare drag in progress */
				gui->mouse.state ^= (BROWSER_MOUSE_PRESS_2 |
					BROWSER_MOUSE_HOLDING_2);
				gui->mouse.state |= BROWSER_MOUSE_DRAG_ON;
			}

			bool shift = mods & B_SHIFT_KEY;
			bool ctrl = mods & B_CONTROL_KEY;

			/* Handle modifiers being removed */
			if (gui->mouse.state & BROWSER_MOUSE_MOD_1 && !shift)
				gui->mouse.state ^= BROWSER_MOUSE_MOD_1;
			if (gui->mouse.state & BROWSER_MOUSE_MOD_2 && !ctrl)
				gui->mouse.state ^= BROWSER_MOUSE_MOD_2;

			browser_window_mouse_track(gui->bw, (browser_mouse_state)gui->mouse.state, 
					(int)(where.x / gui->bw->scale),
					(int)(where.y / gui->bw->scale));

			gui->last_x = (int)where.x;
			gui->last_y = (int)where.y;
			break;
		}
		case B_MOUSE_DOWN:
		{
			if (gui == NULL)
				break;

			BPoint where;
			int32 buttons;
			int32 mods;
			BPoint screenWhere;
			if (message->FindPoint("be:view_where", &where) < B_OK) {
				if (message->FindPoint("where", &where) < B_OK)
					break;
			}
			if (message->FindInt32("buttons", &buttons) < B_OK)
				break;
			if (message->FindPoint("screen_where", &screenWhere) < B_OK)
				break;
			if (message->FindInt32("modifiers", &mods) < B_OK)
				mods = 0;

			if (buttons & B_SECONDARY_MOUSE_BUTTON) {
				/* 2 == right button on BeOS */
				
				nsbeos_scaffolding_popup_menu(gui->scaffold, screenWhere);
				break;
			}

			gui->mouse.state = BROWSER_MOUSE_PRESS_1;

			if (buttons & B_TERTIARY_MOUSE_BUTTON) /* 3 == middle button on BeOS */
				gui->mouse.state = BROWSER_MOUSE_PRESS_2;

			if (mods & B_SHIFT_KEY)
				gui->mouse.state |= BROWSER_MOUSE_MOD_1;
			if (mods & B_CONTROL_KEY)
				gui->mouse.state |= BROWSER_MOUSE_MOD_2;

			gui->mouse.pressed_x = where.x / gui->bw->scale;
			gui->mouse.pressed_y = where.y / gui->bw->scale;

			// make sure the view is in focus
			if (view && view->LockLooper()) {
				if (!view->IsFocus())
					view->MakeFocus();
				view->UnlockLooper();
			}

			browser_window_mouse_click(gui->bw, 
				(browser_mouse_state)gui->mouse.state,
				gui->mouse.pressed_x, gui->mouse.pressed_y);

			break;
		}
		case B_MOUSE_UP:
		{
			if (gui == NULL)
				break;

			BPoint where;
			int32 buttons;
			int32 mods;
			BPoint screenWhere;
			if (message->FindPoint("be:view_where", &where) < B_OK) {
				if (message->FindPoint("where", &where) < B_OK)
					break;
			}
			if (message->FindInt32("buttons", &buttons) < B_OK)
				break;
			if (message->FindPoint("screen_where", &screenWhere) < B_OK)
				break;
			if (message->FindInt32("modifiers", &mods) < B_OK)
				mods = 0;

			/* If the mouse state is PRESS then we are waiting for a release to emit
			 * a click event, otherwise just reset the state to nothing*/
			if (gui->mouse.state & BROWSER_MOUSE_PRESS_1) 
				gui->mouse.state ^= (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_CLICK_1);
			else if (gui->mouse.state & BROWSER_MOUSE_PRESS_2)
				gui->mouse.state ^= (BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_CLICK_2);

			bool shift = mods & B_SHIFT_KEY;
			bool ctrl = mods & B_CONTROL_KEY;

			/* Handle modifiers being removed */
			if (gui->mouse.state & BROWSER_MOUSE_MOD_1 && !shift)
				gui->mouse.state ^= BROWSER_MOUSE_MOD_1;
			if (gui->mouse.state & BROWSER_MOUSE_MOD_2 && !ctrl)
				gui->mouse.state ^= BROWSER_MOUSE_MOD_2;

			/*
			if (view && view->LockLooper()) {
				view->MakeFocus();
				view->UnlockLooper();
			}
			*/

			if (gui->mouse.state & (BROWSER_MOUSE_CLICK_1|BROWSER_MOUSE_CLICK_2))
				browser_window_mouse_click(gui->bw, 
					(browser_mouse_state)gui->mouse.state, 
					where.x / gui->bw->scale,
					where.y / gui->bw->scale);
			else 
				browser_window_mouse_track(gui->bw, (browser_mouse_state)0, 
					where.x, where.y);

			gui->mouse.state = 0;

			break;
		}
		case B_KEY_DOWN:
			if (gui && view)
				nsbeos_window_keypress_event(view, gui, message);
			break;
		case B_VIEW_RESIZED:
			if (gui && view)
				nsbeos_window_resize_event(view, gui, message);
			break;
		case B_VIEW_MOVED:
			if (gui && view)
				nsbeos_window_moved_event(view, gui, message);
			break;
		case B_MOUSE_WHEEL_CHANGED:
			break;
		case B_UI_SETTINGS_CHANGED:
			nsbeos_update_system_ui_colors();
			break;
		case 'nsLO': // login
		{
			nsurl* url;
			BString realm;
			BString auth;
			void* cbpw;
			nserror (*cb)(bool proceed, void* pw);

			if (message->FindPointer("URL", (void**)&url) < B_OK)
				break;
			if (message->FindString("Realm", &realm) < B_OK)
				break;
			if (message->FindString("Auth", &auth) < B_OK)
				break;
			if (message->FindPointer("callback", (void**)&cb) < B_OK)
				break;
			if (message->FindPointer("callback_pw", (void**)&cbpw) < B_OK)
				break;
			//printf("login to '%s' with '%s'\n", url.String(), auth.String());
			urldb_set_auth_details(url, realm.String(), auth.String());
			cb(true, cbpw);
			break;
		}
		default:
			break;
	}
	delete message;
}

void nsbeos_window_expose_event(BView *view, gui_window *g, BMessage *message)
{
	BRect updateRect;
	hlcache_handle *c;
	float scale = g->bw->scale;
	struct rect clip;

	struct redraw_context ctx = { true, true, &nsbeos_plotters };

	assert(g);
	assert(g->bw);

	struct gui_window *z;
	for (z = window_list; z && z != g; z = z->next)
		continue;
	assert(z);
	assert(g->view == view);

	// we'll be resizing = reflowing = redrawing everything anyway...
	if (g->pending_resizes > 1)
		return;

	if (message->FindRect("rect", &updateRect) < B_OK)
		return;

	c = g->bw->current_content;
	if (c == NULL)
		return;

	if (!view->LockLooper())
		return;
	nsbeos_current_gc_set(view);

	if (view->Window())
		view->Window()->BeginViewTransaction();

	clip.x0 = (int)updateRect.left;
	clip.y0 = (int)updateRect.top;
	clip.x1 = (int)updateRect.right + 1;
	clip.y1 = (int)updateRect.bottom + 1;

	browser_window_redraw(g->bw, 0, 0, &clip, &ctx);

	if (g->careth != 0)
		nsbeos_plot_caret(g->caretx, g->carety, g->careth);

	if (view->Window())
		view->Window()->EndViewTransaction();

	// reset clipping just in case
	view->ConstrainClippingRegion(NULL);
	nsbeos_current_gc_set(NULL);
	view->UnlockLooper();
}

void nsbeos_window_keypress_event(BView *view, gui_window *g, BMessage *event)
{
	const char *bytes;
	char buff[6];
	int numbytes = 0;
	uint32 mods;
	uint32 key;
	uint32 raw_char;
	uint32_t nskey;
	int i;

	if (event->FindInt32("modifiers", (int32 *)&mods) < B_OK)
		mods = modifiers();
	if (event->FindInt32("key", (int32 *)&key) < B_OK)
		key = 0;
	if (event->FindInt32("raw_char", (int32 *)&raw_char) < B_OK)
		raw_char = 0;
	/* check for byte[] first, because C-space gives bytes="" (and byte[0] = '\0') */
	for (i = 0; i < 5; i++) {
		buff[i] = '\0';
		if (event->FindInt8("byte", i, (int8 *)&buff[i]) < B_OK)
			break;
	}

	if (i) {
		bytes = buff;
		numbytes = i;
	} else if (event->FindString("bytes", &bytes) < B_OK)
		bytes = "";

	if (!numbytes)
		numbytes = strlen(bytes);

	LOG(("mods 0x%08lx key %ld raw %ld byte[0] %d", mods, key, raw_char, buff[0]));

	char byte;
	if (numbytes == 1) {
		byte = bytes[0];
		if (mods & B_CONTROL_KEY)
			byte = (char)raw_char;
		if (byte >= '!' && byte <= '~')
			nskey = (uint32_t)byte;
		else {
			switch (byte) {
				case B_BACKSPACE:	nskey = KEY_DELETE_LEFT; break;
				case B_TAB:	nskey = KEY_TAB; break;
				/*case XK_Linefeed:	return QKlinefeed;*/
				case B_ENTER:	nskey = (uint32_t)10; break;
				case B_ESCAPE:	nskey = (uint32_t)'\033'; break;
				case B_SPACE:	nskey = (uint32_t)' '; break;
				case B_DELETE:	nskey = KEY_DELETE_RIGHT; break;
				/*
				case B_INSERT:	nskey = KEYSYM("insert"); break;
				*/
				case B_HOME:	nskey = KEY_LINE_START; break; // XXX ?
				case B_END:	nskey = KEY_LINE_END; break; // XXX ?
				case B_PAGE_UP:	nskey = KEY_PAGE_UP; break;
				case B_PAGE_DOWN:	nskey = KEY_PAGE_DOWN; break;
				case B_LEFT_ARROW:	nskey = KEY_LEFT; break;
				case B_RIGHT_ARROW:	nskey = KEY_RIGHT; break;
				case B_UP_ARROW:	nskey = KEY_UP; break;
				case B_DOWN_ARROW:	nskey = KEY_DOWN; break;
				/*
				case B_FUNCTION_KEY:
					switch (scancode) {
						case B_F1_KEY: nskey = KEYSYM("f1"); break;
						case B_F2_KEY: nskey = KEYSYM("f2"); break;
						case B_F3_KEY: nskey = KEYSYM("f3"); break;
						case B_F4_KEY: nskey = KEYSYM("f4"); break;
						case B_F5_KEY: nskey = KEYSYM("f5"); break;
						case B_F6_KEY: nskey = KEYSYM("f6"); break;
						case B_F7_KEY: nskey = KEYSYM("f7"); break;
						case B_F8_KEY: nskey = KEYSYM("f8"); break;
						case B_F9_KEY: nskey = KEYSYM("f9"); break;
						case B_F10_KEY: nskey = KEYSYM("f10"); break;
						case B_F11_KEY: nskey = KEYSYM("f11"); break;
						case B_F12_KEY: nskey = KEYSYM("f12"); break;
						case B_PRINT_KEY: nskey = KEYSYM("print"); break;
						case B_SCROLL_KEY: nskey = KEYSYM("scroll-lock"); break;
						case B_PAUSE_KEY: nskey = KEYSYM("pause"); break;
					}
				*/
				case 0:
					nskey = (uint32_t)0;
				default:
					nskey = (uint32_t)raw_char;
					/*if (simple_p)
						nskey = (uint32_t)0;*/
					break;
			}
		}
	} else {
		nskey = utf8_to_ucs4(bytes, numbytes);
	}

	if(browser_window_key_press(g->bw, nskey))
		return;

	// Remaining events are for scrolling the page around
	float hdelta = 0.0f, vdelta = 0.0f;
	g->view->LockLooper();
	BRect size = g->view->Bounds();
	switch (byte) {
		case B_HOME:
			g->view->ScrollTo(0.0f, 0.0f);
			break;
		case B_END:
		{
			// TODO
			break;
		}
		case B_PAGE_UP:
			vdelta = -size.Height();
			break;
		case B_PAGE_DOWN:
			vdelta = size.Height();
			break;
		case B_LEFT_ARROW:
			hdelta = -10;
			break;
		case B_RIGHT_ARROW:
			hdelta = 10;
			break;
		case B_UP_ARROW:
			vdelta = -10;
			break;
		case B_DOWN_ARROW:
			vdelta = 10;
			break;
	}

	g->view->ScrollBy(hdelta, vdelta);
	g->view->UnlockLooper();
}


void nsbeos_window_resize_event(BView *view, gui_window *g, BMessage *event)
{
	//CALLED();
	int32 width;
	int32 height;

	// drop this event if we have at least 2 resize pending
	if (atomic_add(&g->pending_resizes, -1) > 1)
		return;

	if (event->FindInt32("width", &width) < B_OK)
		width = -1;
	if (event->FindInt32("height", &height) < B_OK)
		height = -1;
	width++;
	height++;


		g->bw->reformat_pending = true;
		browser_reformat_pending = true;

	return;
}


void nsbeos_window_moved_event(BView *view, gui_window *g, BMessage *event)
{
	//CALLED();

#warning XXX: Invalidate ? 
	if (!view || !view->LockLooper())
		return;
	//view->Invalidate(view->Bounds());
	view->UnlockLooper();

	//g->bw->reformat_pending = true;
	//browser_reformat_pending = true;
	

	return;
}


void nsbeos_reflow_all_windows(void)
{
	for (struct gui_window *g = window_list; g; g = g->next)
		g->bw->reformat_pending = true;

	browser_reformat_pending = true;
}


/**
 * Process pending reformats
 */

void nsbeos_window_process_reformats(void)
{
	struct gui_window *g;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		NSBrowserFrameView *view = g->view;
		if (!g->bw->reformat_pending)
			continue;
		if (!view || !view->LockLooper())
			continue;
		g->bw->reformat_pending = false;
		BRect bounds = view->Bounds();
		view->UnlockLooper();
#warning XXX why - 1 & - 2 !???
		browser_window_reformat(g->bw,
				false,
				bounds.Width() + 1 /* - 2*/,
				bounds.Height() + 1);
	}

}


void nsbeos_window_destroy_browser(struct gui_window *g)
{
	browser_window_destroy(g->bw);
}

static void gui_window_destroy(struct gui_window *g)
{
	if (!g)
		return;

	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;

	if (g->next)
		g->next->prev = g->prev;


	LOG(("Destroying gui_window %p", g));
	assert(g != NULL);
	assert(g->bw != NULL);
	LOG(("     Scaffolding: %p", g->scaffold));
	LOG(("     Window name: %s", g->bw->name));

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	BLooper *looper = g->view->Looper();
	/* If we're a top-level gui_window, destroy our scaffold */
	if (g->toplevel) {
		g->view->RemoveSelf();
		delete g->view;
		nsbeos_scaffolding_destroy(g->scaffold);
	} else {
		g->view->RemoveSelf();
		delete g->view;
		looper->Unlock();
	}
	//XXX 
	//looper->Unlock();


	free(g);

}

void nsbeos_redraw_caret(struct gui_window *g)
{
	if (g->careth == 0)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

	g->view->Invalidate(BRect(g->caretx, g->carety,
				g->caretx, g->carety + g->careth));

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

static void gui_window_redraw_window(struct gui_window *g)
{
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

	g->view->Invalidate();

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

static void gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	hlcache_handle *c = g->bw->current_content;

	if (c == NULL)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

//XXX +1 ??
	g->view->Invalidate(BRect(rect->x0, rect->y0,
				   rect->x1 - 1, rect->y1 - 1));

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	//CALLED();
	if (g->view == NULL)
		return false;
	if (!g->view->LockLooper())
		return false;

#warning XXX: report to view frame ?
	if (g->view->ScrollBar(B_HORIZONTAL))
		*sx = (int)g->view->ScrollBar(B_HORIZONTAL)->Value();
	if (g->view->ScrollBar(B_VERTICAL))
		*sy = (int)g->view->ScrollBar(B_VERTICAL)->Value();
		
	g->view->UnlockLooper();
	return true;
}

static void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	//CALLED();
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

#warning XXX: report to view frame ?
	if (g->view->ScrollBar(B_HORIZONTAL))
		g->view->ScrollBar(B_HORIZONTAL)->SetValue(sx);
	if (g->view->ScrollBar(B_VERTICAL))
		g->view->ScrollBar(B_VERTICAL)->SetValue(sy);
		
	g->view->UnlockLooper();
}


static void gui_window_update_extent(struct gui_window *g)
{
	//CALLED();
	if (!g->bw->current_content)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	float x_max = content_get_width(g->bw->current_content) * g->bw->scale /* - 1*/;
	float y_max = content_get_height(g->bw->current_content) * g->bw->scale /* - 1*/;
	float x_prop = g->view->Bounds().Width() / x_max;
	float y_prop = g->view->Bounds().Height() / y_max;
	x_max -= g->view->Bounds().Width() + 1;
	y_max -= g->view->Bounds().Height() + 1;
	LOG(("x_max = %f y_max = %f x_prop = %f y_prop = %f\n", x_max, y_max, x_prop, y_prop));
	if (g->view->ScrollBar(B_HORIZONTAL)) {
		g->view->ScrollBar(B_HORIZONTAL)->SetRange(0, x_max);
		g->view->ScrollBar(B_HORIZONTAL)->SetProportion(x_prop);
		g->view->ScrollBar(B_HORIZONTAL)->SetSteps(10, 50);
	}
	if (g->view->ScrollBar(B_VERTICAL)) {
		g->view->ScrollBar(B_VERTICAL)->SetRange(0, y_max);
		g->view->ScrollBar(B_VERTICAL)->SetProportion(y_prop);
		g->view->ScrollBar(B_VERTICAL)->SetSteps(10, 50);
	}


	g->view->UnlockLooper();
}

/* some cursors like those in Firefox */
// XXX: move to separate file or resource ?

const uint8 kLinkCursorBits[] = {
	16,		/* cursor size */
	1,		/* bits per pixel */
	2,		/* vertical hot spot */
	2,		/* horizontal hot spot */

	/* data */
	0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x24, 0x00, 0x24, 0x00, 0x13, 0xe0, 0x12, 0x5c, 0x09, 0x2a, 
	0x08, 0x01, 0x3c, 0x21, 0x4c, 0x71, 0x42, 0x71, 0x30, 0xf9, 0x0c, 0xf9, 0x02, 0x02, 0x01, 0x00,

	/* mask */
	0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x3c, 0x00, 0x3c, 0x00, 0x1f, 0xe0, 0x1f, 0xfc, 0x0f, 0xfe, 
	0x0f, 0xff, 0x3f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x3f, 0xff, 0x0f, 0xff, 0x03, 0xfc, 0x01, 0xe0
};

const uint8 kWatchCursorBits[] = {
	16,		/* cursor size */
	1,		/* bits per pixel */
	0,		/* vertical hot spot */
	1,		/* horizontal hot spot */

	/* data */
	0x70, 0x00, 0x48, 0x00, 0x48, 0x00, 0x27, 0xc0, 0x24, 0xb8, 0x12, 0x54, 0x10, 0x02, 0x78, 0x02, 
	0x98, 0x02, 0x84, 0x02, 0x60, 0x3a, 0x18, 0x46, 0x04, 0x8a, 0x02, 0x92, 0x01, 0x82, 0x00, 0x45,

	/* mask */
	0x70, 0x00, 0x78, 0x00, 0x78, 0x00, 0x3f, 0xc0, 0x3f, 0xf8, 0x1f, 0xfc, 0x1f, 0xfe, 0x7f, 0xfe, 
	0xff, 0xfe, 0xff, 0xfe, 0x7f, 0xfe, 0x1f, 0xfe, 0x07, 0xfe, 0x03, 0xfe, 0x01, 0xfe, 0x00, 0x7f
};

const uint8 kWatch2CursorBits[] = {
	16,		/* cursor size */
	1,		/* bits per pixel */
	0,		/* vertical hot spot */
	1,		/* horizontal hot spot */

	/* data */
	0x70, 0x00, 0x48, 0x00, 0x48, 0x00, 0x27, 0xc0, 0x24, 0xb8, 0x12, 0x54, 0x10, 0x02, 0x78, 0x02, 
	0x98, 0x02, 0x84, 0x02, 0x60, 0x3a, 0x18, 0x46, 0x04, 0xa2, 0x02, 0x92, 0x01, 0xa2, 0x00, 0x45,

	/* mask */
	0x70, 0x00, 0x78, 0x00, 0x78, 0x00, 0x3f, 0xc0, 0x3f, 0xf8, 0x1f, 0xfc, 0x1f, 0xfe, 0x7f, 0xfe, 
	0xff, 0xfe, 0xff, 0xfe, 0x7f, 0xfe, 0x1f, 0xfe, 0x07, 0xfe, 0x03, 0xfe, 0x01, 0xfe, 0x00, 0x7f
};


static void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	BCursor *cursor = NULL;
	bool allocated = false;

	if (g->current_pointer == shape)
		return;

	g->current_pointer = shape;

	switch (shape) {
	case GUI_POINTER_POINT:
		cursor = new BCursor(kLinkCursorBits);
		allocated = true;
		break;
	case GUI_POINTER_CARET:
		cursor = (BCursor *)B_CURSOR_I_BEAM;
		break;
	case GUI_POINTER_WAIT:
		cursor = new BCursor(kWatchCursorBits);
		allocated = true;
		break;
	case GUI_POINTER_PROGRESS:
		cursor = new BCursor(kWatch2CursorBits);
		allocated = true;
		break;
	default:
		cursor = (BCursor *)B_CURSOR_SYSTEM_DEFAULT;
		allocated = false;
	}

	if (g->view && g->view->LockLooper()) {
		g->view->SetViewCursor(cursor);
		g->view->UnlockLooper();
	}

	if (allocated)
		delete cursor;
}

static void gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip)
{
	//CALLED();
	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_redraw_caret(g);

	g->caretx = x;
	g->carety = y + 1;
	g->careth = height - 2;

	nsbeos_redraw_caret(g);
	g->view->MakeFocus();

	g->view->UnlockLooper();
}

static void gui_window_remove_caret(struct gui_window *g)
{
	int oh = g->careth;

	if (oh == 0)
		return;

	g->careth = 0;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	nsbeos_current_gc_set(g->view);

	g->view->Invalidate(BRect(g->caretx, g->carety, g->caretx, g->carety + oh));

	nsbeos_current_gc_set(NULL);
	g->view->UnlockLooper();
}

static void gui_window_new_content(struct gui_window *g)
{
	if (!g->toplevel)
		return;

	if (g->view == NULL)
		return;
	if (!g->view->LockLooper())
		return;

	// scroll back to top
	g->view->ScrollTo(0,0);

	g->view->UnlockLooper();
}

static void gui_start_selection(struct gui_window *g)
{
	if (!g->view->LockLooper())
		return;

	g->view->MakeFocus();

	g->view->UnlockLooper();
}

void gui_get_clipboard(char **buffer, size_t *length)
{
	BMessage *clip;
	*length = 0;
	*buffer = NULL;

	if (be_clipboard->Lock()) {
		clip = be_clipboard->Data();
		if (clip) {
			const char *text;
			int32 textlen;
			if (clip->FindData("text/plain", B_MIME_TYPE, 
				(const void **)&text, &textlen) >= B_OK) {
				*buffer = (char *)malloc(textlen);
				*length = textlen;
				memcpy(*buffer, text, textlen);
			}
		}
		be_clipboard->Unlock();
	}
}

void gui_set_clipboard(const char *buffer, size_t length,
	nsclipboard_styles styles[], int n_styles)
{
	BMessage *clip;

	if (be_clipboard->Lock()) {
		be_clipboard->Clear();
		clip = be_clipboard->Data();
		if (clip) {
			clip->AddData("text/plain", B_MIME_TYPE, buffer, length);

			int arraySize = sizeof(text_run_array) + 
				n_styles * sizeof(text_run);
			text_run_array *array = (text_run_array *)malloc(arraySize);
			array->count = n_styles;
			for (int i = 0; i < n_styles; i++) {
				BFont font;
				nsbeos_style_to_font(font, &styles[i].style);
				array->runs[i].offset = styles[i].start;
				array->runs[i].font = font;
				array->runs[i].color =
					nsbeos_rgb_colour(styles[i].style.foreground);
			}
			clip->AddData("application/x-vnd.Be-text_run_array", B_MIME_TYPE, 
				array, arraySize);
			free(array);
			be_clipboard->Commit();
		}
		be_clipboard->Unlock();
	}
}

static void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
			       bool scaled)
{
	if (g->view && g->view->LockLooper()) {
		*width = g->view->Bounds().Width() + 1;
		*height = g->view->Bounds().Height() + 1;
		g->view->UnlockLooper();
	}

	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
}

static struct gui_window_table window_table = {
	gui_window_create,
	gui_window_destroy,
	gui_window_redraw_window,
	gui_window_update_box,
	gui_window_get_scroll,
	gui_window_set_scroll,
	gui_window_get_dimensions,
	gui_window_update_extent,

	/* from scaffold */
	gui_window_set_title,
	gui_window_set_url,
	gui_window_set_icon,
	gui_window_set_status,
	gui_window_set_pointer,
	gui_window_place_caret,
	gui_window_remove_caret,
	gui_window_start_throbber,
	gui_window_stop_throbber,
	NULL, //drag_start
	NULL, //save_link
	NULL, //scroll_visible
	NULL, //scroll_start
	gui_window_new_content,
	NULL, //file_gadget_open
	NULL, //drag_save_object
	NULL, //drag_save_selection
	gui_start_selection
};

struct gui_window_table *beos_window_table = &window_table;
