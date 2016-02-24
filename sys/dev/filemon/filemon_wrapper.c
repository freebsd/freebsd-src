/*-
 * Copyright (c) 2011, David E. O'Brien.
 * Copyright (c) 2009-2011, Juniper Networks, Inc.
 * Copyright (c) 2015, EMC Corp.
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

#include <sys/imgact.h>
#include <sys/eventhandler.h>
#include <sys/sx.h>
#include <sys/vnode.h>

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
#define sys_link	link
#define sys_open	open
#define sys_rename	rename
#define sys_stat	stat
#define sys_symlink	symlink
#define sys_unlink	unlink
#ifdef FILEMON_HAS_LINKAT
#define sys_linkat	linkat
#endif
#endif	/* __FreeBSD_version */

static eventhandler_tag filemon_exec_tag;
static eventhandler_tag filemon_exit_tag;
static eventhandler_tag filemon_fork_tag;

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

	filemon_lock_read();
	if (TAILQ_EMPTY(&filemons_inuse)) {
		filemon_unlock_read();
		return (NULL);
	}
	sx_slock(&proctree_lock);
	while (p->p_pid != 0) {
		TAILQ_FOREACH(filemon, &filemons_inuse, link) {
			if (p == filemon->p) {
				sx_sunlock(&proctree_lock);
				filemon_filemon_lock(filemon);
				filemon_unlock_read();
				return (filemon);
			}
		}
		p = proc_realparent(p);
	}
	sx_sunlock(&proctree_lock);
	filemon_unlock_read();
	return (NULL);
}

static int
filemon_wrapper_chdir(struct thread *td, struct chdir_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_chdir(td, uap)) == 0) {
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "C %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}
	}

	return (ret);
}

static void
filemon_event_process_exec(void *arg __unused, struct proc *p,
    struct image_params *imgp)
{
	struct filemon *filemon;
	char *fullpath, *freepath;
	size_t len;

	if ((filemon = filemon_pid_check(p)) != NULL) {
		fullpath = "<unknown>";
		freepath = NULL;

		vn_fullpath(FIRST_THREAD_IN_PROC(p), imgp->vp, &fullpath,
		    &freepath);

		len = snprintf(filemon->msgbufr,
		    sizeof(filemon->msgbufr), "E %d %s\n",
		    p->p_pid, fullpath);

		filemon_output(filemon, filemon->msgbufr, len);

		/* Unlock the found filemon structure. */
		filemon_filemon_unlock(filemon);

		free(freepath, M_TEMP);
	}
}

static int
filemon_wrapper_open(struct thread *td, struct open_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_open(td, uap)) == 0) {
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "S %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}
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
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "S %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}
	}

	return (ret);
}
#endif

static void
filemon_event_process_exit(void *arg __unused, struct proc *p)
{
	size_t len;
	struct filemon *filemon;
	struct timeval now;

	/* Get timestamp before locking. */
	getmicrotime(&now);

	if ((filemon = filemon_pid_check(p)) != NULL) {
		len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr),
		    "X %d %d\n", p->p_pid, W_EXITCODE(p->p_xstat, 0));

		filemon_output(filemon, filemon->msgbufr, len);

		/* Check if the monitored process is about to exit. */
		if (filemon->p == p) {
			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr),
			    "# Stop %ju.%06ju\n# Bye bye\n",
			    (uintmax_t)now.tv_sec, (uintmax_t)now.tv_usec);

			filemon_output(filemon, filemon->msgbufr, len);
			filemon->p = NULL;
		}

		/* Unlock the found filemon structure. */
		filemon_filemon_unlock(filemon);
	}
}

static int
filemon_wrapper_unlink(struct thread *td, struct unlink_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_unlink(td, uap)) == 0) {
		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			copyinstr(uap->path, filemon->fname1,
			    sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr,
			    sizeof(filemon->msgbufr), "D %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}
	}

	return (ret);
}

static void
filemon_event_process_fork(void *arg __unused, struct proc *p1,
    struct proc *p2, int flags __unused)
{
	size_t len;
	struct filemon *filemon;

	if ((filemon = filemon_pid_check(p1)) != NULL) {
		len = snprintf(filemon->msgbufr,
		    sizeof(filemon->msgbufr), "F %d %d\n",
		    p1->p_pid, p2->p_pid);

		filemon_output(filemon, filemon->msgbufr, len);

		/* Unlock the found filemon structure. */
		filemon_filemon_unlock(filemon);
	}
}

static void
filemon_wrapper_install(void)
{
#if defined(__LP64__)
	struct sysent *sv_table = elf64_freebsd_sysvec.sv_table;
#else
	struct sysent *sv_table = elf32_freebsd_sysvec.sv_table;
#endif

	sv_table[SYS_chdir].sy_call = (sy_call_t *) filemon_wrapper_chdir;
	sv_table[SYS_open].sy_call = (sy_call_t *) filemon_wrapper_open;
	sv_table[SYS_openat].sy_call = (sy_call_t *) filemon_wrapper_openat;
	sv_table[SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[SYS_stat].sy_call = (sy_call_t *) filemon_wrapper_stat;
	sv_table[SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[SYS_linkat].sy_call = (sy_call_t *) filemon_wrapper_linkat;
#endif

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
	sv_table = ia32_freebsd_sysvec.sv_table;

	sv_table[FREEBSD32_SYS_chdir].sy_call = (sy_call_t *) filemon_wrapper_chdir;
	sv_table[FREEBSD32_SYS_open].sy_call = (sy_call_t *) filemon_wrapper_open;
	sv_table[FREEBSD32_SYS_openat].sy_call = (sy_call_t *) filemon_wrapper_openat;
	sv_table[FREEBSD32_SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[FREEBSD32_SYS_freebsd32_stat].sy_call = (sy_call_t *) filemon_wrapper_freebsd32_stat;
	sv_table[FREEBSD32_SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[FREEBSD32_SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[FREEBSD32_SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[FREEBSD32_SYS_linkat].sy_call = (sy_call_t *) filemon_wrapper_linkat;
#endif
#endif	/* COMPAT_ARCH32 */

	filemon_exec_tag = EVENTHANDLER_REGISTER(process_exec,
	    filemon_event_process_exec, NULL, EVENTHANDLER_PRI_LAST);
	filemon_exit_tag = EVENTHANDLER_REGISTER(process_exit,
	    filemon_event_process_exit, NULL, EVENTHANDLER_PRI_LAST);
	filemon_fork_tag = EVENTHANDLER_REGISTER(process_fork,
	    filemon_event_process_fork, NULL, EVENTHANDLER_PRI_LAST);
}

static void
filemon_wrapper_deinstall(void)
{
#if defined(__LP64__)
	struct sysent *sv_table = elf64_freebsd_sysvec.sv_table;
#else
	struct sysent *sv_table = elf32_freebsd_sysvec.sv_table;
#endif

	sv_table[SYS_chdir].sy_call = (sy_call_t *)sys_chdir;
	sv_table[SYS_open].sy_call = (sy_call_t *)sys_open;
	sv_table[SYS_openat].sy_call = (sy_call_t *)sys_openat;
	sv_table[SYS_rename].sy_call = (sy_call_t *)sys_rename;
	sv_table[SYS_stat].sy_call = (sy_call_t *)sys_stat;
	sv_table[SYS_unlink].sy_call = (sy_call_t *)sys_unlink;
	sv_table[SYS_link].sy_call = (sy_call_t *)sys_link;
	sv_table[SYS_symlink].sy_call = (sy_call_t *)sys_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[SYS_linkat].sy_call = (sy_call_t *)sys_linkat;
#endif

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
	sv_table = ia32_freebsd_sysvec.sv_table;

	sv_table[FREEBSD32_SYS_chdir].sy_call = (sy_call_t *)sys_chdir;
	sv_table[FREEBSD32_SYS_open].sy_call = (sy_call_t *)sys_open;
	sv_table[FREEBSD32_SYS_openat].sy_call = (sy_call_t *)sys_openat;
	sv_table[FREEBSD32_SYS_rename].sy_call = (sy_call_t *)sys_rename;
	sv_table[FREEBSD32_SYS_freebsd32_stat].sy_call = (sy_call_t *)freebsd32_stat;
	sv_table[FREEBSD32_SYS_unlink].sy_call = (sy_call_t *)sys_unlink;
	sv_table[FREEBSD32_SYS_link].sy_call = (sy_call_t *)sys_link;
	sv_table[FREEBSD32_SYS_symlink].sy_call = (sy_call_t *)sys_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[FREEBSD32_SYS_linkat].sy_call = (sy_call_t *)sys_linkat;
#endif
#endif	/* COMPAT_ARCH32 */

	EVENTHANDLER_DEREGISTER(process_exec, filemon_exec_tag);
	EVENTHANDLER_DEREGISTER(process_exit, filemon_exit_tag);
	EVENTHANDLER_DEREGISTER(process_fork, filemon_fork_tag);
}
