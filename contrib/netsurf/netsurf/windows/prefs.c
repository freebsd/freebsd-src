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
#include <commctrl.h>

#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "windows/gui.h"
#include "windows/prefs.h"
#include "windows/resourceid.h"
#include "windows/windbg.h"

#define NSWS_PREFS_WINDOW_WIDTH 600
#define NSWS_PREFS_WINDOW_HEIGHT 400

static CHOOSEFONT *nsws_prefs_font_prepare(int fontfamily, HWND parent)
{
	CHOOSEFONT *cf = malloc(sizeof(CHOOSEFONT));
	if (cf == NULL) {
		warn_user(messages_get("NoMemory"),0);
		return NULL;
	}
	LOGFONT *lf = malloc(sizeof(LOGFONT));
	if (lf == NULL) {
		warn_user(messages_get("NoMemory"),0);
		free(cf);
		return NULL;
	}
	switch(fontfamily) {
	case FF_ROMAN:
		snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
			 nsoption_charp(font_serif));
		break;
	case FF_MODERN:
		snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
			 nsoption_charp(font_mono));
		break;
	case FF_SCRIPT:
		snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
			 nsoption_charp(font_cursive));
		break;
	case FF_DECORATIVE:
		snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
			 nsoption_charp(font_fantasy));
		break;
	case FF_SWISS:
	default:
		snprintf(lf->lfFaceName, LF_FACESIZE, "%s",
			 nsoption_charp(font_sans));
		break;
	}

	cf->lStructSize = sizeof(CHOOSEFONT);
	cf->hwndOwner = parent;
	cf->lpLogFont = lf;
	cf->Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE;
	cf->nSizeMin = 16;
	cf->nSizeMax = 24;

	return cf;
}

static void change_spinner(HWND sub, double change, double minval, double maxval)
{
	char *temp, number[6];
	int len;
	double value = 0;

	len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
	temp = malloc(len + 1);

	if (temp == NULL)
		return;

	SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM) temp);

	value = strtod(temp, NULL) - change;

	free(temp);
	value = max(value, minval);
	value = min(value, maxval);

	if ((change == 1.0) || (change == -1.0))
		snprintf(number, 6, "%.0f", value);
	else
		snprintf(number, 6, "%.1f", value);

	SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
}


static BOOL CALLBACK options_appearance_dialog_handler(HWND hwnd,
		UINT msg, WPARAM wparam, LPARAM lParam)
{
	int len;
	char *temp, number[6];
	HWND sub;

	LOG_WIN_MSG(hwnd, msg, wparam, lParam);

	switch (msg) {
	case WM_INITDIALOG:
		sub = GetDlgItem(hwnd, IDC_PREFS_FONTDEF);
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Sans serif");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Serif");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Monospace");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Cursive");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Fantasy");
		SendMessage(sub, CB_SETCURSEL,
			    (WPARAM) (nsoption_int(font_default) - 1), 0);

		if ((nsoption_charp(font_sans) != NULL) &&
		    (nsoption_charp(font_sans)[0] != '\0')) {
			sub = GetDlgItem(hwnd, IDC_PREFS_SANS);
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(font_sans));
		}
		if ((nsoption_charp(font_serif) != NULL) &&
		    (nsoption_charp(font_serif)[0] != '\0')) {
			sub = GetDlgItem(hwnd, IDC_PREFS_SERIF);
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(font_serif));
		}
		if ((nsoption_charp(font_mono) != NULL) &&
		    (nsoption_charp(font_mono)[0] != '\0')) {
			sub = GetDlgItem(hwnd, IDC_PREFS_MONO);
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(font_mono));
		}
		if ((nsoption_charp(font_cursive) != NULL) &&
		    (nsoption_charp(font_cursive)[0] != '\0')) {
			sub = GetDlgItem(hwnd, IDC_PREFS_CURSIVE);
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(font_cursive));
		}
		if ((nsoption_charp(font_fantasy) != NULL) &&
		    (nsoption_charp(font_fantasy)[0] != '\0')) {
			sub = GetDlgItem(hwnd, IDC_PREFS_FANTASY);
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(font_fantasy));
		}
		if (nsoption_int(font_min_size) != 0) {
			sub = GetDlgItem(hwnd, IDC_PREFS_FONT_MINSIZE);
			snprintf(number, 6, "%.1f", nsoption_int(font_min_size) / 10.0);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (nsoption_int(font_size) != 0) {
			sub = GetDlgItem(hwnd, IDC_PREFS_FONT_SIZE);
			snprintf(number, 6, "%.1f", nsoption_int(font_size) / 10.0);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (nsoption_int(max_fetchers) != 0) {
			sub = GetDlgItem(hwnd, IDC_PREFS_FETCHERS);
			snprintf(number, 6, "%d", nsoption_int(max_fetchers));
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (nsoption_int(max_fetchers_per_host) != 0) {
			sub = GetDlgItem(hwnd, IDC_PREFS_FETCH_HOST);
			snprintf(number, 6, "%d",
				 nsoption_int(max_fetchers_per_host));
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		if (nsoption_int(max_cached_fetch_handles) != 0) {
			sub = GetDlgItem(hwnd, IDC_PREFS_FETCH_HANDLES);
			snprintf(number, 6, "%d",
				 nsoption_int(max_cached_fetch_handles));
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}


		/* animation */
		sub = GetDlgItem(hwnd, IDC_PREFS_NOANIMATION);
		SendMessage(sub, BM_SETCHECK, (WPARAM)((nsoption_bool(animate_images))
				       ? BST_UNCHECKED : BST_CHECKED),	0);

		if (nsoption_int(minimum_gif_delay) != 0) {
			sub = GetDlgItem(hwnd, IDC_PREFS_ANIMATIONDELAY);
			snprintf(number, 6, "%.1f", nsoption_int(minimum_gif_delay) /
				 100.0);
			SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);
		}
		break;

	case WM_NOTIFY:
		switch (((NMHDR FAR *)lParam)->code) {
		case PSN_APPLY:
			sub = GetDlgItem(hwnd, IDC_PREFS_FONT_SIZE);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)
					    (len + 1), (LPARAM) temp);
				nsoption_int(font_size) = (int)
					(10 * strtod(temp, NULL));
				free(temp);
			}

			sub = GetDlgItem(hwnd, IDC_PREFS_FONT_MINSIZE);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)
					    (len + 1), (LPARAM) temp);
				nsoption_set_int(font_min_size, 
						 (int)(10 * strtod(temp, NULL)));
				free(temp);
			}

			/* animation */
			nsoption_set_bool(animate_images, 
					  (IsDlgButtonChecked(hwnd, IDC_PREFS_NOANIMATION) == BST_CHECKED) ? true : false);


			sub = GetDlgItem(hwnd, IDC_PREFS_ANIMATIONDELAY);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)
					    (len + 1), (LPARAM) temp);
				nsoption_set_int(minimum_gif_delay,
						 (int)(100 * strtod(temp, NULL)));
				free(temp);
			}

			break;

		case UDN_DELTAPOS: {
			NMUPDOWN *ud = (NMUPDOWN *)lParam;
			switch(((NMHDR *)lParam)->idFrom) {
			case IDC_PREFS_FONT_SIZE_SPIN:
				change_spinner(GetDlgItem(hwnd, IDC_PREFS_FONT_SIZE), 0.1  * ud->iDelta, 1.0, 50.0);
				return TRUE;

			case IDC_PREFS_FONT_MINSIZE_SPIN:
				change_spinner(GetDlgItem(hwnd, IDC_PREFS_FONT_MINSIZE), 0.1  * ud->iDelta, 1.0, 50.0);
				return TRUE;

			case IDC_PREFS_ANIMATIONDELAY_SPIN:
				change_spinner(GetDlgItem(hwnd, IDC_PREFS_ANIMATIONDELAY), 0.1  * ud->iDelta, 0.1, 100.0);
				return TRUE;
				
			}
		}
			break;
		}


	case WM_COMMAND:
		LOG(("WM_COMMAND Identifier 0x%x",LOWORD(wparam)));

		switch(LOWORD(wparam)) {
		case IDC_PREFS_PROXYTYPE:
			sub = GetDlgItem(hwnd, IDC_PREFS_PROXYTYPE);
			nsoption_set_int(http_proxy_auth,
					 SendMessage(sub, CB_GETCURSEL, 0, 0) - 1);
			nsoption_set_bool(http_proxy,
					  (nsoption_int(http_proxy_auth) != -1));
			nsoption_set_int(http_proxy_auth,  
					 nsoption_int(http_proxy_auth) + 
					 (nsoption_bool(http_proxy)) ? 0 : 1);
			break;

		case IDC_PREFS_SANS: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_SWISS, hwnd);
			if (cf == NULL) {
				break;
			}

			if (ChooseFont(cf) == TRUE) {
				nsoption_set_charp(font_sans, 
						   strdup(cf->lpLogFont->lfFaceName));
			}

			free(cf->lpLogFont);
			free(cf);
			if ((nsoption_charp(font_sans) != NULL) &&
			    (nsoption_charp(font_sans)[0] != '\0')) {
				sub = GetDlgItem(hwnd, IDC_PREFS_SANS);
				SendMessage(sub, WM_SETTEXT, 0,
					    (LPARAM)nsoption_charp(font_sans));
			}
			break;
		}

		case IDC_PREFS_SERIF: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_ROMAN, hwnd);
			if (cf == NULL) {
				break;
			}

			if (ChooseFont(cf) == TRUE) {
				nsoption_set_charp(font_serif,
						   strdup(cf->lpLogFont->lfFaceName));
			}

			free(cf->lpLogFont);
			free(cf);
			if ((nsoption_charp(font_serif) != NULL) &&
			    (nsoption_charp(font_serif)[0] != '\0')) {
				sub = GetDlgItem(hwnd, IDC_PREFS_SERIF);
				SendMessage(sub, WM_SETTEXT, 0,
					    (LPARAM)nsoption_charp(font_serif));
			}
			break;
		}

		case IDC_PREFS_MONO: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_MODERN, hwnd);
			if (cf == NULL) {
				break;
			}

			if (ChooseFont(cf) == TRUE) {
				nsoption_set_charp(font_mono,
						   strdup(cf->lpLogFont->lfFaceName));
			}

			free(cf->lpLogFont);
			free(cf);

			if ((nsoption_charp(font_mono) != NULL) &&
			    (nsoption_charp(font_mono)[0] != '\0')) {
				sub = GetDlgItem(hwnd, IDC_PREFS_MONO);
				SendMessage(sub, WM_SETTEXT, 0,
					    (LPARAM)nsoption_charp(font_mono));
			}
			break;
		}

		case IDC_PREFS_CURSIVE: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_SCRIPT, hwnd);
			if (cf == NULL) {
				break;
			}

			if (ChooseFont(cf) == TRUE) {
				nsoption_set_charp(font_cursive,
						   strdup(cf->lpLogFont->lfFaceName));
			}
			free(cf->lpLogFont);
			free(cf);
			if ((nsoption_charp(font_cursive) != NULL) &&
			    (nsoption_charp(font_cursive)[0] != '\0')) {
				sub = GetDlgItem(hwnd, IDC_PREFS_CURSIVE);
				SendMessage(sub, WM_SETTEXT, 0,
					    (LPARAM)nsoption_charp(font_cursive));
			}
			break;
		}

		case IDC_PREFS_FANTASY: {
			CHOOSEFONT *cf = nsws_prefs_font_prepare(FF_DECORATIVE, hwnd);
			if (cf == NULL) {
				break;
			}

			if (ChooseFont(cf) == TRUE) {
				nsoption_set_charp(font_fantasy,
						   strdup(cf->lpLogFont->lfFaceName));
			}
			free(cf->lpLogFont);
			free(cf);
			if ((nsoption_charp(font_fantasy) != NULL) &&
			    (nsoption_charp(font_fantasy)[0] != '\0')) {
				sub = GetDlgItem(hwnd, IDC_PREFS_FANTASY);
				SendMessage(sub, WM_SETTEXT, 0,
					    (LPARAM)nsoption_charp(font_fantasy));
			}
			break;
		}

		case IDC_PREFS_FONTDEF: 
			sub = GetDlgItem(hwnd, IDC_PREFS_FONTDEF);
			nsoption_set_int(font_default,
					 SendMessage(sub, CB_GETCURSEL, 0, 0) + 1);
			break;
		
		}
		break;

	}
	return FALSE;
}

static BOOL CALLBACK options_connections_dialog_handler(HWND hwnd,
		UINT msg, WPARAM wparam, LPARAM lParam)
{
	int len;
	char *temp, number[6];
	HWND sub;

	LOG_WIN_MSG(hwnd, msg, wparam, lParam);

	switch (msg) {
	case WM_INITDIALOG:
		sub = GetDlgItem(hwnd, IDC_PREFS_PROXYTYPE);
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"None");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Simple");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"Basic Auth");
		SendMessage(sub, CB_ADDSTRING, 0, (LPARAM)"NTLM Auth");
		if (nsoption_bool(http_proxy)) {
			SendMessage(sub, CB_SETCURSEL, (WPARAM)
				    (nsoption_int(http_proxy_auth) + 1), 0);
		} else {
			SendMessage(sub, CB_SETCURSEL, 0, 0);
		}

		sub = GetDlgItem(hwnd, IDC_PREFS_PROXYHOST);
		if ((nsoption_charp(http_proxy_host) != NULL) &&
		    (nsoption_charp(http_proxy_host)[0] != '\0'))
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(http_proxy_host));

		sub = GetDlgItem(hwnd, IDC_PREFS_PROXYPORT);
		if (nsoption_int(http_proxy_port) != 0) {
			snprintf(number, 6, "%d", nsoption_int(http_proxy_port));
			SendMessage(sub, WM_SETTEXT, 0,	(LPARAM)number);
		}

		sub = GetDlgItem(hwnd, IDC_PREFS_PROXYNAME);
		if ((nsoption_charp(http_proxy_auth_user) != NULL) &&
		    (nsoption_charp(http_proxy_auth_user)[0] != '\0'))
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(http_proxy_auth_user));

		sub = GetDlgItem(hwnd, IDC_PREFS_PROXYPASS);
		if ((nsoption_charp(http_proxy_auth_pass) != NULL) &&
		    (nsoption_charp(http_proxy_auth_pass)[0] != '\0'))
			SendMessage(sub, WM_SETTEXT, 0,
				    (LPARAM)nsoption_charp(http_proxy_auth_pass));

		sub = GetDlgItem(hwnd, IDC_PREFS_FETCHERS);
		snprintf(number, 6, "%d", nsoption_int(max_fetchers));
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);

		sub = GetDlgItem(hwnd, IDC_PREFS_FETCH_HOST);
		snprintf(number, 6, "%d", nsoption_int(max_fetchers_per_host));
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);

		sub = GetDlgItem(hwnd, IDC_PREFS_FETCH_HANDLES);
		snprintf(number, 6, "%d", nsoption_int(max_cached_fetch_handles));
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)number);

		break;

	case WM_NOTIFY:
		switch (((NMHDR FAR *)lParam)->code) {
		case PSN_APPLY:
			sub = GetDlgItem(hwnd, IDC_PREFS_PROXYHOST);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_charp(http_proxy_host, strdup(temp));
				free(temp);
			}

			sub = GetDlgItem(hwnd, IDC_PREFS_PROXYPORT);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_int(http_proxy_port, atoi(temp));
				free(temp);
			}

			sub = GetDlgItem(hwnd, IDC_PREFS_PROXYNAME);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_charp(http_proxy_auth_user, strdup(temp));
				free(temp);
			}

			sub = GetDlgItem(hwnd, IDC_PREFS_PROXYPASS);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_charp(http_proxy_auth_pass, strdup(temp));
				free(temp);
			}

			/* fetchers */
			sub = GetDlgItem(hwnd, IDC_PREFS_FETCHERS);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_int(max_fetchers, atoi(temp));
				free(temp);
			}

			sub = GetDlgItem(hwnd, IDC_PREFS_FETCH_HOST);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_int(max_fetchers_per_host, atoi(temp));
				free(temp);
			}

			sub = GetDlgItem(hwnd, IDC_PREFS_FETCH_HANDLES);
			len = SendMessage(sub, WM_GETTEXTLENGTH, 0, 0);
			temp = malloc(len + 1);
			if (temp != NULL) {
				SendMessage(sub, WM_GETTEXT, (WPARAM)(len + 1),
					    (LPARAM)temp);
				nsoption_set_int(max_cached_fetch_handles, atoi(temp));
				free(temp);
			}
			break;

		case UDN_DELTAPOS: {
			NMUPDOWN *ud = (NMUPDOWN *)lParam;
			switch(((NMHDR *)lParam)->idFrom) {
			case IDC_PREFS_FETCHERS_SPIN:
				change_spinner(GetDlgItem(hwnd, IDC_PREFS_FETCHERS), 1.0  * ud->iDelta, 1.0, 100.0);
				return TRUE;

			case IDC_PREFS_FETCH_HOST_SPIN:
				change_spinner(GetDlgItem(hwnd, IDC_PREFS_FETCH_HOST), 1.0  * ud->iDelta, 1.0, 100.0);
				return TRUE;

			case IDC_PREFS_FETCH_HANDLES_SPIN:
				change_spinner(GetDlgItem(hwnd, IDC_PREFS_FETCH_HANDLES), 1.0  * ud->iDelta, 1.0, 100.0);
				return TRUE;

			}
		}
			break;
		}
	}
	return FALSE;
}

static BOOL CALLBACK options_general_dialog_handler(HWND hwnd,
		UINT msg, WPARAM wparam, LPARAM lParam)
{
	HWND sub;

	LOG_WIN_MSG(hwnd, msg, wparam, lParam);

	switch (msg) {
	case WM_INITDIALOG:
		/* homepage url */
		sub = GetDlgItem(hwnd, IDC_PREFS_HOMEPAGE);
		SendMessage(sub, WM_SETTEXT, 0, (LPARAM)nsoption_charp(homepage_url));

		/* Display images */
		sub = GetDlgItem(hwnd, IDC_PREFS_IMAGES);
		SendMessage(sub, BM_SETCHECK, 
			    (WPARAM) ((nsoption_bool(suppress_images)) ? 
				  BST_CHECKED : BST_UNCHECKED), 0);

		/* advert blocking */
		sub = GetDlgItem(hwnd, IDC_PREFS_ADVERTS);
		SendMessage(sub, BM_SETCHECK, 
			    (WPARAM) ((nsoption_bool(block_advertisements)) ? 
				  BST_CHECKED : BST_UNCHECKED), 0);

		/* Referrer sending */
		sub = GetDlgItem(hwnd, IDC_PREFS_REFERER);
		SendMessage(sub, BM_SETCHECK, 
			    (WPARAM)((nsoption_bool(send_referer)) ?
				 BST_CHECKED : BST_UNCHECKED), 0);
		break;

	case WM_NOTIFY:
		switch (((NMHDR FAR *)lParam)->code) {
		case PSN_APPLY:
			/* homepage */
			sub = GetDlgItem(hwnd, IDC_PREFS_HOMEPAGE);
			if (sub != NULL) {
				int text_length;
				char *text;
				text_length = SendMessage(sub, 
						  WM_GETTEXTLENGTH, 0, 0);
				text = malloc(text_length + 1);
				if (text != NULL) {
					SendMessage(sub, WM_GETTEXT,
						    (WPARAM)text_length + 1,
						    (LPARAM)text);
					nsoption_set_charp(homepage_url, text);
				}
			}

			nsoption_set_bool(suppress_images,
					  (IsDlgButtonChecked(hwnd, IDC_PREFS_IMAGES) == BST_CHECKED) ? true : false);

			nsoption_set_bool(block_advertisements, (IsDlgButtonChecked(hwnd, 
									 IDC_PREFS_ADVERTS) == BST_CHECKED) ? true : false);

			nsoption_set_bool(send_referer, (IsDlgButtonChecked(hwnd, 
									    IDC_PREFS_REFERER) == BST_CHECKED) ? true : false);

			break;
			
		}
	}
	return FALSE;
}

void nsws_prefs_dialog_init(HINSTANCE hinst, HWND parent)
{
	int ret;
	PROPSHEETPAGE psp[3];
	PROPSHEETHEADER psh;

	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags = 0;/*PSP_USEICONID*/
	psp[0].hInstance = hinst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_DLG_OPTIONS_GENERAL);
	psp[0].pfnDlgProc = options_general_dialog_handler;
	psp[0].lParam = 0;
	psp[0].pfnCallback = NULL;

	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags = 0;/*PSP_USEICONID*/
	psp[1].hInstance = hinst;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_DLG_OPTIONS_CONNECTIONS);
	psp[1].pfnDlgProc = options_connections_dialog_handler;
	psp[1].lParam = 0;
	psp[1].pfnCallback = NULL;

	psp[2].dwSize = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags = 0;/*PSP_USEICONID*/
	psp[2].hInstance = hinst;
	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_DLG_OPTIONS_APPERANCE);
	psp[2].pfnDlgProc = options_appearance_dialog_handler;
	psp[2].lParam = 0;
	psp[2].pfnCallback = NULL;


	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_NOAPPLYNOW | PSH_USEICONID | PSH_PROPSHEETPAGE;
	psh.hwndParent = parent;
	psh.hInstance = hinst;
	psh.pszIcon = MAKEINTRESOURCE(IDR_NETSURF_ICON);
	psh.pszCaption = (LPSTR) "NetSurf Options";
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE) &psp;
	psh.pfnCallback = NULL;

	ret = PropertySheet(&psh);
	if (ret == -1) {
		win_perror("PropertySheet");
	} else if (ret > 0) {
		/* user saved changes */
		nsoption_write(options_file_location, NULL, NULL);
	}
}
