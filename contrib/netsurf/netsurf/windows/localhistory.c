/*
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

#include "utils/config.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "desktop/browser_history.h"
#include "desktop/plotters.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "windows/window.h"
#include "windows/localhistory.h"
#include "windows/gui.h"
#include "windows/plot.h"
#include "windows/resourceid.h"
#include "windows/windbg.h"

static const char windowclassname_localhistory[] = "nswslocalhistorywindow";

struct nsws_localhistory {
	HWND hwnd; /**< the window handle */
	int width; /**< the width of the memory history */
	int height; /**< the height of the memory history */
	int guiwidth; /**< the width of the history window */
	int guiheight; /**< the height of the history window */
	int vscroll; /**< the vertical scroll location */
	int hscroll; /**< the horizontal scroll location */
};


static void nsws_localhistory_scroll_check(struct nsws_localhistory *l, struct gui_window *gw)
{
	SCROLLINFO si;

	if ((gw->bw == NULL) || (l->hwnd == NULL))
		return;

	browser_window_history_size(gw->bw, &(l->width), &(l->height));

	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = l->height;
	si.nPage = l->guiheight;
	si.nPos = 0;
	SetScrollInfo(l->hwnd, SB_VERT, &si, TRUE);

	si.nMax = l->width;
	si.nPage = l->guiwidth;
	SetScrollInfo(l->hwnd, SB_HORZ, &si, TRUE);
	if (l->guiheight >= l->height)
		l->vscroll = 0;
	if (l->guiwidth >= l->width)
		l->hscroll = 0;
	SendMessage(l->hwnd, WM_PAINT, 0, 0);
}



static void nsws_localhistory_up(struct nsws_localhistory *l, struct gui_window *gw)
{
	HDC tmp_hdc;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &win_plotters
	};

	LOG(("gui window %p", gw));

	l->vscroll = 0;
	l->hscroll = 0;

	if (gw->bw != NULL) {
		/* set global HDC for the plotters */
		tmp_hdc = plot_hdc;
		plot_hdc = GetDC(l->hwnd);

		browser_window_history_redraw(gw->bw, &ctx);

		ReleaseDC(l->hwnd, plot_hdc);

		plot_hdc = tmp_hdc;
	}

	nsws_localhistory_scroll_check(l, gw);
}


/*
  void history_gui_set_pointer(gui_pointer_shape shape, void *p)
  {
  struct nsws_pointers *pointers = nsws_get_pointers();
  if (pointers == NULL)
  return;
  switch(shape) {
  case GUI_POINTER_POINT:
  SetCursor(pointers->hand);
  break;
  default:
  SetCursor(pointers->arrow);
  break;
  }
  }
*/


void nsws_localhistory_close(struct gui_window *w)
{
	struct nsws_localhistory *l = gui_window_localhistory(w);
	if (l != NULL)
		CloseWindow(l->hwnd);
}

static LRESULT CALLBACK 
nsws_localhistory_event_callback(HWND hwnd, UINT msg,
				 WPARAM wparam, LPARAM lparam)
{
	int x,y;
	struct gui_window *gw;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	gw = nsws_get_gui_window(hwnd);
	if (gw == NULL) {
		LOG(("Unable to find gui window structure for hwnd %p", hwnd));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	switch(msg) {

	case WM_CREATE:
		nsws_localhistory_scroll_check(gw->localhistory, gw);
		break;

	case WM_SIZE:
		gw->localhistory->guiheight = HIWORD(lparam);
		gw->localhistory->guiwidth = LOWORD(lparam);
		nsws_localhistory_scroll_check(gw->localhistory, gw);
		break;

	case WM_LBUTTONUP: 
		if (gw->bw == NULL)
			break;

		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);

		if (browser_window_history_click(gw->bw,
				   gw->localhistory->hscroll + x,
				   gw->localhistory->vscroll + y,
				   false)) {
			DestroyWindow(hwnd);
		}
	
		break;

	case WM_MOUSEMOVE: 
		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);
		return DefWindowProc(hwnd, msg, wparam, lparam);
		break;
	

	case WM_VSCROLL:
	{
		SCROLLINFO si;
		int mem;
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hwnd, SB_VERT, &si);
		mem = si.nPos;
		switch (LOWORD(wparam))	{
		case SB_TOP:
			si.nPos = si.nMin;
			break;
		case SB_BOTTOM:
			si.nPos = si.nMax;
			break;
		case SB_LINEUP:
			si.nPos -= 30;
			break;
		case SB_LINEDOWN:
			si.nPos += 30;
			break;
		case SB_PAGEUP:
			si.nPos -= gw->localhistory->guiheight;
			break;
		case SB_PAGEDOWN:
			si.nPos += gw->localhistory->guiheight;
			break;
		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;
		default:
			break;
		}
		si.nPos = min(si.nPos, gw->localhistory->height);
		si.nPos = min(si.nPos, 0);
		si.fMask = SIF_POS;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		GetScrollInfo(hwnd, SB_VERT, &si);
		if (si.nPos != mem) {
			gw->localhistory->vscroll += si.nPos - mem;
			ScrollWindowEx(hwnd, 0, -(si.nPos - mem), NULL, NULL, NULL, NULL, SW_ERASE | SW_INVALIDATE);
		}
		break;
	}

	case WM_HSCROLL:
	{
		SCROLLINFO si;
		int mem;

		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hwnd, SB_HORZ, &si);
		mem = si.nPos;

		switch (LOWORD(wparam))	{
		case SB_LINELEFT:
			si.nPos -= 30;
			break;
		case SB_LINERIGHT:
			si.nPos += 30;
			break;
		case SB_PAGELEFT:
			si.nPos -= gw->localhistory->guiwidth;
			break;
		case SB_PAGERIGHT:
			si.nPos += gw->localhistory->guiwidth;
			break;
		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;
		default:
			break;
		}
		si.nPos = min(si.nPos, gw->localhistory->width);
		si.nPos = max(si.nPos, 0);
		si.fMask = SIF_POS;
		SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
		GetScrollInfo(hwnd, SB_HORZ, &si);
		if (si.nPos != mem) {
			gw->localhistory->hscroll += si.nPos - mem;
			ScrollWindowEx(hwnd, -(si.nPos - mem), 0, NULL, NULL, NULL, NULL, SW_ERASE | SW_INVALIDATE);
		}
		break;
	}

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc, tmp_hdc;
		struct redraw_context ctx = {
			.interactive = true,
			.background_images = true,
			.plot = &win_plotters
		};

		hdc = BeginPaint(hwnd, &ps);
		if (gw->bw != NULL) {
			/* set global HDC for the plotters */
			tmp_hdc = plot_hdc;
			plot_hdc = hdc;

			browser_window_history_redraw_rectangle(gw->bw,
				 gw->localhistory->hscroll + ps.rcPaint.left,
				 gw->localhistory->vscroll + ps.rcPaint.top,
				 gw->localhistory->hscroll + (ps.rcPaint.right - ps.rcPaint.left),
				 gw->localhistory->vscroll + (ps.rcPaint.bottom - ps.rcPaint.top),
				 ps.rcPaint.left,
				 ps.rcPaint.top, &ctx);

			plot_hdc = tmp_hdc;

		}
		EndPaint(hwnd, &ps);

		break;
	}

	case WM_CLOSE:
		DestroyWindow(hwnd);		
		return 1;

	case WM_DESTROY:
		free(gw->localhistory);
		gw->localhistory = NULL;
		break;

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);

	}
	return 0;
}

/* exported method documented in windows/localhistory.h */
struct nsws_localhistory *nsws_window_create_localhistory(struct gui_window *gw)
{
	struct nsws_localhistory *localhistory;
	INITCOMMONCONTROLSEX icc;
	int margin = 50;
	RECT r;

	LOG(("gui window %p", gw));

	/* if we already have a window, just update and re-show it */
	if (gw->localhistory != NULL) {
		nsws_localhistory_up(gw->localhistory, gw);
		UpdateWindow(gw->localhistory->hwnd);
		ShowWindow(gw->localhistory->hwnd, SW_SHOWNORMAL);
		return gw->localhistory;
	}	

	localhistory = calloc(1, sizeof(struct nsws_localhistory));

	if (localhistory == NULL) {
		return NULL;
	}
	gw->localhistory = localhistory;

	localhistory->width = 0;
	localhistory->height = 0;

	if (gw->bw != NULL) {
		browser_window_history_size(gw->bw, 
			     &(localhistory->width), 
			     &(localhistory->height));
	}

	GetWindowRect(gw->main, &r);
	SetWindowPos(gw->main, HWND_NOTOPMOST, 0, 0, 0, 0, 
		     SWP_NOSIZE | SWP_NOMOVE);

	localhistory->guiwidth = min(r.right - r.left - margin,
				    localhistory->width + margin);
	localhistory->guiheight = min(r.bottom - r.top - margin,
				     localhistory->height + margin);

	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
#if WINVER > 0x0501
	icc.dwICC |= ICC_STANDARD_CLASSES;
#endif
	InitCommonControlsEx(&icc);


	LOG(("creating local history window for hInstance %p", hInstance));
	localhistory->hwnd = CreateWindow(windowclassname_localhistory,
					 "NetSurf History",
					 WS_THICKFRAME | WS_HSCROLL |
					 WS_VSCROLL | WS_CLIPCHILDREN |
					 WS_CLIPSIBLINGS | WS_SYSMENU | CS_DBLCLKS,
					 r.left + margin/2,
					 r.top + margin/2,
					 localhistory->guiwidth,
					 localhistory->guiheight,
					 NULL, NULL, hInstance, NULL);

	/* set the gui window associated with this browser */
	SetProp(localhistory->hwnd, TEXT("GuiWnd"), (HANDLE)gw);

	LOG(("gui_window %p width %d height %d hwnd %p", gw,
	     localhistory->guiwidth, localhistory->guiheight,
	     localhistory->hwnd));

	nsws_localhistory_up(localhistory, gw);
	UpdateWindow(localhistory->hwnd);
	ShowWindow(localhistory->hwnd, SW_SHOWNORMAL);

	return localhistory;
}

/* exported method documented in windows/localhistory.h */
nserror
nsws_create_localhistory_class(HINSTANCE hinstance) {
	nserror ret = NSERROR_OK;
	WNDCLASSEX w;

	/* localhistory window */
	w.cbSize = sizeof(WNDCLASSEX);
	w.style	= 0;
	w.lpfnWndProc = nsws_localhistory_event_callback;
	w.cbClsExtra = 0;
	w.cbWndExtra = 0;
	w.hInstance = hinstance;
	w.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDR_NETSURF_ICON));
	w.hCursor = LoadCursor(NULL, IDC_ARROW);
	w.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	w.lpszMenuName = NULL;
	w.lpszClassName = windowclassname_localhistory;
	w.hIconSm = LoadIcon(hinstance, MAKEINTRESOURCE(IDR_NETSURF_ICON));

	if (RegisterClassEx(&w) == 0) {
		win_perror("DrawableClass");
		ret = NSERROR_INIT_FAILED;
	}

	return ret;
}
