/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  $Id: linux_sysent.c,v 1.3 1995/11/22 07:43:52 bde Exp $
 */

/* XXX we use functions that might not exist. */
#define	COMPAT_43	1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/imgact.h>

#include <i386/linux/linux.h>
#include <i386/linux/sysproto.h>

static struct sysent linux_sysent[] = {
    0, (sy_call_t *)linux_setup,		/* 0 */
    1, (sy_call_t *)exit,			/* 1 */
    0, (sy_call_t *)linux_fork,			/* 2 */
    3, (sy_call_t *)read,			/* 3 */
    3, (sy_call_t *)write,			/* 4 */
    3, (sy_call_t *)linux_open,			/* 5 */
    1, (sy_call_t *)close,			/* 6 */
    3, (sy_call_t *)linux_waitpid,		/* 7 */
    2, (sy_call_t *)linux_creat,		/* 8 */
    2, (sy_call_t *)link,			/* 9 */
    1, (sy_call_t *)unlink,	    		/* 10 */
    3, (sy_call_t *)execve,	    		/* 11 */
    1, (sy_call_t *)chdir,			/* 12 */
    1, (sy_call_t *)linux_time,			/* 13 */
    3, (sy_call_t *)linux_mknod,		/* 14 */
    2, (sy_call_t *)chmod,			/* 15 */
    3, (sy_call_t *)chown,			/* 16 */
    1, (sy_call_t *)linux_break,		/* 17 */
    2, (sy_call_t *)linux_stat,			/* 18 */
    3, (sy_call_t *)linux_lseek,		/* 19 */
    0, (sy_call_t *)getpid,	    		/* 20 */
    5, (sy_call_t *)linux_mount,		/* 21 */
    1, (sy_call_t *)linux_umount,		/* 22 */
    1, (sy_call_t *)setuid,	    		/* 23 */
    0, (sy_call_t *)getuid,	    		/* 24 */
    1, (sy_call_t *)linux_stime,		/* 25 */
    4, (sy_call_t *)linux_ptrace,		/* 26 */
    1, (sy_call_t *)linux_alarm,		/* 27 */
    2, (sy_call_t *)linux_fstat,		/* 28 */
    0, (sy_call_t *)linux_pause,		/* 29 */
    2, (sy_call_t *)linux_utime,		/* 30 */
    0, (sy_call_t *)linux_stty,			/* 31 */
    0, (sy_call_t *)linux_gtty,			/* 32 */
    2, (sy_call_t *)access,	    		/* 33 */
    1, (sy_call_t *)linux_nice,			/* 34 */
    0, (sy_call_t *)linux_ftime,		/* 35 */
    0, (sy_call_t *)sync,			/* 36 */
    2, (sy_call_t *)linux_kill,			/* 37 */
    2, (sy_call_t *)rename,	    		/* 38 */
    2, (sy_call_t *)mkdir,			/* 39 */
    1, (sy_call_t *)rmdir,			/* 40 */
    1, (sy_call_t *)dup,	    		/* 41 */
    1, (sy_call_t *)linux_pipe,			/* 42 */
    1, (sy_call_t *)linux_times,		/* 43 */
    0, (sy_call_t *)linux_prof,			/* 44 */
    1, (sy_call_t *)linux_brk,			/* 45 */
    1, (sy_call_t *)setgid,	    		/* 46 */
    0, (sy_call_t *)getgid,	    		/* 47 */
    2, (sy_call_t *)linux_signal,		/* 48 */
    0, (sy_call_t *)geteuid,	    		/* 49 */
    0, (sy_call_t *)getegid,	    		/* 50 */
    0, (sy_call_t *)acct,			/* 51 */
    0, (sy_call_t *)linux_phys,			/* 52 */
    0, (sy_call_t *)linux_lock,			/* 53 */
    3, (sy_call_t *)linux_ioctl,		/* 54 */
    3, (sy_call_t *)linux_fcntl,		/* 55 */
    0, (sy_call_t *)linux_mpx,			/* 56 */
    2, (sy_call_t *)setpgid,	    		/* 57 */
    0, (sy_call_t *)linux_ulimit,		/* 58 */
    1, (sy_call_t *)linux_olduname,		/* 59 */
    1, (sy_call_t *)umask,			/* 60 */
    1, (sy_call_t *)chroot,	    		/* 61 */
    2, (sy_call_t *)linux_ustat,		/* 62 */
    2, (sy_call_t *)dup2,			/* 63 */
    0, (sy_call_t *)getppid,	    		/* 64 */
    0, (sy_call_t *)getpgrp,	    		/* 65 */
    0, (sy_call_t *)setsid,	    		/* 66 */
    3, (sy_call_t *)linux_sigaction,		/* 67 */
    0, (sy_call_t *)linux_siggetmask,		/* 68 */
    1, (sy_call_t *)linux_sigsetmask,		/* 69 */
    2, (sy_call_t *)setreuid,			/* 70 */
    2, (sy_call_t *)setregid,			/* 71 */
    1, (sy_call_t *)linux_sigsuspend,		/* 72 */
    1, (sy_call_t *)linux_sigpending,		/* 73 */
    2, (sy_call_t *)osethostname,		/* 74 */
    2, (sy_call_t *)osetrlimit,			/* 75 */
    2, (sy_call_t *)ogetrlimit,			/* 76 */
    2, (sy_call_t *)getrusage,			/* 77 */
    2, (sy_call_t *)gettimeofday,		/* 78 */
    2, (sy_call_t *)settimeofday,		/* 79 */
    2, (sy_call_t *)getgroups,			/* 80 */
    2, (sy_call_t *)setgroups,			/* 81 */
    1, (sy_call_t *)linux_select,		/* 82 */
    2, (sy_call_t *)symlink,	    		/* 83 */
    2, (sy_call_t *)ostat,			/* 84 */
    3, (sy_call_t *)readlink,			/* 85 */
    1, (sy_call_t *)linux_uselib,		/* 86 */
    1, (sy_call_t *)swapon,	    		/* 87 */
    3, (sy_call_t *)reboot,	    		/* 88 */
    3, (sy_call_t *)linux_readdir,		/* 89 */
    1, (sy_call_t *)linux_mmap,			/* 90 */
    2, (sy_call_t *)munmap,	    		/* 91 */
    2, (sy_call_t *)otruncate,			/* 92 */
    2, (sy_call_t *)oftruncate,			/* 93 */
    2, (sy_call_t *)fchmod,	    		/* 94 */
    3, (sy_call_t *)fchown,	    		/* 95 */
    2, (sy_call_t *)getpriority,		/* 96 */
    3, (sy_call_t *)setpriority,		/* 97 */
    0, (sy_call_t *)profil,	    		/* 98 */
    2, (sy_call_t *)linux_statfs,		/* 99 */
    2, (sy_call_t *)linux_fstatfs,		/* 100 */
    3, (sy_call_t *)linux_ioperm,		/* 101 */
    2, (sy_call_t *)linux_socketcall,		/* 102 */
    3, (sy_call_t *)linux_syslog,		/* 103 */
    3, (sy_call_t *)setitimer,			/* 104 */
    2, (sy_call_t *)getitimer,			/* 105 */
    2, (sy_call_t *)linux_newstat,		/* 106 */
    2, (sy_call_t *)linux_newlstat,		/* 107 */
    2, (sy_call_t *)linux_newfstat,		/* 108 */
    2, (sy_call_t *)linux_uname,		/* 109 */
    1, (sy_call_t *)linux_iopl,			/* 110 */
    0, (sy_call_t *)linux_vhangup,		/* 111 */
    0, (sy_call_t *)linux_idle,			/* 112 */
    1, (sy_call_t *)linux_vm86,			/* 113 */
    4, (sy_call_t *)linux_wait4,		/* 114 */
    1, (sy_call_t *)linux_swapoff,		/* 115 */
    1, (sy_call_t *)linux_sysinfo,		/* 116 */
    4, (sy_call_t *)linux_ipc,			/* 117 */
    1, (sy_call_t *)fsync,			/* 118 */
    1, (sy_call_t *)linux_sigreturn,		/* 119 */
    0, (sy_call_t *)linux_clone,		/* 120 */
    2, (sy_call_t *)setdomainname,		/* 121 */
    1, (sy_call_t *)linux_newuname,		/* 122 */
    3, (sy_call_t *)linux_modify_ldt,		/* 123 */
    1, (sy_call_t *)linux_adjtimex,		/* 124 */
    3, (sy_call_t *)mprotect,			/* 125 */
    3, (sy_call_t *)linux_sigprocmask,		/* 126 */
    2, (sy_call_t *)linux_create_module,	/* 127 */
    4, (sy_call_t *)linux_init_module,		/* 128 */
    1, (sy_call_t *)linux_delete_module,	/* 129 */
    1, (sy_call_t *)linux_get_kernel_syms,	/* 130 */
    0, (sy_call_t *)linux_quotactl,		/* 131 */
    1, (sy_call_t *)linux_getpgid,		/* 132 */
    1, (sy_call_t *)fchdir,	    		/* 133 */
    0, (sy_call_t *)linux_bdflush,		/* 134 */
};

int bsd_to_linux_errno[ELAST] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
     10, 35, 12, 13, 14, 15, 16, 17, 18, 19,
     20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
     30, 31, 32, 33, 34, 11,115,114, 88, 89,
     90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
    100,101,102,103,104,105,106,107,108,109,
    110,111, 40, 36,112,113, 39, 11, 87,122,
    116, 66,  6,  6,  6,  6,  6, 37, 38,  9,
      6, 
};

int bsd_to_linux_signal[NSIG] = {
    0, LINUX_SIGHUP, LINUX_SIGINT, LINUX_SIGQUIT,
    LINUX_SIGILL, LINUX_SIGTRAP, LINUX_SIGABRT, 0,
    LINUX_SIGFPE, LINUX_SIGKILL, LINUX_SIGBUS, LINUX_SIGSEGV, 
    0, LINUX_SIGPIPE, LINUX_SIGALRM, LINUX_SIGTERM,
    LINUX_SIGURG, LINUX_SIGSTOP, LINUX_SIGTSTP, LINUX_SIGCONT,	
    LINUX_SIGCHLD, LINUX_SIGTTIN, LINUX_SIGTTOU, LINUX_SIGIO, 
    LINUX_SIGXCPU, LINUX_SIGXFSZ, LINUX_SIGVTALRM, LINUX_SIGPROF, 
    LINUX_SIGWINCH, 0, LINUX_SIGUSR1, LINUX_SIGUSR2
};

int linux_to_bsd_signal[LINUX_NSIG] = {
    0, SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGEMT,
    SIGFPE, SIGKILL, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM, 
    SIGBUS, SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGIO,
    SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH, SIGURG, SIGURG, 0
};

int linux_fixup(int **stack_base, struct image_params *imgp)
{
    int *argv, *envp;

    argv = *stack_base;
    envp = *stack_base + (imgp->argc + 1);
    (*stack_base)--;
    **stack_base = (int)envp;
    (*stack_base)--;
    **stack_base = (int)argv;
    (*stack_base)--;
    **stack_base = (int)imgp->argc;
    return 0;			/* XXX */
}

struct sysentvec linux_sysvec = {
    sizeof (linux_sysent) / sizeof(linux_sysent[0]),
    linux_sysent,
    0xff,
    NSIG,
    bsd_to_linux_signal,
    ELAST, 
    bsd_to_linux_errno,
    linux_fixup
};
