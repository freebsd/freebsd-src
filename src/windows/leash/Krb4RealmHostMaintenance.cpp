//	**************************************************************************************
//	File:			Krb4RealmHostMaintenance.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for Krb4RealmHostMaintenance.h. Contains variables and functions
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
#include "Krb4AddToRealmHostList.h"
#include "Krb4RealmHostMaintenance.h"
#include "Krb4EditRealmHostList.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrb4RealmHostMaintenance dialog


IMPLEMENT_DYNCREATE(CKrb4RealmHostMaintenance, CPropertyPage)

CKrb4RealmHostMaintenance::CKrb4RealmHostMaintenance() : CPropertyPage(CKrb4RealmHostMaintenance::IDD)
{
	m_defectiveLines = 0;
    m_initDnsKdcLookup = m_newDnsKdcLookup = 0;
}

CKrb4RealmHostMaintenance::~CKrb4RealmHostMaintenance()
{
}

void CKrb4RealmHostMaintenance::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4RealmHostMaintenance)
	DDX_Control(pDX, IDC_LIST_KRB4_REALM_HOST, m_RealmHostList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb4RealmHostMaintenance, CPropertyPage)
	//{{AFX_MSG_MAP(CKrb4RealmHostMaintenance)
	ON_BN_CLICKED(IDC_BUTTON_KRB4_REALM_HOST_ADD, OnButtonRealmHostAdd)
	ON_BN_CLICKED(IDC_BUTTON_KRB4_REALM_HOST_EDIT, OnButtonRealmHostEdit)
	ON_BN_CLICKED(ID_BUTTON_KRB4_REALM_HOST_REMOVE, OnButtonRealmHostRemove)
	ON_LBN_SELCHANGE(IDC_LIST_KRB4_REALM_HOST, OnSelchangeListRemoveHost)
	ON_LBN_DBLCLK(IDC_LIST_KRB4_REALM_HOST, OnDblclkListRemoveHost)
	ON_BN_CLICKED(IDC_BUTTON_REALMHOST_MAINT_HELP2, OnButtonRealmhostMaintHelp2)
    ON_BN_CLICKED(IDC_KRB4_DNS_KDC, OnCheckDnsKdcLookup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrb4RealmHostMaintenance message handlers

BOOL CKrb4RealmHostMaintenance::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	CStdioFile krbCon;
	if (!krbCon.Open(CKrbProperties::m_krbPath, CFile::modeReadWrite))
	{ // can't find file, so lets set some defaults

		m_RealmHostList.AddString(KRB_REALM	" " KRB_MASTER);
	}
	else
	{
		memset(lineBuf, '\0', sizeof(lineBuf));
		krbCon.ReadString(lineBuf, sizeof(lineBuf));
		while (TRUE)
		{
			if (!krbCon.ReadString(lineBuf, sizeof(lineBuf)))
			  break;

			*(lineBuf + strlen(lineBuf) - 1) = 0;

			if (!strchr(lineBuf, ' ') && !strchr(lineBuf, '\t'))
			{ // found a defective line
				m_defectiveLines++;
			}

            if ( !strncmp(".KERBEROS.OPTION.",lineBuf,17) ) {
                char * p = &lineBuf[17];
                while (isspace(*p))
                    p++;
                if (!strcmp("dns",p))
                    m_initDnsKdcLookup = m_newDnsKdcLookup = 1;
            } else {
                if (LB_ERR == m_RealmHostList.AddString(lineBuf))
                {
                    LeashErrorBox("OnInitDialog::Can't read Configuration File",
                                   CKrbProperties::m_krbPath);
                    krbCon.Close();
                    return FALSE;
                }
            }
		}

		krbCon.Close();
	}

	m_RealmHostList.SetCurSel(0);

	if (!m_RealmHostList.GetCount())
	{
		GetDlgItem(ID_BUTTON_KRB4_REALM_HOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_KRB4_REALM_HOST_EDIT)->EnableWindow(FALSE);
	}

	return TRUE;
}

BOOL CKrb4RealmHostMaintenance::OnApply()
{
	CStdioFile krbCon;
	if (!krbCon.Open(CKrbProperties::m_krbPath, CFile::modeCreate |
														 CFile::modeNoTruncate |
														 CFile::modeReadWrite))
	{
		LeashErrorBox("OnApply::Can't open Configuration File",
					  CKrbProperties::m_krbPath);
		return TRUE;
	}

	memset(lineBuf, '\0', sizeof(lineBuf));
	if (!krbCon.ReadString(lineBuf, sizeof(lineBuf)))
	{
//-----ADL----///strcpy(lineBuf, CKrb4ConfigOptions::m_newDefaultRealm);
		strcat(lineBuf, "\n");
	}

	krbCon.SetLength(0);
	krbCon.WriteString(lineBuf);
	for (INT maxItems = m_RealmHostList.GetCount(), item = 0; item < maxItems; item++)
	{
		memset(lineBuf, '\0', sizeof(lineBuf));
		if (!m_RealmHostList.GetText(item, lineBuf))
          break;

		krbCon.WriteString(lineBuf);
		krbCon.WriteString("\n");
	}

    if ( m_newDnsKdcLookup )
        krbCon.WriteString(".KERBEROS.OPTION. dns\n");

	krbCon.Close();
	return TRUE;
}

void CKrb4RealmHostMaintenance::OnOK()
{
	CPropertyPage::OnOK();
}

void CKrb4RealmHostMaintenance::OnCancel()
{
	CPropertyPage::OnCancel();
}

void CKrb4RealmHostMaintenance::OnCheckDnsKdcLookup()
{
    m_newDnsKdcLookup = (BOOL)IsDlgButtonChecked(IDC_KRB4_DNS_KDC);
    SetModified(TRUE);
}

void CKrb4RealmHostMaintenance::ResetDefaultRealmComboBox()
{ // krb4 is loaded without krb5
	CHAR lineBuf[REALM_SZ + MAX_HSTNM + 20];

	int maxItems = m_RealmHostList.GetCount();

	CKrbConfigOptions::m_krbRealmEditbox.ResetContent();

	for (int xItems = 0; xItems < maxItems; xItems++)
	{
		m_RealmHostList.GetText(xItems, lineBuf);

		LPSTR space = strchr(lineBuf, ' ');
		if (space)
		  *space = 0;
		else
		  ASSERT(0);

		if (CB_ERR == CKrbConfigOptions::m_krbRealmEditbox.FindStringExact(-1, lineBuf))
		{ // no dups
			if (LB_ERR == CKrbConfigOptions::m_krbRealmEditbox.AddString(lineBuf))
			{
				MessageBox("OnInitDialog::Can't add to Kerberos Realm Combobox",
						   "Leash", MB_OK);
				return;
			}
		}
	}

	CHAR krbhst[MAX_HSTNM + 1];
	CHAR krbrlm[REALM_SZ + 1];

	strcpy(krbrlm, CKrbConfigOptions::m_newDefaultRealm);
	memset(krbhst, '\0', sizeof(krbhst));

	// Check for Host
	// don't use KRB4 - krb_get_krbhst - would have to re-logon, on file location
	// change, to use this function
	extern int krb_get_krbhst(char* h, char* r, int n);
	if (KFAILURE == krb_get_krbhst(krbhst, krbrlm, 1))
	{
		MessageBox("We can't find the Host Server for your Default Realm!!!",
                    "Leash", MB_OK);
		return;
	}

	CKrbConfigOptions::m_hostServer = krbhst;
}

void CKrb4RealmHostMaintenance::OnButtonRealmHostAdd()
{
	CKrb4AddToRealmHostList addToRealmHostList;

	if (IDOK == addToRealmHostList.DoModal())
	{
		if (addToRealmHostList.GetNewRealm().IsEmpty())
		  ASSERT(0);

		CString newLine;
		newLine = addToRealmHostList.GetNewRealm() + " " + addToRealmHostList.GetNewHost();

		if (addToRealmHostList.GetNewAdmin())
		  newLine += " admin server";

		// We don't want duplicate items in Listbox
		if (LB_ERR != m_RealmHostList.FindStringExact(-1, newLine))
		{ // found duplicate item in Listbox
			LeashErrorBox("OnButtonRealmHostAdd::Found a Duplicate Item!\nCan't add to List",
						  newLine);
			return;
		}


		m_RealmHostList.InsertString(0, newLine);
		m_RealmHostList.SetCurSel(0);
		SetModified(TRUE);

		ResetDefaultRealmComboBox();

		if (1 == m_RealmHostList.GetCount())
		{
			GetDlgItem(ID_BUTTON_KRB4_REALM_HOST_REMOVE)->EnableWindow();
			GetDlgItem(IDC_BUTTON_KRB4_REALM_HOST_EDIT)->EnableWindow();
		}
	}
}

void CKrb4RealmHostMaintenance::OnButtonRealmHostEdit()
{
	INT selItemIndex = m_RealmHostList.GetCurSel();
	LPSTR pSelItem = new char[m_RealmHostList.GetTextLen(selItemIndex) + 1];
	if (!pSelItem)
	  ASSERT(0);

	CString selItem;
	m_RealmHostList.GetText(selItemIndex, selItem);
	strcpy(pSelItem, selItem);

	CKrb4EditRealmHostList editRealmHostList(pSelItem);
	delete [] pSelItem;

	if (IDOK == editRealmHostList.DoModal())
	{
		CString editedItem = editRealmHostList.GetEditedItem();
		if (0 != selItem.CompareNoCase(editedItem) &&
			LB_ERR != m_RealmHostList.FindStringExact(-1, editedItem))
		{
			LeashErrorBox("OnButtonRealmHostEdit::Found a Duplicate!\nCan't add to List",
							  editedItem);

			return;
		}

		m_RealmHostList.DeleteString(selItemIndex);
		m_RealmHostList.InsertString(selItemIndex, editRealmHostList.GetEditedItem());
		m_RealmHostList.SetCurSel(selItemIndex);
		SetModified(TRUE);

		ResetDefaultRealmComboBox();
	}
}

void CKrb4RealmHostMaintenance::OnButtonRealmHostRemove()
{
	if (IDYES != AfxMessageBox("You are about to remove an item from the list!\n\nContinue?",
		                       MB_YESNO))
	  return;

	INT curSel = m_RealmHostList.GetCurSel();
	m_RealmHostList.DeleteString(curSel);  // Single Sel Listbox

	if (-1 == m_RealmHostList.SetCurSel(curSel))
	  m_RealmHostList.SetCurSel(curSel - 1);

	SetModified(TRUE);

	ResetDefaultRealmComboBox();

	if (!m_RealmHostList.GetCount())
	{
		GetDlgItem(ID_BUTTON_KRB4_REALM_HOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_KRB4_REALM_HOST_EDIT)->EnableWindow(FALSE);
	}

	/* For Mult. Sel Listbox
	const LONG MAX_SEL_BUF = m_RealmHostList.GetSelCount();
	LPINT selectBuf = new INT[MAX_SEL_BUF];

	for (INT maxSelected = m_RealmHostList.GetSelItems(MAX_SEL_BUF, selectBuf), del=0, sel=0;
	     sel < maxSelected; sel++)
	{
		if (LB_ERR == m_RealmHostList.DeleteString(*(selectBuf + sel) - del))
		  MessageBox("Help", "Error", MB_OK);
		else
		  del++;
	}

	delete selectBuf;
	*/
}

void CKrb4RealmHostMaintenance::OnSelchangeListRemoveHost()
{
	//SetModified(TRUE);
}


void CKrb4RealmHostMaintenance::OnDblclkListRemoveHost()
{
	OnButtonRealmHostEdit();
}

BOOL CKrb4RealmHostMaintenance::PreTranslateMessage(MSG* pMsg)
{
	if (m_defectiveLines)
	{
		if (m_defectiveLines == 1)
		  LeashErrorBox("Found a defective entry in file",
						CKrbProperties::m_krbPath, "Warning");
	    else if (m_defectiveLines > 1)
	      LeashErrorBox("Found more then one defective entry in file",
						CKrbProperties::m_krbPath, "Warning");
	}

	m_defectiveLines = 0;
	return CPropertyPage::PreTranslateMessage(pMsg);
}

void CKrb4RealmHostMaintenance::OnButtonRealmhostMaintHelp2()
{
	MessageBox("No Help Available!", "Note", MB_OK);
}
