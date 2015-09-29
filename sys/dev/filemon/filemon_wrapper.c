/*-
 * Copyright (c) 2011, David E. O'Brien.
 * Copyright (c) 2009-2011, Juniper Networks, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY JUNIPER NETWORKS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL JUNIPER NETWORKS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/sx.h>

#include "opt_compat.h"

#if __FreeBSD_version > 800032
#define FILEMON_HAS_LINKAT
#endif

#if __FreeBSD_version < 900044	/* r225617 (2011-09-16) failed to bump
				   __FreeBSD_version.  This really should
				   be based on "900045".  "900044" is r225469
				   (2011-09-10) so this code is broken for
				   9-CURRENT September 10th-16th. */
#define sys_chdir	chdir
#define sys_execve	execve
#define sys_fork	fork
#define sys_link	link
#define sys_open	open
#define sys_rename	rename
#define sys_stat	stat
#define sys_symlink	symlink
#define sys_unlink	unlink
#define sys_vfork	vfork
#define sys_sys_exit	sys_exit
#ifdef FILEMON_HAS_LINKAT
#define sys_linkat	linkat
#endif
#endif	/* __FreeBSD_version */

static void
filemon_output(struct filemon *filemon, char *msg, size_t len)
{
	struct uio auio;
	struct iovec aiov;

	if (filemon->fp == NULL)
		return;

	aiov.iov_base = msg;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = curthread;
	auio.uio_offset = (off_t) -1;

	bwillwrite();

	fo_write(filemon->fp, &auio, curthread->td_ucred, 0, curthread);
}

static struct filemon *
filemon_pid_check(struct proc *p)
{
	struct filemon *filemon;

	sx_slock(&proctree_lock);
	while (p != initproc) {
		TAILQ_FOREACH(filemon, &filemons_inuse, link) {
			if (p->p_pid == filemon->pid) {
				sx_sunlock(&proctree_lock);
				return (filemon);
			}
		}
		p = proc_realparent(p);
	}
	sx_sunlock(&proctree_lock);
	return (NULL);
}

static void
filemon_comment(struct filemon *filemon)
{
	int len;
	struct timeval now;

	/* Load timestamp before locking.  Less accurate but less contention. */
	getmicrotime(&now);

	/* Grab a read lock on the filemon inuse list. */
	filemon_lock_read();

	/* Lock the found filemon structure. */
	filemon_filemon_lock(filemon);

	len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr),
	    "# filemon version %d\n# Target pid %d\n# Start %ju.%06ju\nV %d\n",
	    FILEMON_VERSION, curproc->p_pid, (uintmax_t)now.tv_sec,
	    (uintmax_t)now.tv_usec, FILEMON_VERSION);

	filemon_output(filemon, filemon->msgbufr, len);

	/* Unlock the found filemon structure. */
	filemon_filemon_unlock(filemon);

	/* Release the read lock. */
	filemon_unlock_read();
}

static int
filemon_wrapper_chdir(struct thread *td, struct chdir_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_chdir(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "C %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_execve(struct thread *td, struct execve_args *uap)
{
	char fname[MAXPATHLEN];
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	copyinstr(uap->fname, fname, sizeof(fname), &done);

	if ((ret = sys_execve(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "E %d %s\n",
			    curproc->p_pid, fname);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
static int
filemon_wrapper_freebsd32_execve(struct thread *td,
    struct freebsd32_execve_args *uap)
{
	char fname[MAXPATHLEN];
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	copyinstr(uap->fname, fname, sizeof(fname), &done);

	if ((ret = freebsd32_execve(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "E %d %s\n",
			    curproc->p_pid, fname);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}
#endif

static int
filemon_wrapper_fork(struct thread *td, struct fork_args *uap)
{
	int ret;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_fork(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "F %d %ld\n",
			    curproc->p_pid, (long)curthread->td_retval[0]);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_open(struct thread *td, struct open_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_open(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			if (uap->flags & O_RDWR) {
				/*
				 * We'll get the W record below, but need
				 * to also output an R to distingish from
				 * O_WRONLY.
				 */
				len = snprintf(filemon->msgbufr,
				    sizeof(filemon->msgbufr), "R %d %s\n",
				    curproc->p_pid, filemon->fname1);
				filemon_output(filemon, filemon->msgbufr, len);
			}


			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "%c %d %s\n",
			    (uap->flags & O_ACCMODE) ? 'W':'R',
			    curproc->p_pid, filemon->fname1);
			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_openat(struct thread *td, struct openat_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_openat(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			filemon->fname2[0] = '\0';
			if (filemon->fname1[0] != '/' && uap->fd != AT_FDCWD) {
				/*
				 * rats - we cannot do too much about this.
				 * the trace should show a dir we read
				 * recently.. output an A record as a clue
				 * until we can do better.
				 */
				len = snprintf(filemon->msgbufr,
				    sizeof(filemon->msgbufr), "A %d %s\n",
				    curproc->p_pid, filemon->fname1);
				filemon_output(filemon, filemon->msgbufr, len);
			}
			if (uap->flag & O_RDWR) {
				/*
				 * We'll get the W record below, but need
				 * to also output an R to distingish from
				 * O_WRONLY.
				 */
				len = snprintf(filemon->msgbufr,
				    sizeof(filemon->msgbufr), "R %d %s%s\n",
				    curproc->p_pid, filemon->fname2, filemon->fname1);
				filemon_output(filemon, filemon->msgbufr, len);
			}


			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "%c %d %s%s\n",
			    (uap->flag & O_ACCMODE) ? 'W':'R',
			    curproc->p_pid, filemon->fname2, filemon->fname1);
			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_rename(struct thread *td, struct rename_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_rename(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->from, filemon->fname1,
			    sizeof(filemon->fname1), &done);
			copyinstr(uap->to, filemon->fname2,
			    sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "M %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_link(struct thread *td, struct link_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_link(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);
			copyinstr(uap->link, filemon->fname2,
			    sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "L %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_symlink(struct thread *td, struct symlink_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_symlink(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);
			copyinstr(uap->link, filemon->fname2,
			    sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "L %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

#ifdef FILEMON_HAS_LINKAT
static int
filemon_wrapper_linkat(struct thread *td, struct linkat_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_linkat(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path1, filemon->fname1,
			    sizeof(filemon->fname1), &done);
			copyinstr(uap->path2, filemon->fname2,
			    sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "L %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}
#endif

static int
filemon_wrapper_stat(struct thread *td, struct stat_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_stat(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "S %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
static int
filemon_wrapper_freebsd32_stat(struct thread *td,
    struct freebsd32_stat_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = freebsd32_stat(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "S %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}
#endif

static void
filemon_wrapper_sys_exit(struct thread *td, struct sys_exit_args *uap)
{
	size_t len;
	struct filemon *filemon;
	struct timeval now;

	/* Get timestamp before locking. */
	getmicrotime(&now);

	/* Grab a read lock on the filemon inuse list. */
	filemon_lock_read();

	if ((filemon = filemon_pid_check(curproc)) != NULL) {
		/* Lock the found filemon structure. */
		filemon_filemon_lock(filemon);

		len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr),
		    "X %d %d\n", curproc->p_pid, uap->rval);

		filemon_output(filemon, filemon->msgbufr, len);

		/* Check if the monitored process is about to exit. */
		if (filemon->pid == curproc->p_pid) {
			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr),
			    "# Stop %ju.%06ju\n# Bye bye\n",
			    (uintmax_t)now.tv_sec, (uintmax_t)now.tv_usec);

			filemon_output(filemon, filemon->msgbufr, len);
			filemon->pid = -1;
		}

		/* Unlock the found filemon structure. */
		filemon_filemon_unlock(filemon);
	}

	/* Release the read lock. */
	filemon_unlock_read();

	sys_sys_exit(td, uap);
}

static int
filemon_wrapper_unlink(struct thread *td, struct unlink_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_unlink(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "D %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static int
filemon_wrapper_vfork(struct thread *td, struct vfork_args *uap)
{
	int ret;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_vfork(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "F %d %ld\n",
			    curproc->p_pid, (long)curthread->td_retval[0]);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return (ret);
}

static void
filemon_wrapper_install(void)
{
#if defined(__i386__)
	struct sysent *sv_table = elf32_freebsd_sysvec.sv_table;
#elif defined(__amd64__)
	struct sysent *sv_table = elf64_freebsd_sysvec.sv_table;
#else
#error Machine type not supported
#endif

	sv_table[SYS_chdir].sy_call = (sy_call_t *) filemon_wrapper_chdir;
	sv_table[SYS_exit].sy_call = (sy_call_t *) filemon_wrapper_sys_exit;
	sv_table[SYS_execve].sy_call = (sy_call_t *) filemon_wrapper_execve;
	sv_table[SYS_fork].sy_call = (sy_call_t *) filemon_wrapper_fork;
	sv_table[SYS_open].sy_call = (sy_call_t *) filemon_wrapper_open;
	sv_table[SYS_openat].sy_call = (sy_call_t *) filemon_wrapper_openat;
	sv_table[SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[SYS_stat].sy_call = (sy_call_t *) filemon_wrapper_stat;
	sv_table[SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[SYS_vfork].sy_call = (sy_call_t *) filemon_wrapper_vfork;
	sv_table[SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[SYS_linkat].sy_call = (sy_call_t *) filemon_wrapper_linkat;
#endif

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
	sv_table = ia32_freebsd_sysvec.sv_table;

	sv_table[FREEBSD32_SYS_chdir].sy_call = (sy_call_t *) filemon_wrapper_chdir;
	sv_table[FREEBSD32_SYS_exit].sy_call = (sy_call_t *) filemon_wrapper_sys_exit;
	sv_table[FREEBSD32_SYS_freebsd32_execve].sy_call = (sy_call_t *) filemon_wrapper_freebsd32_execve;
	sv_table[FREEBSD32_SYS_fork].sy_call = (sy_call_t *) filemon_wrapper_fork;
	sv_table[FREEBSD32_SYS_open].sy_call = (sy_call_t *) filemon_wrapper_open;
	sv_table[FREEBSD32_SYS_openat].sy_call = (sy_call_t *) filemon_wrapper_openat;
	sv_table[FREEBSD32_SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[FREEBSD32_SYS_freebsd32_stat].sy_call = (sy_call_t *) filemon_wrapper_freebsd32_stat;
	sv_table[FREEBSD32_SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[FREEBSD32_SYS_vfork].sy_call = (sy_call_t *) filemon_wrapper_vfork;
	sv_table[FREEBSD32_SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[FREEBSD32_SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[FREEBSD32_SYS_linkat].sy_call = (sy_call_t *) filemon_wrapper_linkat;
#endif
#endif	/* COMPAT_ARCH32 */
}

static void
filemon_wrapper_deinstall(void)
{
#if defined(__i386__)
	struct sysent *sv_table = elf32_freebsd_sysvec.sv_table;
#elif defined(__amd64__)
	struct sysent *sv_table = elf64_freebsd_sysvec.sv_table;
#else
#error Machine type not supported
#endif

	sv_table[SYS_chdir].sy_call = (sy_call_t *)sys_chdir;
	sv_table[SYS_exit].sy_call = (sy_call_t *)sys_sys_exit;
	sv_table[SYS_execve].sy_call = (sy_call_t *)sys_execve;
	sv_table[SYS_fork].sy_call = (sy_call_t *)sys_fork;
	sv_table[SYS_open].sy_call = (sy_call_t *)sys_open;
	sv_table[SYS_openat].sy_call = (sy_call_t *)sys_openat;
	sv_table[SYS_rename].sy_call = (sy_call_t *)sys_rename;
	sv_table[SYS_stat].sy_call = (sy_call_t *)sys_stat;
	sv_table[SYS_unlink].sy_call = (sy_call_t *)sys_unlink;
	sv_table[SYS_vfork].sy_call = (sy_call_t *)sys_vfork;
	sv_table[SYS_link].sy_call = (sy_call_t *)sys_link;
	sv_table[SYS_symlink].sy_call = (sy_call_t *)sys_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[SYS_linkat].sy_call = (sy_call_t *)sys_linkat;
#endif

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
	sv_table = ia32_freebsd_sysvec.sv_table;

	sv_table[FREEBSD32_SYS_chdir].sy_call = (sy_call_t *)sys_chdir;
	sv_table[FREEBSD32_SYS_exit].sy_call = (sy_call_t *)sys_sys_exit;
	sv_table[FREEBSD32_SYS_freebsd32_execve].sy_call = (sy_call_t *)freebsd32_execve;
	sv_table[FREEBSD32_SYS_fork].sy_call = (sy_call_t *)sys_fork;
	sv_table[FREEBSD32_SYS_open].sy_call = (sy_call_t *)sys_open;
	sv_table[FREEBSD32_SYS_openat].sy_call = (sy_call_t *)sys_openat;
	sv_table[FREEBSD32_SYS_rename].sy_call = (sy_call_t *)sys_rename;
	sv_table[FREEBSD32_SYS_freebsd32_stat].sy_call = (sy_call_t *)freebsd32_stat;
	sv_table[FREEBSD32_SYS_unlink].sy_call = (sy_call_t *)sys_unlink;
	sv_table[FREEBSD32_SYS_vfork].sy_call = (sy_call_t *)sys_vfork;
	sv_table[FREEBSD32_SYS_link].sy_call = (sy_call_t *)sys_link;
	sv_table[FREEBSD32_SYS_symlink].sy_call = (sy_call_t *)sys_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[FREEBSD32_SYS_linkat].sy_call = (sy_call_t *)sys_linkat;
#endif
#endif	/* COMPAT_ARCH32 */
}
