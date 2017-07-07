//	**************************************************************************************
//	File:			LeashView.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for LeashView.cpp. Contains variables and functions
//					for the Leash FormView
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_LeashVIEW_H__6F45AD99_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
#define AFX_LeashVIEW_H__6F45AD99_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define GOOD_TICKETS	1  // Don't change this value
#define EXPIRED_TICKETS 2  // Don't change this value
#define TICKETS_LOW		3
#define ONE_SECOND		1000
#define SMALL_ICONS     16
#define LARGE_ICONS     32

#define UPDATE_DISPLAY_TIME 60  //seconds

#define ACTIVE_CLOCK          0
#define LOW_CLOCK             1
#define EXPIRED_CLOCK         2
#define ACTIVE_TICKET         3
#define LOW_TICKET            4
#define EXPIRED_TICKET        5
#define TICKET_NOT_INSTALLED  6
#define ACTIVE_PARENT_NODE    7
#define LOW_PARENT_NODE       8
#define EXPIRED_PARENT_NODE   9
#define NONE_PARENT_NODE      10
#define LOW_TRAY_ICON         11
#define EXPIRED_TRAY_ICON     12
#define ACTIVE_TRAY_ICON      13
#define NONE_TRAY_ICON        14
#define TKT_ADDRESS           15
#define TKT_SESSION           16
#define TKT_ENCRYPTION        17
#define IMAGE_COUNT           18

#define NODE_IS_EXPANDED 2

#define CX_BORDER   1
#define CY_BORDER   1

#ifdef NO_TICKETS
#undef NO_TICKETS // XXX - this is evil but necessary thanks to silliness...
#endif

#define WM_TRAYICON (WM_USER+100)
#define WM_WARNINGPOPUP (WM_USER+101)

enum ticketTimeLeft{NO_TICKETS, ZERO_MINUTES_LEFT, FIVE_MINUTES_LEFT, TEN_MINUTES_LEFT,
					FIFTEEN_MINUTES_LEFT, TWENTY_MINUTES_LEFT, PLENTY_OF_TIME,
                    NOT_INSTALLED};
// Don't change 'NO_TICKET's' value

class CLeashDebugWindow;
class ViewColumnInfo
{
public:
    const char * m_name;
    int m_enabled;
    int m_id;
    int m_columnWidth;
};

enum eViewColumn {
    PRINCIPAL,
    TIME_ISSUED,
    RENEWABLE_UNTIL,
    VALID_UNTIL,
    ENCRYPTION_TYPE,
    TICKET_FLAGS,
    CACHE_NAME,
    NUM_VIEW_COLUMNS
};

class CCacheDisplayData
{
public:
    CCacheDisplayData(const char *ccache_name) :
      m_next(NULL),
      m_ccacheName(strdup(ccache_name)),
      m_index(-1),
      m_focus(-1),
      m_expanded(0),
      m_selected(0),
      m_isRenewable(0),
      m_isDefault(0)
    {
    }

    ~CCacheDisplayData()
    {
        if (m_ccacheName)
            free(m_ccacheName);
    }

    CCacheDisplayData *m_next;
    char *m_ccacheName;
    int m_index;               // item index in list view
    int m_focus;               // sub-item with focus
    unsigned int m_expanded;   // true when each individual ticket is displayed
    unsigned int m_selected;   // true when this ccache is selected
    unsigned int m_isRenewable; // true when tgt is renewable
    unsigned int m_isDefault;  // true when this is the default ccache
};

struct ListItemInfo
{
    ListItemInfo() : m_font(NULL), m_durationFont(NULL) {}
    HFONT m_durationFont; // For renewable/valid until; italic when expired
    HFONT m_font;         // For all other items
};

class CLeashView : public CListView
{
private:
////@#+Remove
#ifndef NO_KRB4
    TicketList*         m_listKrb4;
#endif
    TicketList*         m_listAfs;
    CLeashDebugWindow*	m_pDebugWindow;
    CCacheDisplayData*  m_ccacheDisplay;
	CImageList			m_imageList;
	CWinApp*			m_pApp;
	HTREEITEM			m_hPrincipal;
////@#+Remove
#ifndef NO_KRB4
	HTREEITEM			m_hKerb4;
#endif
	HTREEITEM			m_hKerb5;
    HTREEITEM           m_hk5tkt;
	HTREEITEM			m_hAFS;
	TV_INSERTSTRUCT		m_tvinsert;
	HMENU				m_hMenu;
    BOOL				m_startup;
	BOOL				m_isMinimum;
	BOOL				m_debugStartUp;
	BOOL				m_alreadyPlayed;
    INT					m_upperCaseRealm;
	INT					m_destroyTicketsOnExit;
	INT					m_debugWindow;
	INT					m_largeIcons;
	INT					m_lowTicketAlarm;
	INT					m_hPrincipalState;
#ifndef NO_KRB4
	INT					m_hKerb4State;
#endif
	INT					m_hKerb5State;
	INT					m_hAFSState;
    CString*            m_pWarningMessage;
    BOOL                m_bIconAdded;
    BOOL                m_bIconDeleted;
    HFONT               m_BaseFont;
    HFONT               m_BoldFont;
    HFONT               m_ItalicFont;
    HFONT               m_BoldItalicFont;
    ListItemInfo*       m_aListItemInfo;

    static ViewColumnInfo sm_viewColumns[NUM_VIEW_COLUMNS];

    static INT		   	m_autoRenewTickets;
    static INT          m_ticketStatusAfs;
////Remove as well?
    static INT          m_ticketStatusKrb4;
    static INT          m_ticketStatusKrb5;
    static INT          m_autoRenewalAttempted;
	static INT			m_warningOfTicketTimeLeftAfs;
////Remove as well?
	static INT			m_warningOfTicketTimeLeftKrb4;
	static INT			m_warningOfTicketTimeLeftKrb5;
    static INT			m_warningOfTicketTimeLeftLockAfs;
////Remove as well?
    static INT			m_warningOfTicketTimeLeftLockKrb4;
    static INT			m_warningOfTicketTimeLeftLockKrb5;
    static INT			m_updateDisplayCount;
    static INT	        m_alreadyPlayedDisplayCount;
    static time_t		m_ticketTimeLeft;
    static BOOL			m_lowTicketAlarmSound;
    static LONG         m_timerMsgNotInProgress;

    void ToggleViewColumn(eViewColumn viewOption);
	VOID ResetTreeNodes();
    VOID ApplicationInfoMissingMsg();
    VOID GetScrollBarState(CSize sizeClient, CSize& needSb,
	                       CSize& sizeRange, CPoint& ptMove,
                           BOOL bInsideClient);
    VOID UpdateBars();
    VOID GetScrollBarSizes(CSize& sizeSb);
    BOOL GetTrueClientSize(CSize& size, CSize& sizeSb);
    HFONT GetSubItemFont(int iItem, int iSubItem);

    //void   GetRowWidthHeight(CDC* pDC, LPCSTR theString, int& nRowWidth,
    //                         int& nRowHeight, int& nCharWidth);
    static VOID	AlarmBeep();
    static VOID	CALLBACK EXPORT TimerProc(HWND hWnd, UINT nMsg, UINT_PTR nIDEvent,
					  DWORD dwTime);
    static VOID	UpdateTicketTime(TICKETINFO& ticketinfo);
    static INT	GetLowTicketStatus(int);
    static time_t	LeashTime();
    static BOOL IsExpired(TicketList *ticket);
    static BOOL IsExpired(TICKETINFO *info);
    static VOID AddDisplayItem(CListCtrl &list,
                               CCacheDisplayData *elem,
                               int iItem,
                               char *principal,
                               long issued,
                               long valid_until,
                               long renew_until,
                               char *encTypes,
                               unsigned long flags,
                               char *cache_name);

    void   SetTrayIcon(int nim, int state=0);
    void   SetTrayText(int nim, CString tip);

    BOOL   UpdateDisplay();
    static UINT InitTicket(void *);
    static UINT RenewTicket(void *);
    static UINT ImportTicket(void *);
    // Queue a warning popup message.
    // This is a workaround to the MFC deficiency that you cannot safely create
    // a modal dialog while processing messages within AfxPreTranslateMessage()
    // returns TRUE if message is queued successfully.
    BOOL PostWarningMessage(const CString& message);
    afx_msg LRESULT OnWarningPopup(WPARAM wParam, LPARAM lParam);

    BOOL    IsExpanded(TICKETINFO *);

protected: // create from serialization only
	DECLARE_DYNCREATE(CLeashView)

// Attributes
public:
	static INT   m_forwardableTicket;
	static INT   m_proxiableTicket;
    static INT   m_renewableTicket;
    static INT   m_noaddressTicket;
    static DWORD m_publicIPAddress;
    static BOOL  m_importedTickets;

    CLeashView();
	//LeashDoc* GetDocument();

	//{{AFX_DATA(CLeashView)
	enum { IDD = IDD_DIALOG1 };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLeashView)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual VOID OnInitialUpdate();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual VOID OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CLeashView();

#ifdef _DEBUG
	virtual VOID AssertValid() const;
	virtual VOID Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CLeashView)
    afx_msg VOID OnItemexpandedTreeview(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg INT OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg VOID OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg VOID OnClose(void);
	afx_msg VOID OnInitTicket();
	afx_msg VOID OnRenewTicket();
	afx_msg VOID OnImportTicket();
	afx_msg VOID OnDestroyTicket();
	afx_msg VOID OnMakeDefault();
	afx_msg VOID OnChangePassword();
	afx_msg VOID OnUpdateDisplay();
	afx_msg VOID OnSynTime();
	afx_msg VOID OnDebugMode();
	afx_msg VOID OnLargeIcons();
	afx_msg VOID OnTimeIssued();
	afx_msg VOID OnValidUntil();
	afx_msg VOID OnRenewableUntil();
	afx_msg VOID OnShowTicketFlags();
	afx_msg VOID OnEncryptionType();
	afx_msg VOID OnCcacheName();
	afx_msg VOID OnUppercaseRealm();
	afx_msg VOID OnKillTixOnExit();
	afx_msg VOID OnDestroy();
	afx_msg VOID OnUpdateDestroyTicket(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateImportTicket(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateInitTicket(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateRenewTicket(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateTimeIssued(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateValidUntil(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateRenewableUntil(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateShowTicketFlags(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateEncryptionType(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateCcacheName(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateUppercaseRealm(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateKillTixOnExit(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateLowTicketAlarm(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateAutoRenew(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateMakeDefault(CCmdUI* pCmdUI);
	afx_msg VOID OnAppAbout();
	afx_msg VOID OnAfsControlPanel();
	afx_msg VOID OnUpdateDebugMode(CCmdUI* pCmdUI);
	afx_msg VOID OnUpdateCfgFiles(CCmdUI* pCmdUI);
	afx_msg VOID OnKrb4Properties();
	afx_msg VOID OnKrb5Properties();
	afx_msg void OnLeashProperties();
	afx_msg void OnLeashRestore();
	afx_msg void OnLeashMinimize();
	afx_msg void OnLowTicketAlarm();
	afx_msg void OnUpdateKrb4Properties(CCmdUI* pCmdUI);
	afx_msg void OnUpdateKrb5Properties(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAfsControlPanel(CCmdUI* pCmdUI);
    afx_msg void OnKrbProperties();
	afx_msg void OnUpdateProperties(CCmdUI* pCmdUI);
	afx_msg void OnHelpKerberos();
	afx_msg void OnHelpLeash32();
	afx_msg void OnHelpWhyuseleash32();
    afx_msg void OnSysColorChange();
    afx_msg void OnAutoRenew();
	afx_msg LRESULT OnGoodbye(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTrayIcon(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnObtainTGTWithParam(WPARAM wParam, LPARAM lParam);
    afx_msg void OnItemChanged(NMHDR* pNmHdr, LRESULT* pResult);
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnLvnItemchanging(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnLvnItemActivate(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnLvnKeydown(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);
};

/*
#ifndef _DEBUG  // debug version in CLeashView.cpp
inline LeashDoc* CLeashView::GetDocument()
   { return (LeashDoc*)m_pDocument; }
#endif
*/

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LeashVIEW_H__6F45AD99_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
