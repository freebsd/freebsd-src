/*-
 * Copyright (c) 2009, 2010, Juniper Networks, Inc.
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

__FBSDID("$FreeBSD$");

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

	TAILQ_FOREACH(filemon, &filemons_inuse, link) {
		if (p->p_pid == filemon->pid)
			return(filemon);
	}

	if (p->p_pptr == NULL)
		return(NULL);
	
	return (filemon_pid_check(p->p_pptr));
}

static void
filemon_comment(struct filemon *filemon)
{
	int len;

	/* Grab a read lock on the filemon inuse list. */
	filemon_lock_read();

	/* Lock the found filemon structure. */
	filemon_filemon_lock(filemon);

	len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr),
	    "# buildmon version 2\n# Target pid %d\nV 2\n", curproc->p_pid);

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

	if ((ret = chdir(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "C %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
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

	if ((ret = execve(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "E %d %s\n",
			    curproc->p_pid, fname);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

#ifdef COMPAT_IA32
static int
filemon_wrapper_freebsd32_execve(struct thread *td, struct freebsd32_execve_args *uap)
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

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "E %d %s\n",
			    curproc->p_pid, fname);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}
#endif

static int
filemon_wrapper_fork(struct thread *td, struct fork_args *uap)
{
	int ret;
	size_t len;
	struct filemon *filemon;

	if ((ret = fork(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "F %d %ld\n",
			   curproc->p_pid, (long)curthread->td_retval[0]);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

static int
filemon_wrapper_open(struct thread *td, struct open_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = open(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "%c %d %s\n",
			    (uap->flags & O_ACCMODE) ? 'W':'R', curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

static int
filemon_wrapper_rename(struct thread *td, struct rename_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = rename(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->from, filemon->fname1, sizeof(filemon->fname1), &done);
			copyinstr(uap->to, filemon->fname2, sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "M %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

static int
filemon_wrapper_link(struct thread *td, struct link_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = link(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);
			copyinstr(uap->link, filemon->fname2, sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "L %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

static int
filemon_wrapper_symlink(struct thread *td, struct symlink_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = symlink(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);
			copyinstr(uap->link, filemon->fname2, sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "L %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

#if __FreeBSD_version > 800032
#define FILEMON_HAS_LINKAT
#endif

#ifdef FILEMON_HAS_LINKAT
static int
filemon_wrapper_linkat(struct thread *td, struct linkat_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = linkat(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path1, filemon->fname1, sizeof(filemon->fname1), &done);
			copyinstr(uap->path2, filemon->fname2, sizeof(filemon->fname2), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "L %d '%s' '%s'\n",
			    curproc->p_pid, filemon->fname1, filemon->fname2);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}
#endif
static int
filemon_wrapper_stat(struct thread *td, struct stat_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = stat(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "S %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

#ifdef COMPAT_IA32
static int
filemon_wrapper_freebsd32_stat(struct thread *td, struct freebsd32_stat_args *uap)
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

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "S %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}
#endif

static void
filemon_wrapper_sys_exit(struct thread *td, struct sys_exit_args *uap)
{
	size_t len;
	struct filemon *filemon;

	/* Grab a read lock on the filemon inuse list. */
	filemon_lock_read();

	if ((filemon = filemon_pid_check(curproc)) != NULL) {
		/* Lock the found filemon structure. */
		filemon_filemon_lock(filemon);

		len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "X %d\n", curproc->p_pid);

		filemon_output(filemon, filemon->msgbufr, len);

		/* Check if the monitored process is about to exit. */
		if (filemon->pid == curproc->p_pid) {
			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "# Bye bye\n");

			filemon_output(filemon, filemon->msgbufr, len);
		}

		/* Unlock the found filemon structure. */
		filemon_filemon_unlock(filemon);
	}

	/* Release the read lock. */
	filemon_unlock_read();

	sys_exit(td, uap);
}

static int
filemon_wrapper_unlink(struct thread *td, struct unlink_args *uap)
{
	int ret;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = unlink(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			copyinstr(uap->path, filemon->fname1, sizeof(filemon->fname1), &done);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "D %d %s\n",
			    curproc->p_pid, filemon->fname1);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
}

static int
filemon_wrapper_vfork(struct thread *td, struct vfork_args *uap)
{
	int ret;
	size_t len;
	struct filemon *filemon;

	if ((ret = vfork(td, uap)) == 0) {
		/* Grab a read lock on the filemon inuse list. */
		filemon_lock_read();

		if ((filemon = filemon_pid_check(curproc)) != NULL) {
			/* Lock the found filemon structure. */
			filemon_filemon_lock(filemon);

			len = snprintf(filemon->msgbufr, sizeof(filemon->msgbufr), "F %d %ld\n",
			    curproc->p_pid, (long)curthread->td_retval[0]);

			filemon_output(filemon, filemon->msgbufr, len);

			/* Unlock the found filemon structure. */
			filemon_filemon_unlock(filemon);
		}

		/* Release the read lock. */
		filemon_unlock_read();
	}

	return(ret);
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
	sv_table[SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[SYS_stat].sy_call = (sy_call_t *) filemon_wrapper_stat;
	sv_table[SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[SYS_vfork].sy_call = (sy_call_t *) filemon_wrapper_vfork;
	sv_table[SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[SYS_linkat].sy_call = (sy_call_t *) filemon_wrapper_linkat;
#endif

#ifdef COMPAT_IA32
	sv_table = ia32_freebsd_sysvec.sv_table;

	sv_table[FREEBSD32_SYS_chdir].sy_call = (sy_call_t *) filemon_wrapper_chdir;
	sv_table[FREEBSD32_SYS_exit].sy_call = (sy_call_t *) filemon_wrapper_sys_exit;
	sv_table[FREEBSD32_SYS_freebsd32_execve].sy_call = (sy_call_t *) filemon_wrapper_freebsd32_execve;
	sv_table[FREEBSD32_SYS_fork].sy_call = (sy_call_t *) filemon_wrapper_fork;
	sv_table[FREEBSD32_SYS_open].sy_call = (sy_call_t *) filemon_wrapper_open;
	sv_table[FREEBSD32_SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[FREEBSD32_SYS_freebsd32_stat].sy_call = (sy_call_t *) filemon_wrapper_freebsd32_stat;
	sv_table[FREEBSD32_SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[FREEBSD32_SYS_vfork].sy_call = (sy_call_t *) filemon_wrapper_vfork;
	sv_table[FREEBSD32_SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[FREEBSD32_SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[FREEBSD32_SYS_linkat].sy_call = (sy_call_t *) filemon_wrapper_linkat;
#endif
#endif


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

	sv_table[SYS_chdir].sy_call = (sy_call_t *) chdir;
	sv_table[SYS_exit].sy_call = (sy_call_t *) sys_exit;
	sv_table[SYS_execve].sy_call = (sy_call_t *) execve;
	sv_table[SYS_fork].sy_call = (sy_call_t *) fork;
	sv_table[SYS_open].sy_call = (sy_call_t *) open;
	sv_table[SYS_rename].sy_call = (sy_call_t *) rename;
	sv_table[SYS_stat].sy_call = (sy_call_t *) stat;
	sv_table[SYS_unlink].sy_call = (sy_call_t *) unlink;
	sv_table[SYS_vfork].sy_call = (sy_call_t *) vfork;
	sv_table[SYS_link].sy_call = (sy_call_t *) link;
	sv_table[SYS_symlink].sy_call = (sy_call_t *) symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[SYS_linkat].sy_call = (sy_call_t *) linkat;
#endif

#ifdef COMPAT_IA32
	sv_table = ia32_freebsd_sysvec.sv_table;

	sv_table[FREEBSD32_SYS_chdir].sy_call = (sy_call_t *) chdir;
	sv_table[FREEBSD32_SYS_exit].sy_call = (sy_call_t *) sys_exit;
	sv_table[FREEBSD32_SYS_freebsd32_execve].sy_call = (sy_call_t *)freebsd32_execve;
	sv_table[FREEBSD32_SYS_fork].sy_call = (sy_call_t *) fork;
	sv_table[FREEBSD32_SYS_open].sy_call = (sy_call_t *) open;
	sv_table[FREEBSD32_SYS_rename].sy_call = (sy_call_t *) rename;
	sv_table[FREEBSD32_SYS_freebsd32_stat].sy_call = (sy_call_t *) freebsd32_stat;
	sv_table[FREEBSD32_SYS_unlink].sy_call = (sy_call_t *) unlink;
	sv_table[FREEBSD32_SYS_vfork].sy_call = (sy_call_t *) vfork;
	sv_table[FREEBSD32_SYS_link].sy_call = (sy_call_t *) link;
	sv_table[FREEBSD32_SYS_symlink].sy_call = (sy_call_t *) symlink;
#ifdef FILEMON_HAS_LINKAT
	sv_table[FREEBSD32_SYS_linkat].sy_call = (sy_call_t *) linkat;
#endif
#endif

}
