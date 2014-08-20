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

#include <stdio.h>

#include "utils/config.h"

#include <windows.h>

#include "utils/utils.h"
#include "utils/messages.h"
#include "desktop/netsurf.h"
#include "utils/log.h"

#include "windows/gui.h"
#include "windows/about.h"
#include "windows/resourceid.h"

#include "windbg.h"

/**
 * Initialize the about dialog text fields
 */
static BOOL init_about_dialog(HWND hwnd)
{
	char ver_str[128];
	HWND dlg_itm;
	HFONT hFont;

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_VERSION);
	if (dlg_itm != NULL) {

		hFont=CreateFont (26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
		if (hFont != NULL) {
			LOG(("Setting font object"));
			SendMessage(dlg_itm, WM_SETFONT, (WPARAM)hFont, 0);
		}

		snprintf(ver_str, sizeof(ver_str), "%s %s", 
			 messages_get("NetSurf"), netsurf_version); 
		
		SendMessage(dlg_itm, WM_SETTEXT, 0, (LPARAM)ver_str);
	}

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_COPYRIGHT);
	if (dlg_itm != NULL) {
		snprintf(ver_str, sizeof(ver_str), "%s", 
			 messages_get("NetSurfCopyright")); 
		
		SendMessage(dlg_itm, WM_SETTEXT, 0, (LPARAM)ver_str);
	}

	return TRUE;
}

/**
 * destroy resources used to create about dialog
 */
static BOOL destroy_about_dialog(HWND hwnd)
{
	HWND dlg_itm;
	HFONT hFont;

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_VERSION);
	if (dlg_itm != NULL) {
		hFont = (HFONT)SendMessage(dlg_itm, WM_GETFONT, 0, 0);
		if (hFont != NULL) {
			LOG(("Destroyed font object"));
			DeleteObject(hFont); 	
		}
	}
		
	return TRUE;

}

static BOOL CALLBACK 
nsws_about_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	switch(msg) {
	case WM_INITDIALOG: 
		return init_about_dialog(hwnd);

	case WM_COMMAND:
		switch(LOWORD(wparam)) {
		case IDOK:
			LOG(("OK clicked"));
			EndDialog(hwnd, IDOK);
			break;

		case IDCANCEL:
			LOG(("Cancel clicked"));
			EndDialog(hwnd, IDOK);
			break;

		case IDC_BTN_CREDITS: 
			nsws_window_go(hwnd, "about:credits");
			EndDialog(hwnd, IDOK);
			break;

		case IDC_BTN_LICENCE:
			nsws_window_go(hwnd, "about:licence");
			EndDialog(hwnd, IDOK);
			break;

		}
		break;

	case WM_CREATE:
		return TRUE;

	case WM_DESTROY:
		return destroy_about_dialog(hwnd);

	}
	return FALSE;
}

void nsws_about_dialog_init(HINSTANCE hinst, HWND parent)
{
	int ret = DialogBox(hinst, MAKEINTRESOURCE(IDD_DLG_ABOUT), parent,
			nsws_about_event_callback);
	if (ret == -1) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
}
