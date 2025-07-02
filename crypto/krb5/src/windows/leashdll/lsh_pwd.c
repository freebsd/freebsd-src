#define SCALE_FACTOR 31/20

/* LSH_PWD.C

   Jason Hunter
   8/2/94
   DCNS/IS MIT

   Re-written for KFW 2.6 by Jeffrey Altman <jaltman@mit.edu>

   Contains the callback functions for the EnterPassword an
   ChangePassword dialog boxes and well as the API function
   calls:

   Lsh_Enter_Password_Dialog
   Lsh_Change_Password_Dialog

   for calling the dialogs.

   Also contains the callback for the MITPasswordControl.

*/

/* Standard Include files */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <string.h>

/* Private Inlclude files */
#include "leashdll.h"
#include <leashwin.h>
#include "leash-int.h"
#include "leashids.h"
#include <leasherr.h>
#include <krb5.h>
#include <commctrl.h>

extern void * Leash_pec_create(HWND hEditCtl);
extern void Leash_pec_destroy(void *pAutoComplete);
extern void Leash_pec_add_principal(char *principal);
extern void Leash_pec_clear_history(void *pec);

/* Global Variables. */
static long lsh_errno;
static char *err_context;       /* error context */
extern HINSTANCE hLeashInst;
extern HINSTANCE hKrb5;


INT_PTR
CALLBACK
PasswordProc(
    HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
    );

INT_PTR
CALLBACK
AuthenticateProc(
    HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
    );

INT_PTR
CALLBACK
NewPasswordProc(
    HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
    );


long Leash_get_lsh_errno(LONG *err_val)
{
    return lsh_errno;
}

/*/////// ******** API Calls follow here.   ******** /////////*/

static int
NetId_dialog(LPLSH_DLGINFO lpdlginfo)
{
    LRESULT             lrc;
    HWND    	        hNetIdMgr;
    HWND    		hForeground;

    hNetIdMgr = FindWindow("IDMgrRequestDaemonCls", "IDMgrRequestDaemon");
    if (hNetIdMgr != NULL) {
	char desiredPrincipal[512];
	NETID_DLGINFO *dlginfo;
	char		*desiredName = 0;
	char            *desiredRealm = 0;
	HANDLE hMap;
	DWORD  tid = GetCurrentThreadId();
	char mapname[256];

	strcpy(desiredPrincipal, lpdlginfo->principal);

	/* do we want a specific client principal? */
	if (desiredPrincipal[0]) {
	    char * p;
	    desiredName = desiredPrincipal;
	    for (p = desiredName; *p && *p != '@'; p++);
	    if ( *p == '@' ) {
		*p = '\0';
		desiredRealm = ++p;
	    }
	}

	sprintf(mapname,"Local\\NetIDMgr_DlgInfo_%lu",tid);

	hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
				 0, 4096, mapname);
	if (hMap == NULL) {
	    return -1;
	} else if (hMap != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
	    CloseHandle(hMap);
	    return -1;
	}

	dlginfo = (NETID_DLGINFO *)MapViewOfFileEx(hMap, FILE_MAP_READ|FILE_MAP_WRITE,
						 0, 0, 4096, NULL);
	if (dlginfo == NULL) {
	    CloseHandle(hMap);
	    return -1;
	}

	hForeground = GetForegroundWindow();

	memset(dlginfo, 0, sizeof(NETID_DLGINFO));

	dlginfo->size = sizeof(NETID_DLGINFO);
	if (lpdlginfo->dlgtype == DLGTYPE_PASSWD)
	    dlginfo->dlgtype = NETID_DLGTYPE_TGT;
	else
	    dlginfo->dlgtype = NETID_DLGTYPE_CHPASSWD;
	dlginfo->in.use_defaults = 1;

	if (lpdlginfo->title) {
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				lpdlginfo->title, -1,
				dlginfo->in.title, NETID_TITLE_SZ);
	} else if (desiredName && (strlen(desiredName) + strlen(desiredRealm) + 32 < NETID_TITLE_SZ)) {
	    char mytitle[NETID_TITLE_SZ];
	    sprintf(mytitle, "Obtain Kerberos TGT for %s@%s",desiredName,desiredRealm);
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				mytitle, -1,
				dlginfo->in.title, NETID_TITLE_SZ);
	} else {
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				"Obtain Kerberos TGT", -1,
				dlginfo->in.title, NETID_TITLE_SZ);
	}
	if (desiredName)
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				desiredName, -1,
				dlginfo->in.username, NETID_USERNAME_SZ);
	if (desiredRealm)
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				desiredRealm, -1,
				dlginfo->in.realm, NETID_REALM_SZ);
	lrc = SendMessage(hNetIdMgr, 32810, 0, (LPARAM) tid);

	UnmapViewOfFile(dlginfo);
	CloseHandle(hMap);

	SetForegroundWindow(hForeground);
	return lrc;
    }
    return -1;
}

static int
NetId_dialog_ex(LPLSH_DLGINFO_EX lpdlginfo)
{
    HWND    	        hNetIdMgr;
    HWND    		hForeground;

    hNetIdMgr = FindWindow("IDMgrRequestDaemonCls", "IDMgrRequestDaemon");
    if (hNetIdMgr != NULL) {
	NETID_DLGINFO   *dlginfo;
	char		*desiredName = lpdlginfo->username;
	char            *desiredRealm = lpdlginfo->realm;
	LPSTR            title;
	char            *ccache;
	LRESULT         lrc;
	HANDLE hMap;
	DWORD  tid = GetCurrentThreadId();
	char mapname[256];

	sprintf(mapname,"Local\\NetIDMgr_DlgInfo_%lu",tid);

	hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
				 0, 4096, mapname);
	if (hMap == NULL) {
	    return -1;
	} else if (hMap != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
	    CloseHandle(hMap);
	    return -1;
	}

	dlginfo = (NETID_DLGINFO *)MapViewOfFileEx(hMap, FILE_MAP_READ|FILE_MAP_WRITE,
						 0, 0, 4096, NULL);
	if (dlginfo == NULL) {
	    CloseHandle(hMap);
	    return -1;
	}

	hForeground = GetForegroundWindow();

	if (lpdlginfo->size == LSH_DLGINFO_EX_V1_SZ ||
	    lpdlginfo->size == LSH_DLGINFO_EX_V2_SZ)
	{
	    title = lpdlginfo->title;
	    desiredName = lpdlginfo->username;
	    desiredRealm = lpdlginfo->realm;
	    ccache = NULL;
	} else {
	    title = lpdlginfo->in.title;
	    desiredName = lpdlginfo->in.username;
	    desiredRealm = lpdlginfo->in.realm;
	    ccache = lpdlginfo->in.ccache;
	}

	memset(dlginfo, 0, sizeof(NETID_DLGINFO));

	dlginfo->size = sizeof(NETID_DLGINFO);
	if (lpdlginfo->dlgtype == DLGTYPE_PASSWD)
	    dlginfo->dlgtype = NETID_DLGTYPE_TGT;
	else
	    dlginfo->dlgtype = NETID_DLGTYPE_CHPASSWD;

	dlginfo->in.use_defaults = lpdlginfo->use_defaults;
	dlginfo->in.forwardable  = lpdlginfo->forwardable;
	dlginfo->in.noaddresses  = lpdlginfo->noaddresses;
	dlginfo->in.lifetime     = lpdlginfo->lifetime;
	dlginfo->in.renew_till   = lpdlginfo->renew_till;
	dlginfo->in.proxiable    = lpdlginfo->proxiable;
	dlginfo->in.publicip     = lpdlginfo->publicip;

	if (title) {
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				title, -1,
				dlginfo->in.title, NETID_TITLE_SZ);
	} else if (desiredName && (strlen(desiredName) + strlen(desiredRealm) + 32 < NETID_TITLE_SZ)) {
	    char mytitle[NETID_TITLE_SZ];
	    sprintf(mytitle, "Obtain Kerberos TGT for %s@%s",desiredName,desiredRealm);
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				mytitle, -1,
				dlginfo->in.title, NETID_TITLE_SZ);
	} else {
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				"Obtain Kerberos TGT", -1,
				dlginfo->in.title, NETID_TITLE_SZ);
	}
	if (desiredName)
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				desiredName, -1,
				dlginfo->in.username, NETID_USERNAME_SZ);
	if (desiredRealm)
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				desiredRealm, -1,
				dlginfo->in.realm, NETID_REALM_SZ);
	if (ccache)
	    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS,
				ccache, -1,
				dlginfo->in.ccache, NETID_CCACHE_NAME_SZ);
	lrc = SendMessage(hNetIdMgr, 32810, 0, (LPARAM) tid);

	if (lrc > 0) {
	    if (lpdlginfo->size == LSH_DLGINFO_EX_V2_SZ)
	    {
		WideCharToMultiByte(CP_ACP, 0, dlginfo->out.username, -1,
				     lpdlginfo->out.username, LEASH_USERNAME_SZ,
				     NULL, NULL);
		WideCharToMultiByte(CP_ACP, 0, dlginfo->out.realm, -1,
				     lpdlginfo->out.realm, LEASH_REALM_SZ,
				     NULL, NULL);
	    }
	    if (lpdlginfo->size == LSH_DLGINFO_EX_V3_SZ)
	    {
		WideCharToMultiByte(CP_ACP, 0, dlginfo->out.ccache, -1,
				     lpdlginfo->out.ccache, LEASH_CCACHE_NAME_SZ,
				     NULL, NULL);
	    }
	}

	UnmapViewOfFile(dlginfo);
	CloseHandle(hMap);

	SetForegroundWindow(hForeground);
	return lrc;
    }
    return -1;
}


#define LEASH_DLG_MUTEX_NAME  TEXT("Leash_Dialog_Mutex")
int Leash_kinit_dlg(HWND hParent, LPLSH_DLGINFO lpdlginfo)
{
    int rc;
    HANDLE hMutex;

    rc = NetId_dialog(lpdlginfo);
    if (rc > -1)
	return rc;

    hMutex = CreateMutex(NULL, TRUE, LEASH_DLG_MUTEX_NAME);
    if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
        if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
            return -1;
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;   /* pretend the dialog was displayed and succeeded */
    }

    lpdlginfo->dlgtype = DLGTYPE_PASSWD;

    /* set the help file */
    Leash_set_help_file(NULL);

    /* Call the Dialog box with the DLL's Password Callback and the
       DLL's instance handle. */
    rc =  DialogBoxParam(hLeashInst, "EnterPasswordDlg", hParent,
                          PasswordProc, (LPARAM)lpdlginfo);

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return rc;
}


int Leash_kinit_dlg_ex(HWND hParent, LPLSH_DLGINFO_EX lpdlginfo)
{
    int rc;
    HANDLE hMutex;

    rc = NetId_dialog_ex(lpdlginfo);
    if (rc > -1)
	return rc;

    hMutex = CreateMutex(NULL, TRUE, LEASH_DLG_MUTEX_NAME);
    if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
        if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
            return -1;
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;   /* pretend the dialog was displayed and succeeded */
    }

    /* set the help file */
    Leash_set_help_file(NULL);

    /* Call the Dialog box with the DLL's Password Callback and the
       DLL's instance handle. */
    rc = DialogBoxParam(hLeashInst, MAKEINTRESOURCE(IDD_AUTHENTICATE), hParent,
                          AuthenticateProc, (LPARAM)lpdlginfo);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return rc;
}


int Leash_changepwd_dlg(HWND hParent, LPLSH_DLGINFO lpdlginfo)
{
    int rc;
    HANDLE hMutex;

    rc = NetId_dialog(lpdlginfo);
    if (rc > -1)
	return rc;

    hMutex = CreateMutex(NULL, TRUE, LEASH_DLG_MUTEX_NAME);
    if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
        if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
            return -1;
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;   /* pretend the dialog was displayed and succeeded */
    }

    lpdlginfo->dlgtype = DLGTYPE_CHPASSWD;

    /* Call the Dialog box with the DLL's Password Callback and the
       DLL's instance handle. */
    rc = DialogBoxParam(hLeashInst, "CHANGEPASSWORDDLG", hParent,
                          PasswordProc, (LPARAM)lpdlginfo);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return rc;
}

int Leash_changepwd_dlg_ex(HWND hParent, LPLSH_DLGINFO_EX lpdlginfo)
{
    int rc;
    HANDLE hMutex;

    rc = NetId_dialog_ex(lpdlginfo);
    if (rc > -1)
	return rc;

    hMutex = CreateMutex(NULL, TRUE, LEASH_DLG_MUTEX_NAME);
    if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
        if ( WaitForSingleObject( hMutex, INFINITE ) != WAIT_OBJECT_0 ) {
            return -1;
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;   /* pretend the dialog was displayed and succeeded */
    }

    lpdlginfo->dlgtype = DLGTYPE_CHPASSWD;

    /* Call the Dialog box with the DLL's Password Callback and the
       DLL's instance handle. */
    rc = DialogBoxParam(hLeashInst, MAKEINTRESOURCE(IDD_PASSWORD), hParent,
                          NewPasswordProc, (LPARAM)lpdlginfo);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return rc;
}


/*  These little utils are taken from lshutil.c
    they are added here for the Call back function.
****** beginning of added utils from lshutil.c  ******/

BOOL IsDlgItem(HWND hWnd, WORD id)
{
    HWND hChild;

    hChild = GetDlgItem(hWnd, id);
    return hChild ? IsWindow(hChild) : 0;
}

int lsh_getkeystate(WORD keyid)
{
    static BYTE keys[256];

    GetKeyboardState((LPBYTE) &keys);
    return (int) keys[keyid];
}

LPSTR krb_err_func(int offset, long code)
{
    return(NULL);
}

/****** End of Added utils from leash.c  ******/


int PaintLogoBitmap( HANDLE hPicFrame )
{
    HBITMAP hBitmap;
    HBITMAP hOldBitmap;
    BITMAP Bitmap;
    HDC hdc, hdcMem;
    RECT rect;

    /* Invalidate the drawing space of the picframe. */
    InvalidateRect( hPicFrame, NULL, TRUE);
    UpdateWindow( hPicFrame );

    hdc = GetDC(hPicFrame);
    hdcMem = CreateCompatibleDC(hdc);
    GetClientRect(hPicFrame, &rect);
    hBitmap = LoadBitmap(hLeashInst, "LOGOBITMAP");
    hOldBitmap = SelectObject(hdcMem, hBitmap);
    GetObject(hBitmap, sizeof(Bitmap), (LPSTR) &Bitmap);
    StretchBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0,
               Bitmap.bmWidth, Bitmap.bmHeight, SRCCOPY);

    SelectObject(hdcMem, hOldBitmap); /* pbh 8-15-94 */
    ReleaseDC(hPicFrame, hdc);
    DeleteObject( hBitmap );  /* pbh 8-15-94 */
    DeleteDC( hdcMem );       /* pbh 8-15-94 */

    return 0;
}


/* Callback function for the Password Dialog box that initilializes and
   renews tickets. */

INT_PTR
CALLBACK
PasswordProc(
    HWND hDialog,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    static POINT Position = { -1, -1 };
    static short state;
    int lifetime;
#define ISCHPASSWD (lpdi->dlgtype == DLGTYPE_CHPASSWD)
#define STATE_INIT     0
#define STATE_PRINCIPAL 1
#define STATE_OLDPWD   2
#define STATE_NEWPWD1  3
#define STATE_NEWPWD2  4
#define STATE_CLOSED   5
#define NEXTSTATE(newstate) SendMessage(hDialog, WM_COMMAND, ID_NEXTSTATE, newstate)
    static int ids[STATE_NEWPWD2 + 1] = {
        0,
        ID_PRINCIPAL, ID_OLDPASSWORD, ID_CONFIRMPASSWORD1,
        ID_CONFIRMPASSWORD2};
    static char principal[255], oldpassword[255], newpassword[255],
        newpassword2[255];
    static char *strings[STATE_NEWPWD2 + 1] = {
        NULL, principal, oldpassword, newpassword, newpassword2};
    static LPLSH_DLGINFO lpdi;
    char gbuf[200];                 /* global buffer for random stuff. */


#define checkfirst(id, stuff) IsDlgItem(hDialog, id) ? stuff : 0
#define CGetDlgItemText(hDlg, id, cp, len) checkfirst(id, GetDlgItemText(hDlg, id, cp, len))
#define CSetDlgItemText(hDlg, id, cp) checkfirst(id, SetDlgItemText(hDlg, id, cp))
#define CSetDlgItemInt(hDlg, id, i, b) checkfirst(id, SetDlgItemInt(hDlg, id, i, b))
#define CSendDlgItemMessage(hDlg, id, m, w, l) checkfirst(id, SendDlgItemMessage(hDlg, id, m, w, l))
#define CSendMessage(hwnd, m, w, l) IsWindow(hwnd) ? SendMessage(hwnd, m, w, l) : 0
#define CShowWindow(hwnd, state) IsWindow(hwnd) ? ShowWindow(hwnd, state) : 0

#define GETITEMTEXT(id, cp, maxlen) \
  GetDlgItemText(hDialog, id, (LPSTR)(cp), maxlen)
#define CloseMe(x) SendMessage(hDialog, WM_COMMAND, ID_CLOSEME, x)


#define EDITFRAMEIDOFFSET               500

    switch (message) {

    case WM_INITDIALOG:

        *( (LPLSH_DLGINFO far *)(&lpdi) ) = (LPLSH_DLGINFO)(LPSTR)lParam;
        lpdi->dlgstatemax = ISCHPASSWD ? STATE_NEWPWD2
            : STATE_OLDPWD;
        SetWindowText(hDialog, lpdi->title);
        /* stop at old password for normal password dlg */

        SetProp(hDialog, "HANDLES_HELP", (HANDLE)1);

        if (lpdi->principal)
            lstrcpy(principal, lpdi->principal);
        else
	{
            principal[0] = '\0';
            /* is there a principal already being used? if so, use it. */
	    }

        CSetDlgItemText(hDialog, ID_PRINCIPAL, principal);

        lifetime = Leash_get_default_lifetime();
        if (lifetime <= 0)
            lifetime = 600; /* 10 hours */

        CSetDlgItemInt(hDialog, ID_DURATION, lifetime, FALSE);

        /* setup text of stuff. */

        if (Position.x > 0 && Position.y > 0 &&
            Position.x < GetSystemMetrics(SM_CXSCREEN) &&
            Position.y < GetSystemMetrics(SM_CYSCREEN))
            SetWindowPos(hDialog, 0, Position.x, Position.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);

        /* set window pos to last saved window pos */


        /* replace standard edit control with our own password edit
           control for password entry. */
        {
            RECT r;
            POINT pxy, psz;
            HWND hwnd;
            int i;

            for (i = ID_OLDPASSWORD; i <= ids[lpdi->dlgstatemax]; i++)
            {
                hwnd = GetDlgItem(hDialog, i);
                GetWindowRect(hwnd, &r);
                psz.x = r.right - r.left;
                psz.y = r.bottom - r.top;

                pxy.x = r.left; pxy.y = r.top;
                ScreenToClient(hDialog, &pxy);

                /* create a substitute window: */

                DestroyWindow(hwnd);
                /* kill off old edit window. */

                CreateWindow(MIT_PWD_DLL_CLASS,	/* our password window :o] */
                             "",		/* no text */
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP, /* child window, visible,tabstop */
                             pxy.x, pxy.y,	/* x, y coords */
                             psz.x, psz.y,	/* width, height */
                             hDialog,		/* the parent */
                             (HMENU)i,		/* same id *//* id offset for the frames */
                             (HANDLE)hLeashInst,/* instance handles */
                             NULL);		/* createstruct */
            }
        }

        state = STATE_INIT;
        NEXTSTATE(STATE_PRINCIPAL);
        break;

    case WM_PAINT:
        PaintLogoBitmap( GetDlgItem(hDialog, ID_PICFRAME) );
        break;

    case WM_COMMAND:
        switch (wParam) {
        case ID_HELP:
	{
            WinHelp(GetWindow(hDialog,GW_OWNER), KRB_HelpFile, HELP_CONTEXT,
                    ISCHPASSWD ? ID_CHANGEPASSWORD : ID_INITTICKETS);
	}
	break;
        case ID_CLOSEME:
	{
            int i;

            for (i = STATE_PRINCIPAL; i <= lpdi->dlgstatemax; i++)
	    {
                memset(strings[i], '\0', 255);
                SetDlgItemText(hDialog, ids[i], "");
	    }
            /* I claim these passwords in the name
               of planet '\0'... */

            RemoveProp(hDialog, "HANDLES_HELP");
            state = STATE_CLOSED;
            EndDialog(hDialog, (int)lParam);
        return TRUE;
	}
	break;
        case ID_DURATION:
            break;
        case ID_PRINCIPAL:
        case ID_OLDPASSWORD:
        case ID_CONFIRMPASSWORD1:
        case ID_CONFIRMPASSWORD2:
            if (HIWORD(lParam) == EN_SETFOCUS)
            {
                /* nothing, for now. */
            }
            break;
        case ID_NEXTSTATE:
	{
            RECT rbtn, redit;
            POINT p;
            int idfocus, i, s;
            HWND hfocus, hbtn;
            int oldstate = state;

            state = (int)lParam;
            idfocus = ids[state];

#ifdef ONE_NEWPWDBOX
            if (state == STATE_NEWPWD2)
                SendDlgItemMessage(hDialog, ID_CONFIRMPASSWORD1, WM_SETTEXT,
                                   0, (LONG)(LPSTR)"");
#endif

            for (s = STATE_PRINCIPAL; s <= lpdi->dlgstatemax; s++)
	    {
                i = ids[s];

                if (s > state)
                    SendDlgItemMessage(hDialog, i, WM_SETTEXT, 0,
                                       (LONG)(LPSTR)"");
                EnableWindow(GetDlgItem(hDialog, i), i == idfocus);
                ShowWindow(GetDlgItem(hDialog, i),
                           (i <= idfocus ? SW_SHOW : SW_HIDE));
                /* ShowWindow(GetDlgItem(hDialog, i + CAPTION_OFFSET),
                   (i <= idfocus ? SW_SHOW : SW_HIDE));*/
                /* show caption? */
	    }
#ifdef ONE_NEWPWDBOX
            CSetDlgItemText(hDialog, ID_CONFIRMCAPTION1,
                            state < STATE_NEWPWD2 ?
                            "Enter new password:" :
                            "Enter new password again:");
            if (state == STATE_NEWPWD2)
	    {
                HWND htext;
                htext = GetDlgItem(hDialog, ID_CONFIRMCAPTION1);
                FlashAnyWindow(htext);
                WinSleep(50);
                FlashAnyWindow(htext);
	    }
#endif

            hfocus = GetDlgItem(hDialog, idfocus);
            if ( hfocus != (HWND)NULL ){
                SetFocus(hfocus); /* switch focus */
                if (idfocus >= ID_OLDPASSWORD)
                    SendMessage(hfocus, WM_SETTEXT, 0, (LPARAM) (LPSTR) "");
                else
                {
                    SendMessage(hfocus, EM_SETSEL, 0, MAKELONG(0, -1));
                }
                GetWindowRect(hfocus, &redit);
            }

            hbtn   = GetDlgItem(hDialog, IDOK);
            if( IsWindow(hbtn) ){
                GetWindowRect(hbtn, &rbtn);
                p.x = rbtn.left; p.y = redit.top;
                ScreenToClient(hDialog, &p);

                SetWindowPos(hbtn, 0, p.x, p.y, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER);
            }
	}
	break;
        case IDOK:
	{
	    char* p_Principal;
            DWORD value = 0;

	    GETITEMTEXT(ids[state], (LPSTR)strings[state], 255);

            switch(state)
            {
            case STATE_PRINCIPAL:
            {
                if (!principal[0])
                {
                    MessageBox(hDialog,
                                "You are not allowed to enter a blank principal.",
                               "Invalid Principal",
                               MB_OK | MB_ICONSTOP);
                    NEXTSTATE(STATE_PRINCIPAL);
                    return TRUE;
                }

	        // Change 'principal' to upper case after checking
	        // "UpperCase" value in the Registry
                p_Principal = strchr(principal, '@');

                if (p_Principal && Leash_get_default_uppercaserealm())
                    strupr(p_Principal);
                break;
            }
	    case STATE_OLDPWD:
            {
		int duration;

		if (!ISCHPASSWD)
                    duration = GetDlgItemInt(hDialog, ID_DURATION, 0, FALSE);
                if (!oldpassword[0])
                {
                    MessageBox(hDialog, "You are not allowed to enter a "
                               "blank password.",
                               "Invalid Password",
                               MB_OK | MB_ICONSTOP);
                    NEXTSTATE(STATE_OLDPWD);
                    return TRUE;
                }
                if (lpdi->dlgtype == DLGTYPE_CHPASSWD)
                    lsh_errno = Leash_int_checkpwd(principal, oldpassword, 1);
                else
                {
                    lsh_errno = Leash_int_kinit_ex( 0,
                                                    hDialog,
                                                    principal,
                                                    oldpassword,
                                                    duration,
                                                    Leash_get_default_forwardable(),
                                                    Leash_get_default_proxiable(),
                                                    Leash_get_default_renew_till(),
                                                    Leash_get_default_noaddresses(),
                                                    Leash_get_default_publicip(),
                                                    1
                                                    );
                }
		if (lsh_errno != 0)
                {
		    int next_state = state;
		    int capslock;
		    char *cp;

		    err_context = "";

		    switch(lsh_errno)
                    {
                    case LSH_INVPRINCIPAL:
                    case LSH_INVINSTANCE:
                    case LSH_INVREALM:
			next_state = STATE_PRINCIPAL;
			break;
                    }
		    capslock = lsh_getkeystate(VK_CAPITAL);
                    /* low-order bit means caps lock is
                       toggled; if so, warn user since there's
                       been an error. */
		    if (capslock & 1)
                    {
			lstrcpy((LPSTR)gbuf, (LPSTR)err_context);
			cp = gbuf + lstrlen((LPSTR)gbuf);
			if (cp != gbuf)
                            *cp++ = ' ';
			lstrcpy(cp, "(This may be because your CAPS LOCK key is down.)");
			err_context = gbuf;
                    }

// XXX		    DoNiftyErrorReport(lsh_errno, ISCHPASSWD ? ""
// XXX				       : "Ticket initialization failed.");
		    NEXTSTATE(next_state);
		    return TRUE;
                }
		if (ISCHPASSWD)
                    break;
		CloseMe(TRUE); /* success */
            }
            break;
	    case STATE_NEWPWD1:
            {
                int i = 0;
                int bit8 = 0;

                for( i = 0; i < 255; i++ ){
                    if( newpassword[i] == '\0' ){
                        if ( bit8 ) {
                            MessageBox(hDialog,
                                        "Passwords should not contain non-ASCII characters.",
                                        "Internationalization Warning",
                                        MB_OK | MB_ICONINFORMATION);
                        }
                        i = 255;
                        break;
                    } else if( !isprint(newpassword[i]) ){
                        memset(newpassword, '\0', 255);
                        /* I claim these passwords in the name of planet '\0'... */
                        MessageBox(hDialog,
                                   "Passwords may not contain non-printable characters.",
                                    "Invalid Password",
                                    MB_OK | MB_ICONSTOP);
                        NEXTSTATE(STATE_NEWPWD1);
                        return TRUE;
                    } else if ( newpassword[i] > 127 )
                        bit8 = 1;
                }
            }
            break;
	    case STATE_NEWPWD2:
                if (lstrcmp(newpassword, newpassword2))
                {
                    NEXTSTATE(STATE_NEWPWD1);
                    MessageBox(hDialog,
                                "The new password was not entered the same way twice.",
                                "Password validation error",
                                MB_OK | MB_ICONSTOP);
                    return TRUE;
                }
                else
                {
                    /* make them type both pwds again if error */
                    int next_state = STATE_NEWPWD1;
                    int capslock;
                    char *cp;

                    capslock = lsh_getkeystate(VK_CAPITAL);
                    /* low-order bit means caps lock is
                       toggled; if so, warn user since there's
                       been an error. */
                    if (capslock & 1)
                    {
                        lstrcpy((LPSTR)gbuf, (LPSTR)err_context);
                        cp = gbuf + lstrlen((LPSTR)gbuf);
                        if (cp != gbuf)
                            *cp++ = ' ';
                        lstrcpy(cp, "(This may be because your CAPS LOCK key is down.)");
                        err_context = gbuf;
                    }

                    if ((lsh_errno =
                         Leash_int_changepwd(principal, oldpassword,
                                         newpassword, 0, 1))
                        == 0){
                        CloseMe(TRUE);
                    }
                    else {
                        // XXX - DoNiftyErrorReport(lsh_errno, "Error while changing password.");
                        NEXTSTATE(next_state);
                        return TRUE;

                    }
		}
                break;
	    }
            /* increment state, but send the old state as a
               parameter */
            SendMessage(hDialog, WM_COMMAND, ID_NEXTSTATE, state + 1);
	}
	break;
        case IDCANCEL:
            CloseMe(FALSE);
            break;
        case ID_RESTART:
	{
            int i;

            for (i = ID_OLDPASSWORD; i <= ids[lpdi->dlgstatemax]; i++)
                SetDlgItemText(hDialog, i, "");
            SendMessage(hDialog, WM_COMMAND, ID_NEXTSTATE,
                        STATE_PRINCIPAL);
	}
	break;
        }
        break;

    case WM_MOVE:
        if (state != STATE_CLOSED)
#ifdef _WIN32
#define LONG2POINT(l,pt) ((pt).x=(SHORT)LOWORD(l),  \
			  (pt).y=(SHORT)HIWORD(l))
            LONG2POINT(lParam,Position);
#else
        Position = MAKEPOINT(lParam);
#endif
        break;
    }
    return FALSE;
}


#define KRB_FILE                "KRB.CON"
#define KRBREALM_FILE           "KRBREALM.CON"
#define KRB5_FILE               "KRB5.INI"

BOOL
GetProfileFile(
    LPSTR confname,
    UINT szConfname
    )
{
    char **configFile = NULL;
    if (hKrb5 &&
         pkrb5_get_default_config_files(&configFile))
    {
        GetWindowsDirectory(confname,szConfname);
        confname[szConfname-1] = '\0';
        strncat(confname, "\\",sizeof(confname)-strlen(confname));
        confname[szConfname-1] = '\0';
        strncat(confname, KRB5_FILE,sizeof(confname)-strlen(confname));
        confname[szConfname-1] = '\0';
        return FALSE;
    }

    *confname = 0;

    if (hKrb5 && configFile)
    {
        strncpy(confname, *configFile, szConfname);
        pkrb5_free_config_files(configFile);
    }

    if (!*confname)
    {
        GetWindowsDirectory(confname,szConfname);
        confname[szConfname-1] = '\0';
        strncat(confname, "\\",sizeof(confname)-strlen(confname));
        confname[szConfname-1] = '\0';
        strncat(confname, KRB5_FILE,sizeof(confname)-strlen(confname));
        confname[szConfname-1] = '\0';
    }

    return FALSE;
}

int
readstring(FILE * file, char * buf, int len)
{
	int  c,i;
	memset(buf, '\0', sizeof(buf));
	for (i=0, c=fgetc(file); c != EOF ; c=fgetc(file), i++)
	{
		if (i < sizeof(buf)) {
			if (c == '\n') {
				buf[i] = '\0';
				return i;
			} else {
				buf[i] = c;
			}
		} else {
			if (c == '\n') {
				buf[len-1] = '\0';
				return(i);
			}
		}
	}
	if (c == EOF) {
		if (i > 0 && i < len) {
			buf[i] = '\0';
			return(i);
		} else {
			buf[len-1] = '\0';
			return(-1);
		}
	}
    return(-1);
}

typedef struct _slider_info {
	int slider_id;
	int text_id;
	int min;
	int max;
	int increment;
	struct _slider_info * next;
} slider_info;
static slider_info * sliders = NULL;

static slider_info *
FreeSlider(slider_info * s)
{
	slider_info * n = NULL;

	if (s) {
		n = s->next;
		free(s);
	}
	return n;
}

static void
CleanupSliders(void)
{
	while(sliders)
		sliders = FreeSlider(sliders);
}


static unsigned short
NewSliderValue(HWND hDialog, int id)
{
	int value = 0;
	slider_info * s = sliders;
	while(s) {
		if (s->slider_id == id) {
			int pos = CSendDlgItemMessage( hDialog, id,
										 TBM_GETPOS,
										 (WPARAM) 0, (LPARAM) 0);
			value = s->min + (pos * s->increment);
			break;
		}
		s = s->next;
	}
	return(value);
}

static const char *
NewSliderString(int id, int pos)
{
	static char buf[64]="";
	char * p = buf;
	int value = 0;
	int must_hours = 0;
	slider_info * s = sliders;
	while(s) {
		if (s->slider_id == id) {
			value = s->min + pos * s->increment;
			*p = 0;
			if (value >= 60 * 24) {
				sprintf(p,"%d day(s) ",value / (60 * 24));
				value %= (60 * 24);
				p += strlen(p);
				must_hours = 1;
			}
			if (must_hours || value >= 60) {
				sprintf(p,"%d hour(s) ",value / 60);
				value %= 60;
				p += strlen(p);
			}
			sprintf(p,"%d minute(s) ",value);
			break;
		}
		s = s->next;
	}
	return(buf);
}

static void
SetupSlider( HWND hDialog,
			 int sliderID,
			 int textFieldID,
			 int minimum,
			 int maximum,
			 int value)
{
    int min = minimum;
    int max = maximum;
    int increment = 0;
    int range;
	int roundedMinimum;
	int roundedMaximum;
	int roundedValue;
	slider_info * new_info;

    if (max < min) {
        // swap values
        int temp = max;
        max = min;
        min = temp;
    }
	range = max - min;

    if (range < 5*60)             { increment = 1;       //  1 s if under   5 m
    } else if (range < 30*60)     { increment = 5;       //  5 s if under  30 m
    } else if (range < 60*60)     { increment = 15;      // 15 s if under   1 h
    } else if (range < 2*60*60)   { increment = 30;      // 30 s if under   2 h
    } else if (range < 5*60*60)   { increment = 60;      //  1 m if under   5 h
    } else if (range < 50*60*60)  { increment = 5*60;    //  5 m if under  50 h
    } else if (range < 200*60*60) { increment = 15*60;   // 15 m if under 200 h
    } else if (range < 500*60*60) { increment = 30*60;   // 30 m if under 500 h
    } else                        { increment = 60*60; } //  1 h otherwise

    roundedMinimum = (min / increment) * increment;
    if (roundedMinimum > min) { roundedMinimum -= increment; }
    if (roundedMinimum <= 0)  { roundedMinimum += increment; } // make positive

    roundedMaximum = (max / increment) * increment;
    if (roundedMaximum < max) { roundedMaximum += increment; }

    roundedValue = (value / increment) * increment;
    if (roundedValue < roundedMinimum) { roundedValue = roundedMinimum; }
    if (roundedValue > roundedMaximum) { roundedValue = roundedMaximum; }

    if (roundedMinimum == roundedMaximum) {
        // [textField setTextColor: [NSColor grayColor]];
		EnableWindow(GetDlgItem(hDialog,sliderID),FALSE);
    } else {
        // [textField setTextColor: [NSColor blackColor]];
		EnableWindow(GetDlgItem(hDialog,sliderID),TRUE);
    }

	CSendDlgItemMessage( hDialog, sliderID,
						 TBM_SETRANGEMIN,
						 (WPARAM) FALSE,
						 (LPARAM) 0 );
	CSendDlgItemMessage( hDialog, sliderID,
						 TBM_SETRANGEMAX,
						 (WPARAM) FALSE,
						 (LPARAM) (roundedMaximum - roundedMinimum) / increment );
	CSendDlgItemMessage( hDialog, sliderID,
						 TBM_SETPOS,
						 (WPARAM) TRUE,
						 (LPARAM) (roundedValue - roundedMinimum) / increment);

	new_info = (slider_info *) malloc(sizeof(slider_info));
	new_info->slider_id = sliderID;
	new_info->text_id = textFieldID;
	new_info->min = roundedMinimum;
	new_info->max = roundedMaximum;
	new_info->increment = increment;
	new_info->next = sliders;
	sliders = new_info;

	SetWindowText(GetDlgItem(hDialog, textFieldID),
				   NewSliderString(sliderID,(roundedValue - roundedMinimum) / increment));
}


static void
AdjustOptions(HWND hDialog, int show, int hideDiff)
{
    RECT rect;
    RECT dlgRect;
    HWND hwnd;
    int diff;

    Leash_set_hide_kinit_options(!show);

    ShowWindow(GetDlgItem(hDialog,IDC_STATIC_LIFETIME),show);
    ShowWindow(GetDlgItem(hDialog,IDC_STATIC_LIFETIME_VALUE),show);
    ShowWindow(GetDlgItem(hDialog,IDC_SLIDER_LIFETIME),show);
    ShowWindow(GetDlgItem(hDialog,IDC_SLIDER_RENEWLIFE),show);
    ShowWindow(GetDlgItem(hDialog,IDC_STATIC_RENEW),show);
    ShowWindow(GetDlgItem(hDialog,IDC_STATIC_RENEW_TILL_VALUE),show);
    ShowWindow(GetDlgItem(hDialog,IDC_CHECK_FORWARDABLE),show);
    ShowWindow(GetDlgItem(hDialog,IDC_CHECK_NOADDRESS),show);
    ShowWindow(GetDlgItem(hDialog,IDC_CHECK_RENEWABLE),show);
    ShowWindow(GetDlgItem(hDialog,IDC_STATIC_KRB5),show);
    ShowWindow(GetDlgItem(hDialog,IDC_BUTTON_CLEAR_HISTORY),show);

    GetWindowRect( hDialog, &dlgRect );
    diff = dlgRect.top + GetSystemMetrics(SM_CYCAPTION)
         + GetSystemMetrics(SM_CYDLGFRAME) + (show ? -1 : 1) * hideDiff;

    hwnd = GetDlgItem(hDialog,IDOK);
    GetWindowRect(hwnd,&rect);
    SetWindowPos(hwnd,0,rect.left-dlgRect.left-GetSystemMetrics(SM_CXDLGFRAME),rect.top-diff,0,0,SWP_NOZORDER|SWP_NOSIZE);
    hwnd = GetDlgItem(hDialog,IDCANCEL);
    GetWindowRect(hwnd,&rect);
    SetWindowPos(hwnd,0,rect.left-dlgRect.left-GetSystemMetrics(SM_CXDLGFRAME),rect.top-diff,0,0,SWP_NOZORDER|SWP_NOSIZE);
    hwnd = GetDlgItem(hDialog,IDC_BUTTON_OPTIONS);
    GetWindowRect(hwnd,&rect);
    SetWindowPos(hwnd,0,rect.left-dlgRect.left-GetSystemMetrics(SM_CXDLGFRAME),rect.top-diff,0,0,SWP_NOZORDER|SWP_NOSIZE);
    hwnd = GetDlgItem(hDialog,IDC_STATIC_VERSION);
    GetWindowRect(hwnd,&rect);
    SetWindowPos(hwnd,0,rect.left-dlgRect.left-GetSystemMetrics(SM_CXDLGFRAME),rect.top-diff,0,0,SWP_NOZORDER|SWP_NOSIZE);
    hwnd = GetDlgItem(hDialog,IDC_STATIC_COPYRIGHT);
    GetWindowRect(hwnd,&rect);
    SetWindowPos(hwnd,0,rect.left-dlgRect.left-GetSystemMetrics(SM_CXDLGFRAME),rect.top-diff,0,0,SWP_NOZORDER|SWP_NOSIZE);
    SetWindowPos(hDialog,0,0,0,
                 dlgRect.right-dlgRect.left,
                 dlgRect.bottom-dlgRect.top+(show ? 1 : - 1) * hideDiff,
                 SWP_NOZORDER|SWP_NOMOVE);

    CSetDlgItemText(hDialog, IDC_BUTTON_OPTIONS,
                    show ? "Hide Advanced" : "Show Advanced");

}

/* Callback function for the Authentication Dialog box that initializes and
   renews tickets. */

INT_PTR
CALLBACK
AuthenticateProc(
    HWND hDialog,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    static POINT Position = { -1, -1 };
    static char principal[256]="";
    static char password[256]="";
    static int  lifetime=0;
    static int  renew_till=0;
    static int  forwardable=0;
    static int  noaddresses=0;
    static int  proxiable=0;
    static int  publicip=0;
    static LPLSH_DLGINFO_EX lpdi;
    static HWND hDlg=0;
    static HWND hSliderLifetime=0;
    static HWND hSliderRenew=0;
    static RECT dlgRect;
    static int  hideDiff = 0;
    static void *pAutoComplete = 0;
    long realm_count = 0;
    int disable_noaddresses = 0;
    HWND hEditCtrl=0;
    HWND hFocusCtrl=0;
    BOOL bReadOnlyPrinc=0;

    switch (message) {

    case WM_INITDIALOG:
	hDlg = hDialog;

        hEditCtrl = GetDlgItem(hDialog, IDC_EDIT_PRINCIPAL);
        if (hEditCtrl)
            pAutoComplete = Leash_pec_create(hEditCtrl);
	hSliderLifetime = GetDlgItem(hDialog, IDC_STATIC_LIFETIME_VALUE);
	hSliderRenew = GetDlgItem(hDialog, IDC_STATIC_RENEW_TILL_VALUE);

        *( (LPLSH_DLGINFO_EX far *)(&lpdi) ) = (LPLSH_DLGINFO_EX)(LPSTR)lParam;

	if ((lpdi->size != LSH_DLGINFO_EX_V1_SZ &&
	     lpdi->size != LSH_DLGINFO_EX_V2_SZ &&
	      lpdi->size < LSH_DLGINFO_EX_V3_SZ) ||
	     (lpdi->dlgtype & DLGTYPE_MASK) != DLGTYPE_PASSWD) {

	    MessageBox(hDialog, "An incorrect initialization data structure was provided.",
			"AuthenticateProc()",
			MB_OK | MB_ICONSTOP);
	    return FALSE;
	}
        bReadOnlyPrinc = (lpdi->dlgtype & DLGFLAG_READONLYPRINC) ?
                         TRUE : FALSE;

        if ( lpdi->size >= LSH_DLGINFO_EX_V2_SZ ) {
            lpdi->out.username[0] = 0;
            lpdi->out.realm[0] = 0;
        }
        if ( lpdi->size >= LSH_DLGINFO_EX_V3_SZ ) {
            lpdi->out.ccache[0] = 0;
        }

        if ( lpdi->size >= LSH_DLGINFO_EX_V3_SZ )
	    SetWindowText(hDialog, lpdi->in.title);
	else
	    SetWindowText(hDialog, lpdi->title);

        SetProp(hDialog, "HANDLES_HELP", (HANDLE)1);
	if (lpdi->use_defaults) {
	    lifetime = Leash_get_default_lifetime();
	    if (lifetime <= 0)
		lifetime = 600; /* 10 hours */
	    if (Leash_get_default_renewable()) {
                renew_till = Leash_get_default_renew_till();
                if (renew_till < 0)
                    renew_till = 10800; /* 7 days */
            } else
                renew_till = 0;
	    forwardable = Leash_get_default_forwardable();
	    if (forwardable < 0)
		forwardable = 0;
	    noaddresses = Leash_get_default_noaddresses();
	    if (noaddresses < 0)
		noaddresses = 0;
	    proxiable = Leash_get_default_proxiable();
	    if (proxiable < 0)
		proxiable = 0;
	    publicip = Leash_get_default_publicip();
	    if (publicip < 0)
		publicip = 0;
	} else {
	    forwardable = lpdi->forwardable;
	    noaddresses = lpdi->noaddresses;
	    lifetime = lpdi->lifetime;
	    renew_till = lpdi->renew_till;
	    proxiable = lpdi->proxiable;
	    publicip = lpdi->publicip;
	}
        if (lpdi->username && (strlen(lpdi->username) > 0) &&
            lpdi->realm && (strlen(lpdi->realm) > 0)) {
            sprintf_s(principal, sizeof(principal), "%s@%s", lpdi->username,
                      lpdi->realm);
        } else {
            principal[0] = 0;
        }
        Edit_SetReadOnly(hEditCtrl, bReadOnlyPrinc);
        CSetDlgItemText(hDialog, IDC_EDIT_PRINCIPAL, principal);
        CSetDlgItemText(hDialog, IDC_EDIT_PASSWORD, "");

	/* Set Lifetime Slider
	*   min value = 5
	*   max value = 1440
	*   current value
	*/

	SetupSlider( hDialog,
		     IDC_SLIDER_LIFETIME,
		     IDC_STATIC_LIFETIME_VALUE,
		     Leash_get_default_life_min(),
		     Leash_get_default_life_max(),
		     lifetime );

        CheckDlgButton(hDialog, IDC_CHECK_REMEMBER_PRINCIPAL, TRUE);
	/* Set Forwardable checkbox */
	CheckDlgButton(hDialog, IDC_CHECK_FORWARDABLE, forwardable);
	/* Set NoAddress checkbox */
	CheckDlgButton(hDialog, IDC_CHECK_NOADDRESS, noaddresses);
        if ( disable_noaddresses )
            EnableWindow(GetDlgItem(hDialog,IDC_CHECK_NOADDRESS),FALSE);
	/* Set Renewable checkbox */
	CheckDlgButton(hDialog, IDC_CHECK_RENEWABLE, renew_till);
	/* if not renewable, disable Renew Till slider */
	/* if renewable, set Renew Till slider
	*     min value
	*     max value
	*     current value
	*/
	SetupSlider( hDialog,
		     IDC_SLIDER_RENEWLIFE,
		     IDC_STATIC_RENEW_TILL_VALUE,
		     Leash_get_default_renew_min(),
		     Leash_get_default_renew_max(),
		     renew_till);
	if (renew_till) {
	    EnableWindow(GetDlgItem(hDialog,IDC_SLIDER_RENEWLIFE),TRUE);
	} else {
	    EnableWindow(GetDlgItem(hDialog,IDC_SLIDER_RENEWLIFE),FALSE);
	}

        // Compute sizes of items necessary to show/hide the advanced options
        GetWindowRect( hDialog, &dlgRect );
        {
            RECT okRect, staticRect;
            GetWindowRect(GetDlgItem(hDialog,IDC_STATIC_LIFETIME),&staticRect);
            GetWindowRect(GetDlgItem(hDialog,IDOK),&okRect);
            hideDiff = okRect.top - staticRect.top;
        }

        if ( hKrb5 ) {
            if (Leash_get_hide_kinit_options())
                AdjustOptions(hDialog,0,hideDiff);
        } else {
            AdjustOptions(hDialog,0,hideDiff);
            EnableWindow(GetDlgItem(hDialog,IDC_BUTTON_OPTIONS),FALSE);
            ShowWindow(GetDlgItem(hDialog,IDC_BUTTON_OPTIONS),SW_HIDE);
        }

        /* setup text of stuff. */

        if (Position.x > 0 && Position.y > 0 &&
            Position.x < GetSystemMetrics(SM_CXSCREEN) &&
            Position.y < GetSystemMetrics(SM_CYSCREEN))
            SetWindowPos(hDialog, HWND_TOP, Position.x, Position.y, 0, 0, SWP_NOSIZE);
        else /* Center the window on the desktop */
            SetWindowPos(hDialog, HWND_TOP,
                         (GetSystemMetrics(SM_CXSCREEN) - dlgRect.right + dlgRect.left)/2,
                         (GetSystemMetrics(SM_CYSCREEN) - dlgRect.bottom + dlgRect.top)/2,
                         0, 0,
                         SWP_NOSIZE);

        /* Take keyboard focus */
        SetActiveWindow(hDialog);
        SetForegroundWindow(hDialog);
        /* put focus on password if princ is read-only */
        hFocusCtrl = (bReadOnlyPrinc || principal[0] != '\0') ?
            GetDlgItem(hDialog, IDC_EDIT_PASSWORD) : hEditCtrl;
        if (((HWND)wParam) != hFocusCtrl) {
            SetFocus(hFocusCtrl);
        }
        break;

	case WM_HSCROLL:
	switch (LOWORD(wParam)) {
	case TB_THUMBTRACK:
	case TB_THUMBPOSITION:
	    {
		long pos = HIWORD(wParam); // the position of the slider
		int  ctrlID = GetDlgCtrlID((HWND)lParam);

		if (ctrlID == IDC_SLIDER_RENEWLIFE) {
		    SetWindowText(GetDlgItem(hDialog, IDC_STATIC_RENEW_TILL_VALUE),
				   NewSliderString(IDC_SLIDER_RENEWLIFE,pos));
		}
		if (ctrlID == IDC_SLIDER_LIFETIME) {
		    SetWindowText(GetDlgItem(hDialog, IDC_STATIC_LIFETIME_VALUE),
				   NewSliderString(IDC_SLIDER_LIFETIME,pos));
		}
	    }
	    break;
        case TB_BOTTOM:
        case TB_TOP:
        case TB_ENDTRACK:
        case TB_LINEDOWN:
        case TB_LINEUP:
        case TB_PAGEDOWN:
        case TB_PAGEUP:
	default:
	    {
		int  ctrlID = GetDlgCtrlID((HWND)lParam);
		long pos = SendMessage(GetDlgItem(hDialog,ctrlID), TBM_GETPOS, 0, 0); // the position of the slider

		if (ctrlID == IDC_SLIDER_RENEWLIFE) {
		    SetWindowText(GetDlgItem(hDialog, IDC_STATIC_RENEW_TILL_VALUE),
				   NewSliderString(IDC_SLIDER_RENEWLIFE,pos));
		}
		if (ctrlID == IDC_SLIDER_LIFETIME) {
		    SetWindowText(GetDlgItem(hDialog, IDC_STATIC_LIFETIME_VALUE),
				   NewSliderString(IDC_SLIDER_LIFETIME,pos));
		}
	    }
	}
        break;

    case WM_COMMAND:
        switch (wParam) {
	case IDC_BUTTON_OPTIONS:
	    {
                AdjustOptions(hDialog,Leash_get_hide_kinit_options(),hideDiff);
                GetWindowRect(hDialog,&dlgRect);
                if ( dlgRect.bottom > GetSystemMetrics(SM_CYSCREEN))
                    SetWindowPos( hDialog,0,
                                  dlgRect.left,
                                  GetSystemMetrics(SM_CYSCREEN) - dlgRect.bottom + dlgRect.top,
                                  0,0,
                                  SWP_NOZORDER|SWP_NOSIZE);

	    }
	    break;
    case IDC_BUTTON_CLEAR_HISTORY:
        Leash_pec_clear_history(pAutoComplete);
        break;
	case IDC_CHECK_RENEWABLE:
	    {
		if (IsDlgButtonChecked(hDialog, IDC_CHECK_RENEWABLE)) {
		    EnableWindow(hSliderRenew,TRUE);
		} else {
		    EnableWindow(hSliderRenew,FALSE);
		}
	    }
	    break;
        case ID_HELP:
	    {
		WinHelp(GetWindow(hDialog,GW_OWNER), KRB_HelpFile, HELP_CONTEXT,
			 ID_INITTICKETS);
	    }
	    break;
        case ID_CLOSEME:
	    {
		CleanupSliders();
		memset(password,0,sizeof(password));
		RemoveProp(hDialog, "HANDLES_HELP");
        if (pAutoComplete) {
            Leash_pec_destroy(pAutoComplete);
            pAutoComplete = NULL;
        }
		EndDialog(hDialog, (int)lParam);
                return TRUE;
	    }
	    break;
        case IDOK:
	    {
		DWORD value = 0;

		CGetDlgItemText(hDialog, IDC_EDIT_PRINCIPAL, principal, sizeof(principal));
		CGetDlgItemText(hDialog, IDC_EDIT_PASSWORD, password, sizeof(password));

		if (!principal[0]) {
		    MessageBox(hDialog,
                       "You are not allowed to enter a blank principal.",
                       "Invalid Principal",
                       MB_OK | MB_ICONSTOP);
		    return TRUE;
		}
        // @TODO: parse realm portion and auto-uppercase
/*
		if (Leash_get_default_uppercaserealm())
		{
		    // found
		    strupr(realm);
		}
*/

		if (!password[0])
		{
		    MessageBox(hDialog,
                                "You are not allowed to enter a blank password.",
				"Invalid Password",
				MB_OK | MB_ICONSTOP);
		    return TRUE;
		}

		lifetime = NewSliderValue(hDialog, IDC_SLIDER_LIFETIME);

		forwardable = proxiable =
                    IsDlgButtonChecked(hDialog, IDC_CHECK_FORWARDABLE);
		noaddresses = IsDlgButtonChecked(hDialog, IDC_CHECK_NOADDRESS);
		if (IsDlgButtonChecked(hDialog, IDC_CHECK_RENEWABLE)) {
		    renew_till = NewSliderValue(hDialog, IDC_SLIDER_RENEWLIFE);
		} else {
		    renew_till= 0;
		}

		lsh_errno = Leash_int_kinit_ex( 0,
						hDialog,
						principal, password, lifetime,
						forwardable,
						proxiable,
						renew_till,
						noaddresses,
						publicip,
						1
						);
		if (lsh_errno != 0)
		{
#ifdef COMMENT
		    char gbuf[256];
		    int capslock;
		    char *cp;
#endif
		    err_context = "";
		    switch(lsh_errno)
		    {
		    case LSH_INVPRINCIPAL:
		    case LSH_INVINSTANCE:
                        CSendDlgItemMessage(hDialog, IDC_EDIT_PRINCIPAL, EM_SETSEL, 0, 256);
                        SetFocus(GetDlgItem(hDialog,IDC_EDIT_PRINCIPAL));
                        break;
		    case LSH_INVREALM:
                        CSendDlgItemMessage(hDialog, IDC_COMBO_REALM, EM_SETSEL, 0, 256);
                        SetFocus(GetDlgItem(hDialog,IDC_COMBO_REALM));
			break;
                    default:
                        CSendDlgItemMessage(hDialog, IDC_EDIT_PASSWORD, EM_SETSEL, 0, 256);
                        SetFocus(GetDlgItem(hDialog,IDC_EDIT_PASSWORD));
			return(TRUE);
		    }
#ifdef COMMENT
		    capslock = lsh_getkeystate(VK_CAPITAL);
		    /* low-order bit means caps lock is
		    toggled; if so, warn user since there's
		    been an error. */
		    if (capslock & 1)
		    {
			lstrcpy((LPSTR)gbuf, (LPSTR)err_context);
			cp = gbuf + lstrlen((LPSTR)gbuf);
			if (cp != gbuf)
			    *cp++ = ' ';
			lstrcpy(cp, "(This may be because your CAPS LOCK key is down.)");
			err_context = gbuf;
		    }

		    // XXX DoNiftyErrorReport(lsh_errno, ISCHPASSWD ? ""
		    // XXX : "Ticket initialization failed.");
#endif /* COMMENT */
		    return TRUE;
		}

                if ( Leash_get_default_preserve_kinit_settings() )
                {
                    Leash_set_default_lifetime(lifetime);
                    if ( renew_till > 0 ) {
                        Leash_set_default_renew_till(renew_till);
                        Leash_set_default_renewable(1);
                    } else {
                        Leash_set_default_renewable(0);
                    }
                    Leash_set_default_forwardable(forwardable);
                    Leash_set_default_noaddresses(noaddresses);
                }
/* @TODO: out username/realm
                if ( lpdi->size >= LSH_DLGINFO_EX_V2_SZ ) {
                    strncpy(lpdi->out.username, username, LEASH_USERNAME_SZ);
                    lpdi->out.username[LEASH_USERNAME_SZ-1] = 0;
                    strncpy(lpdi->out.realm, realm, LEASH_REALM_SZ);
                    lpdi->out.realm[LEASH_REALM_SZ-1] = 0;
                }
*/
                if (IsDlgButtonChecked(hDialog, IDC_CHECK_REMEMBER_PRINCIPAL))
                    Leash_pec_add_principal(principal);

                CloseMe(TRUE); /* success */
                return FALSE;
	    }
	    break;
        case IDCANCEL:
            CloseMe(FALSE);
            break;
        }
        break;

    case WM_MOVE:
#ifdef _WIN32
#define LONG2POINT(l,pt) ((pt).x=(SHORT)LOWORD(l),  \
			 (pt).y=(SHORT)HIWORD(l))
    LONG2POINT(lParam,Position);
#else
	Position = MAKEPOINT(lParam);
#endif
        break;
    }
    return FALSE;
}

/* Callback function for the Change Password Dialog box */

INT_PTR
CALLBACK
NewPasswordProc(
    HWND hDialog,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    static POINT Position = { -1, -1 };
    static char password[256]="";
    static char password2[256]="";
    static char password3[256]="";
    static LPLSH_DLGINFO_EX lpdi;
    static HWND hDlg=0;
    static void *pAutoComplete = NULL;
    char principal[256];
    long realm_count = 0;
    HWND hEditCtrl = NULL;

    switch (message) {

    case WM_INITDIALOG:
	hDlg = hDialog;

        *( (LPLSH_DLGINFO_EX far *)(&lpdi) ) = (LPLSH_DLGINFO_EX)(LPSTR)lParam;

	if ((lpdi->size < LSH_DLGINFO_EX_V3_SZ &&
	      lpdi->size != LSH_DLGINFO_EX_V1_SZ &&
	      lpdi->size != LSH_DLGINFO_EX_V2_SZ) ||
	     lpdi->dlgtype != DLGTYPE_CHPASSWD) {

	    MessageBox(hDialog, "An incorrect initialization data structure was provided.",
			"PasswordProc()",
			MB_OK | MB_ICONSTOP);
	    return FALSE;
	}

        if ( lpdi->size >= LSH_DLGINFO_EX_V2_SZ ) {
            lpdi->out.username[0] = 0;
            lpdi->out.realm[0] = 0;
        }
        if ( lpdi->size >= LSH_DLGINFO_EX_V3_SZ ) {
            lpdi->out.ccache[0] = 0;
        }

        if ( lpdi->size >= LSH_DLGINFO_EX_V3_SZ )
	    SetWindowText(hDialog, lpdi->in.title);
	else
	    SetWindowText(hDialog, lpdi->title);

        SetProp(hDialog, "HANDLES_HELP", (HANDLE)1);

        if (lpdi->username != NULL && (strlen(lpdi->username) > 0) &&
            lpdi->realm != NULL && (strlen(lpdi->realm) > 0)) {
            sprintf_s(principal,
                      sizeof(principal), "%s@%s", lpdi->username, lpdi->realm);
        } else {
            principal[0] = 0;
        }

        CSetDlgItemText(hDialog, IDC_EDIT_PRINCIPAL, principal);
        CSetDlgItemText(hDialog, IDC_EDIT_PASSWORD, "");
        CSetDlgItemText(hDialog, IDC_EDIT_PASSWORD2, "");
        CSetDlgItemText(hDialog, IDC_EDIT_PASSWORD3, "");

        hEditCtrl = GetDlgItem(hDialog, IDC_EDIT_PRINCIPAL);
        if (hEditCtrl)
            pAutoComplete = Leash_pec_create(hEditCtrl);

        /* setup text of stuff. */

        if (Position.x > 0 && Position.y > 0 &&
            Position.x < GetSystemMetrics(SM_CXSCREEN) &&
            Position.y < GetSystemMetrics(SM_CYSCREEN))
            SetWindowPos(hDialog, 0, Position.x, Position.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        else { /* Center the window on the desktop */
            RECT dlgRect;
            GetWindowRect( hDialog, &dlgRect );
            SetWindowPos(hDialog, 0,
                         (GetSystemMetrics(SM_CXSCREEN) - dlgRect.right + dlgRect.left)/2,
                         (GetSystemMetrics(SM_CYSCREEN) - dlgRect.bottom + dlgRect.top)/2,
                         0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        /* set window pos to last saved window pos */
        break;

    case WM_COMMAND:
        switch (wParam) {
        case ID_HELP:
	    {
		WinHelp(GetWindow(hDialog,GW_OWNER), KRB_HelpFile, HELP_CONTEXT,
			 ID_INITTICKETS);
	    }
	    break;
        case ID_CLOSEME:
	    {
		CleanupSliders();
		memset(password,0,sizeof(password));
		memset(password2,0,sizeof(password2));
		memset(password3,0,sizeof(password3));
		RemoveProp(hDialog, "HANDLES_HELP");
		EndDialog(hDialog, (int)lParam);
                if (pAutoComplete != NULL) {
                    Leash_pec_destroy(pAutoComplete);
                    pAutoComplete = NULL;
                }
                return TRUE;
	    }
	    break;
        case IDOK:
	    {
		DWORD value = 0;
		int i = 0;
                int bit8 = 0;

		CGetDlgItemText(hDialog, IDC_EDIT_PRINCIPAL, principal, sizeof(principal));
		CGetDlgItemText(hDialog, IDC_EDIT_PASSWORD, password, sizeof(password));
		CGetDlgItemText(hDialog, IDC_EDIT_PASSWORD2, password2, sizeof(password2));
		CGetDlgItemText(hDialog, IDC_EDIT_PASSWORD3, password3, sizeof(password3));

		if (!principal[0])
		{
		    MessageBox(hDialog, "You are not allowed to enter a "
				"blank username.",
				"Invalid Principal",
				MB_OK | MB_ICONSTOP);
		    return TRUE;
		}

		if (!password[0] || !password2[0] || !password3[0])
		{
		    MessageBox(hDialog, "You are not allowed to enter a "
				"blank password.",
				"Invalid Password",
				MB_OK | MB_ICONSTOP);
		    return TRUE;
		}

		for( i = 0; i < 255; i++ ){
                    if( password2[i] == '\0' ){
                        if ( bit8 ) {
                            MessageBox(hDialog,
                                        "Passwords should not contain non-ASCII characters.",
                                        "Internationalization Warning",
                                        MB_OK | MB_ICONINFORMATION);
                        }
                        i = 255;
                        break;
                    } else if( !isprint(password2[i]) ){
                        memset(password2, '\0', sizeof(password2));
                        memset(password3, '\0', sizeof(password3));
                        /* I claim these passwords in the name of planet '\0'... */
                        MessageBox(hDialog,
                                   "Passwords may not contain non-printable characters.",
                                    "Invalid Password",
                                    MB_OK | MB_ICONSTOP);
                        return TRUE;
                    } else if ( password2[i] > 127 )
                        bit8 = 1;
		}

		if (lstrcmp(password2, password3))
		{
                    MessageBox(hDialog,
                                "The new password was not entered the same way twice.",
                                "Password validation error",
                                MB_OK | MB_ICONSTOP);
                    return TRUE;
		}

                lsh_errno = Leash_int_changepwd(principal, password, password2, 0, 1);
		if (lsh_errno != 0)
		{
#ifdef COMMENT
		    char gbuf[256];
		    int capslock;
		    char *cp;
#endif /* COMMENT */

		    err_context = "";
		    switch(lsh_errno)
		    {
		    case LSH_INVPRINCIPAL:
		    case LSH_INVINSTANCE:
		    case LSH_INVREALM:
			break;
		    default:
			return(TRUE);
		    }
#ifdef COMMENT
		    capslock = lsh_getkeystate(VK_CAPITAL);
		    /* low-order bit means caps lock is
		    toggled; if so, warn user since there's
		    been an error. */
		    if (capslock & 1)
		    {
			lstrcpy((LPSTR)gbuf, (LPSTR)err_context);
			cp = gbuf + lstrlen((LPSTR)gbuf);
			if (cp != gbuf)
			    *cp++ = ' ';
			lstrcpy(cp, "(This may be because your CAPS LOCK key is down.)");
			err_context = gbuf;
		    }

		    // XXX   DoNiftyErrorReport(lsh_errno, ISCHPASSWD ? ""
		    // XXX   : "Ticket initialization failed.");
#endif /* COMMENT */
                    return TRUE;
		}
                Leash_pec_add_principal(principal);
                MessageBox(NULL, "Password successfully changed.",
                           "Password change", MB_OK);
                CloseMe(TRUE); /* success */
	    }
	    break;
        case IDCANCEL:
            CloseMe(FALSE);
            break;
        }
        break;

    case WM_MOVE:
#ifdef _WIN32
#define LONG2POINT(l,pt) ((pt).x=(SHORT)LOWORD(l),  \
		   (pt).y=(SHORT)HIWORD(l))
    LONG2POINT(lParam,Position);
#else
	Position = MAKEPOINT(lParam);
#endif
        break;
    }
    return FALSE;
}
