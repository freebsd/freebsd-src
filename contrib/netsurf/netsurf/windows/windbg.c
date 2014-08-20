/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

#include <windows.h>
#include <stdio.h>

#include "windbg.h"

const char *msg_num_to_name(int msg)
{
	static char str[256];

	switch (msg) {
	case 32768:
		return "WM_APP";

	case 6:
		return "WM_ACTIVATE ";

	case 28:
		return "WM_ACTIVATEAPP ";

	case 864:
		return "WM_AFXFIRST ";

	case 895:
		return "WM_AFXLAST ";

	case 780:
		return "WM_ASKCBFORMATNAME ";

	case 75:
		return "WM_CANCELJOURNAL ";

	case 31:
		return "WM_CANCELMODE ";

	case 533:
		return "WM_CAPTURECHANGED ";

	case 781:
		return "WM_CHANGECBCHAIN ";

	case 258:
		return "WM_CHAR ";

	case 47:
		return "WM_CHARTOITEM ";

	case 34:
		return "WM_CHILDACTIVATE ";

	case 771:
		return "WM_CLEAR ";

	case 16:
		return "WM_CLOSE ";

	case 273:
		return "WM_COMMAND ";

	case 68:
		return "WM_COMMNOTIFY ";

	case 65:
		return "WM_COMPACTING ";

	case 57:
		return "WM_COMPAREITEM ";

	case 123:
		return "WM_CONTEXTMENU ";

	case 769:
		return "WM_COPY ";

	case 74:
		return "WM_COPYDATA ";

	case 1:
		return "WM_CREATE ";

	case 309:
		return "WM_CTLCOLORBTN ";

	case 310:
		return "WM_CTLCOLORDLG ";

	case 307:
		return "WM_CTLCOLOREDIT ";

	case 308:
		return "WM_CTLCOLORLISTBOX ";

	case 306:
		return "WM_CTLCOLORMSGBOX ";

	case 311:
		return "WM_CTLCOLORSCROLLBAR ";

	case 312:
		return "WM_CTLCOLORSTATIC ";

	case 768:
		return "WM_CUT ";

	case 259:
		return "WM_DEADCHAR ";

	case 45:
		return "WM_DELETEITEM ";

	case 2:
		return "WM_DESTROY ";

	case 775:
		return "WM_DESTROYCLIPBOARD ";

	case 537:
		return "WM_DEVICECHANGE ";

	case 27:
		return "WM_DEVMODECHANGE ";

	case 126:
		return "WM_DISPLAYCHANGE ";

	case 776:
		return "WM_DRAWCLIPBOARD ";

	case 43:
		return "WM_DRAWITEM ";

	case 563:
		return "WM_DROPFILES ";

	case 10:
		return "WM_ENABLE ";

	case 22:
		return "WM_ENDSESSION ";

	case 289:
		return "WM_ENTERIDLE ";

	case 529:
		return "WM_ENTERMENULOOP ";

	case 561:
		return "WM_ENTERSIZEMOVE ";

	case 20:
		return "WM_ERASEBKGND ";

	case 530:
		return "WM_EXITMENULOOP ";

	case 562:
		return "WM_EXITSIZEMOVE ";

	case 29:
		return "WM_FONTCHANGE ";

	case 135:
		return "WM_GETDLGCODE ";

	case 49:
		return "WM_GETFONT ";

	case 51:
		return "WM_GETHOTKEY ";

	case 127:
		return "WM_GETICON ";

	case 36:
		return "WM_GETMINMAXINFO ";

	case 13:
		return "WM_GETTEXT ";

	case 14:
		return "WM_GETTEXTLENGTH ";

	case 856:
		return "WM_HANDHELDFIRST ";

	case 863:
		return "WM_HANDHELDLAST ";

	case 83:
		return "WM_HELP ";

	case 786:
		return "WM_HOTKEY ";

	case 276:
		return "WM_HSCROLL ";

	case 782:
		return "WM_HSCROLLCLIPBOARD ";

	case 39:
		return "WM_ICONERASEBKGND ";

	case 272:
		return "WM_INITDIALOG ";

	case 278:
		return "WM_INITMENU ";

	case 279:
		return "WM_INITMENUPOPUP ";

	case 0x00FF:
		return "WM_INPUT ";

	case 81:
		return "WM_INPUTLANGCHANGE ";

	case 80:
		return "WM_INPUTLANGCHANGEREQUEST ";

	case 256:
		return "WM_KEYDOWN ";

	case 257:
		return "WM_KEYUP ";

	case 8:
		return "WM_KILLFOCUS ";

	case 546:
		return "WM_MDIACTIVATE ";

	case 551:
		return "WM_MDICASCADE ";

	case 544:
		return "WM_MDICREATE ";

	case 545:
		return "WM_MDIDESTROY ";

	case 553:
		return "WM_MDIGETACTIVE ";

	case 552:
		return "WM_MDIICONARRANGE ";

	case 549:
		return "WM_MDIMAXIMIZE ";

	case 548:
		return "WM_MDINEXT ";

	case 564:
		return "WM_MDIREFRESHMENU ";

	case 547:
		return "WM_MDIRESTORE ";

	case 560:
		return "WM_MDISETMENU ";

	case 550:
		return "WM_MDITILE ";

	case 44:
		return "WM_MEASUREITEM ";

	case 0x003D:
		return "WM_GETOBJECT ";

	case 0x0127:
		return "WM_CHANGEUISTATE ";

	case 0x0128:
		return "WM_UPDATEUISTATE ";

	case 0x0129:
		return "WM_QUERYUISTATE ";

	case 0x0125:
		return "WM_UNINITMENUPOPUP ";

	case 290:
		return "WM_MENURBUTTONUP ";

	case 0x0126:
		return "WM_MENUCOMMAND ";

	case 0x0124:
		return "WM_MENUGETOBJECT ";

	case 0x0123:
		return "WM_MENUDRAG ";

	case 0x0319:
		return "WM_APPCOMMAND ";

	case 288:
		return "WM_MENUCHAR ";

	case 287:
		return "WM_MENUSELECT ";

	case 531:
		return "WM_NEXTMENU ";

	case 3:
		return "WM_MOVE ";

	case 534:
		return "WM_MOVING ";

	case 134:
		return "WM_NCACTIVATE ";

	case 131:
		return "WM_NCCALCSIZE ";

	case 129:
		return "WM_NCCREATE ";

	case 130:
		return "WM_NCDESTROY ";

	case 132:
		return "WM_NCHITTEST ";

	case 163:
		return "WM_NCLBUTTONDBLCLK ";

	case 161:
		return "WM_NCLBUTTONDOWN ";

	case 162:
		return "WM_NCLBUTTONUP ";

	case 169:
		return "WM_NCMBUTTONDBLCLK ";

	case 167:
		return "WM_NCMBUTTONDOWN ";

	case 168:
		return "WM_NCMBUTTONUP ";

	case 171:
		return "WM_NCXBUTTONDOWN ";

	case 172:
		return "WM_NCXBUTTONUP ";

	case 173:
		return "WM_NCXBUTTONDBLCLK ";

	case 0x02A0:
		return "WM_NCMOUSEHOVER ";

	case 0x02A2:
		return "WM_NCMOUSELEAVE ";

	case 160:
		return "WM_NCMOUSEMOVE ";

	case 133:
		return "WM_NCPAINT ";

	case 166:
		return "WM_NCRBUTTONDBLCLK ";

	case 164:
		return "WM_NCRBUTTONDOWN ";

	case 165:
		return "WM_NCRBUTTONUP ";

	case 40:
		return "WM_NEXTDLGCTL ";

	case 78:
		return "WM_NOTIFY ";

	case 85:
		return "WM_NOTIFYFORMAT ";

	case 0:
		return "WM_NULL ";

	case 15:
		return "WM_PAINT ";

	case 777:
		return "WM_PAINTCLIPBOARD ";

	case 38:
		return "WM_PAINTICON ";

	case 785:
		return "WM_PALETTECHANGED ";

	case 784:
		return "WM_PALETTEISCHANGING ";

	case 528:
		return "WM_PARENTNOTIFY ";

	case 770:
		return "WM_PASTE ";

	case 896:
		return "WM_PENWINFIRST ";

	case 911:
		return "WM_PENWINLAST ";

	case 72:
		return "WM_POWER ";

	case 536:
		return "WM_POWERBROADCAST ";

	case 791:
		return "WM_PRINT ";

	case 792:
		return "WM_PRINTCLIENT ";

	case 55:
		return "WM_QUERYDRAGICON ";

	case 17:
		return "WM_QUERYENDSESSION ";

	case 783:
		return "WM_QUERYNEWPALETTE ";

	case 19:
		return "WM_QUERYOPEN ";

	case 35:
		return "WM_QUEUESYNC ";

	case 18:
		return "WM_QUIT ";

	case 774:
		return "WM_RENDERALLFORMATS ";

	case 773:
		return "WM_RENDERFORMAT ";

	case 32:
		return "WM_SETCURSOR ";

	case 7:
		return "WM_SETFOCUS ";

	case 48:
		return "WM_SETFONT ";

	case 50:
		return "WM_SETHOTKEY ";

	case 128:
		return "WM_SETICON ";

	case 11:
		return "WM_SETREDRAW ";

	case 12:
		return "WM_SETTEXT ";

	case 26:
		return "WM_SETTINGCHANGE ";

	case 24:
		return "WM_SHOWWINDOW ";

	case 5:
		return "WM_SIZE ";

	case 779:
		return "WM_SIZECLIPBOARD ";

	case 532:
		return "WM_SIZING ";

	case 42:
		return "WM_SPOOLERSTATUS ";

	case 125:
		return "WM_STYLECHANGED ";

	case 124:
		return "WM_STYLECHANGING ";

	case 262:
		return "WM_SYSCHAR ";

	case 21:
		return "WM_SYSCOLORCHANGE ";

	case 274:
		return "WM_SYSCOMMAND ";

	case 263:
		return "WM_SYSDEADCHAR ";

	case 260:
		return "WM_SYSKEYDOWN ";

	case 261:
		return "WM_SYSKEYUP ";

	case 82:
		return "WM_TCARD ";

	case 794:
		return "WM_THEMECHANGED ";

	case 30:
		return "WM_TIMECHANGE ";

	case 275:
		return "WM_TIMER ";

	case 772:
		return "WM_UNDO ";

	case 1024:
		return "WM_USER ";

	case 84:
		return "WM_USERCHANGED ";

	case 46:
		return "WM_VKEYTOITEM ";

	case 277:
		return "WM_VSCROLL ";

	case 778:
		return "WM_VSCROLLCLIPBOARD ";

	case 71:
		return "WM_WINDOWPOSCHANGED ";

	case 70:
		return "WM_WINDOWPOSCHANGING ";

	case 264:
		return "WM_KEYLAST ";

	case 136:
		return "WM_SYNCPAINT  ";

	case 33:
		return "WM_MOUSEACTIVATE ";

	case 512:
		return "WM_MOUSEMOVE ";

	case 513:
		return "WM_LBUTTONDOWN ";

	case 514:
		return "WM_LBUTTONUP ";

	case 515:
		return "WM_LBUTTONDBLCLK ";

	case 516:
		return "WM_RBUTTONDOWN ";

	case 517:
		return "WM_RBUTTONUP ";

	case 518:
		return "WM_RBUTTONDBLCLK ";

	case 519:
		return "WM_MBUTTONDOWN ";

	case 520:
		return "WM_MBUTTONUP ";

	case 521:
		return "WM_MBUTTONDBLCLK ";

	case 522:
		return "WM_MOUSEWHEEL ";

	case 523:
		return "WM_XBUTTONDOWN ";

	case 524:
		return "WM_XBUTTONUP ";

	case 525:
		return "WM_XBUTTONDBLCLK ";

	case 0x2A1:
		return "WM_MOUSEHOVER	";

	case 0x2A3:
		return "WM_MOUSELEAVE	";

	}

	sprintf(str,"%d",msg);

	return str;
}

void win_perror(const char * lpszFunction)
{
	/* Retrieve the system error message for the last-error code */

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	/* Display the error message and exit the process */

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));

	snprintf((LPTSTR)lpDisplayBuf,
		 LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		 TEXT("%s failed with error %ld: %s"),
		 lpszFunction, dw, (char *)lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}
