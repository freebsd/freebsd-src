//	**************************************************************************************
//	File:			Leash.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Leash.cpp. Contains variables and functions
//					for Leash
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_Leash_H__6F45AD91_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
#define AFX_Leash_H__6F45AD91_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

// Help
#define HID_GET_TICKETS_COMMAND			98343 // ID_INIT_TICKET + 65536
#define HID_RENEW_TICKETS_COMMAND       98312 // ID_RENEW_TICKET + 65536
#define HID_DESTROY_TICKETS_COMMAND     98313
#define HID_SYNCHRONIZE_TIME_OPTION     98314
#define HID_CHANGE_PASSWORD_COMMAND		98315
#define HID_UPDATE_DISPLAY_COMMAND      98316
#define HID_DEBUG_WINDOW_OPTION			98317
#define HID_LEASH_PROGRAM               98319
#define HID_ABOUT_KERBEROS              98320
#define HID_LARGE_ICONS_OPTION          98322
#define HID_DESTROY_TICKETS_ON_EXIT		98321
#define HID_UPPERCASE_REALM_OPTION      98323
#define HID_RESET_WINDOW_OPTION			98326
#define HID_KRB5_PROPERTIES_COMMAND		98330
#define HID_LEASH_PROPERTIES_COMMAND	98331
#define HID_LOW_TICKET_ALARM_OPTION		98334
#define HID_KRBCHECK_OPTION				98335
#define HID_KERBEROS_PROPERTIES_COMMAND 98337
#define HID_HELP_CONTENTS               98340
#define HID_WHY_USE_LEASH32				98341

#define HID_ABOUT_LEASH32_COMMAND       123200
#define HID_EXIT_COMMAND                123201
#define HID_TOOLBAR_OPTION				124928
#define HID_STATUS_BAR_OPTION           124929
#define HID_LEASH_COMMANDS              131200
#define HID_ABOUT_LEASH32_MODULES       131225
#define HID_DEBUG_WINDOW				131229
#define HID_KERBEROS_PROPERTIES_EDIT	131233
#define HID_LEASH_PROPERTIES_EDIT		131239
#define HID_KRB5_PROPERTIES_FORWARDING  131240
#define HID_KRB5_PROPERTIES_EDIT	    131241
#define HID_KERBEROS_PROPERTIES_LISTRLM 131250
#define HID_KERBEROS_PROPERTIES_ADDRLM  131253
#define HID_KERBEROS_PROPERTIES_EDITRLM 131254
#define HID_KERBEROS_PROPERTIES_ADDDOM  131255
#define HID_KERBEROS_PROPERTIES_EDITDOM 131256
#define HID_KERBEROS_PROPERTIES_ADDHOST 131269
#define HID_KERBEROS_PROPERTIES_EDITHOST 131271
#define HID_KERBEROS_PROPERTIES_LISTDOM 131279

#define USE_HTMLHELP

#ifdef USE_HTMLHELP
#if _MSC_VER >= 1300
#define CALL_HTMLHELP
#endif
#endif

////Is this a good place for these defines?
#if !defined(MAX_HSTNM)
#define         MAX_HSTNM       100
#endif


#include "resource.h"       // main symbols
#include "lglobals.h"

/////////////////////////////////////////////////////////////////////////////
// CLeashApp:
// See Leash.cpp for the implementation of this class
//

class CLeashApp : public CWinAppEx
{
private:
	CString		m_leashDLL;
	CString		m_krbDLL;
    CString     m_helpFile;
	CString		m_msgError;

	BOOL		InitDLLs();
	BOOL		FirstInstance();

public:
	static HWND			m_hProgram;
	static HINSTANCE	m_hLeashDLL;
	static HINSTANCE	m_hComErr;
////
	static HINSTANCE	m_hKrb5DLL;
	static HINSTANCE	m_hKrb5ProfileDLL;
	static HINSTANCE	m_hPsapi;
	static HINSTANCE	m_hToolHelp32;
	static krb5_context m_krbv5_context;
	static profile_t    m_krbv5_profile;
	static HINSTANCE    m_hKrbLSA;
	static int          m_useRibbon; // temporary while ribbon UI in dev
	static BOOL         m_bUpdateDisplay;

	CLeashApp();
	virtual ~CLeashApp();

    static BOOL  GetProfileFile(LPSTR confname, UINT szConfname);
    static void  ValidateConfigFiles();
    static void  ObtainTicketsViaUserIfNeeded(HWND hWnd);
    static DWORD GetNumOfIpAddrs(void);
    static UINT  IpAddrChangeMonitor(void *);
           DWORD IpAddrChangeMonitorInit(HWND hWnd);
    static BOOL  ProbeKDC(void);
    static UINT  InitWorker(void *);

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLeashApp)
	public:
	virtual BOOL InitInstance();
#ifdef USE_HTMLHELP
#if _MSC_VER < 1300
    virtual void WinHelp(DWORD dwData, UINT nCmd);
#endif
#endif
    //}}AFX_VIRTUAL

    virtual void ParseParam (LPCTSTR lpszParam,BOOL bFlag,BOOL bLast );

  protected:
// Implementation

	//{{AFX_MSG(CLeashApp)
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnIdle(LONG lCount);
};

extern CLeashApp theApp;

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.



#endif // !defined(AFX_Leash_H__6F45AD91_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
