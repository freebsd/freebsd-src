// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil -*-
// leash/LeashUICommandHandler.h - implements IUICommandHandler interfaces
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

// This file contains the class definition for the leash implementation
// of the UICommandHandler interface.  Its primary responsibility is
// to accept UI events (i.e., button presses) and perform the
// corresponding actions to the leash data structures and display
// presentation.  It also supplies values for the state of various
// interface elements when the framework needs an authoritative value.

#ifndef WINDOWS_LEASHUICOMMANDHANDLER_H__
#define WINDOWS_LEASHUICOMMANDHANDLER_H__

#include <UIRibbon.h>
#include "LeashUIApplication.h"

class LeashUICommandHandler : public IUICommandHandler
{
public:
    LeashUIApplication *app;
    // Actual work for creation is done here, not the constructor.
    static HRESULT CreateInstance(IUICommandHandler **out, HWND hwnd);

    // IUnknown virtual methods
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv);

    // IUICommandHandler virtual methods
    HRESULT STDMETHODCALLTYPE Execute(UINT32 commandId, UI_EXECUTIONVERB verb,
                    const PROPERTYKEY *key, const PROPVARIANT *currentValue,
                    IUISimplePropertySet *commandExecutionProperties);
    HRESULT STDMETHODCALLTYPE UpdateProperty(UINT32 commandId,
                                             REFPROPERTYKEY key,
                                             const PROPVARIANT *currentValue,
                                             PROPVARIANT *newValue);

private:
    LeashUICommandHandler() : refcnt(1) {}
    HWND mainwin; // Something to which to send messages.
    LONG refcnt;
};

#endif // WINDOWS_LEASHUICOMMANDHANDLER_H__
