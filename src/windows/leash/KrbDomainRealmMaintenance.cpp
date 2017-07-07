// CKrbDomainRealmMaintenance.cpp : implementation file
//

#include "stdafx.h"
#include "leash.h"
#include "KrbDomainRealmMaintenance.h"
#include "Krb4AddToDomainRealmList.h"
#include "Krb4EditDomainRealmList.h"
#include "KrbProperties.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrbDomainRealmMaintenance dialog


CKrbDomainRealmMaintenance::CKrbDomainRealmMaintenance(CWnd* pParent /*=NULL*/)
	:CPropertyPage(CKrbDomainRealmMaintenance::IDD)
{
	m_dupEntiesError = FALSE;
	//{{AFX_DATA_INIT(CKrbDomainRealmMaintenance)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CKrbDomainRealmMaintenance::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbDomainRealmMaintenance)
	DDX_Control(pDX, IDC_LIST_DOMAINREALM, m_KDCDomainList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrbDomainRealmMaintenance, CPropertyPage)
	//{{AFX_MSG_MAP(CKrbDomainRealmMaintenance)
	ON_BN_CLICKED(IDC_BUTTON_HOST_ADD, OnButtonHostAdd)
	ON_BN_CLICKED(IDC_BUTTON_HOST_EDIT, OnButtonHostEdit)
	ON_BN_CLICKED(ID_BUTTON_HOST_REMOVE, OnButtonHostRemove)
	ON_LBN_DBLCLK(IDC_LIST_DOMAINREALM, OnDblclkListDomainrealm)
	ON_BN_CLICKED(IDC_BUTTON_HOSTMAINT_HELP, OnButtonHostmaintHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrbDomainRealmMaintenance message handlers

BOOL CKrbDomainRealmMaintenance::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	char theName[REALM_SZ + 1];
	char theNameValue[REALM_SZ + MAX_HSTNM + 2];
	const char*  Section[] = {"domain_realm", theName, NULL}; //theSection
	const char** section = Section;
	char **values = NULL,
		 **vpp = NULL;

	const char*  rootSection[] = {"domain_realm", NULL};
	const char** rootsec = rootSection;
	char **sections = NULL,
		 **cpp = NULL;

	long retval = pprofile_get_relation_names(CLeashApp::m_krbv5_profile,
											   rootsec, &sections);

	if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnInitDialog::There is on error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return TRUE;
	}


	for (cpp = sections; *cpp; cpp++)
	{
		strcpy(theName, *cpp);
		retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
									  section, &values);

		for (vpp = values; *vpp; vpp++)
		{
			strcpy(theNameValue, theName);
			strcat(theNameValue, " ");
			strcat(theNameValue, *vpp);

			if (LB_ERR == m_KDCDomainList.FindStringExact(-1, theNameValue))
			{
				if (LB_ERR == m_KDCDomainList.AddString(theNameValue))
				{
					MessageBox("OnInitDialog::Can't add to Kerberos Domain Listbox",
							   "Leash", MB_OK);
					return FALSE;
				}
			}
			else
			  m_dupEntiesError = TRUE;
		}
	}

	m_KDCDomainList.SetCurSel(0);

	if (!m_KDCDomainList.GetCount())
	{
		GetDlgItem(ID_BUTTON_HOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_HOST_EDIT)->EnableWindow(FALSE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CKrbDomainRealmMaintenance::OnApply()
{
	if (!CLeashApp::m_krbv5_profile) {
		CHAR confname[MAX_PATH];
		if (!CLeashApp::GetProfileFile(confname, sizeof(confname)))
        {
	        const char *filenames[2];
		    filenames[0] = confname;
			filenames[1] = NULL;
			pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
		}
	}

	// Save to Kerberos Five config. file "Krb5.ini"
	long retval = pprofile_flush(CLeashApp::m_krbv5_profile);

	if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnApply::There is on error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return TRUE;
	}

#ifndef NO_KRB4
	// Save to Kerberos Four config. file "Krb.con"
	CStdioFile krbrealmCon;
	if (!krbrealmCon.Open(CKrbProperties::m_krbrealmPath, CFile::modeCreate |
			      CFile::modeNoTruncate |
													 CFile::modeReadWrite))
	{
		LeashErrorBox("OnApply::Can't open Configuration File",
					  CKrbProperties::m_krbrealmPath);
		return TRUE;
	}

	krbrealmCon.SetLength(0);

	char theNameValue[REALM_SZ + MAX_HSTNM + 2];

	for (INT maxItems = m_KDCDomainList.GetCount(), item = 0; item < maxItems; item++)
	{
		if (LB_ERR == m_KDCDomainList.GetText(item, theNameValue))
		  ASSERT(0);

		krbrealmCon.WriteString(theNameValue);
		krbrealmCon.WriteString("\n");
	}

	krbrealmCon.Close();
#endif

	return TRUE;
}

void CKrbDomainRealmMaintenance::OnCancel()
{
    CHAR fileName[MAX_PATH];

    if (CLeashApp::GetProfileFile(fileName, sizeof(fileName)))
    {
        MessageBox("Can't locate Kerberos Five Config. file!", "Error", MB_OK);
        return;
    }

    pprofile_abandon(CLeashApp::m_krbv5_profile);

    const char *filenames[2];
    filenames[0] = fileName;
    filenames[1] = NULL;
	pprofile_init(filenames, &CLeashApp::m_krbv5_profile);

	CPropertyPage::OnCancel();
}

void CKrbDomainRealmMaintenance::OnButtonHostAdd()
{
////I don't understand why this is doing K4 operations here
#ifndef NO_KRB4
	CKrb4AddToDomainRealmList addToDomainRealmList;
	if (IDOK == addToDomainRealmList.DoModal())
	{
		char theName[MAX_HSTNM + 1];
		const char* Section[] = {"domain_realm", theName, NULL};
		const char** section = Section;

		if (addToDomainRealmList.GetNewRealm().IsEmpty())
		  ASSERT(0);

		if (CheckForDupDomain(addToDomainRealmList.GetNewDomainHost()))
		{
			MessageBox("Can't have duplicate Host/Domains!\nYour entry will not be saved to list",
					   "Leash", MB_OK);
			return;
		}

		CString newLine;
		newLine = addToDomainRealmList.GetNewDomainHost() + " " + addToDomainRealmList.GetNewRealm();

		if (LB_ERR != m_KDCDomainList.FindStringExact(-1, newLine))
		{
			MessageBox("We can't have duplicates!\nYour entry was not saved to list.",
                        "Leash", MB_OK);
			return;
		}

		CString newHost; // new section in the profile linklist
		strcpy(theName, addToDomainRealmList.GetNewDomainHost());

		long retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
									         section, addToDomainRealmList.GetNewRealm());

		if (retval)
		{
			MessageBox("OnButtonHostAdd::There is on error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
				       "Leash", MB_OK);
		}

		m_KDCDomainList.AddString(newLine);
		SetModified(TRUE);

		if (1 == m_KDCDomainList.GetCount())
		{
			GetDlgItem(ID_BUTTON_HOST_REMOVE)->EnableWindow();
			GetDlgItem(IDC_BUTTON_HOST_EDIT)->EnableWindow();
		}
	}
#endif
}

void CKrbDomainRealmMaintenance::OnButtonHostEdit()
{
	INT selItemIndex = m_KDCDomainList.GetCurSel();
	LPSTR pSelItem = new char[m_KDCDomainList.GetTextLen(selItemIndex) + 1];
	if (!pSelItem)
	  ASSERT(0);

	CHAR theName[MAX_HSTNM + 1];
	char theNameValue[REALM_SZ + MAX_HSTNM + 2];
	CHAR OLD_VALUE[REALM_SZ + 1];
	m_KDCDomainList.GetText(selItemIndex, theName);
	strcpy(pSelItem, theName);

	LPSTR pselItem = strchr(theName, ' ');
	if (pselItem)
	  *pselItem = 0;
	else
	  ASSERT(0);

	strcpy(OLD_VALUE, pselItem + 1);
	strcpy(theNameValue, pSelItem);

	CKrb4EditDomainRealmList editDomainRealmList(pSelItem);

	if (IDOK == editDomainRealmList.DoModal())
	{
		if (0 != strcmp(theName, editDomainRealmList.GetDomainHost())
			&& CheckForDupDomain(editDomainRealmList.GetDomainHost()))
		{ // Duplicate Host/Domain Error
			MessageBox("We can't have duplicate Host/Domains!\nYour entry will not be saved to list",
					   "Leash", MB_OK);
			return;
		}

		const char* Section[] = {"domain_realm", theName, NULL};
		const char** section = Section;

		CString editedHost = editDomainRealmList.GetEditedItem();

		if (0 != editedHost.CompareNoCase(theNameValue) &&
			LB_ERR != m_KDCDomainList.FindStringExact(-1, editedHost))
		{
			MessageBox("We can't have duplicate Realms!\nYour entry was not saved to list.",
                        "Leash", MB_OK);
			delete [] pSelItem;
			return;
		}

		long retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
										        section, OLD_VALUE,	NULL);

		if (retval)
		{
			MessageBox("OnButtonHostEdit::There is on error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
					   "Leash", MB_OK);
			return;
		}

		strcpy(theName, editDomainRealmList.GetDomainHost());

		retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
										section, editDomainRealmList.GetRealm());


		if (retval)
		{ // thsi might not be the best way to handle this type of error
			MessageBox("OnButtonHostEdit::There is on error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
					   "Leash", MB_OK);
			return;
		}

		m_KDCDomainList.DeleteString(selItemIndex);
		m_KDCDomainList.AddString(editedHost);
		selItemIndex = m_KDCDomainList.FindStringExact(-1, editedHost);
		m_KDCDomainList.SetCurSel(selItemIndex);

		SetModified(TRUE);
	}

	delete [] pSelItem;
}

void CKrbDomainRealmMaintenance::OnDblclkListDomainrealm()
{
	OnButtonHostEdit();
}

void CKrbDomainRealmMaintenance::OnButtonHostRemove()
{
	CHAR theName[MAX_HSTNM + 1];
	CHAR OLD_VALUE[REALM_SZ + 1];
	char theNameValue[REALM_SZ + MAX_HSTNM + 2];
	const char* Section[] = {"domain_realm", theName, NULL};
	const char** section = Section;

	INT curSel = m_KDCDomainList.GetCurSel();
	m_KDCDomainList.GetText(curSel, theNameValue);

	CString serverHostMsg;
	CString serverHost;
	serverHostMsg.Format("Your about to remove Host/Domain \"%s\" from the list!\n\nContinue?",
						  theNameValue);

	if (IDYES != AfxMessageBox(serverHostMsg, MB_YESNO))
	  return;

	LPSTR pNameValue = strchr(theNameValue, ' ');
	if (pNameValue)
	{
		*pNameValue = 0;
		strcpy(theName, theNameValue);
		pNameValue++;
		strcpy(OLD_VALUE, pNameValue);
	}
	else
	  ASSERT(0);

	if (!m_KDCDomainList.GetCount())
	{
		GetDlgItem(ID_BUTTON_HOSTNAME_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_HOSTNAME_EDIT)->EnableWindow(FALSE);
	}

	long retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
										    section, OLD_VALUE, NULL);

	if (retval)
	{
		MessageBox("OnButtonHostRemove::There is on error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}

	m_KDCDomainList.DeleteString(curSel);  // Single Sel Listbox

	if (-1 == m_KDCDomainList.SetCurSel(curSel))
	  m_KDCDomainList.SetCurSel(curSel - 1);

	if (!m_KDCDomainList.GetCount())
	{
		GetDlgItem(ID_BUTTON_HOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_HOST_EDIT)->EnableWindow(FALSE);
	}

	SetModified(TRUE);
}


BOOL CKrbDomainRealmMaintenance::PreTranslateMessage(MSG* pMsg)
{
	if (m_dupEntiesError)
	{
		MessageBox("Found an error (duplicate items) in your Kerberos Five Config. File!!!\
                    \nPlease contract your Administrator.",
				   "Leash", MB_OK);

		m_dupEntiesError = FALSE;
	}

	return CPropertyPage::PreTranslateMessage(pMsg);
}

BOOL CKrbDomainRealmMaintenance::CheckForDupDomain(CString& newDomainHost)
{
	char theName[REALM_SZ + MAX_HSTNM + 2];

	for (INT maxItems = m_KDCDomainList.GetCount(), item = 0; item < maxItems; item++)
	{
		if (LB_ERR == m_KDCDomainList.GetText(item, theName))
		  ASSERT(0);

		LPSTR pValue = strchr(theName, ' ');
		if (pValue)
		  *pValue = 0;
		else
		  ASSERT(0);

		if (0 == newDomainHost.CompareNoCase(theName))
		  return TRUE;
	}

	return FALSE;
}

void CKrbDomainRealmMaintenance::OnButtonHostmaintHelp()
{
	MessageBox("No Help Available!", "Leash", MB_OK);
}
