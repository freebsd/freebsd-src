/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* dllmain.c - main function to krb4.dll
 * Author:	J Karlsson <d93-jka@nada.kth.se>
 * Date:	June 1996
 */

#include "krb_locl.h"
#include "ticket_memory.h"
#include <Windows.h>

RCSID("$Id: dllmain.c,v 1.9 1999/12/02 16:58:41 joda Exp $");

void
msg(char *text, int error)
{
    char *buf;

    asprintf (&buf, "%s\nAn error of type: %d", text, error);

    MessageBox(GetActiveWindow(),
	       buf ? buf : "Out of memory!",
	       "kerberos message",
	       MB_OK|MB_APPLMODAL);
    free (buf);
}

void
PostUpdateMessage(void)
{
	HWND		hWnd;
	static UINT km_message;
	
    if(km_message == 0)
        km_message = RegisterWindowMessage("krb4-update-cache");

	hWnd = FindWindow("KrbManagerWndClass", NULL);
	if (hWnd == NULL)
		hWnd = HWND_BROADCAST;
	PostMessage(hWnd, km_message, 0, 0);
}


BOOL WINAPI
DllMain (HANDLE hInst, 
	 ULONG reason,
	 LPVOID lpReserved)
{
    WORD wVersionRequested; 
    WSADATA wsaData; 
    PROCESS_INFORMATION p;	
    int err; 

    switch(reason){
    case DLL_PROCESS_ATTACH:
	wVersionRequested = MAKEWORD(1, 1); 
	err = WSAStartup(wVersionRequested, &wsaData); 
	if (err != 0) 
	{
	    /* Tell the user that we couldn't find a useable */ 
	    /* winsock.dll.     */ 
	    msg("Cannot find winsock.dll", err);
	    return FALSE;
	}
	if(newTktMem(0) != KSUCCESS)
	{
	    /* Tell the user that we couldn't alloc shared memory. */ 
	    msg("Cannot allocate shared ticket memory", GetLastError());
	    return FALSE;
	}
	if(GetLastError() != ERROR_ALREADY_EXISTS)
	{
	    STARTUPINFO s = {
		sizeof(s),
		NULL,
		NULL,
		NULL,
		0,0,
		0,0,
		0,0,
		0,
		STARTF_USESHOWWINDOW,
		SW_SHOWMINNOACTIVE,
		0, NULL,
		NULL, NULL, NULL
	    };

	    if(!CreateProcess(0,"krbmanager",
			      0,0,FALSE,0,0,
			      0,&s, &p)) {
#if 0
		msg("Unable to create Kerberos manager process.\n"
		    "Make sure krbmanager.exe is in your PATH.",
		    GetLastError());
		return FALSE;
#endif
	    }
	}
	break;
    case DLL_PROCESS_DETACH:
	/* should this really be done here? */
	freeTktMem(0);
	WSACleanup();
	break;
    }

    return TRUE;
}
