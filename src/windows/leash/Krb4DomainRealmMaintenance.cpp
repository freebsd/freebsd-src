//	**************************************************************************************
//	File:			Krb4DomainRealmMaintenance.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for Krb4DomainRealmMaintenance.h. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************

#include "stdafx.h"
#include "leash.h"
#include "KrbProperties.h"
#include "Krb4Properties.h"
#include "Krb4AddToDomainRealmList.h"
#include "Krb4EditDomainRealmList.h"
#include "Krb4DomainRealmMaintenance.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrb4DomainRealmMaintenance dialog


IMPLEMENT_DYNCREATE(CKrb4DomainRealmMaintenance, CPropertyPage)

CKrb4DomainRealmMaintenance::CKrb4DomainRealmMaintenance() :
  CPropertyPage(CKrb4DomainRealmMaintenance ::IDD)
{
	m_defectiveLines = 0;
}

CKrb4DomainRealmMaintenance::~CKrb4DomainRealmMaintenance()
{
}

void CKrb4DomainRealmMaintenance::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4DomainRealmMaintenance)
	DDX_Control(pDX, IDC_LIST_DOMAINREALM, m_realmDomainList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb4DomainRealmMaintenance, CPropertyPage)
	//{{AFX_MSG_MAP(CKrb4DomainRealmMaintenance)
	ON_BN_CLICKED(IDC_BUTTON_REALM_HOST_ADD, OnButtonRealmHostAdd)
	ON_BN_CLICKED(ID_BUTTON_REALM_HOST_REMOVE, OnButtonRealmHostRemove)
	ON_BN_CLICKED(IDC_BUTTON_REALM_HOST_EDIT, OnButtonRealmHostEdit)
	ON_LBN_SELCHANGE(IDC_LIST_DOMAINREALM, OnSelchangeListDomainrealm)
	ON_LBN_DBLCLK(IDC_LIST_DOMAINREALM, OnDblclkListDomainrealm)
	ON_BN_CLICKED(IDC_BUTTON_HOSTMAINT_HELP, OnButtonHostmaintHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrb4DomainRealmMaintenance message handlers

BOOL CKrb4DomainRealmMaintenance::OnApply()
{
	CStdioFile krbrealmCon;
	if (!krbrealmCon.Open(CKrbProperties::m_krbrealmPath, CFile::modeCreate |
														  CFile::modeNoTruncate |
														  CFile::modeReadWrite))
	{
		LeashErrorBox("OnApply::Can't open Configuration File",
					  CKrbProperties::m_krbrealmPath);
		return TRUE;
	}

	memset(lineBuf, '\0', sizeof(lineBuf));
	krbrealmCon.SetLength(0);
	krbrealmCon.WriteString(lineBuf);
	for (INT maxItems = m_realmDomainList.GetCount(), item = 0; item < maxItems; item++)
	{
		memset(lineBuf, '\0', sizeof(lineBuf));
		if (!m_realmDomainList.GetText(item, lineBuf))
          break;

		krbrealmCon.WriteString(lineBuf);
		krbrealmCon.WriteString("\n");
	}

	krbrealmCon.Close();

	return TRUE;
}

BOOL CKrb4DomainRealmMaintenance::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	CStdioFile krbrealmCon;

	if (!krbrealmCon.Open(CKrbProperties::m_krbrealmPath, CFile::modeReadWrite))
	{ // can't find file, so lets set some defaults
		CString defaultStr;
		defaultStr.Format("%s %s", "MIT.EDU", KRB_REALM);
		m_realmDomainList.AddString(defaultStr);
	}
	else
	{
		while (TRUE)
		{
			if (!krbrealmCon.ReadString(lineBuf, sizeof(lineBuf)))
			  break;

			*(lineBuf + strlen(lineBuf) - 1) = 0;

			if (!strchr(lineBuf, ' ') && !strchr(lineBuf, '\t'))
			{ // found a defective line
				m_defectiveLines++;
			}

			if (LB_ERR == m_realmDomainList.AddString(lineBuf))
			{
				LeashErrorBox("OnInitDialog::Can't read Configuration File",
							  CKrbProperties::m_krbrealmPath);
				krbrealmCon.Close();
				return FALSE;
			}
		}

		krbrealmCon.Close();
	}

	m_realmDomainList.SetCurSel(0);

	if (!m_realmDomainList.GetCount())
	{
		GetDlgItem(ID_BUTTON_REALM_HOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_REALM_HOST_EDIT)->EnableWindow(FALSE);
	}

	return TRUE;
}

void CKrb4DomainRealmMaintenance::OnButtonRealmHostAdd()
{
	CKrb4AddToDomainRealmList addToDomainRealmList;
	if (IDOK == addToDomainRealmList.DoModal())
	{
		if (addToDomainRealmList.GetNewRealm().IsEmpty())
		  ASSERT(0);

		CString newLine;
		newLine = addToDomainRealmList.GetNewDomainHost() + " " + addToDomainRealmList.GetNewRealm();

		// We don't want duplicate items in Listbox
		CString ckDups;
		for (INT item = 0; item < m_realmDomainList.GetCount(); item++)
		{
			m_realmDomainList.GetText(item, ckDups);
			if (0 == ckDups.CompareNoCase(newLine))
			{ // found duplicate item in Listbox
				LeashErrorBox("OnButtonRealmHostAdd::Found a Duplicate Item\nCan't add to List",
							  ckDups);
				return;
			}
		}

		m_realmDomainList.InsertString(0, newLine);
		m_realmDomainList.SetCurSel(0);
		SetModified(TRUE);

		if (1 == m_realmDomainList.GetCount())
		{
			GetDlgItem(ID_BUTTON_REALM_HOST_REMOVE)->EnableWindow();GetDlgItem(IDC_BUTTON_REALM_HOST_EDIT)->EnableWindow();
		}
	}
}

void CKrb4DomainRealmMaintenance::OnButtonRealmHostRemove()
{
	if (IDYES != AfxMessageBox("Your about to remove an item from the list!\n\nContinue?",
							   MB_YESNO))
	  return;

	INT curSel = m_realmDomainList.GetCurSel();
	m_realmDomainList.DeleteString(curSel);  // Single Sel Listbox

	if (-1 == m_realmDomainList.SetCurSel(curSel))
	  m_realmDomainList.SetCurSel(curSel - 1);

	if (!m_realmDomainList.GetCount())
	{
		GetDlgItem(ID_BUTTON_REALM_HOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_REALM_HOST_EDIT)->EnableWindow(FALSE);
	}

	SetModified(TRUE);
}

void CKrb4DomainRealmMaintenance::OnButtonRealmHostEdit()
{
	INT selItemIndex = m_realmDomainList.GetCurSel();
	LPSTR pSelItem = new char[m_realmDomainList.GetTextLen(selItemIndex) + 1];
	if (!pSelItem)
	  ASSERT(0);

	CString selItem;
	m_realmDomainList.GetText(selItemIndex, selItem);
	strcpy(pSelItem, selItem);

	CKrb4EditDomainRealmList editDomainRealmList(pSelItem);
	delete [] pSelItem;

	if (IDOK == editDomainRealmList.DoModal())
	{
		CString editedItem = editDomainRealmList.GetEditedItem();
		if (0 != selItem.CompareNoCase(editedItem) &&
			LB_ERR != m_realmDomainList.FindStringExact(-1, editedItem))
		{
			LeashErrorBox("OnButtonRealmHostEdit::Found a Duplicate!\nCan't add to List",
							  editedItem);

			return;
		}

		m_realmDomainList.DeleteString(selItemIndex);
		m_realmDomainList.InsertString(selItemIndex, editDomainRealmList.GetEditedItem());
		m_realmDomainList.SetCurSel(selItemIndex);
		SetModified(TRUE);
	}
}

void CKrb4DomainRealmMaintenance::OnSelchangeListDomainrealm()
{
	//SetModified(TRUE);
}

void CKrb4DomainRealmMaintenance::OnDblclkListDomainrealm()
{
	OnButtonRealmHostEdit();
}

BOOL CKrb4DomainRealmMaintenance::PreTranslateMessage(MSG* pMsg)
{
	if (m_defectiveLines)
	{
		if (m_defectiveLines == 1)
		  LeashErrorBox("Found a defective entry in file",
						CKrbProperties::m_krbrealmPath, "Warning");
	    else if (m_defectiveLines > 1)
	      LeashErrorBox("Found more then one defective entry in file",
						CKrbProperties::m_krbrealmPath, "Warning");
	}

	m_defectiveLines = 0;
	return CPropertyPage::PreTranslateMessage(pMsg);
}




void CKrb4DomainRealmMaintenance::OnButtonHostmaintHelp()
{
	MessageBox("No Help Available!", "Leash", MB_OK);
}
