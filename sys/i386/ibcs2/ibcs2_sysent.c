/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
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
 *	$Id: ibcs2_sysent.c,v 1.1 1994/10/14 08:53:10 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/sysent.h>

#define NERR	80	/* XXX must match sys/errno.h */

/* errno conversion tables */
int bsd_to_svr3_errno[NERR] = {
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	 10, 45, 12, 13, 14, 15, 16, 17, 18, 19,
	 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	 30, 31, 32, 33, 34, 11, 91, 92, 93, 94,
	 95, 96,118, 97, 98, 99,100,101,102,103,
	104,105,106,107,108, 63,110,111,112,113,
	114,115, 31, 78,116,117,145, 11, 11, 11,
	  0, 66,  0,  0,  0,  0,  0, 46, 89,  0,
};

/* function defines */
int	ibcs2_access();
int	ibcs2_advfs();
int	ibcs2_alarm();
int	ibcs2_break();
int	ibcs2_chdir();
int	ibcs2_chmod();
int	ibcs2_chown();
int	ibcs2_chroot();
int	ibcs2_cisc();
int	ibcs2_clocal();
int	ibcs2_close();
int	ibcs2_creat();
int	ibcs2_cxenix();
int	ibcs2_dup();
int	ibcs2_exec();
int	ibcs2_exece();
int	ibcs2_exit();
int	ibcs2_fcntl();
int	ibcs2_fork();
int	ibcs2_fstat();
int	ibcs2_fstatfs();
int	ibcs2_fsync();
int	ibcs2_getdents();
int	ibcs2_getgid();
int	ibcs2_getmsg();
int	ibcs2_getpid();
int	ibcs2_getuid();
int	ibcs2_gtime();
int	ibcs2_gtty();
int	ibcs2_ioctl();
int	ibcs2_kill();
int	ibcs2_libattach();
int	ibcs2_libdetach();
int	ibcs2_link();
int	ibcs2_lstat();
int	ibcs2_mkdir();
int	ibcs2_mknod();
int	ibcs2_msgsys();
int	ibcs2_nice();
int	ibcs2_nosys();
int	ibcs2_open();
int	ibcs2_pause();
int	ibcs2_pipe();
int	ibcs2_plock();
int	ibcs2_poll();
int	ibcs2_procids();
int	ibcs2_profil();
int	ibcs2_ptrace();
int	ibcs2_putmsg();
int	ibcs2_read();
int	ibcs2_readlink();
int	ibcs2_rfdebug();
int	ibcs2_rfstart();
int	ibcs2_rfstop();
int	ibcs2_rfsys();
int	ibcs2_rmdir();
int	ibcs2_rmount();
int	ibcs2_rumount();
int	ibcs2_secure();
int	ibcs2_seek();
int	ibcs2_semsys();
int	ibcs2_setgid();
int	ibcs2_setuid();
int	ibcs2_shmsys();
int	ibcs2_sigsys();
int	ibcs2_smount();
int	ibcs2_stat();
int	ibcs2_statfs();
int	ibcs2_stime();
int	ibcs2_stty();
int	ibcs2_sumount();
int	ibcs2_symlink();
int	ibcs2_sync();
int	ibcs2_sysacct();
int	ibcs2_sysfs();
int	ibcs2_sysi86();
int	ibcs2_times();
int	ibcs2_uadmin();
int	ibcs2_ulimit();
int	ibcs2_umask();
int	ibcs2_unadvfs();
int	ibcs2_unlink();
int	ibcs2_utime();
int	ibcs2_utssys();
int	ibcs2_wait();
int	ibcs2_write();
int	ibcs2_traceemu();		/* XXX */
int	sigreturn();			/* XXX */

/* ibcs2 svr3 sysent table */
struct sysent svr3_sysent[] =
{
	0, ibcs2_nosys,			/*  0 = indir */
	1, ibcs2_exit,			/*  1 = exit */
	0, ibcs2_fork,			/*  2 = fork */
	3, ibcs2_read,			/*  3 = read */
	3, ibcs2_write,			/*  4 = write */
	3, ibcs2_open,			/*  5 = open */
	1, ibcs2_close,			/*  6 = close */
	3, ibcs2_wait,			/*  7 = wait */
	2, ibcs2_creat,			/*  8 = creat */
	2, ibcs2_link,			/*  9 = link */
	1, ibcs2_unlink,		/* 10 = unlink */
	2, ibcs2_exec,			/* 11 = exec */
	1, ibcs2_chdir,			/* 12 = chdir */
	0, ibcs2_gtime,			/* 13 = time */
	3, ibcs2_mknod,			/* 14 = mknod */
	2, ibcs2_chmod,			/* 15 = chmod */
	3, ibcs2_chown,			/* 16 = chown */
	1, ibcs2_break,			/* 17 = break */
	2, ibcs2_stat,			/* 18 = stat */
	3, ibcs2_seek,			/* 19 = seek */
	0, ibcs2_getpid,		/* 20 = getpid */
	6, ibcs2_smount,		/* 21 = mount */
	1, ibcs2_sumount,		/* 22 = umount */
	1, ibcs2_setuid,		/* 23 = setuid */
	0, ibcs2_getuid,		/* 24 = getuid */
	1, ibcs2_stime,			/* 25 = stime */
	4, ibcs2_ptrace,		/* 26 = ptrace */
	1, ibcs2_alarm,			/* 27 = alarm */
	2, ibcs2_fstat,			/* 28 = fstat */
	0, ibcs2_pause,			/* 29 = pause */
	2, ibcs2_utime,			/* 30 = utime */
	2, ibcs2_stty,			/* 31 = stty */
	2, ibcs2_gtty,			/* 32 = gtty */
	2, ibcs2_access,		/* 33 = access */
	1, ibcs2_nice,			/* 34 = nice */
	4, ibcs2_statfs,		/* 35 = statfs */
	0, ibcs2_sync,			/* 36 = sync */
	2, ibcs2_kill,			/* 37 = kill */
	4, ibcs2_fstatfs,		/* 38 = fstatfs */
	1, ibcs2_procids,		/* 39 = procids */
	5, ibcs2_cxenix,		/* 40 = XENIX special system call */
	1, ibcs2_dup,			/* 41 = dup */
	1, ibcs2_pipe,			/* 42 = pipe */
	1, ibcs2_times,			/* 43 = times */
	4, ibcs2_profil,		/* 44 = prof */
	1, ibcs2_plock,			/* 45 = proc lock */
	1, ibcs2_setgid,		/* 46 = setgid */
	0, ibcs2_getgid,		/* 47 = getgid */
	2, ibcs2_sigsys,		/* 48 = signal */
	6, ibcs2_msgsys,		/* 49 = IPC message */
	4, ibcs2_sysi86,               	/* 50 = i386-specific system call */
	1, ibcs2_sysacct,		/* 51 = turn acct off/on */
	4, ibcs2_shmsys,               	/* 52 = shared memory */
	5, ibcs2_semsys,		/* 53 = IPC semaphores */
	3, ibcs2_ioctl,			/* 54 = ioctl */
	3, ibcs2_uadmin,		/* 55 = uadmin */
	0, ibcs2_nosys,			/* 56 = reserved for exch */
	3, ibcs2_utssys,		/* 57 = utssys */
	1, ibcs2_fsync,			/* 58 = fsync */
	3, ibcs2_exece,			/* 59 = exece */
	1, ibcs2_umask,			/* 60 = umask */
	1, ibcs2_chroot,		/* 61 = chroot */
	3, ibcs2_fcntl,			/* 62 = fcntl */
	2, ibcs2_ulimit,		/* 63 = ulimit */
	0, ibcs2_nosys,			/* 64 = nosys */
	0, ibcs2_nosys,			/* 65 = nosys */
	0, ibcs2_nosys,			/* 66 = nosys */
	0, ibcs2_nosys,			/* 67 = file locking call */
	0, ibcs2_nosys,			/* 68 = local system calls */
	0, ibcs2_nosys,			/* 69 = inode open */
	4, ibcs2_advfs,			/* 70 = advfs */
	1, ibcs2_unadvfs,		/* 71 = unadvfs */
	4, ibcs2_rmount,		/* 72 = rmount */
	1, ibcs2_rumount,		/* 73 = rumount */
	5, ibcs2_rfstart,		/* 74 = rfstart */
	0, ibcs2_nosys,			/* 75 = not used */
	1, ibcs2_rfdebug, 	 	/* 76 = rfdebug */
	0, ibcs2_rfstop,	  	/* 77 = rfstop */
	6, ibcs2_rfsys,			/* 78 = rfsys */
	1, ibcs2_rmdir,			/* 79 = rmdir */
	2, ibcs2_mkdir,			/* 80 = mkdir */
	4, ibcs2_getdents,		/* 81 = getdents */
	3, ibcs2_libattach,		/* 82 = libattach */
	1, ibcs2_libdetach,		/* 83 = libdetach */
	3, ibcs2_sysfs,			/* 84 = sysfs */
	4, ibcs2_getmsg,		/* 85 = getmsg */
	4, ibcs2_putmsg,		/* 86 = putmsg */
	3, ibcs2_poll,			/* 87 = poll */
	0, ibcs2_nosys,			/* 88 = not used */
	6, ibcs2_secure,		/* 89 = secureware */
	2, ibcs2_symlink,		/* 90 = symlink */
	2, ibcs2_lstat,			/* 91 = lstat */
	3, ibcs2_readlink,		/* 92 = readlink */
	0, ibcs2_nosys,			/* 93 = not used */
	0, ibcs2_nosys,			/* 94 = not used */
	0, ibcs2_nosys,			/* 95 = not used */
	0, ibcs2_nosys,			/* 96 = not used */
	0, ibcs2_nosys,			/* 97 = not used */
	0, ibcs2_nosys,			/* 98 = not used */
	0, ibcs2_nosys,			/* 99 = not used */
	0, ibcs2_nosys,			/* 100 = not used */
	0, ibcs2_nosys,			/* 101 = not used */
	0, ibcs2_nosys,			/* 102 = not used */
	1, sigreturn,			/* 103 = BSD sigreturn XXX */
	0, ibcs2_nosys,			/* 104 = not used */
	5, ibcs2_cisc,			/* 105 = ISC special */
	0, ibcs2_nosys,			/* 106 = not used */
	0, ibcs2_nosys,			/* 107 = not used */
	0, ibcs2_nosys,			/* 108 = not used */
	0, ibcs2_nosys,			/* 109 = not used */
	0, ibcs2_nosys,			/* 110 = not used */
	0, ibcs2_nosys,			/* 111 = not used */
	0, ibcs2_nosys,			/* 112 = not used */
	0, ibcs2_nosys,			/* 113 = not used */
	0, ibcs2_nosys,			/* 114 = not used */
	0, ibcs2_nosys,			/* 115 = not used */
	0, ibcs2_nosys,			/* 116 = not used */
	0, ibcs2_nosys,			/* 117 = not used */
	0, ibcs2_nosys,			/* 118 = not used */
	0, ibcs2_nosys,			/* 119 = not used */
	0, ibcs2_nosys,			/* 120 = not used */
	0, ibcs2_nosys,			/* 121 = not used */
	0, ibcs2_nosys,			/* 122 = not used */
	0, ibcs2_nosys,			/* 123 = not used */
	0, ibcs2_nosys,			/* 124 = not used */
	0, ibcs2_nosys,			/* 125 = not used */
	1, ibcs2_traceemu,		/* 126 = ibcs2 emulator trace cntl */
	5, ibcs2_clocal,		/* 127 = local system calls */
};

struct sysentvec ibcs2_svr3_sysvec = {
	sizeof (svr3_sysent) / sizeof (svr3_sysent[0]),
	svr3_sysent,
	0x7F,
	NSIG,
	bsd_to_ibcs2_signal,
	NERR,
	bsd_to_svr3_errno
};

#if 0

int	ibcs2_acancel();
int	ibcs2_adjtime();
int	ibcs2_context();
int	ibcs2_evsys();
int	ibcs2_evtrapret();
int	ibcs2_fchdir();
int	ibcs2_fchmod();
int	ibcs2_fchown();
int	ibcs2_fstatvfs();
int	ibcs2_fxstat();
int	ibcs2_getgroups();
int	ibcs2_getpmsg();
int	ibcs2_getrlimit();
int	ibcs2_hrtsys();
int	ibcs2_lchown();
int	ibcs2_lxstat();
int	ibcs2_memcntl();
int	ibcs2_mincore();
int	ibcs2_mmap();
int	ibcs2_mprotect();
int	ibcs2_munmap();
int	ibcs2_pathconf();
int	ibcs2_priocntlsys();
int	ibcs2_putgmsg();
int	ibcs2_readv();
int	ibcs2_rename();
int	ibcs2_setegid();
int	ibcs2_seteuid();
int	ibcs2_setgroups();
int	ibcs2_setrlimit();
int	ibcs2_sigaction();
int	ibcs2_sigaltstack();
int	ibcs2_sigpending();
int	ibcs2_sigprocmask();
int	ibcs2_sigsendsys();
int	ibcs2_sigsuspend();
int	ibcs2_statvfs();
int	ibcs2_sysconfig();
int	ibcs2_systeminfo();
int	ibcs2_vfork();
int	ibcs2_waitsys();
int	ibcs2_writev();
int	ibcs2_xmknod();
int	ibcs2_xstat();

int bsd_to_svr4_errno[NERR] = {
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	 10, 45, 12, 13, 14, 15, 16, 17, 18, 19,
	 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	 30, 31, 32, 33, 34, 11,150,149, 95, 96,
	 97, 98, 99,120,121,122,123,124,125,126,
	127,128,129,130,131,132,133,134,143,144,
	145,146, 90, 78,147,148, 93, 11, 94, 11,
	  0,  0,  0,  0,  0,  0,  0, 46, 89,  0,
};

/* ibcs2 svr4 sysent table */
struct sysent svr4_sysent[] =
{
	0, ibcs2_nosys,			/*  0 = indir */
	1, ibcs2_exit,			/*  1 = exit */
	0, ibcs2_fork,			/*  2 = fork */
	3, ibcs2_read,			/*  3 = read */
	3, ibcs2_write,			/*  4 = write */
	3, ibcs2_open,			/*  5 = open */
	1, ibcs2_close,			/*  6 = close */
	3, ibcs2_wait,			/*  7 = wait */
	2, ibcs2_creat,			/*  8 = creat */
	2, ibcs2_link,			/*  9 = link */
	1, ibcs2_unlink,		/* 10 = unlink */
	2, ibcs2_exec,			/* 11 = exec */
	1, ibcs2_chdir,			/* 12 = chdir */
	0, ibcs2_gtime,			/* 13 = time */
	3, ibcs2_mknod,			/* 14 = mknod */
	2, ibcs2_chmod,			/* 15 = chmod */
	3, ibcs2_chown,			/* 16 = chown */
	1, ibcs2_break,			/* 17 = break */
	2, ibcs2_stat,			/* 18 = stat */
	3, ibcs2_seek,			/* 19 = seek */
	0, ibcs2_getpid,		/* 20 = getpid */
	6, ibcs2_smount,		/* 21 = mount */
	1, ibcs2_sumount,		/* 22 = umount */
	1, ibcs2_setuid,		/* 23 = setuid */
	0, ibcs2_getuid,		/* 24 = getuid */
	1, ibcs2_stime,			/* 25 = stime */
	4, ibcs2_ptrace,		/* 26 = ptrace */
	1, ibcs2_alarm,			/* 27 = alarm */
	2, ibcs2_fstat,			/* 28 = fstat */
	0, ibcs2_pause,			/* 29 = pause */
	2, ibcs2_utime,			/* 30 = utime */
	2, ibcs2_stty,			/* 31 = stty */
	2, ibcs2_gtty,			/* 32 = gtty */
	2, ibcs2_access,		/* 33 = access */
	1, ibcs2_nice,			/* 34 = nice */
	4, ibcs2_statfs,		/* 35 = statfs */
	0, ibcs2_sync,			/* 36 = sync */
	2, ibcs2_kill,			/* 37 = kill */
	4, ibcs2_fstatfs,		/* 38 = fstatfs */
	1, ibcs2_procids,		/* 39 = procids */
	5, ibcs2_cxenix,		/* 40 = XENIX special system call */
	1, ibcs2_dup,			/* 41 = dup */
	1, ibcs2_pipe,			/* 42 = pipe */
	1, ibcs2_times,			/* 43 = times */
	4, ibcs2_profil,		/* 44 = prof */
	1, ibcs2_plock,			/* 45 = proc lock */
	1, ibcs2_setgid,		/* 46 = setgid */
	0, ibcs2_getgid,		/* 47 = getgid */
	2, ibcs2_sigsys,		/* 48 = signal */
	6, ibcs2_msgsys,		/* 49 = IPC message */
	4, ibcs2_sysi86,               	/* 50 = i386-specific system call */
	1, ibcs2_sysacct,		/* 51 = turn acct off/on */
	4, ibcs2_shmsys,               	/* 52 = shared memory */
	5, ibcs2_semsys,		/* 53 = IPC semaphores */
	3, ibcs2_ioctl,			/* 54 = ioctl */
	3, ibcs2_uadmin,		/* 55 = uadmin */
	0, ibcs2_nosys,			/* 56 = reserved for exch */
	3, ibcs2_utssys,		/* 57 = utssys */
	1, ibcs2_fsync,			/* 58 = fsync */
	3, ibcs2_exece,			/* 59 = exece */
	1, ibcs2_umask,			/* 60 = umask */
	1, ibcs2_chroot,		/* 61 = chroot */
	3, ibcs2_fcntl,			/* 62 = fcntl */
	2, ibcs2_ulimit,		/* 63 = ulimit */
	0, ibcs2_nosys,			/* 64 = nosys */
	0, ibcs2_nosys,			/* 65 = nosys */
	0, ibcs2_nosys,			/* 66 = nosys */
	0, ibcs2_nosys,			/* 67 = file locking call */
	0, ibcs2_nosys,			/* 68 = local system calls */
	0, ibcs2_nosys,			/* 69 = inode open */
	4, ibcs2_advfs,			/* 70 = advfs */
	1, ibcs2_unadvfs,		/* 71 = unadvfs */
	4, ibcs2_rmount,		/* 72 = rmount */
	1, ibcs2_rumount,		/* 73 = rumount */
	5, ibcs2_rfstart,		/* 74 = rfstart */
	0, ibcs2_nosys,			/* 75 = not used */
	1, ibcs2_rfdebug, 	 	/* 76 = rfdebug */
	0, ibcs2_rfstop,	  	/* 77 = rfstop */
	6, ibcs2_rfsys,			/* 78 = rfsys */
	1, ibcs2_rmdir,			/* 79 = rmdir */
	2, ibcs2_mkdir,			/* 80 = mkdir */
	4, ibcs2_getdents,		/* 81 = getdents */
	3, ibcs2_libattach,		/* 82 = libattach */
	1, ibcs2_libdetach,		/* 83 = libdetach */
	3, ibcs2_sysfs,			/* 84 = sysfs */
	4, ibcs2_getmsg,		/* 85 = getmsg */
	4, ibcs2_putmsg,		/* 86 = putmsg */
	3, ibcs2_poll,			/* 87 = poll */
	6, ibcs2_lstat,			/* 88 = lstat */
	2, ibcs2_symlink,		/* 89 = symlink */
	3, ibcs2_readlink,		/* 90 = readlink */
	2, ibcs2_setgroups,		/* 91 = setgroups */
	2, ibcs2_getgroups,		/* 92 = getgroups */
	2, ibcs2_fchmod,		/* 93 = fchmod */
	3, ibcs2_fchown,		/* 94 = fchown */
	3, ibcs2_sigprocmask,		/* 95 = sigprocmask */
	0, ibcs2_sigsuspend,		/* 96 = sigsuspend */
	2, ibcs2_sigaltstack,		/* 97 = sigaltstack */
	3, ibcs2_sigaction,		/* 98 = sigaction */
	1, ibcs2_sigpending,		/* 99 = sigpending */
	0, ibcs2_context,		/* 100 = context */
	0, ibcs2_evsys,			/* 101 = evsys */
	0, ibcs2_evtrapret,		/* 102 = evtrapret */
	0, ibcs2_statvfs,		/* 103 = statvfs */
	0, ibcs2_fstatvfs,		/* 104 = fstatvfs */
	5, ibcs2_cisc,			/* 105 = ISC special */
	0, ibcs2_nfssys,		/* 106 = nfssys */
	0, ibcs2_waitsys,		/* 107 = waitsys */
	0, ibcs2_sigsendsys,		/* 108 = sigsendsys */
	0, ibcs2_hrtsys,		/* 109 = hrtsys */
	0, ibcs2_acancel,		/* 110 = acancel */
	0, ibcs2_async,			/* 111 = async */
	0, ibcs2_priocntlsys,		/* 112 = priocntlsys */
	0, ibcs2_pathconf,		/* 113 = pathconf */
	0, ibcs2_mincore,		/* 114 = mincore */
	6, ibcs2_mmap,			/* 115 = mmap */
	3, ibcs2_mprotect,		/* 116 = mprotect */
	2, ibcs2_munmap,		/* 117 = munmap */
	0, ibcs2_pathconf,		/* 118 = fpathconf */
	0, ibcs2_vfork,			/* 119 = vfork */
	0, ibcs2_fchdir,		/* 120 = fchdir */
	0, ibcs2_readv,			/* 121 = readv */
	0, ibcs2_writev,		/* 122 = writev */
	3, ibcs2_xstat,			/* 123 = xstat */
	3, ibcs2_lxstat,		/* 124 = lxstat */
	3, ibcs2_fxstat,		/* 125 = fxstat */
	4, ibcs2_xmknod,		/* 126 = xmknod */
	5, ibcs2_clocal,		/* 127 = local system calls */
	0, ibcs2_setrlimit,		/* 128 = setrlimit */
	0, ibcs2_getrlimit,		/* 129 = getrlimit */
	0, ibcs2_lchown,		/* 130 = lchown */
	0, ibcs2_memcntl,		/* 131 = memcntl */
	0, ibcs2_getpmsg,		/* 132 = getpmsg */
	0, ibcs2_putgmsg,		/* 133 = putgmsg */
	2, ibcs2_rename,		/* 134 = rename */
	1, ibcs2_uname,			/* 135 = uname */
	0, ibcs2_setegid,		/* 136 = setegid */
	0, ibcs2_sysconfig,		/* 137 = sysconfig */
	0, ibcs2_adjtime,		/* 138 = adjtime */
	0, ibcs2_systeminfo,		/* 139 = systeminfo */
	0, ibcs2_nosys,			/* 140 = not used */
	0, ibcs2_seteuid,		/* 141 = seteuid */
};

struct sysentvec ibcs2_svr4_sysvec = {
	sizeof (svr4_sysent) / sizeof (svr4_sysent[0]),
	svr4_sysent,
	0xFF,
	NSIG,
	bsd_to_ibcs2_signal,
	NERR,
	bsd_to_svr4_errno
};

#endif
