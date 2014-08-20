/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
#include <unistd.h>
#include <string.h>

#include "utils/config.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "content/urldb.h"
#include "content/fetch.h"
#include "css/utils.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "render/html.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "windows/window.h"
#include "windows/about.h"
#include "windows/gui.h"
#include "windows/drawable.h"
#include "windows/font.h"
#include "windows/localhistory.h"
#include "windows/plot.h"
#include "windows/prefs.h"
#include "windows/resourceid.h"
#include "windows/schedule.h"
#include "windows/findfile.h"
#include "windows/windbg.h"
#include "windows/filetype.h"

HINSTANCE hInstance; /** win32 application instance handle. */

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;
HWND font_hwnd;

static int open_windows = 0;

static const char windowclassname_main[] = "nswsmainwindow";

#define NSWS_THROBBER_WIDTH 24
#define NSWS_URL_ENTER (WM_USER)

static struct nsws_pointers nsws_pointer;

void gui_window_set_scroll(struct gui_window *w, int sx, int sy);
static bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy);


static void win32_poll(bool active)
{
	MSG Msg; /* message from system */
	BOOL bRet; /* message fetch result */
	int timeout; /* timeout in miliseconds */
	UINT timer_id = 0;

	/* run the scheduler and discover how long to wait for the next event */
	timeout = schedule_run();

	/* if active set timeout so message is not waited for */
	if (active)
		timeout = 0;

	if (timeout == 0) {
		bRet = PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE);
	} else {
		if (timeout > 0) {
			/* set up a timer to ensure we get woken */
			timer_id = SetTimer(NULL, 0, timeout, NULL);
		}

		/* wait for a message */
		bRet = GetMessage(&Msg, NULL, 0, 0);

		/* if a timer was sucessfully created remove it */
		if (timer_id != 0) {
			KillTimer(NULL, timer_id);
		}
	}


	if (bRet > 0) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
}


bool
nsws_window_go(HWND hwnd, const char *urltxt)
{
	struct gui_window *gw;
	nsurl *url;

	gw = nsws_get_gui_window(hwnd);
	if (gw == NULL)
		return false;

	if (nsurl_create(urltxt, &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(gw->bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return true;
}

/**
 * callback for url bar events
 */
static LRESULT CALLBACK
nsws_window_urlbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
	WNDPROC urlproc;
	HFONT hFont;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	gw = nsws_get_gui_window(hwnd);

	urlproc = (WNDPROC)GetProp(hwnd, TEXT("OrigMsgProc"));

	/* override messages */
	switch (msg) {
	case WM_CHAR:
		if (wparam == 13) {
			SendMessage(gw->main, WM_COMMAND, IDC_MAIN_LAUNCH_URL, 0);
			return 0;
		}
		break;

	case WM_DESTROY:
		hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
		if (hFont != NULL) {
			LOG(("Destroyed font object"));
			DeleteObject(hFont); 	
		}


	case WM_NCDESTROY:
		/* remove properties if window is being destroyed */
		RemoveProp(hwnd, TEXT("GuiWnd"));
		RemoveProp(hwnd, TEXT("OrigMsgProc"));
		break;
	}

	if (urlproc == NULL) {
		/* the original toolbar procedure is not available */
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	/* chain to the next handler */
	return CallWindowProc(urlproc, hwnd, msg, wparam, lparam);
}

/* calculate the dimensions of the url bar relative to the parent toolbar */
static void
urlbar_dimensions(HWND hWndParent,
		  int toolbuttonsize,
		  int buttonc,
		  int *x,
		  int *y,
		  int *width,
		  int *height)
{
	RECT rc;
	const int cy_edit = 23;

	GetClientRect(hWndParent, &rc);
	*x = (toolbuttonsize + 1) * (buttonc + 1) + (NSWS_THROBBER_WIDTH>>1);
	*y = ((((rc.bottom - 1) - cy_edit) >> 1) * 2) / 3;
	*width = (rc.right - 1) - *x - (NSWS_THROBBER_WIDTH>>1) - NSWS_THROBBER_WIDTH;
	*height = cy_edit;
}


static LRESULT
nsws_window_toolbar_command(struct gui_window *gw,
		    int notification_code,
		    int identifier,
		    HWND ctrl_window)
{
	LOG(("notification_code %d identifier %d ctrl_window %p",
	     notification_code, identifier,  ctrl_window));

	switch(identifier) {

	case IDC_MAIN_URLBAR:
		switch (notification_code) {
		case EN_CHANGE:
			LOG(("EN_CHANGE"));
			break;

		case EN_ERRSPACE:
			LOG(("EN_ERRSPACE"));
			break;

		case EN_HSCROLL:
			LOG(("EN_HSCROLL"));
			break;

		case EN_KILLFOCUS:
			LOG(("EN_KILLFOCUS"));
			break;

		case EN_MAXTEXT:
			LOG(("EN_MAXTEXT"));
			break;

		case EN_SETFOCUS:
			LOG(("EN_SETFOCUS"));
			break;

		case EN_UPDATE:
			LOG(("EN_UPDATE"));
			break;

		case EN_VSCROLL:
			LOG(("EN_VSCROLL"));
			break;

		default:
			LOG(("Unknown notification_code"));
			break;
		}
		break;

	default:
		return 1; /* unhandled */

	}
	return 0; /* control message handled */
}

/**
 * callback for toolbar events
 */
static LRESULT CALLBACK
nsws_window_toolbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
	int urlx, urly, urlwidth, urlheight;
	WNDPROC toolproc;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	gw = nsws_get_gui_window(hwnd);

	switch (msg) {
	case WM_SIZE:

		urlbar_dimensions(hwnd,
				  gw->toolbuttonsize,
				  gw->toolbuttonc,
				  &urlx, &urly, &urlwidth, &urlheight);

		/* resize url */
		if (gw->urlbar != NULL) {
			MoveWindow(gw->urlbar, urlx, urly, urlwidth, urlheight, true);
		}

		/* move throbber */
		if (gw->throbber != NULL) {
			MoveWindow(gw->throbber,
				   LOWORD(lparam) - NSWS_THROBBER_WIDTH - 4, 8,
				   NSWS_THROBBER_WIDTH, NSWS_THROBBER_WIDTH,
				   true);
		}
		break;

	case WM_COMMAND:
		if (nsws_window_toolbar_command(gw,
						HIWORD(wparam),
						LOWORD(wparam),
						(HWND)lparam) == 0)
			return 0;
		break;
	}

	/* remove properties if window is being destroyed */
	if (msg == WM_NCDESTROY) {
		RemoveProp(hwnd, TEXT("GuiWnd"));
		toolproc = (WNDPROC)RemoveProp(hwnd, TEXT("OrigMsgProc"));
	} else {
		toolproc = (WNDPROC)GetProp(hwnd, TEXT("OrigMsgProc"));
	}

	if (toolproc == NULL) {
		/* the original toolbar procedure is not available */
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	/* chain to the next handler */
	return CallWindowProc(toolproc, hwnd, msg, wparam, lparam);

}

/**
 * update state of forward/back buttons/menu items when page changes
 */
static void nsws_window_update_forward_back(struct gui_window *w)
{
	if (w->bw == NULL)
		return;

	bool forward = browser_window_history_forward_available(w->bw);
	bool back = browser_window_history_back_available(w->bw);

	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, IDM_NAV_FORWARD,
			       (forward ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->mainmenu, IDM_NAV_BACK,
			       (back ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->rclick, IDM_NAV_FORWARD,
			       (forward ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->rclick, IDM_NAV_BACK,
			       (back ? MF_ENABLED : MF_GRAYED));
	}

	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_FORWARD,
			    MAKELONG((forward ? TBSTATE_ENABLED :
				      TBSTATE_INDETERMINATE), 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_BACK,
			    MAKELONG((back ? TBSTATE_ENABLED :
				      TBSTATE_INDETERMINATE), 0));
	}
}

static void nsws_update_edit(struct gui_window *w)
{
	browser_editor_flags editor_flags = (w->bw == NULL) ?
			BW_EDITOR_NONE : browser_window_get_editor_flags(w->bw);
	bool paste, copy, del;
	bool sel = (editor_flags & BW_EDITOR_CAN_COPY);
	if (GetFocus() == w->urlbar) {
		DWORD i, ii;
		SendMessage(w->urlbar, EM_GETSEL, (WPARAM)&i, (LPARAM)&ii);
		paste = true;
		copy = (i != ii);
		del = (i != ii);

	} else if (sel){
		paste = (editor_flags & BW_EDITOR_CAN_PASTE);
		copy = sel;
		del = (editor_flags & BW_EDITOR_CAN_CUT);
	} else {
		paste = false;
		copy = false;
		del = false;
	}
	EnableMenuItem(w->mainmenu,
		       IDM_EDIT_PASTE,
		       (paste ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->rclick,
		       IDM_EDIT_PASTE,
		       (paste ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->mainmenu,
		       IDM_EDIT_COPY,
		       (copy ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->rclick,
		       IDM_EDIT_COPY,
		       (copy ? MF_ENABLED : MF_GRAYED));

	if (del == true) {
		EnableMenuItem(w->mainmenu, IDM_EDIT_CUT, MF_ENABLED);
		EnableMenuItem(w->mainmenu, IDM_EDIT_DELETE, MF_ENABLED);
		EnableMenuItem(w->rclick, IDM_EDIT_CUT, MF_ENABLED);
		EnableMenuItem(w->rclick, IDM_EDIT_DELETE, MF_ENABLED);
	} else {
		EnableMenuItem(w->mainmenu, IDM_EDIT_CUT, MF_GRAYED);
		EnableMenuItem(w->mainmenu, IDM_EDIT_DELETE, MF_GRAYED);
		EnableMenuItem(w->rclick, IDM_EDIT_CUT, MF_GRAYED);
		EnableMenuItem(w->rclick, IDM_EDIT_DELETE, MF_GRAYED);
	}
}

static bool
nsws_ctx_menu(struct gui_window *w, HWND hwnd, int x, int y)
{
	RECT rc; /* client area of window */
	POINT pt = { x, y }; /* location of mouse click */

	/* Get the bounding rectangle of the client area. */
	GetClientRect(hwnd, &rc);

	/* Convert the mouse position to client coordinates. */
	ScreenToClient(hwnd, &pt);

	/* If the position is in the client area, display a shortcut menu. */
	if (PtInRect(&rc, pt)) {
		ClientToScreen(hwnd, &pt);
		nsws_update_edit(w);
		TrackPopupMenu(GetSubMenu(w->rclick, 0),
			       TPM_CENTERALIGN | TPM_TOPALIGN,
			       x,
			       y,
			       0,
			       hwnd,
			       NULL);

		return true;
	}

	/* Return false if no menu is displayed. */
	return false;
}

/**
 * set accelerators
 */
static void nsws_window_set_accels(struct gui_window *w)
{
	int i, nitems = 13;
	ACCEL accels[nitems];
	for (i = 0; i < nitems; i++)
		accels[i].fVirt = FCONTROL | FVIRTKEY;
	accels[0].key = 0x51; /* Q */
	accels[0].cmd = IDM_FILE_QUIT;
	accels[1].key = 0x4E; /* N */
	accels[1].cmd = IDM_FILE_OPEN_WINDOW;
	accels[2].key = VK_LEFT;
	accels[2].cmd = IDM_NAV_BACK;
	accels[3].key = VK_RIGHT;
	accels[3].cmd = IDM_NAV_FORWARD;
	accels[4].key = VK_UP;
	accels[4].cmd = IDM_NAV_HOME;
	accels[5].key = VK_BACK;
	accels[5].cmd = IDM_NAV_STOP;
	accels[6].key = VK_SPACE;
	accels[6].cmd = IDM_NAV_RELOAD;
	accels[7].key = 0x4C; /* L */
	accels[7].cmd = IDM_FILE_OPEN_LOCATION;
	accels[8].key = 0x57; /* w */
	accels[8].cmd = IDM_FILE_CLOSE_WINDOW;
	accels[9].key = 0x41; /* A */
	accels[9].cmd = IDM_EDIT_SELECT_ALL;
	accels[10].key = VK_F8;
	accels[10].cmd = IDM_VIEW_SOURCE;
	accels[11].key = VK_RETURN;
	accels[11].fVirt = FVIRTKEY;
	accels[11].cmd = IDC_MAIN_LAUNCH_URL;
	accels[12].key = VK_F11;
	accels[12].fVirt = FVIRTKEY;
	accels[12].cmd = IDM_VIEW_FULLSCREEN;

	w->acceltable = CreateAcceleratorTable(accels, nitems);
}

/**
 * creation of throbber
 */
static HWND
nsws_window_throbber_create(struct gui_window *w)
{
	HWND hwnd;
	char avi[PATH_MAX];

	hwnd = CreateWindow(ANIMATE_CLASS,
			    "",
			    WS_CHILD | WS_VISIBLE | ACS_TRANSPARENT,
			    w->width - NSWS_THROBBER_WIDTH - 4,
			    8,
			    NSWS_THROBBER_WIDTH,
			    NSWS_THROBBER_WIDTH,
			    w->main,
			    (HMENU) IDC_MAIN_THROBBER,
			    hInstance,
			    NULL);

	nsws_find_resource(avi, "throbber.avi", "windows/res/throbber.avi");
	LOG(("setting throbber avi as %s", avi));
	Animate_Open(hwnd, avi);
	if (w->throbbing)
		Animate_Play(hwnd, 0, -1, -1);
	else
		Animate_Seek(hwnd, 0);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	return hwnd;
}

static HIMAGELIST
get_imagelist(int resid, int bsize, int bcnt)
{
	HIMAGELIST hImageList;
	HBITMAP hScrBM;

	LOG(("resource id %d, bzize %d, bcnt %d",resid, bsize, bcnt));

	hImageList = ImageList_Create(bsize, bsize, ILC_COLOR24 | ILC_MASK, 0, bcnt);
	if (hImageList == NULL) 
		return NULL;

	hScrBM = LoadImage(hInstance, MAKEINTRESOURCE(resid),
			   IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);

	if (hScrBM == NULL) {
		win_perror("LoadImage");
		return NULL;		
	}

	if (ImageList_AddMasked(hImageList, hScrBM, 0xcccccc) == -1) {
		/* failed to add masked bitmap */
		ImageList_Destroy(hImageList);
		hImageList = NULL;
	}
	DeleteObject(hScrBM);

	return hImageList;
}

/** create a urlbar and message handler
 *
 * Create an Edit control for enerting urls
 */
static HWND
nsws_window_urlbar_create(struct gui_window *gw, HWND hwndparent)
{
	int urlx, urly, urlwidth, urlheight;
	HWND hwnd;
	WNDPROC	urlproc;
	HFONT hFont;

	urlbar_dimensions(hwndparent,
			  gw->toolbuttonsize,
			  gw->toolbuttonc,
			  &urlx, &urly, &urlwidth, &urlheight);

	/* Create the edit control */
	hwnd = CreateWindowEx(0L,
			      TEXT("Edit"),
			      NULL,
			      WS_CHILD | WS_BORDER | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
			      urlx,
			      urly,
			      urlwidth,
			      urlheight,
			      hwndparent,
			      (HMENU)IDC_MAIN_URLBAR,
			      hInstance,
			      0);

	if (hwnd == NULL) {
		return NULL;
	}

	/* set the gui window associated with this control */
	SetProp(hwnd, TEXT("GuiWnd"), (HANDLE)gw);

	/* subclass the message handler */
	urlproc = (WNDPROC)SetWindowLongPtr(hwnd,
					    GWLP_WNDPROC,
					    (LONG_PTR)nsws_window_urlbar_callback);

	/* save the real handler  */
	SetProp(hwnd, TEXT("OrigMsgProc"), (HANDLE)urlproc);

	hFont = CreateFont(urlheight - 4, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
	if (hFont != NULL) {
		LOG(("Setting font object"));
		SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, 0);
	}

	LOG(("Created url bar hwnd:%p, x:%d, y:%d, w:%d, h:%d", hwnd,urlx, urly, urlwidth,  urlheight));

	return hwnd;
}

/* create a toolbar add controls and message handler */
static HWND
nsws_window_create_toolbar(struct gui_window *gw, HWND hWndParent)
{
	HIMAGELIST hImageList;
	HWND hWndToolbar;
	/* Toolbar buttons */
	TBBUTTON tbButtons[] = {
		{0, IDM_NAV_BACK, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{1, IDM_NAV_FORWARD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{2, IDM_NAV_HOME, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{3, IDM_NAV_RELOAD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{4, IDM_NAV_STOP, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
	};
	WNDPROC	toolproc;

	/* Create the toolbar window and subclass its message handler. */
	hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, "Toolbar",
				     WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
				     0, 0, 0, 0,
				     hWndParent, NULL, HINST_COMMCTRL, NULL);

	if (!hWndToolbar) {
		return NULL;
	}

	/* set the gui window associated with this toolbar */
	SetProp(hWndToolbar, TEXT("GuiWnd"), (HANDLE)gw);

	/* subclass the message handler */
	toolproc = (WNDPROC)SetWindowLongPtr(hWndToolbar,
					     GWLP_WNDPROC,
					     (LONG_PTR)nsws_window_toolbar_callback);

	/* save the real handler  */
	SetProp(hWndToolbar, TEXT("OrigMsgProc"), (HANDLE)toolproc);

	/* remember how many buttons are being created */
	gw->toolbuttonc = sizeof(tbButtons) / sizeof(TBBUTTON);

	/* Create the standard image list and assign to toolbar. */
	hImageList = get_imagelist(IDR_TOOLBAR_BITMAP, gw->toolbuttonsize, gw->toolbuttonc);
	if (hImageList != NULL) 
		SendMessage(hWndToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImageList);

	/* Create the disabled image list and assign to toolbar. */
	hImageList = get_imagelist(IDR_TOOLBAR_BITMAP_GREY, gw->toolbuttonsize, gw->toolbuttonc);
	if (hImageList != NULL) 
		SendMessage(hWndToolbar, TB_SETDISABLEDIMAGELIST, 0, (LPARAM)hImageList);

	/* Create the hot image list and assign to toolbar. */
	hImageList = get_imagelist(IDR_TOOLBAR_BITMAP_HOT, gw->toolbuttonsize, gw->toolbuttonc);
	if (hImageList != NULL) 
		SendMessage(hWndToolbar, TB_SETHOTIMAGELIST, 0, (LPARAM)hImageList);

	/* Add buttons. */
	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)gw->toolbuttonc, (LPARAM)&tbButtons);

	gw->urlbar = nsws_window_urlbar_create(gw, hWndToolbar);

	gw->throbber = nsws_window_throbber_create(gw);

	return hWndToolbar;
}

static LRESULT
nsws_window_resize(struct gui_window *gw,
		   HWND hwnd,
		   WPARAM wparam,
		   LPARAM lparam)
{
	int x, y;
	RECT rstatus, rtool;

	if ((gw->toolbar == NULL) ||
	    (gw->urlbar == NULL) ||
	    (gw->statusbar == NULL))
		return 0;

	SendMessage(gw->statusbar, WM_SIZE, wparam, lparam);
	SendMessage(gw->toolbar, WM_SIZE, wparam, lparam);

	GetClientRect(gw->toolbar, &rtool);
	GetWindowRect(gw->statusbar, &rstatus);
	gui_window_get_scroll(gw, &x, &y);
	gw->width = LOWORD(lparam);
	gw->height = HIWORD(lparam) - (rtool.bottom - rtool.top) - (rstatus.bottom - rstatus.top);

	if (gw->drawingarea != NULL) {
		MoveWindow(gw->drawingarea,
			   0,
			   rtool.bottom,
			   gw->width,
			   gw->height,
			   true);
	}
	nsws_window_update_forward_back(gw);

	gui_window_set_scroll(gw, x, y);

	if (gw->toolbar != NULL) {
		SendMessage(gw->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_STOP,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
	}

	return 0;
}

/**
 * redraw the whole window
 */
static void gui_window_redraw_window(struct gui_window *gw)
{
	/* LOG(("gw:%p", gw)); */
	if (gw == NULL)
		return;

	RedrawWindow(gw->drawingarea, NULL, NULL, RDW_INVALIDATE | RDW_NOERASE);
}


static LRESULT
nsws_window_command(HWND hwnd,
		    struct gui_window *gw,
		    int notification_code,
		    int identifier,
		    HWND ctrl_window)
{
	LOG(("notification_code %x identifier %x ctrl_window %p",
	     notification_code, identifier,  ctrl_window));

	switch(identifier) {

	case IDM_FILE_QUIT:
	{
		struct gui_window *w;
		w = window_list;
		while (w != NULL) {
			PostMessage(w->main, WM_CLOSE, 0, 0);
			w = w->next;
		}
		break;
	}

	case IDM_FILE_OPEN_LOCATION:
		SetFocus(gw->urlbar);
		break;

	case IDM_FILE_OPEN_WINDOW:
		browser_window_create(BW_CREATE_NONE,
				      NULL,
				      NULL,
				      gw->bw,
				      NULL);
		break;

	case IDM_FILE_CLOSE_WINDOW:
		PostMessage(gw->main, WM_CLOSE, 0, 0);
		break;

	case IDM_FILE_SAVE_PAGE:
		break;

	case IDM_FILE_SAVEAS_TEXT:
		break;

	case IDM_FILE_SAVEAS_PDF:
		break;

	case IDM_FILE_SAVEAS_POSTSCRIPT:
		break;

	case IDM_FILE_PRINT_PREVIEW:
		break;

	case IDM_FILE_PRINT:
		break;

	case IDM_EDIT_CUT:
		OpenClipboard(gw->main);
		EmptyClipboard();
		CloseClipboard();
		if (GetFocus() == gw->urlbar) {
			SendMessage(gw->urlbar, WM_CUT, 0, 0);
		} else if (gw->bw != NULL) {
			browser_window_key_press(gw->bw, KEY_CUT_SELECTION);
		}
		break;

	case IDM_EDIT_COPY:
		OpenClipboard(gw->main);
		EmptyClipboard();
		CloseClipboard();
		if (GetFocus() == gw->urlbar) {
			SendMessage(gw->urlbar, WM_COPY, 0, 0);
		} else if (gw->bw != NULL) {
			browser_window_key_press(gw->bw, KEY_COPY_SELECTION);
		}
		break;

	case IDM_EDIT_PASTE: {
		OpenClipboard(gw->main);
		HANDLE h = GetClipboardData(CF_TEXT);
		if (h != NULL) {
			char *content = GlobalLock(h);
			LOG(("pasting %s\n", content));
			GlobalUnlock(h);
		}
		CloseClipboard();
		if (GetFocus() == gw->urlbar)
			SendMessage(gw->urlbar, WM_PASTE, 0, 0);
		else
			browser_window_key_press(gw->bw, KEY_PASTE);
		break;
	}

	case IDM_EDIT_DELETE:
		if (GetFocus() == gw->urlbar)
			SendMessage(gw->urlbar, WM_CUT, 0, 0);
		else
			browser_window_key_press(gw->bw, KEY_DELETE_RIGHT);
		break;

	case IDM_EDIT_SELECT_ALL:
		if (GetFocus() == gw->urlbar)
			SendMessage(gw->urlbar, EM_SETSEL, 0, -1);
		else
			browser_window_key_press(gw->bw, KEY_SELECT_ALL);
		break;

	case IDM_EDIT_SEARCH:
		break;

	case IDM_EDIT_PREFERENCES:
		nsws_prefs_dialog_init(hInstance, gw->main);
		break;

	case IDM_NAV_BACK:
		if ((gw->bw != NULL) &&
		    (browser_window_history_back_available(gw->bw))) {
			browser_window_history_back(gw->bw, false);
		}
		nsws_window_update_forward_back(gw);
		break;

	case IDM_NAV_FORWARD:
		if ((gw->bw != NULL) &&
		    (browser_window_history_forward_available(gw->bw))) {
			browser_window_history_forward(gw->bw, false);
		}
		nsws_window_update_forward_back(gw);
		break;

	case IDM_NAV_HOME:
	{
		nsurl *url;

		if (nsurl_create(nsoption_charp(homepage_url), &url) != NSERROR_OK) {
			warn_user("NoMemory", 0);
		} else {
			browser_window_navigate(gw->bw,
						url,
						NULL,
						BW_NAVIGATE_HISTORY,
						NULL,
						NULL,
						NULL);
			nsurl_unref(url);
		}
		break;
	}

	case IDM_NAV_STOP:
		browser_window_stop(gw->bw);
		break;

	case IDM_NAV_RELOAD:
		browser_window_reload(gw->bw, true);
		break;

	case IDM_NAV_LOCALHISTORY:
		gw->localhistory = nsws_window_create_localhistory(gw);
		break;

	case IDM_NAV_GLOBALHISTORY:
		break;

	case IDM_VIEW_ZOOMPLUS: {
		int x, y;
		gui_window_get_scroll(gw, &x, &y);
		if (gw->bw != NULL) {
			browser_window_set_scale(gw->bw, gw->bw->scale * 1.1, true);
			browser_window_reformat(gw->bw, false, gw->width, gw->height);
		}
		gui_window_redraw_window(gw);
		gui_window_set_scroll(gw, x, y);
		break;
	}

	case IDM_VIEW_ZOOMMINUS: {
		int x, y;
		gui_window_get_scroll(gw, &x, &y);
		if (gw->bw != NULL) {
			browser_window_set_scale(gw->bw,
						 gw->bw->scale * 0.9, true);
			browser_window_reformat(gw->bw, false, gw->width, gw->height);
		}
		gui_window_redraw_window(gw);
		gui_window_set_scroll(gw, x, y);
		break;
	}

	case IDM_VIEW_ZOOMNORMAL: {
		int x, y;
		gui_window_get_scroll(gw, &x, &y);
		if (gw->bw != NULL) {
			browser_window_set_scale(gw->bw, 1.0, true);
			browser_window_reformat(gw->bw, false, gw->width, gw->height);
		}
		gui_window_redraw_window(gw);
		gui_window_set_scroll(gw, x, y);
		break;
	}

	case IDM_VIEW_SOURCE:
		break;

	case IDM_VIEW_SAVE_WIN_METRICS: {
		RECT r;
		GetWindowRect(gw->main, &r);
		nsoption_set_int(window_x, r.left);
		nsoption_set_int(window_y, r.top);
		nsoption_set_int(window_width, r.right - r.left);
		nsoption_set_int(window_height, r.bottom - r.top);
		nsoption_write(options_file_location, NULL, NULL);
		break;
	}

	case IDM_VIEW_FULLSCREEN: {
		RECT rdesk;
		if (gw->fullscreen == NULL) {
			HWND desktop = GetDesktopWindow();
			gw->fullscreen = malloc(sizeof(RECT));
			if ((desktop == NULL) ||
			    (gw->fullscreen == NULL)) {
				warn_user("NoMemory", 0);
				break;
			}
			GetWindowRect(desktop, &rdesk);
			GetWindowRect(gw->main, gw->fullscreen);
			DeleteObject(desktop);
			SetWindowLong(gw->main, GWL_STYLE, 0);
			SetWindowPos(gw->main, HWND_TOPMOST, 0, 0,
				     rdesk.right - rdesk.left,
				     rdesk.bottom - rdesk.top,
				     SWP_SHOWWINDOW);
		} else {
			SetWindowLong(gw->main, GWL_STYLE,
				      WS_OVERLAPPEDWINDOW |
				      WS_HSCROLL | WS_VSCROLL |
				      WS_CLIPCHILDREN |
				      WS_CLIPSIBLINGS | CS_DBLCLKS);
			SetWindowPos(gw->main, HWND_TOPMOST,
				     gw->fullscreen->left,
				     gw->fullscreen->top,
				     gw->fullscreen->right -
				     gw->fullscreen->left,
				     gw->fullscreen->bottom -
				     gw->fullscreen->top,
				     SWP_SHOWWINDOW | SWP_FRAMECHANGED);
			free(gw->fullscreen);
			gw->fullscreen = NULL;
		}
		break;
	}

	case IDM_VIEW_DOWNLOADS:
		break;

	case IDM_VIEW_TOGGLE_DEBUG_RENDERING:
		html_redraw_debug = !html_redraw_debug;
		if (gw->bw != NULL) {
			/* TODO: This should only redraw, not reformat.
			 * (Layout doesn't change, so reformat is a waste of time) */
			browser_window_reformat(gw->bw, false, gw->width, gw->height);
		}
		break;

	case IDM_VIEW_DEBUGGING_SAVE_BOXTREE:
		break;

	case IDM_VIEW_DEBUGGING_SAVE_DOMTREE:
		break;

	case IDM_HELP_CONTENTS:
		nsws_window_go(hwnd,
			       "http://www.netsurf-browser.org/documentation/");
		break;

	case IDM_HELP_GUIDE:
		nsws_window_go(hwnd,
			       "http://www.netsurf-browser.org/documentation/guide");
		break;

	case IDM_HELP_INFO:
		nsws_window_go(hwnd,
			       "http://www.netsurf-browser.org/documentation/info");
		break;

	case IDM_HELP_ABOUT:
		nsws_about_dialog_init(hInstance, gw->main);
		break;

	case IDC_MAIN_LAUNCH_URL:
	{
		nsurl *url;

		if (GetFocus() != gw->urlbar)
			break;

		int len = SendMessage(gw->urlbar, WM_GETTEXTLENGTH, 0, 0);
		char addr[len + 1];
		SendMessage(gw->urlbar, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)addr);
		LOG(("launching %s\n", addr));

		if (nsurl_create(addr, &url) != NSERROR_OK) {
			warn_user("NoMemory", 0);
		} else {
			browser_window_navigate(gw->bw,
						url,
						NULL,
						BW_NAVIGATE_HISTORY,
						NULL,
						NULL,
						NULL);
			nsurl_unref(url);
		}

		break;
	}


	default:
		return 1; /* unhandled */

	}
	return 0; /* control message handled */
}


/**
 * callback for window events generally
 */
static LRESULT CALLBACK
nsws_window_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
	RECT rmain;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	/* deal with window creation as a special case */
	if (msg == WM_CREATE) {
		/* To cause all the component child windows to be
		 * re-sized correctly a WM_SIZE message of the actual
		 * created size must be sent. 
		 *
		 * The message must be posted here because the actual
		 * size values of the component windows are not known
		 * until after the WM_CREATE message is dispatched.
		 */
		GetClientRect(hwnd, &rmain);
		PostMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rmain.right, rmain.bottom));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}


	gw = nsws_get_gui_window(hwnd);
	if (gw == NULL) {
		LOG(("Unable to find gui window structure for hwnd %p", hwnd));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	switch(msg) {


	case WM_CONTEXTMENU:
		if (nsws_ctx_menu(gw, hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
			return 0;
		break;

	case WM_COMMAND:
		if (nsws_window_command(hwnd, gw, HIWORD(wparam), LOWORD(wparam), (HWND)lparam) == 0)
			return 0;
		break;

	case WM_SIZE:
		return nsws_window_resize(gw, hwnd, wparam, lparam);

	case WM_NCDESTROY:
		RemoveProp(hwnd, TEXT("GuiWnd"));
		browser_window_destroy(gw->bw);
		if (--open_windows <= 0) {
			netsurf_quit = true;
		}
		break;

	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}




/**
 * creation of status bar
 */
static HWND nsws_window_create_statusbar(struct gui_window *w)
{
	HWND hwnd = CreateWindowEx(0,
				   STATUSCLASSNAME,
				   NULL,
				   WS_CHILD | WS_VISIBLE,
				   0, 0, 0, 0,
				   w->main,
				   (HMENU)IDC_MAIN_STATUSBAR,
				   hInstance,
				   NULL);
	SendMessage(hwnd, SB_SETTEXT, 0, (LPARAM)"NetSurf");
	return hwnd;
}

static css_fixed get_window_dpi(HWND hwnd)
{
	HDC hdc = GetDC(hwnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	css_fixed fix_dpi = INTTOFIX(96);

	if (dpi > 10) {
		fix_dpi = INTTOFIX(dpi);
	}

	ReleaseDC(hwnd, hdc);

	LOG(("FIX DPI %x", fix_dpi));

	return fix_dpi;
}

/**
 * creation of a new full browser window
 */
static HWND nsws_window_create(struct gui_window *gw)
{
	HWND hwnd;
	INITCOMMONCONTROLSEX icc;

	LOG(("GUI window %p", gw));

	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
#if WINVER > 0x0501
	icc.dwICC |= ICC_STANDARD_CLASSES;
#endif
	InitCommonControlsEx(&icc);

	gw->mainmenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU_MAIN));
	gw->rclick = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU_CONTEXT));

	LOG(("creating window for hInstance %p", hInstance));
	hwnd = CreateWindowEx(0,
			      windowclassname_main,
			      "NetSurf Browser",
			      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CS_DBLCLKS,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,
			      gw->width,
			      gw->height,
			      NULL,
			      gw->mainmenu,
			      hInstance,
			      NULL);

	if (hwnd == NULL) {
		LOG(("Window create failed"));
		return NULL;
	}

	/* set the gui window associated with this browser */
	SetProp(hwnd, TEXT("GuiWnd"), (HANDLE)gw);

	nscss_screen_dpi = get_window_dpi(hwnd);

	if ((nsoption_int(window_width) >= 100) &&
	    (nsoption_int(window_height) >= 100) &&
	    (nsoption_int(window_x) >= 0) &&
	    (nsoption_int(window_y) >= 0)) {
		LOG(("Setting Window position %d,%d %d,%d",
		     nsoption_int(window_x), nsoption_int(window_y),
		     nsoption_int(window_width), nsoption_int(window_height)));
		SetWindowPos(hwnd, HWND_TOP,
			     nsoption_int(window_x), nsoption_int(window_y),
			     nsoption_int(window_width), nsoption_int(window_height),
			     SWP_SHOWWINDOW);
	}

	nsws_window_set_accels(gw);

	return hwnd;
}

/**
 * create a new gui_window to contain a browser_window
 * \param bw the browser_window to connect to the new gui_window
 */
static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *gw;

	LOG(("Creating gui window for browser window %p", bw));

	gw = calloc(1, sizeof(struct gui_window));

	if (gw == NULL) {
		return NULL;
	}

	/* connect gui window to browser window */
	gw->bw = bw;

	gw->width = 800;
	gw->height = 600;
	gw->toolbuttonsize = 24;
	gw->requestscrollx = 0;
	gw->requestscrolly = 0;
	gw->localhistory = NULL;

	gw->mouse = malloc(sizeof(struct browser_mouse));
	if (gw->mouse == NULL) {
		free(gw);
		LOG(("Unable to allocate mouse state"));
		return NULL;
	}
	gw->mouse->gui = gw;
	gw->mouse->state = 0;
	gw->mouse->pressed_x = 0;
	gw->mouse->pressed_y = 0;

	/* add window to list */
	if (window_list != NULL)
		window_list->prev = gw;
	gw->next = window_list;
	window_list = gw;

	gw->main = nsws_window_create(gw);
	gw->toolbar = nsws_window_create_toolbar(gw, gw->main);
	gw->statusbar = nsws_window_create_statusbar(gw);
	gw->drawingarea = nsws_window_create_drawable(hInstance, gw->main, gw);

	LOG(("new window: main:%p toolbar:%p statusbar %p drawingarea %p",
			gw->main, gw->toolbar, gw->statusbar, gw->drawingarea));

	font_hwnd = gw->drawingarea;
	input_window = gw;
	open_windows++;
	ShowWindow(gw->main, SW_SHOWNORMAL);

	return gw;
}





/**
 * cache pointers for quick swapping
 */
void nsws_window_init_pointers(HINSTANCE hinstance)
{
	nsws_pointer.hand = LoadCursor(NULL, IDC_HAND);
	nsws_pointer.ibeam = LoadCursor(NULL, IDC_IBEAM);
	nsws_pointer.cross = LoadCursor(NULL, IDC_CROSS);
	nsws_pointer.sizeall = LoadCursor(NULL, IDC_SIZEALL);
	nsws_pointer.sizewe = LoadCursor(NULL, IDC_SIZEWE);
	nsws_pointer.sizens = LoadCursor(NULL, IDC_SIZENS);
	nsws_pointer.sizenesw = LoadCursor(NULL, IDC_SIZENESW);
	nsws_pointer.sizenwse = LoadCursor(NULL, IDC_SIZENWSE);
	nsws_pointer.wait = LoadCursor(NULL, IDC_WAIT);
	nsws_pointer.appstarting = LoadCursor(NULL, IDC_APPSTARTING);
	nsws_pointer.no = LoadCursor(NULL, IDC_NO);
	nsws_pointer.help = LoadCursor(NULL, IDC_HELP);
	nsws_pointer.arrow = LoadCursor(NULL, IDC_ARROW);
}



HWND gui_window_main_window(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->main;
}

HWND gui_window_toolbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->toolbar;
}

HWND gui_window_urlbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->urlbar;
}

HWND gui_window_statusbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->statusbar;
}

HWND gui_window_drawingarea(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->drawingarea;
}

struct nsws_localhistory *gui_window_localhistory(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->localhistory;
}


RECT *gui_window_redraw_rect(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return &(w->redraw);
}

int gui_window_width(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->width;
}

int gui_window_height(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->height;
}

int gui_window_scrollingx(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->requestscrollx;
}

int gui_window_scrollingy(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->requestscrolly;
}

struct gui_window *gui_window_iterate(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->next;
}

struct browser_window *gui_window_browser_window(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->bw;
}

/**
 * window cleanup code
 */
static void gui_window_destroy(struct gui_window *w)
{
	if (w == NULL)
		return;

	if (w->prev != NULL)
		w->prev->next = w->next;
	else
		window_list = w->next;

	if (w->next != NULL)
		w->next->prev = w->prev;

	DestroyAcceleratorTable(w->acceltable);

	free(w);
	w = NULL;
}

/**
 * set window title
 * \param title the [url]
 */
static void gui_window_set_title(struct gui_window *w, const char *title)
{
	if (w == NULL)
		return;
	LOG(("%p, title %s", w, title));
	char *fulltitle = malloc(strlen(title) +
				 SLEN("  -  NetSurf") + 1);
	if (fulltitle == NULL) {
		warn_user("NoMemory", 0);
		return;
	}
	strcpy(fulltitle, title);
	strcat(fulltitle, "  -  NetSurf");
	SendMessage(w->main, WM_SETTEXT, 0, (LPARAM)fulltitle);
	free(fulltitle);
}

static void gui_window_update_box(struct gui_window *gw, const struct rect *rect)
{
	/* LOG(("gw:%p %f,%f %f,%f", gw, data->redraw.x, data->redraw.y, data->redraw.width, data->redraw.height)); */

	if (gw == NULL)
		return;

	RECT redrawrect;

	redrawrect.left = (long)rect->x0 - (gw->scrollx / gw->bw->scale);
	redrawrect.top = (long)rect->y0 - (gw->scrolly / gw->bw->scale);
	redrawrect.right =(long)rect->x1;
	redrawrect.bottom = (long)rect->y1;

	RedrawWindow(gw->drawingarea, &redrawrect, NULL, RDW_INVALIDATE | RDW_NOERASE);

}

static bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
	LOG(("get scroll"));
	if (w == NULL)
		return false;
	*sx = w->scrollx;
	*sy = w->scrolly;

	return true;
}

/**
 * scroll the window
 * \param sx the new 'absolute' scroll location
 * \param sy the new 'absolute' scroll location
 */
void gui_window_set_scroll(struct gui_window *w, int sx, int sy)
{
	SCROLLINFO si;
	POINT p;

	if ((w == NULL) ||
	    (w->bw == NULL) ||
	    (w->bw->current_content == NULL))
		return;

	/* limit scale range */
	if (abs(w->bw->scale - 0.0) < 0.00001)
		w->bw->scale = 1.0;

	w->requestscrollx = sx - w->scrollx;
	w->requestscrolly = sy - w->scrolly;

	/* set the vertical scroll offset */
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = (content_get_height(w->bw->current_content) * w->bw->scale) - 1;
	si.nPage = w->height;
	si.nPos = max(w->scrolly + w->requestscrolly, 0);
	si.nPos = min(si.nPos, content_get_height(w->bw->current_content) * w->bw->scale - w->height);
	SetScrollInfo(w->drawingarea, SB_VERT, &si, TRUE);
	LOG(("SetScrollInfo VERT min:%d max:%d page:%d pos:%d", si.nMin, si.nMax, si.nPage, si.nPos));

	/* set the horizontal scroll offset */
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = (content_get_width(w->bw->current_content) * w->bw->scale) -1;
	si.nPage = w->width;
	si.nPos = max(w->scrollx + w->requestscrollx, 0);
	si.nPos = min(si.nPos, content_get_width(w->bw->current_content) * w->bw->scale - w->width);
	SetScrollInfo(w->drawingarea, SB_HORZ, &si, TRUE);
	LOG(("SetScrollInfo HORZ min:%d max:%d page:%d pos:%d", si.nMin, si.nMax, si.nPage, si.nPos));

	/* Set caret position */
	GetCaretPos(&p);
	HideCaret(w->drawingarea);
	SetCaretPos(p.x - w->requestscrollx, p.y - w->requestscrolly);
	ShowCaret(w->drawingarea);

	RECT r, redraw;
	r.top = 0;
	r.bottom = w->height + 1;
	r.left = 0;
	r.right = w->width + 1;
	ScrollWindowEx(w->drawingarea, - w->requestscrollx, - w->requestscrolly, &r, NULL, NULL, &redraw, SW_INVALIDATE);
	w->scrolly += w->requestscrolly;
	w->scrollx += w->requestscrollx;
	w->requestscrollx = 0;
	w->requestscrolly = 0;

}

static void gui_window_get_dimensions(struct gui_window *w, int *width, int *height,
			       bool scaled)
{
	if (w == NULL)
		return;

	LOG(("get dimensions %p w=%d h=%d", w, w->width, w->height));

	*width = w->width;
	*height = w->height;
}

static void gui_window_update_extent(struct gui_window *w)
{

}

/**
 * set the status bar message
 */
static void gui_window_set_status(struct gui_window *w, const char *text)
{
	if (w == NULL)
		return;
	SendMessage(w->statusbar, WM_SETTEXT, 0, (LPARAM)text);
}

/**
 * set the pointer shape
 */
static void gui_window_set_pointer(struct gui_window *w, gui_pointer_shape shape)
{
	if (w == NULL)
		return;

	switch (shape) {
	case GUI_POINTER_POINT: /* link */
	case GUI_POINTER_MENU:
		SetCursor(nsws_pointer.hand);
		break;

	case GUI_POINTER_CARET: /* input */
		SetCursor(nsws_pointer.ibeam);
		break;

	case GUI_POINTER_CROSS:
		SetCursor(nsws_pointer.cross);
		break;

	case GUI_POINTER_MOVE:
		SetCursor(nsws_pointer.sizeall);
		break;

	case GUI_POINTER_RIGHT:
	case GUI_POINTER_LEFT:
		SetCursor(nsws_pointer.sizewe);
		break;

	case GUI_POINTER_UP:
	case GUI_POINTER_DOWN:
		SetCursor(nsws_pointer.sizens);
		break;

	case GUI_POINTER_RU:
	case GUI_POINTER_LD:
		SetCursor(nsws_pointer.sizenesw);
		break;

	case GUI_POINTER_RD:
	case GUI_POINTER_LU:
		SetCursor(nsws_pointer.sizenwse);
		break;

	case GUI_POINTER_WAIT:
		SetCursor(nsws_pointer.wait);
		break;

	case GUI_POINTER_PROGRESS:
		SetCursor(nsws_pointer.appstarting);
		break;

	case GUI_POINTER_NO_DROP:
	case GUI_POINTER_NOT_ALLOWED:
		SetCursor(nsws_pointer.no);
		break;

	case GUI_POINTER_HELP:
		SetCursor(nsws_pointer.help);
		break;

	default:
		SetCursor(nsws_pointer.arrow);
		break;
	}
}

struct nsws_pointers *nsws_get_pointers(void)
{
	return &nsws_pointer;
}

static void gui_window_set_url(struct gui_window *w, const char *url)
{
	if (w == NULL)
		return;
	SendMessage(w->urlbar, WM_SETTEXT, 0, (LPARAM) url);
}


static void gui_window_start_throbber(struct gui_window *w)
{
	if (w == NULL)
		return;
	nsws_window_update_forward_back(w);

	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, IDM_NAV_STOP, MF_ENABLED);
		EnableMenuItem(w->mainmenu, IDM_NAV_RELOAD, MF_GRAYED);
	}
	if (w->rclick != NULL) {
		EnableMenuItem(w->rclick, IDM_NAV_STOP, MF_ENABLED);
		EnableMenuItem(w->rclick, IDM_NAV_RELOAD, MF_GRAYED);
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE, (WPARAM) IDM_NAV_STOP,
			    MAKELONG(TBSTATE_ENABLED, 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_RELOAD,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
	}
	w->throbbing = true;
	Animate_Play(w->throbber, 0, -1, -1);
}

static void gui_window_stop_throbber(struct gui_window *w)
{
	if (w == NULL)
		return;
	nsws_window_update_forward_back(w);
	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, IDM_NAV_STOP, MF_GRAYED);
		EnableMenuItem(w->mainmenu, IDM_NAV_RELOAD, MF_ENABLED);
	}
	if (w->rclick != NULL) {
		EnableMenuItem(w->rclick, IDM_NAV_STOP, MF_GRAYED);
		EnableMenuItem(w->rclick, IDM_NAV_RELOAD, MF_ENABLED);
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE, (WPARAM) IDM_NAV_STOP,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_RELOAD,
			    MAKELONG(TBSTATE_ENABLED, 0));
	}
	w->throbbing = false;
	Animate_Stop(w->throbber);
	Animate_Seek(w->throbber, 0);
}

/**
 * place caret in window
 */
static void gui_window_place_caret(struct gui_window *w, int x, int y,
				   int height, const struct rect *clip)
{
	if (w == NULL)
		return;
	CreateCaret(w->drawingarea, (HBITMAP)NULL, 1, height * w->bw->scale);
	SetCaretPos(x * w->bw->scale - w->scrollx,
		    y * w->bw->scale - w->scrolly);
	ShowCaret(w->drawingarea);
}

/**
 * clear window caret
 */
static void gui_window_remove_caret(struct gui_window *w)
{
	if (w == NULL)
		return;
	HideCaret(w->drawingarea);
}

/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
	/* TODO: Implement this */
	HANDLE clipboard_handle;
	char *content;

	clipboard_handle = GetClipboardData(CF_TEXT);
	if (clipboard_handle != NULL) {
		content = GlobalLock(clipboard_handle);
		LOG(("pasting %s", content));
		GlobalUnlock(clipboard_handle);
	}
}

/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	/* TODO: Implement this */
	HANDLE hnew;
	char *new, *original;
	HANDLE h = GetClipboardData(CF_TEXT);
	if (h == NULL)
		original = (char *)"";
	else
		original = GlobalLock(h);

	size_t len = strlen(original) + 1;
	hnew = GlobalAlloc(GHND, length + len);
	new = (char *)GlobalLock(hnew);
	snprintf(new, length + len, "%s%s", original, buffer);

	if (h != NULL) {
		GlobalUnlock(h);
		EmptyClipboard();
	}
	GlobalUnlock(hnew);
	SetClipboardData(CF_TEXT, hnew);
}


/**
 * Create the main window class.
 */
nserror 
nsws_create_main_class(HINSTANCE hinstance) {
	nserror ret = NSERROR_OK;
	WNDCLASSEX w;

	/* main window */
	w.cbSize = sizeof(WNDCLASSEX);
	w.style	= 0;
	w.lpfnWndProc = nsws_window_event_callback;
	w.cbClsExtra = 0;
	w.cbWndExtra = 0;
	w.hInstance = hinstance;
	w.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDR_NETSURF_ICON));
	w.hCursor = NULL;
	w.hbrBackground	= (HBRUSH)(COLOR_MENU + 1);
	w.lpszMenuName = NULL;
	w.lpszClassName = windowclassname_main;
	w.hIconSm = LoadIcon(hinstance, MAKEINTRESOURCE(IDR_NETSURF_ICON));

	if (RegisterClassEx(&w) == 0) {
		win_perror("DrawableClass");
		ret = NSERROR_INIT_FAILED;
	}

	hInstance = hinstance;

	return ret;
}

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */
static char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '\\');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */
static bool path_add_part(char *path, int length, const char *newpart)
{
	if(path[strlen(path) - 1] != '\\')
		strncat(path, "\\", length);

	strncat(path, newpart, length);

	return true;
}

static struct gui_window_table window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.redraw = gui_window_redraw_window,
	.update = gui_window_update_box,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.update_extent = gui_window_update_extent,

	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
	.remove_caret = gui_window_remove_caret,
	.start_throbber = gui_window_start_throbber,
	.stop_throbber = gui_window_stop_throbber,
};

struct gui_window_table *win32_window_table = &window_table;


static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *win32_clipboard_table = &clipboard_table;


static struct gui_fetch_table fetch_table = {
	.filename_from_path = filename_from_path,
	.path_add_part = path_add_part,
	.filetype = fetch_filetype,
	.path_to_url = path_to_url,
	.url_to_path = url_to_path,

	.mimetype = fetch_mimetype,
};
struct gui_fetch_table *win32_fetch_table = &fetch_table;


static struct gui_browser_table browser_table = {
	.poll = win32_poll,
	.schedule = win32_schedule,
};

struct gui_browser_table *win32_browser_table = &browser_table;
