/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <BeBuild.h>
#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Dragger.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Node.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Resources.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>
#include <Window.h>

#if defined(__HAIKU__)
#include <IconUtils.h>
#include "WindowStack.h"
#endif

#include <fs_attr.h>
extern "C" {
#include "content/content.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/plotters.h"
#include "utils/nsoption.h"
#include "desktop/textinput.h"
#include "render/font.h"
#include "render/form.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"
}
#include "beos/about.h"
#include "beos/bitmap.h"
#include "beos/gui.h"
#include "beos/plotters.h"
#include "beos/scaffolding.h"
#include "beos/gui_options.h"
//#include "beos/completion.h"
#include "beos/throbber.h"
#include "beos/window.h"
#include "beos/schedule.h"
//#include "beos/download.h"

#define TOOLBAR_HEIGHT 32
#define DRAGGER_WIDTH 8

struct beos_history_window;

class NSIconTextControl;
class NSBrowserWindow;
class NSThrobber;

struct beos_scaffolding {
	NSBrowserWindow		*window;	// top-level container object

	// top-level view, contains toolbar & top-level browser view
	NSBaseView		*top_view;

	BMenuBar		*menu_bar;

	BPopUpMenu		*popup_menu;

	BDragger		*dragger;

	BView			*tool_bar;

	BControl		*back_button;
	BControl		*forward_button;
	BControl		*stop_button;
	BControl		*reload_button;
	BControl		*home_button;

	NSIconTextControl	*url_bar;
	//BMenuField	*url_bar_completion;

	NSThrobber		*throbber;

	BStringView		*status_bar;

	BScrollView		*scroll_view;

	struct beos_history_window *history_window;

	int			throb_frame;
	struct gui_window	*top_level;
	int			being_destroyed;

	bool			fullscreen;
};

struct beos_history_window {
	struct beos_scaffolding 	*g;
	BWindow		*window;

};

struct menu_events {
	const char *widget;
};

// passed to the replicant main thread
struct replicant_thread_info {
	char app[B_PATH_NAME_LENGTH];
	BString url;
	char *args[3];
};


static int open_windows = 0;		/**< current number of open browsers */
static NSBaseView *replicant_view = NULL; /**< if not NULL, the replicant View we are running NetSurf for */
static sem_id replicant_done_sem = -1;
static thread_id replicant_thread = -1;

static void nsbeos_window_update_back_forward(struct beos_scaffolding *);
static void nsbeos_throb(void *);
static int32 nsbeos_replicant_main_thread(void *_arg);

// in beos_gui.cpp
extern int main(int argc, char** argv);

// in fetch_rsrc.cpp
extern BResources *gAppResources;

// #pragma mark - class NSIconTextControl

#define ICON_WIDTH 16

class NSIconTextControl : public BTextControl {
public:
		NSIconTextControl(BRect frame, const char* name,
						const char* label, const char* initialText,
						BMessage* message,
						uint32 resizeMode
							= B_FOLLOW_LEFT | B_FOLLOW_TOP,
						uint32 flags
							= B_WILL_DRAW | B_NAVIGABLE | B_DRAW_ON_CHILDREN);
virtual	~NSIconTextControl();

virtual	void	FrameResized(float newWidth, float newHeight);
virtual void	Draw(BRect updateRect);
virtual void	DrawAfterChildren(BRect updateRect);
virtual void	AttachedToWindow();

void	SetBitmap(const BBitmap *bitmap);
void	FixupTextRect();

private:
	BPoint fIconOffset;
	BRect fIconFrame;
	const BBitmap *fIconBitmap;
};

NSIconTextControl::NSIconTextControl(BRect frame, const char* name,
						const char* label, const char* initialText,
						BMessage* message,
						uint32 resizeMode,
						uint32 flags)
	: BTextControl(frame, name, label, initialText, message, resizeMode, flags),
	fIconOffset(0,0),
	fIconBitmap(NULL)
{
	BRect r(Bounds());
	BRect frame = r;
	frame.right = frame.left + ICON_WIDTH - 1;
	frame.bottom = frame.top + ICON_WIDTH - 1;
	frame.OffsetBy((int32)((r.IntegerHeight() - ICON_WIDTH + 3) / 2),
		(int32)((r.IntegerHeight() - ICON_WIDTH + 1) / 2));
	fIconFrame = frame;
	FixupTextRect();
}


NSIconTextControl::~NSIconTextControl()
{
	delete fIconBitmap;
}


void
NSIconTextControl::FrameResized(float newWidth, float newHeight)
{
	BTextControl::FrameResized(newWidth, newHeight);
	FixupTextRect();
}


void
NSIconTextControl::Draw(BRect updateRect)
{
	FixupTextRect();
	BTextControl::Draw(updateRect);
}


void
NSIconTextControl::DrawAfterChildren(BRect updateRect)
{
	BTextControl::DrawAfterChildren(updateRect);

	PushState();

	SetDrawingMode(B_OP_ALPHA);
	DrawBitmap(fIconBitmap, fIconFrame);

	//XXX: is this needed?
	PopState();
}


void
NSIconTextControl::AttachedToWindow()
{
	BTextControl::AttachedToWindow();
	FixupTextRect();
}


void
NSIconTextControl::SetBitmap(const BBitmap *bitmap)
{
	delete fIconBitmap;
	fIconBitmap = NULL;

	// keep a copy
	if (bitmap)
		fIconBitmap = new BBitmap(bitmap);
	// invalidate just the icon area
	Invalidate(fIconFrame);
}


void
NSIconTextControl::FixupTextRect()
{
	// FIXME: this flickers on resize, quite ugly
	BRect r(TextView()->TextRect());

	// don't fix the fix
	if (r.left > ICON_WIDTH)
		return;

	r.left += r.bottom - r.top;
	TextView()->SetTextRect(r);
}


#undef ICON_WIDTH

// #pragma mark - class NSResizeKnob

class NSResizeKnob : public BView {
public:
		NSResizeKnob(BRect frame, BView *target);
virtual	~NSResizeKnob();

virtual	void	MouseDown(BPoint where);
virtual	void	MouseUp(BPoint where);
virtual	void	MouseMoved(BPoint where, uint32 code,
							const BMessage* dragMessage);

virtual void	Draw(BRect updateRect);

void			SetBitmap(const BBitmap *bitmap);

private:
	const BBitmap *fBitmap;
	BView *fTarget;
	BPoint fOffset;
};

NSResizeKnob::NSResizeKnob(BRect frame, BView *target)
	: BView(frame, "NSResizeKnob", B_FOLLOW_BOTTOM | B_FOLLOW_RIGHT, B_WILL_DRAW),
	fBitmap(NULL),
	fTarget(target),
	fOffset(-1, -1)
{
	SetViewColor(0, 255, 0);
}


NSResizeKnob::~NSResizeKnob()
{
}


void
NSResizeKnob::MouseDown(BPoint where)
{
	SetMouseEventMask(B_POINTER_EVENTS,
		B_NO_POINTER_HISTORY | B_LOCK_WINDOW_FOCUS);
	fOffset = where;
}


void
NSResizeKnob::MouseUp(BPoint where)
{
	fOffset.Set(-1, -1);
}


void
NSResizeKnob::MouseMoved(BPoint where, uint32 code,
						const BMessage* dragMessage)
{
	if (fOffset.x >= 0) {
		fTarget->ResizeBy(where.x - fOffset.x, where.y - fOffset.y);
	}
}


void
NSResizeKnob::Draw(BRect updateRect)
{
	if (!fBitmap)
		return;
	DrawBitmap(fBitmap);
}


void
NSResizeKnob::SetBitmap(const BBitmap *bitmap)
{
	fBitmap = bitmap;
	Invalidate();
}


// #pragma mark - class NSThrobber

class NSThrobber : public BView {
public:
		NSThrobber(BRect frame);
virtual	~NSThrobber();

virtual void	MessageReceived(BMessage *message);
virtual void	Draw(BRect updateRect);
void			SetBitmap(const BBitmap *bitmap);

private:
	const BBitmap *fBitmap;
};

NSThrobber::NSThrobber(BRect frame)
	: BView(frame, "NSThrobber", B_FOLLOW_TOP | B_FOLLOW_RIGHT, B_WILL_DRAW),
	fBitmap(NULL)
{
}


NSThrobber::~NSThrobber()
{
}


void
NSThrobber::MessageReceived(BMessage *message)
{
	BView::MessageReceived(message);
}


void
NSThrobber::Draw(BRect updateRect)
{
	if (!fBitmap)
		return;
	DrawBitmap(fBitmap);
}


void
NSThrobber::SetBitmap(const BBitmap *bitmap)
{
	fBitmap = bitmap;
	Invalidate();
}


// #pragma mark - class NSBaseView


NSBaseView::NSBaseView(BRect frame)
	: BView(frame, "NetSurf", B_FOLLOW_ALL_SIDES, 
		0 /*B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS*/ /*| B_SUBPIXEL_PRECISE*/),
	fScaffolding(NULL)
{
}

NSBaseView::NSBaseView(BMessage *archive)
	: BView(archive),
	fScaffolding(NULL)
{
}


NSBaseView::~NSBaseView()
{
	//warn_user ("~NSBaseView()", NULL);
	if (replicated) {
		BMessage *message = new BMessage(B_QUIT_REQUESTED);
		nsbeos_pipe_message_top(message, NULL, fScaffolding);
		while (acquire_sem(replicant_done_sem) == EINTR);
		//debugger("plop");
		status_t status = -1;
		wait_for_thread(replicant_thread, &status);
	}
}


void
NSBaseView::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_ABOUT_REQUESTED:
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
		case ABOUT_BUTTON:
		case APPLICATION_QUIT:
			if (Window())
				Window()->DetachCurrentMessage();
			nsbeos_pipe_message_top(message, NULL, fScaffolding);
			break;
		default:
			//message->PrintToStream();
			BView::MessageReceived(message);
	}
}


status_t
NSBaseView::Archive(BMessage *archive, bool deep) const
{
	// force archiving only the base view
	deep = false;
	status_t err;
	err = BView::Archive(archive, deep);
	if (err < B_OK)
		return err;
	// add our own fields
	// we try to reuse the same fields as NetPositive
	archive->AddString("add_on", "application/x-vnd.NetSurf");
	//archive->AddInt32("version", 2);
	archive->AddString("url", fScaffolding->url_bar->Text());
	archive->AddBool("openAsText", false);
	archive->AddInt32("encoding", 258);
	return err;
}


BArchivable	*
NSBaseView::Instantiate(BMessage *archive)
{
	if (!validate_instantiation(archive, "NSBaseView"))
		return NULL;
	const char *url;
	if (archive->FindString("url", &url) < B_OK
		|| url == NULL || strlen(url) == 0) {
		url = "about:";
	}

	struct replicant_thread_info *info = new replicant_thread_info;
	info->url = BString(url);
	if (nsbeos_find_app_path(info->app) < B_OK)
		return NULL;
	info->args[0] = info->app;
	info->args[1] = (char *)info->url.String();
	info->args[2] = NULL;
	NSBaseView *view = new NSBaseView(archive);
	replicant_view = view;
	replicated = true;

	//TODO:FIXME: fix replicants
	// do as much as possible in this thread to avoid deadlocks
	
	gui_init_replicant(2, info->args);

	replicant_done_sem = create_sem(0, "NS Replicant created");
	replicant_thread = spawn_thread(nsbeos_replicant_main_thread,
		"NetSurf Main Thread", B_NORMAL_PRIORITY, info);
	if (replicant_thread < B_OK) {
		delete_sem(replicant_done_sem);
		delete info;
		delete view;
		return NULL;
	}
	resume_thread(replicant_thread);
	//XXX: deadlocks BeHappy
	//while (acquire_sem(replicant_done_sem) == EINTR);

	return view;
}


void
NSBaseView::SetScaffolding(struct beos_scaffolding *scaf)
{
	fScaffolding = scaf;
}


// AttachedToWindow() is not enough to get the dragger and status bar
// stick to the panel color
void
NSBaseView::AllAttached()
{
	BView::AllAttached();

	struct beos_scaffolding *g = fScaffolding;
	if (!g)
		return;
	// set targets to the topmost ns view
	g->back_button->SetTarget(this);
	g->forward_button->SetTarget(this);
	g->stop_button->SetTarget(this);
	g->reload_button->SetTarget(this);
	g->home_button->SetTarget(this);

	g->url_bar->SetTarget(this);

	rgb_color c = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetViewColor(c);

	g->tool_bar->SetViewColor(c);
	g->back_button->SetViewColor(c);
	g->back_button->SetLowColor(c);
	g->forward_button->SetViewColor(c);
	g->forward_button->SetLowColor(c);
	g->stop_button->SetViewColor(c);
	g->stop_button->SetLowColor(c);
	g->reload_button->SetViewColor(c);
	g->reload_button->SetLowColor(c);
	g->home_button->SetViewColor(c);
	g->home_button->SetLowColor(c);
	g->url_bar->SetViewColor(c);
	g->throbber->SetViewColor(c);
	g->scroll_view->SetViewColor(c);

	g->dragger->SetViewColor(c);

	g->status_bar->SetViewColor(c);
	g->status_bar->SetLowColor(c);
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	g->status_bar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif
}


// #pragma mark - class NSBrowserWindow


NSBrowserWindow::NSBrowserWindow(BRect frame, struct beos_scaffolding *scaf)
	: BWindow(frame, "NetSurf", B_DOCUMENT_WINDOW, 0),
	fScaffolding(scaf)
{
}


NSBrowserWindow::~NSBrowserWindow()
{
	if(activeWindow == this)
		activeWindow = NULL;
}


void
NSBrowserWindow::DispatchMessage(BMessage *message, BHandler *handler)
{
	BMessage *msg;
	switch (message->what) {
		case B_UI_SETTINGS_CHANGED:
			msg = new BMessage(*message);
			nsbeos_pipe_message_top(msg, this, fScaffolding);
			break;
	}
	BWindow::DispatchMessage(message, handler);
}


void
NSBrowserWindow::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_ARGV_RECEIVED:
		case B_REFS_RECEIVED:
		case B_UI_SETTINGS_CHANGED:
			DetachCurrentMessage();
			nsbeos_pipe_message_top(message, this, fScaffolding);
			break;
		default:
			BWindow::MessageReceived(message);
	}
}

bool
NSBrowserWindow::QuitRequested(void)
{
	BWindow::QuitRequested();
	BMessage *message = DetachCurrentMessage();
	// BApplication::Quit() calls us directly...
	if (message == NULL)
		message = new BMessage(B_QUIT_REQUESTED);
	nsbeos_pipe_message_top(message, this, fScaffolding);
	return false; // we will Quit() ourselves from the main thread
}


void
NSBrowserWindow::WindowActivated(bool active)
{
	if(active)
		activeWindow = this;
	else if(activeWindow == this)
		activeWindow = NULL;
}


// #pragma mark - implementation

int32 nsbeos_replicant_main_thread(void *_arg)
{
	struct replicant_thread_info *info = (struct replicant_thread_info *)_arg;
	int32 ret = 0;

	netsurf_main_loop();
	netsurf_exit();
	delete info;
	delete_sem(replicant_done_sem);
	return ret;
}


/* event handlers and support functions for them */

static void nsbeos_window_destroy_event(NSBrowserWindow *window, nsbeos_scaffolding *g, BMessage *event)
{
	LOG(("Being Destroyed = %d", g->being_destroyed));

	if (--open_windows == 0)
		netsurf_quit = true;

	if (window) {
		window->Lock();
		window->Quit();
	}

	if (!g->being_destroyed) {
		g->being_destroyed = 1;
		nsbeos_window_destroy_browser(g->top_level);
	}
}


static void nsbeos_scaffolding_update_colors(nsbeos_scaffolding *g)
{
	if (!g->top_view->LockLooper())
		return;
	rgb_color c = ui_color(B_PANEL_BACKGROUND_COLOR);
	g->top_view->SetViewColor(c);

	g->tool_bar->SetViewColor(c);
	g->back_button->SetViewColor(c);
	g->forward_button->SetViewColor(c);
	g->stop_button->SetViewColor(c);
	g->reload_button->SetViewColor(c);
	g->home_button->SetViewColor(c);
	g->url_bar->SetViewColor(c);
	g->throbber->SetViewColor(c);
	g->scroll_view->SetViewColor(c);

	g->dragger->SetViewColor(c);

	g->status_bar->SetViewColor(c);
	g->status_bar->SetLowColor(c);
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	g->status_bar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif
	g->top_view->UnlockLooper();
}


/*static*/ BWindow*
NSBrowserWindow::activeWindow = NULL;


void nsbeos_scaffolding_dispatch_event(nsbeos_scaffolding *scaffold, BMessage *message)
{
	struct browser_window *bw;
	bw = nsbeos_get_browser_for_gui(scaffold->top_level);
	bool reloadAll = false;

	LOG(("nsbeos_scaffolding_dispatch_event() what = 0x%08lx", message->what));
	switch (message->what) {
		case B_QUIT_REQUESTED:
			nsbeos_scaffolding_destroy(scaffold);
			break;
		case B_ABOUT_REQUESTED:
		{
			nsbeos_about(scaffold->top_level);
			break;
		}
		case B_NETPOSITIVE_DOWN:
			//XXX WRITEME
			break;
		case B_SIMPLE_DATA:
		{
			if (!message->HasRef("refs")) {
				// XXX handle DnD
				break;
			}
			// FALL THROUGH
			// handle refs
		}
		case B_REFS_RECEIVED:
		{
			int32 i;
			entry_ref ref;

			for (i = 0; message->FindRef("refs", i, &ref) >= B_OK; i++) {
				BString url("file://");
				BPath path(&ref);
				if (path.InitCheck() < B_OK)
					break;

				BNode node(path.Path());
				if (node.InitCheck() < B_OK)
					break;
				if (node.IsSymLink()) {
					// dereference the symlink
					BEntry entry(path.Path(), true);
					if (entry.InitCheck() < B_OK)
						break;
					if (entry.GetPath(&path) < B_OK)
						break;
					if (node.SetTo(path.Path()) < B_OK)
						break;
				}

				attr_info ai;
				if (node.GetAttrInfo("META:url", &ai) >= B_OK) {
					char data[(size_t)ai.size + 1];
					memset(data, 0, (size_t)ai.size + 1);
					if (node.ReadAttr("META:url", B_STRING_TYPE, 0LL, data, (size_t)ai.size) < 4)
						break;
					url = data;
				} else
					url << path.Path();

                nsurl *nsurl;
                nserror error;

                error = nsurl_create(url.String(), &nsurl);
				if (error == NSERROR_OK) {
					if (/*message->WasDropped() &&*/ i == 0) {
						browser_window_navigate(bw, nsurl, NULL,
							(browser_window_nav_flags)
							(BW_NAVIGATE_HISTORY),
							NULL, NULL, NULL);
					} else {
						error = browser_window_create(BW_CREATE_CLONE,
								nsurl,
								NULL,
								bw,
								NULL);
					}
					nsurl_unref(nsurl);
				}
				if (error != NSERROR_OK) {
					warn_user(messages_get_errorcode(error), 0);
				}
			}
			break;
		}
		case B_ARGV_RECEIVED:
		{
			int32 i;
			BString urltxt;
                        nsurl *url;
                        nserror error;

			for (i = 1; message->FindString("argv", i, &urltxt) >= B_OK; i++) {
                                error = nsurl_create(urltxt.String(), &url);
                                if (error == NSERROR_OK) {
                                        error = browser_window_create(BW_CREATE_CLONE,
                                                                      url,
                                                                      NULL,
                                                                      bw,
                                                                      NULL);
                                        nsurl_unref(url);
                                }
                                if (error != NSERROR_OK) {
                                        warn_user(messages_get_errorcode(error), 0);
                                }
			}
			break;
		}
		case B_UI_SETTINGS_CHANGED:
			nsbeos_update_system_ui_colors();
			nsbeos_scaffolding_update_colors(scaffold);
			break;
		case B_NETPOSITIVE_OPEN_URL:
		{
			int32 i;
			BString url;
			if (message->FindString("be:url", &url) < B_OK)
				break;

			nsurl *nsurl;
			nserror error;

			error = nsurl_create(url.String(), &nsurl);
			if (error != NSERROR_OK) {
				warn_user(messages_get_errorcode(error), 0);
			} else {
				browser_window_navigate(bw,
						nsurl,
						NULL,
						(browser_window_nav_flags)(BW_NAVIGATE_HISTORY | BW_NAVIGATE_UNVERIFIABLE),
						NULL,
						NULL,
						NULL);
				nsurl_unref(nsurl);
			}
			break;
		}
		case B_COPY:
			browser_window_key_press(bw, KEY_COPY_SELECTION);
			break;
		case B_CUT:
			browser_window_key_press(bw, KEY_CUT_SELECTION);
			break;
		case B_PASTE:
			browser_window_key_press(bw, KEY_PASTE);
			break;
		case B_SELECT_ALL:
			LOG(("Selecting all text"));
			browser_window_key_press(bw, KEY_SELECT_ALL);
			break;
		case B_NETPOSITIVE_BACK:
		case BROWSER_NAVIGATE_BACK:
		case 'back':
			if (!browser_window_history_back_available(bw))
				break;
			browser_window_history_back(bw, false);
			nsbeos_window_update_back_forward(scaffold);
			break;
		case B_NETPOSITIVE_FORWARD:
		case BROWSER_NAVIGATE_FORWARD:
		case 'forw':
			if (!browser_window_history_forward_available(bw))
				break;
			browser_window_history_forward(bw, false);
			nsbeos_window_update_back_forward(scaffold);
			break;
		case B_NETPOSITIVE_STOP:
		case BROWSER_NAVIGATE_STOP:
		case 'stop':
			browser_window_stop(bw);
			break;
		case B_NETPOSITIVE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
		case 'relo':
			reloadAll = true;
			// FALLTHRU
		case BROWSER_NAVIGATE_RELOAD:
			browser_window_reload(bw, reloadAll);
			break;
		case B_NETPOSITIVE_HOME:
		case BROWSER_NAVIGATE_HOME:
		case 'home':
		{
			nsurl *url;
			nserror error;

			static const char *addr = NETSURF_HOMEPAGE;

			if (nsoption_charp(homepage_url) != NULL) {
				addr = nsoption_charp(homepage_url);
			}

			error = nsurl_create(addr, &url);
			if (error != NSERROR_OK) {
				warn_user(messages_get_errorcode(error), 0);
			} else {
				browser_window_navigate(bw,
					url,
					NULL,
					(browser_window_nav_flags)(BW_NAVIGATE_HISTORY),
					NULL,
					NULL,
					NULL);
				nsurl_unref(url);
			}
			break;
		}
		case 'urle':
		{
            nsurl *url;
            nserror error;
			BString text;

			if (!scaffold->url_bar->LockLooper())
				break;

			text = scaffold->url_bar->Text();
			scaffold->scroll_view->Target()->MakeFocus();
			scaffold->url_bar->UnlockLooper();

                        error = nsurl_create(text.String(), &url);
                        if (error != NSERROR_OK) {
                                warn_user(messages_get_errorcode(error), 0);
                        } else {
                                browser_window_navigate(bw,
					url,
					NULL,
					(browser_window_nav_flags)(BW_NAVIGATE_HISTORY),
					NULL,
					NULL,
					NULL);
                                nsurl_unref(url);
                        }
			break;
		}
		case 'urlc':
		{
			BString text;
			if (!scaffold->url_bar->LockLooper())
				break;
			text = scaffold->url_bar->Text();
			scaffold->url_bar->UnlockLooper();
			//nsbeos_completion_update(text.String());
			break;
		}
/*
		case 'menu':
		{
			menu_action action;
			if (message->FindInt32("action", (int32 *)&action) < B_OK)
				break;
			switch (action) {
				case NO_ACTION:
				case HELP_OPEN_CONTENTS:
				case HELP_OPEN_GUIDE:
				case HELP_OPEN_INFORMATION:
				case HELP_OPEN_ABOUT:
				case HELP_LAUNCH_INTERACTIVE:

					break;
			}
#warning XXX
			break;
		}
*/
		case NO_ACTION:
			break;
		case HELP_OPEN_CONTENTS:
			break;
		case HELP_OPEN_GUIDE:
			break;
		case HELP_OPEN_INFORMATION:
			break;
		case HELP_OPEN_ABOUT:
			break;
		case HELP_LAUNCH_INTERACTIVE:
			break;
		case HISTORY_SHOW_LOCAL:
			break;
		case HISTORY_SHOW_GLOBAL:
			break;
		case HOTLIST_ADD_URL:
			break;
		case HOTLIST_SHOW:
			break;
		case COOKIES_SHOW:
			break;
		case COOKIES_DELETE:
			break;
		case BROWSER_PAGE:
			break;
		case BROWSER_PAGE_INFO:
			break;
		case BROWSER_PRINT:
			break;
		case BROWSER_NEW_WINDOW:
		{
			BString text;
                        nsurl *url;
                        nserror error;

			if (!scaffold->url_bar->LockLooper())
				break;
			text = scaffold->url_bar->Text();
			scaffold->url_bar->UnlockLooper();

			NSBrowserWindow::activeWindow = scaffold->window;

                        error = nsurl_create(text.String(), &url);
                        if (error == NSERROR_OK) {
                                error = browser_window_create(BW_CREATE_CLONE,
                                                              url,
                                                              NULL,
                                                              bw,
                                                              NULL);
                                nsurl_unref(url);
                        }
                        if (error != NSERROR_OK) {
                                warn_user(messages_get_errorcode(error), 0);
                        }
			break;
		}
		case BROWSER_VIEW_SOURCE:
		{
			if (!bw || !bw->current_content)
				break;
			nsbeos_gui_view_source(bw->current_content);
			break;
		}
		case BROWSER_OBJECT:
			break;
		case BROWSER_OBJECT_INFO:
			break;
		case BROWSER_OBJECT_RELOAD:
			break;
		case BROWSER_OBJECT_SAVE:
			break;
		case BROWSER_OBJECT_EXPORT_SPRITE:
			break;
		case BROWSER_OBJECT_SAVE_URL_URI:
			break;
		case BROWSER_OBJECT_SAVE_URL_URL:
			break;
		case BROWSER_OBJECT_SAVE_URL_TEXT:
			break;
		case BROWSER_SAVE:
			break;
		case BROWSER_SAVE_COMPLETE:
			break;
		case BROWSER_EXPORT_DRAW:
			break;
		case BROWSER_EXPORT_TEXT:
			break;
		case BROWSER_SAVE_URL_URI:
			break;
		case BROWSER_SAVE_URL_URL:
			break;
		case BROWSER_SAVE_URL_TEXT:
			break;
		case HOTLIST_EXPORT:
			break;
		case HISTORY_EXPORT:
			break;
		case B_NETPOSITIVE_UP:
		case BROWSER_NAVIGATE_UP:
			break;
		case BROWSER_NAVIGATE_URL:
			if (!scaffold->url_bar->LockLooper())
				break;
			scaffold->url_bar->MakeFocus();
			scaffold->url_bar->UnlockLooper();
			break;
		case BROWSER_SCALE_VIEW:
			break;
		case BROWSER_FIND_TEXT:
			break;
		case BROWSER_IMAGES_FOREGROUND:
			break;
		case BROWSER_IMAGES_BACKGROUND:
			break;
		case BROWSER_BUFFER_ANIMS:
			break;
		case BROWSER_BUFFER_ALL:
			break;
		case BROWSER_SAVE_VIEW:
			break;
		case BROWSER_WINDOW_DEFAULT:
			break;
		case BROWSER_WINDOW_STAGGER:
			break;
		case BROWSER_WINDOW_COPY:
			break;
		case BROWSER_WINDOW_RESET:
			break;
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
			break;
		case TOOLBAR_BUTTONS:
			break;
		case TOOLBAR_ADDRESS_BAR:
			break;
		case TOOLBAR_THROBBER:
			break;
		case TOOLBAR_EDIT:
			break;
		case CHOICES_SHOW:
			break;
		case ABOUT_BUTTON:
						/* XXX: doesn't work yet! bug in rsrc:/
			BString url("rsrc:/about.en.html,text/html");
			browser_window_create(url.String(), NULL, NULL, true, false);
			*/
		{
			int32 button;
			if (message->FindInt32("which", &button) == B_OK) {
				const char *goto_url = NULL;
				nserror nserr;
				nsurl *url;
				switch (button) {
					case 0:
						goto_url = "about:credits";
						break;
					case 1:
						goto_url = "about:licence";
						break;
					default:
						break;
				}
				if (goto_url == NULL)
					break;
				nserr = nsurl_create(goto_url, &url);
				if (nserr == NSERROR_OK) {
					nserr = browser_window_navigate(bw,
			    			url, NULL,
							(browser_window_nav_flags)(BW_NAVIGATE_HISTORY),
						    NULL, NULL, NULL);
					nsurl_unref(url);
				}
				if (nserr != NSERROR_OK) {
					warn_user(messages_get_errorcode(nserr), 0);
				}
			}
		}
			break;
		case APPLICATION_QUIT:
			netsurf_quit = true;
			break;
		default:
			break;
	}
}

void nsbeos_scaffolding_destroy(nsbeos_scaffolding *scaffold)
{
	LOG(("Being Destroyed = %d", scaffold->being_destroyed));
	if (scaffold->being_destroyed) return;
	scaffold->being_destroyed = 1;
	nsbeos_window_destroy_event(scaffold->window, scaffold, NULL);
}


void nsbeos_window_update_back_forward(struct beos_scaffolding *g)
{
	struct browser_window *bw = nsbeos_get_browser_for_gui(g->top_level);

	if (!g->top_view->LockLooper())
		return;

	g->back_button->SetEnabled(browser_window_history_back_available(bw));
	g->forward_button->SetEnabled(browser_window_history_forward_available(bw));

	g->top_view->UnlockLooper();

}

void nsbeos_throb(void *p)
{
	struct beos_scaffolding *g = (struct beos_scaffolding *)p;

	if (g->throb_frame >= (nsbeos_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	if (!g->top_view->LockLooper())
		return;

	g->throbber->SetBitmap(nsbeos_throbber->framedata[g->throb_frame]);
	g->throbber->Invalidate();

	g->top_view->UnlockLooper();

	beos_schedule(100, nsbeos_throb, p);

}


NSBrowserWindow *nsbeos_find_last_window(void)
{
	int32 i;
	if (!be_app || !be_app->Lock())
		return NULL;
	for (i = be_app->CountWindows() - 1; i >= 0; i--) {
		if (be_app->WindowAt(i) == NULL)
			continue;
		NSBrowserWindow *win;
		win = dynamic_cast<NSBrowserWindow *>(be_app->WindowAt(i));
		if (win) {
			win->Lock();
			be_app->Unlock();
			return win;
		}
	}
	be_app->Unlock();
	return NULL;
}

NSBrowserWindow *nsbeos_get_bwindow_for_scaffolding(nsbeos_scaffolding *scaffold)
{
	 return scaffold->window;
}

NSBaseView *nsbeos_get_baseview_for_scaffolding(nsbeos_scaffolding *scaffold)
{
	 return scaffold->top_view;
}

static void recursively_set_menu_items_target(BMenu *menu, BHandler *handler)
{
	menu->SetTargetForItems(handler);
	for (int i = 0; menu->ItemAt(i); i++) {
		if (!menu->SubmenuAt(i))
			continue;
		recursively_set_menu_items_target(menu->SubmenuAt(i), handler);
	}
}

void nsbeos_attach_toplevel_view(nsbeos_scaffolding *g, BView *view)
{
	LOG(("Attaching view to scaffolding %p", g));

	// this is a replicant,... and it went bad
	if (!g->window) {
		if (g->top_view->Looper() && !g->top_view->LockLooper())
			return;
	}

	BRect rect(g->top_view->Bounds());
	rect.top += TOOLBAR_HEIGHT;
	rect.right -= B_V_SCROLL_BAR_WIDTH;
	rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
	
	view->ResizeTo(rect.Width() /*+ 1*/, rect.Height() /*+ 1*/);
	view->MoveTo(rect.LeftTop());


	g->scroll_view = new BScrollView("NetSurfScrollView", view, 
		B_FOLLOW_ALL, 0, true, true, B_NO_BORDER);

	g->top_view->AddChild(g->scroll_view);

	// for replicants, add a NSResizeKnob to allow resizing
	if (!g->window) {
		BRect frame = g->scroll_view->Bounds();
		frame.left = frame.right - B_V_SCROLL_BAR_WIDTH;
		frame.top = frame.bottom - B_H_SCROLL_BAR_HEIGHT;
		NSResizeKnob *knob = new NSResizeKnob(frame, g->top_view);
		//TODO: set bitmap
		g->scroll_view->AddChild(knob);
	}

	view->MakeFocus();

	// resize the horiz scrollbar to make room for the status bar and add it.

	BScrollBar *sb = g->scroll_view->ScrollBar(B_HORIZONTAL);
	rect = sb->Frame();
	float divider = rect.Width() + 1;
	//divider /= 2;
	divider *= 67.0/100; // 67%

	sb->ResizeBy(-divider, 0);
	sb->MoveBy(divider, 0);

	rect.right = rect.left + divider - 1;

	/*
	BBox *statusBarBox = new BBox(rect, "StatusBarBox", 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM,
		B_WILL_DRAW | B_FRAME_EVENTS,
		B_RAISED_BORDER);
	*/

	g->status_bar->MoveTo(rect.LeftTop());
	g->status_bar->ResizeTo(rect.Width() + 1, rect.Height() + 1);
	g->scroll_view->AddChild(g->status_bar);
	g->status_bar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->status_bar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;
#if defined(__HAIKU__) || defined(B_DANO_VERSION)
	g->status_bar->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
#endif



	// set targets to the topmost ns view,
	// we might not have a window later (replicant ?)
	// this won't work for replicants, since the base view isn't attached yet
	// we'll redo this in NSBaseView::AllAttached
	g->back_button->SetTarget(view);
	g->forward_button->SetTarget(view);
	g->stop_button->SetTarget(view);
	g->reload_button->SetTarget(view);
	g->home_button->SetTarget(view);

	g->url_bar->SetTarget(view);

	nsbeos_scaffolding_update_colors(g);

	if (g->window) {
		recursively_set_menu_items_target(g->menu_bar, view);

		// add toolbar shortcuts
		BMessage *message;

		message = new BMessage('back');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut(B_LEFT_ARROW, 0, message, view);

		message = new BMessage('forw');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut(B_RIGHT_ARROW, 0, message, view);

		message = new BMessage('stop');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut('S', 0, message, view);

		message = new BMessage('relo');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut('R', 0, message, view);

		message = new BMessage('home');
		message->AddPointer("scaffolding", g);
		g->window->AddShortcut('H', 0, message, view);


#if defined(__HAIKU__)
		// Make sure the window is layouted and answering to events, but do not
		// show it before it is actually resized
		g->window->Hide();
		g->window->Show();

		if(NSBrowserWindow::activeWindow) {
			BWindowStack stack(NSBrowserWindow::activeWindow);
			stack.AddWindow(g->window);
		}
#endif
		g->window->Show();

	} else {
		if (g->top_view->Looper())
			g->top_view->UnlockLooper();
	}


}

static BMenuItem *make_menu_item(const char *name, BMessage *message)
{
	BMenuItem *item;
	BString label(messages_get(name));
	BString accel;
	uint32 mods = 0;
	char key = 0;
	// try to understand accelerators
	int32 start = label.IFindLast(" ");
	if (start > 0 && (label.Length() - start > 1)
		&& (label.Length() - start < 7) 
		&& (label[start + 1] == 'F' 
		|| !strcmp(label.String() + start + 1, "PRINT")
		|| label[start + 1] == '\xe2'
		|| label[start + 1] == '^')) {

		label.MoveInto(accel, start + 1, label.Length());
		// strip the trailing spaces
		while (label[label.Length() - 1] == ' ')
			label.Truncate(label.Length() - 1);

		if (accel.FindFirst("\xe2\x87\x91") > -1) {
			accel.RemoveFirst("\xe2\x87\x91");
			mods |= B_SHIFT_KEY;
		}
		if (accel.FindFirst("^") > -1) {
			accel.RemoveFirst("^");
			mods |= B_CONTROL_KEY; // ALT!!!
		}
		if (accel.FindFirst("PRINT") > -1) {
			accel.RemoveFirst("PRINT");
			//mods |= ; // ALT!!!
			key = B_PRINT_KEY;
		}
		if (accel.Length() > 1 && accel[0] == 'F') { // Function key
			int num;
			if (sscanf(accel.String(), "F%d", &num) > 0) {
				//
			}
		} else if (accel.Length() > 0) {
			key = accel[0];
		}
		//printf("MENU: detected 	accel '%s' mods 0x%08lx, key %d\n", accel.String(), mods, key);
	}

	// turn ... into ellipsis
	label.ReplaceAll("...", B_UTF8_ELLIPSIS);

	item = new BMenuItem(label.String(), message, key, mods);

	return item;
}


class BBitmapButton: public BButton
{
	public:
		BBitmapButton(BRect rect, const char* name, const char* label,
			BMessage* message);
		~BBitmapButton();

		void Draw(BRect updateRect);
		void SetBitmap(const char* attrName);
	private:
		BBitmap* fBitmap;
		BBitmap* fDisabledBitmap;
};


BBitmapButton::BBitmapButton(BRect rect, const char* name, const char* label,
		BMessage* message)
	: BButton(rect, name, label, message)
{
	SetBitmap(name);
}


BBitmapButton::~BBitmapButton()
{
	delete fBitmap;
	delete fDisabledBitmap;
}


void BBitmapButton::Draw(BRect updateRect)
{
	if(fBitmap == NULL) {
		BButton::Draw(updateRect);
		return;
	}

	SetDrawingMode(B_OP_COPY);
	FillRect(updateRect, B_SOLID_LOW);
	rgb_color color = LowColor();

	SetDrawingMode(B_OP_ALPHA);
	if(IsEnabled()) {
		if(Value() != 0) {
			// button is clicked
			DrawBitmap(fBitmap, BPoint(1, 1));
		} else {
			// button is released
			DrawBitmap(fBitmap, BPoint(0, 0));
		}
	} else
		DrawBitmap(fDisabledBitmap, BPoint(0, 0));
}


void BBitmapButton::SetBitmap(const char* attrname)
{
#ifdef __HAIKU__
	size_t size = 0;
	const void* data = gAppResources->LoadResource('VICN', attrname, &size);

	if (!data) {
		printf("CANT LOAD RESOURCE %s\n", attrname);
		return;
	}

	fBitmap = new BBitmap(BRect(0, 0, 32, 32), B_RGB32);
	status_t status = BIconUtils::GetVectorIcon((const uint8*)data, size, fBitmap);
	
	if(status != B_OK) {
		fprintf(stderr, "%s > oops %s\n", attrname, strerror(status));
		delete fBitmap;
		fBitmap = NULL;
	}

	fDisabledBitmap = new BBitmap(fBitmap);
	rgb_color* pixel = (rgb_color*)fDisabledBitmap->Bits();
	for(int i = 0; i < fDisabledBitmap->BitsLength()/4; i++)
	{
		*pixel = tint_color(*pixel, B_DISABLED_MARK_TINT);
		pixel++;
	}
#else
	// No vector icon support on BeOS. We could try to load a bitmap one
	fBitmap = NULL;
	fDisabledBitmap = NULL;
#endif
}


nsbeos_scaffolding *nsbeos_new_scaffolding(struct gui_window *toplevel)
{
	struct beos_scaffolding *g = (struct beos_scaffolding *)malloc(sizeof(*g));

	LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));

	g->top_level = toplevel;
	g->being_destroyed = 0;
	g->fullscreen = false;

	open_windows++;

	BMessage *message;
	BRect rect;

	g->window = NULL;
	g->menu_bar = NULL;

	if (replicated && !replicant_view) {
		warn_user("Error: No subwindow allowed when replicated.", NULL);
		return NULL;
	}


	if (!replicant_view) {
		BRect frame(0, 0, 600-1, 500-1);
		if (nsoption_int(window_width) > 0) {
			frame.Set(0, 0, nsoption_int(window_width) - 1, nsoption_int(window_height) - 1);
			frame.OffsetToSelf(nsoption_int(window_x), nsoption_int(window_y));
		} else {
			BPoint pos(50, 50);
			// XXX: use last BApplication::WindowAt()'s dynamic_cast<NSBrowserWindow *> Frame()
			NSBrowserWindow *win = nsbeos_find_last_window();
			if (win) {
				pos = win->Frame().LeftTop();
				win->UnlockLooper();
			}
			pos += BPoint(20, 20);
			BScreen screen;
			BRect screenFrame(screen.Frame());
			if (pos.y + frame.Height() >= screenFrame.Height()) {
				pos.y = 50;
				pos.x += 50;
			}
			if (pos.x + frame.Width() >= screenFrame.Width()) {
				pos.x = 50;
				pos.y = 50;
			}
			frame.OffsetToSelf(pos);
		}

		g->window = new NSBrowserWindow(frame, g);

		rect = frame.OffsetToCopy(0,0);
		rect.bottom = rect.top + 20;

		// build menus
		g->menu_bar = new BMenuBar(rect, "menu_bar");
		g->window->AddChild(g->menu_bar);

		BMenu *menu;
		BMenu *submenu;
		BMenuItem *item;

		// App menu
		//XXX: use icon item ?

		menu = new BMenu(messages_get("NetSurf"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("Info", message);
		menu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("AppHelp", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Open"));
		menu->AddItem(submenu);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("OpenURL", message);
		submenu->AddItem(item);

		message = new BMessage(CHOICES_SHOW);
		item = make_menu_item("Choices", message);
		menu->AddItem(item);

		message = new BMessage(APPLICATION_QUIT);
		item = make_menu_item("Quit", message);
		menu->AddItem(item);

		// Page menu

		menu = new BMenu(messages_get("Page"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_PAGE_INFO);
		item = make_menu_item("PageInfo", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_SAVE);
		item = make_menu_item("Save", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_SAVE_COMPLETE);
		item = make_menu_item("SaveComp", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Export"));
		menu->AddItem(submenu);

		/*
		message = new BMessage(BROWSER_EXPORT_DRAW);
		item = make_menu_item("Draw", message);
		submenu->AddItem(item);
		*/

		message = new BMessage(BROWSER_EXPORT_TEXT);
		item = make_menu_item("Text", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("SaveURL"));
		menu->AddItem(submenu);

		//XXX
		message = new BMessage(BROWSER_OBJECT_SAVE_URL_URL);
		item = make_menu_item("URL", message);
		submenu->AddItem(item);


		message = new BMessage(BROWSER_PRINT);
		item = make_menu_item("Print", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NEW_WINDOW);
		item = make_menu_item("NewWindow", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_VIEW_SOURCE);
		item = make_menu_item("ViewSrc", message);
		menu->AddItem(item);

		// Object menu

		menu = new BMenu(messages_get("Object"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_OBJECT_INFO);
		item = make_menu_item("ObjInfo", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_OBJECT_SAVE);
		item = make_menu_item("ObjSave", message);
		menu->AddItem(item);
		// XXX: submenu: Sprite ?

		message = new BMessage(BROWSER_OBJECT_RELOAD);
		item = make_menu_item("ObjReload", message);
		menu->AddItem(item);

		// Navigate menu

		menu = new BMenu(messages_get("Navigate"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_NAVIGATE_HOME);
		item = make_menu_item("Home", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_BACK);
		item = make_menu_item("Back", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_FORWARD);
		item = make_menu_item("Forward", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_UP);
		item = make_menu_item("UpLevel", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_RELOAD);
		item = make_menu_item("Reload", message);
		menu->AddItem(item);

		message = new BMessage(BROWSER_NAVIGATE_STOP);
		item = make_menu_item("Stop", message);
		menu->AddItem(item);

		// View menu

		menu = new BMenu(messages_get("View"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(BROWSER_SCALE_VIEW);
		item = make_menu_item("ScaleView", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Images"));
		menu->AddItem(submenu);

		message = new BMessage(BROWSER_IMAGES_FOREGROUND);
		item = make_menu_item("ForeImg", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_IMAGES_BACKGROUND);
		item = make_menu_item("BackImg", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("Toolbars"));
		menu->AddItem(submenu);
		submenu->SetEnabled(false);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolButtons", message);
		submenu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolAddress", message);
		submenu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolThrob", message);
		submenu->AddItem(item);

		message = new BMessage(NO_ACTION);
		item = make_menu_item("ToolStatus", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("Render"));
		menu->AddItem(submenu);

		message = new BMessage(BROWSER_BUFFER_ANIMS);
		item = make_menu_item("RenderAnims", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_BUFFER_ALL);
		item = make_menu_item("RenderAll", message);
		submenu->AddItem(item);


		message = new BMessage(NO_ACTION);
		item = make_menu_item("OptDefault", message);
		menu->AddItem(item);

		// Utilities menu

		menu = new BMenu(messages_get("Utilities"));
		g->menu_bar->AddItem(menu);

		submenu = new BMenu(messages_get("Hotlist"));
		menu->AddItem(submenu);

		message = new BMessage(HOTLIST_ADD_URL);
		item = make_menu_item("HotlistAdd", message);
		submenu->AddItem(item);

		message = new BMessage(HOTLIST_SHOW);
		item = make_menu_item("HotlistShow", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("History"));
		menu->AddItem(submenu);

		message = new BMessage(HISTORY_SHOW_LOCAL);
		item = make_menu_item("HistLocal", message);
		submenu->AddItem(item);

		message = new BMessage(HISTORY_SHOW_GLOBAL);
		item = make_menu_item("HistGlobal", message);
		submenu->AddItem(item);


		submenu = new BMenu(messages_get("Cookies"));
		menu->AddItem(submenu);

		message = new BMessage(COOKIES_SHOW);
		item = make_menu_item("ShowCookies", message);
		submenu->AddItem(item);

		message = new BMessage(COOKIES_DELETE);
		item = make_menu_item("DeleteCookies", message);
		submenu->AddItem(item);


		message = new BMessage(BROWSER_FIND_TEXT);
		item = make_menu_item("FindText", message);
		menu->AddItem(item);

		submenu = new BMenu(messages_get("Window"));
		menu->AddItem(submenu);

		message = new BMessage(BROWSER_WINDOW_DEFAULT);
		item = make_menu_item("WindowSave", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_WINDOW_STAGGER);
		item = make_menu_item("WindowStagr", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_WINDOW_COPY);
		item = make_menu_item("WindowSize", message);
		submenu->AddItem(item);

		message = new BMessage(BROWSER_WINDOW_RESET);
		item = make_menu_item("WindowReset", message);
		submenu->AddItem(item);


		// Help menu

		menu = new BMenu(messages_get("Help"));
		g->menu_bar->AddItem(menu);

		message = new BMessage(HELP_OPEN_CONTENTS);
		item = make_menu_item("HelpContent", message);
		menu->AddItem(item);

		message = new BMessage(HELP_OPEN_GUIDE);
		item = make_menu_item("HelpGuide", message);
		menu->AddItem(item);

		message = new BMessage(HELP_OPEN_INFORMATION);
		item = make_menu_item("HelpInfo", message);
		menu->AddItem(item);

		message = new BMessage(HELP_OPEN_ABOUT);
		item = make_menu_item("HelpAbout", message);
		menu->AddItem(item);

		message = new BMessage(HELP_LAUNCH_INTERACTIVE);
		item = make_menu_item("HelpInter", message);
		menu->AddItem(item);

		// the base view that receives the toolbar, statusbar and top-level view.
		rect = frame.OffsetToCopy(0,0);
		rect.top = g->menu_bar->Bounds().Height() + 1;
		//rect.top = 20 + 1; // XXX
		//rect.bottom -= B_H_SCROLL_BAR_HEIGHT;
		g->top_view = new NSBaseView(rect);
		// add the top view to the window
		g->window->AddChild(g->top_view);
	} else { // replicant_view
		// the base view has already been created with the archive constructor
		g->top_view = replicant_view;
	}
	g->top_view->SetScaffolding(g);

	// build popup menu
	g->popup_menu = new BPopUpMenu("");


	// the dragger to allow replicating us
	// XXX: try to stuff it in the status bar at the bottom
	// (BDragger *must* be a parent, sibiling or direct child of NSBaseView!)
	rect = g->top_view->Bounds();
	rect.bottom = rect.top + TOOLBAR_HEIGHT - 1;
	rect.left = rect.right - DRAGGER_WIDTH + 1;
	g->dragger = new BDragger(rect, g->top_view, 
		B_FOLLOW_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW);
	g->top_view->AddChild(g->dragger);
	g->dragger->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->dragger->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;

	// tool_bar
	// the toolbar is also the dragger for now
	// XXX: try to stuff it in the status bar at the bottom
	// (BDragger *must* be a parent, sibiling or direct child of NSBaseView!)
	// XXX: B_FULL_UPDATE_ON_RESIZE avoids leaving bits on resize,
	// but causes flicker
	rect = g->top_view->Bounds();
	rect.bottom = rect.top + TOOLBAR_HEIGHT - 1;
	rect.right = rect.right - DRAGGER_WIDTH;
	g->tool_bar = new BBox(rect, "Toolbar", 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW | B_FRAME_EVENTS
		| B_FULL_UPDATE_ON_RESIZE | B_NAVIGABLE_JUMP, B_PLAIN_BORDER);
	g->top_view->AddChild(g->tool_bar);
	g->tool_bar->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	g->tool_bar->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR)) ;

	// buttons
	rect = g->tool_bar->Bounds();
	rect.right = TOOLBAR_HEIGHT;
	rect.InsetBySelf(5, 5);
	rect.OffsetBySelf(0, -1);
	int nButtons = 0;

	message = new BMessage('back');
	message->AddPointer("scaffolding", g);
	g->back_button = new BBitmapButton(rect, "back_button", "<", message);
	g->tool_bar->AddChild(g->back_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('forw');
	message->AddPointer("scaffolding", g);
	g->forward_button = new BBitmapButton(rect, "forward_button", ">", message);
	g->tool_bar->AddChild(g->forward_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('stop');
	message->AddPointer("scaffolding", g);
	g->stop_button = new BBitmapButton(rect, "stop_button", "S", message);
	g->tool_bar->AddChild(g->stop_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('relo');
	message->AddPointer("scaffolding", g);
	g->reload_button = new BBitmapButton(rect, "reload_button", "R", message);
	g->tool_bar->AddChild(g->reload_button);
	nButtons++;

	rect.OffsetBySelf(TOOLBAR_HEIGHT, 0);
	message = new BMessage('home');
	message->AddPointer("scaffolding", g);
	g->home_button = new BBitmapButton(rect, "home_button", "H", message);
	g->tool_bar->AddChild(g->home_button);
	nButtons++;


	// url bar
	rect = g->tool_bar->Bounds();
	rect.left += TOOLBAR_HEIGHT * nButtons;
	rect.right -= TOOLBAR_HEIGHT * 1;
	rect.InsetBySelf(5, 5);
	message = new BMessage('urle');
	message->AddPointer("scaffolding", g);
	g->url_bar = new NSIconTextControl(rect, "url_bar", "", "", message, 
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	g->url_bar->SetDivider(0);
	rect = g->url_bar->TextView()->TextRect();
	rect.left += 16;
	g->url_bar->TextView()->SetTextRect(rect);
	g->tool_bar->AddChild(g->url_bar);


	// throbber
	rect.Set(0, 0, 24, 24);
	rect.OffsetTo(g->tool_bar->Bounds().right - 24 - (TOOLBAR_HEIGHT - 24) / 2,
		(TOOLBAR_HEIGHT - 24) / 2);
	g->throbber = new NSThrobber(rect);
	g->tool_bar->AddChild(g->throbber);
	g->throbber->SetViewColor(g->tool_bar->ViewColor());
	g->throbber->SetLowColor(g->tool_bar->ViewColor());
	g->throbber->SetDrawingMode(B_OP_ALPHA);
	g->throbber->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	/* set up the throbber. */
	g->throbber->SetBitmap(nsbeos_throbber->framedata[0]);
	g->throb_frame = 0;


	// the status bar at the bottom
	BString status("NetSurf");
	status << " " << netsurf_version;
	g->status_bar = new BStringView(BRect(0,0,-1,-1), "StatusBar", 
		status.String(), B_FOLLOW_LEFT/*_RIGHT*/ | B_FOLLOW_BOTTOM);

	// will be added to the scrollview when adding the top view.

	// notify the thread creating the replicant that we're done
	if (replicant_view)
		release_sem(replicant_done_sem);

	replicant_view = NULL;

	return g;
}

void gui_window_set_title(struct gui_window *_g, const char *title)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);
	if (g->top_level != _g) return;

	// if we're a replicant, discard
	if (!g->window)
		return;

	BString nt(title);
	if (nt.Length())
		nt << " - ";
	nt << "NetSurf";

	if (!g->top_view->LockLooper())
		return;

	g->window->SetTitle(nt.String());

	g->top_view->UnlockLooper();
}

void gui_window_set_status(struct gui_window *_g, const char *text)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);
	assert(g);
	assert(g->status_bar);

	if (!g->top_view->LockLooper())
		return;

	if (text == NULL || text[0] == '\0')
	{
		BString status("NetSurf");
		status << " " << netsurf_version;
		g->status_bar->SetText(status.String());
	}
	else
	{
		g->status_bar->SetText(text);
	}
	g->top_view->UnlockLooper();
}

void gui_window_set_url(struct gui_window *_g, const char *url)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);
	if (g->top_level != _g) return;
	assert(g->status_bar);

	if (!g->top_view->LockLooper())
		return;

	g->url_bar->SetText(url);

	g->top_view->UnlockLooper();
}

void gui_window_start_throbber(struct gui_window* _g)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);

	if (!g->top_view->LockLooper())
		return;

	g->stop_button->SetEnabled(true);
	g->reload_button->SetEnabled(false);

	g->top_view->UnlockLooper();

	nsbeos_window_update_back_forward(g);

	beos_schedule(100, nsbeos_throb, g);
}

void gui_window_stop_throbber(struct gui_window* _g)
{
	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);

	nsbeos_window_update_back_forward(g);

	beos_schedule(-1, nsbeos_throb, g);

	if (!g->top_view->LockLooper())
		return;

	g->stop_button->SetEnabled(false);
	g->reload_button->SetEnabled(true);

	g->throbber->SetBitmap(nsbeos_throbber->framedata[0]);
	g->throbber->Invalidate();

	g->top_view->UnlockLooper();
}

/**
 * add retrieved favicon to the gui
 */
void gui_window_set_icon(struct gui_window *_g, hlcache_handle *icon)
{
	BBitmap *bitmap = NULL;
    struct bitmap *bmp_icon;

    bmp_icon = (icon != NULL) ? content_get_bitmap(icon) : NULL;

	if (bmp_icon) {
		bitmap = nsbeos_bitmap_get_primary(bmp_icon);
	}

	struct beos_scaffolding *g = nsbeos_get_scaffold(_g);

	if (!g->top_view->LockLooper())
		return;

	g->url_bar->SetBitmap(bitmap);

	g->top_view->UnlockLooper();
}


void nsbeos_scaffolding_popup_menu(nsbeos_scaffolding *g, BPoint where)
{
	g->popup_menu->Go(where);
}

