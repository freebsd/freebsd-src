/*$Header: /p/tcsh/cvsroot/tcsh/win32/nt.char.c,v 1.10 2008/10/11 12:47:39 christos Exp $*/
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
 * nt.char.c : Does NLS-like stuff
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include "sh.h"


static HMODULE hlangdll;


extern DWORD gdwPlatform;


unsigned char oem_it(unsigned char ch) {
	unsigned char ch1[2],ch2[2];

	ch1[0] = ch;
	ch1[1] = 0;

	OemToChar((char*)ch1,(char*)ch2);

	return ch2[0];
}
void nls_dll_unload(void) {
	FreeLibrary(hlangdll);
	hlangdll=NULL;
}
char * nt_cgets(int set, int msgnum, char *def) {

	int rc;
	int msg;
	static char oembuf[256];/*FIXBUF*/
	WCHAR buffer[256];/*FIXBUF*/



	if (!hlangdll)
		return def;

	msg = set * 10000 + msgnum;

	if (gdwPlatform == VER_PLATFORM_WIN32_WINDOWS) {
		rc = LoadString(hlangdll,msg,oembuf,sizeof(oembuf));

		if(!rc)
			return def;
		return oembuf;
	}
	rc = LoadStringW(hlangdll,msg,buffer,ARRAYSIZE(buffer));

	if(!rc)
		return def;

	WideCharToMultiByte(CP_OEMCP,
			0,
			buffer,
			-1,
			oembuf,//winbuf,
			256,
			NULL,NULL);

	return oembuf;
}
#if defined(DSPMBYTE)
void nt_autoset_dspmbyte(void) {
	switch (GetConsoleCP()) {
		case 932: /* Japan */
			setcopy(CHECK_MBYTEVAR, STRsjis, VAR_READWRITE);
			update_dspmbyte_vars();
			break;
	}
}

// _mbmap must be copied to the child during fork()
unsigned short _mbmap[256] = { 0 };
#endif

#undef free
void nls_dll_init(void) {

	char *ptr;
	size_t size = 0;


	if (_dupenv_s(&ptr,&size,"TCSHLANG") == 0){

		if (hlangdll)
			FreeLibrary(hlangdll);
		hlangdll = LoadLibrary(ptr);

		free(ptr);
	}
}
