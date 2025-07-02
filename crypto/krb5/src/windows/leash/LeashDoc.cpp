//	**************************************************************************************
//	File:			LeashDoc.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    CPP file for LeashDoc.h. Contains variables and functions
//					for Leash
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "Leash.h"

#include "LeashDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// LeashDoc

IMPLEMENT_DYNCREATE(LeashDoc, CDocument)

BEGIN_MESSAGE_MAP(LeashDoc, CDocument)
	//{{AFX_MSG_MAP(LeashDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LeashDoc construction/destruction

LeashDoc::LeashDoc()
{
	// TODO: add one-time construction code here

}

LeashDoc::~LeashDoc()
{
}

BOOL LeashDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// LeashDoc serialization

void LeashDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// LeashDoc diagnostics

#ifdef _DEBUG
void LeashDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void LeashDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// LeashDoc commands
