//	**************************************************************************************
//	File:			LeashFrame.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for LeashFrame.h. Contains variables and functions
//					for Leash
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "LeashFrame.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif
///////////////////////////////////////////////////////////////
// CLeashFrame

const CRect CLeashFrame::s_rectDefault(0, 0, 740, 400);  // static public (l,t,r,b)
const char CLeashFrame::s_profileHeading[] = "Window size";
const char CLeashFrame::s_profileRect[] = "Rect";
const char CLeashFrame::s_profileIcon[] = "icon";
const char CLeashFrame::s_profileMax[] = "max";
const char CLeashFrame::s_profileTool[] = "tool";
const char CLeashFrame::s_profileStatus[] = "status";

IMPLEMENT_DYNAMIC(CLeashFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CLeashFrame, CFrameWndEx)
	//{{AFX_MSG_MAP(CLeashFrame)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////
CLeashFrame::CLeashFrame()
{
    m_bFirstTime = TRUE;
}

///////////////////////////////////////////////////////////////
CLeashFrame::~CLeashFrame()
{
}

///////////////////////////////////////////////////////////////
void CLeashFrame::OnDestroy()
{
	CString strText;
	BOOL bIconic, bMaximized;

	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	// gets current window position and
	//  iconized/maximized status
	BOOL bRet = GetWindowPlacement(&wndpl);
	if (wndpl.showCmd == SW_SHOWNORMAL)
	{
		bIconic = FALSE;
		bMaximized = FALSE;
	}
	else if (wndpl.showCmd == SW_SHOWMAXIMIZED)
	{
		bIconic = FALSE;
		bMaximized = TRUE;
	}
	else if (wndpl.showCmd == SW_SHOWMINIMIZED)
	{
		bIconic = TRUE;
		if (wndpl.flags)
		{
			bMaximized = TRUE;
		}
		else
		{
			bMaximized = FALSE;
		}
	}

	strText.Format("%04d %04d %04d %04d",
	               wndpl.rcNormalPosition.left,
	               wndpl.rcNormalPosition.top,
	               wndpl.rcNormalPosition.right,
	               wndpl.rcNormalPosition.bottom);

	AfxGetApp()->WriteProfileString(s_profileHeading,
	                                s_profileRect, strText);

	AfxGetApp()->WriteProfileInt(s_profileHeading,
	                             s_profileIcon, bIconic);

	AfxGetApp()->WriteProfileInt(s_profileHeading,
	                             s_profileMax, bMaximized);

	SaveBarState(AfxGetApp()->m_pszProfileName);

	CFrameWndEx::OnDestroy();
}

///////////////////////////////////////////////////////////////
void CLeashFrame::ActivateFrame(int nCmdShow)
{

    if (m_bFirstTime)
	{
		m_bFirstTime = FALSE;

    }

	CFrameWndEx::ActivateFrame(nCmdShow);
}
