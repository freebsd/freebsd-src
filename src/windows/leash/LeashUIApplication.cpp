// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil -*-
// leash/LeashUIApplication.cpp - Implement IUIApplication for leash
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

// Implementation of the LeashUIApplication class.  In addition
// to the minimum requirements for the IUIApplication interface,
// it also saves and loads the ribbon state across application
// sessions, and initiates a redraw of the parent window when
// the ribbon size changes.

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include "kfwribbon.h"
#include "LeashUIApplication.h"
#include "LeashUICommandHandler.h"

HWND LeashUIApplication::mainwin;

// The input hwnd is the window to which to bind the ribbon, i.e.,
// the Leash CMainFrame.
HRESULT
LeashUIApplication::CreateInstance(IUIApplication **out, HWND hwnd)
{
    LeashUIApplication *app = NULL;
    LeashUICommandHandler *handler;
    HRESULT ret;

    if (out == NULL)
        return E_POINTER;
    *out = NULL;

    app = new LeashUIApplication();
    ret = LeashUICommandHandler::CreateInstance(&app->commandHandler, hwnd);
    if (FAILED(ret))
        goto out;
    ret = app->InitializeRibbon(hwnd);
    if (FAILED(ret))
        goto out;
    mainwin = hwnd;
    // Only the Leash-specific handler type has the back-pointer.
    handler = static_cast<LeashUICommandHandler *>(app->commandHandler);
    handler->app = app;
    *out = static_cast<IUIApplication *>(app);
    app = NULL;
    ret = S_OK;

out:
    if (app != NULL)
        app->Release();
    return ret;
}

// Create a ribbon framework and ribbon for the LeashUIApplication.
// CoInitializeEx() is required to be called before calling any COM
// functions.  AfxOleInit(), called from CLeashApp::InitInstance(),
// makes that call, but it is only scoped to the calling thread,
// and the LeashUIApplication is created from CMainFrame, which is
// the frame for the MFC document template.  It is unclear if the
// Leash main thread will be the same thread which runs the frame
// from the document template, so call CoInitializeEx() ourselves
// just in case.  It is safe to call multiple times (it will return
// S_FALSE on subsequent calls).
HRESULT
LeashUIApplication::InitializeRibbon(HWND hwnd)
{
    HRESULT ret;

    if (hwnd == NULL)
        return -1;
    ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(ret))
        return ret;
    ret = CoCreateInstance(CLSID_UIRibbonFramework, NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&ribbonFramework));
    if (FAILED(ret))
        return ret;
    ret = ribbonFramework->Initialize(hwnd, this);
    if (FAILED(ret))
        return ret;
    ret = ribbonFramework->LoadUI(GetModuleHandle(NULL),
                                  L"KFW_RIBBON_RIBBON");
    if (FAILED(ret))
        return ret;
    return S_OK;
}

// Import ribbon state (minimization state and Quick Access Toolbar
// customizations) from a serialized stream stored in the registry.
// In particular, the serialized state does not include the state
// of checkboxes and other ribbon controls.
//
// This functionality is not very important, since we do not offer
// much in the way of QAT customization.  Paired with SaveRibbonState().
HRESULT
LeashUIApplication::LoadRibbonState(IUIRibbon *ribbon)
{
    HRESULT ret;
    IStream *s;

    s = SHOpenRegStream2(HKEY_CURRENT_USER, "Software\\MIT\\Kerberos5",
                         "RibbonState", STGM_READ);
    if (s == NULL)
        return E_FAIL;
    ret = ribbon->LoadSettingsFromStream(s);
    s->Release();
    return ret;
}

// Serialize the ribbon state (minimization state and Quick Access Toolbar
// customizations) to the registry.  Paired with LoadRibbonState().
HRESULT
LeashUIApplication::SaveRibbonState()
{
    HRESULT ret;
    IStream *s = NULL;
    IUIRibbon *ribbon = NULL;

    // No ribbon means no state to save.
    if (ribbonFramework == NULL)
        return S_OK;
    // ViewID of 0 is the ribbon itself.
    ret = ribbonFramework->GetView(0, IID_PPV_ARGS(&ribbon));
    if (FAILED(ret))
        return ret;

    s = SHOpenRegStream2(HKEY_CURRENT_USER, "Software\\MIT\\Kerberos5",
                         "RibbonState", STGM_WRITE);
    if (s == NULL) {
        ret = E_FAIL;
        goto out;
    }
    ret = ribbon->SaveSettingsToStream(s);

out:
    if (s != NULL)
        s->Release();
    if (ribbon != NULL)
        ribbon->Release();
    return ret;
}

UINT
LeashUIApplication::GetRibbonHeight()
{
    return ribbonHeight;
}

ULONG
LeashUIApplication::AddRef()
{
    return InterlockedIncrement(&refcnt);
}

ULONG
LeashUIApplication::Release()
{
    LONG tmp;

    tmp = InterlockedDecrement(&refcnt);
    if (tmp == 0) {
        if (commandHandler != NULL)
            commandHandler->Release();
        if (ribbonFramework != NULL)
            ribbonFramework->Release();
        delete this;
    }
    return tmp;
}

HRESULT
LeashUIApplication::QueryInterface(REFIID iid, void **ppv)
{
    if (ppv == NULL)
        return E_POINTER;

    if (iid == __uuidof(IUnknown)) {
        *ppv = static_cast<IUnknown*>(this);
    } else if (iid == __uuidof(IUIApplication)) {
        *ppv = static_cast<IUIApplication*>(this);
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

// This is called by the ribbon framework on events which change the (ribbon)
// view, such as creation and resizing.  (There may be other non-ribbon views
// in the future, but for now, the ribbon is the only one.)  With the hybrid
// COM/MFC setup used by Leash, the destroy event is not always received,
// since the main thread is in the MFC half, and that thread gets the
// WM_DESTROY message from the system; the MFC code does not know that it
// needs to cleanly destroy the IUIFramework.
HRESULT
LeashUIApplication::OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID,
                                  IUnknown *view, UI_VIEWVERB verb,
                                  INT32 uReasonCode)
{
    IUIRibbon *ribbon;
    HRESULT ret;

    // A viewId means "the ribbon".
    if (viewId != 0 || typeID != UI_VIEWTYPE_RIBBON)
        return E_NOTIMPL;

    switch(verb) {
        case UI_VIEWVERB_DESTROY:
            return SaveRibbonState();
        case UI_VIEWVERB_CREATE:
            ret = view->QueryInterface(IID_PPV_ARGS(&ribbon));
            if (FAILED(ret))
                return ret;
            ret = LoadRibbonState(ribbon);
            ribbon->Release();
            if (FAILED(ret))
                return ret;
            // FALLTHROUGH
        case UI_VIEWVERB_SIZE:
            ret = view->QueryInterface(IID_PPV_ARGS(&ribbon));
            if (FAILED(ret))
                return ret;
            ret = ribbon->GetHeight(&ribbonHeight);
            ribbon->Release();
            if (FAILED(ret))
                return ret;
            // Tell the main frame to recalculate its layout and redraw.
            SendMessage(mainwin, WM_RIBBON_RESIZE, 0, NULL);
            return S_OK;
        case UI_VIEWVERB_ERROR:
            // FALLTHROUGH
        default:
            return E_NOTIMPL;
    }
}

// Provide a command handler to which the command with ID commandId will
// be bound.  All of our commands get the same handler.
//
// The typeID argument is just an enum which classifies what type of
// command this is, grouping types of buttons together, collections,
// etc.  Since we only have one command handler, it can safely be ignored.
HRESULT
LeashUIApplication::OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID,
                                      IUICommandHandler **commandHandler)
{
    return this->commandHandler->QueryInterface(IID_PPV_ARGS(commandHandler));
}

// It looks like this is called by the framework when the window with the
// ribbon is going away, to give the application a chance to free any
// application-specific resources (not from the framwork) that were bound
// to a command in OnCreateUICommand.
//
// We do not have any such resources, so we do not need to implement this
// function other than by returning success.
HRESULT
LeashUIApplication::OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID,
                                       IUICommandHandler *commandHandler)
{
    return S_OK;
}
