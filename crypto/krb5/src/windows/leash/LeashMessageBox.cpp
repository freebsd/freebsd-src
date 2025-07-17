//	**************************************************************************************
//	File:			LeashMessageBox.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    CPP file for LeashMessageBox.h. Contains variables and functions
//					for the Leash Special Message Dialog Box
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************



#include "stdafx.h"
#include "leash.h"
#include "LeashMessageBox.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

DWORD CLeashMessageBox ::m_dwTime;

/////////////////////////////////////////////////////////////////////////////
// CLeashMessageBox dialog

CLeashMessageBox::CLeashMessageBox(CWnd* pParent, const CString msgText, DWORD dwTime)
	: CDialog(CLeashMessageBox::IDD, pParent)
{
	m_dwTime = dwTime;

	//{{AFX_DATA_INIT(CLeashMessageBox)
	m_messageText = _T(msgText);
	//}}AFX_DATA_INIT
}

CLeashMessageBox::~CLeashMessageBox()
{
}

void CLeashMessageBox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLeashMessageBox)
	DDX_Text(pDX, IDC_LEASH_WARNING_MSG, m_messageText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLeashMessageBox, CDialog)
	//{{AFX_MSG_MAP(CLeashMessageBox)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLeashMessageBox message handlers

void CALLBACK CLeashMessageBox::MessageBoxTimer(HWND hwnd, UINT uiMsg, UINT_PTR idEvent, DWORD dwTime)
{
	::KillTimer(hwnd, 2);
	::SendMessage(hwnd, WM_CLOSE, 0, 0);
}

void CLeashMessageBox::OnOK()
{
	KillTimer(2);
    SendMessage(WM_CLOSE, 0, 0);
}

BOOL CLeashMessageBox::OnInitDialog()
{
	CDialog::OnInitDialog();
	UINT_PTR idTimer = SetTimer(2, m_dwTime, &MessageBoxTimer);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
