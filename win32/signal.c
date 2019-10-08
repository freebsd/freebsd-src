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
 * signal.c: Signal emulation hacks.
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include "ntport.h"
#include "forkdata.h"
#include "signal.h"

#pragma warning(disable:4055)

#define SIGBAD(signo)        ( (signo) <=0 || (signo) >=NSIG) 
#define fast_sigmember(a,b)  ( (*(a) & (1 << (b-1)) ) )
#define inc_pending(a)       (gPending[(a)]+=1)

#define suspend_main_thread() SuspendThread(hmainthr)
#define resume_main_thread() ResumeThread(hmainthr)

int generic_handler(DWORD);
int ctrl_handler(DWORD);

typedef struct _child_list {
	DWORD dwProcessId;
	DWORD exitcode;
	struct _child_list *next;
}ChildListNode;

Sigfunc *handlers[NSIG]={0};
static unsigned long gPending[NSIG]={0};
static unsigned long gBlockMask = 0;

static ChildListNode *clist_h; //head of list
static ChildListNode *clist_t; // tail of list

static CRITICAL_SECTION sigcritter;
static HANDLE hmainthr;
static HANDLE hsigsusp;
static int __is_suspended = 0;
static HANDLE __halarm=0;

extern HANDLE __h_con_alarm,__h_con_int, __h_con_hup;

// must be done before fork;
void nt_init_signals(void) {
	
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler,TRUE);
	InitializeCriticalSection(&sigcritter);

	clist_t = clist_h = NULL;


	if (!DuplicateHandle(GetCurrentProcess(),
					GetCurrentThread(),
					GetCurrentProcess(),
					&hmainthr,
					0,
					FALSE,
					DUPLICATE_SAME_ACCESS)){
		ExitProcess(GetLastError());
	}
	hsigsusp = CreateEvent(NULL,FALSE,FALSE,NULL);
	__h_con_alarm=CreateEvent(NULL,FALSE,FALSE,NULL);
	__h_con_int=CreateEvent(NULL,FALSE,FALSE,NULL);
	__h_con_hup=CreateEvent(NULL,FALSE,FALSE,NULL);
	if (!hsigsusp)
		abort();

}
void nt_cleanup_signals(void) {
	if (__forked)
		return;
	DeleteCriticalSection(&sigcritter);
	CloseHandle(hmainthr);
	CloseHandle(hsigsusp);
	CloseHandle(__h_con_alarm);
	CloseHandle(__h_con_int);
	CloseHandle(__h_con_hup);
	CloseHandle(__halarm);
}
int sigaddset(sigset_t *set, int signo) {

	if (SIGBAD(signo)) {
		errno = EINVAL;
		return -1;
	}
	*set |= 1 << (signo-1);
	return 0;
}
int sigdelset(sigset_t *set, int signo) {
	if (SIGBAD(signo)) {
		errno = EINVAL;
		return -1;
	}
	*set &= ~( 1 << (signo-1));

	return 0;
	
}
int sigismember(const sigset_t *set, int signo) {
	if (SIGBAD(signo)) {
		errno = EINVAL;
		return -1;
	}

	return ( (*set & (1 <<(signo-1)) ) != 0);
	
}
void deliver_pending(void) {
	unsigned long temp;
	int sig=1;

	temp = ~gBlockMask;
	while(temp && (sig < NSIG)) {

		if (temp & 0x01){
			if (gPending[sig]){
				//gPending[sig]=0;
                do {
                    dprintf("deliver_pending for sig %d\n",sig);
                    gPending[sig]--;
                    generic_handler(sig);
                }while(gPending[sig] != 0);
			}
		}
		temp >>= 1;
		sig++;
	}
}
int sigprocmask(int how, const sigset_t *set, sigset_t*oset) {

	if (oset)
		*oset = gBlockMask;
	if (set) {
		switch (how) {
			case SIG_BLOCK:
				gBlockMask |= *set;
				break;
			case SIG_UNBLOCK:
				gBlockMask &= (~(*set));
				break;
			case SIG_SETMASK:
				gBlockMask = *set;
				break;
			default:
				break;
		}
	}
	if (how != SIG_BLOCK)
		deliver_pending();

	return 0;

}
int sigsuspend(const sigset_t *mask) {
	sigset_t omask;


	EnterCriticalSection(&sigcritter);
	__is_suspended++;
	LeaveCriticalSection(&sigcritter);

	sigprocmask(SIG_SETMASK,mask,&omask);

    dprintf("suspending main thread susp count %d\n",__is_suspended);
    do {
        WaitForSingleObject(hsigsusp,INFINITE);
    }while(__is_suspended > 0);


	sigprocmask(SIG_SETMASK,&omask,0);
	errno = EINTR;
	return -1;

}

int sigaction(int signo, const struct sigaction *act, struct sigaction *oact) {

	if (SIGBAD(signo)) {
		errno = EINVAL;
		return -1;
	}

	if(oact){
			oact->sa_handler = handlers[signo];
			oact->sa_mask = 0;
			oact->sa_flags =0;
	}
	if ((signo == SIGHUP) && (act && (act->sa_handler == SIG_IGN)) 
						&& __forked)
		__nt_child_nohupped = 1;
	if (act)
		handlers[signo]=act->sa_handler;

	return 0;
	
}
int ctrl_handler(DWORD event) {

	if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
		SetEvent(__h_con_int);
		return TRUE;
	}
	if (event == CTRL_CLOSE_EVENT) {
		SetEvent(__h_con_hup);
		return TRUE;
	}

	return generic_handler(event+1);
}
int generic_handler(DWORD signo) {

	int blocked=0;

	if (SIGBAD(signo) )
		return FALSE;
	switch (signo) {
		case SIGINT:
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] == SIG_DFL)
					ExitProcess(0xC000013AL);
				else
					handlers[signo](signo);
			}
			break;
		case SIGBREAK:
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] == SIG_DFL)
					ExitProcess(0xC000013AL);
				else
					handlers[signo](signo);
			}
			break;
		case SIGHUP: //CTRL_CLOSE_EVENT
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] == SIG_DFL)
					ExitProcess(604);
				else
					handlers[signo](signo);
			}
			break;
		case SIGTERM: //CTRL_LOGOFF_EVENT
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] == SIG_DFL)
					ExitProcess(604);
				else
					handlers[signo](signo);
			}
			else
				ExitProcess(604);
			break;
		case SIGKILL: //CTRL_SHUTDOWN_EVENT
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] == SIG_DFL)
					ExitProcess(604);
				else
					handlers[signo](signo);
			}
			else
				ExitProcess(604);
			break;
		case SIGALRM:
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] == SIG_DFL)
					ExitProcess(604);
				else
					handlers[signo](signo);
			}
			break;
		case SIGCHLD:
			if (handlers[signo] != SIG_IGN){
				if (fast_sigmember(&gBlockMask,signo) ) {
                    dprintf("inc pending for sig %d count %d\n",signo,
                        gPending[signo]);
					inc_pending(signo);
					blocked=1;
				}
				else if (handlers[signo] != SIG_DFL)
					handlers[signo](signo);
			}
			break;
		default:
			ExitProcess(604);
			break;
	}
    if (!blocked && __is_suspended) {
        EnterCriticalSection(&sigcritter);
        __is_suspended--;
        LeaveCriticalSection(&sigcritter);
        dprintf("releasing suspension is_suspsend = %d\n",__is_suspended);
        SetEvent(hsigsusp);
    }
	return TRUE;
}
Sigfunc *_nt_signal(int signal, Sigfunc * handler) {

	Sigfunc *old;

	if (SIGBAD(signal)) {
		errno = EINVAL;
		return SIG_ERR;
	}
	if (signal == SIGHUP  && handler == SIG_IGN && __forked) {
		__nt_child_nohupped = 1;
	}


	old = handlers[signal];
	handlers[signal] = handler;


	return old;
}
int waitpid(pid_t pid, int *statloc, int options) {
	
	ChildListNode *temp;
	int retcode;

	UNREFERENCED_PARAMETER(options);
	errno = EINVAL;
	if (pid != -1)
		return -1;

	EnterCriticalSection(&sigcritter);
		if (!clist_h)
			retcode =0;
		else {
			retcode = clist_h->dwProcessId;
			if (statloc) *statloc = clist_h->exitcode;
			temp = clist_h;
			clist_h = clist_h->next;
			heap_free(temp);
		}
	LeaveCriticalSection(&sigcritter);

	errno = 0;
	return retcode;
	
}
unsigned int __alarm_set=0;

void CALLBACK alarm_callback( unsigned long interval) {

	int rc;

	rc = WaitForSingleObject(__halarm,interval*1000);
	if (rc != WAIT_TIMEOUT)
		return ;

	SetEvent(__h_con_alarm);
	__alarm_set = 0;
	return;
	
	// consoleread() now waits for above event, and calls generic_handler to
	// handle SIGALRM in the main thread. That helps me avoid
	// problems with  fork() when we are in a secondary thread.
	//
	// This means sched, periodic etc will not be signalled unless consoleread
	// is called, but that's a reasonable risk, i think.
	// -amol 4/10/97

}
unsigned int alarm(unsigned int seconds) {

	unsigned int temp;
	static unsigned int prev_val=0;
	HANDLE ht;
	DWORD tid;
	SECURITY_ATTRIBUTES secd;

	secd.nLength=sizeof(secd);
	secd.lpSecurityDescriptor=NULL;
	secd.bInheritHandle=TRUE;


	if (!__halarm) {
		__halarm=CreateEvent(&secd,FALSE,FALSE,NULL);
                if(!__halarm) {
                        return 0;
                }
	}
	if(__alarm_set )
		SetEvent(__halarm);

	if (!seconds){
		__alarm_set=0;
		return 0;
	}
	__alarm_set = 1;

	ht = CreateThread(NULL,gdwStackSize,
				(LPTHREAD_START_ROUTINE)alarm_callback, 
				(void*)UIntToPtr(seconds),
				0,&tid);
	if (ht)
		CloseHandle(ht);
	
	temp = prev_val;
	prev_val = seconds*1000;

	return temp;
}
void add_to_child_list(DWORD dwpid,DWORD exitcode) {
	if (clist_h == NULL) {
		clist_h = heap_alloc(sizeof(ChildListNode));
		if (!clist_h)
			goto end;
		clist_h->dwProcessId = dwpid;
		clist_h->exitcode = exitcode;
		clist_h->next= NULL;
		clist_t = clist_h;
	}
	else {
		clist_t->next = heap_alloc(sizeof(ChildListNode));
		if (!clist_t->next)
			goto end;
		clist_t = clist_t->next;
		clist_t->dwProcessId= dwpid;
		clist_h->exitcode = exitcode;
		clist_t->next = NULL;	
	}
end:
	;
}
void sig_child_callback(DWORD pid,DWORD exitcode) {
	
	DWORD ecode = 0;

	EnterCriticalSection(&sigcritter);
	add_to_child_list(pid,exitcode);
	suspend_main_thread();
	//
	// pchild() tries to reset(), which crashes the thread
	//
	__try {
		generic_handler(SIGCHLD);
	}
	__except(ecode = GetExceptionCode()) {
		;
	}
	resume_main_thread();
	LeaveCriticalSection(&sigcritter);

}
struct thread_args {
	DWORD pid;
	HANDLE hproc;
};
void sigchild_thread(struct thread_args *args) {

	DWORD exitcode=0;
	WaitForSingleObject(args->hproc,INFINITE);
	GetExitCodeProcess(args->hproc,&exitcode);
	CloseHandle(args->hproc);
	sig_child_callback(args->pid,exitcode);
    dprintf("exiting sigchild thread for pid %d\n",args->pid);
	heap_free(args);
}
void start_sigchild_thread(HANDLE hproc, DWORD pid) {

    struct thread_args *args=heap_alloc(sizeof(struct thread_args));
    DWORD tid;
    HANDLE hthr;
    if(!args) {
        return;
    }
    args->hproc = hproc;
    args->pid = pid;

    dprintf("creating sigchild thread for pid %d\n",pid);
    hthr = CreateThread(NULL,
            gdwStackSize,
            (LPTHREAD_START_ROUTINE)sigchild_thread,
            (LPVOID)args,
            0,
            &tid);


    if(hthr) {
        CloseHandle(hthr);
    }

}
int kill(int pid, int sig) {

    HANDLE hproc;
    int ret =0;
    extern DWORD gdwPlatform;
    BOOL is_winnt = TRUE;

    errno = EPERM;
    is_winnt = (gdwPlatform != VER_PLATFORM_WIN32_WINDOWS);

    if(is_winnt) {
        if(pid < 0)
        {
            if (pid == -1)
                return -1;
            pid = -pid; //no groups that we can actually do anything with.

        }
    }
    else { //win9x has -ve pids
        if(pid > 0)
        {
            if (pid == 1)
                return -1;
            pid = -pid; //no groups that we can actually do anything with.

        }
    }


    switch(sig) {
        case 0:
        case 7:
            hproc = OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
            if (hproc  == NULL) {
                errno = ESRCH;
                ret = -1;
                dprintf("proc %d not found\n",pid);
                break;
            }
            else{
                dprintf("proc %d found\n",pid);
            }
            if (sig == 7) {
                if (!TerminateProcess(hproc,0xC000013AL) ) {
                    ret = -1;
                }
            }
            CloseHandle(hproc);
            break;
        case 1:
            if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT,pid)) 
                ret = -1;
            break;
        case 2:
            if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,pid)) 
                ret = -1;
            break;
        case 3:
            if (kill_by_wm_close(pid) <0 ) {
                errno = ESRCH;
                ret = -1;
            }
        default:
            break;
    }
    return ret;
}
//
// nice(niceness)
//
// where niceness is an integer in the range -6 to +7
//
// A usual foreground process starts at level 9 in the chart below
//
// the range -6 to +7 takes it from Base priority 15 down to 2. 
//
// Note that level 1 or > 15 are not allowed.
//
// Priority Level 11 (niceness -2) or greater affects system performance, 
//	so use with care.
//
// niceness defaults to  +4, which is lowest for background normal class.
// As in unix, +ve niceness indicates lower priorities.

/***************************************************************************
Niceness    Base    Priority class/thread priority

            1    Idle, normal, or high class,    THREAD_PRIORITY_IDLE

+7          2    Idle class,                     THREAD_PRIORITY_LOWEST
+6          3    Idle class,                     THREAD_PRIORITY_BELOW_NORMAL
+5          4    Idle class,                     THREAD_PRIORITY_NORMAL
+4          5    Background normal class,        THREAD_PRIORITY_LOWEST
                    Idle class,                  THREAD_PRIORITY_ABOVE_NORMAL
+3          6    Background normal class,        THREAD_PRIORITY_BELOW_NORMAL
                    Idle class,                  THREAD_PRIORITY_HIGHEST
+2          7    Foreground normal class,        THREAD_PRIORITY_LOWEST
                    Background normal class,     THREAD_PRIORITY_NORMAL
+1          8    Foreground normal class,        THREAD_PRIORITY_BELOW_NORMAL
                    Background normal class,     THREAD_PRIORITY_ABOVE_NORMAL
 0          9    Foreground normal class,        THREAD_PRIORITY_NORMAL
                    Background normal class,     THREAD_PRIORITY_HIGHEST
-1          10   Foreground normal class,        THREAD_PRIORITY_ABOVE_NORMAL
-2          11    High class,                    THREAD_PRIORITY_LOWEST
                    Foreground normal class,     THREAD_PRIORITY_HIGHEST
-3          12    High class,                    THREAD_PRIORITY_BELOW_NORMAL
-4          13    High class,                    THREAD_PRIORITY_NORMAL
-5          14    High class,                    THREAD_PRIORITY_ABOVE_NORMAL
-6          15    Idle, normal, or high class,   THREAD_PRIORITY_TIME_CRITICAL 
                  High class,                    THREAD_PRIORITY_HIGHEST


    16    Real-time class, THREAD_PRIORITY_IDLE
    22    Real-time class, THREAD_PRIORITY_LOWEST
    23    Real-time class, THREAD_PRIORITY_BELOW_NORMAL
    24    Real-time class, THREAD_PRIORITY_NORMAL
    25    Real-time class, THREAD_PRIORITY_ABOVE_NORMAL
    26    Real-time class, THREAD_PRIORITY_HIGHEST
    31    Real-time class, THREAD_PRIORITY_TIME_CRITICAL
****************************************************************************/
int nice(int niceness) {

    DWORD pclass = IDLE_PRIORITY_CLASS;
    int priority = THREAD_PRIORITY_NORMAL;

    if (niceness < -6 || niceness > 7) {
        errno = EPERM;
        return -1;
    }
    switch (niceness) {
        case 7:
            pclass = IDLE_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_LOWEST;
            break;
        case 6:
            pclass = IDLE_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case 5:
            pclass = IDLE_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_NORMAL;
            break;
        case 4:
            pclass = IDLE_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        case 3:
            pclass = IDLE_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_HIGHEST;
            break;
        case 2:
            pclass = NORMAL_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_LOWEST;
            break;
        case 1:
            pclass = NORMAL_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case (-1):
            pclass = NORMAL_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        case (-2):
            pclass = NORMAL_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_HIGHEST;
            break;
        case (-3):
            pclass = HIGH_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case (-4):
            pclass = HIGH_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_NORMAL;
            break;
        case (-5):
            pclass = HIGH_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        case (-6):
            pclass = HIGH_PRIORITY_CLASS;
            priority = THREAD_PRIORITY_HIGHEST;
            break;
        default:
            break;
    }

    if (!SetPriorityClass(GetCurrentProcess(),pclass)){
        errno = EPERM;
        return -1;
    }
    if (!SetThreadPriority(GetCurrentThread(),priority)){
        errno = EPERM;
        return -1;
    }
	return -1;
}
