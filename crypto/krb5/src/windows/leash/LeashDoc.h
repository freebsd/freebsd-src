//	**************************************************************************************
//	File:			LeashDoc.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    H file for LeashDoc.cpp. Contains variables and functions
//					for Leash
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_LeashDOC_H__6F45AD97_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
#define AFX_LeashDOC_H__6F45AD97_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000


class LeashDoc : public CDocument
{
protected: // create from serialization only
	LeashDoc();
	DECLARE_DYNCREATE(LeashDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LeashDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~LeashDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(LeashDoc)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LeashDOC_H__6F45AD97_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
