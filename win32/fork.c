/*$Header: /p/tcsh/cvsroot/tcsh/win32/fork.c,v 1.11 2008/08/31 14:09:01 amold Exp $*/
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
 * The fork() here is based on the ideas used by cygwin
 * -amol
 *
 */

/*
 * _M_ALPHA changes by Mark Tucker
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <setjmp.h>
#include <ntport.h>
#include "forkdata.h"
#include "sh.h"

#pragma intrinsic("memcpy", "memset","memcmp")
#pragma warning(push,3) // forget about W4 here

typedef unsigned long u_long;
typedef void *ptr_t;
typedef unsigned char U_char;	
typedef unsigned int U_int;
typedef unsigned short U_short;
typedef unsigned long U_long;


static void stack_probe(void *ptr) ;
/*static void heap_init(void);*/
BOOL CreateWow64Events(DWORD , HANDLE *, HANDLE *, BOOL);

//
// This is exported from the user program.
// It must return 0 for no error !!!!
extern int fork_copy_user_mem(HANDLE );

/* 
 * Apparently , visual c++ on the alpha does not place the
 * fork data contiguously. To work around that, Mark created
 * this structure (see forkdata.h)
 * -amol
 */
ForkData gForkData = {0,0,0,0,0,{0},0,0,0};


#ifdef _M_IX86

u_long _old_exr = 0; // Saved exception registration for longjmp

#endif // _M_ALPHA
/*
 * This hack is an attempt at getting to the exception registration
 * in an architecture-independent way. It's critical for longjmp in a
 * code using __try/__except blocks. Microsoft Visual C++ does a global
 * unwind during a longjmp, and that can cause havoc if the exception 
 * registration stored in longjmp is lower(address wise, indicating a jump
 * from below of the stack upward.) in the stack than the current
 * registration (returned by NtCurrentTeb).
 *
 * This works with VC++, because that's all I have. With other compilers, 
 * there might be minimal changes required, depending on where the 
 * exception registration record is stored in the longjmp structure.
 *
 * -amol 2/6/97
 */

NT_TIB * (* myNtCurrentTeb)(void);

#define GETEXCEPTIONREGIST() (((NT_TIB*)get_teb())->ExceptionList)
#define GETSTACKBASE()		 (((NT_TIB*)get_teb())->StackBase)



static NT_TIB *the_tib;

#if !defined(_M_IA64) && !defined(_M_AMD64)
void *get_teb(void) {


	if (the_tib)
		return the_tib;

	myNtCurrentTeb = (void*)GetProcAddress(LoadLibrary("ntdll.dll"),
							"NtCurrentTeb");
	if (!myNtCurrentTeb)
		return NULL;
	the_tib = myNtCurrentTeb();

	if (the_tib == NULL)
		abort();
	return the_tib;
}
#else
#define get_teb NtCurrentTeb
#endif _M_IA64

void set_stackbase(void*ptr){
	GETSTACKBASE() = ptr;
}
/* 
 * This must be called by the application as the first thing it does.
 * -amol 2/6/97
 *
 * Well, maybe not the FIRST..
 * -amol 11/10/97
 */

extern BOOL bIsWow64Process;

int fork_init(void) {


	//heap_init(); Now called as the very first thing in silly_entry().

	if (__forked) {


		// stack_probe probes out a decent-sized stack for the child,
		// since initially it has a very small stack (1 page).
		//

		/* not needed since default commit is set to 0.5MB in 
		 * makefile.win32
		 *
		 * stack_probe((char *)__fork_stack_end - 64);
		 */

		//
		// Save the old Exception registration record and jump
		// off the cliff.
		//
#ifdef  _M_IX86
		_old_exr = __fork_context[6];
		__fork_context[6] =(int)GETEXCEPTIONREGIST();//tmp;
#endif  _M_ALPHA
		//
		// Whee !
		longjmp(__fork_context,1);
	}

	return 0;
}
int fork(void) {

	size_t rc;
	size_t stacksize;
	char modname[512];/*FIXBUF*/
	HANDLE  hProc,hThread, hArray[2];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	DWORD dwCreationflags;
	unsigned int priority;
	HANDLE h64Parent,h64Child;

#ifndef _M_ALPHA
	unsigned long fork_stack_end;
#endif _M_ALPHA

	__fork_stack_begin =GETSTACKBASE();

#ifndef _M_ALPHA
	__fork_stack_end = &fork_stack_end;
#else
	__fork_stack_end = (unsigned long *)__asm("mov $sp, $0");
#endif /*_M_ALPHA*/

	h64Parent = h64Child = NULL;
	//
	// Create two inheritable events
	//
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor =0;
	sa.bInheritHandle = TRUE;
	if (!__hforkchild)
		__hforkchild = CreateEvent(&sa,TRUE,FALSE,NULL);
	if (!__hforkparent)
		__hforkparent = CreateEvent(&sa,TRUE,FALSE,NULL);

	rc = setjmp(__fork_context);

	if (rc) { // child
#ifdef  _M_IX86
		//
		// Restore old registration
		// -amol 2/2/97
		GETEXCEPTIONREGIST() = (struct _EXCEPTION_REGISTRATION_RECORD*)_old_exr;
#endif // _M_ALPHA
		SetEvent(__hforkchild);

		dprintf("Child ready to rumble\n");
		if(WaitForSingleObject(__hforkparent,FORK_TIMEOUT) != WAIT_OBJECT_0)
			ExitProcess(0xFFFF);

		CloseHandle(__hforkchild);
		CloseHandle(__hforkparent);
		__hforkchild = __hforkparent=0;

		//__asm { int 3};
		restore_fds();

		STR_environ = blk2short(environ);
		environ = short2blk(STR_environ);	/* So that we can free it */

		return 0;
	}
	copy_fds();
	memset(&si,0,sizeof(si));
	si.cb= sizeof(si);

	/*
	 * This f!@#!@% function returns the old value even if the std handles
	 * have been closed.
	 * Skip this step, since we know tcsh will do the right thing later.
	 * 
	 si.hStdInput= GetStdHandle(STD_INPUT_HANDLE);
	 si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	 si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	 */

	if (!GetModuleFileName(GetModuleHandle(NULL),modname,512) ) {
		rc = GetLastError();
		return -1;
	}
	dwCreationflags = GetPriorityClass(GetCurrentProcess());
	priority = GetThreadPriority(GetCurrentThread());
	rc = CreateProcess(NULL,
			modname,
			NULL,
			NULL,
			TRUE,
			CREATE_SUSPENDED | dwCreationflags,
			NULL,
			NULL,
			&si,
			&pi);
	if (!rc)  {
		rc = GetLastError();
		return -1;
	}

	ResetEvent(__hforkchild);
	ResetEvent(__hforkparent);

	hProc = pi.hProcess;
	hThread = pi.hThread;


	__forked=1;
	/*
	 * Usage of events in the wow64 case:
	 *
	 * h64Parent : initially non-signalled
	 * h64Child  : initially non-signalled
	 *
	 *    1. Create the events, resume the child thread.
	 *    2. Child opens h64Parent to see if it is a child process in wow64
	 *    3. Child opens and sets h64Child to tell parent it's running. (This
	 *       step is needed because we can't copy to a process created in the
	 *       suspended state on wow64.)
	 *    4. Copy gForkData and then set h64Parent. This tells the child
	 *       that the parameters in the structure are trustworthy.
	 *    5. Wait for h64Child so that we know the child has created the stack
	 *       in dynamic memory.
	 *
	 *   The rest of the fork hack should now proceed as in x86
	 *
	 */
	if (bIsWow64Process) {

		// allocate the heap for the child. this can be done even when
		// the child is suspended. 
		// avoids inexplicable allocation failures in the child.
		if (VirtualAllocEx(hProc,
					__heap_base,
					__heap_size,
					MEM_RESERVE,
					PAGE_READWRITE) == NULL) {
			dprintf("virtual allocex failed %d\n",GetLastError());
			goto error;
		}
		if (VirtualAllocEx(hProc,
					__heap_base,
					__heap_size,
					MEM_COMMIT,
					PAGE_READWRITE) == NULL) {
			dprintf("virtual allocex2 failed %d\n",GetLastError());
			goto error;
		}

		// Do NOT expect existing events
		if (!CreateWow64Events(pi.dwProcessId,&h64Parent,&h64Child,FALSE)) {
			goto error;
		}
		ResumeThread(hThread);

		// wait for the child to tell us it is running
		//if (WaitForSingleObject(h64Child,FORK_TIMEOUT) != WAIT_OBJECT_0) {
		//	rc = GetLastError();
		//	goto error;
		//}
		hArray[0] = h64Child;
		hArray[1] = hProc;

		if (WaitForMultipleObjects(2,hArray,FALSE,FORK_TIMEOUT) != 
				WAIT_OBJECT_0){

			rc = GetLastError();
			goto error;
		}

	}
	//
	// Copy all the shared data
	//
	if (!WriteProcessMemory(hProc,&gForkData,&gForkData,
				sizeof(ForkData),&rc)) {
		goto error;
	}
	if (rc != sizeof(ForkData)) 
		goto error;

	if (!bIsWow64Process) {
		rc = ResumeThread(hThread);
	}
	// in the wow64 case, the child will be waiting  on h64parent again.
	// set it, and then wait for h64child. This will mean the child has
	// a stack set up at the right location.
	else {
		SetEvent(h64Parent);
		hArray[0] = h64Child;
		hArray[1] = hProc;

		if (WaitForMultipleObjects(2,hArray,FALSE,FORK_TIMEOUT) != 
				WAIT_OBJECT_0){

			rc = GetLastError();
			goto error;
		}
		CloseHandle(h64Parent);
		CloseHandle(h64Child);
		h64Parent = h64Child = NULL;
	}

	//
	// Wait for the child to start and init itself.
	// The timeout is so that we don't wait too long
	//
	hArray[0] = __hforkchild;
	hArray[1] = hProc;

	if (WaitForMultipleObjects(2,hArray,FALSE,FORK_TIMEOUT) != WAIT_OBJECT_0){

		int err = GetLastError(); // For debugging purposes
		dprintf("wait failed err %d\n",err);
		goto error;
	}

	// Stop the child again and copy the stack and heap
	//
	SuspendThread(hThread);

	if (!SetThreadPriority(hThread,priority) ) {
		priority =GetLastError();
	}

	// stack
	stacksize = (char*)__fork_stack_begin - (char*)__fork_stack_end;
	if (!WriteProcessMemory(hProc,(char *)__fork_stack_end,
				(char *)__fork_stack_end,
				(u_long)stacksize,
				&rc)){
		goto error;
	}
	//
	// copy heap itself
	if (!WriteProcessMemory(hProc, (void*)__heap_base,(void*)__heap_base, 
				(DWORD)((char*)__heap_top-(char*)__heap_base),
				&rc)){
		goto error;
	}

	rc = fork_copy_user_mem(hProc);

	if(rc) {
		goto error;
	}

	// Release the child.
	SetEvent(__hforkparent);
	rc = ResumeThread(hThread);

	__forked=0;
	dprintf("forked process %d\n",pi.dwProcessId);
	start_sigchild_thread(hProc,pi.dwProcessId);
	close_copied_fds();

	CloseHandle(hThread);
	//
	// return process id to parent.
	return pi.dwProcessId;

error:
	__forked=0;
	SetEvent(__hforkparent);
	ResumeThread(hThread);
	CloseHandle(hProc);
	CloseHandle(hThread);
	if (h64Parent) {
		SetEvent(h64Parent); // don't let child block forever
		CloseHandle(h64Parent);
	}
	if (h64Child)
		CloseHandle(h64Child);
	return -1;
}
#pragma optimize("",off)
// The damn optimizer will remove the recursion, resulting in an infinite
// loop. -amol 4/17/97
void stack_probe (void *ptr) {
	char buf[1000];
	int x;

	if (&x > (int *)ptr)
		stack_probe(ptr);
	(void)buf;
}
#pragma optimize("",on)
//
// This function basically reserves some heap space.
// In the child it also commits the size committed in the parent.
void heap_init(void) {

	char * temp;
	int err;
	if (__forked) {
		temp = (char *)VirtualAlloc((void*)__heap_base,__heap_size, MEM_RESERVE,
				PAGE_READWRITE);
		if (temp != (char*)__heap_base) {
			if (!temp){
				err = GetLastError();
				if (bIsWow64Process)
					ExitProcess(0);
				abort();
			}
			else 
				__heap_base = temp;
		}
		if (!VirtualAlloc(__heap_base,(char*)__heap_top -(char*)__heap_base, 
					MEM_COMMIT,PAGE_READWRITE)){
			err = GetLastError();
			if (bIsWow64Process)
				ExitProcess(0);
			abort();
		}
		temp = (char*)__heap_base;
	}
	else {
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		__heap_size = sysinfo.dwPageSize * 1024;
		__heap_base = VirtualAlloc(0 , __heap_size,MEM_RESERVE|MEM_TOP_DOWN,
				PAGE_READWRITE);

		if (__heap_base == 0) {
			abort();
		}

		__heap_top = __heap_base;
	}

}
//
// Implementation of sbrk() for the fmalloc family
//
void * sbrk(int delta) {

	void *retval;
	void *old_top=__heap_top;
	char *b = (char*)__heap_top;

	if (delta == 0)
		return  __heap_top;
	if (delta > 0) {

		retval =VirtualAlloc((void*)__heap_top,delta,MEM_COMMIT,PAGE_READWRITE);

		if (retval == 0 )
			abort();

		b += delta;
		__heap_top = (void*)b;
	}
	else {
		retval = VirtualAlloc((void*)((char*)__heap_top - delta), 
				delta,MEM_DECOMMIT, PAGE_READWRITE);

		if (retval == 0)
			abort();

		b -= delta;
		__heap_top = (void*)b;
	}

	return (void*) old_top;
}
/*
 * Semantics of CreateWow64Events
 *
 * Try to open the events even if bOpenExisting is FALSE. This will help
 * us detect name duplication.
 *
 *       1. If OpenEvent succeeds,and bOpenExisting is FALSE,  fail.
 *
 *       2. If OpenEvent failed,and bOpenExisting is TRUE fail
 *
 *       3. else create the events anew
 *
 */
#define TCSH_WOW64_PARENT_EVENT_NAME "tcsh-wow64-parent-event"
#define TCSH_WOW64_CHILD_EVENT_NAME  "tcsh-wow64-child-event"
BOOL CreateWow64Events(DWORD pid, HANDLE *hParent, HANDLE *hChild, 
		BOOL bOpenExisting) {

	SECURITY_ATTRIBUTES sa;
	char parentname[256],childname[256];

	*hParent = *hChild = NULL;

	// make darn sure they're not inherited
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor =0;
	sa.bInheritHandle = FALSE;
	//

#pragma warning(disable:4995)

	// This event tells the child to hold for gForkData to be copied
	wsprintfA(parentname, "Local\\%d-%s",pid, TCSH_WOW64_PARENT_EVENT_NAME);

	wsprintfA(childname, "Local\\%d-%s",pid, TCSH_WOW64_CHILD_EVENT_NAME );

#pragma warning(default:4995)

	*hParent = OpenEvent(EVENT_ALL_ACCESS,FALSE, parentname);

	if(*hParent) {
		if (bOpenExisting == FALSE) { // didn't expect to be a child process
			CloseHandle(*hParent);
			*hParent = NULL;
			return FALSE;
		}

		*hChild = OpenEvent(EVENT_ALL_ACCESS,FALSE, childname);
		if (!*hChild) {
			CloseHandle(*hParent);
			*hParent = NULL;
			return FALSE;
		}

		return TRUE;
	}
	else { //event does not exist
		if (bOpenExisting == TRUE)
			return FALSE;
	}

	*hParent = CreateEvent(&sa,FALSE,FALSE,parentname);	
	if (!*hParent)
		return FALSE;


	*hChild = CreateEvent(&sa,FALSE,FALSE,childname);	
	if (!*hChild){
		CloseHandle(*hParent);
		*hParent = NULL;
		return FALSE;
	}
	return TRUE;
}
