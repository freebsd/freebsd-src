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
 *  $Id: linux_sysent.c,v 1.1 1995/06/25 17:32:43 sos Exp $
 */

#include <i386/linux/linux.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/imgact.h>
#include <sys/errno.h>
#include <sys/signal.h>

extern int access();
extern int acct();
extern int linux_adjtimex();
extern int linux_alarm();
extern int linux_bdflush();
extern int linux_break();
extern int linux_brk();
extern int chdir();
extern int chmod();
extern int chown();
extern int chroot();
extern int linux_clone();
extern int close();
extern int linux_creat();
extern int linux_create_module();
extern int linux_delete_module();
extern int dup();
extern int dup2();
extern int execve();
extern int exit();
extern int fchdir();
extern int fchmod();
extern int fchown();
extern int linux_fcntl();
extern int linux_fork();
extern int linux_fstat();
extern int linux_fstatfs();
extern int fsync();
extern int linux_ftime();
extern int oftruncate();
extern int linux_get_kernel_syms();
extern int getegid();
extern int geteuid();
extern int getgid();
extern int getgroups();
extern int getitimer();
extern int linux_getpgid();
extern int getpgrp();
extern int getpid();
extern int getppid();
extern int getpriority();
extern int ogetrlimit();
extern int getrusage();
extern int gettimeofday();
extern int getuid();
extern int linux_gtty();
extern int linux_idle();
extern int linux_init_module();
extern int linux_ioctl();
extern int linux_ioperm();
extern int linux_iopl();
extern int linux_ipc();
extern int linux_kill();
extern int link();
extern int linux_lock();
extern int linux_lseek();
extern int ostat();
extern int mkdir();
extern int mknod();
extern int linux_mmap();
extern int linux_modify_ldt();
extern int linux_mount();
extern int mprotect();
extern int linux_mpx();
extern int munmap();
extern int linux_newfstat();
extern int linux_newlstat();
extern int linux_newstat();
extern int linux_newuname();
extern int linux_nice();
extern int linux_olduname();
extern int linux_open();
extern int linux_pause();
extern int linux_phys();
extern int linux_pipe();
extern int linux_prof();
extern int profil();
extern int linux_ptrace();
extern int linux_quotactl();
extern int read();
extern int linux_readdir();
extern int readlink();
extern int reboot();
extern int rename();
extern int rmdir();
extern int linux_select();
extern int setdomainname();
extern int setgid();
extern int setgroups();
extern int osethostname();
extern int setitimer();
extern int setpgid();
extern int setpriority();
extern int setregid();
extern int setreuid();
extern int osetrlimit();
extern int setsid();
extern int settimeofday();
extern int setuid();
extern int sigreturn();
extern int linux_setup();
extern int linux_sigaction();
extern int linux_siggetmask();
extern int linux_signal();
extern int linux_sigpending();
extern int linux_sigprocmask();
extern int linux_sigreturn();
extern int linux_sigsetmask();
extern int linux_sigsuspend();
extern int linux_socketcall();
extern int linux_stat();
extern int linux_statfs();
extern int linux_stime();
extern int linux_stty();
extern int linux_swapoff();
extern int swapon();
extern int symlink();
extern int sync();
extern int linux_sysinfo();
extern int linux_syslog();
extern int linux_time();
extern int linux_times();
extern int otruncate();
extern int linux_ulimit();
extern int umask();
extern int linux_umount();
extern int linux_uname();
extern int unlink();
extern int linux_uselib();
extern int linux_ustat();
extern int linux_utime();
extern int linux_vhangup();
extern int linux_vm86();
extern int linux_wait4();
extern int linux_waitpid();
extern int write();

static struct sysent linux_sysent[] = {
    0, linux_setup,		/* 0 */
    1, exit,			/* 1 */
    0, linux_fork,		/* 2 */
    3, read,			/* 3 */
    3, write,			/* 4 */
    3, linux_open,		/* 5 */
    1, close,			/* 6 */
    3, linux_waitpid,		/* 7 */
    2, linux_creat,		/* 8 */
    2, link,			/* 9 */
    1, unlink,	    		/* 10 */
    3, execve,	    		/* 11 */
    1, chdir,			/* 12 */
    1, linux_time,		/* 13 */
    3, mknod,			/* 14 */
    2, chmod,			/* 15 */
    3, chown,			/* 16 */
    1, linux_break,		/* 17 */
    2, linux_stat,		/* 18 */
    3, linux_lseek,		/* 19 */
    0, getpid,	    		/* 20 */
    5, linux_mount,		/* 21 */
    1, linux_umount,		/* 22 */
    1, setuid,	    		/* 23 */
    0, getuid,	    		/* 24 */
    1, linux_stime,		/* 25 */
    4, linux_ptrace,		/* 26 */
    1, linux_alarm,		/* 27 */
    2, linux_fstat,		/* 28 */
    0, linux_pause,		/* 29 */
    2, linux_utime,		/* 30 */
    0, linux_stty,		/* 31 */
    0, linux_gtty,		/* 32 */
    2, access,	    		/* 33 */
    1, linux_nice,		/* 34 */
    0, linux_ftime,		/* 35 */
    0, sync,			/* 36 */
    2, linux_kill,		/* 37 */
    2, rename,	    		/* 38 */
    2, mkdir,			/* 39 */
    1, rmdir,			/* 40 */
    1, dup,	    		/* 41 */
    1, linux_pipe,		/* 42 */
    1, linux_times,		/* 43 */
    0, linux_prof,		/* 44 */
    1, linux_brk,		/* 45 */
    1, setgid,	    		/* 46 */
    0, getgid,	    		/* 47 */
    2, linux_signal,		/* 48 */
    0, geteuid,	    		/* 49 */
    0, getegid,	    		/* 50 */
    0, acct,			/* 51 */
    0, linux_phys,		/* 52 */
    0, linux_lock,		/* 53 */
    3, linux_ioctl,		/* 54 */
    3, linux_fcntl,		/* 55 */
    0, linux_mpx,		/* 56 */
    2, setpgid,	    		/* 57 */
    0, linux_ulimit,		/* 58 */
    1, linux_olduname,		/* 59 */
    1, umask,			/* 60 */
    1, chroot,	    		/* 61 */
    2, linux_ustat,		/* 62 */
    2, dup2,			/* 63 */
    0, getppid,	    		/* 64 */
    0, getpgrp,	    		/* 65 */
    0, setsid,	    		/* 66 */
    3, linux_sigaction,		/* 67 */
    0, linux_siggetmask,	/* 68 */
    1, linux_sigsetmask,	/* 69 */
    2, setreuid,		/* 70 */
    2, setregid,		/* 71 */
    1, linux_sigsuspend,	/* 72 */
    1, linux_sigpending,	/* 73 */
    2, osethostname,		/* 74 */
    2, osetrlimit,		/* 75 */
    2, ogetrlimit,		/* 76 */
    2, getrusage,		/* 77 */
    2, gettimeofday,		/* 78 */
    2, settimeofday,		/* 79 */
    2, getgroups,		/* 80 */
    2, setgroups,		/* 81 */
    1, linux_select,		/* 82 */
    2, symlink,	    		/* 83 */
    2, ostat,			/* 84 */
    3, readlink,		/* 85 */
    1, linux_uselib,		/* 86 */
    1, swapon,	    		/* 87 */
    3, reboot,	    		/* 88 */
    3, linux_readdir,		/* 89 */
    1, linux_mmap,		/* 90 */
    2, munmap,	    		/* 91 */
    2, otruncate,		/* 92 */
    2, oftruncate,		/* 93 */
    2, fchmod,	    		/* 94 */
    3, fchown,	    		/* 95 */
    2, getpriority,		/* 96 */
    3, setpriority,		/* 97 */
    0, profil,	    		/* 98 */
    2, linux_statfs,		/* 99 */
    2, linux_fstatfs,		/* 100 */
    3, linux_ioperm,		/* 101 */
    2, linux_socketcall,	/* 102 */
    3, linux_syslog,		/* 103 */
    3, setitimer,		/* 104 */
    2, getitimer,		/* 105 */
    2, linux_newstat,		/* 106 */
    2, linux_newlstat,		/* 107 */
    2, linux_newfstat,		/* 108 */
    2, linux_uname,		/* 109 */
    1, linux_iopl,		/* 110 */
    0, linux_vhangup,		/* 111 */
    0, linux_idle,		/* 112 */
    1, linux_vm86,		/* 113 */
    4, linux_wait4,		/* 114 */
    1, linux_swapoff,		/* 115 */
    1, linux_sysinfo,		/* 116 */
    4, linux_ipc,		/* 117 */
    1, fsync,			/* 118 */
    1, linux_sigreturn,		/* 119 */
    0, linux_clone,		/* 120 */
    2, setdomainname,		/* 121 */
    1, linux_newuname,		/* 122 */
    3, linux_modify_ldt,	/* 123 */
    1, linux_adjtimex,		/* 124 */
    3, mprotect,		/* 125 */
    3, linux_sigprocmask,	/* 126 */
    2, linux_create_module,	/* 127 */
    4, linux_init_module,	/* 128 */
    1, linux_delete_module,	/* 129 */
    1, linux_get_kernel_syms,	/* 130 */
    0, linux_quotactl,		/* 131 */
    1, linux_getpgid,		/* 132 */
    1, fchdir,	    		/* 133 */
    0, linux_bdflush,		/* 134 */
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
