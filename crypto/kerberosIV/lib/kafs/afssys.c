/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "kafs_locl.h"

RCSID("$Id: afssys.c,v 1.53 1997/05/04 02:30:41 assar Exp $");

/* Magic to get AIX syscalls to work */
#ifdef _AIX

static int (*Pioctl)(char*, int, void*, int);
static int (*Setpag)(void);

#include "dlfcn.h"

static int
isSuid()
{
    int uid = getuid();
    int gid = getgid();
    int euid = getegid();
    int egid = getegid();
    return (uid != euid) || (gid != egid);
}

static int
aix_setup(void)
{
#ifdef STATIC_AFS_SYSCALLS
    Pioctl = aix_pioctl;
    Setpag = aix_setpag;
#else
    void *ptr;
    char path[MaxPathLen], *p;
    /*
     * If we are root or running setuid don't trust AFSLIBPATH!
     */
    if (getuid() != 0 && !isSuid() && (p = getenv("AFSLIBPATH")) != NULL)
	strcpy(path, p);
    else
	snprintf(path, sizeof(path), "%s/afslib.so", LIBDIR);
	
    ptr = dlopen(path, 0);
    if(ptr){
	Setpag = (int (*)(void))dlsym(ptr, "aix_setpag");
	Pioctl = (int (*)(char*, int, void*, int))dlsym(ptr, "aix_pioctl");
    }
#endif
}
#endif /* _AIX */

#define NO_ENTRY_POINT		0
#define SINGLE_ENTRY_POINT	1
#define MULTIPLE_ENTRY_POINT	2
#define SINGLE_ENTRY_POINT2	3
#define SINGLE_ENTRY_POINT3	4
#define AIX_ENTRY_POINTS	5
#define UNKNOWN_ENTRY_POINT	6
static int afs_entry_point = UNKNOWN_ENTRY_POINT;
static int afs_syscalls[2];


int
k_pioctl(char *a_path,
	 int o_opcode,
	 struct ViceIoctl *a_paramsP,
	 int a_followSymlinks)
{
#ifndef NO_AFS
    switch(afs_entry_point){
#if defined(AFS_SYSCALL) || defined(AFS_SYSCALL2) || defined(AFS_SYSCALL3)
    case SINGLE_ENTRY_POINT:
    case SINGLE_ENTRY_POINT2:
    case SINGLE_ENTRY_POINT3:
	return syscall(afs_syscalls[0], AFSCALL_PIOCTL,
		       a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif
#if defined(AFS_PIOCTL)
    case MULTIPLE_ENTRY_POINT:
	return syscall(afs_syscalls[0],
		       a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif
#ifdef _AIX
    case AIX_ENTRY_POINTS:
	return Pioctl(a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif
    }
    
    errno = ENOSYS;
#ifdef SIGSYS
    kill(getpid(), SIGSYS);	/* You loose! */
#endif
#endif /* NO_AFS */
    return -1;
}

int
k_afs_cell_of_file(const char *path, char *cell, int len)
{
    struct ViceIoctl parms;
    parms.in = NULL;
    parms.in_size = 0;
    parms.out = cell;
    parms.out_size = len;
    return k_pioctl((char*)path, VIOC_FILE_CELL_NAME, &parms, 1);
}

int
k_unlog(void)
{
    struct ViceIoctl parms;
    memset(&parms, 0, sizeof(parms));
    return k_pioctl(0, VIOCUNLOG, &parms, 0);
}

int
k_setpag(void)
{
#ifndef NO_AFS
    switch(afs_entry_point){
#if defined(AFS_SYSCALL) || defined(AFS_SYSCALL2) || defined(AFS_SYSCALL3)
    case SINGLE_ENTRY_POINT:
    case SINGLE_ENTRY_POINT2:
    case SINGLE_ENTRY_POINT3:
	return syscall(afs_syscalls[0], AFSCALL_SETPAG);
#endif
#if defined(AFS_PIOCTL)
    case MULTIPLE_ENTRY_POINT:
	return syscall(afs_syscalls[1]);
#endif
#ifdef _AIX
    case AIX_ENTRY_POINTS:
	return Setpag();
#endif
    }
    
    errno = ENOSYS;
#ifdef SIGSYS
    kill(getpid(), SIGSYS);	/* You loose! */
#endif
#endif /* NO_AFS */
    return -1;
}

static jmp_buf catch_SIGSYS;

#ifdef SIGSYS

static RETSIGTYPE
SIGSYS_handler(int sig)
{
    errno = 0;
    signal(SIGSYS, SIGSYS_handler); /* Need to reinstall handler on SYSV */
    longjmp(catch_SIGSYS, 1);
}

#endif

int
k_hasafs(void)
{
    int saved_errno;
    RETSIGTYPE (*saved_func)();
    struct ViceIoctl parms;
  
    /*
     * Already checked presence of AFS syscalls?
     */
    if (afs_entry_point != UNKNOWN_ENTRY_POINT)
	return afs_entry_point != NO_ENTRY_POINT;

    /*
     * Probe kernel for AFS specific syscalls,
     * they (currently) come in two flavors.
     * If the syscall is absent we recive a SIGSYS.
     */
    afs_entry_point = NO_ENTRY_POINT;
    memset(&parms, 0, sizeof(parms));
  
    saved_errno = errno;
#ifndef NO_AFS
#ifdef SIGSYS
    saved_func = signal(SIGSYS, SIGSYS_handler);
#endif

#ifdef AFS_SYSCALL
    if (setjmp(catch_SIGSYS) == 0)
	{
	    syscall(AFS_SYSCALL, AFSCALL_PIOCTL,
		    0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	    if (errno == EINVAL)
		{
		    afs_entry_point = SINGLE_ENTRY_POINT;
		    afs_syscalls[0] = AFS_SYSCALL;
		    goto done;
		}
	}
#endif /* AFS_SYSCALL */

#ifdef AFS_PIOCTL
    if (setjmp(catch_SIGSYS) == 0)
	{
	    syscall(AFS_PIOCTL,
		    0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	    if (errno == EINVAL)
		{
		    afs_entry_point = MULTIPLE_ENTRY_POINT;
		    afs_syscalls[0] = AFS_PIOCTL;
		    afs_syscalls[1] = AFS_SETPAG;
		    goto done;
		}
	}
#endif /* AFS_PIOCTL */

#ifdef AFS_SYSCALL2
    if (setjmp(catch_SIGSYS) == 0)
	{
	    syscall(AFS_SYSCALL2, AFSCALL_PIOCTL,
		    0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	    if (errno == EINVAL)
		{
		    afs_entry_point = SINGLE_ENTRY_POINT2;
		    afs_syscalls[0] = AFS_SYSCALL2;
		    goto done;
		}
	}
#endif /* AFS_SYSCALL */

#ifdef AFS_SYSCALL3
    if (setjmp(catch_SIGSYS) == 0)
	{
	    syscall(AFS_SYSCALL3, AFSCALL_PIOCTL,
		    0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	    if (errno == EINVAL)
		{
		    afs_entry_point = SINGLE_ENTRY_POINT3;
		    afs_syscalls[0] = AFS_SYSCALL3;
		    goto done;
		}
	}
#endif /* AFS_SYSCALL */

#ifdef _AIX
    aix_setup();
    if(Pioctl != NULL && Setpag != NULL){
	afs_entry_point = AIX_ENTRY_POINTS;
	goto done;
    }
#endif

done:
#ifdef SIGSYS
    signal(SIGSYS, saved_func);
#endif
#endif /* NO_AFS */
    errno = saved_errno;
    return afs_entry_point != NO_ENTRY_POINT;
}
