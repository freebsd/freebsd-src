//*****************************************************************************
// File:	KrbMiscConfigOpt.cpp
// By:		Paul B. Hill
// Created:	08/12/1999
// Copyright:	@1999 Massachusetts Institute of Technology - All rights
//		reserved.
// Description: CPP file for KrbMiscConfigOpt.cpp.  Contains variables
//		and functions for Kerberos Properties.
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 08/12/99	PBH	Original
//*****************************************************************************

#include "stdafx.h"
#include "Leash.h"
#include "KrbProperties.h"
#include "KrbMiscConfigOpt.h"
#include "LeashFileDialog.h"
#include "LeashMessageBox.h"
#include "lglobals.h"
#include <direct.h>
#include "reminder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


///////////////////////////////////////////////////////////////////////
// CKrbMiscConfigOpt property page

UINT CKrbMiscConfigOpt::m_DefaultLifeTime;
CString CKrbMiscConfigOpt::m_initDefaultLifeTimeMin;
CString CKrbMiscConfigOpt::m_newDefaultLifeTimeMin;
CEdit CKrbMiscConfigOpt::m_krbLifeTimeMinEditbox;
CString CKrbMiscConfigOpt::m_initDefaultLifeTimeHr;
CString CKrbMiscConfigOpt::m_newDefaultLifeTimeHr;
CEdit CKrbMiscConfigOpt::m_krbLifeTimeHrEditbox;
CString CKrbMiscConfigOpt::m_initDefaultLifeTimeDay;
CString CKrbMiscConfigOpt::m_newDefaultLifeTimeDay;
CEdit CKrbMiscConfigOpt::m_krbLifeTimeDayEditbox;

UINT CKrbMiscConfigOpt::m_DefaultRenewTill;
CString CKrbMiscConfigOpt::m_initDefaultRenewTillMin;
CString CKrbMiscConfigOpt::m_newDefaultRenewTillMin;
CEdit CKrbMiscConfigOpt::m_krbRenewTillMinEditbox;
CString CKrbMiscConfigOpt::m_initDefaultRenewTillHr;
CString CKrbMiscConfigOpt::m_newDefaultRenewTillHr;
CEdit CKrbMiscConfigOpt::m_krbRenewTillHrEditbox;
CString CKrbMiscConfigOpt::m_initDefaultRenewTillDay;
CString CKrbMiscConfigOpt::m_newDefaultRenewTillDay;
CEdit CKrbMiscConfigOpt::m_krbRenewTillDayEditbox;

UINT CKrbMiscConfigOpt::m_DefaultLifeMin;
CString CKrbMiscConfigOpt::m_initDefaultLifeMinMin;
CString CKrbMiscConfigOpt::m_newDefaultLifeMinMin;
CEdit CKrbMiscConfigOpt::m_krbLifeMinMinEditbox;
CString CKrbMiscConfigOpt::m_initDefaultLifeMinHr;
CString CKrbMiscConfigOpt::m_newDefaultLifeMinHr;
CEdit CKrbMiscConfigOpt::m_krbLifeMinHrEditbox;
CString CKrbMiscConfigOpt::m_initDefaultLifeMinDay;
CString CKrbMiscConfigOpt::m_newDefaultLifeMinDay;
CEdit CKrbMiscConfigOpt::m_krbLifeMinDayEditbox;

UINT CKrbMiscConfigOpt::m_DefaultLifeMax;
CString CKrbMiscConfigOpt::m_initDefaultLifeMaxMin;
CString CKrbMiscConfigOpt::m_newDefaultLifeMaxMin;
CEdit CKrbMiscConfigOpt::m_krbLifeMaxMinEditbox;
CString CKrbMiscConfigOpt::m_initDefaultLifeMaxHr;
CString CKrbMiscConfigOpt::m_newDefaultLifeMaxHr;
CEdit CKrbMiscConfigOpt::m_krbLifeMaxHrEditbox;
CString CKrbMiscConfigOpt::m_initDefaultLifeMaxDay;
CString CKrbMiscConfigOpt::m_newDefaultLifeMaxDay;
CEdit CKrbMiscConfigOpt::m_krbLifeMaxDayEditbox;

UINT CKrbMiscConfigOpt::m_DefaultRenewMin;
CString CKrbMiscConfigOpt::m_initDefaultRenewMinMin;
CString CKrbMiscConfigOpt::m_newDefaultRenewMinMin;
CEdit CKrbMiscConfigOpt::m_krbRenewMinMinEditbox;
CString CKrbMiscConfigOpt::m_initDefaultRenewMinHr;
CString CKrbMiscConfigOpt::m_newDefaultRenewMinHr;
CEdit CKrbMiscConfigOpt::m_krbRenewMinHrEditbox;
CString CKrbMiscConfigOpt::m_initDefaultRenewMinDay;
CString CKrbMiscConfigOpt::m_newDefaultRenewMinDay;
CEdit CKrbMiscConfigOpt::m_krbRenewMinDayEditbox;

UINT CKrbMiscConfigOpt::m_DefaultRenewMax;
CString CKrbMiscConfigOpt::m_initDefaultRenewMaxMin;
CString CKrbMiscConfigOpt::m_newDefaultRenewMaxMin;
CEdit CKrbMiscConfigOpt::m_krbRenewMaxMinEditbox;
CString CKrbMiscConfigOpt::m_initDefaultRenewMaxHr;
CString CKrbMiscConfigOpt::m_newDefaultRenewMaxHr;
CEdit CKrbMiscConfigOpt::m_krbRenewMaxHrEditbox;
CString CKrbMiscConfigOpt::m_initDefaultRenewMaxDay;
CString CKrbMiscConfigOpt::m_newDefaultRenewMaxDay;
CEdit CKrbMiscConfigOpt::m_krbRenewMaxDayEditbox;


IMPLEMENT_DYNCREATE(CKrbMiscConfigOpt, CPropertyPage)

CKrbMiscConfigOpt::CKrbMiscConfigOpt() : CPropertyPage(CKrbMiscConfigOpt::IDD)
{
    m_noLifeTime = FALSE;

    m_DefaultLifeTime = 0;
    m_DefaultRenewTill = 0;
	m_DefaultLifeMin = 0;
	m_DefaultLifeMax = 0;
	m_DefaultRenewMin = 0;
	m_DefaultRenewMax = 0;
    m_initUseKrb4 = m_newUseKrb4 = 0;
    m_initKinitPreserve = m_newKinitPreserve = 0;

	//{{AFX_DATA_INIT(CKrbConfigOptions)
	//}}AFX_DATA_INIT
}

CKrbMiscConfigOpt::~CKrbMiscConfigOpt()
{
}

VOID CKrbMiscConfigOpt::DoDataExchange(CDataExchange* pDX)
{
    TRACE("Entering CKrbMiscConfigOpt::DoDataExchange -- %d\n",
          pDX->m_bSaveAndValidate);
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CKrbMscConfigOpt)

    DDX_Control(pDX, IDC_EDIT_LIFETIME_D,  m_krbLifeTimeDayEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFETIME_H,  m_krbLifeTimeHrEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFETIME_M,  m_krbLifeTimeMinEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEWTILL_D,  m_krbRenewTillDayEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEWTILL_H,  m_krbRenewTillHrEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEWTILL_M,  m_krbRenewTillMinEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFE_MIN_D,  m_krbLifeMinDayEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFE_MIN_H,  m_krbLifeMinHrEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFE_MIN_M,  m_krbLifeMinMinEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFE_MAX_D,  m_krbLifeMaxDayEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFE_MAX_H,  m_krbLifeMaxHrEditbox);
    DDX_Control(pDX, IDC_EDIT_LIFE_MAX_M,  m_krbLifeMaxMinEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEW_MIN_D, m_krbRenewMinDayEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEW_MIN_H, m_krbRenewMinHrEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEW_MIN_M, m_krbRenewMinMinEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEW_MAX_D, m_krbRenewMaxDayEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEW_MAX_H, m_krbRenewMaxHrEditbox);
    DDX_Control(pDX, IDC_EDIT_RENEW_MAX_M, m_krbRenewMaxMinEditbox);
    //}}AFX_DATA_MAP
}


BOOL CKrbMiscConfigOpt::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

	DWORD tmp = m_DefaultLifeTime = pLeash_get_default_lifetime();
    if (tmp)
        m_noLifeTime = FALSE; // We now have the value.
    else
        m_noLifeTime = TRUE;

    LPTSTR buf = m_initDefaultLifeTimeDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_initDefaultLifeTimeDay.ReleaseBuffer();
	m_newDefaultLifeTimeDay = m_initDefaultLifeTimeDay;

	buf = m_initDefaultLifeTimeHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_initDefaultLifeTimeHr.ReleaseBuffer();
	m_newDefaultLifeTimeHr = m_initDefaultLifeTimeHr;

	buf = m_initDefaultLifeTimeMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_initDefaultLifeTimeMin.ReleaseBuffer();
	m_newDefaultLifeTimeMin = m_initDefaultLifeTimeMin;

	tmp = m_DefaultRenewTill = pLeash_get_default_renew_till();
    buf = m_initDefaultRenewTillDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_initDefaultRenewTillDay.ReleaseBuffer();
	m_newDefaultRenewTillDay = m_initDefaultRenewTillDay;

	buf = m_initDefaultRenewTillHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_initDefaultRenewTillHr.ReleaseBuffer();
	m_newDefaultRenewTillHr = m_initDefaultRenewTillHr;

	buf = m_initDefaultRenewTillMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_initDefaultRenewTillMin.ReleaseBuffer();
	m_newDefaultRenewTillMin = m_initDefaultRenewTillMin;

    tmp = m_DefaultLifeMin = pLeash_get_default_life_min();
	buf = m_initDefaultLifeMinDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_initDefaultLifeMinDay.ReleaseBuffer();
	m_newDefaultLifeMinDay = m_initDefaultLifeMinDay;

	buf = m_initDefaultLifeMinHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_initDefaultLifeMinHr.ReleaseBuffer();
	m_newDefaultLifeMinHr = m_initDefaultLifeMinHr;

	buf = m_initDefaultLifeMinMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_initDefaultLifeMinMin.ReleaseBuffer();
	m_newDefaultLifeMinMin = m_initDefaultLifeMinMin;

	tmp = m_DefaultLifeMax = pLeash_get_default_life_max();
	buf = m_initDefaultLifeMaxDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_initDefaultLifeMaxDay.ReleaseBuffer();
	m_newDefaultLifeMaxDay = m_initDefaultLifeMaxDay;

	buf = m_initDefaultLifeMaxHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_initDefaultLifeMaxHr.ReleaseBuffer();
	m_newDefaultLifeMaxHr = m_initDefaultLifeMaxHr;

	buf = m_initDefaultLifeMaxMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_initDefaultLifeMaxMin.ReleaseBuffer();
	m_newDefaultLifeMaxMin = m_initDefaultLifeMaxMin;

    tmp = m_DefaultRenewMin = pLeash_get_default_renew_min();
	buf = m_initDefaultRenewMinDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_initDefaultRenewMinDay.ReleaseBuffer();
	m_newDefaultRenewMinDay = m_initDefaultRenewMinDay;

	buf = m_initDefaultRenewMinHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_initDefaultRenewMinHr.ReleaseBuffer();
	m_newDefaultRenewMinHr = m_initDefaultRenewMinHr;

	buf = m_initDefaultRenewMinMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_initDefaultRenewMinMin.ReleaseBuffer();
	m_newDefaultRenewMinMin = m_initDefaultRenewMinMin;

	tmp = m_DefaultRenewMax = pLeash_get_default_renew_max();
	buf = m_initDefaultRenewMaxDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_initDefaultRenewMaxDay.ReleaseBuffer();
	m_newDefaultRenewMaxDay = m_initDefaultRenewMaxDay;

	buf = m_initDefaultRenewMaxHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_initDefaultRenewMaxHr.ReleaseBuffer();
	m_newDefaultRenewMaxHr = m_initDefaultRenewMaxHr;

	buf = m_initDefaultRenewMaxMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_initDefaultRenewMaxMin.ReleaseBuffer();
	m_newDefaultRenewMaxMin = m_initDefaultRenewMaxMin;

    if (!CLeashApp::m_hKrb5DLL)
    {
        GetDlgItem(IDC_EDIT_RENEWTILL_D)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEWTILL_H)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEWTILL_M)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEW_MIN_D)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEW_MIN_H)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEW_MIN_M)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEW_MAX_D)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEW_MAX_H)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RENEW_MAX_M)->EnableWindow(FALSE);
    }

#ifndef NO_KRB4
    m_initUseKrb4 = m_newUseKrb4 = (CLeashApp::m_hKrb4DLL ? pLeash_get_default_use_krb4() : 0);
    CheckDlgButton(IDC_CHECK_REQUEST_KRB4, m_initUseKrb4);
    if ( !CLeashApp::m_hKrb4DLL )
        GetDlgItem(IDC_CHECK_REQUEST_KRB4)->EnableWindow(FALSE);
#else
////Or remove these completely?
    m_initUseKrb4 = m_newUseKrb4 = 0;
    CheckDlgButton(IDC_CHECK_REQUEST_KRB4, 0);
    GetDlgItem(IDC_CHECK_REQUEST_KRB4)->EnableWindow(FALSE);
#endif

    m_initKinitPreserve = m_newKinitPreserve = pLeash_get_default_preserve_kinit_settings();
    CheckDlgButton(IDC_CHECK_PRESERVE_KINIT_OPTIONS, m_initKinitPreserve);

    return(TRUE);
}

BOOL CKrbMiscConfigOpt::OnApply()
{
    DWORD lifetime = ((atoi(m_newDefaultLifeTimeDay)*24 + atoi(m_newDefaultLifeTimeHr)) * 60) + atoi(m_newDefaultLifeTimeMin);
    DWORD renewtill = ((atoi(m_newDefaultRenewTillDay)*24 + atoi(m_newDefaultRenewTillHr)) * 60) + atoi(m_newDefaultRenewTillMin);
    DWORD lifemin = ((atoi(m_newDefaultLifeMinDay)*24 + atoi(m_newDefaultLifeMinHr)) * 60) + atoi(m_newDefaultLifeMinMin);
    DWORD lifemax = ((atoi(m_newDefaultLifeMaxDay)*24 + atoi(m_newDefaultLifeMaxHr)) * 60) + atoi(m_newDefaultLifeMaxMin);
    DWORD renewmin = ((atoi(m_newDefaultRenewMinDay)*24 + atoi(m_newDefaultRenewMinHr)) * 60) + atoi(m_newDefaultRenewMinMin);
    DWORD renewmax = ((atoi(m_newDefaultRenewMaxDay)*24 + atoi(m_newDefaultRenewMaxHr)) * 60) + atoi(m_newDefaultRenewMaxMin);

    // If no changes were made, quit this function
    if ( m_DefaultLifeTime == lifetime &&
         m_DefaultRenewTill == renewtill &&
		 m_DefaultLifeMin == lifemin &&
		 m_DefaultLifeMax == lifemax &&
		 m_DefaultRenewMin == renewmin &&
		 m_DefaultRenewMax == renewmax &&
		 m_initUseKrb4 == m_newUseKrb4 &&
		 m_initKinitPreserve == m_newKinitPreserve
		 )
        return TRUE;

    if ( lifemin > lifemax ) {
        MessageBox("The Minimum Ticket Lifetime must be less than the Maximum Ticket Lifetime.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if (lifetime < lifemin || lifetime > lifemax) {
        MessageBox("The default Ticket Lifetime must fall within the range specified by the "
                   "Minimum and Maximum Ticket Lifetime fields",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if ( CLeashApp::m_hKrb5DLL && (renewmin > renewmax) ) {
        MessageBox("The Minimum Ticket Renewable Lifetime must be less than the Maximum Ticket Renewable Lifetime.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if ( CLeashApp::m_hKrb5DLL && (renewmin < lifemin) ) {
        MessageBox("The Minimum Renewable Ticket Lifetime must not be smaller than the Minimum Ticket Lifetime.",
                    "Leash", MB_OK);
    }

    if ( CLeashApp::m_hKrb5DLL && (renewtill < renewmin || renewtill > renewmax) ) {
        MessageBox("The default Renewable Ticket Lifetime must fall within the range specified by the "
                   "Minimum and Maximum Renewable Ticket Lifetime fields",
                    "Leash", MB_OK);
        return(FALSE);
    }

	m_DefaultLifeMin = lifemin;
	pLeash_set_default_life_min(m_DefaultLifeMin);
	m_initDefaultLifeMinDay = m_newDefaultLifeMinDay;
	m_initDefaultLifeMinHr  = m_newDefaultLifeMinHr ;
	m_initDefaultLifeMinMin = m_newDefaultLifeMinMin;

	m_DefaultLifeMax = lifemax;
	pLeash_set_default_life_max(m_DefaultLifeMax);
	m_initDefaultLifeMaxDay = m_newDefaultLifeMaxDay;
	m_initDefaultLifeMaxHr  = m_newDefaultLifeMaxHr ;
	m_initDefaultLifeMaxMin = m_newDefaultLifeMaxMin;

	m_DefaultRenewMin = renewmin;
	pLeash_set_default_renew_min(m_DefaultRenewMin);
	m_initDefaultRenewMinDay = m_newDefaultRenewMinDay;
	m_initDefaultRenewMinHr  = m_newDefaultRenewMinHr ;
	m_initDefaultRenewMinMin = m_newDefaultRenewMinMin;

	m_DefaultRenewMax = renewmax;
	pLeash_set_default_renew_max(m_DefaultRenewMax);
	m_initDefaultRenewMaxDay = m_newDefaultRenewMaxDay;
	m_initDefaultRenewMaxHr  = m_newDefaultRenewMaxHr ;
	m_initDefaultRenewMaxMin = m_newDefaultRenewMaxMin;

    m_DefaultRenewTill = renewtill;
	pLeash_set_default_renew_till(m_DefaultRenewTill);
	m_initDefaultRenewTillDay = m_newDefaultRenewTillDay;
	m_initDefaultRenewTillHr  = m_newDefaultRenewTillHr ;
	m_initDefaultRenewTillMin = m_newDefaultRenewTillMin;

    if( getenv("LIFETIME") !=  NULL)
    {
        MessageBox("The ticket lifetime is being controlled by the environment "
                   "variable LIFETIME instead of the registry. Leash cannot modify "
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    m_DefaultLifeTime = lifetime;
	pLeash_set_default_lifetime(m_DefaultLifeTime);
	m_initDefaultLifeTimeDay = m_newDefaultLifeTimeDay;
	m_initDefaultLifeTimeHr  = m_newDefaultLifeTimeHr ;
	m_initDefaultLifeTimeMin = m_newDefaultLifeTimeMin;

    // If we're using an environment variable tell the user that we
    // can't use Leash to modify the value.

    if (!m_DefaultLifeTime)
    {
        MessageBox("A lifetime setting of 0 is special in that it means that "
                   "the application is free to pick whatever default it deems "
                   "appropriate",
                   "Leash", MB_OK);
    }

#ifndef NO_KRB4
	if ( m_initUseKrb4 != m_newUseKrb4 ) {
		pLeash_set_default_use_krb4(m_newUseKrb4);
	}
#endif

	if ( m_initKinitPreserve != m_newKinitPreserve ) {
		pLeash_set_default_preserve_kinit_settings(m_newKinitPreserve);
	}

	return TRUE;
}

void CKrbMiscConfigOpt::OnSelchangeEditDefaultLifeTime()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_LIFETIME_D, m_newDefaultLifeTimeDay);
        GetDlgItemText(IDC_EDIT_LIFETIME_H, m_newDefaultLifeTimeHr);
        GetDlgItemText(IDC_EDIT_LIFETIME_M, m_newDefaultLifeTimeMin);
		DWORD value = (((atoi(m_newDefaultLifeTimeDay)*24 + atoi(m_newDefaultLifeTimeHr)) * 60) + atoi(m_newDefaultLifeTimeMin));
		LPSTR buf = m_newDefaultLifeTimeDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultLifeTimeDay.ReleaseBuffer();
		buf = m_newDefaultLifeTimeHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultLifeTimeHr.ReleaseBuffer();
		buf = m_newDefaultLifeTimeMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultLifeTimeMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_LIFETIME_D, m_newDefaultLifeTimeDay);
        SetDlgItemText(IDC_EDIT_LIFETIME_H, m_newDefaultLifeTimeHr);
        SetDlgItemText(IDC_EDIT_LIFETIME_M, m_newDefaultLifeTimeMin);
        SetModified(TRUE);
		in_progress = 0;
    }
}

void CKrbMiscConfigOpt::OnEditKillfocusEditDefaultLifeTime()
{
    static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_LIFETIME_D, m_newDefaultLifeTimeDay);
        GetDlgItemText(IDC_EDIT_LIFETIME_H, m_newDefaultLifeTimeHr);
        GetDlgItemText(IDC_EDIT_LIFETIME_M, m_newDefaultLifeTimeMin);
		DWORD value = (((atoi(m_newDefaultLifeTimeDay)*24 + atoi(m_newDefaultLifeTimeHr)) * 60) + atoi(m_newDefaultLifeTimeMin));
		LPSTR buf = m_newDefaultLifeTimeDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultLifeTimeDay.ReleaseBuffer();
		buf = m_newDefaultLifeTimeHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultLifeTimeHr.ReleaseBuffer();
		buf = m_newDefaultLifeTimeMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultLifeTimeMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_LIFETIME_D, m_newDefaultLifeTimeDay);
        SetDlgItemText(IDC_EDIT_LIFETIME_H, m_newDefaultLifeTimeHr);
        SetDlgItemText(IDC_EDIT_LIFETIME_M, m_newDefaultLifeTimeMin);

		SetModified(TRUE);
		in_progress = 0;
    }
}

void CKrbMiscConfigOpt::ResetDefaultLifeTimeEditBox()
{
    // Reset Config Tab's Default LifeTime Editbox

	DWORD tmp = m_DefaultLifeTime = pLeash_get_default_lifetime();
	LPSTR buf = m_newDefaultLifeTimeDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_newDefaultLifeTimeDay.ReleaseBuffer();
	buf = m_newDefaultLifeTimeHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_newDefaultLifeTimeHr.ReleaseBuffer();
	buf = m_newDefaultLifeTimeMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_newDefaultLifeTimeMin.ReleaseBuffer();

	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFETIME_D, m_newDefaultLifeTimeDay);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFETIME_H, m_newDefaultLifeTimeHr);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFETIME_M, m_newDefaultLifeTimeMin);
}


void CKrbMiscConfigOpt::OnSelchangeEditDefaultRenewTill()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_RENEWTILL_D, m_newDefaultRenewTillDay);
        GetDlgItemText(IDC_EDIT_RENEWTILL_H, m_newDefaultRenewTillHr);
        GetDlgItemText(IDC_EDIT_RENEWTILL_M, m_newDefaultRenewTillMin);
		DWORD value = (((atoi(m_newDefaultRenewTillDay)*24 + atoi(m_newDefaultRenewTillHr)) * 60) + atoi(m_newDefaultRenewTillMin));
		LPSTR buf = m_newDefaultRenewTillDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultRenewTillDay.ReleaseBuffer();
		buf = m_newDefaultRenewTillHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultRenewTillHr.ReleaseBuffer();
		buf = m_newDefaultRenewTillMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultRenewTillMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_RENEWTILL_D, m_newDefaultRenewTillDay);
        SetDlgItemText(IDC_EDIT_RENEWTILL_H, m_newDefaultRenewTillHr);
        SetDlgItemText(IDC_EDIT_RENEWTILL_M, m_newDefaultRenewTillMin);
        SetModified(TRUE);
		in_progress = 0;
    }
}

void CKrbMiscConfigOpt::OnEditKillfocusEditDefaultRenewTill()
{
    static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_RENEWTILL_D, m_newDefaultRenewTillDay);
        GetDlgItemText(IDC_EDIT_RENEWTILL_H, m_newDefaultRenewTillHr);
        GetDlgItemText(IDC_EDIT_RENEWTILL_M, m_newDefaultRenewTillMin);
		DWORD value = (((atoi(m_newDefaultRenewTillDay)*24 + atoi(m_newDefaultRenewTillHr)) * 60) + atoi(m_newDefaultRenewTillMin));
		LPSTR buf = m_newDefaultRenewTillDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultRenewTillDay.ReleaseBuffer();
		buf = m_newDefaultRenewTillHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultRenewTillHr.ReleaseBuffer();
		buf = m_newDefaultRenewTillMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultRenewTillMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_RENEWTILL_D, m_newDefaultRenewTillDay);
        SetDlgItemText(IDC_EDIT_RENEWTILL_H, m_newDefaultRenewTillHr);
        SetDlgItemText(IDC_EDIT_RENEWTILL_M, m_newDefaultRenewTillMin);

		SetModified(TRUE);
		in_progress = 0;
    }
}

void CKrbMiscConfigOpt::ResetDefaultRenewTillEditBox()
{
    // Reset Config Tab's Default RenewTill Editbox

	DWORD tmp = m_DefaultRenewTill = pLeash_get_default_lifetime();
	LPSTR buf = m_newDefaultRenewTillDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_newDefaultRenewTillDay.ReleaseBuffer();
	buf = m_newDefaultRenewTillHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_newDefaultRenewTillHr.ReleaseBuffer();
	buf = m_newDefaultRenewTillMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_newDefaultRenewTillMin.ReleaseBuffer();

	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEWTILL_D, m_newDefaultRenewTillDay);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEWTILL_H, m_newDefaultRenewTillHr);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEWTILL_M, m_newDefaultRenewTillMin);
}


void CKrbMiscConfigOpt::OnSelchangeEditDefaultLifeMin()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_LIFE_MIN_D, m_newDefaultLifeMinDay);
        GetDlgItemText(IDC_EDIT_LIFE_MIN_H, m_newDefaultLifeMinHr);
        GetDlgItemText(IDC_EDIT_LIFE_MIN_M, m_newDefaultLifeMinMin);
		DWORD value = (((atoi(m_newDefaultLifeMinDay)*24 + atoi(m_newDefaultLifeMinHr)) * 60) + atoi(m_newDefaultLifeMinMin));
		LPSTR buf = m_newDefaultLifeMinDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultLifeMinDay.ReleaseBuffer();
		buf = m_newDefaultLifeMinHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultLifeMinHr.ReleaseBuffer();
		buf = m_newDefaultLifeMinMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultLifeMinMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_LIFE_MIN_D, m_newDefaultLifeMinDay);
        SetDlgItemText(IDC_EDIT_LIFE_MIN_H, m_newDefaultLifeMinHr);
        SetDlgItemText(IDC_EDIT_LIFE_MIN_M, m_newDefaultLifeMinMin);
        SetModified(TRUE);
		in_progress = 0;
    }
}

void CKrbMiscConfigOpt::OnEditKillfocusEditDefaultLifeMin()
{
    static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_LIFE_MIN_D, m_newDefaultLifeMinDay);
        GetDlgItemText(IDC_EDIT_LIFE_MIN_H, m_newDefaultLifeMinHr);
        GetDlgItemText(IDC_EDIT_LIFE_MIN_M, m_newDefaultLifeMinMin);
		DWORD value = (((atoi(m_newDefaultLifeMinDay)*24 + atoi(m_newDefaultLifeMinHr)) * 60) + atoi(m_newDefaultLifeMinMin));
		LPSTR buf = m_newDefaultLifeMinDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultLifeMinDay.ReleaseBuffer();
		buf = m_newDefaultLifeMinHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultLifeMinHr.ReleaseBuffer();
		buf = m_newDefaultLifeMinMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultLifeMinMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_LIFE_MIN_D, m_newDefaultLifeMinDay);
        SetDlgItemText(IDC_EDIT_LIFE_MIN_H, m_newDefaultLifeMinHr);
        SetDlgItemText(IDC_EDIT_LIFE_MIN_M, m_newDefaultLifeMinMin);

		SetModified(TRUE);
		in_progress = 0;
    }
}

void CKrbMiscConfigOpt::ResetDefaultLifeMinEditBox()
{
    // Reset Config Tab's Default LifeMin Editbox

	DWORD tmp = m_DefaultLifeMin = pLeash_get_default_life_min();
	LPSTR buf = m_newDefaultLifeMinDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_newDefaultLifeMinDay.ReleaseBuffer();
	buf = m_newDefaultLifeMinHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_newDefaultLifeMinHr.ReleaseBuffer();
	buf = m_newDefaultLifeMinMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_newDefaultLifeMinMin.ReleaseBuffer();

	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFE_MIN_D, m_newDefaultLifeMinDay);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFE_MIN_H, m_newDefaultLifeMinHr);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFE_MIN_M, m_newDefaultLifeMinMin);
}

void CKrbMiscConfigOpt::OnSelchangeEditDefaultLifeMax()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_LIFE_MAX_D, m_newDefaultLifeMaxDay);
        GetDlgItemText(IDC_EDIT_LIFE_MAX_H, m_newDefaultLifeMaxHr);
        GetDlgItemText(IDC_EDIT_LIFE_MAX_M, m_newDefaultLifeMaxMin);
		DWORD value = (((atoi(m_newDefaultLifeMaxDay)*24 + atoi(m_newDefaultLifeMaxHr)) * 60) + atoi(m_newDefaultLifeMaxMin));
		LPSTR buf = m_newDefaultLifeMaxDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultLifeMaxDay.ReleaseBuffer();
		buf = m_newDefaultLifeMaxHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultLifeMaxHr.ReleaseBuffer();
		buf = m_newDefaultLifeMaxMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultLifeMaxMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_LIFE_MAX_D, m_newDefaultLifeMaxDay);
        SetDlgItemText(IDC_EDIT_LIFE_MAX_H, m_newDefaultLifeMaxHr);
        SetDlgItemText(IDC_EDIT_LIFE_MAX_M, m_newDefaultLifeMaxMin);

        SetModified(TRUE);
		in_progress = 0;
	}
}

void CKrbMiscConfigOpt::OnEditKillfocusEditDefaultLifeMax()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_LIFE_MAX_D, m_newDefaultLifeMaxDay);
        GetDlgItemText(IDC_EDIT_LIFE_MAX_H, m_newDefaultLifeMaxHr);
        GetDlgItemText(IDC_EDIT_LIFE_MAX_M, m_newDefaultLifeMaxMin);
		DWORD value = (((atoi(m_newDefaultLifeMaxDay)*24 + atoi(m_newDefaultLifeMaxHr)) * 60) + atoi(m_newDefaultLifeMaxMin));
		LPSTR buf = m_newDefaultLifeMaxDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultLifeMaxDay.ReleaseBuffer();
		buf = m_newDefaultLifeMaxHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultLifeMaxHr.ReleaseBuffer();
		buf = m_newDefaultLifeMaxMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultLifeMaxMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_LIFE_MAX_D, m_newDefaultLifeMaxDay);
        SetDlgItemText(IDC_EDIT_LIFE_MAX_H, m_newDefaultLifeMaxHr);
        SetDlgItemText(IDC_EDIT_LIFE_MAX_M, m_newDefaultLifeMaxMin);

		SetModified(TRUE);
		in_progress = 0;
	}
}

void CKrbMiscConfigOpt::ResetDefaultLifeMaxEditBox()
{
    // Reset Config Tab's Default LifeMax Editbox

	DWORD tmp = m_DefaultLifeMax = pLeash_get_default_life_min();
	LPSTR buf = m_newDefaultLifeMaxDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_newDefaultLifeMaxDay.ReleaseBuffer();
	buf = m_newDefaultLifeMaxHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_newDefaultLifeMaxHr.ReleaseBuffer();
	buf = m_newDefaultLifeMaxMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_newDefaultLifeMaxMin.ReleaseBuffer();

	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFE_MAX_D, m_newDefaultLifeMaxDay);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFE_MAX_H, m_newDefaultLifeMaxHr);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_LIFE_MAX_M, m_newDefaultLifeMaxMin);
}

void CKrbMiscConfigOpt::OnSelchangeEditDefaultRenewMin()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_RENEW_MIN_D, m_newDefaultRenewMinDay);
        GetDlgItemText(IDC_EDIT_RENEW_MIN_H, m_newDefaultRenewMinHr);
        GetDlgItemText(IDC_EDIT_RENEW_MIN_M, m_newDefaultRenewMinMin);
		DWORD value = (((atoi(m_newDefaultRenewMinDay)*24 + atoi(m_newDefaultRenewMinHr)) * 60) + atoi(m_newDefaultRenewMinMin));
		LPSTR buf = m_newDefaultRenewMinDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultRenewMinDay.ReleaseBuffer();
		buf = m_newDefaultRenewMinHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultRenewMinHr.ReleaseBuffer();
		buf = m_newDefaultRenewMinMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultRenewMinMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_RENEW_MIN_D, m_newDefaultRenewMinDay);
        SetDlgItemText(IDC_EDIT_RENEW_MIN_H, m_newDefaultRenewMinHr);
        SetDlgItemText(IDC_EDIT_RENEW_MIN_M, m_newDefaultRenewMinMin);

        SetModified(TRUE);
		in_progress = 0;
	}
}

void CKrbMiscConfigOpt::OnEditKillfocusEditDefaultRenewMin()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_RENEW_MIN_D, m_newDefaultRenewMinDay);
        GetDlgItemText(IDC_EDIT_RENEW_MIN_H, m_newDefaultRenewMinHr);
        GetDlgItemText(IDC_EDIT_RENEW_MIN_M, m_newDefaultRenewMinMin);
		DWORD value = (((atoi(m_newDefaultRenewMinDay)*24 + atoi(m_newDefaultRenewMinHr)) * 60) + atoi(m_newDefaultRenewMinMin));
		LPSTR buf = m_newDefaultRenewMinDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultRenewMinDay.ReleaseBuffer();
		buf = m_newDefaultRenewMinHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultRenewMinHr.ReleaseBuffer();
		buf = m_newDefaultRenewMinMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultRenewMinMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_RENEW_MIN_D, m_newDefaultRenewMinDay);
        SetDlgItemText(IDC_EDIT_RENEW_MIN_H, m_newDefaultRenewMinHr);
        SetDlgItemText(IDC_EDIT_RENEW_MIN_M, m_newDefaultRenewMinMin);

		SetModified(TRUE);
		in_progress = 0;
	}
}

void CKrbMiscConfigOpt::ResetDefaultRenewMinEditBox()
{
    // Reset Config Tab's Default RenewMin Editbox

	DWORD tmp = m_DefaultRenewMin = pLeash_get_default_life_min();
	LPSTR buf = m_newDefaultRenewMinDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_newDefaultRenewMinDay.ReleaseBuffer();
	buf = m_newDefaultRenewMinHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_newDefaultRenewMinHr.ReleaseBuffer();
	buf = m_newDefaultRenewMinMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_newDefaultRenewMinMin.ReleaseBuffer();

	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEW_MIN_D, m_newDefaultRenewMinDay);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEW_MIN_H, m_newDefaultRenewMinHr);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEW_MIN_M, m_newDefaultRenewMinMin);
}

void CKrbMiscConfigOpt::OnSelchangeEditDefaultRenewMax()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_RENEW_MAX_D, m_newDefaultRenewMaxDay);
        GetDlgItemText(IDC_EDIT_RENEW_MAX_H, m_newDefaultRenewMaxHr);
        GetDlgItemText(IDC_EDIT_RENEW_MAX_M, m_newDefaultRenewMaxMin);
		DWORD value = (((atoi(m_newDefaultRenewMaxDay)*24 + atoi(m_newDefaultRenewMaxHr)) * 60) + atoi(m_newDefaultRenewMaxMin));
		LPSTR buf = m_newDefaultRenewMaxDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultRenewMaxDay.ReleaseBuffer();
		buf = m_newDefaultRenewMaxHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultRenewMaxHr.ReleaseBuffer();
		buf = m_newDefaultRenewMaxMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultRenewMaxMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_RENEW_MAX_D, m_newDefaultRenewMaxDay);
        SetDlgItemText(IDC_EDIT_RENEW_MAX_H, m_newDefaultRenewMaxHr);
        SetDlgItemText(IDC_EDIT_RENEW_MAX_M, m_newDefaultRenewMaxMin);

        SetModified(TRUE);
		in_progress = 0;
	}
}

void CKrbMiscConfigOpt::OnEditKillfocusEditDefaultRenewMax()
{
	static int in_progress = 0;
    if (!in_progress && !m_startupPage2)
    {
		in_progress = 1;
        GetDlgItemText(IDC_EDIT_RENEW_MAX_D, m_newDefaultRenewMaxDay);
        GetDlgItemText(IDC_EDIT_RENEW_MAX_H, m_newDefaultRenewMaxHr);
        GetDlgItemText(IDC_EDIT_RENEW_MAX_M, m_newDefaultRenewMaxMin);
		DWORD value = (((atoi(m_newDefaultRenewMaxDay)*24 + atoi(m_newDefaultRenewMaxHr)) * 60) + atoi(m_newDefaultRenewMaxMin));
		LPSTR buf = m_newDefaultRenewMaxDay.GetBuffer(80);
		_itoa(value/24/60, buf, 10);
		value %= (24 * 60);
		m_newDefaultRenewMaxDay.ReleaseBuffer();
		buf = m_newDefaultRenewMaxHr.GetBuffer(80);
		_itoa(value/60, buf, 10);
		value %= 60;
		m_newDefaultRenewMaxHr.ReleaseBuffer();
		buf = m_newDefaultRenewMaxMin.GetBuffer(80);
		_itoa(value, buf, 10);
		m_newDefaultRenewMaxMin.ReleaseBuffer();
        SetDlgItemText(IDC_EDIT_RENEW_MAX_D, m_newDefaultRenewMaxDay);
        SetDlgItemText(IDC_EDIT_RENEW_MAX_H, m_newDefaultRenewMaxHr);
        SetDlgItemText(IDC_EDIT_RENEW_MAX_M, m_newDefaultRenewMaxMin);

		SetModified(TRUE);
		in_progress = 0;
	}
}

void CKrbMiscConfigOpt::ResetDefaultRenewMaxEditBox()
{
    // Reset Config Tab's Default RenewMax Editbox

	DWORD tmp = m_DefaultRenewMax = pLeash_get_default_life_min();
	LPSTR buf = m_newDefaultRenewMaxDay.GetBuffer(80);
	_itoa(tmp/24/60, buf, 10);
	tmp %= (24 * 60);
    m_newDefaultRenewMaxDay.ReleaseBuffer();
	buf = m_newDefaultRenewMaxHr.GetBuffer(80);
	_itoa(tmp/60, buf, 10);
	tmp %= 60;
    m_newDefaultRenewMaxHr.ReleaseBuffer();
	buf = m_newDefaultRenewMaxMin.GetBuffer(80);
	_itoa(tmp, buf, 10);
    m_newDefaultRenewMaxMin.ReleaseBuffer();

	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEW_MAX_D, m_newDefaultRenewMaxDay);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEW_MAX_H, m_newDefaultRenewMaxHr);
	::SetDlgItemText(::GetForegroundWindow(), IDC_EDIT_RENEW_MAX_M, m_newDefaultRenewMaxMin);
}

void CKrbMiscConfigOpt::OnCheckUseKrb4()
{
    m_newUseKrb4 = (BOOL)IsDlgButtonChecked(IDC_CHECK_REQUEST_KRB4);
}

void CKrbMiscConfigOpt::OnCheckKinitPreserve()
{
    m_newKinitPreserve = (BOOL)IsDlgButtonChecked(IDC_CHECK_PRESERVE_KINIT_OPTIONS);
}

void CKrbMiscConfigOpt::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CPropertyPage::OnShowWindow(bShow, nStatus);

    if (CLeashApp::m_hKrb5DLL)
        ResetDefaultLifeTimeEditBox();

	SetDlgItemText(IDC_EDIT_LIFETIME_D, m_newDefaultLifeTimeDay);
	SetDlgItemText(IDC_EDIT_LIFETIME_H, m_newDefaultLifeTimeHr);
	SetDlgItemText(IDC_EDIT_LIFETIME_M, m_newDefaultLifeTimeMin);
	SetDlgItemText(IDC_EDIT_RENEWTILL_D, m_newDefaultRenewTillDay);
	SetDlgItemText(IDC_EDIT_RENEWTILL_H, m_newDefaultRenewTillHr);
	SetDlgItemText(IDC_EDIT_RENEWTILL_M, m_newDefaultRenewTillMin);
	SetDlgItemText(IDC_EDIT_LIFE_MIN_D, m_newDefaultLifeMinDay);
	SetDlgItemText(IDC_EDIT_LIFE_MIN_H, m_newDefaultLifeMinHr);
	SetDlgItemText(IDC_EDIT_LIFE_MIN_M, m_newDefaultLifeMinMin);
	SetDlgItemText(IDC_EDIT_LIFE_MAX_D, m_newDefaultLifeMaxDay);
	SetDlgItemText(IDC_EDIT_LIFE_MAX_H, m_newDefaultLifeMaxHr);
	SetDlgItemText(IDC_EDIT_LIFE_MAX_M, m_newDefaultLifeMaxMin);
	SetDlgItemText(IDC_EDIT_RENEW_MIN_D, m_newDefaultRenewMinDay);
	SetDlgItemText(IDC_EDIT_RENEW_MIN_H, m_newDefaultRenewMinHr);
	SetDlgItemText(IDC_EDIT_RENEW_MIN_M, m_newDefaultRenewMinMin);
	SetDlgItemText(IDC_EDIT_RENEW_MAX_D, m_newDefaultRenewMaxDay);
	SetDlgItemText(IDC_EDIT_RENEW_MAX_H, m_newDefaultRenewMaxHr);
	SetDlgItemText(IDC_EDIT_RENEW_MAX_M, m_newDefaultRenewMaxMin);
}

BOOL CKrbMiscConfigOpt::PreTranslateMessage(MSG* pMsg)
{
    if (!m_startupPage2)
    {
        if (m_noLifeTime)
        {
            MessageBox("A lifetime setting of 0 is special in that it means that "
                       "the application is free to pick whatever default it deems "
                       "appropriate",
                       "Leash", MB_OK);
            m_noLifeTime = FALSE;
        }
    }

    m_startupPage2 = FALSE;
    return CPropertyPage::PreTranslateMessage(pMsg);
}


BEGIN_MESSAGE_MAP(CKrbMiscConfigOpt, CPropertyPage)
	//{{AFX_MSG_MAP(CKrbConfigOptions)
	ON_WM_SHOWWINDOW()

	ON_EN_KILLFOCUS(IDC_EDIT_LIFETIME_D, OnEditKillfocusEditDefaultLifeTime)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFETIME_D, OnSelchangeEditDefaultLifeTime)
	ON_EN_KILLFOCUS(IDC_EDIT_LIFETIME_H, OnEditKillfocusEditDefaultLifeTime)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFETIME_H, OnSelchangeEditDefaultLifeTime)
	ON_EN_KILLFOCUS(IDC_EDIT_LIFETIME_M, OnEditKillfocusEditDefaultLifeTime)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFETIME_M, OnSelchangeEditDefaultLifeTime)

	ON_EN_KILLFOCUS(IDC_EDIT_RENEWTILL_D, OnEditKillfocusEditDefaultRenewTill)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEWTILL_D, OnSelchangeEditDefaultRenewTill)
	ON_EN_KILLFOCUS(IDC_EDIT_RENEWTILL_H, OnEditKillfocusEditDefaultRenewTill)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEWTILL_H, OnSelchangeEditDefaultRenewTill)
	ON_EN_KILLFOCUS(IDC_EDIT_RENEWTILL_M, OnEditKillfocusEditDefaultRenewTill)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEWTILL_M, OnSelchangeEditDefaultRenewTill)

	ON_EN_KILLFOCUS(IDC_EDIT_LIFE_MIN_D, OnEditKillfocusEditDefaultLifeMin)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFE_MIN_D, OnSelchangeEditDefaultLifeMin)
	ON_EN_KILLFOCUS(IDC_EDIT_LIFE_MIN_H, OnEditKillfocusEditDefaultLifeMin)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFE_MIN_H, OnSelchangeEditDefaultLifeMin)
	ON_EN_KILLFOCUS(IDC_EDIT_LIFE_MIN_M, OnEditKillfocusEditDefaultLifeMin)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFE_MIN_M, OnSelchangeEditDefaultLifeMin)

	ON_EN_KILLFOCUS(IDC_EDIT_LIFE_MAX_D, OnEditKillfocusEditDefaultLifeMax)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFE_MAX_D, OnSelchangeEditDefaultLifeMax)
	ON_EN_KILLFOCUS(IDC_EDIT_LIFE_MAX_H, OnEditKillfocusEditDefaultLifeMax)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFE_MAX_H, OnSelchangeEditDefaultLifeMax)
	ON_EN_KILLFOCUS(IDC_EDIT_LIFE_MAX_M, OnEditKillfocusEditDefaultLifeMax)
	ON_CBN_SELCHANGE(IDC_EDIT_LIFE_MAX_M, OnSelchangeEditDefaultLifeMax)

	ON_EN_KILLFOCUS(IDC_EDIT_RENEW_MIN_D, OnEditKillfocusEditDefaultRenewMin)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEW_MIN_D, OnSelchangeEditDefaultRenewMin)
	ON_EN_KILLFOCUS(IDC_EDIT_RENEW_MIN_H, OnEditKillfocusEditDefaultRenewMin)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEW_MIN_H, OnSelchangeEditDefaultRenewMin)
	ON_EN_KILLFOCUS(IDC_EDIT_RENEW_MIN_M, OnEditKillfocusEditDefaultRenewMin)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEW_MIN_M, OnSelchangeEditDefaultRenewMin)

	ON_EN_KILLFOCUS(IDC_EDIT_RENEW_MAX_D, OnEditKillfocusEditDefaultRenewMax)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEW_MAX_D, OnSelchangeEditDefaultRenewMax)
	ON_EN_KILLFOCUS(IDC_EDIT_RENEW_MAX_H, OnEditKillfocusEditDefaultRenewMax)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEW_MAX_H, OnSelchangeEditDefaultRenewMax)
	ON_EN_KILLFOCUS(IDC_EDIT_RENEW_MAX_M, OnEditKillfocusEditDefaultRenewMax)
	ON_CBN_SELCHANGE(IDC_EDIT_RENEW_MAX_M, OnSelchangeEditDefaultRenewMax)

    ON_BN_CLICKED(IDC_CHECK_REQUEST_KRB4, OnCheckUseKrb4)
	ON_BN_CLICKED(IDC_CHECK_PRESERVE_KINIT_OPTIONS, OnCheckKinitPreserve)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
