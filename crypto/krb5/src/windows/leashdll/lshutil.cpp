/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* leashdll/lshutil.cpp - text manipulation for principal entry */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * Leash Principal Edit Control
 *
 * Edit control customized to enter a principal.
 * -Autocomplete functionality using history of previously successful
 *  authentications
 * -Option to automatically convert realm to uppercase as user types
 * -Suggest default realm when no matches available from history
 */

#include <windows.h>
#include <wtypes.h>  // LPOLESTR
#include <Shldisp.h> // IAutoComplete
#include <ShlGuid.h> // CLSID_AutoComplete
#include <shobjidl.h> // IAutoCompleteDropDown
#include <objbase.h> // CoCreateInstance
#include <tchar.h>
#include <map>
#include <vector>

#include "leashwin.h"
#include "leashdll.h"

#pragma comment(lib, "ole32.lib") // CoCreateInstance

//
// DynEnumString:
// IEnumString implementation that can be dynamically updated after creation.
//
class DynEnumString : public IEnumString
{
public:
    // IUnknown
    STDMETHODIMP_(ULONG) AddRef()
    {
        return ++m_refcount;
    }

    STDMETHODIMP_(ULONG) Release()
    {
        ULONG refcount = --m_refcount;
        if (refcount == 0)
            delete this;
        return refcount;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
    {
        IUnknown *punk = NULL;
        if (riid == IID_IUnknown)
            punk = static_cast<IUnknown*>(this);
        else if (riid == IID_IEnumString)
            punk = static_cast<IEnumString*>(this);
        *ppvObject = punk;
        if (punk == NULL)
            return E_NOINTERFACE;
        punk->AddRef();
        return S_OK;
    }

    // IEnumString
public:
    STDMETHODIMP Clone(IEnumString **ppclone)
    {
        LPTSTR *data = m_aStrings.data();
        ULONG count = m_aStrings.size();
        *ppclone = new DynEnumString(count, data);
        return S_OK;
    }

    STDMETHODIMP Next(ULONG count, LPOLESTR *elements, ULONG *pFetched)
    {
        ULONG fetched = 0;
        while (fetched < count) {
            if (m_iter == m_aStrings.end())
                break;
            LPTSTR src = *m_iter++;
            // @TODO: add _UNICODE version
            DWORD nLengthW = ::MultiByteToWideChar(CP_ACP,
                                                   0, &src[0], -1, NULL, 0);
            LPOLESTR copy =
                (LPOLESTR )::CoTaskMemAlloc(sizeof(OLECHAR) * nLengthW);
            if (copy != NULL) {
                if (::MultiByteToWideChar(CP_ACP,
                                          0, &src[0], -1, copy, nLengthW)) {
                    elements[fetched++] = copy;
                } else {
                    // failure...
                    // TODO: debug spew
                    ::CoTaskMemFree(copy);
                    copy = NULL;
                }
            }
        }
        *pFetched = fetched;

        return fetched == count ? S_OK : S_FALSE;
    }

    STDMETHODIMP Reset()
    {
        m_iter = m_aStrings.begin();
        return S_OK;
    }

    STDMETHODIMP Skip(ULONG count)
    {
        for (ULONG i=0; i<count; i++) {
            if (m_iter == m_aStrings.end()) {
                m_iter = m_aStrings.begin();
                break;
            }
            m_iter++;
        }
        return S_OK;
    }

    // Custom interface
    DynEnumString(ULONG count, LPTSTR *strings)
    {
        m_aStrings.reserve(count + 1);
        for (ULONG i = 0; i < count; i++) {
            AddString(strings[i]);
        }
        m_iter = m_aStrings.begin();
        m_refcount = 1;
    }

    virtual ~DynEnumString()
    {
        RemoveAll();
    }

    void RemoveAll()
    {
        for (m_iter = m_aStrings.begin();
             m_iter != m_aStrings.end();
             m_iter++)
            delete[] (*m_iter);
        m_aStrings.erase(m_aStrings.begin(), m_aStrings.end());
    }

    void AddString(LPTSTR str)
    {
        LPTSTR copy = NULL;
        if (str) {
            copy = _tcsdup(str);
            if (copy)
                m_aStrings.push_back(copy);
        }
    }


    void RemoveString(LPTSTR str)
    {
        std::vector<LPTSTR>::const_iterator i;
        for (i = m_aStrings.begin(); i != m_aStrings.end(); i++) {
            if (_tcscmp(*i, str) == 0) {
                delete[] (*i);
                m_aStrings.erase(i);
                break;
            }
        }
    }

private:
    ULONG m_refcount;
    std::vector<LPTSTR>::iterator m_iter;
    std::vector<LPTSTR> m_aStrings;
};

// Registry key to store history of successfully authenticated principals
#define LEASH_REGISTRY_PRINCIPALS_KEY_NAME "Software\\MIT\\Leash\\Principals"

// Free principal list obtained by getPrincipalList()
static void freePrincipalList(LPTSTR *princs, int count)
{
    int i;
    if (count) {
        for (i = 0; i < count; i++)
            if (princs[i])
                free(princs[i]);
        delete[] princs;
    }
}

// Retrieve history of successfully authenticated principals from registry
static void getPrincipalList(LPTSTR **outPrincs, int *outPrincCount)
{
    DWORD count = 0;
    DWORD valCount = 0;
    DWORD maxLen = 0;
    LPTSTR tempValName = NULL;
    LPTSTR *princs = NULL;
    *outPrincs = NULL;
    HKEY hKey = NULL;
    unsigned long rc = RegCreateKeyEx(HKEY_CURRENT_USER,
                                      LEASH_REGISTRY_PRINCIPALS_KEY_NAME, 0, 0,
                                      0, KEY_READ, 0, &hKey, 0);
    if (rc == S_OK) {
        // get string count
        rc = RegQueryInfoKey(
            hKey,
            NULL, // __out_opt    LPTSTR lpClass,
            NULL, // __inout_opt  LPDWORD lpcClass,
            NULL, // __reserved   LPDWORD lpReserved,
            NULL, // __out_opt    LPDWORD lpcSubKeys,
            NULL, // __out_opt    LPDWORD lpcMaxSubKeyLen,
            NULL, // __out_opt    LPDWORD lpcMaxClassLen,
            &valCount, //__out_opt    LPDWORD lpcValues,
            &maxLen, // __out_opt    LPDWORD lpcMaxValueNameLen,
            NULL, // __out_opt    LPDWORD lpcMaxValueLen,
            NULL, // __out_opt    LPDWORD lpcbSecurityDescriptor,
            NULL  // __out_opt    PFILETIME lpftLastWriteTime
        );
    }
    if (valCount == 0)
        goto cleanup;

    princs = new LPTSTR[valCount];
    if (princs == NULL)
        goto cleanup;

    tempValName = new TCHAR[maxLen+1];
    if (tempValName == NULL)
        goto cleanup;

    // enumerate values...
    for (DWORD iReg = 0; iReg < valCount; iReg++) {
        LPTSTR princ = NULL;
        DWORD size = maxLen+1;
        rc = RegEnumValue(hKey, iReg, tempValName, &size,
                          NULL, NULL, NULL, NULL);
        if (rc == ERROR_SUCCESS)
            princ = _tcsdup(tempValName);
        if (princ != NULL)
            princs[count++] = princ;
    }

    *outPrincCount = count;
    count = 0;
    *outPrincs = princs;
    princs = NULL;

cleanup:
    if (tempValName)
        delete[] tempValName;
    if (princs)
        freePrincipalList(princs, count);
    if (hKey)
        RegCloseKey(hKey);
    return;
}


// HookWindow
// Utility class to process messages relating to the specified hwnd
class HookWindow
{
public:
    typedef std::pair<HWND, HookWindow*> map_elem;
    typedef std::map<HWND, HookWindow*> map;

    HookWindow(HWND in_hwnd) : m_hwnd(in_hwnd)
    {
        // add 'this' to static hash
        m_ctrl_id = GetDlgCtrlID(in_hwnd);
        m_parent = ::GetParent(m_hwnd);
        sm_map.insert(map_elem(m_parent, this));
        // grab current window proc and replace with our wndproc
        m_parent_wndproc = SetWindowLongPtr(m_parent,
                                            GWLP_WNDPROC,
                                            (ULONG_PTR)(&sWindowProc));
    }

    virtual ~HookWindow()
    {
        // unhook hwnd and restore old wndproc
        SetWindowLongPtr(m_parent, GWLP_WNDPROC, m_parent_wndproc);
        sm_map.erase(m_parent);
    }

    // Process a message
    // return 'false' to forward message to parent wndproc
    virtual bool WindowProc(UINT msg, WPARAM wParam, LPARAM lParam,
                            LRESULT *lr) = 0;

protected:
    static LRESULT sWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                               LPARAM lParam);

    HWND m_hwnd;
    HWND m_parent;
    ULONG_PTR m_parent_wndproc;
    int m_ctrl_id;

    static map sm_map;
};

HookWindow::map HookWindow::sm_map;

LRESULT HookWindow::sWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam)
{
    LRESULT result;
    // hash hwnd to get object and call actual window proc,
    // then parent window proc as necessary
    HookWindow::map::const_iterator iter = sm_map.find(hwnd);
    if (iter != sm_map.end()) {
        if (!iter->second->WindowProc(uMsg, wParam, lParam, &result))
            result = CallWindowProc((WNDPROC )iter->second->m_parent_wndproc,
                           hwnd, uMsg, wParam, lParam);
    } else {
        result = ::DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return result;
}

//
class PrincipalEditControl : public HookWindow
{
public:
    PrincipalEditControl(HWND hwnd, bool bUpperCaseRealm) : HookWindow(hwnd)
        ,m_ignore_change(0)
        ,m_bUpperCaseRealm(bUpperCaseRealm)
        ,m_defaultRealm(NULL)
        ,m_ctx(0)
        ,m_enumString(NULL)
        ,m_acdd(NULL)
        ,m_princStr(NULL)
    {
        pkrb5_init_context(&m_ctx);
        GetDefaultRealm();
        InitAutocomplete();
    }

    ~PrincipalEditControl()
    {
        DestroyAutocomplete();
        if (m_princStr)
            delete[] m_princStr;
        if (m_ctx && m_defaultRealm)
            pkrb5_free_default_realm(m_ctx, m_defaultRealm);
        if (m_ctx)
            pkrb5_free_context(m_ctx);
    }

    void ClearHistory()
    {
        if (m_enumString != NULL)
            m_enumString->RemoveAll();
        if (m_acdd != NULL)
            m_acdd->ResetEnumerator();
        if (m_princStr != NULL) {
            delete[] m_princStr;
            m_princStr = NULL;
        }
    }

protected:
    // Convert str to upper case
    // This should be more-or-less _UNICODE-agnostic
    static bool StrToUpper(LPTSTR str)
    {
        bool bChanged = false;
        int c;
        if (str != NULL) {
            while ((c = *str) != NULL) {
                if (__isascii(c) && islower(c)) {
                    bChanged = true;
                    *str = _toupper(c);
                }
                str++;
            }
        }
        return bChanged;
    }

    void GetDefaultRealm()
    {
        // @TODO: _UNICODE support here
        if ((m_defaultRealm == NULL) && m_ctx) {
            pkrb5_get_default_realm(m_ctx, &m_defaultRealm);
        }
    }

    // Append default realm to user and add to the autocomplete enum string
    void SuggestDefaultRealm(LPTSTR user)
    {
        if (m_defaultRealm == NULL)
            return;

        int princ_len = _tcslen(user) + _tcslen(m_defaultRealm) + 1;
        LPTSTR princStr = new TCHAR[princ_len];
        if (princStr) {
            _sntprintf_s(princStr, princ_len, _TRUNCATE, "%s%s", user,
                         m_defaultRealm);
            if (m_princStr != NULL && (_tcscmp(princStr, m_princStr) == 0)) {
                // this string is already added, ok to just bail
                delete[] princStr;
            } else {
                if (m_princStr != NULL) {
                    // get rid of the old suggestion
                    m_enumString->RemoveString(m_princStr);
                    delete[] m_princStr;
                }
                // add the new one
                m_enumString->AddString(princStr);
                if (m_acdd != NULL)
                    m_acdd->ResetEnumerator();
                m_princStr = princStr;
            }
        }
    }

    bool AdjustRealmCase(LPTSTR princStr, LPTSTR realmStr)
    {
        bool bChanged = StrToUpper(realmStr);
        if (bChanged) {
            DWORD selStart, selEnd;
            ::SendMessage(m_hwnd, EM_GETSEL, (WPARAM)&selStart,
                          (LPARAM)&selEnd);
            ::SetWindowText(m_hwnd, princStr);
            ::SendMessage(m_hwnd, EM_SETSEL, (WPARAM)selStart, (LPARAM)selEnd);
        }
        return bChanged;
    }

    bool ProcessText()
    {
        bool bChanged = false;
        int text_len = GetWindowTextLength(m_hwnd);
        if (text_len > 0) {
            LPTSTR str = new TCHAR [++text_len];
            if (str != NULL) {
                GetWindowText(m_hwnd, str, text_len);
                LPTSTR realmStr = strchr(str, '@');
                if (realmStr != NULL) {
                    ++realmStr;
                    if (*realmStr == 0) {
                        SuggestDefaultRealm(str);
                    }
                    else if (m_bUpperCaseRealm) {
                        AdjustRealmCase(str, realmStr);
                        bChanged = true;
                    }
                }
                delete[] str;
            }
        }
        return bChanged;
    }

    virtual bool WindowProc(UINT msg, WPARAM wp, LPARAM lp, LRESULT *lr)
    {
        bool bChanged = false;
        switch (msg) {
        case WM_COMMAND:
            if ((LOWORD(wp)==m_ctrl_id) &&
                (HIWORD(wp)==EN_CHANGE)) {
                if ((!m_ignore_change++) && ProcessText()) {
                    bChanged = true;
                    *lr = 0;
                }
                m_ignore_change--;
            }
        default:
            break;
        }
        return bChanged;
    }

    void InitAutocomplete()
    {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

        // read strings from registry
        LPTSTR *princs = NULL;
        int count = 0;
        getPrincipalList(&princs, &count);

        // Create our custom IEnumString implementation
        HRESULT hRes;
        DynEnumString *pEnumString = new DynEnumString(count, princs);
        if (princs)
            freePrincipalList(princs, count);

        m_enumString = pEnumString;

        // Create and initialize IAutoComplete object using IEnumString
        IAutoComplete *pac = NULL;
        hRes = CoCreateInstance(CLSID_AutoComplete, NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pac));
        if (pac != NULL) {
            pac->Init(m_hwnd, pEnumString, NULL, NULL);

            IAutoCompleteDropDown* pacdd = NULL;
            hRes = pac->QueryInterface(IID_IAutoCompleteDropDown, (LPVOID*)&pacdd);
            pac->Release();
            m_acdd = pacdd;
        }
    }

    void DestroyAutocomplete()
    {
        if (m_acdd != NULL)
            m_acdd->Release();
        if (m_enumString != NULL)
            m_enumString->Release();
    }

    int m_ignore_change;
    bool m_bUpperCaseRealm;
    LPTSTR m_defaultRealm;
    LPTSTR m_princStr;
    krb5_context m_ctx;
    DynEnumString *m_enumString;
    IAutoCompleteDropDown *m_acdd;
};



extern "C" void Leash_pec_add_principal(char *principal)
{
    // write princ to registry
    HKEY hKey;
    unsigned long rc = RegCreateKeyEx(HKEY_CURRENT_USER,
                                      LEASH_REGISTRY_PRINCIPALS_KEY_NAME,
                                      0, 0, 0, KEY_WRITE, 0, &hKey, 0);
    if (rc) {
        // TODO: log failure
        return;
    }
    rc = RegSetValueEx(hKey, principal, 0, REG_NONE, NULL, 0);
    if (rc) {
        // TODO: log failure
    }
    if (hKey)
        RegCloseKey(hKey);
}

extern "C" void Leash_pec_clear_history(void *pec)
{
    // clear princs from registry
    RegDeleteKey(HKEY_CURRENT_USER,
                 LEASH_REGISTRY_PRINCIPALS_KEY_NAME);
    // ...and from the specified widget
    static_cast<PrincipalEditControl *>(pec)->ClearHistory();
}


extern "C" void *Leash_pec_create(HWND hEdit)
{
    return new PrincipalEditControl(
        hEdit,
        Leash_get_default_uppercaserealm() ? true : false);
}

extern "C" void Leash_pec_destroy(void *pec)
{
    if (pec != NULL)
        delete ((PrincipalEditControl *)pec);
}
