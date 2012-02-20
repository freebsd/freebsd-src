/*$Header: /p/tcsh/cvsroot/tcsh/win32/clip.c,v 1.9 2006/03/05 08:59:36 amold Exp $*/
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * clip.c : support for clipboard functions.
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "sh.h"
#include "ed.h"

BOOL InitApplication(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


HWND ghwndmain;

extern int ctrl_handler(DWORD);
extern void c_insert(int);

/* 
 * Creating a hidden window may not be strictly necessary on
 * NT, but why tempt fate ?
 * -amol
 */

void  clipper_thread(void) {

   MSG msg;
   HINSTANCE hInstance = GetModuleHandle(NULL);


   if (!InitApplication(hInstance)) {
	   return ;
   }

   if (!InitInstance(hInstance, 0)) {
	   return ;
   }
   // Main message loop:
   while (GetMessage(&msg, NULL, 0, 0)) {
	   TranslateMessage(&msg);
	   DispatchMessage(&msg);
   }
   if ( !ctrl_handler(CTRL_CLOSE_EVENT))
   		init_clipboard();
	return;
}
void init_clipboard(void) {
	HANDLE ht;
	DWORD tid;

	ht = CreateThread(NULL,gdwStackSize,
					(LPTHREAD_START_ROUTINE)clipper_thread, NULL,0,&tid);

	if (!ht)
		abort();
	CloseHandle(ht);
}

BOOL InitApplication(HINSTANCE hInstance)
{
    WNDCLASS  wc;


	// Fill in window class structure with parameters that describe
	// the main window.
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = (WNDPROC)WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = NULL;//LoadIcon (hInstance, szAppName);
	wc.hCursor       = NULL;//LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)IntToPtr(COLOR_WINDOW+1);

	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "tcshclipboard";

   return RegisterClass(&wc);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
   HWND hWnd;


   UNREFERENCED_PARAMETER(nCmdShow);

   hWnd = CreateWindow("tcshclipboard", "tcshclipboard", 
   			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
			  NULL, NULL, hInstance, NULL);

   if (!hWnd) {
      return (FALSE);
   }

   UpdateWindow(hWnd);
   ghwndmain = hWnd;

   return (TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

   switch (message) {

	  case WM_DESTROYCLIPBOARD:
	  	break;
      case WM_DESTROY:
         PostQuitMessage(0);
         break;

      default:
         return (DefWindowProc(hWnd, message, wParam, lParam));
   }
   return (0);
}

CCRETVAL e_copy_to_clipboard(Char c) {
	unsigned char *cbp;
	Char *kp;
	int err;
	size_t len;
	unsigned char *clipbuf;
	HANDLE hclipbuf;

	UNREFERENCED_PARAMETER(c);

	if (!ghwndmain)	
		return (CC_ERROR);
	
	if (KillRingLen == 0)
		return (CC_ERROR);

	len = Strlen(KillRing[YankPos].buf);

	hclipbuf = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, len+2);
	if (!hclipbuf)
		return (CC_ERROR);
	clipbuf = (unsigned char*)GlobalLock(hclipbuf);

	if (!clipbuf){
		err = GetLastError();
		GlobalFree(hclipbuf);
		return (CC_ERROR);
	}
	
	kp = KillRing[YankPos].buf;
	cbp = clipbuf;
		
	while(*kp != '\0') {
		*cbp = (u_char)(*kp & CHAR);
		cbp++;kp++;
	}
	*cbp = 0;

	GlobalUnlock(clipbuf);

	if (!OpenClipboard(ghwndmain))
		goto error;

	if (!EmptyClipboard())
		goto error;
		
	if (SetClipboardData(CF_TEXT,hclipbuf) != hclipbuf){
		err = GetLastError();
		goto error;

	}

	CloseClipboard();
	return (CC_NORM);
error:
	GlobalFree(hclipbuf);
	CloseClipboard();
	return (CC_ERROR);
}
CCRETVAL e_paste_from_clipboard(Char c) {
	HANDLE hclip;
	unsigned char *cbp;
	Char *cp;
	int len;
	unsigned char *clipbuf;



	UNREFERENCED_PARAMETER(c);

	if (!ghwndmain)	
		return (CC_ERROR);
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return CC_ERROR;
	
	if (!OpenClipboard(ghwndmain))
		return CC_ERROR;
	

	hclip = GetClipboardData(CF_TEXT);
	if (hclip) {
		clipbuf = (unsigned char*)GlobalLock(hclip);

		cbp = clipbuf;
		len = 0;

		while(*cbp && *cbp != '\r') {
			len++;
			cbp++;
		}
		cbp  = clipbuf;

		cp = Cursor;

		c_insert(len);

		if (LastChar + len >= InputLim)
			goto error;

		while(*cbp && *cbp !='\r' && (cp <LastChar) ) {
			*cp = *cbp ;
			cp++;cbp++;
		}
		Cursor = cp;
		GlobalUnlock(hclip);
	}
	CloseClipboard();

	return (CC_REFRESH);
error: 
	return (CC_ERROR);
}

int is_dev_clipboard_active=0;
HANDLE ghdevclipthread;

/* Reads from pipe and write to clipboard */
void clip_writer_proc(HANDLE hinpipe) {
	unsigned char *realbuf;
	unsigned char *clipbuf;
	unsigned char *ptr;
	DWORD bread=0,spleft,err,i,rbsize;
	DWORD ptrloc;
	HANDLE hclipbuf;


	rbsize = 4096;
	realbuf = heap_alloc(rbsize);
	ptr = realbuf;
	ptrloc = 0;
	spleft = rbsize;

	while(spleft) {
		if (!ReadFile(hinpipe,ptr,spleft,&bread,NULL)) {
			spleft = GetLastError();
			dprintf("hinpipe returend %d\n",spleft);
			if (spleft == ERROR_BROKEN_PIPE)
				break;
		}
		if (bread == 0)
			break;
		ptr += bread;
		ptrloc += bread;
		spleft -=bread;

		if (spleft <=0){
			u_char *tmp;

			rbsize <<=1;

			tmp = realbuf;
			realbuf = heap_realloc(realbuf,rbsize);
			if (!realbuf) {
				realbuf = tmp;
				break;
			}
			spleft += rbsize >> 1;

			ptr = realbuf+ptrloc;

			dprintf("updated size now %d, splef %d, ptrloc %d, ptr 0x%08x, realbuf 0x%08x\n",rbsize,spleft,ptrloc,ptr,realbuf);
		}
	}
	CloseHandle(hinpipe);

	bread = rbsize-spleft;

	hclipbuf = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, bread+256);
	if (!hclipbuf) {
		is_dev_clipboard_active=0;
		return;
	}
	clipbuf = (u_char*)GlobalLock(hclipbuf);

	if (!clipbuf){
		err = GetLastError();
		GlobalFree(hclipbuf);
		is_dev_clipboard_active=0;
		return ;
	}
	ptr = clipbuf;
	for (i=0;i <bread;i++) {

		if (realbuf[i] == '\n' && (i >0 && realbuf[i-1] != '\r') )
			*ptr++ = '\r';

		*ptr++ =realbuf[i];

		if ((ptr - clipbuf) >= rbsize)
			break;
	}
	*ptr=0;

	heap_free(realbuf);

	GlobalUnlock(clipbuf);

	if (!OpenClipboard(ghwndmain))
		goto error;

	if (!EmptyClipboard())
		goto error;
		
	if (SetClipboardData(CF_TEXT,hclipbuf) != hclipbuf){
		err = GetLastError();
		goto error;

	}
	CloseClipboard();
	is_dev_clipboard_active=0;
	return ;
error:
	is_dev_clipboard_active=0;
	GlobalFree(hclipbuf);
	CloseClipboard();
}
HANDLE create_clip_writer_thread(void) {
	HANDLE  hread,hwrite;
	DWORD tid;
	SECURITY_ATTRIBUTES secd;

	if (is_dev_clipboard_active)
		return INVALID_HANDLE_VALUE;
	secd.nLength=sizeof(secd);
	secd.lpSecurityDescriptor=NULL;
	secd.bInheritHandle=FALSE;

	if (!CreatePipe(&hread,&hwrite,&secd,0)) {
		abort();
	}
	is_dev_clipboard_active = 1;
	ghdevclipthread = CreateThread(NULL,gdwStackSize,
				(LPTHREAD_START_ROUTINE)clip_writer_proc, hread,0,&tid);
//	CloseHandle(ht);
	return hwrite;
}

/* Read from clipboard and write to pipe */
void clip_reader_proc(HANDLE houtpipe) {

	HANDLE hclip;
	unsigned char *cbp;
	unsigned char *clipbuf;
	unsigned char * outbuf,*ptr;
	DWORD bwrote, len;
	DWORD obsize;

	obsize = 4096;
	outbuf = heap_alloc(obsize);
	ptr = outbuf;


	if (!IsClipboardFormatAvailable(CF_TEXT))
		goto done ;
	
	if (!OpenClipboard(ghwndmain))
		goto done ;
	

	len = 0;
	hclip = GetClipboardData(CF_TEXT);
	if (hclip) {
		clipbuf = (unsigned char*)GlobalLock(hclip);

		cbp = clipbuf;

		while(*cbp ) {
			*ptr++ = *cbp++;
			len++;
			if (len == obsize) {
				obsize <<= 1;
				outbuf = heap_realloc(outbuf,obsize);
				if (!outbuf)
					break;
				ptr = outbuf+len;
			}
		}
		GlobalUnlock(hclip);
	}
	CloseClipboard();

	if (!WriteFile(houtpipe,outbuf,len,&bwrote,NULL)) {
		;
	}
	CloseHandle(houtpipe);
	heap_free(outbuf);

done:
	is_dev_clipboard_active=0;
	return;
}
HANDLE create_clip_reader_thread(void) {
	HANDLE  hread,hwrite;
	DWORD tid;
	SECURITY_ATTRIBUTES secd;

	if (is_dev_clipboard_active)
		return INVALID_HANDLE_VALUE;

	secd.nLength=sizeof(secd);
	secd.lpSecurityDescriptor=NULL;
	secd.bInheritHandle=FALSE;

	if (!CreatePipe(&hread,&hwrite,&secd,0)) {
		abort();
	}
	is_dev_clipboard_active = 1;
	ghdevclipthread = CreateThread(NULL,gdwStackSize,
				(LPTHREAD_START_ROUTINE)clip_reader_proc, hwrite,0,&tid);
	return hread;
}

CCRETVAL
e_dosify_next(Char c)
{
	register Char *cp, *buf, *bp;
	int len;
    BOOL bDone = FALSE;


	USE(c);
	if (Cursor == LastChar)
		return(CC_ERROR);

	// worst case assumption
	buf = heap_alloc(( LastChar - Cursor + 1)*2*sizeof(Char));

	cp = Cursor;
	bp = buf;
	len = 0;

	while(  cp < LastChar) {
		if ( ((*cp & CHAR) == ' ') && ((cp[-1] & CHAR) != '\\') )
			bDone = TRUE;
		if (!bDone &&  (*cp & CHAR) == '/')  {
			*bp++ = '\\'  | (Char)(*cp & ~(*cp & CHAR) );
			*bp++ = '\\'  | (Char)(*cp & ~(*cp & CHAR) );

			len++;

			cp++;
		}
		else 
			*bp++ = *cp++;

		len++;
	}
	if (Cursor+ len >= InputLim) {
		heap_free(buf);
		return CC_ERROR;
	}
	cp = Cursor;
	bp = buf;
	while(len > 0) {
		*cp++ = *bp++;
		len--;
	}

	heap_free(buf);

	Cursor =  cp;

    if(LastChar < Cursor + len)
        LastChar = Cursor + len;

	return (CC_REFRESH);
}
/*ARGSUSED*/
CCRETVAL
e_dosify_prev(Char c)
{
	register Char *cp;

	USE(c);
	if (Cursor == InputBuf)
		return(CC_ERROR);
	/* else */

	cp = Cursor-1;
	/* Skip trailing spaces */
	while ((cp > InputBuf) && ( (*cp & CHAR) == ' '))
		cp--;

	while (cp > InputBuf) {
		if ( ((*cp & CHAR) == ' ') && ((cp[-1] & CHAR) != '\\') )
			break;
		cp--;
	}
	if(cp != InputBuf)
	  Cursor = cp + 1;
	else
	  Cursor = cp;
	
	return e_dosify_next(0);
}
extern BOOL ConsolePageUpOrDown(BOOL);
CCRETVAL
e_page_up(Char c) //blukas@broadcom.com
{
    USE(c);
	ConsolePageUpOrDown(TRUE);
	return (CC_REFRESH);
}
CCRETVAL
e_page_down(Char c)
{
    USE(c);
	ConsolePageUpOrDown(FALSE);
	return (CC_REFRESH);
}
