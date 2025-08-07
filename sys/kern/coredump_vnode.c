/*
 * SPDX-License-Identifier: BSD-3-Clause AND BSD-2-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 * - kern_sig.c
 */
/*
 * Copyright (c) 1993, David Greenman
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -kern_exec.c
 */

#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/compressor.h>
#include <sys/devctl.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/limits.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/ucoredump.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <security/audit/audit.h>

#define	GZIP_SUFFIX	".gz"
#define	ZSTD_SUFFIX	".zst"

#define	MAX_NUM_CORE_FILES 100000
#ifndef NUM_CORE_FILES
#define	NUM_CORE_FILES 5
#endif

static coredumper_handle_fn	coredump_vnode;
static struct coredumper vnode_coredumper = {
	.cd_name = "vnode_coredumper",
	.cd_handle = coredump_vnode,
};

SYSINIT(vnode_coredumper_register, SI_SUB_EXEC, SI_ORDER_ANY,
    coredumper_register, &vnode_coredumper);

_Static_assert(NUM_CORE_FILES >= 0 && NUM_CORE_FILES <= MAX_NUM_CORE_FILES,
    "NUM_CORE_FILES is out of range (0 to " __STRING(MAX_NUM_CORE_FILES) ")");
static int num_cores = NUM_CORE_FILES;

static int capmode_coredump;
SYSCTL_INT(_kern, OID_AUTO, capmode_coredump, CTLFLAG_RWTUN,
    &capmode_coredump, 0, "Allow processes in capability mode to dump core");

static int set_core_nodump_flag = 0;
SYSCTL_INT(_kern, OID_AUTO, nodump_coredump, CTLFLAG_RW, &set_core_nodump_flag,
	0, "Enable setting the NODUMP flag on coredump files");

static int coredump_devctl = 0;
SYSCTL_INT(_kern, OID_AUTO, coredump_devctl, CTLFLAG_RW, &coredump_devctl,
	0, "Generate a devctl notification when processes coredump");

/*
 * corefilename[] is protected by the allproc_lock.
 */
static char corefilename[MAXPATHLEN] = { "%N.core" };
TUNABLE_STR("kern.corefile", corefilename, sizeof(corefilename));

static int
sysctl_kern_corefile(SYSCTL_HANDLER_ARGS)
{
	int error;

	sx_xlock(&allproc_lock);
	error = sysctl_handle_string(oidp, corefilename, sizeof(corefilename),
	    req);
	sx_xunlock(&allproc_lock);

	return (error);
}
SYSCTL_PROC(_kern, OID_AUTO, corefile, CTLTYPE_STRING | CTLFLAG_RW |
    CTLFLAG_MPSAFE, 0, 0, sysctl_kern_corefile, "A",
    "Process corefile name format string");

static int
sysctl_debug_num_cores_check (SYSCTL_HANDLER_ARGS)
{
	int error;
	int new_val;

	new_val = num_cores;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val > MAX_NUM_CORE_FILES)
		new_val = MAX_NUM_CORE_FILES;
	if (new_val < 0)
		new_val = 0;
	num_cores = new_val;
	return (0);
}
SYSCTL_PROC(_debug, OID_AUTO, ncores,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, 0, sizeof(int),
    sysctl_debug_num_cores_check, "I",
    "Maximum number of generated process corefiles while using index format");

static void
vnode_close_locked(struct thread *td, struct vnode *vp)
{

	VOP_UNLOCK(vp);
	vn_close(vp, FWRITE, td->td_ucred, td);
}

int
core_vn_write(const struct coredump_writer *cdw, const void *base, size_t len,
    off_t offset, enum uio_seg seg, struct ucred *cred, size_t *resid,
    struct thread *td)
{
	struct coredump_vnode_ctx *ctx = cdw->ctx;

	return (vn_rdwr_inchunks(UIO_WRITE, ctx->vp, __DECONST(void *, base),
	    len, offset, seg, IO_UNIT | IO_DIRECT | IO_RANGELOCKED,
	    cred, ctx->fcred, resid, td));
}

int
core_vn_extend(const struct coredump_writer *cdw, off_t newsz,
    struct ucred *cred)
{
	struct coredump_vnode_ctx *ctx = cdw->ctx;
	struct mount *mp;
	int error;

	error = vn_start_write(ctx->vp, &mp, V_WAIT);
	if (error != 0)
		return (error);
	vn_lock(ctx->vp, LK_EXCLUSIVE | LK_RETRY);
	error = vn_truncate_locked(ctx->vp, newsz, false, cred);
	VOP_UNLOCK(ctx->vp);
	vn_finished_write(mp);
	return (error);
}

/*
 * If the core format has a %I in it, then we need to check
 * for existing corefiles before defining a name.
 * To do this we iterate over 0..ncores to find a
 * non-existing core file name to use. If all core files are
 * already used we choose the oldest one.
 */
static int
corefile_open_last(struct thread *td, char *name, int indexpos,
    int indexlen, int ncores, struct vnode **vpp)
{
	struct vnode *oldvp, *nextvp, *vp;
	struct vattr vattr;
	struct nameidata nd;
	int error, i, flags, oflags, cmode;
	char ch;
	struct timespec lasttime;

	nextvp = oldvp = NULL;
	cmode = S_IRUSR | S_IWUSR;
	oflags = VN_OPEN_NOAUDIT | VN_OPEN_NAMECACHE |
	    (capmode_coredump ? VN_OPEN_NOCAPCHECK : 0);

	for (i = 0; i < ncores; i++) {
		flags = O_CREAT | FWRITE | O_NOFOLLOW;

		ch = name[indexpos + indexlen];
		(void)snprintf(name + indexpos, indexlen + 1, "%.*u", indexlen,
		    i);
		name[indexpos + indexlen] = ch;

		NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name);
		error = vn_open_cred(&nd, &flags, cmode, oflags, td->td_ucred,
		    NULL);
		if (error != 0)
			break;

		vp = nd.ni_vp;
		NDFREE_PNBUF(&nd);
		if ((flags & O_CREAT) == O_CREAT) {
			nextvp = vp;
			break;
		}

		error = VOP_GETATTR(vp, &vattr, td->td_ucred);
		if (error != 0) {
			vnode_close_locked(td, vp);
			break;
		}

		if (oldvp == NULL ||
		    lasttime.tv_sec > vattr.va_mtime.tv_sec ||
		    (lasttime.tv_sec == vattr.va_mtime.tv_sec &&
		    lasttime.tv_nsec >= vattr.va_mtime.tv_nsec)) {
			if (oldvp != NULL)
				vn_close(oldvp, FWRITE, td->td_ucred, td);
			oldvp = vp;
			VOP_UNLOCK(oldvp);
			lasttime = vattr.va_mtime;
		} else {
			vnode_close_locked(td, vp);
		}
	}

	if (oldvp != NULL) {
		if (nextvp == NULL) {
			if ((td->td_proc->p_flag & P_SUGID) != 0) {
				error = EFAULT;
				vn_close(oldvp, FWRITE, td->td_ucred, td);
			} else {
				nextvp = oldvp;
				error = vn_lock(nextvp, LK_EXCLUSIVE);
				if (error != 0) {
					vn_close(nextvp, FWRITE, td->td_ucred,
					    td);
					nextvp = NULL;
				}
			}
		} else {
			vn_close(oldvp, FWRITE, td->td_ucred, td);
		}
	}
	if (error != 0) {
		if (nextvp != NULL)
			vnode_close_locked(td, oldvp);
	} else {
		*vpp = nextvp;
	}

	return (error);
}

/*
 * corefile_open(comm, uid, pid, td, compress, vpp, namep)
 * Expand the name described in corefilename, using name, uid, and pid
 * and open/create core file.
 * corefilename is a printf-like string, with three format specifiers:
 *	%N	name of process ("name")
 *	%P	process id (pid)
 *	%U	user id (uid)
 * For example, "%N.core" is the default; they can be disabled completely
 * by using "/dev/null", or all core files can be stored in "/cores/%U/%N-%P".
 * This is controlled by the sysctl variable kern.corefile (see above).
 */
static int
corefile_open(const char *comm, uid_t uid, pid_t pid, struct thread *td,
    int compress, int signum, struct vnode **vpp, char **namep)
{
	struct sbuf sb;
	struct nameidata nd;
	const char *format;
	char *hostname, *name;
	int cmode, error, flags, i, indexpos, indexlen, oflags, ncores;

	hostname = NULL;
	format = corefilename;
	name = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);
	indexlen = 0;
	indexpos = -1;
	ncores = num_cores;
	(void)sbuf_new(&sb, name, MAXPATHLEN, SBUF_FIXEDLEN);
	sx_slock(&allproc_lock);
	for (i = 0; format[i] != '\0'; i++) {
		switch (format[i]) {
		case '%':	/* Format character */
			i++;
			switch (format[i]) {
			case '%':
				sbuf_putc(&sb, '%');
				break;
			case 'H':	/* hostname */
				if (hostname == NULL) {
					hostname = malloc(MAXHOSTNAMELEN,
					    M_TEMP, M_WAITOK);
				}
				getcredhostname(td->td_ucred, hostname,
				    MAXHOSTNAMELEN);
				sbuf_cat(&sb, hostname);
				break;
			case 'I':	/* autoincrementing index */
				if (indexpos != -1) {
					sbuf_printf(&sb, "%%I");
					break;
				}

				indexpos = sbuf_len(&sb);
				sbuf_printf(&sb, "%u", ncores - 1);
				indexlen = sbuf_len(&sb) - indexpos;
				break;
			case 'N':	/* process name */
				sbuf_printf(&sb, "%s", comm);
				break;
			case 'P':	/* process id */
				sbuf_printf(&sb, "%u", pid);
				break;
			case 'S':	/* signal number */
				sbuf_printf(&sb, "%i", signum);
				break;
			case 'U':	/* user id */
				sbuf_printf(&sb, "%u", uid);
				break;
			default:
				log(LOG_ERR,
				    "Unknown format character %c in "
				    "corename `%s'\n", format[i], format);
				break;
			}
			break;
		default:
			sbuf_putc(&sb, format[i]);
			break;
		}
	}
	sx_sunlock(&allproc_lock);
	free(hostname, M_TEMP);
	if (compress == COMPRESS_GZIP)
		sbuf_cat(&sb, GZIP_SUFFIX);
	else if (compress == COMPRESS_ZSTD)
		sbuf_cat(&sb, ZSTD_SUFFIX);
	if (sbuf_error(&sb) != 0) {
		log(LOG_ERR, "pid %ld (%s), uid (%lu): corename is too "
		    "long\n", (long)pid, comm, (u_long)uid);
		sbuf_delete(&sb);
		free(name, M_TEMP);
		return (ENOMEM);
	}
	sbuf_finish(&sb);
	sbuf_delete(&sb);

	if (indexpos != -1) {
		error = corefile_open_last(td, name, indexpos, indexlen, ncores,
		    vpp);
		if (error != 0) {
			log(LOG_ERR,
			    "pid %d (%s), uid (%u):  Path `%s' failed "
			    "on initial open test, error = %d\n",
			    pid, comm, uid, name, error);
		}
	} else {
		cmode = S_IRUSR | S_IWUSR;
		oflags = VN_OPEN_NOAUDIT | VN_OPEN_NAMECACHE |
		    (capmode_coredump ? VN_OPEN_NOCAPCHECK : 0);
		flags = O_CREAT | FWRITE | O_NOFOLLOW;
		if ((td->td_proc->p_flag & P_SUGID) != 0)
			flags |= O_EXCL;

		NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name);
		error = vn_open_cred(&nd, &flags, cmode, oflags, td->td_ucred,
		    NULL);
		if (error == 0) {
			*vpp = nd.ni_vp;
			NDFREE_PNBUF(&nd);
		}
	}

	if (error != 0) {
#ifdef AUDIT
		audit_proc_coredump(td, name, error);
#endif
		free(name, M_TEMP);
		return (error);
	}
	*namep = name;
	return (0);
}

/*
 * The vnode dumper is the traditional coredump handler.  Our policy and limits
 * are generally checked already, so it creates the coredump name and passes on
 * a vnode and a size limit to the process-specific coredump routine if there is
 * one.  If there _is not_ one, it returns ENOSYS; otherwise it returns the
 * error from the process-specific routine.
 */
static int
coredump_vnode(struct thread *td, off_t limit)
{
	struct proc *p = td->td_proc;
	struct ucred *cred = td->td_ucred;
	struct vnode *vp;
	struct coredump_vnode_ctx wctx;
	struct coredump_writer cdw = { };
	struct flock lf;
	struct vattr vattr;
	size_t fullpathsize;
	int error, error1, jid, locked, ppid, sig;
	char *name;			/* name of corefile */
	void *rl_cookie;
	char *fullpath, *freepath = NULL;
	struct sbuf *sb;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	ppid = p->p_oppid;
	sig = p->p_sig;
	jid = p->p_ucred->cr_prison->pr_id;
	PROC_UNLOCK(p);

	error = corefile_open(p->p_comm, cred->cr_uid, p->p_pid, td,
	    compress_user_cores, sig, &vp, &name);
	if (error != 0)
		return (error);

	/*
	 * Don't dump to non-regular files or files with links.
	 * Do not dump into system files. Effective user must own the corefile.
	 */
	if (vp->v_type != VREG || VOP_GETATTR(vp, &vattr, cred) != 0 ||
	    vattr.va_nlink != 1 || (vp->v_vflag & VV_SYSTEM) != 0 ||
	    vattr.va_uid != cred->cr_uid) {
		VOP_UNLOCK(vp);
		error = EFAULT;
		goto out;
	}

	VOP_UNLOCK(vp);

	/* Postpone other writers, including core dumps of other processes. */
	rl_cookie = vn_rangelock_wlock(vp, 0, OFF_MAX);

	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	lf.l_type = F_WRLCK;
	locked = (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &lf, F_FLOCK) == 0);

	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	if (set_core_nodump_flag)
		vattr.va_flags = UF_NODUMP;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_SETATTR(vp, &vattr, cred);
	VOP_UNLOCK(vp);
	PROC_LOCK(p);
	p->p_acflag |= ACORE;
	PROC_UNLOCK(p);

	wctx.vp = vp;
	wctx.fcred = NOCRED;

	cdw.ctx = &wctx;
	cdw.write_fn = core_vn_write;
	cdw.extend_fn = core_vn_extend;

	if (p->p_sysent->sv_coredump != NULL) {
		error = p->p_sysent->sv_coredump(td, &cdw, limit, 0);
	} else {
		error = ENOSYS;
	}

	if (locked) {
		lf.l_type = F_UNLCK;
		VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
	}
	vn_rangelock_unlock(vp, rl_cookie);

	/*
	 * Notify the userland helper that a process triggered a core dump.
	 * This allows the helper to run an automated debugging session.
	 */
	if (error != 0 || coredump_devctl == 0)
		goto out;
	sb = sbuf_new_auto();
	if (vn_fullpath_global(p->p_textvp, &fullpath, &freepath) != 0)
		goto out2;
	sbuf_cat(sb, "comm=\"");
	devctl_safe_quote_sb(sb, fullpath);
	free(freepath, M_TEMP);
	sbuf_cat(sb, "\" core=\"");

	/*
	 * We can't lookup core file vp directly. When we're replacing a core, and
	 * other random times, we flush the name cache, so it will fail. Instead,
	 * if the path of the core is relative, add the current dir in front if it.
	 */
	if (name[0] != '/') {
		fullpathsize = MAXPATHLEN;
		freepath = malloc(fullpathsize, M_TEMP, M_WAITOK);
		if (vn_getcwd(freepath, &fullpath, &fullpathsize) != 0) {
			free(freepath, M_TEMP);
			goto out2;
		}
		devctl_safe_quote_sb(sb, fullpath);
		free(freepath, M_TEMP);
		sbuf_putc(sb, '/');
	}
	devctl_safe_quote_sb(sb, name);
	sbuf_putc(sb, '"');

	sbuf_printf(sb, " jid=%d pid=%d ppid=%d signo=%d",
	    jid, p->p_pid, ppid, sig);
	if (sbuf_finish(sb) == 0)
		devctl_notify("kernel", "signal", "coredump", sbuf_data(sb));
out2:
	sbuf_delete(sb);
out:
	error1 = vn_close(vp, FWRITE, cred, td);
	if (error == 0)
		error = error1;
#ifdef AUDIT
	audit_proc_coredump(td, name, error);
#endif
	free(name, M_TEMP);
	return (error);
}
