// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#if !defined(AFX_STDAFX_H__6F45AD93_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
#define AFX_STDAFX_H__6F45AD93_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WINVER 0x0600       // Target Vista+
#define _WIN32_WINNT 0x0600 // Target Vista+
#include <afxwin.h>         // MFC core and standard components
#include <afxcview.h>
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#include "htmlhelp.h"

#endif // _AFX_NO_AFXCMN_SUPPORT
#include <afxcontrolbars.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__6F45AD93_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
