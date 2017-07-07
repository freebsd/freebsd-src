//*****************************************************************************
// File:	lgobals.cpp
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright:	@1998 Massachusetts Institute of Technology - All rights
//              reserved.
// Description:	CPP file for lgobals.cpp. Contains global variables and helper
//		functions
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 02/02/98	ADL	Original
//*****************************************************************************

#include "stdafx.h"
#include "leash.h"
#include <direct.h>
#include "lglobals.h"

static const char *const conf_yes[] = {
    "y", "yes", "true", "t", "1", "on",
    0,
};

static const char *const conf_no[] = {
    "n", "no", "false", "nil", "0", "off",
    0,
};

int
config_boolean_to_int(const char *s)
{
    const char *const *p;

    for(p=conf_yes; *p; p++) {
        if (!strcasecmp(*p,s))
            return 1;
    }

    for(p=conf_no; *p; p++) {
        if (!strcasecmp(*p,s))
            return 0;
    }

    /* Default to "no" */
    return 0;
}


// Global Function for deleting or putting a value in the Registry
BOOL SetRegistryVariable(const CString& regVariable,
                         const CString& regValue,
                         const char* regSubKey)
{
    // Set Register Variable
    HKEY hKey = NULL;
    LONG err = 0L;


    if (ERROR_SUCCESS != (err = RegOpenKeyEx(HKEY_CURRENT_USER,
                                             regSubKey,
                                             0, KEY_ALL_ACCESS, &hKey)))
    {
        if ((err = RegCreateKeyEx(HKEY_CURRENT_USER, regSubKey, 0, 0, 0,
                                  KEY_ALL_ACCESS, 0, &hKey, 0)))
        {
            // Error
            return TRUE;
        }
    }

    if (ERROR_SUCCESS == err && hKey)
    {
        if (regValue.IsEmpty())
        {
            // Delete
            RegDeleteValue(hKey, regVariable);
        }
        else
        {
            // Insure that Name (Variable) is in the Registry and set
            // it's new value
            char nVariable[MAX_PATH+1];
            char* pVARIABLE = nVariable;
            strncpy(pVARIABLE, regValue, MAX_PATH);

            if (ERROR_SUCCESS !=
                RegSetValueEx(hKey, regVariable, 0,
                              REG_SZ, (const unsigned char*)pVARIABLE,
                              lstrlen(regValue)))
            {
                // Error
                return FALSE;
            }
        }

        RegCloseKey(hKey);

        // Send this message to all top-level windows in the system
        ::PostMessage(HWND_BROADCAST, WM_WININICHANGE, 0L, (LPARAM) regSubKey);
        return FALSE;
    }

    return TRUE;
}

VOID LeashErrorBox(LPCSTR errorMsg, LPCSTR insertedString, LPCSTR errorFlag)
{
    CString strMessage;
    strMessage = errorMsg;
    strMessage += ": ";
    strMessage += insertedString;

    MessageBox(CLeashApp::m_hProgram, strMessage, errorFlag, MB_OK);

    //if (*errorFlag == 'E')
    //ASSERT(0); // on error condition only
}

Directory::Directory(LPCSTR pathToValidate)
{
    m_pathToValidate = pathToValidate;
    _getdcwd(_getdrive(), m_savCurPath, sizeof(m_savCurPath));
}

Directory::~Directory()
{
    if (-1 == _chdir(m_savCurPath))
        ASSERT(0);
}

BOOL Directory::IsValidDirectory()
{
    if (-1 == _chdir(m_pathToValidate))
        return FALSE;

    return TRUE;
}

BOOL Directory::IsValidFile()
{
    CFileFind fileFind;
    if (!fileFind.FindFile(m_pathToValidate))
        return FALSE;

    return TRUE;
}
