#include <windows.h>
#include <winver.h>

#if 0
//#ifdef USE_VS
#include <vs.h>

#define ININAME	"leash.ini"

int VScheckVersion(HWND hWnd, HANDLE hThisInstance)
{
    VS_Request		vrequest;
    VS_Status		status;
    BOOL		ok_to_continue;
    HCURSOR		hcursor;
    char		szFilename[255];
    char		szVerQ[90];
    char		*cp;
    LPSTR		lpAppVersion;
    LPSTR		lpAppName;
    LONG FAR		*lpLangInfo;
    DWORD		hVersionInfoID;
    DWORD		size;
    GLOBALHANDLE	hVersionInfo;
    LPSTR		lpVersionInfo;
    int			dumint;
    int			retval;

    GetModuleFileName(hThisInstance, (LPSTR)szFilename, 255);
    size = GetFileVersionInfoSize((LPSTR) szFilename, &hVersionInfoID);
    hVersionInfo = GlobalAlloc(GHND, size);
    lpVersionInfo = GlobalLock(hVersionInfo);
    retval = GetFileVersionInfo(szFilename, hVersionInfoID, size,
                                lpVersionInfo);
    retval = VerQueryValue(lpVersionInfo, "\\VarFileInfo\\Translation",
                           (LPSTR FAR *)&lpLangInfo, &dumint);
    wsprintf(szVerQ, "\\StringFileInfo\\%04x%04x\\",
             LOWORD(*lpLangInfo), HIWORD(*lpLangInfo));
    cp = szVerQ + lstrlen(szVerQ);
    lstrcpy(cp, "ProductName");
    retval = VerQueryValue(lpVersionInfo, szVerQ, &lpAppName, &dumint);
    lstrcpy(cp, "ProductVersion");

    retval = VerQueryValue(lpVersionInfo, szVerQ, &lpAppVersion, &dumint);
    hcursor = SetCursor(LoadCursor((HINSTANCE)NULL, IDC_WAIT));
    vrequest = VSFormRequest(lpAppName, lpAppVersion, ININAME, NULL, hWnd,
                             V_CHECK_AND_LOG);
    if ((ok_to_continue = (ReqStatus(vrequest) != V_E_CANCEL))
        && v_complain((status = VSProcessRequest(vrequest)), ININAME))
        WinVSReportRequest(vrequest, hWnd, "Version Server Status Report");
    if (ok_to_continue && status == V_REQUIRED)
        ok_to_continue = FALSE;
    VSDestroyRequest(vrequest);
    SetCursor(hcursor);
    GlobalUnlock(hVersionInfo);
    GlobalFree(hVersionInfo);
    return(ok_to_continue);
}
#else
int VScheckVersion(HWND hWnd, HANDLE hThisInstance)
{
    return(1);
}
#endif
