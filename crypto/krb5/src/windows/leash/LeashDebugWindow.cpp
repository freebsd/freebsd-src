//	**************************************************************************************
//	File:			LeashDebugWindow.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for LeashDebugWindow.h. Contains variables and functions
//					for the Leash Debug Window
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************



#include "stdafx.h"
#include "leash.h"
#include "LeashDebugWindow.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLeashDebugWindow dialog


CLeashDebugWindow::CLeashDebugWindow(CWnd* pParent /*=NULL*/)
	: CDialog(CLeashDebugWindow::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLeashDebugWindow)
	//}}AFX_DATA_INIT

	m_pView = NULL;
}

CLeashDebugWindow::CLeashDebugWindow(CFormView* pView)
{
	m_pView = pView;
}

void CLeashDebugWindow::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLeashDebugWindow)
	DDX_Control(pDX, IDC_DEBUG_LISTBOX, m_debugListBox);
	DDX_Control(pDX, IDC_LOG_FILE_LOCATION_TEXT, m_debugFile);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLeashDebugWindow, CDialog)
	//{{AFX_MSG_MAP(CLeashDebugWindow)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_COPY_TO_CLIPBOARD, OnCopyToClipboard)
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLeashDebugWindow message handlers


BOOL CLeashDebugWindow::Create(const LPCSTR debugFilePath)
{
	m_debugFilePath = debugFilePath;
	return CDialog::Create(CLeashDebugWindow::IDD);
}


void CLeashDebugWindow::OnCancel()
{
	if (m_pView != NULL)
	{
		CWinApp* pApp;
		pApp = AfxGetApp();
		pApp->WriteProfileInt("Settings", "DebugWindow", FALSE_FLAG);
		m_pView->PostMessage(WM_GOODBYE, IDCANCEL);	// modeless case
////        pset_krb_debug(OFF);
////	    pset_krb_ap_req_debug(OFF);
    }
	else
	{
		CDialog::OnCancel(); // modal case
	}
}

void CLeashDebugWindow::OnOK()
{
	if (m_pView != NULL)
	{
		// modeless case
		UpdateData(TRUE);
		m_pView->PostMessage(WM_GOODBYE, IDOK);
	}
	else
	{
		CDialog::OnOK(); // modal case
	}
}

BOOL CLeashDebugWindow::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set Debug flags
////	pset_krb_debug(ON); //(int)m_debugListBox.GetSafeHwnd()
////    pset_krb_ap_req_debug(ON);

	if (*m_debugFilePath != 0)
	  SetDlgItemText(IDC_LOG_FILE_LOCATION_TEXT, m_debugFilePath);
    else
	  SetDlgItemText(IDC_LOG_FILE_LOCATION_TEXT, "Not Available");

	if (!m_debugListBox.GetCount())
	  GetDlgItem(IDC_COPY_TO_CLIPBOARD)->EnableWindow(FALSE);

	m_CopyButton = FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CLeashDebugWindow::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
}

void CLeashDebugWindow::OnCopyToClipboard()
{
    if (!OpenClipboard())
	{
        MessageBox("Unable to open Clipboard!", "Error", MB_OK);
		return;
	}

	EmptyClipboard();

    int maxItems = m_debugListBox.GetCount();
	const int MAX_MEM = maxItems * 90; // 90 chars per line seems safe like a safe bet

	HGLOBAL hDebugText = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, MAX_MEM);
    if (NULL != hDebugText)
    {
		CString listboxItem;
		LPSTR pDebugText = (LPSTR) GlobalLock(hDebugText);
		if (!pDebugText)
		{
		    MessageBox("Unable to write to Clipboard!", "Error", MB_OK);
			ASSERT(pDebugText);
			return;
		}

		*pDebugText = 0;
		for (int xItem = 0; xItem < maxItems; xItem++)
		{
			m_debugListBox.GetText(xItem, listboxItem);
			strcat(pDebugText, listboxItem);
			strcat(pDebugText, "\r\n");
		}

		GlobalUnlock(hDebugText);
    }

    if (NULL != hDebugText)
        SetClipboardData(CF_TEXT, hDebugText);

	CloseClipboard();
	MessageBox("Copy to Clipboard was Successful!\r\n Paste it in your favorite editor.",
                "Note", MB_OK);
}

BOOL CLeashDebugWindow::PreTranslateMessage(MSG* pMsg)
{
	if (!m_CopyButton && m_debugListBox.GetCount())
	{
		m_CopyButton = TRUE;
		GetDlgItem(IDC_COPY_TO_CLIPBOARD)->EnableWindow(TRUE);
	}

	return CDialog::PreTranslateMessage(pMsg);
}
