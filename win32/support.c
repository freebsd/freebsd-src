/*$Header: /p/tcsh/cvsroot/tcsh/win32/support.c,v 1.14 2008/08/31 14:09:01 amold Exp $*/
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
 * support.c
 * various routines to do exec, etc.
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincon.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include "ntport.h"
#include "sh.err.h"
#include "sh.h"
#include "nt.const.h"


DWORD gdwPlatform,gdwVersion;
unsigned short __nt_really_exec = 0,__nt_child_nohupped =0;
DWORD gdwStackSize = 524288;//0.5 MB

void path_slashify(char *pstr) {
	while(*pstr) {
#ifdef DSPMBYTE
		if (Ismbyte1(*pstr) && *(pstr + 1))
			pstr ++;
		else
#endif /* DSPMBYTE */
			if (*pstr == '\\') 
				*pstr = '/';
		pstr++;
	}
}

void do_nothing(const wchar_t *p1, const wchar_t *p2, const wchar_t*p3,
		unsigned int p4, uintptr_t p5) {
        UNREFERENCED_PARAMETER(p1);
        UNREFERENCED_PARAMETER(p2);
        UNREFERENCED_PARAMETER(p3);
        UNREFERENCED_PARAMETER(p4);
        UNREFERENCED_PARAMETER(p5);
}
void nt_init(void) {


#ifdef SECURE_CD
	{
		char temp[512];/*FIXBUF*/
		extern char gcurr_drive;
		if(!GetCurrentDirectory(512,temp))
			ExitProcess((DWORD)-1);
		gcurr_drive=temp[0];
	}
#endif SECURE_CD

	_set_invalid_parameter_handler(do_nothing);
	init_stdio();
	nt_init_signals();
	nt_term_init();
	init_hb_subst();
	setlocale(LC_ALL,"");
	init_shell_dll();
	init_plister();
	fork_init();
	init_clipboard();
	return;
}
void nt_cleanup(void){
	nt_term_cleanup();
	nt_cleanup_signals();
	cleanup_netbios();
}
void caseify_pwd(char *curwd) {
	char *sp, *dp, p,*s;
	WIN32_FIND_DATA fdata;
	HANDLE hFind;

	if (gdwPlatform !=VER_PLATFORM_WIN32_NT) 
		return;

	if (*curwd == '\\' && (!curwd[1] || curwd[1] == '\\'))
		return;
	sp = curwd +3;
	dp = curwd +3;
	do {
		p= *sp;
		if (p && p != '\\'){
			sp++;
			continue;
		}
		else {
			*sp = 0;
			hFind = FindFirstFile(curwd,&fdata);
			*sp = p;
			if (hFind != INVALID_HANDLE_VALUE) {
				FindClose(hFind);
				s = fdata.cFileName;	
				while(*s) {
					*dp++ = *s++;
				}
				dp++;
				sp = dp;
			}
			else {
				sp++;
				dp = sp;
			}
		}
		sp++;
	}while(p != 0);

}
static char defcwd[MAX_PATH];
char * forward_slash_get_cwd(char * path, size_t maxlen) {

	char *ptemp;
	Char *vp;
	int rc ;

	if ((path == NULL) || (maxlen == 0)) {
		path = &defcwd[0];
		maxlen = MAX_PATH;
	}

	rc = GetCurrentDirectory((DWORD)maxlen,path);
	if (rc > maxlen) {
		errno = ERANGE;
		return NULL;
	}
	vp = varval(STRNTcaseifypwd);
	if (vp != STRNULL) {
		caseify_pwd(path);
	}
	ptemp=path;

	path_slashify(ptemp);

	return path;
}
void getmachine (void) {

	char temp[256];
	char *vendor, *ostype;
	OSVERSIONINFO osver;
	SYSTEM_INFO sysinfo;


	memset(&osver,0,sizeof(osver));
	memset(&sysinfo,0,sizeof(sysinfo));
	vendor = "Microsoft";

	tsetenv(STRVENDOR,str2short(vendor));

	osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (!GetVersionEx(&osver)) {
		MessageBox(NULL,"GetVersionEx failed in getmachine",
				"tcsh",MB_ICONHAND);
		ExitProcess(0xFF);
	}
	GetSystemInfo(&sysinfo);

	if(osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		char *ostr;
		ostype = "WindowsNT";
		ostr = "Windows NT";

		(void)StringCbPrintf(temp,sizeof(temp),"%s %d.%d Build %d (%s)",
							 ostr,
							 osver.dwMajorVersion,osver.dwMinorVersion,
							 osver.dwBuildNumber,
							 osver.szCSDVersion[0]?osver.szCSDVersion:"");
		tsetenv(STRHOSTTYPE,str2short(temp));
	}
	else if (osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		ostype = "Windows9x";
		(void)StringCbPrintf(temp,sizeof(temp),
							 "Win9x %d.%d:%d",osver.dwMajorVersion,osver.dwMinorVersion,
							 LOWORD(osver.dwBuildNumber));
		tsetenv(STRHOSTTYPE,str2short(temp));
	}
	else {
		ostype = "WindowsWhoKnows";
		MessageBox(NULL,"Unknown platform","tcsh",MB_ICONHAND);
	}
	tsetenv(STROSTYPE,str2short(ostype));
	switch (sysinfo.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_INTEL:
			if ( ( sysinfo.wProcessorLevel < 3) || 
					( sysinfo.wProcessorLevel > 9)  )
				sysinfo.wProcessorLevel = 3;

			(void)StringCbPrintf(temp,sizeof(temp),
								 "i%d86",sysinfo.wProcessorLevel);
			break;
		case PROCESSOR_ARCHITECTURE_ALPHA:
			(void)StringCbPrintf(temp,sizeof(temp),"Alpha");
			break;
		case PROCESSOR_ARCHITECTURE_MIPS:
			(void)StringCbPrintf(temp,sizeof(temp),"Mips");
			break;
		case PROCESSOR_ARCHITECTURE_PPC:
			(void)StringCbPrintf(temp,sizeof(temp),"PPC");
			break;
		case PROCESSOR_ARCHITECTURE_AMD64:
			(void)StringCbPrintf(temp,sizeof(temp),"AMD64");
			break;
		default:
			(void)StringCbPrintf(temp,sizeof(temp),"Unknown");
			break;
	}
	tsetenv(STRMACHTYPE,str2short(temp));

}
void nt_exec(char *prog, char**args) {
	nt_execve(prog,args,NULL);
}
void nt_execve(char *prog, char**args, char**envir ) {

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE htemp;
	BOOL bRet;
	DWORD type=0;
	DWORD dwCreationflags;
	unsigned int priority;
	char *argv0= NULL;
	char *cmdstr, *cmdend ;
	char *originalPtr;
	unsigned int cmdsize,cmdlen;
	char *p2;
	char **savedargs;
	int retries=0;
	int hasdot =0;
	int is_winnt ;

	UNREFERENCED_PARAMETER(envir);

	memset(&si,0,sizeof(si));
	savedargs = args;

	/* 
	 * This memory is not freed because we are exec()ed and will
	 * not be alive long.
	 */
	originalPtr = cmdstr= heap_alloc(MAX_PATH<<2);

	is_winnt = (gdwPlatform != VER_PLATFORM_WIN32_WINDOWS);


	cmdsize = MAX_PATH<<2;

	p2 = cmdstr;

	cmdlen = 0;
	cmdlen += copy_quote_and_fix_slashes(prog,cmdstr,&hasdot);

	p2 += cmdlen;

	/* If the command was not quoted ,
	   skip initial character we left for quote */
	if (*cmdstr != '"') {
		*cmdstr = 'A';
		cmdstr++; 
		cmdsize--;
	}
	*p2 = 0;
	cmdend = p2;


	if (!is_winnt){
		argv0 = NULL;
		goto win95_directly_here;
	}
	else {
		argv0 = heap_alloc(MAX_PATH); /* not freed */
		(void)StringCbPrintf(argv0,MAX_PATH,"%s",prog);
	}

retry:

	bRet=GetBinaryType(argv0,&type);
	dprintf("binary type for %s is %d\n",argv0,bRet);
	//
	// For NT, append .EXE and retry
	//
	if (is_winnt && !bRet ) {
		/* Don't append .EXE if it could be a script file */
		if (GetLastError() == ERROR_BAD_EXE_FORMAT){
			errno = ENOEXEC;
			if (!__nt_only_start_exes)
				try_shell_ex(args,1,FALSE); //can't throw on error
			return;
		}
		else if ( retries ){
			if (
					( (argv0[0] == '\\') ||(argv0[0] == '/') ) &&
					( (argv0[1] == '\\') ||(argv0[1] == '/') ) &&
					(!args[1])
			   )
				if (!__nt_only_start_exes)
					try_shell_ex(args,1,FALSE);
			errno  = ENOENT;
		}
		if (retries > 1){
			return;
		}
		// Try uppercase once and then lower case
		//
		if (!retries) {
			(void)StringCbPrintf(argv0,MAX_PATH,"%s.exe",prog);
		}
		else  {
			(void)StringCbPrintf(argv0,MAX_PATH,"%s.EXE",prog); 
			/* fix for clearcase */
		}
		retries++;
		goto retry;
	}

win95_directly_here:

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;
	htemp= (HANDLE)_get_osfhandle(0);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdInput,0,TRUE,DUPLICATE_SAME_ACCESS);
	htemp= (HANDLE)_get_osfhandle(1);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdOutput,0,TRUE,DUPLICATE_SAME_ACCESS);
	htemp= (HANDLE)_get_osfhandle(2);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdError,0,TRUE,DUPLICATE_SAME_ACCESS);



	args++; // the first arg is the command


	dprintf("nt_execve calling c_a_a_q");
	if(!concat_args_and_quote(args,&originalPtr,&cmdstr,&cmdlen,&cmdend,
				&cmdsize))
	{
		dprintf("concat_args_and_quote failed\n");
		heap_free(originalPtr);
		errno = ENOMEM;
		goto fail_return;
	}

	*cmdend = 0;

	dwCreationflags = GetPriorityClass(GetCurrentProcess());
	if (__nt_child_nohupped) {
		dwCreationflags |= DETACHED_PROCESS;
	}
	priority = GetThreadPriority(GetCurrentThread());

	(void)fix_path_for_child();

	if (is_winnt)
		dwCreationflags |= CREATE_SUSPENDED;


re_cp:
	dprintf("argv0 %s cmdstr %s\n",argv0,cmdstr);
	bRet = CreateProcessA(argv0, cmdstr,
			NULL, NULL,
			TRUE, // need this for redirecting std handles
			dwCreationflags,
			NULL, NULL,
			&si,
			&pi);
	if (!bRet){
		if (GetLastError() == ERROR_BAD_EXE_FORMAT) {
			if (!__nt_only_start_exes)
				try_shell_ex(savedargs,1,FALSE);
			errno  = ENOEXEC;
		}
		else if (GetLastError() == ERROR_INVALID_PARAMETER) {
			/* can't get invalid parameter, so this must be
			 *  the case when we exceed the command length limit.
			 */
			errno = ENAMETOOLONG;
		}
		else {
			errno  = ENOENT;
		}
		if (!is_winnt && !hasdot) { //append '.' to the end if needed
			(void)StringCbCat(cmdstr,cmdsize,".");
			hasdot=1;
			goto re_cp;
		}
	}
	else{
		int gui_app ;
		char guivar[50];

		if (GetEnvironmentVariable("TCSH_NOASYNCGUI",guivar,50))
			gui_app=0;
		else {
			if (is_winnt || hasdot)
				gui_app= is_gui(argv0);
			else
				gui_app = is_9x_gui(prog);
		}

		if (is_winnt && !SetThreadPriority(pi.hThread,priority) ) {
			priority =GetLastError();
		}
		if (is_winnt)
			ResumeThread(pi.hThread);
		errno= 0;

		if (__nt_really_exec||__nt_child_nohupped || gui_app){
			ExitProcess(0);
		}
		else {
			DWORD exitcode=0;
			WaitForSingleObject(pi.hProcess,INFINITE);
			(void)GetExitCodeProcess(pi.hProcess,&exitcode);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			/*
			 * If output was redirected to /dev/clipboard,
			 * we need to close the pipe handles
			 */
			if (is_dev_clipboard_active) {
				CloseHandle((HANDLE)_get_osfhandle(0));
				CloseHandle((HANDLE)_get_osfhandle(1));
				CloseHandle((HANDLE)_get_osfhandle(2));
				CloseHandle(si.hStdInput);
				CloseHandle(si.hStdOutput);
				CloseHandle(si.hStdError);
				WaitForSingleObject(ghdevclipthread,60*1000);
			}
			ExitProcess(exitcode);
		}
	}
fail_return:
	CloseHandle(si.hStdInput);
	CloseHandle(si.hStdOutput);
	CloseHandle(si.hStdError);
	return;
}
/* This function from  Mark Tucker (mtucker@fiji.sidefx.com) */
int quoteProtect(char *dest, char *src,unsigned long destsize) {
	char	*prev, *curr;
	for (curr = src; *curr; curr++) {

		// Protect " from MS-DOS expansion
		if (*curr == '"') {
			// Now, protect each preceeding backslash
			for (prev = curr-1; prev >= src && *prev == '\\'; prev--) {
				*dest++ = '\\';
				destsize--;
				if(destsize == 0)
					return ERROR_BUFFER_OVERFLOW;
			}

			*dest++ = '\\';
			destsize--;
			if(destsize == 0)
				return ERROR_BUFFER_OVERFLOW;
		}
		*dest++ = *curr;
		destsize--;
		if(destsize == 0)
			return ERROR_BUFFER_OVERFLOW;

	}
	*dest = 0;

	return NO_ERROR;
}


int gethostname(char *buf, int len) {
	GetComputerName(buf,(DWORD*)&len);
	return 0;
}
int nt_chdir (char *path) {
	char *tmp = path;
	if (gdwPlatform !=VER_PLATFORM_WIN32_NT) {
		while(*tmp) {
			if (*tmp == '/') *tmp = '\\';
			tmp++;
		}
	}
	return _chdir(path);
}
void WINAPI uhef( EXCEPTION_POINTERS *lpep) {
	ExitProcess(lpep->ExceptionRecord->ExceptionCode);
}
extern BOOL CreateWow64Events(DWORD,HANDLE*,HANDLE*,BOOL);
// load kernel32 and look for iswow64. if not found, assume FALSE
BOOL bIsWow64Process = FALSE;
void init_wow64(void) {
	HMODULE hlib;
	//BOOL (WINAPI *pfnIsWow64)(HANDLE,BOOL*);
	FARPROC pfnIsWow64;

	bIsWow64Process = FALSE;

	hlib = LoadLibrary("kernel32.dll");
	if (!hlib) {
		return;
	}
	pfnIsWow64 = GetProcAddress(hlib,"IsWow64Process");
	if (!pfnIsWow64) {
		FreeLibrary(hlib);
		return;
	}
	if (!pfnIsWow64(GetCurrentProcess(),&bIsWow64Process) )
		bIsWow64Process = FALSE;

	FreeLibrary(hlib);
	return;

}

extern void mainCRTStartup(void *);

/* 
 * heap_init() MUST NOT be moved outside the entry point. Sometimes child
 * processes may load random DLLs not loaded by the parent and 
 * use the heap address reserved for fmalloc() in the parent. This
 * causes havoc as no dynamic memory can then be inherited.
 *  
 */
extern void heap_init(void);

#include <forkdata.h>
void silly_entry(void *peb) {
	char * path1=NULL;
	int rc;
	char temp[MAX_PATH+5];
	char buf[MAX_PATH];
	char ptr1[MAX_PATH];
	char ptr2[MAX_PATH];
	char ptr3[MAX_PATH];
	OSVERSIONINFO osver;

	osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (!GetVersionEx(&osver)) {
		MessageBox(NULL,"GetVersionEx failed","tcsh",MB_ICONHAND);
		ExitProcess(0xFF);
	}
	gdwVersion = osver.dwMajorVersion;

	if(gdwVersion < 6) // no wow64 hackery for vista.
	{
		init_wow64();
	}

#ifdef _M_IX86
	// look at the explanation in fork.c for why we do these steps.
	if (bIsWow64Process) {
		HANDLE h64Parent,h64Child;
		char *stk, *end;
		DWORD mb = (1<<20);

		// if we found the events, then we're the product of a fork()
		if (CreateWow64Events(GetCurrentProcessId(),
					&h64Parent,&h64Child,TRUE)) {

			if (!h64Parent || !h64Child)
				return;

			// tell parent we're rolling
			SetEvent(h64Child);

			if(WaitForSingleObject(h64Parent,FORK_TIMEOUT) != WAIT_OBJECT_0) {
				return;
			}

			// if __forked is 0, we shouldn't have found the events
			if (!__forked) 
				return;
		}

		// now create the stack 

		if (!__forked) {
			stk = VirtualAlloc(NULL,mb+65536,MEM_COMMIT,PAGE_READWRITE);
			if (!stk) {
				dprintf("virtual alloc in parent failed %d\n",GetLastError());
				return;
			}
			end = stk + mb + 65536;
			end -= sizeof(char*);

			__fork_stack_begin = end;

			__asm {mov esp,end };

			set_stackbase(end);
			heap_init();
		}
		else { // child process
			stk = (char*)__fork_stack_begin + sizeof(char*)- mb - 65536;

			dprintf("begin is 0x%08x\n",stk);
			end = VirtualAlloc(stk, mb+65536 , MEM_RESERVE , PAGE_READWRITE);
			if (!end) {
				rc = GetLastError();
				dprintf("virtual alloc 1 in child failed %d\n",rc);
				return;
			}
			stk = VirtualAlloc(end, mb+65536 , MEM_COMMIT , PAGE_READWRITE);
			if (!stk) {
				rc = GetLastError();
				dprintf("virtual alloc 2 in child failed %d\n",rc);
				return;
			}
			end = stk + mb + 65536;
			__asm {mov esp, end};
			set_stackbase(end);

			SetEvent(h64Child);

			CloseHandle(h64Parent);
			CloseHandle(h64Child);
		}
	}
#endif _M_IX86


	SetFileApisToOEM();

	if (!bIsWow64Process)
		heap_init();




	/* If home is set, we only need to change '\' to '/' */
	rc = GetEnvironmentVariable("HOME",buf,MAX_PATH);
	if (rc && (rc < MAX_PATH)){
		path_slashify(buf);
		(void)SetEnvironmentVariable("HOME",buf);
		goto skippy;
	}

	memset(ptr1,0,MAX_PATH);
	memset(ptr2,0,MAX_PATH);
	memset(ptr3,0,MAX_PATH);

	if(osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		GetEnvironmentVariable("USERPROFILE",ptr1,MAX_PATH);
		GetEnvironmentVariable("HOMEDRIVE",ptr2,MAX_PATH);
		GetEnvironmentVariable("HOMEPATH",ptr3,MAX_PATH);

		ptr1[MAX_PATH -1] = ptr2[MAX_PATH-1] = ptr3[MAX_PATH-1]= 0;

#pragma warning(disable:4995)
		if (!ptr1[0] || osver.dwMajorVersion <4) {
			wsprintfA(temp, "%s%s",ptr2[0]?ptr2:"C:",ptr3[0]?ptr3:"\\");
		}
		else if (osver.dwMajorVersion >= 4) {
			wsprintfA(temp, "%s",ptr1);
		}
#pragma warning(default:4995)
	}
	else if (osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {

		rc = GetWindowsDirectory(ptr1,MAX_PATH);
		if (rc > MAX_PATH) {
			MessageBox(NULL,"This should never happen","tcsh",MB_ICONHAND);
			ExitProcess(0xFF);
		}
		(void)StringCbPrintf(temp,sizeof(temp),"%s",ptr1);
	}
	else {
		MessageBox(NULL,"Unknown platform","tcsh",MB_ICONHAND);
	}
	path_slashify(temp);
	SetEnvironmentVariable("HOME",temp);

skippy:
	gdwPlatform = osver.dwPlatformId;


	rc = GetEnvironmentVariable("Path",path1,0);
	if ( rc !=0) {

		path1 =heap_alloc(rc);

		GetEnvironmentVariable("Path",path1,rc);
		SetEnvironmentVariable("Path",NULL);
		/*SetEnvironmentVariable("PATH",NULL);*/
		SetEnvironmentVariable("PATH",path1);

		heap_free(path1);
	}
	mainCRTStartup(peb);
}

/* 
 * Copy source into target, quote if it has space, also converting '/' to '\'. 
 *
 * hasdot is set to 1 if source ends in a file extension
 * return value is the  length of the string copied.
 */
int copy_quote_and_fix_slashes(char *source,char *target, int *hasdot ) {

	int len ;
	int hasspace;
	char *save;
	char *ptr;

	save = target; /* leave space for quote */
	len = 1;

	target++;

	hasspace = 0;
	while(*source) {
		if (*source == '/')
			*source = '\\';
		else if (*source == ' ')
			hasspace = 1;

		*target++ = *source;

		source++;
		len++;
	}
	ptr  = target;//source;
	while( (ptr > save ) && (*ptr != '\\')) {
		if (*ptr == '.')
			*hasdot = 1;
		ptr--;
	}

	if (hasspace) {
		*save = '"';
		*target = '"';
		len++;
	}
	return len;
}
/*
 * This routine is a replacement for the old, horrible strcat() loop
 * that was used to turn the argv[] array into a string for CreateProcess().
 * It's about a zillion times faster. 
 * -amol 2/4/99
 */
char *concat_args_and_quote(char **args, char **poriginalPtr,char **cstr, 
		unsigned int *clen, char **cend, unsigned int *cmdsize) {

	unsigned int argcount, arglen, cmdlen;
	char *tempptr, *cmdend ,*cmdstr;
	short quotespace = 0;
	short quotequote = 0;
	short noquoteprotect = 0;
	char *tempquotedbuf;
	unsigned long tqlen = 256;
	int rc;

	dprintf("entering concat_args_and_quote\n");
	tempquotedbuf = heap_alloc(tqlen);

	noquoteprotect = (short)(varval(STRNTnoquoteprotect) != STRNULL);
	/* 
	   quotespace hack needed since execv() would have separated args, but
	   createproces doesnt
	   -amol 9/14/96
	 */
	cmdend= *cend;
	cmdstr = *cstr;
	cmdlen = *clen;

	argcount = 0;
	while (*args) {

		*cmdend++ = ' ';
		cmdlen++;

		tempptr = *args;

		arglen = 0;
		argcount++;

		//dprintf("args is %s\n",*args);
		if (!*tempptr) {
			*cmdend++ = '"';
			*cmdend++ = '"';
		}
		while(*tempptr) {
			if (*tempptr == ' ' || *tempptr == '\t') 
				quotespace = 1;
			else if (*tempptr == '"')
				quotequote = 1;
			tempptr++;
			arglen++;
		}
		if (arglen + cmdlen +4 > *cmdsize) { // +4 is if we have to quote


			tempptr = heap_realloc(*poriginalPtr,*cmdsize<<1);

			if(!tempptr)
				return NULL;

			// If it's not the same heap block, re-adjust the pointers.
			if (tempptr != *poriginalPtr) {
				cmdstr = tempptr + (cmdstr - *poriginalPtr);
				cmdend = tempptr + (cmdend- *poriginalPtr);
				*poriginalPtr = tempptr;
			}

			*cmdsize <<=1;
		}
		if (quotespace)
			*cmdend++ = '"';

		if ((noquoteprotect == 0) && quotequote){
			tempquotedbuf[0]=0;

			tempptr = &tempquotedbuf[0];

			rc = quoteProtect(tempquotedbuf,*args,tqlen);

			while(rc == ERROR_BUFFER_OVERFLOW) {
				char *tmp = tempquotedbuf;
				tempquotedbuf = heap_realloc(tempquotedbuf,tqlen <<1);
				if(!tempquotedbuf) {
					heap_free(tmp);
					return NULL;
				}
				tqlen <<= 1;
				tempptr = &tempquotedbuf[0];
				rc = quoteProtect(tempquotedbuf,*args,tqlen);
			}
			while (*tempptr) {
				*cmdend = *tempptr;
				cmdend++;
				tempptr++;
			}
			cmdlen +=2;
		}
		else {
			tempptr = *args;
			while(*tempptr) {
				*cmdend = *tempptr;
				cmdend++;
				tempptr++;
			}
		}

		if (quotespace) {
			*cmdend++ = '"';
			cmdlen +=2;
		}
		cmdlen += arglen;

		args++;
	}
	*clen = cmdlen;
	*cend = cmdend;
	*cstr = cmdstr;

	heap_free(tempquotedbuf);


	return cmdstr;
}
char *fix_path_for_child(void) {

	char *ptr;
	Char *vp;
	char *pathstr;
	char *oldpath;
	long len;

	vp = varval(STRNTlamepathfix);

	if (vp != STRNULL) {

		len = GetEnvironmentVariable("PATH",NULL,0);

		oldpath = heap_alloc(len+1);
		pathstr = heap_alloc(len+1);

		len = GetEnvironmentVariable("PATH",oldpath,len+1);
		memcpy(pathstr,oldpath,len);

		ptr = pathstr;
		while(*ptr) {
			if (*ptr == '/')
				*ptr = '\\';
			ptr++;
		}
		SetEnvironmentVariable("PATH",pathstr);
		heap_free(pathstr);

		return oldpath; //freed in restore_path;
	}
	else
		return NULL;

}
void restore_path(char *oldpath) {
	if (oldpath) {
		SetEnvironmentVariable("PATH",oldpath);
		heap_free(oldpath);
	}
}
