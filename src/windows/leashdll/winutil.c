#include <windows.h>
#include "leash-int.h"

//#include <string.h>

static ATOM sAtom = 0;
static HINSTANCE shInstance = 0;

/* Callback for the MITPasswordControl
This is a replacement for the normal edit control.  It does not show the
annoying password char in the edit box so that the number of chars in the
password are not known.
*/

#define PASSWORDCHAR '#'
#define DLGHT(ht) (HIWORD(GetDialogBaseUnits())*(ht)/8)
#define DLGWD(wd) (LOWORD(GetDialogBaseUnits())*(wd)/4)

static
LRESULT
CALLBACK
MITPasswordEditProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    static SIZE pwdcharsz;
    BOOL pass_the_buck = FALSE;

    if (message > WM_USER && message < 0x7FFF)
        pass_the_buck = TRUE;

    switch(message)
    {
    case WM_GETTEXT:
    case WM_GETTEXTLENGTH:
    case WM_SETTEXT:
        pass_the_buck = TRUE;
        break;
    case WM_PAINT:
    {
        HDC hdc;
        PAINTSTRUCT ps;
        RECT r;

        hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &r);
        Rectangle(hdc, 0, 0, r.right, r.bottom);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_SIZE:
    {
        MoveWindow(GetDlgItem(hWnd, 1), DLGWD(2), DLGHT(2),
		   pwdcharsz.cx / 2, pwdcharsz.cy, TRUE);
    }
    break;
    case WM_LBUTTONDOWN:
    case WM_SETFOCUS:
    {
        SetFocus(GetDlgItem(hWnd, 1));
    }
    break;
    case WM_CREATE:
    {
        HWND heditchild;
        char pwdchar = PASSWORDCHAR;
        HDC hdc;
        /* Create a child window of this control for default processing. */
        hdc = GetDC(hWnd);
        GetTextExtentPoint32(hdc, &pwdchar, 1, &pwdcharsz);
        ReleaseDC(hWnd, hdc);

        heditchild =
            CreateWindow("edit", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL |
                         ES_LEFT | ES_PASSWORD | WS_TABSTOP,
                         0, 0, 0, 0,
                         hWnd,
                         (HMENU)1,
                         ((LPCREATESTRUCT)lParam)->hInstance,
                         NULL);
        SendMessage(heditchild, EM_SETPASSWORDCHAR, PASSWORDCHAR, 0L);
    }
    break;
    }

    if (pass_the_buck)
        return SendMessage(GetDlgItem(hWnd, 1), message, wParam, lParam);
    return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL
Register_MITPasswordEditControl(
    HINSTANCE hInst
    )
{
    if (!sAtom) {
        WNDCLASS wndclass;

        memset(&wndclass, 0, sizeof(WNDCLASS));

        shInstance = hInst;

        wndclass.style = CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc = (WNDPROC)MITPasswordEditProc;
        wndclass.cbClsExtra = sizeof(HWND);
        wndclass.cbWndExtra = 0;
        wndclass.hInstance = shInstance;
        wndclass.hbrBackground = (void *)(COLOR_WINDOW + 1);
        wndclass.lpszClassName = MIT_PWD_DLL_CLASS;
        wndclass.hCursor = LoadCursor((HINSTANCE)NULL, IDC_IBEAM);

        sAtom = RegisterClass(&wndclass);
    }
    return sAtom ? TRUE : FALSE;
}

BOOL
Unregister_MITPasswordEditControl(
    HINSTANCE hInst
    )
{
    BOOL result = TRUE;

    if ((hInst != shInstance) || !sAtom) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    result = UnregisterClass(MIT_PWD_DLL_CLASS, hInst);
    if (result) {
        sAtom = 0;
        shInstance = 0;
    }
    return result;
}
