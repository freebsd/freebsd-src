/*$Header: /p/tcsh/cvsroot/tcsh/win32/ps.c,v 1.9 2006/03/14 01:22:58 mitr Exp $*/
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
 * ps.c : ps,shutdown builtins.
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winperf.h>
#include <tlhelp32.h>
#include <sh.h>
#include <errno.h>


#define REGKEY_PERF         "software\\microsoft\\windows nt\\currentversion\\perflib"
#define REGSUBKEY_COUNTERS  "Counters"
#define PROCESS_COUNTER     "process"
#define PROCESSID_COUNTER   "id process"

typedef struct _pslist {
	DWORD pid;
	HANDLE hwnd;
	char exename[MAX_PATH];
	char title[80];
}pslist;


typedef BOOL (WINAPI *walker)(HANDLE,LPPROCESSENTRY32);
typedef HANDLE (WINAPI *create_snapshot)(DWORD,DWORD);
static walker proc32First;
static walker proc32Next;
static create_snapshot createSnapshot;

typedef BOOL (WINAPI *enumproc)(DWORD *,DWORD,DWORD *);
typedef BOOL (WINAPI *enummod)(HANDLE,HMODULE*,DWORD,DWORD*);
typedef DWORD(WINAPI *getfilename_ex)(HANDLE,HANDLE , char*,DWORD);
typedef DWORD (WINAPI *getbasename)(HANDLE,HMODULE,char*,DWORD);
static enumproc enum_processes;
static enummod enum_process_modules;
static getfilename_ex getfilenameex;
static getbasename GetModuleBaseNameA;

typedef DWORD (*plist_proc)(void);

DWORD Win95Lister(void);
DWORD NTLister(void);

plist_proc ProcessListFunc;
pslist *processlist;
static unsigned long numprocs, g_dowindows;

static HMODULE hlib;

extern DWORD gdwPlatform;
extern void make_err_str(int,char *,int);

BOOL CALLBACK enum_wincb(HWND hwnd,LPARAM nump) {

	unsigned int i;
	DWORD pid = 0;

	if (!GetWindowThreadProcessId(hwnd,&pid))
		return TRUE;

	for (i =0;i < nump;i++) {
		if (processlist[i].pid == pid){
			processlist[i].hwnd = hwnd;
			if (processlist[i].title[0] !=0)
				break;;
			GetWindowText(hwnd,processlist[i].title,
					sizeof(processlist[i].title));
			break;
		}
	}
	return TRUE;
}
static HWND ghwndtokillbywm_close;
BOOL CALLBACK enum_wincb2(HWND hwnd,LPARAM pidtokill) {
	DWORD pid = 0;

	if (!GetWindowThreadProcessId(hwnd,&pid))
		return TRUE;
	if (pid == (DWORD)pidtokill){
		ghwndtokillbywm_close = hwnd;
		PostMessage( hwnd, WM_CLOSE, 0, 0 );
		return TRUE;
	}

	return TRUE;
}
int kill_by_wm_close(int pid)  {
	EnumWindows(enum_wincb2,(LPARAM)pid);
	if (!ghwndtokillbywm_close)
		return -1;
	ghwndtokillbywm_close = NULL;
	return 0;
}
DWORD Win95Lister(void) {

	HANDLE hsnap;
	PROCESSENTRY32 pe;
	unsigned long nump =0;


	hsnap = createSnapshot(TH32CS_SNAPPROCESS,0);
	if (hsnap == INVALID_HANDLE_VALUE)
		return 0;

	//	if (processlist)
	//		p_free(processlist);

	pe.dwSize = sizeof(PROCESSENTRY32);
	if (proc32First(hsnap,&pe) ) {
		processlist = heap_alloc(100*sizeof(pslist));
		if (!processlist)
			goto done;

		do {
			StringCbCopy(processlist[nump].exename,
					sizeof(processlist[nump].exename),pe.szExeFile);

			processlist[nump].title[0] = 0;
			processlist[nump].pid = pe.th32ProcessID;
			nump++;
		}while(proc32Next(hsnap,&pe));
	}
done:
	CloseHandle(hsnap);

	if (g_dowindows) {
		EnumWindows(enum_wincb,(LPARAM)nump);
	}
	return nump;
}

DWORD NTLister(void) {

	DWORD procs[200],dummy,ignore;
	HANDLE hproc;
	HMODULE hmod;
	unsigned int i;


	//	if (processlist)
	//		p_free(processlist);

	if (!enum_processes(procs,sizeof(procs),&dummy) ) {
		return 0;
	}

	dummy = dummy/sizeof(DWORD); // number of entries filled

	processlist = heap_alloc(dummy*sizeof(pslist));
	if (!processlist){
		return 0;
	}

	for(i=0 ; i< dummy;i++) {
		processlist[i].pid = procs[i];
		processlist[i].title[0] = 0;
		hproc = OpenProcess(PROCESS_QUERY_INFORMATION |PROCESS_VM_READ,
				FALSE,procs[i]);
		if (hproc) {
			if (enum_process_modules(hproc,&hmod,sizeof(hmod),&ignore)) {
				GetModuleBaseNameA(hproc,hmod, processlist[i].exename,MAX_PATH);
			}
			else
				StringCbCopy(processlist[i].exename,
						sizeof(processlist[i].exename),"(unknown)");
			CloseHandle(hproc);
		}
		else
			StringCbCopy(processlist[i].exename,
					sizeof(processlist[i].exename),"(unknown)");

	}
	if (g_dowindows) {
		EnumWindows(enum_wincb,(LPARAM)dummy);
	}
	return dummy;
}

void init_plister(void) {


	hlib = LoadLibrary("kernel32.dll");
	if (!hlib)
		return ;


	ProcessListFunc = Win95Lister;

	proc32First = (walker)GetProcAddress(hlib,"Process32First");
	proc32Next = (walker)GetProcAddress(hlib,"Process32Next");
	createSnapshot= (create_snapshot)GetProcAddress(hlib,
			"CreateToolhelp32Snapshot");

	FreeLibrary(hlib);
	if (!proc32First || !proc32Next || !createSnapshot) {
		ProcessListFunc = NULL;
	}
}
void dops(Char ** vc, struct command *c) {

	DWORD nump;
	unsigned int i,k;
	char **v;

	UNREFERENCED_PARAMETER(c);

	if (!ProcessListFunc)
		return;
	vc = glob_all_or_error(vc);
	v = short2blk(vc);
	blkfree(vc);
	for (k = 0; v[k] != NULL ; k++){
		if ( v[k][0] == '-' ) {
			if( (v[k][1] == 'W') || (v[k][1] == 'w'))
				g_dowindows = 1;
		}
	}
	blkfree((Char**)v);
	nump = ProcessListFunc();

	for(i=0; i< nump; i++) {
		if (gdwPlatform == VER_PLATFORM_WIN32_NT) 
			xprintf("%6u  %-20s %-30s\n",processlist[i].pid,
					processlist[i].exename, 
					g_dowindows?processlist[i].title:"");
		else
			xprintf("0x%08x  %-20s %-30s\n",processlist[i].pid,
					processlist[i].exename,
					g_dowindows?processlist[i].title:"");
	}
	g_dowindows =0;

	if (processlist)
		heap_free(processlist);

}
static char shutdown_usage[]= {"shutdown -[r|l][f] now\n-r reboots, -l logs\
	off the current user\n-f forces termination of running applications.\n\
		The default action is to shutdown without a reboot.\n\"now\" must be \
		specified to actually shutdown or reboot\n"};

void doshutdown(Char **vc, struct command *c) {

	unsigned int flags = 0;
	unsigned char reboot,shutdown,logoff,shutdown_ok;
	char **v;
	char *ptr;
	char errbuf[128];
	int k;
	HANDLE hToken;
	TOKEN_PRIVILEGES tp,tpPrevious;
	LUID luid;
	DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);

	UNREFERENCED_PARAMETER(c);

	if (gdwPlatform != VER_PLATFORM_WIN32_NT) {
		stderror(ERR_SYSTEM,"shutdown","Sorry,not supported on win95");
	}

	shutdown_ok = reboot = shutdown = logoff = 0;
	vc = glob_all_or_error(vc);
	v = short2blk(vc);
	blkfree(vc);
	cleanup_push((Char **)v, blk_cleanup);
	for (k = 0; v[k] != NULL ; k++){
		if ( v[k][0] == '-' ) {
			ptr = v[k];
			ptr++;
			while( ptr && *ptr) {
				if (*ptr == 'f')
					flags |= EWX_FORCE;
				if (*ptr == 'r')
					reboot =1;
				else if (*ptr == 'l')
					logoff =1;
				else
					stderror(ERR_SYSTEM,"Usage",shutdown_usage);
				ptr++;
			}
		}
		else if (!_stricmp(v[k],"now")) {
			shutdown_ok = 1;
		}
	}
	if (k == 0)
		stderror(ERR_SYSTEM,"Usage",shutdown_usage);
	if (!reboot && !logoff){
		flags |= EWX_SHUTDOWN;
		shutdown = 1;
	}
	if (reboot && logoff )
		stderror(ERR_SYSTEM,"Usage",shutdown_usage);
	if (reboot)
		flags |= EWX_REBOOT;
	if (logoff)
		flags |= EWX_LOGOFF;

	if ((reboot || shutdown) && (!shutdown_ok) )
		stderror(ERR_SYSTEM,"shutdown","Specify \"now\" to really shutdown");


	if (!OpenProcessToken(GetCurrentProcess(),
				TOKEN_ADJUST_PRIVILEGES| TOKEN_QUERY,
				&hToken) ){
		make_err_str(GetLastError(),errbuf,128);
		stderror(ERR_SYSTEM,"shutdown failed",errbuf);
	}


	if (!LookupPrivilegeValue(NULL,SE_SHUTDOWN_NAME,&luid)) {
		make_err_str(GetLastError(),errbuf,128);
		stderror(ERR_SYSTEM,"shutdown failed",errbuf);
	}
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	if (!AdjustTokenPrivileges(hToken,FALSE,&tp,sizeof(tp),&tpPrevious,
				&cbPrevious)){
		make_err_str(GetLastError(),errbuf,128);
		stderror(ERR_SYSTEM,"shutdown failed",errbuf);
	}
	tpPrevious.PrivilegeCount = 1;
	tpPrevious.Privileges[0].Luid = luid;
	tpPrevious.Privileges[0].Attributes |= SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken,FALSE,&tpPrevious,cbPrevious,NULL,
				NULL)){
		make_err_str(GetLastError(),errbuf,128);
		stderror(ERR_SYSTEM,"shutdown failed",errbuf);
	}
	if  (  !ExitWindowsEx(flags,0) ) {
		make_err_str(GetLastError(),errbuf,128);
		stderror(ERR_SYSTEM,"shutdown failed",errbuf);
	}
	cleanup_until((Char **)v);
}
