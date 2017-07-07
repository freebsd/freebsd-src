/*

Copyright 2004 by the Massachusetts Institute of Technology

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/* custom.h
 *
 * Declarations for Kerberos for Windows MSI setup tools
 *
 * rcsid : $Id$
 */

#pragma once

#include<windows.h>
#include<setupapi.h>
#include<msiquery.h>
#include<string.h>
#include<tchar.h>
#include<tlhelp32.h>

#define MSIDLLEXPORT UINT __stdcall

#define CHECK(x)	if((x)) goto _cleanup

#define CHECKX(x,y) if(!(x)) { msiErr = (y); goto _cleanup; }

#define CHECK2(x,y)  if((x)) { msiErr = (y); goto _cleanup; }

#define STR_KEY_ORDER _T("SYSTEM\\CurrentControlSet\\Control\\NetworkProvider\\Order")
#define STR_VAL_ORDER _T("ProviderOrder")

#define STR_SERVICE _T("MIT Kerberos")
#define STR_SERVICE_LEN 12


void ShowMsiError(MSIHANDLE, DWORD, DWORD);
UINT SetAllowTgtSessionKey( MSIHANDLE hInstall, BOOL pInstall );
UINT KillRunningProcessesSlave( MSIHANDLE hInstall, BOOL bKill );

/* exported */
MSIDLLEXPORT AbortMsiImmediate( MSIHANDLE );
MSIDLLEXPORT UninstallNsisInstallation( MSIHANDLE hInstall );
MSIDLLEXPORT RevertAllowTgtSessionKey( MSIHANDLE hInstall );
MSIDLLEXPORT EnableAllowTgtSessionKey( MSIHANDLE hInstall );
MSIDLLEXPORT KillRunningProcesses( MSIHANDLE hInstall ) ;
MSIDLLEXPORT ListRunningProcesses( MSIHANDLE hInstall );
MSIDLLEXPORT InstallNetProvider( MSIHANDLE );
MSIDLLEXPORT UninstallNetProvider ( MSIHANDLE );

#define INP_ERR_PRESENT 1
#define INP_ERR_ADDED   2
#define INP_ERR_ABSENT  3
#define INP_ERR_REMOVED 4

/* Custom errors */
#define ERR_CUSTACTDATA 4001
#define ERR_NSS_FAILED  4003
#define ERR_ABORT       4004
#define ERR_PROC_LIST   4006
#define ERR_NPI_FAILED  4007
#define ERR_NSS_FAILED_CP 4008
