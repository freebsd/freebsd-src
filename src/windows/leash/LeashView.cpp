//*****************************************************************************
// File:	LeashView.cpp
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright	@1998 Massachusetts Institute of Technology - All rights reserved.
// Description:	CPP file for LeashView.h. Contains variables and functions
//		for the Leash FormView
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 12/02/98	ADL		Original
// 20030508     JEA     Added
//*****************************************************************************

#include "stdafx.h"
#include <afxpriv.h>
#include "Leash.h"
#include "LeashDoc.h"
#include "LeashView.h"
#include "MainFrm.h"
#include "reminder.h"
#include "lglobals.h"
#include "LeashDebugWindow.h"
#include "LeashMessageBox.h"
#include "LeashAboutBox.h"
#include <krb5.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static CHAR THIS_FILE[] = __FILE__;
#endif

#pragma comment(lib, "uxtheme")
/////////////////////////////////////////////////////////////////////////////
// CLeashView

IMPLEMENT_DYNCREATE(CLeashView, CListView)

BEGIN_MESSAGE_MAP(CLeashView, CListView)
	//{{AFX_MSG_MAP(CLeashView)
    ON_MESSAGE(WM_WARNINGPOPUP, OnWarningPopup)
	ON_MESSAGE(WM_GOODBYE, OnGoodbye)
    ON_MESSAGE(WM_TRAYICON, OnTrayIcon)
    ON_NOTIFY(TVN_ITEMEXPANDED, IDC_TREEVIEW, OnItemexpandedTreeview)
	ON_WM_CREATE()
	ON_WM_SHOWWINDOW()
	ON_COMMAND(ID_INIT_TICKET, OnInitTicket)
	ON_COMMAND(ID_RENEW_TICKET, OnRenewTicket)
	ON_COMMAND(ID_DESTROY_TICKET, OnDestroyTicket)
	ON_COMMAND(ID_CHANGE_PASSWORD, OnChangePassword)
	ON_COMMAND(ID_MAKE_DEFAULT, OnMakeDefault)
	ON_COMMAND(ID_UPDATE_DISPLAY, OnUpdateDisplay)
	ON_COMMAND(ID_SYN_TIME, OnSynTime)
	ON_COMMAND(ID_DEBUG_MODE, OnDebugMode)
	ON_COMMAND(ID_LARGE_ICONS, OnLargeIcons)
	ON_COMMAND(ID_TIME_ISSUED, OnTimeIssued)
	ON_COMMAND(ID_VALID_UNTIL, OnValidUntil)
	ON_COMMAND(ID_RENEWABLE_UNTIL, OnRenewableUntil)
	ON_COMMAND(ID_SHOW_TICKET_FLAGS, OnShowTicketFlags)
	ON_COMMAND(ID_ENCRYPTION_TYPE, OnEncryptionType)
	ON_COMMAND(ID_CCACHE_NAME, OnCcacheName)
	ON_UPDATE_COMMAND_UI(ID_TIME_ISSUED, OnUpdateTimeIssued)
	ON_UPDATE_COMMAND_UI(ID_VALID_UNTIL, OnUpdateValidUntil)
	ON_UPDATE_COMMAND_UI(ID_RENEWABLE_UNTIL, OnUpdateRenewableUntil)
	ON_UPDATE_COMMAND_UI(ID_SHOW_TICKET_FLAGS, OnUpdateShowTicketFlags)
	ON_UPDATE_COMMAND_UI(ID_ENCRYPTION_TYPE, OnUpdateEncryptionType)
	ON_UPDATE_COMMAND_UI(ID_CCACHE_NAME, OnUpdateCcacheName)
	ON_COMMAND(ID_UPPERCASE_REALM, OnUppercaseRealm)
	ON_COMMAND(ID_KILL_TIX_ONEXIT, OnKillTixOnExit)
	ON_UPDATE_COMMAND_UI(ID_UPPERCASE_REALM, OnUpdateUppercaseRealm)
	ON_UPDATE_COMMAND_UI(ID_KILL_TIX_ONEXIT, OnUpdateKillTixOnExit)
	ON_WM_DESTROY()
	ON_UPDATE_COMMAND_UI(ID_DESTROY_TICKET, OnUpdateDestroyTicket)
	ON_UPDATE_COMMAND_UI(ID_INIT_TICKET, OnUpdateInitTicket)
	ON_UPDATE_COMMAND_UI(ID_RENEW_TICKET, OnUpdateRenewTicket)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_UPDATE_COMMAND_UI(ID_DEBUG_MODE, OnUpdateDebugMode)
	ON_UPDATE_COMMAND_UI(ID_CFG_FILES, OnUpdateCfgFiles)
    ON_COMMAND(ID_LEASH_RESTORE, OnLeashRestore)
    ON_COMMAND(ID_LEASH_MINIMIZE, OnLeashMinimize)
	ON_COMMAND(ID_LOW_TICKET_ALARM, OnLowTicketAlarm)
	ON_COMMAND(ID_AUTO_RENEW, OnAutoRenew)
	ON_UPDATE_COMMAND_UI(ID_LOW_TICKET_ALARM, OnUpdateLowTicketAlarm)
	ON_UPDATE_COMMAND_UI(ID_AUTO_RENEW, OnUpdateAutoRenew)
	ON_UPDATE_COMMAND_UI(ID_MAKE_DEFAULT, OnUpdateMakeDefault)
	ON_UPDATE_COMMAND_UI(ID_PROPERTIES, OnUpdateProperties)
	ON_COMMAND(ID_HELP_KERBEROS_, OnHelpKerberos)
	ON_COMMAND(ID_HELP_LEASH32, OnHelpLeash32)
	ON_COMMAND(ID_HELP_WHYUSELEASH32, OnHelpWhyuseleash32)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
    ON_WM_SYSCOLORCHANGE()
    ON_MESSAGE(ID_OBTAIN_TGT_WITH_LPARAM, OnObtainTGTWithParam)
    ON_NOTIFY(HDN_ITEMCHANGED, 0, OnItemChanged)
	//}}AFX_MSG_MAP

    ON_NOTIFY_REFLECT(LVN_ITEMCHANGING, &CLeashView::OnLvnItemchanging)
    ON_NOTIFY_REFLECT(LVN_ITEMACTIVATE, &CLeashView::OnLvnItemActivate)
    ON_NOTIFY_REFLECT(LVN_KEYDOWN, &CLeashView::OnLvnKeydown)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CLeashView::OnNMCustomdraw)
END_MESSAGE_MAP()


time_t CLeashView::m_ticketTimeLeft = 0;  // # of seconds left before tickets expire
INT  CLeashView::m_ticketStatusKrb5 = 0; // Defense Condition: are we low on tickets?
INT  CLeashView::m_warningOfTicketTimeLeftKrb5 = 0; // Prevents warning box from coming up repeatively
INT  CLeashView::m_warningOfTicketTimeLeftLockKrb5 = 0;
INT  CLeashView::m_updateDisplayCount;
INT  CLeashView::m_alreadyPlayedDisplayCount;
INT  CLeashView::m_autoRenewTickets = 0;
BOOL CLeashView::m_lowTicketAlarmSound;
INT  CLeashView::m_autoRenewalAttempted = 0;
LONG CLeashView::m_timerMsgNotInProgress = 1;
ViewColumnInfo CLeashView::sm_viewColumns[] =
{
    {"Principal", true, -1, 200},                        // PRINCIPAL
    {"Issued", false, ID_TIME_ISSUED, 100},              // TIME_ISSUED
    {"Renewable Until", false, ID_RENEWABLE_UNTIL, 100}, // RENEWABLE_UNTIL
    {"Valid Until", true, ID_VALID_UNTIL, 100},          // VALID_UNTIL
    {"Encryption Type", false, ID_ENCRYPTION_TYPE, 100}, // ENCRYPTION_TYPE
    {"Flags", false, ID_SHOW_TICKET_FLAGS, 100},         // TICKET_FLAGS
    {"Credential Cache", false, ID_CCACHE_NAME, 105},    // CACHE_NAME
};

static struct TicketFlag {
    unsigned long m_flag;
    const LPTSTR m_description;
} sm_TicketFlags[] =
{
    {TKT_FLG_FORWARDABLE, _T("Forwardable")},
    {TKT_FLG_FORWARDED, _T("Forwarded")},
    {TKT_FLG_PROXIABLE, _T("Proxiable")},
    {TKT_FLG_PROXY, _T("Proxy")},
    {TKT_FLG_RENEWABLE, _T("Renewable")},
};

static void krb5TicketFlagsToString(unsigned long flags, LPTSTR *outStr)
{
    const int numFlags = sizeof(sm_TicketFlags) / sizeof(sm_TicketFlags[0]);
    int strSize = 1;
    LPTSTR str;
    // pass 1: compute size
    for (int i = 0; i < numFlags; i++) {
        if (flags & sm_TicketFlags[i].m_flag) {
            if (strSize > 1)
                strSize += 2;
            strSize += strlen(sm_TicketFlags[i].m_description);
        }
    }
    // allocate
    str = (LPSTR)malloc(strSize);
    if (str != NULL) {
        *str = 0;
        // pass 2: construct string
        for (int i = 0; i < numFlags; i++) {
            if (flags & sm_TicketFlags[i].m_flag) {
                if (str[0])
                    _tcscat_s(str, strSize, _T(", "));
                _tcscat_s(str, strSize, sm_TicketFlags[i].m_description);
            }
        }
    }
    *outStr = str;
}


static HFONT CreateBoldFont(HFONT font)
{
    // @TODO: Should probably enumerate fonts here instead since this
    // does not actually seem to guarantee returning a new font
    // distinguishable from the original.
    LOGFONT fontAttributes = { 0 };
    ::GetObject(font, sizeof(fontAttributes), &fontAttributes);
    fontAttributes.lfWeight = FW_BOLD;
    HFONT boldFont = ::CreateFontIndirect(&fontAttributes);
    return boldFont;
}

static HFONT CreateItalicFont(HFONT font)
{
    LOGFONT fontAttributes = { 0 };
    ::GetObject(font, sizeof(fontAttributes), &fontAttributes);
    fontAttributes.lfItalic = TRUE;
    HFONT italicFont = ::CreateFontIndirect(&fontAttributes);
    return italicFont;
}

static HFONT CreateBoldItalicFont(HFONT font)
{
    LOGFONT fontAttributes = { 0 };
    ::GetObject(font, sizeof(fontAttributes), &fontAttributes);
    fontAttributes.lfWeight = FW_BOLD;
    fontAttributes.lfItalic = TRUE;
    HFONT boldItalicFont = ::CreateFontIndirect(&fontAttributes);
    return boldItalicFont;
}

bool change_icon_size = true;

void TimestampToFileTime(time_t t, LPFILETIME pft)
{
    // Note that LONGLONG is a 64-bit value
    ULONGLONG ll;

    ll = UInt32x32To64((DWORD)t, 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD)ll;
    pft->dwHighDateTime = ll >> 32;
}

// allocate outstr
void TimestampToLocalizedString(time_t t, LPTSTR *outStr)
{
    FILETIME ft, lft;
    SYSTEMTIME st;
    TimestampToFileTime(t, &ft);
    FileTimeToLocalFileTime(&ft, &lft);
    FileTimeToSystemTime(&lft, &st);
    TCHAR timeFormat[80]; // 80 is max required for LOCALE_STIMEFORMAT
    GetLocaleInfo(LOCALE_SYSTEM_DEFAULT,
                  LOCALE_STIMEFORMAT,
                  timeFormat,
                  sizeof(timeFormat) / sizeof(timeFormat[0]));

    int timeSize = GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
                                 TIME_NOSECONDS,
                                 &st,
                                 timeFormat,
                                 NULL,
                                 0);
    // Using dateFormat prevents localization of Month/day order,
    // but there is no other way AFAICT to suppress the year
    TCHAR * dateFormat = "MMM dd'  '";
    int dateSize = GetDateFormat(LOCALE_SYSTEM_DEFAULT,
        0, // flags
        &st,
        dateFormat, // format
        NULL, // date string
        0);

    if (*outStr)
        free(*outStr);

    // Allocate string for combined date and time,
    // but only need one terminating NULL
    LPTSTR str = (LPSTR)malloc((dateSize + timeSize - 1) * sizeof(TCHAR));
    if (!str) {
        // LeashWarn allocation failure
        *outStr = NULL;
        return;
    }
    GetDateFormat(LOCALE_SYSTEM_DEFAULT,
        0, // flags
        &st,
        dateFormat, // format
        &str[0],
        dateSize);

    GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
                    TIME_NOSECONDS,
                    &st,
                    timeFormat,
                    &str[dateSize - 1],
                    timeSize);
    *outStr = str;
}

#define SECONDS_PER_MINUTE (60)
#define SECONDS_PER_HOUR (60 * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY (24 * SECONDS_PER_HOUR)
#define MAX_DURATION_STR 255
// convert time in seconds to string
void DurationToString(long delta, LPTSTR *outStr)
{
    int days;
    int hours;
    int minutes;
    TCHAR minutesStr[MAX_DURATION_STR+1];
    TCHAR hoursStr[MAX_DURATION_STR+1];

    if (*outStr)
        free(*outStr);
    *outStr = (LPSTR)malloc((MAX_DURATION_STR + 1)* sizeof(TCHAR));
    if (!(*outStr))
        return;

    days = delta / SECONDS_PER_DAY;
    delta -= days * SECONDS_PER_DAY;
    hours = delta / SECONDS_PER_HOUR;
    delta -= hours * SECONDS_PER_HOUR;
    minutes = delta / SECONDS_PER_MINUTE;

    _snprintf(minutesStr, MAX_DURATION_STR, "%d m", minutes);
    minutesStr[MAX_DURATION_STR] = 0;

    _snprintf(hoursStr, MAX_DURATION_STR, "%d h", hours);
    hoursStr[MAX_DURATION_STR] = 0;

    if (days > 0) {
        _snprintf(*outStr, MAX_DURATION_STR, "(%d d, %s remaining)", days,
                  hoursStr);
    } else if (hours > 0) {
        _snprintf(*outStr, MAX_DURATION_STR, "(%s, %s remaining)", hoursStr,
                  minutesStr);
    } else {
        _snprintf(*outStr, MAX_DURATION_STR, "(%s remaining)", minutesStr);
    }
    (*outStr)[MAX_DURATION_STR] = 0;
}

/////////////////////////////////////////////////////////////////////////////
// CLeashView construction/destruction

CLeashView::CLeashView()
{
////@#+Need removing as well!
    m_startup = TRUE;
    m_warningOfTicketTimeLeftKrb5 = 0;
    m_warningOfTicketTimeLeftLockKrb5 = 0;
    m_largeIcons = 0;
    m_destroyTicketsOnExit = 0;
    m_debugWindow = 0;
    m_upperCaseRealm = 0;
    m_lowTicketAlarm = 0;

    m_pDebugWindow = NULL;
    m_pDebugWindow = new CLeashDebugWindow(this);
    if (!m_pDebugWindow)
    {
        AfxMessageBox("There is a problem with the Leash Debug Window!",
                   MB_OK|MB_ICONSTOP);
    }

    m_debugStartUp = TRUE;
    m_isMinimum = FALSE;
    m_lowTicketAlarmSound = FALSE;
    m_alreadyPlayed = FALSE;
    ResetTreeNodes();
    m_hMenu = NULL;
    m_pApp = NULL;
    m_ccacheDisplay = NULL;
    m_autoRenewTickets = 0;
    m_autoRenewalAttempted = 0;
    m_pWarningMessage = NULL;
    m_bIconAdded = FALSE;
    m_bIconDeleted = FALSE;
    m_BaseFont = NULL;
    m_BoldFont = NULL;
    m_ItalicFont = NULL;
    m_aListItemInfo = NULL;
}


CLeashView::~CLeashView()
{
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem) {
        CCacheDisplayData *next = elem->m_next;
        delete elem;
        elem = next;
    }
    m_ccacheDisplay = NULL;
    // destroys window if not already destroyed
    if (m_pDebugWindow)
        delete m_pDebugWindow;
    if (m_BoldFont)
        DeleteObject(m_BoldFont);
    if (m_ItalicFont)
        DeleteObject(m_ItalicFont);
    if (m_aListItemInfo)
        delete[] m_aListItemInfo;
}

void CLeashView::OnItemChanged(NMHDR* pNmHdr, LRESULT* pResult)
{
    NMHEADER* pHdr = (NMHEADER*)pNmHdr;
    if (!pHdr->pitem)
        return;
    if (!pHdr->pitem->mask & HDI_WIDTH)
        return;

    // Sync column width and save to registry
    for (int i = 0, columnIndex = 0; i < NUM_VIEW_COLUMNS; i++) {
        ViewColumnInfo &info = sm_viewColumns[i];
        if ((info.m_enabled) && (columnIndex++ == pHdr->iItem)) {
            info.m_columnWidth = pHdr->pitem->cxy;
            if (m_pApp)
                m_pApp->WriteProfileInt("ColumnWidths", info.m_name, info.m_columnWidth);
            break;
        }
    }
}

BOOL CLeashView::PreCreateWindow(CREATESTRUCT& cs)
{
    // TODO: Modify the Window class or styles here by modifying
    //  the CREATESTRUCT cs

    return CListView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CLeashView diagnostics

#ifdef _DEBUG
VOID CLeashView::AssertValid() const
{
    CListView::AssertValid();
}

VOID CLeashView::Dump(CDumpContext& dc) const
{
    CListView::Dump(dc);
}

/*
LeashDoc* CLeashView::GetDocument() // non-debug version is inline
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(LeashDoc)));
    return (LeashDoc*)m_pDocument;
}
*/
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CLeashView message handlers

BOOL CLeashView::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                        DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                        UINT nID, CCreateContext* pContext)
{
    return CListView::Create(lpszClassName, lpszWindowName, dwStyle, rect,
                             pParentWnd, nID, pContext);
}

INT CLeashView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CListView::OnCreate(lpCreateStruct) == -1)
        return -1;
    return 0;
}

VOID CLeashView::OnClose(void)
{
    printf("OnClose\n");
}

time_t CLeashView::LeashTime()
{
    _tzset();
    return time(0);
}

// Call while possessing a lock to ticketinfo.lockObj
INT CLeashView::GetLowTicketStatus(int ver)
{
    BOOL b_notix = (ver == 5 && !ticketinfo.Krb5.btickets);

    if (b_notix)
        return NO_TICKETS;

    if (m_ticketTimeLeft <= 0L)
        return ZERO_MINUTES_LEFT;

    if (m_ticketTimeLeft <= 20 * 60)
        return (INT)(m_ticketTimeLeft / 5 / 60) + 2 -
            (m_ticketTimeLeft % (5 * 60) == 0 ? 1 : 0);

    return PLENTY_OF_TIME;
}

VOID CLeashView::UpdateTicketTime(TICKETINFO& ti)
{
    if (!ti.btickets) {
        m_ticketTimeLeft = 0L;
        return;
    }

    m_ticketTimeLeft = ti.valid_until - LeashTime();

    if (m_ticketTimeLeft <= 0L)
        ti.btickets = EXPIRED_TICKETS;
}


VOID CALLBACK EXPORT CLeashView::TimerProc(HWND hWnd, UINT nMsg,
                                           UINT_PTR nIDEvent, DWORD dwTime)
{
    // All of the work is being done in the PreTranslateMessage method
    // in order to have access to the object
}

VOID  CLeashView::ApplicationInfoMissingMsg()
{
    AfxMessageBox("There is a problem finding Leash application information!",
               MB_OK|MB_ICONSTOP);
}

VOID CLeashView::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CListView::OnShowWindow(bShow, nStatus);

    // Get State of Icons Size
    m_pApp = AfxGetApp();
    if (!m_pApp)
    {
        ApplicationInfoMissingMsg();
    }
    else
    {
        m_largeIcons = m_pApp->GetProfileInt("Settings", "LargeIcons", ON);

        // Get State of Destroy Tickets On Exit
        m_destroyTicketsOnExit = m_pApp->GetProfileInt("Settings", "DestroyTicketsOnExit", OFF);

        // Get State of Low Ticket Alarm
        m_lowTicketAlarm = m_pApp->GetProfileInt("Settings", "LowTicketAlarm", ON);

        // Get State of Auto Renew Tickets
        m_autoRenewTickets = m_pApp->GetProfileInt("Settings", "AutoRenewTickets", ON);

        // Get State of Upper Case Realm
        m_upperCaseRealm = pLeash_get_default_uppercaserealm();

        // UI main display column widths
        for (int i=0; i<NUM_VIEW_COLUMNS; i++) {
            ViewColumnInfo &info = sm_viewColumns[i];
            info.m_enabled = m_pApp->GetProfileInt("Settings",
                                                   info.m_name,
                                                   info.m_enabled);
            info.m_columnWidth = m_pApp->GetProfileInt("ColumnWidths",
                                                   info.m_name,
                                                   info.m_columnWidth);
        }

        OnLargeIcons();
    }

    SetTimer(1, ONE_SECOND, TimerProc);

    if (!CLeashApp::m_hKrb5DLL)
    {
////Update not to mention K4
        AfxMessageBox("Kerberos Five is not loaded!!!"
                   "\r\nYou will not be able to retrieve tickets and/or "
                   "tokens.",
                   MB_OK|MB_ICONWARNING);
    }

    SetDlgItemText(IDC_LABEL_KERB_TICKETS,
		   "Your Kerberos Tickets (Issued/Expires/[Renew]/Principal)");

    // CLeashApp::m_krbv5_context = NULL;
}

VOID CLeashView::OnInitTicket()
{
    try {
        InitTicket(m_hWnd);
    }
    catch(...) {
        AfxMessageBox("Ticket Getting operation already in progress", MB_OK, 0);
    }
}

UINT CLeashView::InitTicket(void * hWnd)
{
    LSH_DLGINFO_EX ldi;
    char username[64];
    char realm[192];
    int i=0, j=0;
    if (WaitForSingleObject( ticketinfo.lockObj, INFINITE ) != WAIT_OBJECT_0) {
        throw("Unable to lock ticketinfo");
    }
    LeashKRB5ListDefaultTickets(&ticketinfo.Krb5);
    char * principal = ticketinfo.Krb5.principal;
    if (principal)
        for (; principal[i] && principal[i] != '@'; i++)
            username[i] = principal[i];
    username[i] = '\0';
    if (principal && principal[i]) {
        for (i++ ; principal[i] ; i++, j++)
        {
            realm[j] = principal[i];
        }
    }
    realm[j] = '\0';
    LeashKRB5FreeTicketInfo(&ticketinfo.Krb5);
    ReleaseMutex(ticketinfo.lockObj);

    ldi.size = sizeof(ldi);
    ldi.dlgtype = DLGTYPE_PASSWD;
    ldi.title = ldi.in.title;
    strcpy_s(ldi.in.title,"MIT Kerberos: Get Ticket");
    ldi.username = ldi.in.username;
    strcpy(ldi.in.username,username);
    ldi.realm = ldi.in.realm;
    strcpy(ldi.in.realm,realm);
    ldi.dlgtype = DLGTYPE_PASSWD;
    ldi.use_defaults = 1;

    if (!hWnd)
    {
        AfxMessageBox("There is a problem finding the Leash Window!",
                   MB_OK|MB_ICONSTOP);
        return 0;
    }

    int result = pLeash_kinit_dlg_ex((HWND)hWnd, &ldi);

    if (-1 == result)
    {
        AfxMessageBox("There is a problem getting tickets!",
                   MB_OK|MB_ICONSTOP);
    }
    else if ( result )
    {
        if (WaitForSingleObject( ticketinfo.lockObj, INFINITE ) != WAIT_OBJECT_0) {
            throw("Unable to lock ticketinfo");
        }
        m_warningOfTicketTimeLeftKrb5 = 0;
        m_ticketStatusKrb5 = 0;
        ReleaseMutex(ticketinfo.lockObj);
        m_autoRenewalAttempted = 0;
        ::SendMessage((HWND)hWnd, WM_COMMAND, ID_UPDATE_DISPLAY, 0);
    }
    return 0;
}

static UINT krenew(void *param)
{
    char *ccache_name = (char *)param;
    krb5_context ctx = 0;
    krb5_ccache ccache = NULL;
    krb5_principal me = 0;
    krb5_principal server = 0;
    krb5_creds my_creds;
    krb5_data *realm = 0;

    memset(&my_creds, 0, sizeof(krb5_creds));
    if (ccache_name == NULL)
        // Bad param
        goto cleanup;

    krb5_error_code code = pkrb5_init_context(&ctx);
    if (code) {
        // TODO: spew error
        goto cleanup;
    }
    code = pkrb5_cc_resolve(ctx, ccache_name, &ccache);
    if (code) {
        // TODO: spew error
        goto cleanup;
    }

    code = pkrb5_cc_get_principal(ctx, ccache, &me);
    if (code)
        goto cleanup;

    realm = krb5_princ_realm(ctx, me);

    code = pkrb5_build_principal_ext(ctx, &server,
                                    realm->length, realm->data,
                                    KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                    realm->length, realm->data,
                                    0);
    if (code)
        goto cleanup;

    my_creds.client = me;
    my_creds.server = server;

#ifdef KRB5_TC_NOTICKET
    pkrb5_cc_set_flags(ctx, ccache, 0);
#endif
    code = pkrb5_get_renewed_creds(ctx, &my_creds, me, ccache, NULL);
#ifdef KRB5_TC_NOTICKET
    pkrb5_cc_set_flags(ctx, ccache, KRB5_TC_NOTICKET);
#endif
    if (code) {
/* TODO
        if (code != KRB5KDC_ERR_ETYPE_NOSUPP || code != KRB5_KDC_UNREACH)
            Leash_krb5_error(code, "krb5_get_renewed_creds()", 0, &ctx,
                             &ccache);
*/
        goto cleanup;
    }

    code = pkrb5_cc_initialize(ctx, ccache, me);
    if (code)
        goto cleanup;

    code = pkrb5_cc_store_cred(ctx, ccache, &my_creds);
    if (code)
        goto cleanup;

cleanup:
    if (my_creds.client == me)
        my_creds.client = 0;
    if (my_creds.server == server)
        my_creds.server = 0;
    pkrb5_free_cred_contents(ctx, &my_creds);
    if (me != NULL)
        pkrb5_free_principal(ctx, me);
    if (server != NULL)
        pkrb5_free_principal(ctx, server);
    if (ccache != NULL)
        pkrb5_cc_close(ctx, ccache);
    if (ctx != NULL)
        pkrb5_free_context(ctx);
    if (ccache_name != NULL)
        free(ccache_name);

    CLeashApp::m_bUpdateDisplay = TRUE;
    return 0;
}

VOID CLeashView::OnRenewTicket()
{
    if ( !CLeashApp::m_hKrb5DLL )
        return;

    // @TODO: grab list mutex
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem != NULL) {
        if (elem->m_selected) {
            char *ccache_name = strdup(elem->m_ccacheName);
            if (ccache_name)
                AfxBeginThread(krenew, (void *)ccache_name);
        }
        elem = elem->m_next;
    }
    // release list mutex
}

UINT CLeashView::RenewTicket(void * hWnd)
{
    if ( !CLeashApp::m_hKrb5DLL )
        return 0;

    // Try to renew
    BOOL b_renewed = pLeash_renew();
    if ( b_renewed ) {
        m_warningOfTicketTimeLeftKrb5 = 0;
        m_ticketStatusKrb5 = 0;
        m_autoRenewalAttempted = 0;
        ReleaseMutex(ticketinfo.lockObj);
        ::SendMessage((HWND)hWnd, WM_COMMAND, ID_UPDATE_DISPLAY, 0);
        return 0;
    }

    AfxBeginThread(InitTicket,hWnd);

    return 0;
}

static void kdestroy(const char *ccache_name)
{
    krb5_context ctx;
    krb5_ccache ccache=NULL;
    int code = pkrb5_init_context(&ctx);
    if (code) {
        // TODO: spew error
        goto cleanup;
    }
    code = pkrb5_cc_resolve(ctx, ccache_name, &ccache);
    if (code) {
        // TODO: spew error
        goto cleanup;
    }
    code = pkrb5_cc_destroy(ctx, ccache);
    if (code) {
        goto cleanup;
    }
cleanup:
    if (ctx)
        pkrb5_free_context(ctx);
}


VOID CLeashView::OnDestroyTicket()
{
    // @TODO: grab mutex
    BOOL destroy = FALSE;
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem) {
        if (elem->m_selected) {
            // @TODO add princ to msg text
            destroy = TRUE;
        }
        elem = elem->m_next;
    }
    // release mutex

    if (destroy)
    {
        INT whatToDo;

        whatToDo = AfxMessageBox("Are you sure you want to destroy these tickets?",
                                    MB_ICONEXCLAMATION|MB_YESNO, 0);

        if (whatToDo == IDYES)
        {
            // grab list mutex
            elem = m_ccacheDisplay;
            while (elem) {
                if (elem->m_selected)
                    kdestroy(elem->m_ccacheName);
                elem = elem->m_next;
            }
            // release list mutex
            SendMessage(WM_COMMAND, ID_UPDATE_DISPLAY, 0);
        }
    }
    m_autoRenewalAttempted = 0;
}

VOID CLeashView::OnMakeDefault()
{
    CCacheDisplayData *elem = m_ccacheDisplay;
    int code = 0;
    krb5_context ctx;
    krb5_ccache cc;
    while (elem) {
        if (elem->m_selected) {
            pkrb5_init_context(&ctx);
            code = pkrb5_cc_resolve(ctx, elem->m_ccacheName, &cc);
            if (!code)
                code = pkrb5_cc_switch(ctx, cc);
            if (!code) {
                const char *cctype = pkrb5_cc_get_type(ctx, cc);
                if (cctype != NULL) {
                    char defname[20];
                    sprintf_s(defname, "%s:", cctype);
                    code = pkrb5int_cc_user_set_default_name(ctx, defname);
                }
            }
            pkrb5_free_context(ctx);
            CLeashApp::m_bUpdateDisplay = TRUE;
            break;
        }
        elem = elem->m_next;
    }
}

VOID CLeashView::OnChangePassword()
{
    krb5_context ctx = 0;
    krb5_ccache ccache = 0;
    krb5_principal princ = 0;
    char *pname = NULL;
    char *username = NULL;
    char *realm = NULL;
    int code = 0;

    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem != NULL) {
        if (elem->m_selected) {
            if (elem->m_ccacheName)
                break;
        }
        elem = elem->m_next;
    }
    if (elem != NULL) {
        code = pkrb5_init_context(&ctx);
        if (code) {
            // TODO: spew error
            goto cleanup;
        }
        code = pkrb5_cc_resolve(ctx, elem->m_ccacheName, &ccache);
        if (code) {
            // TODO: spew error
            goto cleanup;
        }
        code = pkrb5_cc_get_principal(ctx, ccache, &princ);
        if (code) {
            goto cleanup;
        }
        code = pkrb5_unparse_name(ctx, princ, &pname);
        if (code) {
            goto cleanup;
        }
    }

    LSH_DLGINFO_EX ldi;
    if (pname != NULL) {
        username = pname;
        realm = strchr(pname, '@');
        if (realm != NULL)
            *realm++ = '\0';
    }
    ldi.size = sizeof(ldi);
    ldi.dlgtype = DLGTYPE_CHPASSWD;
    ldi.title = ldi.in.title;
    strcpy_s(ldi.in.title, "MIT Kerberos: Change Password");
    ldi.username = ldi.in.username;
    strcpy_s(ldi.in.username, username ? username : "");
    ldi.realm = ldi.in.realm;
    strcpy_s(ldi.in.realm, realm ? realm : "");
    ldi.use_defaults = 1;

    int result = pLeash_changepwd_dlg_ex(m_hWnd, &ldi);
    if (-1 == result) {
        AfxMessageBox("There is a problem changing password!",
                   MB_OK|MB_ICONSTOP);
    }
cleanup:
    if (pname != NULL)
        pkrb5_free_unparsed_name(ctx, pname);
    if (princ != NULL)
        pkrb5_free_principal(ctx, princ);
    if (ccache != NULL)
        pkrb5_cc_close(ctx, ccache);
    if (ctx != NULL)
        pkrb5_free_context(ctx);
}

static CCacheDisplayData **
FindCCacheDisplayData(const char * ccacheName, CCacheDisplayData **pList)
{
    CCacheDisplayData *elem;
    while ((elem = *pList)) {
        if (strcmp(ccacheName, elem->m_ccacheName)==0)
            return pList;
        pList = &elem->m_next;
    }
    return NULL;
}

void CLeashView::AddDisplayItem(CListCtrl &list,
                                CCacheDisplayData *elem,
                                int iItem,
                                char *principal,
                                time_t issued,
                                time_t valid_until,
                                time_t renew_until,
                                char *encTypes,
                                unsigned long flags,
                                char *ccache_name)
{
    TCHAR* localTimeStr=NULL;
    TCHAR* durationStr=NULL;
    TCHAR* flagsStr=NULL;
    TCHAR tempStr[MAX_DURATION_STR+1];
    time_t now = LeashTime();

    list.InsertItem(iItem, principal, -1);

    int iSubItem = 1;
    if (sm_viewColumns[TIME_ISSUED].m_enabled) {
        if (issued == 0) {
            list.SetItemText(iItem, iSubItem++, "Unknown");
        } else {
            TimestampToLocalizedString(issued, &localTimeStr);
            list.SetItemText(iItem, iSubItem++, localTimeStr);
        }
    }
    if (sm_viewColumns[RENEWABLE_UNTIL].m_enabled) {
        if (valid_until == 0) {
            list.SetItemText(iItem, iSubItem++, "Unknown");
        } else if (valid_until < now) {
            list.SetItemText(iItem, iSubItem++, "Expired");
        } else if (renew_until) {
            TimestampToLocalizedString(renew_until, &localTimeStr);
            DurationToString(renew_until - now, &durationStr);
            if (localTimeStr && durationStr) {
                _snprintf(tempStr, MAX_DURATION_STR, "%s %s", localTimeStr, durationStr);
                tempStr[MAX_DURATION_STR] = 0;
                list.SetItemText(iItem, iSubItem++, tempStr);
            }
        } else {
            list.SetItemText(iItem, iSubItem++, "Not renewable");
        }
    }
    if (sm_viewColumns[VALID_UNTIL].m_enabled) {
        if (valid_until == 0) {
            list.SetItemText(iItem, iSubItem++, "Unknown");
        } else if (valid_until < now) {
            list.SetItemText(iItem, iSubItem++, "Expired");
        } else {
            TimestampToLocalizedString(valid_until, &localTimeStr);
            DurationToString(valid_until - now, &durationStr);
            if (localTimeStr && durationStr) {
                _snprintf(tempStr, MAX_DURATION_STR, "%s %s", localTimeStr, durationStr);
                tempStr[MAX_DURATION_STR] = 0;
                list.SetItemText(iItem, iSubItem++, tempStr);
            }
        }
    }

    if (sm_viewColumns[ENCRYPTION_TYPE].m_enabled) {
        list.SetItemText(iItem, iSubItem++, encTypes);
    }
    if (sm_viewColumns[TICKET_FLAGS].m_enabled) {
        krb5TicketFlagsToString(flags, &flagsStr);
        list.SetItemText(iItem, iSubItem++, flagsStr);
    }
    if (sm_viewColumns[CACHE_NAME].m_enabled) {
        list.SetItemText(iItem, iSubItem++, ccache_name);
    }
    if (flagsStr)
        free(flagsStr);
    if (localTimeStr)
        free(localTimeStr);
    if (durationStr)
        free(durationStr);
}

BOOL CLeashView::IsExpanded(TICKETINFO *info)
{
    CCacheDisplayData **pElem = FindCCacheDisplayData(info->ccache_name,
                                                      &m_ccacheDisplay);
    return (pElem && (*pElem)->m_expanded) ? TRUE : FALSE;
}

BOOL CLeashView::IsExpired(TICKETINFO *info)
{
    return LeashTime() > info->valid_until ? TRUE : FALSE;
}

BOOL CLeashView::IsExpired(TicketList *ticket)
{
    return LeashTime() > ticket->valid_until ? TRUE : FALSE;
}

CCacheDisplayData *
FindCCacheDisplayElem(CCacheDisplayData *pElem, int itemIndex)
{
    while (pElem != NULL) {
        if (pElem->m_index == itemIndex)
            return pElem;
        pElem = pElem->m_next;
    }
    return NULL;
}

VOID CLeashView::OnUpdateDisplay()
{
    CListCtrl& list = GetListCtrl();
    // @TODO: there is probably a more sensible place to initialize these...
    if ((m_BaseFont == NULL) && (list.GetFont())) {
        m_BaseFont = *list.GetFont();
        m_BoldFont = CreateBoldFont(m_BaseFont);
        m_ItalicFont = CreateItalicFont(m_BaseFont);
        m_BoldItalicFont = CreateBoldItalicFont(m_BaseFont);
    }
    // Determine currently focused item
    int focusItem = list.GetNextItem(-1, LVNI_FOCUSED);
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem) {
        if (focusItem >= elem->m_index) {
            elem->m_focus = focusItem - elem->m_index;
            focusItem = -1;
        } else {
            elem->m_focus = -1;
        }
        elem = elem->m_next;
    }

    list.DeleteAllItems();
    ModifyStyle(LVS_TYPEMASK, LVS_REPORT);
	UpdateWindow();
    // Delete all of the columns.
    while (list.DeleteColumn(0));

    list.SetImageList(&m_imageList, LVSIL_SMALL);

    // Reconstruct based on current options
    int columnIndex = 0;
    int itemIndex = 0;
    for (int i = 0; i < NUM_VIEW_COLUMNS; i++) {
        ViewColumnInfo &info = sm_viewColumns[i];
        if (info.m_enabled) {
            list.InsertColumn(columnIndex++,
                (info.m_name), // @LOCALIZEME!
                LVCFMT_LEFT,
                info.m_columnWidth,
                itemIndex++);
        }
    }

    INT ticketIconStatusKrb5;
    INT ticketIconStatus_SelectedKrb5;
    INT iconStatusKrb5;

    if (WaitForSingleObject( ticketinfo.lockObj, 100 ) != WAIT_OBJECT_0)
        throw("Unable to lock ticketinfo");

    // Get Kerb 5 tickets in list
    LeashKRB5ListDefaultTickets(&ticketinfo.Krb5);
    if (CLeashApp::m_hKrb5DLL && !CLeashApp::m_krbv5_profile)
    {
        CHAR confname[MAX_PATH];
        if (CLeashApp::GetProfileFile(confname, sizeof(confname)))
        {
            AfxMessageBox("Can't locate Kerberos Five Config. file!",
                        MB_OK|MB_ICONSTOP);
        }

        const char *filenames[2];
        filenames[0] = confname;
        filenames[1] = NULL;
        pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
    }

    /*
     * Update Ticket Status for Krb5 so that we may use their state
     * to select the appropriate Icon for the Parent Node
     */

    /* Krb5 */
    UpdateTicketTime(ticketinfo.Krb5);
    m_ticketStatusKrb5 = GetLowTicketStatus(5);
    if ((!ticketinfo.Krb5.btickets) ||
        EXPIRED_TICKETS == ticketinfo.Krb5.btickets ||
        m_ticketStatusKrb5 == ZERO_MINUTES_LEFT)
    {
        ticketIconStatusKrb5 = EXPIRED_CLOCK;
        ticketIconStatus_SelectedKrb5 = EXPIRED_CLOCK;
        iconStatusKrb5 = EXPIRED_TICKET;
    }
    else if (TICKETS_LOW == ticketinfo.Krb5.btickets ||
             m_ticketStatusKrb5 == FIVE_MINUTES_LEFT ||
             m_ticketStatusKrb5 == TEN_MINUTES_LEFT ||
             m_ticketStatusKrb5 == FIFTEEN_MINUTES_LEFT)
    {
        ticketIconStatusKrb5 = LOW_CLOCK;
        ticketIconStatus_SelectedKrb5 = LOW_CLOCK;
        iconStatusKrb5 = LOW_TICKET;
    }
    else if ( CLeashApp::m_hKrb5DLL )
    {
        ticketIconStatusKrb5 = ACTIVE_CLOCK;
        ticketIconStatus_SelectedKrb5 = ACTIVE_CLOCK;
        iconStatusKrb5 = ACTIVE_TICKET;
    } else
    {
        ticketIconStatusKrb5 = EXPIRED_CLOCK;
        ticketIconStatus_SelectedKrb5 = EXPIRED_CLOCK;
        iconStatusKrb5 = TICKET_NOT_INSTALLED;
    }

    int trayIcon = NONE_PARENT_NODE;
    if (CLeashApp::m_hKrb5DLL && ticketinfo.Krb5.btickets) {
        switch ( iconStatusKrb5 ) {
        case ACTIVE_TICKET:
            trayIcon = ACTIVE_PARENT_NODE;
            break;
        case LOW_TICKET:
            trayIcon = LOW_PARENT_NODE;
            break;
        case EXPIRED_TICKET:
            trayIcon = EXPIRED_PARENT_NODE;
            break;
        }
    }
    SetTrayIcon(NIM_MODIFY, trayIcon);

    CCacheDisplayData* prevCCacheDisplay = m_ccacheDisplay;
    m_ccacheDisplay = NULL;

    const char *def_ccache_name = ticketinfo.Krb5.ccache_name;
    TICKETINFO *principallist = NULL;
    LeashKRB5ListAllTickets(&principallist);
    int iItem = 0;
    TicketList* tempList;
    TICKETINFO *principal = principallist;
    while (principal != NULL) {
        CCacheDisplayData **pOldElem;
        pOldElem = FindCCacheDisplayData(principal->ccache_name,
                                         &prevCCacheDisplay);
        if (pOldElem) {
            // remove from old list
            elem = *pOldElem;
            *pOldElem = elem->m_next;
            elem->m_next = NULL;
        } else {
            elem = new CCacheDisplayData(principal->ccache_name);
        }
        elem->m_isDefault = def_ccache_name &&
                            (strcmp(def_ccache_name, elem->m_ccacheName) == 0);
        elem->m_isRenewable = principal->renew_until != 0;

        elem->m_next = m_ccacheDisplay;
        m_ccacheDisplay = elem;
        elem->m_index = iItem;

        AddDisplayItem(list,
                       elem,
                       iItem++,
                       principal->principal,
                       principal->issued,
                       principal->valid_until,
                       principal->renew_until,
                       "",
                       principal->flags,
                       principal->ccache_name);
        if (elem->m_expanded) {
            for (tempList = principal->ticket_list;
                 tempList != NULL;
                 tempList = tempList->next) {
                AddDisplayItem(list,
                               elem,
                               iItem++,
                               tempList->service,
                               tempList->issued,
                               tempList->valid_until,
                               tempList->renew_until,
                               tempList->encTypes,
                               tempList->flags,
                               principal->ccache_name);
            }
        }
        if ((elem->m_focus >= 0) &&
            (iItem > elem->m_index + elem->m_focus)) {
            list.SetItemState(elem->m_index + elem->m_focus, LVIS_FOCUSED,
                              LVIS_FOCUSED);
        }
        if (elem->m_selected)
            list.SetItemState(elem->m_index, LVIS_SELECTED, LVIS_SELECTED);

        principal = principal->next;
    }

    // create list item font data array
    if (m_aListItemInfo != NULL)
        delete[] m_aListItemInfo;
    m_aListItemInfo = new ListItemInfo[iItem];
    iItem = 0;
    for (principal = principallist; principal != NULL;
         principal = principal->next) {
        //
        HFONT font, durationFont;
        elem = FindCCacheDisplayElem(m_ccacheDisplay, iItem);
        if (elem != NULL && elem->m_isDefault) {
            font = m_BoldFont;
            durationFont = IsExpired(principal) ? m_BoldItalicFont : m_BoldFont;
        } else {
            font = m_BaseFont;
            durationFont = IsExpired(principal) ? m_ItalicFont : m_BaseFont;
        }
        m_aListItemInfo[iItem].m_font = font;
        m_aListItemInfo[iItem++].m_durationFont = durationFont;

        if (IsExpanded(principal)) {
            for (TicketList *ticket = principal->ticket_list;
                 ticket != NULL; ticket = ticket->next) {
                font = m_BaseFont;
                durationFont = IsExpired(ticket) ? m_ItalicFont : m_BaseFont;
                m_aListItemInfo[iItem].m_font = font;
                m_aListItemInfo[iItem++].m_durationFont = durationFont;
            }
        }
    }

    // delete ccache items that no longer exist
    while (prevCCacheDisplay != NULL) {
        CCacheDisplayData *next = prevCCacheDisplay->m_next;
        delete prevCCacheDisplay;
        prevCCacheDisplay = next;
    }

    LeashKRB5FreeTicketInfo(&ticketinfo.Krb5);
    LeashKRB5FreeTickets(&principallist);

    ReleaseMutex(ticketinfo.lockObj);
}

VOID CLeashView::OnSynTime()
{
    LONG returnValue;
    returnValue = pLeash_timesync(1);
}

VOID CLeashView::OnActivateView(BOOL bActivate, CView* pActivateView,
                                CView* pDeactiveView)
{
    UINT check = NULL;

    if (m_alreadyPlayed)
    {
        CListView::OnActivateView(bActivate, pActivateView, pDeactiveView);
        return;
    }

    // The following code has put here because at the time
    // 'checking and unchecking' a menuitem with the
    // 'OnUpdate.....(CCmdUI* pCmdUI) functions' were unreliable
    // in CLeashView -->> Better done in CMainFrame
    if( CLeashApp::m_hProgram != 0 )
    {
        m_hMenu = ::GetMenu(CLeashApp::m_hProgram);
    } else {
        return;
    }

    if (m_hMenu) {
        if (!m_largeIcons)
            check = CheckMenuItem(m_hMenu, ID_LARGE_ICONS, MF_CHECKED);
        else
            check = CheckMenuItem(m_hMenu, ID_LARGE_ICONS, MF_UNCHECKED);

        if( check != MF_CHECKED || check != MF_UNCHECKED )
        {
            m_debugStartUp = 1;
        }

        if (!m_destroyTicketsOnExit)
            check = CheckMenuItem(m_hMenu, ID_KILL_TIX_ONEXIT, MF_UNCHECKED);
        else
            check = CheckMenuItem(m_hMenu, ID_KILL_TIX_ONEXIT, MF_CHECKED);

        if (!m_upperCaseRealm)
            check = CheckMenuItem(m_hMenu, ID_UPPERCASE_REALM, MF_UNCHECKED);
        else
            check = CheckMenuItem(m_hMenu, ID_UPPERCASE_REALM, MF_CHECKED);

        for (int i=0; i<NUM_VIEW_COLUMNS; i++) {
            ViewColumnInfo &info = sm_viewColumns[i];
            if (info.m_id >= 0)
                CheckMenuItem(m_hMenu, info.m_id,
                              info.m_enabled ? MF_CHECKED : MF_UNCHECKED);
        }

        if (!m_lowTicketAlarm)
            CheckMenuItem(m_hMenu, ID_LOW_TICKET_ALARM, MF_UNCHECKED);
        else
            CheckMenuItem(m_hMenu, ID_LOW_TICKET_ALARM, MF_CHECKED);

        if (!m_autoRenewTickets)
            CheckMenuItem(m_hMenu, ID_AUTO_RENEW, MF_UNCHECKED);
        else
            CheckMenuItem(m_hMenu, ID_AUTO_RENEW, MF_CHECKED);

        m_debugWindow = m_pApp->GetProfileInt("Settings", "DebugWindow", 0);
        if (!m_debugWindow)
            check = CheckMenuItem(m_hMenu, ID_DEBUG_MODE, MF_UNCHECKED);
        else
            check = CheckMenuItem(m_hMenu, ID_DEBUG_MODE, MF_CHECKED);
    }
    m_lowTicketAlarmSound = !!m_lowTicketAlarm;
    m_alreadyPlayed = TRUE;
    if (m_pApp)
    {
        m_debugWindow = m_pApp->GetProfileInt("Settings", "DebugWindow", 0);

        if (m_hMenu)
        {
            if (!m_debugWindow)
            {
                CheckMenuItem(m_hMenu, ID_DEBUG_MODE, MF_UNCHECKED);
            }
            else
            {
                CheckMenuItem(m_hMenu, ID_DEBUG_MODE, MF_CHECKED);
            }
        }
    }
    else
    {
        ApplicationInfoMissingMsg();
    }

    m_alreadyPlayed = TRUE;

    if (m_debugStartUp)
    {
        OnDebugMode();
    }

    m_debugStartUp = FALSE;

    CListView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

////@#+Is this KRB4 only?
VOID CLeashView::OnDebugMode()
{
    if (!m_pDebugWindow)
    {
        AfxMessageBox("There is a problem with the Leash Debug Window!",
                   MB_OK|MB_ICONSTOP);
        return;
    }


    // Check all possible 'KRB' system variables, then delete debug file
    CHAR*  Env[] = {"TEMP", "TMP", "HOME", NULL};
    CHAR** pEnv = Env;
    CHAR debugFilePath[MAX_PATH];
    *debugFilePath = 0;

    while (*pEnv)
    {
        CHAR* ptestenv = getenv(*pEnv);
        if (ptestenv)
        {
            // reset debug file
            strcpy(debugFilePath, ptestenv);
            strcat(debugFilePath, "\\LshDebug.log");
            remove(debugFilePath);
            break;
        }

        pEnv++;
    }

    if (!m_debugStartUp)
    {
        if (m_debugWindow%2 == 0)
            m_debugWindow = ON;
        else
            m_debugWindow = OFF;
    }

    if (!m_pApp)
    {
        ApplicationInfoMissingMsg();
    }
    else if (!m_debugWindow)
    {
        if (m_hMenu)
            CheckMenuItem(m_hMenu, ID_DEBUG_MODE, MF_UNCHECKED);

        m_pApp->WriteProfileInt("Settings", "DebugWindow", FALSE_FLAG);
        m_pDebugWindow->DestroyWindow();
        return;
    }
    else
    {
        if (m_hMenu)
            CheckMenuItem(m_hMenu, ID_DEBUG_MODE, MF_CHECKED);

        m_pApp->WriteProfileInt("Settings", "DebugWindow", TRUE_FLAG);
    }

    // Creates the Debug dialog if not created already
    if (m_pDebugWindow->GetSafeHwnd() == 0)
    { // displays the Debug Window
        m_pDebugWindow->Create(debugFilePath);
    }
}

void CLeashView::ToggleViewColumn(eViewColumn viewOption)
{
    if ((viewOption < 0) || (viewOption >= NUM_VIEW_COLUMNS)) {
        //LeashWarn("ToggleViewColumn(): invalid view option index %i", viewOption);
        return;
    }
    ViewColumnInfo &info = sm_viewColumns[viewOption];
    info.m_enabled = !info.m_enabled;
    if (m_pApp)
        m_pApp->WriteProfileInt("Settings", info.m_name, info.m_enabled);
    // Don't update display immediately; wait for next idle so our
    // checkbox controls will be more responsive
    CLeashApp::m_bUpdateDisplay = TRUE;
}

VOID CLeashView::OnRenewableUntil()
{
    ToggleViewColumn(RENEWABLE_UNTIL);
}

VOID CLeashView::OnUpdateRenewableUntil(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(sm_viewColumns[RENEWABLE_UNTIL].m_enabled);
}

VOID CLeashView::OnShowTicketFlags()
{
    ToggleViewColumn(TICKET_FLAGS);
}

VOID CLeashView::OnUpdateShowTicketFlags(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(sm_viewColumns[TICKET_FLAGS].m_enabled);
}

VOID CLeashView::OnTimeIssued()
{
    ToggleViewColumn(TIME_ISSUED);
}

VOID CLeashView::OnUpdateTimeIssued(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(sm_viewColumns[TIME_ISSUED].m_enabled);
}

VOID CLeashView::OnValidUntil()
{
    ToggleViewColumn(VALID_UNTIL);
}

VOID CLeashView::OnUpdateValidUntil(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(sm_viewColumns[VALID_UNTIL].m_enabled);
}

VOID CLeashView::OnEncryptionType()
{
    ToggleViewColumn(ENCRYPTION_TYPE);
}

VOID CLeashView::OnUpdateEncryptionType(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(sm_viewColumns[ENCRYPTION_TYPE].m_enabled);
}

VOID CLeashView::OnCcacheName()
{
    ToggleViewColumn(CACHE_NAME);
}

VOID CLeashView::OnUpdateCcacheName(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(sm_viewColumns[CACHE_NAME].m_enabled);
}

VOID CLeashView::OnLargeIcons()
{
    INT x, y, n;

    if (change_icon_size)
    {
        if (m_largeIcons%2 == 0)
            m_largeIcons = ON;
        else
            m_largeIcons = OFF;
    }
    else
    {
        if (m_largeIcons%2 == 0)
            m_largeIcons = OFF;
        else
            m_largeIcons = ON;
    }

    x = y = SMALL_ICONS;

    if (!m_pApp)
        ApplicationInfoMissingMsg();
    else
    {
        if (!m_largeIcons)
        {
            if (m_hMenu)
                CheckMenuItem(m_hMenu, ID_LARGE_ICONS, MF_CHECKED);

            x = y = LARGE_ICONS;

	    if (!m_startup)
	    {
                m_pApp->WriteProfileInt("Settings", "LargeIcons", TRUE_FLAG);
	    }
        }
        else
        {
            if (m_hMenu)
                CheckMenuItem(m_hMenu, ID_LARGE_ICONS, MF_UNCHECKED);

            x = y = SMALL_ICONS;

            if (!m_startup)
            {
                m_pApp->WriteProfileInt("Settings", "LargeIcons", FALSE_FLAG);
            }
        }
    }

    HICON hIcon[IMAGE_COUNT];
    for (n = 0; n < IMAGE_COUNT; n++)
    {
        hIcon[n] = NULL;
    }

    m_imageList.DeleteImageList( );

    UINT bitsPerPixel = GetDeviceCaps( ::GetDC(::GetDesktopWindow()), BITSPIXEL);
    UINT ilcColor;
    if ( bitsPerPixel >= 32 )
        ilcColor = ILC_COLOR32;
    else if ( bitsPerPixel >= 24 )
        ilcColor = ILC_COLOR24;
    else if ( bitsPerPixel >= 16 )
        ilcColor = ILC_COLOR16;
    else if ( bitsPerPixel >= 8 )
        ilcColor = ILC_COLOR8;
    else
        ilcColor = ILC_COLOR;
    m_imageList.Create(x, y, ilcColor | ILC_MASK, IMAGE_COUNT, 1);
    m_imageList.SetBkColor(GetSysColor(COLOR_WINDOW));

    hIcon[ACTIVE_TRAY_ICON] = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_GOOD);
    hIcon[LOW_TRAY_ICON] = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_LOW);
    hIcon[EXPIRED_TRAY_ICON] = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_EXPIRED);
    hIcon[NONE_TRAY_ICON]  = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_NONE);
    hIcon[ACTIVE_PARENT_NODE] = AfxGetApp()->LoadIcon(IDI_LEASH_PRINCIPAL_GOOD);
    hIcon[LOW_PARENT_NODE] = AfxGetApp()->LoadIcon(IDI_LEASH_PRINCIPAL_LOW);
    hIcon[EXPIRED_PARENT_NODE] = AfxGetApp()->LoadIcon(IDI_LEASH_PRINCIPAL_EXPIRED);
    hIcon[NONE_PARENT_NODE]  = AfxGetApp()->LoadIcon(IDI_LEASH_PRINCIPAL_NONE);
    hIcon[ACTIVE_TICKET] = AfxGetApp()->LoadIcon(IDI_TICKETTYPE_GOOD);
    hIcon[LOW_TICKET] = AfxGetApp()->LoadIcon(IDI_TICKETTYPE_LOW);
    hIcon[EXPIRED_TICKET] = AfxGetApp()->LoadIcon(IDI_TICKETTYPE_EXPIRED);
    hIcon[TICKET_NOT_INSTALLED] = AfxGetApp()->LoadIcon(IDI_TICKETTYPE_NOTINSTALLED);
    hIcon[ACTIVE_CLOCK] = AfxGetApp()->LoadIcon(IDI_TICKET_GOOD);
    hIcon[LOW_CLOCK] = AfxGetApp()->LoadIcon(IDI_TICKET_LOW);
    hIcon[EXPIRED_CLOCK] = AfxGetApp()->LoadIcon(IDI_TICKET_EXPIRED);
    hIcon[TKT_ADDRESS] = AfxGetApp()->LoadIcon(IDI_LEASH_TICKET_ADDRESS);
    hIcon[TKT_SESSION] = AfxGetApp()->LoadIcon(IDI_LEASH_TICKET_SESSION);
    hIcon[TKT_ENCRYPTION] = AfxGetApp()->LoadIcon(IDI_LEASH_TICKET_ENCRYPTION);

    for (n = 0; n < IMAGE_COUNT; n++)
    {
        if ( !hIcon[n] ) {
            AfxMessageBox("Can't find one or more images in the Leash Ticket Tree!",
                        MB_OK|MB_ICONSTOP);
            return;
        }
        m_imageList.Add(hIcon[n]);
    }

    if (!m_startup)
        SendMessage(WM_COMMAND, ID_UPDATE_DISPLAY, 0);
}

VOID CLeashView::OnKillTixOnExit()
{
    m_destroyTicketsOnExit = !m_destroyTicketsOnExit;

    if (m_pApp)
        m_pApp->WriteProfileInt("Settings", "DestroyTicketsOnExit",
                                m_destroyTicketsOnExit);
}

VOID CLeashView::OnUpdateKillTixOnExit(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(m_destroyTicketsOnExit);
}

VOID CLeashView::OnUppercaseRealm()
{
    m_upperCaseRealm = !m_upperCaseRealm;

    pLeash_set_default_uppercaserealm(m_upperCaseRealm);
}

VOID CLeashView::OnUpdateUppercaseRealm(CCmdUI *pCmdUI)
{
    // description is now 'allow mixed case', so reverse logic
    pCmdUI->SetCheck(!m_upperCaseRealm);
}

VOID CLeashView::ResetTreeNodes()
{
    m_hPrincipalState = 0;
    m_hKerb5State = 0;
}

VOID CLeashView::OnDestroy()
{
    CCacheDisplayData *elem;
    SetTrayIcon(NIM_DELETE);

    if (m_destroyTicketsOnExit) {
        elem = m_ccacheDisplay;
        while (elem != NULL) {
            kdestroy(elem->m_ccacheName);
            elem = elem->m_next;
        }
    }
    CListView::OnDestroy();
}

VOID CLeashView::OnUpdateDestroyTicket(CCmdUI* pCmdUI)
{
    // @TODO: mutex
    BOOL enable = FALSE;
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem != NULL) {
        if (elem->m_selected) {
            enable = TRUE;
            break;
        }
        elem = elem->m_next;
    }

    pCmdUI->Enable(enable);
}

VOID CLeashView::OnUpdateInitTicket(CCmdUI* pCmdUI)
{
  if (!CLeashApp::m_hKrb5DLL)
        pCmdUI->Enable(FALSE);
    else
        pCmdUI->Enable(TRUE);
}

VOID CLeashView::OnUpdateRenewTicket(CCmdUI* pCmdUI)
{
    // @TODO: mutex
    BOOL enable = FALSE;
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem != NULL) {
        if (elem->m_selected) { // @TODO: && elem->m_renewable
            enable = TRUE;
            break;
        }
        elem = elem->m_next;
    }

    pCmdUI->Enable(enable);
}

LRESULT CLeashView::OnGoodbye(WPARAM wParam, LPARAM lParam)
{
    m_pDebugWindow->DestroyWindow();
    return 0L;
}

VOID CLeashView::OnLeashRestore()
{
    if ( CMainFrame::m_isMinimum ) {
        CMainFrame * frame = (CMainFrame *)GetParentFrame();
        frame->ShowTaskBarButton(TRUE);
        frame->ShowWindow(SW_SHOWNORMAL);
    }
}

VOID CLeashView::OnLeashMinimize()
{
    if ( !CMainFrame::m_isMinimum ) {
        CMainFrame * frame = (CMainFrame *)GetParentFrame();
        // frame->ShowTaskBarButton(FALSE);
        frame->ShowWindow(SW_HIDE);
        frame->ShowWindow(SW_MINIMIZE);
    }
}

LRESULT CLeashView::OnTrayIcon(WPARAM wParam, LPARAM lParam)
{
    switch ( lParam ) {
    case WM_LBUTTONDOWN:
        if ( CMainFrame::m_isMinimum )
            OnLeashRestore();
        else
            OnLeashMinimize();
        break;
    case WM_RBUTTONDOWN:
        {
            int nFlags;
            CMenu * menu = new CMenu();
            menu->CreatePopupMenu();
            if ( !CMainFrame::m_isMinimum )
                menu->AppendMenu(MF_STRING, ID_LEASH_MINIMIZE, "&Close MIT Kerberos Window");
            else
                menu->AppendMenu(MF_STRING, ID_LEASH_RESTORE, "&Open MIT Kerberos Window");
            menu->AppendMenu(MF_SEPARATOR);
            menu->AppendMenu(MF_STRING, ID_INIT_TICKET, "&Get Tickets");
            if (WaitForSingleObject( ticketinfo.lockObj, INFINITE ) != WAIT_OBJECT_0)
                throw("Unable to lock ticketinfo");
            if (!ticketinfo.Krb5.btickets ||
		!CLeashApp::m_hKrb5DLL)
                nFlags = MF_STRING | MF_GRAYED;
            else
                nFlags = MF_STRING;
            menu->AppendMenu(nFlags, ID_RENEW_TICKET, "&Renew Tickets");
            if (!ticketinfo.Krb5.btickets)
                nFlags = MF_STRING | MF_GRAYED;
            else
                nFlags = MF_STRING;
            ReleaseMutex(ticketinfo.lockObj);
            menu->AppendMenu(MF_STRING, ID_DESTROY_TICKET, "&Destroy Tickets");
            menu->AppendMenu(MF_STRING, ID_CHANGE_PASSWORD, "&Change Password");

            menu->AppendMenu(MF_SEPARATOR);
            if ( m_autoRenewTickets )
                nFlags = MF_STRING | MF_CHECKED;
            else
                nFlags = MF_STRING | MF_UNCHECKED;
            menu->AppendMenu(nFlags, ID_AUTO_RENEW, "&Automatic Ticket Renewal");
            if ( m_lowTicketAlarm )
                nFlags = MF_STRING | MF_CHECKED;
            else
                nFlags = MF_STRING | MF_UNCHECKED;
            menu->AppendMenu(nFlags, ID_LOW_TICKET_ALARM, "&Expiration Alarm");
            menu->AppendMenu(MF_SEPARATOR);
            menu->AppendMenu(MF_STRING, ID_APP_EXIT, "E&xit");
            menu->SetDefaultItem(ID_LEASH_RESTORE);

            POINT pt;
            GetCursorPos(&pt);

	    SetForegroundWindow();
            menu->TrackPopupMenu(TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
                                pt.x, pt.y, GetParentFrame());
	    PostMessage(WM_NULL, 0, 0);
            menu->DestroyMenu();
            delete menu;
        }
        break;
    case WM_MOUSEMOVE:
        // SendMessage(WM_COMMAND, ID_UPDATE_DISPLAY, 0);
        break;
    }
    return 0L;
}

VOID CLeashView::OnAppAbout()
{
    CLeashAboutBox leashAboutBox;
    // To debug loaded dlls:
    // leashAboutBox.m_bListModules = TRUE;
    leashAboutBox.DoModal();
}


VOID CLeashView::OnInitialUpdate()
{
    CListView::OnInitialUpdate();
    CLeashApp::m_hProgram = ::FindWindow(_T("LEASH.0WNDCLASS"), NULL);
    EnableToolTips();
}

VOID CLeashView::OnItemexpandedTreeview(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

    if (m_hPrincipal == pNMTreeView->itemNew.hItem)
        m_hPrincipalState = pNMTreeView->action;
    else if (m_hKerb5 == pNMTreeView->itemNew.hItem)
        m_hKerb5State = pNMTreeView->action;

    CMainFrame::m_isBeingResized = TRUE;
    *pResult = 0;
}

VOID CLeashView::OnUpdateDebugMode(CCmdUI* pCmdUI)
{
        pCmdUI->Enable(FALSE);
}

VOID CLeashView::OnUpdateCfgFiles(CCmdUI* pCmdUI)
{
        pCmdUI->Enable(FALSE);
}

/*
void CLeashView::GetRowWidthHeight(CDC* pDC, LPCSTR theString, int& nRowWidth,
                                   int& nRowHeight, int& nCharWidth)
{
    TEXTMETRIC tm;

    //CEx29aDoc* pDoc = GetDocument();
	pDC->GetTextMetrics(&tm);
    nCharWidth = tm.tmAveCharWidth + 1;
    nRowWidth = strlen(theString);

    //int nFields = theString.GetLength();

    //for(int i = 0; i < nFields; i++)
    //{
	//    nRowWidth += nCharWidth;
	//}

    nRowWidth *= nCharWidth;
    nRowHeight = tm.tmHeight;
}
*/

void CLeashView::SetTrayText(int nim, CString tip)
{
    if ( (nim == NIM_MODIFY) && (m_bIconDeleted) )
        return;
    if ( (nim == NIM_MODIFY) && (!m_bIconAdded) )
        nim = NIM_ADD;

    if ( (nim != NIM_DELETE) || IsWindow(m_hWnd) )
    {
        NOTIFYICONDATA nid;
        memset (&nid, 0x00, sizeof(NOTIFYICONDATA));
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = m_hWnd;
        nid.uID = 0;
        nid.uFlags = NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        strncpy(nid.szTip, (LPCTSTR) tip, sizeof(nid.szTip));
        nid.szTip[sizeof(nid.szTip)-1] = '\0';
        Shell_NotifyIcon (nim, &nid);
    }

    if ( nim == NIM_ADD )
        m_bIconAdded = TRUE;
    if ( nim == NIM_DELETE )
        m_bIconDeleted = TRUE;
}

void CLeashView::SetTrayIcon(int nim, int state)
{
    static HICON hIcon[IMAGE_COUNT];
    static BOOL bIconInit = FALSE;

    if ( (nim == NIM_MODIFY) && (m_bIconDeleted) )
        return;
    if ( (nim == NIM_MODIFY) && (!m_bIconAdded) )
        nim = NIM_ADD;

    if ( (nim != NIM_DELETE) || IsWindow(m_hWnd) )
    {
        if ( !bIconInit ) {
            // The state is reported as the parent node value although
            // we want to use the Tray Version of the icons
            hIcon[ACTIVE_PARENT_NODE] = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_GOOD);
            hIcon[LOW_PARENT_NODE] = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_LOW);
            hIcon[EXPIRED_PARENT_NODE] = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_EXPIRED);
            hIcon[NONE_PARENT_NODE]  = AfxGetApp()->LoadIcon(IDI_LEASH_TRAY_NONE);
            bIconInit = TRUE;
        }

        NOTIFYICONDATA nid;
        memset (&nid, 0x00, sizeof(NOTIFYICONDATA));
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = m_hWnd;
        nid.uID = 0;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = hIcon[state];
        Shell_NotifyIcon (nim, &nid);
    }

    if ( nim == NIM_ADD )
        m_bIconAdded = TRUE;
    if ( nim == NIM_DELETE )
        m_bIconDeleted = TRUE;
}

BOOL CLeashView::PostWarningMessage(const CString& message)
{
    if (m_pWarningMessage)
    {
        return FALSE; // can't post more than one warning at a time
    }
    m_pWarningMessage = new CString(message);
    PostMessage(WM_WARNINGPOPUP);
    return TRUE;
}

LRESULT CLeashView::OnWarningPopup(WPARAM wParam, LPARAM lParam)
{
    CLeashMessageBox leashMessageBox(CMainFrame::m_isMinimum ? GetDesktopWindow() : NULL,
                                        *m_pWarningMessage, 100000);
    leashMessageBox.DoModal();
    delete m_pWarningMessage;
    m_pWarningMessage = NULL;
    return 0L;
}

BOOL CLeashView::PreTranslateMessage(MSG* pMsg)
{
	if ( pMsg->message == ID_OBTAIN_TGT_WITH_LPARAM )
	{
		OutputDebugString("Obtain TGT with LParam\n");
	}

    if ( pMsg->message == WM_TIMER ) {
        try {
        if (InterlockedDecrement(&m_timerMsgNotInProgress) == 0) {

            CString ticketStatusKrb5 = TCHAR(NOT_INSTALLED);
            CString strTimeDate;
            CString lowTicketWarningKrb5;

          timer_start:
            if (WaitForSingleObject( ticketinfo.lockObj, 100 ) != WAIT_OBJECT_0)
                throw("Unable to lock ticketinfo");
            if (CLeashApp::m_hKrb5DLL)
            {
                // KRB5
                UpdateTicketTime(ticketinfo.Krb5);

                if (!ticketinfo.Krb5.btickets)
                {
                    ticketStatusKrb5 = "Kerb-5: No Tickets";
                }
                else if (EXPIRED_TICKETS == ticketinfo.Krb5.btickets)
                {
                    ticketStatusKrb5 = "Kerb-5: Expired Ticket(s)";
                    m_ticketTimeLeft = 0;
                    lowTicketWarningKrb5 = "Your Kerberos Five ticket(s) have expired";
                    if (!m_warningOfTicketTimeLeftLockKrb5)
                        m_warningOfTicketTimeLeftKrb5 = 0;
                    m_warningOfTicketTimeLeftLockKrb5 = ZERO_MINUTES_LEFT;
                }
                else
                {
                    m_ticketStatusKrb5 = GetLowTicketStatus(5);
                    switch (m_ticketStatusKrb5)
                    {
                    case TWENTY_MINUTES_LEFT:
                        break;
                    case FIFTEEN_MINUTES_LEFT:
                        ticketinfo.Krb5.btickets = TICKETS_LOW;
                        lowTicketWarningKrb5 = "Less then 15 minutes left on your Kerberos Five ticket(s)";
                        break;
                    case TEN_MINUTES_LEFT:
                        ticketinfo.Krb5.btickets = TICKETS_LOW;
                        lowTicketWarningKrb5 = "Less then 10 minutes left on your Kerberos Five ticket(s)";
                        if (!m_warningOfTicketTimeLeftLockKrb5)
                            m_warningOfTicketTimeLeftKrb5 = 0;
                        m_warningOfTicketTimeLeftLockKrb5 = TEN_MINUTES_LEFT;
                        break;
                    case FIVE_MINUTES_LEFT:
                        ticketinfo.Krb5.btickets = TICKETS_LOW;
                        if (m_warningOfTicketTimeLeftLockKrb5 == TEN_MINUTES_LEFT)
                            m_warningOfTicketTimeLeftKrb5 = 0;
                        m_warningOfTicketTimeLeftLockKrb5 = FIVE_MINUTES_LEFT;
                        lowTicketWarningKrb5 = "Less then 5 minutes left on your Kerberos Five ticket(s)";
                        break;
                    default:
                        m_ticketStatusKrb5 = 0;
                        break;
                    }
                }

                if (CMainFrame::m_isMinimum)
                {
                    // minimized display
                    ticketStatusKrb5.Format("Kerb-5: %02d:%02d Left",
                                             (m_ticketTimeLeft / 60L / 60L),
                                             (m_ticketTimeLeft / 60L % 60L));
                }
                else
                {
                    // normal display
                    if (GOOD_TICKETS == ticketinfo.Krb5.btickets || TICKETS_LOW == ticketinfo.Krb5.btickets)
                    {
                        if ( m_ticketTimeLeft >= 60 ) {
                            ticketStatusKrb5.Format("Kerb-5 Ticket Life: %02d:%02d",
                                                     (m_ticketTimeLeft / 60L / 60L),
                                                     (m_ticketTimeLeft / 60L % 60L));
                        } else {
                            ticketStatusKrb5.Format("Kerb-5 Ticket Life: < 1 min");
                        }
                    }
#ifndef NO_STATUS_BAR
                    if (CMainFrame::m_wndStatusBar)
                    {
                        CMainFrame::m_wndStatusBar.SetPaneInfo(1, 111112, SBPS_NORMAL, 130);
                        CMainFrame::m_wndStatusBar.SetPaneText(1, ticketStatusKrb5, SBT_POPOUT);
                    }
#endif
                }
            }
            else
            {
                // not installed
                ticketStatusKrb5.Format("Kerb-5: Not Available");
#ifndef NO_STATUS_BAR
                if (CMainFrame::m_wndStatusBar)
                {
                    CMainFrame::m_wndStatusBar.SetPaneInfo(1, 111112, SBPS_NORMAL, 130);
                    CMainFrame::m_wndStatusBar.SetPaneText(1, ticketStatusKrb5, SBT_POPOUT);
                }
#endif
            }
            //KRB5

            if ( m_ticketStatusKrb5 == TWENTY_MINUTES_LEFT &&
                 m_autoRenewTickets && !m_autoRenewalAttempted && ticketinfo.Krb5.renew_until &&
                 (ticketinfo.Krb5.renew_until - LeashTime() > 20 * 60))
            {
                m_autoRenewalAttempted = 1;
                ReleaseMutex(ticketinfo.lockObj);
                AfxBeginThread(RenewTicket,m_hWnd);
                goto timer_start;
            }

            BOOL warningKrb5 = m_ticketStatusKrb5 > NO_TICKETS &&
                m_ticketStatusKrb5 < TWENTY_MINUTES_LEFT &&
                    !m_warningOfTicketTimeLeftKrb5;

            // Play warning message only once per each case statement above
            if (warningKrb5)
            {

                CString lowTicketWarning = "";
                int warnings = 0;

                if (warningKrb5) {
                    lowTicketWarning += lowTicketWarningKrb5;
                    m_warningOfTicketTimeLeftKrb5 = ON;
                    warnings++;
                }

                ReleaseMutex(ticketinfo.lockObj);
                AlarmBeep();
                PostWarningMessage(lowTicketWarning);
                if (WaitForSingleObject( ticketinfo.lockObj, 100 ) != WAIT_OBJECT_0)
                    throw("Unable to lock ticketinfo");
            }

            CTime tTimeDate = CTime::GetCurrentTime();

            if (CMainFrame::m_isMinimum)
            {
	      strTimeDate = ( "MIT Kerberos - "
			      "[" + ticketStatusKrb5 + "] - " +
			      "[" + ticketinfo.Krb5.principal + "]" + " - " +
			      tTimeDate.Format("%A, %B %d, %Y  %H:%M "));
            }
            else
            {
                strTimeDate = ("MIT Kerberos - " +
                                tTimeDate.Format("%A, %B %d, %Y  %H:%M ")
                                //timeDate.Format("%d %b %y %H:%M:%S - ")
                                );
            }
            ::SetWindowText(CLeashApp::m_hProgram, strTimeDate);

            if (CLeashApp::m_hKrb5DLL) {
                if ( ticketinfo.Krb5.btickets )
                    strTimeDate = ( "MIT Kerberos: "
                                    "[" + ticketStatusKrb5 + "]" +
                                    " - [" + ticketinfo.Krb5.principal + "]");
                else
                    strTimeDate = "MIT Kerberos: No Tickets";
            }
            ReleaseMutex(ticketinfo.lockObj);

            SetTrayText(NIM_MODIFY, strTimeDate);

            m_updateDisplayCount++;
            m_alreadyPlayedDisplayCount++;
        }
        } catch (...) {
        }
        InterlockedIncrement(&m_timerMsgNotInProgress);
    }  // WM_TIMER


    if (UPDATE_DISPLAY_TIME == m_updateDisplayCount)
    {
        m_updateDisplayCount = 0;
        SendMessage(WM_COMMAND, ID_UPDATE_DISPLAY, 0);
    }

    if (m_alreadyPlayedDisplayCount > 2)
    {
        m_alreadyPlayedDisplayCount = 0;
        m_alreadyPlayed = FALSE;
    }

    if (CMainFrame::m_isBeingResized)
    {
        m_startup = FALSE;

        UpdateWindow();

        CMainFrame::m_isBeingResized = FALSE;
    }

	if (::IsWindow(pMsg->hwnd))
		return CListView::PreTranslateMessage(pMsg);
	else
		return FALSE;
}

VOID CLeashView::OnLowTicketAlarm()
{
    m_lowTicketAlarm = !m_lowTicketAlarm;

    if (m_pApp)
        m_pApp->WriteProfileInt("Settings", "LowTicketAlarm", m_lowTicketAlarm);
}

VOID CLeashView::OnUpdateLowTicketAlarm(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_lowTicketAlarm);
}

VOID CLeashView::OnAutoRenew()
{
    m_autoRenewTickets = !m_autoRenewTickets;

    if (m_pApp)
        m_pApp->WriteProfileInt("Settings", "AutoRenewTickets", m_autoRenewTickets);

    m_autoRenewalAttempted = 0;
}

VOID CLeashView::OnUpdateAutoRenew(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_autoRenewTickets);
}

VOID CLeashView::OnUpdateMakeDefault(CCmdUI* pCmdUI)
{
    // enable if exactly one principal is selected and that principal is not
    // the default principal
    BOOL enable = FALSE;
    CCacheDisplayData *elem = m_ccacheDisplay;
    while (elem != NULL) {
        if (elem->m_selected) {
            if (enable) {
                // multiple selection; disable button
                enable = FALSE;
                break;
            }
            if (elem->m_isDefault)
                break;

            enable = TRUE;
        }
        elem = elem->m_next;
    }
    pCmdUI->Enable(enable);
}

VOID CLeashView::AlarmBeep()
{
	if (m_lowTicketAlarmSound)
	{
		::Beep(2000, 200);
		::Beep(200, 200);
		::Beep(700, 200);
	}
}

VOID CLeashView::OnUpdateProperties(CCmdUI* pCmdUI)
{
    if (CLeashApp::m_hKrb5DLL)
        pCmdUI->Enable();
    else
        pCmdUI->Enable(FALSE);
}

void CLeashView::OnHelpLeash32()
{
#ifdef CALL_HTMLHELP
	AfxGetApp()->HtmlHelp(HID_LEASH_PROGRAM);
#else
    AfxGetApp()->WinHelp(HID_LEASH_PROGRAM);
#endif
}

void CLeashView::OnHelpKerberos()
{
#ifdef CALL_HTMLHELP
    AfxGetApp()->HtmlHelp(HID_ABOUT_KERBEROS);
#else
    AfxGetApp()->WinHelp(HID_ABOUT_KERBEROS);
#endif
}

void CLeashView::OnHelpWhyuseleash32()
{
#ifdef CALL_HTMLHELP
    AfxGetApp()->HtmlHelp(HID_WHY_USE_LEASH32);
#else
    AfxGetApp()->WinHelp(HID_WHY_USE_LEASH32);
#endif
}

void CLeashView::OnSysColorChange()
{
    change_icon_size = FALSE;
    CWnd::OnSysColorChange();
    OnLargeIcons();
    m_imageList.SetBkColor(GetSysColor(COLOR_WINDOW));
    change_icon_size = TRUE;
}


LRESULT
CLeashView::OnObtainTGTWithParam(WPARAM wParam, LPARAM lParam)
{
    LRESULT res = 0;
    char *param = 0;
    LSH_DLGINFO_EX ldi;
    ldi.size = sizeof(ldi);
    ldi.dlgtype = DLGTYPE_PASSWD;
    ldi.use_defaults = 1;
    ldi.title = ldi.in.title;
    ldi.username = ldi.in.username;
    ldi.realm = ldi.in.realm;

    if (lParam)
        param = (char *) MapViewOfFile((HANDLE)lParam,
                                       FILE_MAP_ALL_ACCESS,
                                       0,
                                       0,
                                       4096);

    if ( param ) {
        if ( *param )
            strcpy_s(ldi.in.title,param);
        param += strlen(param) + 1;
        if ( *param )
            strcpy_s(ldi.in.username,param);
        param += strlen(param) + 1;
        if ( *param )
            strcpy_s(ldi.in.realm,param);
        param += strlen(param) + 1;
	if ( *param )
	    strcpy_s(ldi.in.ccache,param);
    } else {
        strcpy_s(ldi.in.title, "MIT Kerberos: Get Ticket");
    }

    if (strlen(ldi.username) > 0 && strlen(ldi.realm) > 0)
        ldi.dlgtype |= DLGFLAG_READONLYPRINC;

    res = pLeash_kinit_dlg_ex(m_hWnd, &ldi);
    if (param)
        UnmapViewOfFile(param);
    if (lParam)
        CloseHandle((HANDLE )lParam);
    ::SendMessage(m_hWnd, WM_COMMAND, ID_UPDATE_DISPLAY, 0);
    return res;
}


// Find the CCacheDisplayData corresponding to the specified item, if it exists
static CCacheDisplayData *
FindCCacheDisplayData(int item, CCacheDisplayData *elem)
{
    while (elem != NULL) {
        if (elem->m_index == item)
            break;
        elem = elem->m_next;
    }
    return elem;
}


void CLeashView::OnLvnItemActivate(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    // TODO: Add your control notification handler code here
    CCacheDisplayData *elem = FindCCacheDisplayData(pNMIA->iItem,
                                                    m_ccacheDisplay);
    if (elem != NULL) {
        elem->m_expanded = !elem->m_expanded;
        OnUpdateDisplay();
    }
    *pResult = 0;
}


void CLeashView::OnLvnKeydown(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
    int expand = -1; // -1 = unchanged; 0 = collapse; 1 = expand
    switch (pLVKeyDow->wVKey) {
    case VK_RIGHT:
        // expand focus item
        expand = 1;
        break;
    case VK_LEFT:
        // collapse focus item
        expand = 0;
        break;
    default:
        break;
    }
    if (expand >= 0) {
        int focusedItem = GetListCtrl().GetNextItem(-1, LVNI_FOCUSED);
        if (focusedItem >= 0) {
            CCacheDisplayData *elem = FindCCacheDisplayData(focusedItem,
                                                            m_ccacheDisplay);
            if (elem != NULL) {
                if (elem->m_expanded != expand) {
                    elem->m_expanded = expand;
                    OnUpdateDisplay();
                }
            }
        }
    }
    *pResult = 0;
}

void CLeashView::OnLvnItemchanging(NMHDR *pNMHDR, LRESULT *pResult)
{
    CCacheDisplayData *elem;
    LRESULT result = 0;
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: Add your control notification handler code here
    if ((pNMLV->uNewState ^ pNMLV->uOldState) & LVIS_SELECTED) {
        // selection state changing
        elem = FindCCacheDisplayData(pNMLV->iItem, m_ccacheDisplay);
        if (elem == NULL) {
            // this is an individual ticket, not a cache, so prevent selection
            if (pNMLV->uNewState & LVIS_SELECTED) {
                unsigned int newState =  pNMLV->uNewState & ~LVIS_SELECTED;
                result = 1; // suppress changes
                if (newState != pNMLV->uOldState) {
                    // but need to make other remaining changes still
                    GetListCtrl().SetItemState(pNMLV->iItem, newState,
                                               newState ^ pNMLV->uOldState);
                }
            }
        } else {
            elem->m_selected = (pNMLV->uNewState & LVIS_SELECTED) ? 1 : 0;
        }
    }
    *pResult = result;
}

HFONT CLeashView::GetSubItemFont(int iItem, int iSubItem)
{
    HFONT retval = m_BaseFont;
    int iColumn, columnSubItem = 0;

    // Translate subitem to column index
    for (iColumn = 0; iColumn < NUM_VIEW_COLUMNS; iColumn++) {
        if (sm_viewColumns[iColumn].m_enabled) {
            if (columnSubItem == iSubItem)
                break;
            else
                columnSubItem++;
        }
    }
    switch (iColumn) {
    case RENEWABLE_UNTIL:
    case VALID_UNTIL:
        retval = m_aListItemInfo[iItem].m_durationFont;
        break;
    default:
        retval = m_aListItemInfo[iItem].m_font;
        break;
    }
    return retval;
}

void CLeashView::OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult)
{
    HFONT font;
    CCacheDisplayData *pElem;
    *pResult = CDRF_DODEFAULT;
    int iItem;

    LPNMLVCUSTOMDRAW pNMLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    switch (pNMLVCD->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;
    case CDDS_ITEMPREPAINT:
        *pResult = CDRF_NOTIFYSUBITEMDRAW;
        break;
    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
        iItem = pNMLVCD->nmcd.dwItemSpec;
        pElem = FindCCacheDisplayElem(m_ccacheDisplay, iItem);
        font = GetSubItemFont(iItem, pNMLVCD->iSubItem);
        SelectObject(pNMLVCD->nmcd.hdc, font);
        if (pElem != NULL && pNMLVCD->iSubItem == 0) {
            CListCtrl &list = GetListCtrl();
            CRect drawRect, nextRect;
            if (list.GetSubItemRect(iItem, 0, LVIR_BOUNDS, drawRect)) {
                HTHEME hTheme = OpenThemeData(pNMLVCD->nmcd.hdr.hwndFrom,
                                              L"Explorer::TreeView");
                drawRect.right = drawRect.left +
                                 (drawRect.bottom - drawRect.top);
                // @TODO: need hot states, too: TVP_HOTGLYPH, HGLPS_OPENED,
                //        HGLPS_CLOSED
                int state = pElem->m_expanded ? GLPS_OPENED : GLPS_CLOSED;
                DrawThemeBackground(hTheme,
                                    pNMLVCD->nmcd.hdc,
                                    TVP_GLYPH, state,
                                    &drawRect, NULL);
            }
        }
        *pResult = CDRF_NEWFONT;
        break;
    default:
        break;
    }
}
