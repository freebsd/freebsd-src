// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil -*-
// leash/LeashUIApplication.h - UIApplication implementation for Leash
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

// The class description for the LeashUIApplication class, which
// implements the UIApplication interfaces.  All applications using
// the windows framework are required to implement this interface.
// Leash is an MFC application, but in order to use the ribbon
// from the windows framework, we must implement this interface so
// that we have a UIApplication to hang the ribbon off of, even if we
// do not make use of any other UIApplication features.

#ifndef LEASH_LEASHUIAPPLICATION_H__
#define LEASH_LEASHUIAPPLICATION_H__

#include <UIRibbon.h>

#define WM_RIBBON_RESIZE (WM_USER + 10)

class LeashUIApplication : public IUIApplication
{
public:
    // The "ribbon state" here is just whether it's minimized, and the
    // contents of the Quick Access Toolbar.
    HRESULT LoadRibbonState(IUIRibbon *ribbon);
    HRESULT SaveRibbonState();
    // Export how much space the ribbon is taking up.
    UINT GetRibbonHeight();
    // Do the real work here, not in the constructor
    static HRESULT CreateInstance(IUIApplication **out, HWND hwnd);

    // IUnknown virtual methods
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv);

    // IUIApplication virtual methods
    HRESULT STDMETHODCALLTYPE OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID,
                                            IUnknown *view, UI_VIEWVERB verb,
                                            INT32 uReasonCode);
    HRESULT STDMETHODCALLTYPE
        OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID,
                          IUICommandHandler **commandHandler);
    HRESULT STDMETHODCALLTYPE
        OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID,
                           IUICommandHandler *commandHandler);

private:
    LeashUIApplication() : refcnt(1), commandHandler(NULL),
                           ribbonFramework(NULL) {}
    HRESULT InitializeRibbon(HWND hwnd);
    static HWND mainwin;
    LONG refcnt;
    UINT ribbonHeight;
    IUICommandHandler *commandHandler;
    IUIFramework *ribbonFramework;
};

#endif // LEASH_LEASHUIAPPLICATION_H__
