/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* passwd_dlg.c - Dialog boxes for Windows95/NT
 * Author:	Jörgen Karlsson - d93-jka@nada.kth.se
 * Date:	June 1996
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: passwd_dlg.c,v 1.11 1999/12/02 16:58:40 joda Exp $");
#endif

#ifdef WIN32	/* Visual C++ 4.0 (Windows95/NT) */
#include <Windows.h>
#include "passwd_dlg.h"
#include "Resource.h"
#define passwdBufSZ 64
#define usr_nameSZ 64

static char user_name[usr_nameSZ];
static char passwd[passwdBufSZ];

BOOL CALLBACK
pwd_dialog_proc(HWND hwndDlg,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
	SetDlgItemText(hwndDlg, IDC_EDIT1, user_name);
	return TRUE;
	break;

    case WM_COMMAND: 
	switch(wParam)
	{
	case IDOK:
	    if(!GetDlgItemText(hwndDlg,IDC_EDIT1, user_name, usr_nameSZ))
		EndDialog(hwndDlg, IDCANCEL);
	    if(!GetDlgItemText(hwndDlg,IDC_EDIT2, passwd, passwdBufSZ))
		EndDialog(hwndDlg, IDCANCEL);
	case IDCANCEL:
	    EndDialog(hwndDlg, wParam);
	    return TRUE;
	}
	break;
    }
    return FALSE;
}


/* return 0 if ok, 1 otherwise */
int
pwd_dialog(char *user, size_t user_sz,
	   char *password, size_t password_sz)
{
    int i;
    HWND wnd = GetActiveWindow();
    HANDLE hInst = GetModuleHandle("kclnt32");

    strlcpy(user_name, user, sizeof(user_name));
    switch(DialogBox(hInst,MAKEINTRESOURCE(IDD_DIALOG1),wnd,pwd_dialog_proc))
    {
    case IDOK:
	strlcpy(user, user_name, user_sz);
	strlcpy(password, passwd, password_sz);
	memset (passwd, 0, sizeof(passwd));
	return 0;
    case IDCANCEL:
    default:
	memset (passwd, 0, sizeof(passwd));
	return 1;
    }
}

#endif /* WIN32 */
