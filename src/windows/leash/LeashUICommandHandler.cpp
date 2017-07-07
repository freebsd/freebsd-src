// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil -*-
// leash/LeashUICommandHandler.cpp - implements IUICommandHandler interfaces
//
// Copyright (C) 2014 by the Massachusetts Institute of Technology.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in
//   the documentation and/or other materials provided with the
//   distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// This file contains the class implementation of the leash implementation
// of the UICommandHandler interface.  Its primary responsibility is
// to accept UI events (i.e., button presses) and perform the
// corresponding actions to the leash data structures and display
// presentation.

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include "kfwribbon.h"
#include "LeashUICommandHandler.h"
#include "resource.h"

#include <loadfuncs-leash.h>

// Allowing mixed-case realms has both a machine and user-specific knob,
// and thus needs a function to manage it.
extern DWORD Leash_get_default_uppercaserealm();
extern DECL_FUNC_PTR(Leash_get_default_uppercaserealm);

HRESULT
LeashUICommandHandler::CreateInstance(IUICommandHandler **out, HWND hwnd)
{
    LeashUICommandHandler *handler;

    if (out == NULL)
        return E_POINTER;

    handler = new LeashUICommandHandler();
    handler->mainwin = hwnd;
    *out = static_cast<IUICommandHandler *>(handler);
    return S_OK;
}

ULONG
LeashUICommandHandler::AddRef()
{
    return InterlockedIncrement(&refcnt);
}

ULONG
LeashUICommandHandler::Release()
{
    LONG tmp;

    tmp = InterlockedDecrement(&refcnt);
    if (tmp == 0)
        delete this;
    return tmp;
}

HRESULT
LeashUICommandHandler::QueryInterface(REFIID iid, void **ppv)
{
    if (ppv == NULL)
        return E_POINTER;

    if (iid == __uuidof(IUnknown)) {
        *ppv = static_cast<IUnknown*>(this);
    } else if (iid == __uuidof(IUICommandHandler)) {
        *ppv = static_cast<IUICommandHandler*>(this);
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

// Called by the framework when a control is activated that may require
// an action to be taken, such as a button being pressed or a checkbox
// state flipped.  (It is not called when the user changes tabs on the
// ribbon.)  Just proxy these commands through to the existing MFC
// handlers by sendding the appropriate message to the main window.
// Action only needs to be taken on the EXECUTE verb, so we can
// ignore the additional properties surrouding the action, which would
// be relevant for other verbs.
//
// The commandIds are taken from the XML ribbon description.
HRESULT
LeashUICommandHandler::Execute(UINT32 commandId, UI_EXECUTIONVERB verb,
                               const PROPERTYKEY *key,
                               const PROPVARIANT *currentValue,
                               IUISimplePropertySet *commandExecutionProperties)
{
    if (verb != UI_EXECUTIONVERB_EXECUTE)
        return E_NOTIMPL;

    switch(commandId) {
        case cmdGetTicketButton:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_INIT_TICKET, 1), 0);
            break;
        case cmdRenewTicketButton:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_RENEW_TICKET, 1), 0);
            break;
        case cmdDestroyTicketButton:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_DESTROY_TICKET, 1),
                        0);
            break;
        case cmdMakeDefaultButton:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_MAKE_DEFAULT, 1),
                        0);
            break;
        case cmdChangePasswordButton:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_CHANGE_PASSWORD, 1),
                        0);
            break;
        case cmdIssuedCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_TIME_ISSUED, 1), 0);
            break;
        case cmdRenewUntilCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_RENEWABLE_UNTIL, 1),
                        0);
            break;
        case cmdValidUntilCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_VALID_UNTIL, 1), 0);
            break;
        case cmdEncTypeCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_ENCRYPTION_TYPE, 1),
                        0);
            break;
        case cmdFlagsCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_SHOW_TICKET_FLAGS,
                                                        1), 0);
            break;
        case cmdCcacheNameCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_CCACHE_NAME, 1), 0);
            break;
        case cmdAutoRenewCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_AUTO_RENEW, 1), 0);
            break;
        case cmdExpireAlarmCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_LOW_TICKET_ALARM,
                                                        1), 0);
            break;
        case cmdDestroyOnExitCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_KILL_TIX_ONEXIT, 1),
                        0);
            break;
        case cmdMixedCaseCheckBox:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_UPPERCASE_REALM, 1),
                        0);
            break;
        case cmdHelp:
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(ID_HELP_LEASH32, 1), 0);
            break;
        case cmdAbout:
            // ID_APP_ABOUT (0xe140) is defined in afxres.h, an MFC header
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(0xe140, 1), 0);
            break;
        case cmdExit:
            // Save Ribbon customizations here, since this is the only
            // path to a clean exit from the application.
            if (app != NULL)
                app->SaveRibbonState();
            // ID_APP_EXIT (0xe141) is defined in afxres.h, an MFC header
            SendMessage(mainwin, WM_COMMAND, MAKEWPARAM(0xe141, 1), 0);
            break;
        default:
            // Lots of commands we don't need to pass on
            return S_OK;
    }
    return S_OK;
}

// Looks up a given registry key in this application's Settings space
// (analogous to CWinApp::GetProfileInt()), converting it to a
// (boolean) PROPVARIANT which is returned in *out.  Uses the given
// default value if the registry key cannot be loaded.
static HRESULT
RegKeyToProperty(const char *regkey, bool default, PROPVARIANT *out)
{
    DWORD bsize = sizeof(DWORD), enabled;
    LONG code;

    code = RegGetValue(HKEY_CURRENT_USER,
                       "Software\\MIT\\MIT Kerberos\\Settings",
                       regkey, RRF_RT_DWORD, NULL, &enabled,
                       &bsize);
    if (code == ERROR_FILE_NOT_FOUND) {
        code = ERROR_SUCCESS;
        enabled = default ? 1 : 0;
    }
    if (FAILED(code) || bsize != sizeof(enabled))
        return E_FAIL;
    return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, enabled, out);
}

// Called by the framework when the value of a property needs to be
// re-evaluated, e.g., if it has been explicitly invalidated, or at
// program startup.  This is the way to specify the initial/default
// state for ribbon elements which have state, such as checkboxes.
// The registry values which are modified by the MFC checkbox
// action handlers can be read directly from here in order to present
// a consistent visual interface.  The MFC handlers only write to the
// registry when a value is changed, though, so we must duplicate
// the default values which are hardcoded in CLeashView::sm_viewColumns[]
// and elsewhere in LeashView.cpp.
HRESULT
LeashUICommandHandler::UpdateProperty(UINT32 commandId, REFPROPERTYKEY key,
                                      const PROPVARIANT *currentValue,
                                      PROPVARIANT *newValue)
{
    if (key != UI_PKEY_BooleanValue)
        return E_NOTIMPL;

    // These default values duplicate those hardcoded in
    // CLeashView::sm_viewColumns[] and elsewhere in LeashView.cpp.
    switch(commandId) {
        case cmdIssuedCheckBox:
            return RegKeyToProperty("Issued", false, newValue);
        case cmdRenewUntilCheckBox:
            return RegKeyToProperty("Renewable Until", false, newValue);
        case cmdValidUntilCheckBox:
            return RegKeyToProperty("Valid Until", true, newValue);
        case cmdEncTypeCheckBox:
            return RegKeyToProperty("Encryption Type", false, newValue);
        case cmdFlagsCheckBox:
            return RegKeyToProperty("Flags", false, newValue);
        case cmdCcacheNameCheckBox:
            return RegKeyToProperty("Credential Cache", false, newValue);
        case cmdAutoRenewCheckBox:
            return RegKeyToProperty("AutoRenewTickets", true, newValue);
        case cmdExpireAlarmCheckBox:
            return RegKeyToProperty("LowTicketAlarm", true, newValue);
        case cmdDestroyOnExitCheckBox:
            return RegKeyToProperty("DestroyTicketsOnExit", false, newValue);
        case cmdMixedCaseCheckBox:
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue,
                pLeash_get_default_uppercaserealm(), newValue);
        default:
            return E_NOTIMPL;
    }

    return E_NOTIMPL;
}
